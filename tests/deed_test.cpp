/* deed_test.cpp — one database, many files (rancho/deed.hpp, okf/concepts/deed.md).
 *
 * The deed's own promises first: it round-trips and keeps what it does not
 * know; a text that is not one is refused with a reason; its check finds what
 * is wrong. Then a database opened from one: a new one has an id and a home; a
 * store is a database of one part; THE PLACEMENT LAW — the same rows in one
 * part or in three give the same document, and Core reads it; a missing part
 * is a region not loaded; a pinned part is that snapshot or nothing; a form
 * this version does not read is kept and skipped; another database's store is
 * not taken as a home; declarations travel with their part; a blob is read
 * from its part and never from outside it. */
#include "check.hpp"

#include "rancho/deed.hpp"
#include "rancho/overlay.hpp"
#include "rancho/segments.hpp"
#include "rancho/store.hpp"

#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>

namespace fs = std::filesystem;
using nlohmann::json;
using namespace rancho;
using test::plain;

namespace {

fs::path scratch(const std::string& name) { return test::scratch("deed-test", name); } // fresh: wiped first
fs::path at(const std::string& name) { return fs::temp_directory_path() / "rancho-deed-test" / name; } // as it is

/* Three mantles, in the order the placement test splits them: two that stay
 * home, and an archive that moves out. */
std::string three_mantles() {
    VcCore c;
    c.register_glyph(R"({"glyph":"thing","label":"Thing","fields":["note"]})");
    std::vector<std::string> cmds;
    for (const char* m : {"org", "notes", "archive"}) {
        cmds.push_back(std::string("mantle new ") + m);
        cmds.push_back(std::string("use ") + m);
        for (int i = 0; i < 40; ++i) {
            const std::string r = std::string(m) + "-" + std::to_string(i);
            cmds.push_back("rune new thing " + r);
            cmds.push_back("set " + r + " note " + arg("note " + std::to_string(i) + " ñ"));
            cmds.push_back("tag " + r + " +g:" + std::to_string(i % 4));
            if (i) cmds.push_back("link " + r + " " + std::string(m) + "-" + std::to_string(i - 1) + " --weight 0.5");
        }
    }
    cmds.push_back("use org");
    cmds.push_back("config set org.name " + arg("Ñandú"));
    CHECK(c.dispatch(batch(cmds)).ok);
    return c.export_state();
}

void import_into(Store& s, const std::string& doc) {
    JsonStream in = JsonStream::from_text(doc);
    CHECK(s.import(in).ok);
}

/* A store holding only `keep` of `from`'s mantles (the others as empty skeletons). */
void copy_mantles(Store& from, Store& into, std::set<std::string> keep) {
    Selection sel = Selection::none();
    sel.whole = std::move(keep);
    import_into(into, from.state(sel));
}

Part part(const std::string& id, const std::string& form, const std::string& path,
          std::vector<std::string> mantles = {}, const std::string& pin = {}) {
    Part p;
    p.id = id;
    p.form = form;
    p.path = path;
    p.mantles = std::move(mantles);
    p.pin = pin;
    return p;
}

Deed deed_of(std::vector<Part> parts) {
    Deed d;
    d.id = mint_database_id();
    d.name = "test";
    d.parts = std::move(parts);
    return d;
}

std::unique_ptr<Database> open_deed(const Deed& d, const fs::path& file, const fs::path& cache = {}) {
    std::string err;
    CHECK(write_deed(d, file, &err));
    DatabaseOptions o;
    o.cache_dir = cache.empty() ? file.parent_path() / "cache" : cache;
    auto db = Database::open(file, o, &err);
    CHECK(db != nullptr);
    if (!db) std::cerr << "  open: " << err << "\n";
    return db;
}

const PartState* state_of(Database& db, const std::string& id) {
    for (const auto& s : db.parts())
        if (s.id == id) return &s;
    return nullptr;
}

std::vector<std::string> mantle_names(const std::string& doc) {
    std::vector<std::string> out;
    const json j = json::parse(doc); // held: a loop over a temporary's member reads freed memory
    for (const auto& m : j["mantles"]) out.push_back(m["name"].get<std::string>());
    return out;
}

// ── the deed ────────────────────────────────────────────────────────────────

void a_deed_round_trips_and_keeps_what_it_does_not_know() {
    const std::string text = R"({
      "rancho": 3, "id": "abc", "name": "Ñandú", "home": "main",
      "parts": [
        {"id": "main", "form": "store", "path": "main.db", "replicas": 2},
        {"id": "cold", "form": "bucket", "url": "s3://somewhere/x", "mantles": ["old"]},
        {"id": "snap", "form": "segments", "path": "snap", "mantles": ["a", "b"],
         "pin": "sha256:)" + std::string(64, 'a') + R"("}
      ],
      "lineage": {"from": "def"}
    })";
    std::string err;
    const auto d = parse_deed(text, &err);
    CHECK(d.has_value());
    if (!d) return;
    CHECK(d->format == 3); // a newer deed is read for what this version knows
    CHECK(d->id == "abc" && d->name == "Ñandú" && d->home == "main");
    CHECK(d->parts.size() == 3);
    CHECK(d->parts[2].mantles == std::vector<std::string>({"a", "b"}));
    const auto again = parse_deed(deed_json(*d));
    CHECK(again.has_value());
    if (!again) return;
    CHECK(deed_json(*again) == deed_json(*d));
    const json j = json::parse(deed_json(*d));
    CHECK(j["lineage"]["from"] == "def");      // the deed's unknown member
    // read, never rewritten: this version cannot tell which newer member an edit makes false
    test::scratch("deed-test", "newer.db");
    std::string why;
    CHECK(!write_deed(*d, scratch("newer.rancho"), &why));
    CHECK(why.find("never rewrites") != std::string::npos);
    CHECK(!fs::exists(at("newer.rancho")));
    {
        Store s(at("newer.db"));
        CHECK(s.ok());
        CHECK(!embed_deed(s, *d, &why));
        CHECK(s.meta("rancho.deed").empty());
    }
    // a member Rancho does not know, in its own format, is kept and noted, never read
    Deed own = *d;
    own.format = Deed::kFormat;
    bool noted = false;
    for (const auto& n : check(own).notes) noted |= n.find("\"lineage\" is not Rancho's") != std::string::npos;
    CHECK(noted);
    CHECK(j["parts"][0]["replicas"] == 2);     // a part's
    CHECK(j["parts"][1]["url"] == "s3://somewhere/x");
    CHECK(j["parts"][1]["form"] == "bucket");  // a form this version does not read, kept
    const auto c = check(*d);
    bool told_newer = false, told_form = false;
    for (const auto& n : c.notes) {
        told_newer |= n.find("newer Rancho") != std::string::npos;
        told_form |= n.find("'bucket'") != std::string::npos;
    }
    CHECK(told_newer && told_form);
}

