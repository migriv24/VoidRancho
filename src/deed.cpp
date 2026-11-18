/* deed.cpp — see rancho/deed.hpp and okf/concepts/deed.md. */
#include "rancho/deed.hpp"

#include "rancho/overlay.hpp"

#include "rancho/segments.hpp"

#include "json.hpp"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <map>
#include <random>
#include <set>
#include <sstream>

namespace rancho {

namespace fs = std::filesystem;
using nlohmann::json;
using nlohmann::ordered_json;

namespace {

const std::set<std::string> kDeedMembers = {"rancho", "id", "name", "home", "parts"};

bool is_sqlite(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    char head[16] = {};
    in.read(head, sizeof head);
    return in.gcount() == 16 && std::string_view(head, 16) == std::string_view("SQLite format 3\0", 16);
}
const std::set<std::string> kPartMembers = {"id", "form", "path", "mantles", "pin", "database", "overlay"};

bool is_hex(std::string_view s) {
    for (char c : s)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return !s.empty();
}

/* What a JSON object holds beyond `known`, as an object's text ("" if nothing). */
std::string extra_of(const json& j, const std::set<std::string>& known) {
    json rest = json::object();
    for (auto it = j.begin(); it != j.end(); ++it)
        if (!known.count(it.key())) rest[it.key()] = it.value();
    return rest.empty() ? std::string() : rest.dump();
}

void add_extra(ordered_json& out, const std::string& extra) {
    if (extra.empty()) return;
    const json rest = json::parse(extra, nullptr, false);
    if (!rest.is_object()) return;
    for (auto it = rest.begin(); it != rest.end(); ++it)
        if (!out.contains(it.key())) out[it.key()] = it.value();
}

std::string read_text(const fs::path& p, bool& ok) {
    std::ifstream in(p, std::ios::binary);
    ok = (bool)in;
    if (!ok) return {};
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

/* A part's path, resolved against the deed's folder. Never std::filesystem::
 * absolute: MinGW's turns \\server\share into C:\server\share
 * (okf/design/building-0.1.md §3a). */
fs::path resolve(const fs::path& folder, const std::string& path) {
    const fs::path p(path);
    if (p.is_absolute() || is_unc(path) || p.has_root_name()) return p;
    return (folder / p).lexically_normal();
}

bool on_disk(const fs::path& p) {
    std::error_code ec;
    return fs::exists(p, ec);
}

/* "assets/x.png" stays inside its folder; "../x", "/x", "C:x" do not. */
bool inside(const std::string& rel) {
    if (rel.empty() || rel[0] == '/' || rel[0] == '\\' || rel.find(':') != std::string::npos) return false;
    const fs::path p = fs::path(rel).lexically_normal();
    for (const auto& seg : p)
        if (seg == "..") return false;
    return !p.is_absolute();
}

} // namespace

// ── the deed ────────────────────────────────────────────────────────────────

const Part* Deed::part(const std::string& part_id) const {
    for (const auto& p : parts)
        if (p.id == part_id) return &p;
    return nullptr;
}

const Part* Deed::home_part() const {
    if (!home.empty()) return part(home);
    for (const auto& p : parts)
        if (p.form == "store") return &p;
    for (const auto& p : parts)
        if (p.form == "segments") return &p;
    return nullptr;
}

const Part* Deed::part_of(const MantleRef& mantle) const {
    /* The key first, in every part, then the name: an id is exact, and a name
     * left in an older part must not take a mantle another part names by id. */
    for (const std::string* want : {&mantle.key, &mantle.name})
        for (const auto& p : parts)
            if (p.holds_rows() && !want->empty())
                for (const auto& m : p.mantles)
                    if (m == *want) return &p;
    for (const auto& p : parts)
        if (p.holds_rows() && p.holds_rest()) return &p;
    return nullptr;
}

std::string mint_database_id() {
    /* The random device, the clock and an address, through SHA-256: an id that
     * stays unique even where random_device is weak. */
    std::random_device rd;
    std::ostringstream seed;
    for (int i = 0; i < 8; ++i) seed << rd() << ".";
    seed << std::chrono::high_resolution_clock::now().time_since_epoch().count() << "." << (const void*)&seed;
    return sha256_hex(seed.str()).substr(0, 32);
}

std::optional<Deed> parse_deed(std::string_view text, std::string* error) {
    auto fail = [&](const std::string& why) -> std::optional<Deed> {
        if (error) *error = why;
        return std::nullopt;
    };
    const json j = json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return fail("not a JSON object");
    if (!j.contains("rancho") || !j["rancho"].is_number_integer() || j["rancho"].get<long long>() < 1)
        return fail("no \"rancho\" format number: not a deed");
    if (!j.contains("id") || !j["id"].is_string() || j["id"].get<std::string>().empty())
        return fail("no \"id\": a deed names who the database is");
    Deed d;
    d.format = (int)j["rancho"].get<long long>();
    d.id = j["id"].get<std::string>();
    if (j.contains("name") && j["name"].is_string()) d.name = j["name"].get<std::string>();
    if (j.contains("home") && j["home"].is_string()) d.home = j["home"].get<std::string>();
    if (j.contains("parts")) {
        if (!j["parts"].is_array()) return fail("\"parts\" is not a list");
        for (const auto& pj : j["parts"]) {
            if (!pj.is_object()) return fail("a part is not an object");
            Part p;
            if (!pj.contains("id") || !pj["id"].is_string()) return fail("a part has no \"id\"");
            p.id = pj["id"].get<std::string>();
            if (!pj.contains("form") || !pj["form"].is_string()) return fail("part '" + p.id + "' has no \"form\"");
            p.form = pj["form"].get<std::string>();
            if (pj.contains("path")) {
                if (!pj["path"].is_string()) return fail("part '" + p.id + "': \"path\" is not text");
                p.path = pj["path"].get<std::string>();
            }
            if (pj.contains("mantles")) {
                if (!pj["mantles"].is_array()) return fail("part '" + p.id + "': \"mantles\" is not a list");
                for (const auto& m : pj["mantles"]) {
                    if (!m.is_string()) return fail("part '" + p.id + "': a mantle name is not text");
                    p.mantles.push_back(m.get<std::string>());
                }
            }
            if (pj.contains("pin")) {
                if (!pj["pin"].is_string()) return fail("part '" + p.id + "': \"pin\" is not text");
                p.pin = pj["pin"].get<std::string>();
            }
            for (const char* k : {"database", "overlay"})
                if (pj.contains(k)) {
                    if (!pj[k].is_string()) return fail("part '" + p.id + "': \"" + k + "\" is not text");
                    (std::string(k) == "database" ? p.database : p.overlay) = pj[k].get<std::string>();
                }
            p.extra = extra_of(pj, kPartMembers);
            d.parts.push_back(std::move(p));
        }
    }
    d.extra = extra_of(j, kDeedMembers);
    if (error) error->clear();
    return d;
}

std::string deed_json(const Deed& d) {
    ordered_json out;
    out["rancho"] = d.format;
    out["id"] = d.id;
    if (!d.name.empty()) out["name"] = d.name;
    if (!d.home.empty()) out["home"] = d.home;
    ordered_json parts = ordered_json::array();
    for (const auto& p : d.parts) {
        ordered_json pj;
        pj["id"] = p.id;
        pj["form"] = p.form;
        if (!p.path.empty()) pj["path"] = p.path;
        if (!p.mantles.empty()) pj["mantles"] = p.mantles;
        if (!p.pin.empty()) pj["pin"] = p.pin;
        if (!p.database.empty()) pj["database"] = p.database;
        if (!p.overlay.empty()) pj["overlay"] = p.overlay;
        add_extra(pj, p.extra);
        parts.push_back(std::move(pj));
    }
    out["parts"] = std::move(parts);
    add_extra(out, d.extra);
    return out.dump(2) + "\n";
}

std::optional<Deed> read_deed(const fs::path& file, std::string* error) {
    bool ok = false;
    const std::string text = read_text(file, ok);
    if (!ok) {
        if (error) *error = "cannot read " + file.string();
        return std::nullopt;
    }
    return parse_deed(text, error);
}

namespace {
/* A deed this version must not rewrite: a newer Rancho wrote it, and this one
 * cannot tell which of its members an edit would make false (PNG's
 * safe-to-copy problem, answered by not editing). */
bool newer_refused(const Deed& deed, std::string* error) {
    if (deed.format <= Deed::kFormat) return false;
    if (error)
        *error = "written by a newer Rancho (format " + std::to_string(deed.format) +
                 "): this version reads it and never rewrites it";
    return true;
}
} // namespace

bool write_deed(const Deed& deed, const fs::path& file, std::string* error) {
    if (newer_refused(deed, error)) return false;
    /* Beside the file, then renamed over it: a reader sees the old deed or the
     * new one, never half. */
    const fs::path tmp = file.string() + ".writing";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out << deed_json(deed);
        if (!out) {
            if (error) *error = "cannot write " + tmp.string();
            return false;
        }
    }
    std::error_code ec;
    fs::rename(tmp, file, ec);
    if (ec) {
        fs::remove(tmp, ec);
        if (error) *error = "cannot replace " + file.string();
        return false;
    }
    return true;
}

DeedCheck check(const Deed& d) {
    DeedCheck c;
    if (d.format > Deed::kFormat)
        c.notes.push_back("written by a newer Rancho (format " + std::to_string(d.format) +
                          "): read for what this version knows, and never rewritten by it");
    else {
        /* Rancho is agnostic: a member it does not know is passed through, and
         * what a host has to say about its database belongs in a mantle. A
         * newer format's members are the newer Rancho's, so they are not noted. */
        auto note_extra = [&](const std::string& extra, const std::string& where) {
            if (extra.empty()) return;
            const json rest = json::parse(extra, nullptr, false);
            if (!rest.is_object()) return;
            for (auto it = rest.begin(); it != rest.end(); ++it)
                c.notes.push_back(where + "member \"" + it.key() +
                                  "\" is not Rancho's: it is kept as it is and not read. What a host says "
                                  "about its database belongs in a mantle");
        };
        note_extra(d.extra, "");
        for (const auto& p : d.parts)
            if (p.known_form()) note_extra(p.extra, "part '" + p.id + "': ");
    }
    std::set<std::string> ids;
    std::map<std::string, std::string> owner; // mantle -> part
    std::string rest;
    int rows = 0;
    for (const auto& p : d.parts) {
        if (p.id.empty()) c.errors.push_back("a part has an empty id");
        else if (!ids.insert(p.id).second) c.errors.push_back("two parts are called '" + p.id + "'");
        if (!p.known_form()) {
            c.notes.push_back("part '" + p.id + "' is a '" + p.form + "', which this version does not read");
            for (const auto& m : p.mantles) {
                auto [it, fresh] = owner.emplace(m, p.id);
                if (!fresh && it->second != p.id)
                    c.errors.push_back("mantle '" + m + "' is in parts '" + it->second + "' and '" + p.id +
                                       "': a mantle lives in one part (the first is read)");
            }
            continue;
        }
        if (p.path.empty()) c.errors.push_back("part '" + p.id + "' has no path");
        const fs::path pp(p.path);
        if (!p.path.empty() && !is_unc(p.path) && (pp.is_absolute() || pp.has_root_name()))
            c.notes.push_back("part '" + p.id + "' is at an absolute path: the deed opens it only where that path exists");
        if (!p.pin.empty()) {
            if (p.form != "segments")
                c.errors.push_back("part '" + p.id + "' is pinned, but only segments can be: a " + p.form +
                                   " is live");
            else if (p.pin.rfind("sha256:", 0) != 0 || p.pin.size() != 7 + 64 || !is_hex(p.pin.substr(7)))
                c.errors.push_back("part '" + p.id + "': a pin is \"sha256:\" and 64 lowercase hex digits");
        }
        if (!p.overlay.empty()) {
            const Part* o = d.part(p.overlay);
            if (!o || o->form != "store" || o == &p)
                c.errors.push_back("part '" + p.id + "': its overlay '" + p.overlay +
                                   "' is not another store part of this deed");
            if (&p == d.home_part())
                c.errors.push_back("part '" + p.id + "' is the home, which a working set writes: an overlay is over "
                                   "a part that is only read");
        }
        if (!p.database.empty() && p.database == d.id)
            c.notes.push_back("part '" + p.id + "' names this database's own id: \"database\" is for another's");
        if (p.form == "blobs") {
            if (!p.mantles.empty()) c.errors.push_back("part '" + p.id + "' holds blobs, not mantles");
            continue;
        }
        ++rows;
        if (p.holds_rest()) {
            if (!rest.empty())
                c.errors.push_back("parts '" + rest + "' and '" + p.id +
                                   "' both hold every other mantle: one may (the first is read)");
            else rest = p.id;
        }
        for (const auto& m : p.mantles) {
            auto [it, fresh] = owner.emplace(m, p.id);
            if (!fresh && it->second != p.id)
                c.errors.push_back("mantle '" + m + "' is in parts '" + it->second + "' and '" + p.id +
                                   "': a mantle lives in one part (the first is read)");
        }
    }
    if (!d.home.empty()) {
        const Part* h = d.part(d.home);
        if (!h) c.errors.push_back("the home, '" + d.home + "', is not one of the parts");
        else if (!h->holds_rows()) c.errors.push_back("the home, '" + d.home + "', is a " + h->form + ": it must hold rows");
    }
    if (!rows) c.notes.push_back("no part holds mantles: the database is empty wherever it is opened");
    if (rows && rest.empty()) c.notes.push_back("no part holds the other mantles: a mantle no part names has no place");
    return c;
}

Deed deed_for_store(Store& store, const fs::path& deed_folder, const std::string& name) {
    Deed d;
    d.id = store.meta("rancho.database");
    if (d.id.empty()) d.id = mint_database_id();
    d.name = name;
    Part p;
    p.id = "main";
    p.form = "store";
    const fs::path at = store.path();
    const fs::path rel = at.lexically_relative(deed_folder);
    const bool under = !rel.empty() && *rel.begin() != ".." && !rel.is_absolute();
    p.path = (under ? rel : at).generic_string();
    d.parts.push_back(p);
    return d;
}

namespace {

/* An embedded deed's home is the store that carries it, wherever that file is
 * now: a copy or a rename still finds itself. */
void home_is(Deed& d, const fs::path& store_file) {
    const std::string self = store_file.filename().generic_string();
    for (auto& p : d.parts)
        if (d.home.empty() ? p.form == "store" : p.id == d.home) {
            p.form = "store";
            p.path = self;
            if (d.home.empty()) d.home = p.id;
            return;
        }
    Part p;
    p.id = d.home.empty() ? "main" : d.home;
    p.form = "store";
    p.path = self;
    d.home = p.id;
    d.parts.insert(d.parts.begin(), p);
}

} // namespace

bool embed_deed(Store& store, Deed deed, std::string* error) {
    if (newer_refused(deed, error)) return false;
    const std::string owner = store.meta("rancho.database");
    if (!owner.empty() && owner != deed.id) {
        if (error) *error = "this store is the home of another database (" + owner + ")";
        return false;
    }
    home_is(deed, store.path());
    if (owner.empty()) store.set_meta("rancho.database", deed.id);
    store.set_meta("rancho.deed", deed_json(deed));
    return true;
}

const char* part_status_name(PartStatus s) {
    switch (s) {
    case PartStatus::Present: return "present";
    case PartStatus::Missing: return "missing";
    case PartStatus::NotOpenable: return "not openable";
    case PartStatus::Mismatch: return "mismatch";
    }
    return "?";
}

// ── the database ────────────────────────────────────────────────────────────

Database::~Database() = default;

std::unique_ptr<Database> Database::open(const fs::path& deed_file, const DatabaseOptions& options, std::string* error) {
    if (is_sqlite(deed_file)) {
        /* A store is a rancho too: the deed it carries, else one of one part.
         * The store is closed again before its part opens it. */
        Deed d;
        {
            Store s(deed_file, OpenOptions{options.sharing, options.temp_dir});
            if (!s.ok()) {
                if (error) *error = s.error();
                return nullptr;
            }
            const std::string text = s.meta("rancho.deed");
            if (text.empty()) d = deed_for_store(s, deed_file.parent_path(), deed_file.stem().string());
            else {
                std::string why;
                auto parsed = parse_deed(text, &why);
                if (!parsed) {
                    if (error) *error = "its embedded deed: " + why;
                    return nullptr;
                }
                d = std::move(*parsed);
                home_is(d, deed_file);
            }
        }
        return from_deed(std::move(d), deed_file.parent_path(), options);
    }
    auto deed = read_deed(deed_file, error);
    if (!deed) return nullptr;
    return from_deed(std::move(*deed), deed_file.parent_path(), options);
}

std::unique_ptr<Database> Database::from_deed(Deed deed, const fs::path& folder, const DatabaseOptions& options) {
    std::unique_ptr<Database> db(new Database());
    db->deed_ = std::move(deed);
    db->folder_ = folder;
    const DeedCheck c = check(db->deed_);
    for (const auto& e : c.errors) db->notes_.push_back("deed: " + e);
    for (const auto& n : c.notes) db->notes_.push_back("deed: " + n);
    db->open_parts(options);
    db->merge_declarations();
    for (std::size_t i = 0; i < db->deed_.parts.size(); ++i) {
        if (!db->stores_[i]) continue;
        const Part* here = &db->deed_.parts[i];
        const auto refs = db->stores_[i]->mantle_refs();
        /* A part may hold rows of a mantle the deed places elsewhere (a copy left
         * behind by a move, say): they are not read, and a person should know. */
        for (const auto& m : refs) {
            const Part* placed = db->deed_.part_of(m);
            if (placed == here) continue;
            db->notes_.push_back("mantle '" + m.name + "' in part '" + here->id + "' is not read: " +
                                 (placed ? "the deed places it in part '" + placed->id + "'"
                                         : std::string("the deed places it nowhere")));
        }
        /* A mantle named by its name is found, but a rename would lose it. */
        for (const auto& entry : here->mantles)
            for (const auto& m : refs)
                if (m.name == entry && m.key != entry)
                    db->notes_.push_back("part '" + here->id + "' names mantle '" + entry + "' by its name: its id, " +
                                         m.key + ", is what a rename keeps (`rancho ids` writes it)");
    }
    db->check_expectations(options.expect);
    return db;
}

void Database::check_expectations(const std::vector<Expectation>& given) {
    std::map<std::string, const Expectation*> by_name;
    for (const auto& e : given) by_name.emplace(e.name, &e);
    /* What each one builds on before its own check, each once. Returns whether
     * it held; a cycle in what a host gave holds nothing. */
    std::map<std::string, int> state; // 1 visiting, 2 held, 3 not held
    std::function<bool(const std::string&, const std::string&)> visit = [&](const std::string& name,
                                                                           const std::string& by) -> bool {
        const auto seen = state.find(name);
        if (seen != state.end()) {
            if (seen->second == 1) {
                expectation_check_.errors.push_back(name + ": builds on itself");
                notes_.push_back("expectation " + name + ": builds on itself");
                return false;
            }
            return seen->second == 2;
        }
        const auto it = by_name.find(name);
        if (it == by_name.end()) {
            const std::string e = by + ": builds on '" + name + "', which this host did not give";
            expectation_check_.errors.push_back(e);
            notes_.push_back("expectation " + e);
            state[name] = 3;
            return false;
        }
        state[name] = 1;
        bool held = true;
        for (const auto& base : it->second->builds_on) held = visit(base, name) && held;
        DeedCheck found;
        if (it->second->check) it->second->check(*this, found);
        for (const auto& e : found.errors) {
            expectation_check_.errors.push_back(name + ": " + e);
            notes_.push_back("expectation " + name + ": " + e);
        }
        for (const auto& n : found.notes) {
            expectation_check_.notes.push_back(name + ": " + n);
            notes_.push_back("expectation " + name + ": " + n);
        }
        held = held && found.ok();
        state[name] = held ? 2 : 3;
        if (held) met_.insert(name);
        return held;
    };
    for (const auto& e : given) visit(e.name, "");
}

Deed Database::deed_by_id() {
    Deed d = deed_;
    for (std::size_t i = 0; i < d.parts.size(); ++i) {
        if (!stores_[i]) continue;
        const auto refs = stores_[i]->mantle_refs();
        for (auto& entry : d.parts[i].mantles)
            for (const auto& m : refs)
                if (m.name == entry && m.key != entry) {
                    entry = m.key;
                    break;
                }
    }
    return d;
}

std::unique_ptr<Database> Database::create(const fs::path& deed_file, const std::string& name,
                                           const DatabaseOptions& options, std::string* error) {
    if (on_disk(deed_file)) {
        if (error) *error = deed_file.string() + " already exists";
        return nullptr;
    }
    const fs::path folder = deed_file.parent_path();
    const fs::path store_file = (folder / deed_file.stem()).string() + ".db";
    if (on_disk(store_file)) {
        if (error) *error = store_file.string() + " already exists: a new database does not take another's store";
        return nullptr;
    }
    Deed d;
    d.id = mint_database_id();
    d.name = name;
    Part main;
    main.id = "main";
    main.form = "store";
    main.path = deed_file.stem().string() + ".db";
    d.parts.push_back(std::move(main));
    {
        Store s(store_file, OpenOptions{options.sharing, options.temp_dir});
        if (!s.ok()) {
            if (error) *error = s.error();
            return nullptr;
        }
        s.set_meta("rancho.database", d.id);
    }
    if (!write_deed(d, deed_file, error)) return nullptr;
    return open(deed_file, options, error);
}

void Database::open_parts(const DatabaseOptions& options) {
    /* A phone outside Void Maiz's shell may have no temp folder, and
     * temp_directory_path throws there: the cache then sits beside the deed. */
    fs::path cache = options.cache_dir;
    if (cache.empty()) {
        std::error_code ec;
        const fs::path tmp = fs::temp_directory_path(ec);
        cache = ec || tmp.empty() ? folder_ / ".rancho-cache" : tmp / "rancho-cache";
    }
    const Part* home = deed_.home_part();
    states_.assign(deed_.parts.size(), {});
    stores_.resize(deed_.parts.size());
    for (std::size_t i = 0; i < deed_.parts.size(); ++i) {
        const Part& p = deed_.parts[i];
        PartState& st = states_[i];
        st.id = p.id;
        st.where = p.path.empty() ? fs::path() : resolve(folder_, p.path);
        auto absent = [&](PartStatus s, const std::string& why) {
            st.status = s;
            st.reason = why;
        };
        if (!p.known_form()) {
            absent(PartStatus::NotOpenable, "a '" + p.form + "' part: this version of Rancho does not read that form");
            continue;
        }
        if (p.path.empty()) {
            absent(PartStatus::Missing, "the deed gives it no path");
            continue;
        }
        if (!on_disk(st.where)) {
            absent(PartStatus::Missing, "not found at " + st.where.string());
            continue;
        }
        if (p.form == "blobs") {
            std::error_code ec;
            if (!fs::is_directory(st.where, ec)) absent(PartStatus::Mismatch, "a blobs part is a folder");
            else st.status = PartStatus::Present;
            continue;
        }
        if (p.form == "store") {
            auto s = std::make_unique<Store>(st.where, OpenOptions{options.sharing, options.temp_dir});
            if (!s->ok()) {
                absent(PartStatus::Missing, "cannot be opened: " + s->error());
                continue;
            }
            if (&p == home) {
                /* The home records whose it is, so a deed pointed at another
                 * database's store says so instead of writing into it. */
                const std::string owner = s->meta("rancho.database");
                if (!owner.empty() && owner != deed_.id) {
                    absent(PartStatus::Mismatch, "this store is the home of another database (" + owner + ")");
                    continue;
                }
                if (owner.empty()) s->set_meta("rancho.database", deed_.id);
            }
            if (!p.database.empty() && &p != home) {
                /* Another database's part: its store must say it is that
                 * database's (Void Squid's question 1). */
                const std::string owner = s->meta("rancho.database");
                if (owner != p.database) {
                    absent(PartStatus::Mismatch, owner.empty()
                                                     ? "its store records no database, and the deed names " + p.database
                                                     : "its store is database " + owner + "'s, not " + p.database + "'s");
                    continue;
                }
            }
            if (s->medium() == Medium::Network && &p != home)
                notes_.push_back("part '" + p.id +
                                 "' is a store on a network path, locked to this machine while open: a part "
                                 "several machines read is better shared as segments");
            stores_[i] = std::move(s);
            st.status = PartStatus::Present;
            continue;
        }
        // segments: find the snapshot, check it, read it through a restored copy
        long long n = 0;
        std::string hash;
        bool ok = false;
        if (p.pin.empty()) {
            n = current_snapshot(st.where);
            if (n) hash = sha256_hex(read_text(st.where / "manifests" / (std::to_string(n) + ".json"), ok));
            if (!n || !ok) {
                absent(PartStatus::Missing, "no snapshot in " + st.where.string());
                continue;
            }
        } else {
            const std::string want = p.pin.substr(p.pin.rfind(':') + 1);
            for (long long k : snapshots(st.where)) {
                const std::string h =
                    sha256_hex(read_text(st.where / "manifests" / (std::to_string(k) + ".json"), ok));
                if (ok && h == want) {
                    n = k;
                    hash = h;
                    break;
                }
            }
            if (!n) {
                absent(PartStatus::Mismatch, "no snapshot there is the one pinned (" + p.pin + ")");
                continue;
            }
        }
        std::error_code ec;
        fs::create_directories(cache, ec);
        const fs::path copy = cache / ("segments-" + hash.substr(0, 32) + ".db");
        auto s = std::make_unique<Store>(copy);
        if (!s->ok()) {
            absent(PartStatus::Missing, "its cache cannot be opened: " + s->error());
            continue;
        }
        if (s->meta("rancho.restored_from") != hash) {
            const RestoreStats r = restore(st.where, *s, n);
            if (!r.ok) {
                absent(PartStatus::Mismatch, "cannot be read: " + r.error);
                continue;
            }
            s->set_meta("rancho.restored_from", hash);
        }
        stores_[i] = std::move(s);
        st.status = PartStatus::Present;
    }
}

void Database::merge_declarations() {
    /* Declared glyphs and domains travel in each part's own document; a mantle
     * read from a part needs its part's declarations to mean what it meant. The
     * home's win; a disagreement is noted, once per name. */
    const Part* home = deed_.home_part();
    std::vector<std::size_t> order;
    for (std::size_t i = 0; i < deed_.parts.size(); ++i)
        if (stores_[i] && &deed_.parts[i] == home) order.insert(order.begin(), i);
        else if (stores_[i]) order.push_back(i);
    for (const char* key : {"glyphs", "domains"}) {
        json merged = json::object();
        bool any = false;
        for (std::size_t i : order) {
            const auto text = stores_[i]->top(key);
            if (!text) continue;
            const json obj = json::parse(*text, nullptr, false);
            if (!obj.is_object()) continue;
            any = true;
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                if (!merged.contains(it.key())) merged[it.key()] = it.value();
                else if (merged[it.key()] != it.value())
                    notes_.push_back(std::string(key == std::string("glyphs") ? "glyph" : "domain") + " '" + it.key() +
                                     "' is declared differently in part '" + deed_.parts[i].id +
                                     "': the first part's is read");
            }
        }
        (key == std::string("glyphs") ? glyphs_ : domains_) = any ? merged.dump() : std::string();
    }
}

