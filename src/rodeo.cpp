/* rodeo.cpp — see rancho/rodeo.hpp and okf/concepts/rodeo.md. Reads the store
 * through its read-only handle, as the matrix view and segments do. */
#include "rancho/rodeo.hpp"

#include "rancho/segments.hpp" // sha256_hex

#include "sql.hpp"

#include "json.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <system_error>

namespace rancho {

namespace fs = std::filesystem;

// ── el rodeo ────────────────────────────────────────────────────────────────

Rodeo::Rodeo(Store& store, const std::string& cursor, const RodeoOptions& options)
    : store_(store), options_(options) {
    sqlite3* db = store.handle();
    if (!db) {
        error_ = "the store is not open";
        done_ = true;
        return;
    }
    {
        Stmt q(db, "SELECT id, name FROM mantles WHERE deleted=0 ORDER BY pos");
        while (q.row()) mantles_.emplace_back(q.col_int(0), std::string(q.col(1)));
    }
    if (cursor.empty()) return;
    /* "rodeo/1 <mantle> <stage> <key>": the mantle by its position in order,
     * the key as written (a path, or a position). */
    std::istringstream in(cursor);
    std::string format;
    std::size_t m = 0;
    int stage = 0;
    in >> format >> m >> stage;
    std::getline(in >> std::ws, key_);
    if (format != "rodeo/1" || !in.eof() || stage < 0 || stage > 3 || m > mantles_.size()) {
        error_ = "not a rodeo cursor: " + cursor;
        done_ = true;
        return;
    }
    m_ = m;
    stage_ = stage;
    done_ = m_ >= mantles_.size();
}

std::string Rodeo::cursor() const {
    return "rodeo/1 " + std::to_string(m_) + " " + std::to_string(stage_) + " " + key_;
}

bool Rodeo::next(Herd& out) {
    out = Herd{};
    while (!done_) {
        if (advance(out)) return true;
        if (!error_.empty()) return false;
    }
    return false;
}

/* One step: a herd (true), or the end of a stage or a mantle (false, and the
 * position moves on). */
bool Rodeo::advance(Herd& out) {
    if (m_ >= mantles_.size()) {
        done_ = true;
        return false;
    }
    sqlite3* db = store_.handle();
    const long long mid = mantles_[m_].first;
    out.mantle = mantles_[m_].second;
    auto next_stage = [&] {
        ++stage_;
        key_.clear();
        if (stage_ > 3) {
            stage_ = 0;
            ++m_;
            done_ = m_ >= mantles_.size();
        }
        return false;
    };
    // the edges from each rune of the herd: each edge once, with its source
    auto edges_of_runes = [&](const std::vector<std::string>& names) {
        Stmt q(db, "SELECT json FROM edges INDEXED BY edges_by_src WHERE mantle=? AND src=? AND deleted=0"
                   " ORDER BY pos");
        for (const auto& n : names) {
            q.integer(1, mid).text(2, n);
            while (q.row()) out.edges.emplace_back(q.col(0));
            q.reset();
        }
    };
    std::vector<std::string> names;

    switch (stage_) {
    case 0: { // the always-loaded runes (measures): one herd
        Stmt q(db, "SELECT r.json, r.name FROM clusters c INDEXED BY clusters_always CROSS JOIN runes r"
                   " ON r.id=c.rune WHERE c.mantle=? AND c.always=1 AND r.deleted=0 ORDER BY r.pos");
        q.integer(1, mid);
        while (q.row()) {
            out.runes.emplace_back(q.col(0));
            names.emplace_back(q.col(1));
        }
        next_stage();
        if (out.runes.empty()) return false;
        out.path = "!";
        edges_of_runes(names);
        return true;
    }
    case 1: { // the next leaf after key_, in path order
        std::string path;
        {
            Stmt q(db, "SELECT path FROM clusters INDEXED BY clusters_by_path WHERE mantle=? AND path>? AND always=0"
                       " ORDER BY path LIMIT 1");
            q.integer(1, mid).text(2, key_);
            if (!q.row()) return next_stage();
            path = std::string(q.col(0));
        }
        Stmt q(db, "SELECT r.json, r.name FROM clusters c INDEXED BY clusters_by_path CROSS JOIN runes r"
                   " ON r.id=c.rune WHERE c.mantle=? AND c.path=? AND c.always=0 AND r.deleted=0 ORDER BY r.pos");
        q.integer(1, mid).text(2, path);
        while (q.row()) {
            out.runes.emplace_back(q.col(0));
            names.emplace_back(q.col(1));
        }
        key_ = path;
        if (out.runes.empty()) return false; // a leaf whose runes were all removed
        out.path = path;
        edges_of_runes(names);
        return true;
    }
    case 2: { // runes in no cluster, in pages, from the position after key_
        const double after = key_.empty() ? -1e300 : std::stod(key_);
        Stmt q(db, "SELECT r.pos, r.json, r.name FROM runes r INDEXED BY runes_by_pos WHERE r.mantle=? AND r.pos>?"
                   " AND r.deleted=0 AND NOT EXISTS(SELECT 1 FROM clusters c WHERE c.rune=r.id) ORDER BY r.pos");
        q.integer(1, mid).real(2, after);
        std::uint64_t bytes = 0;
        double last = after;
        while (q.row()) {
            last = q.col_real(0);
            out.runes.emplace_back(q.col(1));
            names.emplace_back(q.col(2));
            bytes += out.runes.back().size();
            if (bytes >= options_.page_bytes) break;
        }
        if (out.runes.empty()) return next_stage();
        std::ostringstream k;
        k.precision(17);
        k << last;
        key_ = k.str();
        out.path = "page";
        edges_of_runes(names);
        return true;
    }
    default: { // edges whose source is no live rune: they belong to no herd above
        const double after = key_.empty() ? -1e300 : std::stod(key_);
        Stmt q(db, "SELECT e.pos, e.json FROM edges e INDEXED BY edges_by_pos WHERE e.mantle=?1 AND e.pos>?2"
                   " AND e.deleted=0 AND NOT EXISTS(SELECT 1 FROM runes r INDEXED BY runes_by_name"
                   " WHERE r.mantle=?1 AND r.name=e.src AND r.deleted=0) ORDER BY e.pos");
        q.integer(1, mid).real(2, after);
        std::uint64_t bytes = 0;
        double last = after;
        while (q.row()) {
            last = q.col_real(0);
            out.edges.emplace_back(q.col(1));
            bytes += out.edges.back().size();
            if (bytes >= options_.page_bytes) break;
        }
        if (out.edges.empty()) return next_stage();
        std::ostringstream k;
        k.precision(17);
        k << last;
        key_ = k.str();
        out.path = "edges";
        return true;
    }
    }
}

// ── el embarque ─────────────────────────────────────────────────────────────

namespace {

bool is_hex64(const std::string& s) {
    return s.size() == 64 && std::all_of(s.begin(), s.end(), [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}

std::optional<std::string> slurp(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return std::nullopt;
    std::ostringstream s;
    s << f.rdbuf();
    return s.str();
}

/* Write `bytes` at `p` whole or not at all: a temporary beside it, renamed. */
bool write_whole(const fs::path& p, std::string_view bytes, std::string& error) {
    fs::path tmp = p;
    tmp += ".part";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        f.write(bytes.data(), (std::streamsize)bytes.size());
        f.flush();
        if (!f) {
            error = "cannot write " + tmp.string();
            return false;
        }
    }
    std::error_code ec;
    fs::rename(tmp, p, ec);
    if (ec) {
        fs::remove(tmp, ec);
        error = "cannot move a partida into place: " + p.string();
        return false;
    }
    return true;
}

} // namespace

EmbarqueWriter::EmbarqueWriter(const fs::path& dir) : dir_(dir) {
    std::error_code ec;
    fs::create_directories(dir_ / "partidas", ec);
    fs::create_directories(dir_ / "embarques", ec);
    if (ec) error_ = "cannot create " + dir_.string() + ": " + ec.message();
}

bool EmbarqueWriter::add(const std::string& name, std::string_view bytes) {
    if (!error_.empty()) return false;
    const auto at = std::lower_bound(names_.begin(), names_.end(), name);
    if (at != names_.end() && *at == name) {
        error_ = "a partida named '" + name + "' is already in this embarque";
        return false;
    }
    names_.insert(at, name);
    PartidaRef r{name, sha256_hex(bytes), bytes.size()};
    const fs::path p = dir_ / "partidas" / r.sha256;
    std::error_code ec;
    if (fs::exists(p, ec) && fs::file_size(p, ec) == bytes.size()) {
        ++reused_; // the same bytes, by their name: already there
    } else {
        if (!write_whole(p, bytes, error_)) return false;
        ++written_;
    }
    partidas_.push_back(std::move(r));
    return true;
}

std::string EmbarqueWriter::finish(const std::string& meta_json) {
    if (!error_.empty()) return {};
    nlohmann::json meta = nlohmann::json::parse(meta_json, nullptr, false);
    if (!meta.is_object()) {
        error_ = "an embarque's meta is a JSON object";
        return {};
    }
    nlohmann::json m;
    m["format"] = "voidrancho.embarque/1";
    m["meta"] = meta;
    m["partidas"] = nlohmann::json::array();
    std::uint64_t total = 0;
    for (const auto& p : partidas_) {
        m["partidas"].push_back({{"name", p.name}, {"sha256", p.sha256}, {"bytes", p.bytes}});
        total += p.bytes;
    }
    m["bytes"] = total;
    const std::string text = m.dump();
    const std::string id = sha256_hex(text);
    const fs::path p = dir_ / "embarques" / (id + ".json");
    std::error_code ec;
    if (!fs::exists(p, ec) && !write_whole(p, text, error_)) return {};
    return id;
}

std::optional<Embarque> read_embarque(const fs::path& dir, const std::string& id, std::string* error) {
    auto fail = [&](const std::string& why) -> std::optional<Embarque> {
        if (error) *error = why;
        return std::nullopt;
    };
    if (!is_hex64(id)) return fail("not an embarque's name: " + id);
    const auto text = slurp(dir / "embarques" / (id + ".json"));
    if (!text) return fail("no embarque " + id + " in " + dir.string());
    if (sha256_hex(*text) != id) return fail("embarque " + id + " does not match its name: the file is damaged");
    const nlohmann::json m = nlohmann::json::parse(*text, nullptr, false);
    if (!m.is_object() || m.value("format", "") != "voidrancho.embarque/1") return fail("not an embarque manifest");
    Embarque e;
    e.id = id;
    e.meta = m.value("meta", nlohmann::json::object()).dump();
    e.bytes = m.value("bytes", 0ull);
    for (const auto& p : m.value("partidas", nlohmann::json::array()))
        e.partidas.push_back({p.value("name", ""), p.value("sha256", ""), p.value("bytes", 0ull)});
    return e;
}

std::optional<std::string> read_partida(const fs::path& dir, const std::string& sha256, std::string* error) {
    auto fail = [&](const std::string& why) -> std::optional<std::string> {
        if (error) *error = why;
        return std::nullopt;
    };
    if (!is_hex64(sha256)) return fail("not a partida's name: " + sha256);
    auto bytes = slurp(dir / "partidas" / sha256);
    if (!bytes) return fail("no partida " + sha256 + " in " + dir.string());
    if (sha256_hex(*bytes) != sha256) return fail("partida " + sha256 + " does not match its name: the file is damaged");
    return bytes;
}

} // namespace rancho