void a_text_that_is_not_a_deed_is_refused_with_a_reason() {
    std::string err;
    CHECK(!parse_deed("[1,2]", &err) && !err.empty());
    CHECK(!parse_deed(R"({"id":"x","parts":[]})", &err) && err.find("rancho") != std::string::npos);
    CHECK(!parse_deed(R"({"rancho":1,"parts":[]})", &err) && err.find("id") != std::string::npos);
    CHECK(!parse_deed(R"({"rancho":1,"id":"x","parts":[{"form":"store"}]})", &err));
    CHECK(!parse_deed(R"({"rancho":1,"id":"x","parts":[{"id":"a","form":"store","mantles":"m"}]})", &err));
    CHECK(!Database::open(scratch("absent.rancho"), {}, &err) && !err.empty());
}

void check_finds_what_is_wrong() {
    auto errors_with = [](const Deed& d, const std::string& needle) {
        for (const auto& e : check(d).errors)
            if (e.find(needle) != std::string::npos) return true;
        return false;
    };
    CHECK(check(deed_of({part("main", "store", "m.db")})).ok());
    CHECK(errors_with(deed_of({part("a", "store", "a.db"), part("a", "store", "b.db", {"x"})}), "two parts"));
    CHECK(errors_with(deed_of({part("a", "store", "a.db", {"x"}), part("b", "store", "b.db", {"x"})}),
                      "mantle 'x'"));
    CHECK(errors_with(deed_of({part("a", "store", "a.db"), part("b", "segments", "b")}), "every other mantle"));
    CHECK(errors_with(deed_of({part("a", "store", "a.db", {}, "sha256:" + std::string(64, '0'))}), "live"));
    CHECK(errors_with(deed_of({part("a", "store", "a.db"), part("s", "segments", "s", {"x"}, "sha256:xyz")}),
                      "64 lowercase hex"));
    CHECK(errors_with(deed_of({part("a", "store", "a.db"), part("p", "blobs", "p", {"x"})}), "blobs, not mantles"));
    CHECK(errors_with(deed_of({part("a", "store", "")}), "no path"));
    Deed d = deed_of({part("a", "store", "a.db"), part("p", "blobs", "pictures")});
    d.home = "p";
    CHECK(errors_with(d, "must hold rows"));
    d.home = "zzz";
    CHECK(errors_with(d, "not one of the parts"));
    // the first part that names a mantle is where it is read, whatever the check said
    const Deed twice = deed_of({part("a", "store", "a.db", {"x"}), part("b", "store", "b.db", {"x"}),
                                part("c", "store", "c.db")});
    CHECK(twice.part_of("x")->id == "a");
    CHECK(twice.part_of("y")->id == "c");
}