bool Database::complete() const {
    for (const auto& s : states_)
        if (s.status != PartStatus::Present) return false;
    return true;
}

Store* Database::home_rows() {
    const Part* h = deed_.home_part();
    if (!h) return nullptr;
    for (std::size_t i = 0; i < deed_.parts.size(); ++i)
        if (&deed_.parts[i] == h) return stores_[i].get();
    return nullptr;
}

Store* Database::home() {
    /* A segments home is read through a restored copy: a working set writing
     * there would lose every edit at the next restore. Only a store is written. */
    const Part* h = deed_.home_part();
    return h && h->form == "store" ? home_rows() : nullptr;
}

Store* Database::store_of(const std::string& mantle) {
    for (std::size_t i = 0; i < deed_.parts.size(); ++i) {
        if (!stores_[i]) continue;
        for (const auto& m : stores_[i]->mantle_refs())
            if ((m.key == mantle || m.name == mantle) && deed_.part_of(m) == &deed_.parts[i]) return stores_[i].get();
    }
    return nullptr;
}

std::vector<Database::Placed> Database::placement() {
    std::vector<Placed> out;
    std::set<std::string> seen; // keys
    for (std::size_t i = 0; i < deed_.parts.size(); ++i) {
        const Part& p = deed_.parts[i];
        if (!p.holds_rows()) continue;
        if (stores_[i]) {
            for (const auto& m : stores_[i]->mantle_refs())
                if (deed_.part_of(m) == &p && seen.insert(m.key).second) out.push_back({m.name, p.id, true, m.key});
        } else {
            for (const auto& m : p.mantles)
                if (deed_.part_of(m) == &p && seen.insert(m).second) out.push_back({m, p.id, false, m});
        }
    }
    return out;
}