// ── a database opened from its deed ─────────────────────────────────────────

void a_new_database_has_an_id_and_a_home() {
    const fs::path f = scratch("new.rancho");
    test::scratch("deed-test", "new.db");
    std::string err;
    auto db = Database::create(f, "A new one", {}, &err);
    CHECK(db != nullptr);
    if (!db) return;
    CHECK(db->deed().id.size() == 32);
    CHECK(db->home() != nullptr && db->complete());
    CHECK(db->home()->meta("rancho.database") == db->deed().id);
    CHECK(read_deed(f)->parts[0].path == "new.db"); // relative: the folder can move
    CHECK(!Database::create(f, "again", {}, &err)); // never over a deed
    CHECK(err.find("already exists") != std::string::npos);
    CHECK(mint_database_id() != mint_database_id());
}

void one_store_is_a_database_of_one_part() {
    const std::string doc = three_mantles();
    const fs::path sp = scratch("one.db");
    {
        Store s(sp);
        import_into(s, doc);
    }
    Store s(sp);
    const Deed d = deed_for_store(s, sp.parent_path(), "one");
    CHECK(d.parts.size() == 1 && d.parts[0].path == "one.db" && d.parts[0].holds_rest());
    const std::string expect = s.state();
    Selection some = Selection::none();
    some.by_name["org"] = {"org-3", "org-4"};
    const std::string expect_some = s.state(some);
    auto db = open_deed(d, scratch("one.rancho"));
    if (!db) return;
    CHECK(db->complete());
    CHECK(plain(db->state()) == plain(expect));
    CHECK(plain(db->state(some)) == plain(expect_some));
    CHECK(plain(db->state()) == plain(doc));
}

void the_placement_law_one_part_or_three_give_the_same_document() {
    const std::string doc = three_mantles();
    Store whole(scratch("whole.db"));
    import_into(whole, doc);
    // two stores: the archive moves out of the home
    {
        Store home(scratch("home.db")), arch(scratch("arch.db"));
        copy_mantles(whole, home, {"org", "notes"});
        copy_mantles(whole, arch, {"archive"});
    }
    const Deed two = deed_of({part("main", "store", "home.db"), part("archive", "store", "arch.db", {"archive"})});
    {
        auto db = open_deed(two, scratch("two.rancho"));
        if (!db) return;
        CHECK(db->complete());
        CHECK(plain(db->state()) == plain(whole.state()));
        // the empty skeletons each copy kept of the other's mantles are not read, and are noted
        int shadowed = 0;
        for (const auto& n : db->notes()) shadowed += n.find("is not read") != std::string::npos;
        CHECK(shadowed == 3);
        Selection some = Selection::none();
        some.by_name["archive"] = {"archive-7"};
        some.by_tag["org"] = {"g:1"};
        CHECK(plain(db->state(some)) == plain(whole.state(some)));
        CHECK(db->store_of("archive") != db->home());
        CHECK(db->store_of("org") == db->home());
        const auto placed = db->placement();
        CHECK(placed.size() == 3);
        CHECK(placed[2].mantle == "archive" && placed[2].part == "archive" && placed[2].present);
        // Core reads the assembled document as the whole one
        VcCore c(db->state());
        CHECK(test::ls(c, "--tag g:2").size() == 10);
        CHECK(plain(c.export_state())["mantles"] == plain(doc)["mantles"]);
    }
    // three parts: the notes on their own, the archive as segments on a "shared drive"
    {
        Store notes(scratch("notes.db")), arch(at("arch.db"));
        copy_mantles(whole, notes, {"notes"});
        const SnapshotStats st = snapshot(arch, scratch("shared-archive"));
        CHECK(st.ok);
    }
    Deed three = deed_of({part("main", "store", "home.db"), part("notes", "store", "notes.db", {"notes"}),
                          part("archive", "segments", "shared-archive", {"archive"})});
    three.id = two.id; // the same database, re-placed: its id is who it is, whatever its parts
    const fs::path cache = scratch("cache");
    for (int round = 0; round < 2; ++round) { // the second open reads the restored copy, not the segments
        auto db = open_deed(three, scratch("three.rancho"), cache);
        if (!db) return;
        CHECK(db->complete());
        CHECK(plain(db->state()) == plain(whole.state()));
        int copies = 0;
        for (const auto& e : fs::directory_iterator(cache)) copies += e.path().extension() == ".db";
        CHECK(copies == 1);
    }
}

void a_missing_part_is_a_region_not_loaded() {
    const std::string doc = three_mantles();
    {
        Store whole(scratch("whole2.db")), home(scratch("home2.db"));
        import_into(whole, doc);
        copy_mantles(whole, home, {"org", "notes"});
    }
    const Deed d = deed_of({part("main", "store", "home2.db"), part("usb", "store", "E-drive/arch.db", {"archive"}),
                            part("nas", "segments", "//nowhere-host/share/x", {"old-years"})});
    auto db = open_deed(d, scratch("missing.rancho"));
    if (!db) return;
    CHECK(!db->complete());
    CHECK(db->home() != nullptr); // the database opens, and its home is writable
    CHECK(state_of(*db, "usb")->status == PartStatus::Missing);
    CHECK(state_of(*db, "usb")->reason.find("not found") != std::string::npos);
    CHECK(state_of(*db, "nas")->status == PartStatus::Missing);
    CHECK(!fs::exists(scratch("E-drive"))); // opening creates no part's file
    CHECK(mantle_names(db->state()) == std::vector<std::string>({"org", "notes"}));
    bool archive_absent = false, years_absent = false;
    for (const auto& p : db->placement()) {
        archive_absent |= p.mantle == "archive" && !p.present && p.part == "usb";
        years_absent |= p.mantle == "old-years" && !p.present;
    }
    CHECK(archive_absent && years_absent);
    CHECK(db->store_of("archive") == nullptr);
}

void a_pinned_part_is_that_snapshot_or_nothing() {
    const std::string doc = three_mantles();
    const fs::path dir = scratch("pinned-archive");
    std::string first_manifest;
    {
        Store whole(scratch("whole3.db")), arch(scratch("arch3.db")), home(scratch("home3.db"));
        import_into(whole, doc);
        copy_mantles(whole, home, {"org", "notes"});
        copy_mantles(whole, arch, {"archive"});
        CHECK(snapshot(arch, dir).ok);
        std::ifstream m(dir / "manifests" / "1.json", std::ios::binary);
        std::ostringstream b;
        b << m.rdbuf();
        first_manifest = b.str();
        // the archive changes, and a second snapshot moves ROOT
        json r = json::parse(arch.state())["mantles"][2]["runes"][0];
        r["tags"].push_back("edited-later");
        CHECK(arch.begin());
        arch.put_rune(arch.mantle_row("archive"), r["spirit"]["name"].get<std::string>(), r.dump());
        CHECK(arch.commit());
        CHECK(snapshot(arch, dir).ok);
    }
    const std::string pin = "sha256:" + sha256_hex(first_manifest);
    const std::string id = mint_database_id(); // one database, opened through three deeds
    auto deed_with = [&](std::vector<Part> parts) {
        Deed d = deed_of(std::move(parts));
        d.id = id;
        return d;
    };
    auto edited = [](Database& db) { return db.state().find("edited-later") != std::string::npos; };
    {
        auto db = open_deed(deed_with({part("main", "store", "home3.db"),
                                     part("archive", "segments", "pinned-archive", {"archive"}, pin)}),
                            scratch("pinned.rancho"));
        if (!db) return;
        CHECK(db->complete());
        CHECK(!edited(*db)); // the pin holds the first snapshot, though ROOT moved
        CHECK(mantle_names(db->state()).size() == 3);
    }
    {
        auto db = open_deed(deed_with({part("main", "store", "home3.db"),
                                     part("archive", "segments", "pinned-archive", {"archive"})}),
                            scratch("following.rancho"));
        if (!db) return;
        CHECK(edited(*db)); // unpinned, it follows ROOT
    }
    {
        // a database whose home is a snapshot is read-only: nothing to write a working set into
        auto db = open_deed(deed_of({part("snap", "segments", "pinned-archive", {}, pin)}),
                            scratch("read-only.rancho"));
        if (!db) return;
        CHECK(db->complete());
        CHECK(db->home() == nullptr);
        CHECK(db->store_of("archive") != nullptr);
        CHECK(json::parse(db->state())["mantles"].size() == 3);
    }
    {
        auto db = open_deed(deed_with({part("main", "store", "home3.db"),
                                     part("archive", "segments", "pinned-archive", {"archive"},
                                          "sha256:" + std::string(64, 'e'))}),
                            scratch("wrong-pin.rancho"));
        if (!db) return;
        CHECK(state_of(*db, "archive")->status == PartStatus::Mismatch);
        CHECK(!db->complete());
        CHECK(mantle_names(db->state()) == std::vector<std::string>({"org", "notes"}));
    }
}