bool Database::write_state(std::ostream& out, const Selection& sel) {
    Store* h = home_rows(); // the top-level keys, from a store or a snapshot alike
    std::vector<std::string> keys = h ? h->top_keys() : std::vector<std::string>{};
    if (keys.empty()) keys = {"version", "mantles"};
    bool has_glyphs = false, has_domains = false;
    for (const auto& k : keys) {
        has_glyphs |= k == "glyphs";
        has_domains |= k == "domains";
    }
    if (!has_glyphs && !glyphs_.empty()) keys.push_back("glyphs");
    if (!has_domains && !domains_.empty()) keys.push_back("domains");

    out << "{";
    bool first_key = true;
    for (const auto& k : keys) {
        std::optional<std::string> value;
        if (k == "glyphs" && !glyphs_.empty()) value = glyphs_;
        else if (k == "domains" && !domains_.empty()) value = domains_;
        else if (k != "mantles" && h) value = h->top(k);
        if (k == "version" && !value) value = "1";
        if (k != "mantles" && !value) continue;
        if (!first_key) out << ",";
        first_key = false;
        out << json(k).dump() << ":";
        if (k != "mantles") {
            out << *value;
            continue;
        }
        /* Part by part in the deed's order, each writing the mantles the deed
         * places in it: the rows go straight from each store to `out`. */
        out << "[";
        bool first_m = true;
        for (std::size_t i = 0; i < deed_.parts.size(); ++i) {
            if (!stores_[i]) continue;
            const Part* p = &deed_.parts[i];
            Store* over = overlay_store(*p);
            if (!over) {
                stores_[i]->write_mantles(out, sel, [&](const MantleRef& m) { return deed_.part_of(m) == p; }, first_m);
                continue;
            }
            /* base ⊕ overlay, mantle by mantle. In this first form a mantle
             * with an overlay is held in memory while it is written (a
             * library's catalogue, not a 5×10^5-rune mantle). */
            std::ostringstream base;
            bool first_b = true;
            stores_[i]->write_mantles(base, sel, [&](const MantleRef& m) { return deed_.part_of(m) == p; }, first_b);
            json mantles = json::parse("[" + base.str() + "]", nullptr, false);
            if (!mantles.is_array()) return false;
            for (auto& m : mantles) {
                const std::string key = m.contains("id") && m["id"].is_string() && !m["id"].get<std::string>().empty()
                                            ? m["id"].get<std::string>()
                                            : "name:" + m.value("name", "");
                auto diffs = over->overlays(p->id, key);
                if (!diffs.empty() && m.contains("runes") && m["runes"].is_array()) {
                    for (auto& r : m["runes"]) {
                        const std::string rid = r.contains("spirit") ? r["spirit"].value("id", "") : "";
                        const auto d = diffs.find(rid);
                        if (d == diffs.end()) continue;
                        if (!overlay::is_made(d->second)) r = json::parse(overlay::apply(r.dump(), d->second));
                        diffs.erase(d);
                    }
                    for (const auto& [rid, d] : diffs) // runes the person made (residuals stay out of the document)
                        if (overlay::is_made(d)) m["runes"].push_back(json::parse(overlay::apply("{}", d)));
                }
                if (!first_m) out << ",";
                first_m = false;
                out << m.dump();
            }
        }
        out << "]";
    }
    out << "}";
    return (bool)out;
}