void a_form_this_version_does_not_read_is_kept_and_skipped() {
    test::scratch("deed-test", "future.db");
    auto made = Database::create(scratch("future.rancho"), "future");
    CHECK(made != nullptr);
    made.reset();
    Deed d = *read_deed(at("future.rancho"));
    Part p = part("cold", "bucket", "", {"old"});
    p.extra = R"({"url":"s3://bucket/x"})";
    d.parts.push_back(p);
    auto db = open_deed(d, at("future.rancho"));
    if (!db) return;
    CHECK(state_of(*db, "cold")->status == PartStatus::NotOpenable);
    CHECK(db->home() != nullptr);
    CHECK(json::parse(deed_json(db->deed()))["parts"][1]["url"] == "s3://bucket/x");
    // it still holds what it names: a stale copy in the home is not read in its place
    CHECK(db->home()->begin());
    db->home()->put_mantle(MantleRow{"old", "mantle_old", R"({"id":"mantle_old","name":"old"})", std::nullopt, true, false}, 0);
    CHECK(db->home()->commit());
    db.reset();
    db = open_deed(d, at("future.rancho"));
    if (!db) return;
    CHECK(db->deed().part_of("old")->id == "cold");
    CHECK(db->store_of("old") == nullptr);
    CHECK(db->state().find("\"old\"") == std::string::npos);
    bool absent = false;
    for (const auto& p : db->placement()) absent |= p.mantle == "old" && p.part == "cold" && !p.present;
    CHECK(absent);
}

void a_home_of_another_database_is_not_written() {
    test::scratch("deed-test", "theirs.db");
    auto theirs = Database::create(scratch("theirs.rancho"), "theirs");
    CHECK(theirs != nullptr);
    if (!theirs) return;
    const std::string their_id = theirs->deed().id;
    theirs.reset();
    // a second deed, a different database, pointed at the first one's store
    auto db = open_deed(deed_of({part("main", "store", "theirs.db")}), scratch("mine.rancho"));
    if (!db) return;
    CHECK(state_of(*db, "main")->status == PartStatus::Mismatch);
    CHECK(state_of(*db, "main")->reason.find(their_id) != std::string::npos);
    CHECK(db->home() == nullptr);
    Store s(at("theirs.db"));
    CHECK(s.meta("rancho.database") == their_id); // untouched
}

void declarations_travel_with_their_part() {
    const char* kEvent = R"({"glyph":"event","label":"Event","fields":["when"]})";
    VcCore a, b;
    CHECK(a.dispatch("mantle new org").ok);
    CHECK(b.dispatch("glyph declare " + arg(kEvent)).ok);
    CHECK(b.dispatch("glyph declare " + arg(R"({"glyph":"thing","label":"Other thing","fields":["x"]})")).ok);
    CHECK(a.dispatch("glyph declare " + arg(R"({"glyph":"thing","label":"Thing","fields":["note"]})")).ok);
    CHECK(b.dispatch("mantle new events").ok);
    CHECK(b.dispatch("rune new event e1").ok);
    {
        Store home(scratch("decl-home.db")), ev(scratch("decl-events.db"));
        import_into(home, a.export_state());
        import_into(ev, b.export_state());
    }
    auto db = open_deed(deed_of({part("main", "store", "decl-home.db"),
                                 part("events", "store", "decl-events.db", {"events"})}),
                        scratch("decl.rancho"));
    if (!db) return;
    const json doc = json::parse(db->state());
    CHECK(doc["glyphs"].contains("event"));                    // the part's declaration came with its mantle
    CHECK(doc["glyphs"]["thing"]["label"] == "Thing");         // the home's wins
    bool noted = false;
    for (const auto& n : db->notes()) noted |= n.find("glyph 'thing'") != std::string::npos;
    CHECK(noted);
    VcCore c(db->state());
    CHECK(c.dispatch("use events").ok);
    CHECK(test::ls(c).size() == 1);
}

void a_blob_is_read_from_its_part_and_never_from_outside_it() {
    test::scratch("deed-test", "pics.db");
    auto made = Database::create(scratch("pics.rancho"), "pics");
    CHECK(made != nullptr);
    if (!made) return;
    made->home()->put_blob("assets/old.png", "in the store");
    made.reset();
    const fs::path pics = scratch("pictures");
    fs::create_directories(pics / "assets");
    std::ofstream(pics / "assets" / "x.png", std::ios::binary) << "picture bytes";
    std::ofstream(scratch("secret.txt"), std::ios::binary) << "outside";
    Deed d = *read_deed(at("pics.rancho"));
    d.parts.push_back(part("pictures", "blobs", "pictures"));
    auto db = open_deed(d, at("pics.rancho"));
    if (!db) return;
    CHECK(db->blob("assets/x.png") == std::optional<std::string>("picture bytes"));
    CHECK(db->blob("assets/old.png") == std::optional<std::string>("in the store")); // the home's, after
    CHECK(!db->blob("../secret.txt"));
    CHECK(!db->blob("assets/../../secret.txt"));
    CHECK(!db->blob(at("secret.txt").string()));
    CHECK(!db->blob("assets/none.png"));
}

void a_mantle_is_placed_by_its_id_and_a_rename_keeps_its_place() {
    const std::string doc = three_mantles();
    const std::string id = mint_database_id();
    {
        Store whole(scratch("whole4.db")), home(scratch("home4.db")), arch(scratch("arch4.db"));
        import_into(whole, doc);
        copy_mantles(whole, home, {"org", "notes"});
        copy_mantles(whole, arch, {"archive"});
    }
    Deed by_name = deed_of({part("main", "store", "home4.db"), part("archive", "store", "arch4.db", {"archive"})});
    by_name.id = id;
    std::string key;
    {
        auto db = open_deed(by_name, scratch("by-name.rancho"));
        if (!db) return;
        bool noted = false;
        for (const auto& n : db->notes()) noted |= n.find("by its name") != std::string::npos;
        CHECK(noted); // read, and a person is told
        const Deed ids = db->deed_by_id();
        key = ids.parts[1].mantles.at(0);
        CHECK(key.rfind("mantle_", 0) == 0); // Core's id for the mantle
        for (const auto& p : db->placement())
            if (p.mantle == "archive") CHECK(p.key == key);
    }
    Deed by_id = by_name;
    by_id.parts[1].mantles = {key};
    // the archive is renamed where it lives, as Core renames: its id stays
    {
        Store arch(at("arch4.db"));
        VcCore c(arch.state());
        CHECK(c.dispatch("mantle rename archive archive-old").ok);
        import_into(arch, c.export_state());
    }
    {
        auto db = open_deed(by_id, scratch("by-id.rancho"));
        if (!db) return;
        CHECK(mantle_names(db->state()) == std::vector<std::string>({"org", "notes", "archive-old"}));
        CHECK(db->store_of("archive-old") != db->home());
        CHECK(db->store_of(key) == db->store_of("archive-old"));
    }
    {
        // named by name, the renamed mantle has no place: it falls to the home, which does not hold it
        auto db = open_deed(by_name, scratch("by-name.rancho"));
        if (!db) return;
        CHECK(mantle_names(db->state()) == std::vector<std::string>({"org", "notes"}));
        bool lost = false;
        for (const auto& n : db->notes()) lost |= n.find("'archive-old' in part 'archive' is not read") != std::string::npos;
        CHECK(lost);
    }
}

void a_database_declares_no_kind_and_a_host_brings_its_expectations() {
    test::scratch("deed-test", "kind.db");
    auto made = Database::create(scratch("kind.rancho"), "kind");
    CHECK(made != nullptr);
    if (!made) return;
    made.reset();
    // the deed says nothing about what kind of database it is
    CHECK(json::parse(deed_json(*read_deed(at("kind.rancho")))).size() == 4); // rancho, id, name, parts

    // the host relies on two things: a scene, and an ink, which builds on a scene
    int ran = 0;
    DatabaseOptions o;
    o.cache_dir = scratch("kind-cache");
    o.expect.push_back(Expectation{"scene", {}, [&](Database& db, DeedCheck& out) {
                                     ++ran;
                                     bool scene = false;
                                     for (const auto& p : db.placement()) scene |= p.mantle == "scene";
                                     if (!scene) out.errors.push_back("a scene has a mantle called 'scene'");
                                 }});
    o.expect.push_back(Expectation{"ink", {"scene"}, [&](Database&, DeedCheck& out) {
                                     ++ran;
                                     out.notes.push_back("checked as an ink");
                                 }});
    std::string err;
    auto db = Database::open(at("kind.rancho"), o, &err);
    CHECK(db != nullptr);
    if (!db) return;
    CHECK(ran == 2); // what it builds on, and its own, each once
    CHECK(!db->meets("scene") && !db->meets("ink") && !db->meets("other"));
    CHECK(db->expectation_check().errors.size() == 1 &&
          db->expectation_check().errors[0].rfind("scene: ", 0) == 0);
    CHECK(db->home() != nullptr); // a host's complaint is reported; the database still opens
    // the scene arrives, and the check passes
    CHECK(db->home()->begin());
    db->home()->put_mantle(MantleRow{"scene", "mantle_scene", R"({"id":"mantle_scene","name":"scene"})",
                                     std::nullopt, true, false},
                           0);
    CHECK(db->home()->commit());
    db.reset();
    db = Database::open(at("kind.rancho"), o, &err);
    if (!db) return;
    CHECK(db->expectation_check().ok());
    CHECK(db->meets("scene") && db->meets("ink"));
    // an expectation that builds on one the host did not give does not hold
    DatabaseOptions half;
    half.cache_dir = o.cache_dir;
    half.expect.push_back(o.expect[1]);
    db = Database::open(at("kind.rancho"), half, &err);
    if (!db) return;
    CHECK(!db->meets("ink"));
    CHECK(db->expectation_check().errors.size() == 1 &&
          db->expectation_check().errors[0].find("did not give") != std::string::npos);
    // a host that expects nothing opens the same database, the same
    db = Database::open(at("kind.rancho"), {}, &err);
    if (!db) return;
    CHECK(db->expectation_check().ok() && !db->meets("ink"));
    CHECK(json::parse(db->state())["mantles"].size() == 1);
}