Store* Database::overlay_store(const Part& p) {
    if (p.overlay.empty()) return nullptr;
    for (std::size_t i = 0; i < deed_.parts.size(); ++i)
        if (deed_.parts[i].id == p.overlay && deed_.parts[i].form == "store" && &deed_.parts[i] != &p)
            return stores_[i].get();
    return nullptr;
}

bool Database::record_overlay(const std::string& mantle, const std::string& rune, const std::string& source,
                              std::string* error) {
    auto fail = [&](const std::string& why) {
        if (error) *error = why;
        return false;
    };
    for (std::size_t i = 0; i < deed_.parts.size(); ++i) {
        if (!stores_[i]) continue;
        const Part& p = deed_.parts[i];
        for (const auto& m : stores_[i]->mantle_refs()) {
            if ((m.key != mantle && m.name != mantle) || deed_.part_of(m) != &p) continue;
            Store* over = overlay_store(p);
            if (!over) return fail("part '" + p.id + "' holds mantle " + mantle + " and keeps no overlay");
            const json r = json::parse(rune, nullptr, false);
            const std::string rid = r.is_object() && r.contains("spirit") ? r["spirit"].value("id", "") : "";
            if (rid.empty()) return fail("a rune with no spirit id cannot be overlaid: identity is its key");
            const auto had = over->overlays(p.id, m.key);
            const auto old = had.find(rid);
            const auto base = stores_[i]->rune_by_id(m.key, rid);
            std::string d;
            if (base) {
                d = overlay::diff(*base, rune, source,
                                  old == had.end() ? std::map<std::string, std::string>{} : overlay::added_tags(old->second));
            } else {
                // made by the person; a residual's edits are kept as they were (rule 7)
                if (old != had.end() && !overlay::is_made(old->second)) return true;
                d = overlay::made(rune, old != had.end() ? json::parse(old->second).value("source", source) : source);
            }
            over->put_overlay(p.id, m.key, rid, d);
            return true;
        }
    }
    return fail("no present part holds mantle " + mantle);
}