void a_store_can_carry_its_own_deed() {
    test::scratch("deed-test", "carried.db");
    auto made = Database::create(scratch("carried.rancho"), "carried");
    CHECK(made != nullptr);
    if (!made) return;
    const std::string id = made->deed().id;
    made.reset();
    const fs::path pics = scratch("carried-pictures");
    fs::create_directories(pics);
    std::ofstream(pics / "p.png", std::ios::binary) << "a picture";
    Deed d = *read_deed(at("carried.rancho"));
    d.parts.push_back(part("pictures", "blobs", "carried-pictures"));
    {
        Store s(at("carried.db"));
        std::string err;
        CHECK(embed_deed(s, d, &err));
        Deed other = d;
        other.id = mint_database_id();
        CHECK(!embed_deed(s, other, &err) && err.find("another database") != std::string::npos);
    }
    // no deed beside it, and a new file name: the store is the whole database, and finds itself
    fs::remove(at("carried.rancho"));
    fs::rename(at("carried.db"), scratch("renamed.db"));
    std::string err;
    DatabaseOptions o;
    o.cache_dir = scratch("carried-cache");
    auto db = Database::open(at("renamed.db"), o, &err);
    CHECK(db != nullptr);
    if (!db) return;
    CHECK(db->deed().id == id);
    CHECK(db->complete());
    CHECK(db->home() != nullptr);
    CHECK(db->blob("p.png") == std::optional<std::string>("a picture"));
    // a store that carries nothing is a database of one part
    {
        Store plain_store(scratch("plain.db"));
        import_into(plain_store, three_mantles());
    }
    auto one = Database::open(at("plain.db"), o, &err);
    CHECK(one != nullptr);
    if (!one) return;
    CHECK(one->deed().parts.size() == 1 && one->complete());
    CHECK(mantle_names(one->state()).size() == 3);
    // and a file that is neither is refused with a reason
    std::ofstream(scratch("neither.txt"), std::ios::binary) << "hello";
    CHECK(Database::open(at("neither.txt"), o, &err) == nullptr && !err.empty());
}

/* Void Squid's splice, as overlays (2026-11-09): a project reads a library it
 * does not own, and a person's edits to the library's runes survive the
 * library being rebuilt. Each of Squid's seven rules is a check here. */
void a_persons_edits_over_another_database_survive_its_rebuild() {
    // the library: another database, whose store records its id
    VcCore c;
    c.register_glyph(R"({"glyph":"thing","label":"Thing","fields":["kind","note"]})");
    c.dispatch("mantle new games");
    std::vector<std::string> cmds;
    for (int i = 0; i < 5; ++i) {
        const std::string n = "g" + std::to_string(i);
        cmds.push_back("rune new thing " + n);
        cmds.push_back("set " + n + " kind unknown");
        cmds.push_back("tag " + n + " +old +scanned");
    }
    CHECK(c.dispatch(batch(cmds)).ok);
    const json first = json::parse(c.export_state());
    const std::string games = first["mantles"][0]["id"];
    std::map<std::string, std::string> ids; // name -> spirit id: the toolbox keeps them across rebuilds
    for (const auto& r : first["mantles"][0]["runes"]) ids[r["spirit"]["name"]] = r["spirit"]["id"];
    const std::string lib_id = mint_database_id();
    {
        Store lib(scratch("lib.store"));
        import_into(lib, first.dump());
        lib.set_meta("rancho.database", lib_id);
    }
    { Store home(scratch("project.store")); }

    Part library = part("library", "store", "lib.store", {games});
    library.database = lib_id;
    library.overlay = "project";
    Deed d = deed_of({part("project", "store", "project.store"), library});
    d.home = "project";
    CHECK(check(d).ok());
    const auto again = parse_deed(deed_json(d));
    CHECK(again && again->parts[1].database == lib_id && again->parts[1].overlay == "project");
    auto db = open_deed(d, scratch("project.rancho"));
    if (!db) return;
    CHECK(state_of(*db, "library")->status == PartStatus::Present);

    auto rune_of = [&](const std::string& name) {
        const json doc = json::parse(db->state());
        for (const auto& m : doc["mantles"])
            for (const auto& r : m.value("runes", json::array()))
                if (r["spirit"]["name"] == name) return r;
        return json();
    };
    // the person's Core edits g1 (rules 1, 2, 3, 6), touches g4, and makes a rune of their own (rule 4)
    json g1 = rune_of("g1");
    g1["tags"] = json::array({"scanned", "fav"});
    g1["content"]["kind"] = "boss";
    g1["placement"] = {{"x", 3}, {"y", 4}};
    std::string err;
    CHECK(db->record_overlay("games", g1.dump(), "person", &err));
    json g4 = rune_of("g4");
    g4["tags"].push_back("keep");
    CHECK(db->record_overlay(games, g4.dump(), "person"));
    json mine = rune_of("g0");
    mine["spirit"] = {{"id", "made-1"}, {"name", "mine"}};
    mine["tags"] = json::array({"made:person"});
    CHECK(db->record_overlay("games", mine.dump(), "person"));
    CHECK(db->record_overlay("games", rune_of("g2").dump(), "person")); // unchanged: nothing kept
    CHECK(db->home()->overlays("library", games).size() == 3);

    // the toolbox rebuilds the library: g4 is gone, every kind is now "minion", every rune is tagged "old" again
    json rebuilt = first;
    json runes = json::array();
    for (auto r : first["mantles"][0]["runes"]) {
        if (r["spirit"]["name"] == "g4") continue;
        r["content"]["kind"] = "minion";
        runes.push_back(r);
    }
    rebuilt["mantles"][0]["runes"] = runes;
    {
        Store lib(at("lib.store"));
        import_into(lib, rebuilt.dump());
        CHECK(lib.meta("rancho.database") == lib_id);
    }
    const json g1_now = rune_of("g1");
    CHECK(g1_now["spirit"]["id"] == ids["g1"]);                                      // 5: identity kept
    const auto tags = g1_now["tags"].get<std::vector<std::string>>();
    CHECK(std::count(tags.begin(), tags.end(), "fav") == 1);                          // 1: added stays
    CHECK(std::count(tags.begin(), tags.end(), "old") == 0);                          // 2: removed stays removed
    CHECK(g1_now["content"]["kind"] == "boss");                                       // 3: the person's value wins
    CHECK(g1_now["placement"] == json({{"x", 3}, {"y", 4}}));                         // 6: placement is theirs
    CHECK(rune_of("g0")["content"]["kind"] == "minion");                              // the base shows through
    CHECK(rune_of("mine")["tags"] == json::array({"made:person"}));                   // 4: their rune stays
    CHECK(rune_of("g4").is_null());
    const auto left = db->residuals();                                                // 7: never dropped silently
    CHECK(left.size() == 1 && left[0] == "library/" + games + "/" + ids["g4"]);
    CHECK(overlay::added_tags(db->home()->overlays("library", games).at(ids["g1"])).at("fav") == "person");
    // reverting g1 forgets its record
    CHECK(db->forget_overlay("games", ids["g1"]));
    CHECK(rune_of("g1")["content"]["kind"] == "minion");
    db.reset();

    // a library swapped for another database's is a mismatch, not a substitute
    Part other = library;
    other.database = mint_database_id();
    Deed swapped = deed_of({part("project", "store", "project.store"), other});
    swapped.home = "project";
    swapped.id = d.id;
    auto sdb = open_deed(swapped, at("project.rancho"));
    if (!sdb) return;
    CHECK(state_of(*sdb, "library")->status == PartStatus::Mismatch);
    CHECK(state_of(*sdb, "library")->reason.find(lib_id) != std::string::npos);
}

} // namespace

int main() {
    static const test::Case cases[] = {
        {"a deed round-trips and keeps what it does not know", a_deed_round_trips_and_keeps_what_it_does_not_know},
        {"a text that is not a deed is refused with a reason", a_text_that_is_not_a_deed_is_refused_with_a_reason},
        {"the check finds what is wrong", check_finds_what_is_wrong},
        {"a new database has an id and a home", a_new_database_has_an_id_and_a_home},
        {"one store is a database of one part", one_store_is_a_database_of_one_part},
        {"the placement law: one part or three give the same document",
         the_placement_law_one_part_or_three_give_the_same_document},
        {"a missing part is a region not loaded", a_missing_part_is_a_region_not_loaded},
        {"a pinned part is that snapshot or nothing", a_pinned_part_is_that_snapshot_or_nothing},
        {"a form this version does not read is kept and skipped", a_form_this_version_does_not_read_is_kept_and_skipped},
        {"another database's store is not taken as a home", a_home_of_another_database_is_not_written},
        {"declarations travel with their part", declarations_travel_with_their_part},
        {"a blob is read from its part and never from outside it", a_blob_is_read_from_its_part_and_never_from_outside_it},
        {"a mantle is placed by its id, and a rename keeps its place",
         a_mantle_is_placed_by_its_id_and_a_rename_keeps_its_place},
        {"a database declares no kind; a host brings its expectations", a_database_declares_no_kind_and_a_host_brings_its_expectations},
        {"a store can carry its own deed", a_store_can_carry_its_own_deed},
        {"a person's edits over another database survive its rebuild",
         a_persons_edits_over_another_database_survive_its_rebuild},
    };
    return test::run(cases);
}