bool Database::forget_overlay(const std::string& mantle, const std::string& rid) {
    for (std::size_t i = 0; i < deed_.parts.size(); ++i) {
        if (!stores_[i]) continue;
        const Part& p = deed_.parts[i];
        for (const auto& m : stores_[i]->mantle_refs())
            if ((m.key == mantle || m.name == mantle) && deed_.part_of(m) == &p) {
                Store* over = overlay_store(p);
                if (!over) return false;
                over->put_overlay(p.id, m.key, rid, "");
                return true;
            }
    }
    return false;
}

std::vector<std::string> Database::residuals() {
    std::vector<std::string> out;
    for (std::size_t i = 0; i < deed_.parts.size(); ++i) {
        if (!stores_[i]) continue;
        const Part& p = deed_.parts[i];
        Store* over = overlay_store(p);
        if (!over) continue;
        for (const auto& m : stores_[i]->mantle_refs()) {
            if (deed_.part_of(m) != &p) continue;
            for (const auto& [rid, d] : over->overlays(p.id, m.key))
                if (!overlay::is_made(d) && !stores_[i]->rune_by_id(m.key, rid)) out.push_back(p.id + "/" + m.key + "/" + rid);
        }
    }
    return out;
}

std::string Database::state(const Selection& sel) {
    std::ostringstream out;
    write_state(out, sel);
    return out.str();
}

std::optional<std::string> Database::blob(const std::string& path) {
    if (!inside(path)) return std::nullopt;
    for (std::size_t i = 0; i < deed_.parts.size(); ++i) {
        if (deed_.parts[i].form != "blobs" || states_[i].status != PartStatus::Present) continue;
        const fs::path f = states_[i].where / fs::path(path);
        if (!on_disk(f)) continue;
        bool ok = false;
        std::string bytes = read_text(f, ok);
        if (ok) return bytes;
    }
    if (Store* h = home()) return h->blob(path);
    return std::nullopt;
}

} // namespace rancho
