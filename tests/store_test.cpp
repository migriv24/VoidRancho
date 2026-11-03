/* store_test.cpp — the store at rest (okf/concepts/store.md).
 *
 * Void Hormiga's tests/store_test.cpp first, ported: the streaming reader at a
 * buffer of a few bytes so every token crosses a refill; the round trip
 * against a live Void Core; the .miga v3 envelope; selections; an import that
 * fails halfway; and the promise the design rests on, that importing four
 * times the data does not take four times the memory.
 *
 * Then what Rancho adds: weights, direction and ports as columns; a source on
 * every tag; the row index ρ surviving a re-import; typed columns from glyph
 * `kinds`; blobs; and a store Void Hormiga wrote, opened and migrated. */
#include "check.hpp"

#include "rancho/json_stream.hpp"
#include "rancho/memory.hpp"
#include "rancho/store.hpp"

#include "sqlite3.h"

#include <chrono>
#include <fstream>
#include <random>
#include <set>

namespace fs = std::filesystem;
using nlohmann::json;
using namespace rancho;
using test::canonical;
using test::plain;

namespace {

fs::path scratch(const std::string& name) { return test::scratch("store-test", name); }

void write_file(const fs::path& p, const std::string& text) { std::ofstream(p, std::ios::binary) << text; }

const char* kThing = R"({"glyph":"thing","label":"Thing","fields":["note","title"]})";
const char* kMetric = R"({"glyph":"metric","label":"Metric","kind":"measure","fields":[]})";

/* A document with every shape the store has to carry: several mantles (one
 * empty), tags, quantities, a measure, value edges, an undirected edge, unicode,
 * config and a declared glyph. Built through a real core, so it is Core's own
 * format and not this file's idea of it. */
std::string sample_state() {
    VcCore c;
    c.register_glyph(kThing);
    c.register_glyph(kMetric);
    c.dispatch("mantle new org");
    for (int i = 0; i < 40; ++i) {
        const std::string n = "t" + std::to_string(i);
        c.dispatch("rune new thing " + n);
        c.dispatch("set " + n + " note " + arg("note " + std::to_string(i) + " — café ☕ \"quoted\"\n2nd line"));
        c.dispatch("tag " + n + " +c1:" + std::to_string(i % 4) + " +type:thing");
    }
    for (int i = 0; i + 1 < 40; ++i)
        c.dispatch("link t" + std::to_string(i) + " t" + std::to_string(i + 1) + " --relation next");
    c.dispatch("link t0 t5 --relation knows --undirected");
    c.dispatch("rune new metric revenue");
    c.dispatch("measure revenue --level ratio --unit USD");
    c.dispatch("link t3 revenue --weight 1234.5");
    c.dispatch("mantle new notes");
    c.dispatch("rune new thing n1");
    c.dispatch("mantle new empty");
    c.dispatch("config set org.name " + arg("Ñandú & Co"));
    c.dispatch("use org");
    return c.export_state();
}

// ── the reader ────────────────────────────────────────────────────────────

void strings_decode_escapes_and_surrogate_pairs() {
    JsonStream s = JsonStream::from_text(R"("a\"b\\c\/d\n\té😀")");
    std::string out;
    CHECK(s.string(out));
    CHECK(out == "a\"b\\c/d\n\t\xC3\xA9\xF0\x9F\x98\x80");
}

void capture_keeps_the_exact_bytes() {
    const std::string v = "{ \"a\" : [1, 2.5e3, true, null, \"x}]\\\"\"] , \"b\":{} }";
    JsonStream s = JsonStream::from_text("[" + v + ", 7]");
    std::string got;
    CHECK(s.expect('['));
    CHECK(s.capture(got));
    CHECK(got == v);
    CHECK(s.eat(','));
    CHECK(s.capture(got) && got == "7");
    CHECK(s.expect(']'));
}

void a_tiny_buffer_reads_the_same_as_a_large_one() {
    const std::string doc = sample_state();
    const fs::path p = scratch("tiny.json");
    write_file(p, doc);
    for (const std::size_t buffer : {std::size_t(1), std::size_t(3), std::size_t(7), std::size_t(4096)}) {
        JsonStream s(p.string(), buffer);
        std::string got;
        CHECK(s.capture(got));
        CHECK(got == doc);
        CHECK(s.offset() == doc.size());
    }
}

void a_broken_document_says_where() {
    JsonStream s = JsonStream::from_text(R"({"a": "never closed)");
    const bool ok = s.object([&](const std::string&) { return s.skip(); });
    CHECK(!ok);
    CHECK(s.error().find("never closed") != std::string::npos);
    CHECK(s.error().find("at byte") != std::string::npos);
}

void a_long_string_streams_in_pieces() {
    const std::string big(300000, 'Q');
    JsonStream s = JsonStream::from_text("\"" + big + "\"");
    std::size_t total = 0, pieces = 0;
    CHECK(s.string_chunks([&](std::string_view c) {
        total += c.size();
        ++pieces;
    }));
    CHECK(total == big.size());
    CHECK(pieces > 1); // never one 300 KB string in memory at once
}

// ── the store (Hormiga's promises) ────────────────────────────────────────

void a_document_comes_back_as_it_went_in() {
    const std::string doc = sample_state();
    const fs::path src = scratch("in.json"), db = scratch("round.store");
    write_file(src, doc);
    Store st(db);
    CHECK(st.ok());
    JsonStream in(src.string());
    const auto stats = st.import(in);
    CHECK(stats.ok);
    CHECK(stats.mantles == 3);
    CHECK(stats.runes == 42);
    CHECK(stats.edges == 41);
    CHECK(!stats.envelope);
    CHECK(plain(st.state()) == plain(doc));
    // and Void Core reads it back to the same document
    VcCore again(st.state());
    CHECK(plain(again.export_state()) == plain(doc));
    CHECK(st.rune_count("org") == 40 + 1);
    CHECK(st.rune_count() == 42);
}

void the_store_survives_being_reopened() {
    const std::string doc = sample_state();
    const fs::path db = scratch("reopen.store");
    {
        Store st(db);
        JsonStream in = JsonStream::from_text(doc);
        CHECK(st.import(in).ok);
    }
    Store st(db);
    CHECK(st.ok());
    CHECK(st.opened_format() == Store::kFormat);
    CHECK(plain(st.state()) == plain(doc));
}

void a_member_that_was_absent_stays_absent() {
    const std::string doc = R"({"version":1,"mantles":[{"id":"m1","name":"bare"},)"
                            R"({"id":"m2","name":"no-edges","runes":[],"layout":{"zoom":2}}],"config":{}})";
    Store st(scratch("absent.store"));
    JsonStream in = JsonStream::from_text(doc);
    CHECK(st.import(in).ok);
    CHECK(json::parse(st.state()) == json::parse(doc));
}

void a_v3_bundle_imports_its_state_and_pictures() {
    const std::string doc = sample_state();
    // "hello, picture" in base64
    const std::string env = R"({"magic":"MIGA","version":3,"meta":{"name":"Cats","app":"hormiga"},)"
                            R"("rebuild":{},"assets":{"assets/a.txt":"aGVsbG8sIHBpY3R1cmU="},"state":)" +
                            doc + "}";
    const fs::path src = scratch("bundle.miga");
    write_file(src, env);
    { // to a sink, as Hormiga takes them
        Store st(scratch("bundle.store"));
        JsonStream in(src.string(), 5); // tiny buffer: the base64 crosses refills
        std::string path, bytes;
        const auto stats = st.import(in, {}, [&](const std::string& p, const std::string& b) {
            path = p;
            bytes = b;
        });
        CHECK(stats.ok);
        CHECK(stats.envelope);
        CHECK(stats.bundle_name == "Cats");
        CHECK(stats.assets == 1);
        CHECK(path == "assets/a.txt");
        CHECK(bytes == "hello, picture");
        CHECK(plain(st.state()) == plain(doc));
        CHECK(st.blob_paths().empty());
    }
    { // with no sink: the store keeps them
        Store st(scratch("bundle-blobs.store"));
        JsonStream in(src.string());
        CHECK(st.import(in).ok);
        CHECK(st.blob("assets/a.txt").value_or("") == "hello, picture");
    }
}

/* Void Hormiga, 2026-11-02: a host that keeps its pictures beside the store
 * must be able to drop the store's copy, and get the disk back. */
void a_blob_can_be_dropped_and_its_space_given_back() {
    const fs::path p = scratch("blobs-drop.store");
    const std::string doc = sample_state();
    const std::string mb(1 << 20, 'x');
    {
        Store st(p);
        JsonStream in = JsonStream::from_text(doc);
        CHECK(st.import(in).ok);
        for (int i = 0; i < 4; ++i) st.put_blob("assets/" + std::to_string(i) + ".png", mb);
        CHECK(st.blob_paths().size() == 4);
        CHECK(st.remove_blob("assets/0.png"));
        CHECK(!st.remove_blob("assets/0.png")); // already gone
        CHECK(!st.blob("assets/0.png"));
        CHECK(st.blob("assets/1.png").value_or("") == mb);
    }
    const auto full = fs::file_size(p);
    {
        Store st(p);
        CHECK(st.clear_blobs() == 3);
        CHECK(st.blob_paths().empty());
        CHECK(st.clear_blobs() == 0);
    }
    CHECK(fs::file_size(p) + (1 << 20) > full); // freed inside the file, not yet on the disk
    {
        Store st(p);
        CHECK(st.compact());
        CHECK(plain(st.state()) == plain(doc)); // the document untouched
    }
    CHECK(fs::file_size(p) + 3 * (1 << 20) <= full);
}

void a_failed_import_leaves_the_store_as_it_was() {
    const std::string doc = sample_state();
    Store st(scratch("atomic.store"));
    {
        JsonStream in = JsonStream::from_text(doc);
        CHECK(st.import(in).ok);
    }
    const long long row = st.row_of("org", json::parse(doc)["mantles"][0]["runes"][7]["spirit"]["id"]);
    JsonStream in = JsonStream::from_text(doc.substr(0, doc.size() / 2)); // cut off in the middle of the runes
    const auto stats = st.import(in);
    CHECK(!stats.ok);
    CHECK(!stats.error.empty());
    CHECK(plain(st.state()) == plain(doc)); // the first document, whole
    CHECK(st.select("org", "c1:1").size() == 10); // and its indexes with it
    CHECK(st.row_of("org", json::parse(doc)["mantles"][0]["runes"][7]["spirit"]["id"]) == row);
}

void a_selection_holds_its_runes_and_only_their_edges() {
    const std::string doc = sample_state();
    Store st(scratch("sel.store"));
    JsonStream in = JsonStream::from_text(doc);
    CHECK(st.import(in).ok);

    Selection sel = Selection::none();
    sel.by_tag["org"] = {"c1:1"};  // t1, t5, t9, ...
    sel.by_name["org"] = {"t2"};   // and one more by name
    sel.whole.insert("notes");
    const json part = json::parse(st.state(sel));

    std::set<std::string> names;
    std::size_t edges = 0;
    for (const auto& m : part["mantles"]) {
        if (m["name"] == "org") {
            for (const auto& r : m["runes"]) names.insert(r["spirit"]["name"].get<std::string>());
            for (const auto& e : m["layout"]["edges"]) {
                ++edges;
                CHECK(names.count(e["from"].get<std::string>()) == 1);
                CHECK(names.count(e["to"].get<std::string>()) == 1);
            }
        }
        if (m["name"] == "notes") CHECK(m["runes"].size() == 1);
        if (m["name"] == "empty") CHECK(m["runes"].empty());
    }
    CHECK(names.size() == 11); // ten with c1:1, and t2
    CHECK(names.count("t1") && names.count("t2") && names.count("t5") && !names.count("t0"));
    CHECK(edges == 1); // of the `next` chain only t1->t2 has both ends loaded; t0-t5 and t3->revenue do not
    // the part is a state Core opens, and a mantle not chosen is still there, empty
    VcCore c(part.dump());
    c.dispatch("use org");
    CHECK(test::ls(c).size() == 11);
    CHECK(c.dispatch("mantles").text.find("empty") != std::string::npos);
}

// ── what Rancho adds ──────────────────────────────────────────────────────

void weights_direction_and_ports_are_columns() {
    VcCore c;
    c.register_glyph(kThing);
    c.dispatch("mantle new g");
    for (const char* n : {"a", "b", "c"}) c.dispatch(std::string("rune new thing ") + n);
    CHECK(c.dispatch("link a b --relation supports --weight 0.8").ok);
    CHECK(c.dispatch("link b c --relation knows --undirected").ok);
    CHECK(c.dispatch("link a c --relation 3:1").ok);
    Store st(scratch("cols.store"));
    JsonStream in = JsonStream::from_text(c.export_state());
    CHECK(st.import(in).ok);
    sqlite3_stmt* q = nullptr;
    sqlite3_prepare_v2(st.handle(), "SELECT src, dst, relation, weight, directed, from_port, to_port FROM edges ORDER BY pos",
                       -1, &q, nullptr);
    std::vector<std::string> rows;
    while (sqlite3_step(q) == SQLITE_ROW) {
        std::string r;
        for (int i = 0; i < 7; ++i) {
            const auto* t = (const char*)sqlite3_column_text(q, i);
            r += (i ? "|" : "") + std::string(t ? t : "NULL");
        }
        rows.push_back(r);
    }
    sqlite3_finalize(q);
    CHECK(rows.size() == 3);
    CHECK(rows.size() == 3 && rows[0] == "a|b|supports|0.8|1|NULL|NULL");
    CHECK(rows.size() == 3 && rows[1] == "b|c|knows|1.0|0|NULL|NULL");
    CHECK(rows.size() == 3 && rows[2] == "a|c|3:1|1.0|1|3|1");
}

void every_tag_says_who_wrote_it() {
    Store st(scratch("sources.store"));
    JsonStream in = JsonStream::from_text(sample_state());
    CHECK(st.import(in).ok);
    const auto tags = st.tags_of("org", "t1");
    CHECK(tags.size() == 2);
    for (const auto& t : tags) CHECK(t.source == "import");
    // a rewrite keeps the source of a tag that was there, and gives a new one the writer's
    const long long m = st.mantle_row("org");
    json r = json::parse(st.state())["mantles"][0]["runes"][1];
    r["tags"].push_back("vision:cat");
    st.set_source("agent:vision");
    CHECK(st.begin());
    st.put_rune(m, "t1", r.dump());
    CHECK(st.commit());
    std::map<std::string, std::string> by;
    for (const auto& t : st.tags_of("org", "t1")) by[t.tag] = t.source;
    CHECK(by["c1:1"] == "import");
    CHECK(by["vision:cat"] == "agent:vision");
}

void a_rune_keeps_its_row_across_imports() {
    const std::string doc = sample_state();
    const json j = json::parse(doc);
    Store st(scratch("rows.store"));
    {
        JsonStream in = JsonStream::from_text(doc);
        CHECK(st.import(in).ok);
    }
    std::vector<long long> before;
    for (const auto& r : j["mantles"][0]["runes"]) before.push_back(st.row_of("org", r["spirit"]["id"]));
    CHECK(before.front() == 0);
    CHECK(before.back() == (long long)before.size() - 1);
    // the same document with its runes reversed and one removed: rows follow the ids, not the order
    json k = j;
    auto& runes = k["mantles"][0]["runes"];
    std::reverse(runes.begin(), runes.end());
    runes.erase(runes.begin());
    {
        JsonStream in = JsonStream::from_text(k.dump());
        CHECK(st.import(in).ok);
    }
    for (std::size_t i = 0; i + 1 < j["mantles"][0]["runes"].size(); ++i)
        CHECK(st.row_of("org", j["mantles"][0]["runes"][i]["spirit"]["id"]) == before[i]);
    // and a rune never seen before takes the next row, never a freed one
    json fresh = j["mantles"][0]["runes"][0];
    fresh["spirit"] = {{"id", "rune_new"}, {"name", "fresh"}};
    CHECK(st.begin());
    st.put_rune(st.mantle_row("org"), "fresh", fresh.dump());
    CHECK(st.commit());
    CHECK(st.row_of("org", "rune_new") == (long long)before.size());
}

void annotated_fields_become_typed_columns() {
    VcCore c;
    c.dispatch("mantle new events");
    CHECK(c.dispatch("glyph declare " + arg(R"({"glyph":"event","label":"Event","fields":["when","seats","kind","title"],)"
                                            R"("kinds":{"when":{"level":"interval"},"seats":{"level":"ratio","unit":"people"},)"
                                            R"("kind":{"level":"nominal"}}})")).ok);
    const char* dates[] = {"2026-10-01", "2026-10-15", "2026-11-02", "2026-11-30T18:30:00Z", "not a date"};
    for (int i = 0; i < 5; ++i) {
        const std::string n = "e" + std::to_string(i);
        CHECK(c.dispatch("rune new event " + n).ok);
        c.dispatch("set " + n + " when " + arg(dates[i]));
        c.dispatch("set " + n + " seats " + std::to_string(10 * (i + 1)));
        c.dispatch("set " + n + " kind " + (i % 2 ? std::string("talk") : std::string("walk")));
    }
    Store st(scratch("fields.store"));
    JsonStream in = JsonStream::from_text(c.export_state());
    CHECK(st.import(in).ok);
    // November, as a calendar asks: dates are points, indexed as seconds
    const auto nov = st.range("events", "when", 1793491200.0 /* 2026-11-01 */, 1796083200.0 /* 2026-12-01 */);
    CHECK((nov == std::vector<std::string>{"e2", "e3"}));
    CHECK((st.range("events", "seats", 20, 40) == std::vector<std::string>{"e1", "e2", "e3"}));
    CHECK(st.range("events", "title", 0, 1e18).empty()); // not annotated: not a column
    CHECK(st.field_levels()["event"]["kind"] == "nominal");
}

/* The tables Void Hormiga 0.2.3 creates (its src/store/store.cpp, format 1). */
const char* kHormigaFormat1 =
    "CREATE TABLE store_meta(key TEXT PRIMARY KEY, value TEXT);"
    "CREATE TABLE top(key TEXT PRIMARY KEY, pos INTEGER NOT NULL, json TEXT);"
    "CREATE TABLE mantles(id INTEGER PRIMARY KEY, pos INTEGER NOT NULL, name TEXT NOT NULL DEFAULT '',"
    " json TEXT NOT NULL DEFAULT '{}', layout TEXT, has_runes INTEGER NOT NULL DEFAULT 1,"
    " has_edges INTEGER NOT NULL DEFAULT 1, mid TEXT, deleted INTEGER NOT NULL DEFAULT 0, deleted_by TEXT);"
    "CREATE TABLE runes(id INTEGER PRIMARY KEY, mantle INTEGER NOT NULL, pos REAL NOT NULL,"
    " name TEXT NOT NULL, rid TEXT, glyph TEXT, json TEXT NOT NULL, deleted INTEGER NOT NULL DEFAULT 0,"
    " deleted_by TEXT, bytes INTEGER NOT NULL DEFAULT 0);"
    "CREATE TABLE edges(id INTEGER PRIMARY KEY, mantle INTEGER NOT NULL, pos REAL NOT NULL,"
    " src TEXT NOT NULL, dst TEXT NOT NULL, relation TEXT, json TEXT NOT NULL, deleted INTEGER NOT NULL DEFAULT 0,"
    " deleted_by TEXT, bytes INTEGER NOT NULL DEFAULT 0);"
    "CREATE TABLE clusters(rune INTEGER PRIMARY KEY, mantle INTEGER NOT NULL, path TEXT NOT NULL,"
    " always INTEGER NOT NULL DEFAULT 0);"
    "CREATE TABLE est_mantles(mantle INTEGER PRIMARY KEY, runes INTEGER, edges INTEGER, rune_bytes INTEGER,"
    " edge_bytes INTEGER);"
    "CREATE TABLE est_leaves(mantle INTEGER, path TEXT, runes INTEGER, bytes INTEGER, PRIMARY KEY(mantle, path));"
    "CREATE TABLE tags(rune INTEGER NOT NULL, tag TEXT NOT NULL);"
    "INSERT INTO store_meta VALUES('format','1');";

void a_store_hormiga_wrote_opens_and_is_migrated() {
    const std::string doc = sample_state();
    const fs::path ours = scratch("donor.store"), theirs = scratch("hormiga.store");
    {
        Store st(ours);
        JsonStream in = JsonStream::from_text(doc);
        CHECK(st.import(in).ok);
    }
    { // the same rows, in Hormiga's tables
        sqlite3* db = nullptr;
        sqlite3_open(theirs.string().c_str(), &db);
        const std::string sql = std::string(kHormigaFormat1) + "ATTACH '" + ours.string() + "' AS r;"
            "INSERT INTO top SELECT * FROM r.top;"
            "INSERT INTO mantles SELECT * FROM r.mantles;"
            "INSERT INTO runes SELECT * FROM r.runes;"
            "INSERT INTO edges SELECT id,mantle,pos,src,dst,relation,json,deleted,deleted_by,bytes FROM r.edges;"
            "INSERT INTO tags SELECT rune, tag FROM r.tags; DETACH r;";
        CHECK(sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK);
        sqlite3_close(db);
    }
    Store st(theirs);
    CHECK(st.ok());
    CHECK(st.opened_format() == 1);
    CHECK(st.meta("format") == "2");
    CHECK(plain(st.state()) == plain(doc));          // nothing that was there changed
    CHECK(st.select("org", "c1:1 AND type:thing").size() == 10);
    CHECK(st.row_of("org", json::parse(doc)["mantles"][0]["runes"][0]["spirit"]["id"]) == 0);
    sqlite3_stmt* q = nullptr;
    sqlite3_prepare_v2(st.handle(), "SELECT weight FROM edges WHERE dst='revenue'", -1, &q, nullptr);
    CHECK(sqlite3_step(q) == SQLITE_ROW && sqlite3_column_double(q, 0) == 1234.5);
    sqlite3_finalize(q);
    // and it opens a second time without migrating again
    Store again(theirs);
    CHECK(again.ok() && again.opened_format() == 2);
}

struct Run {
    double peak_mb = 0, seconds = 0;
};

Run import_measured(const fs::path& src, const fs::path& db) {
    Store st(db);
    const auto base = memory::process_bytes();
    std::uint64_t peak = base;
    const auto t0 = std::chrono::steady_clock::now();
    JsonStream in(src.string());
    const auto stats = st.import(in, [&](double, const char*) { peak = std::max(peak, memory::process_bytes()); });
    CHECK(stats.ok);
    peak = std::max(peak, memory::process_bytes());
    Run r;
    r.peak_mb = (double)(peak - std::min(peak, base)) / (1024.0 * 1024.0);
    r.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    return r;
}

/* THE PROMISE: memory does not grow with the file. Four times the runes, and
 * the import's extra memory stays under a constant that a 2 GB device can
 * spare. Time is allowed to grow linearly (with a generous margin), never
 * quadratically. */
void importing_four_times_the_data_does_not_take_four_times_the_memory() {
    const fs::path small = scratch("n20k.json"), large = scratch("n80k.json");
    test::write_big_state(small, 20000);
    test::write_big_state(large, 80000);
    const Run a = import_measured(small, scratch("n20k.store"));
    const Run b = import_measured(large, scratch("n80k.store"));
    std::cout << "    20k runes: +" << a.peak_mb << " MB, " << a.seconds << " s;  80k runes: +" << b.peak_mb
              << " MB, " << b.seconds << " s  (file " << fs::file_size(large) / (1024 * 1024) << " MB)\n";
    CHECK(b.peak_mb < 64.0);               // a constant, far below the 2 GB device's budget
    CHECK(b.peak_mb < a.peak_mb * 2 + 16); // and not proportional to the file
    CHECK(b.seconds < a.seconds * 10 + 1); // linear (4x) with room for a noisy machine, not quadratic (16x)
    Store st(fs::temp_directory_path() / "rancho-store-test" / "n80k.store");
    CHECK(st.rune_count("big") == 80000);
}

/* A SELECTION COSTS WHAT IT LOADS, and a query what its narrowest atoms
 * select — never what the mantle holds. Runs after the import test, whose
 * stores it reuses. */
void a_selection_and_a_query_cost_what_they_return() {
    auto timed = [](const char* file) {
        Store st(fs::temp_directory_path() / "rancho-store-test" / file);
        Selection sel = Selection::none();
        for (int i = 0; i < 200; ++i) sel.by_name["big"].push_back("org-" + std::to_string(i * 37));
        double best = 1e9, best_q = 1e9;
        std::size_t bytes = 0, hits = 0;
        for (int k = 0; k < 3; ++k) { // best of three: the first warms the cache
            auto t0 = std::chrono::steady_clock::now();
            bytes = st.state(sel).size();
            best = std::min(best, std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
            t0 = std::chrono::steady_clock::now();
            hits = st.select("big", "org-5 OR org-77 OR org-1555").size();
            best_q = std::min(best_q, std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
        }
        return std::make_tuple(best, bytes, best_q, hits);
    };
    const auto [a, abytes, qa, ha] = timed("n20k.store");
    const auto [b, bbytes, qb, hb] = timed("n80k.store");
    std::cout << "    200 runes from 20k: " << a * 1000 << " ms;  from 80k: " << b * 1000 << " ms\n";
    std::cout << "    a narrow query on 20k: " << qa * 1000 << " ms;  on 80k: " << qb * 1000 << " ms\n";
    CHECK(abytes > 200 * 200 && bbytes > 200 * 200);
    CHECK(b < a * 2.5 + 0.005); // not 4x: the mantle's size does not enter
    CHECK(ha == 3 && hb == 3);
    CHECK(qb < qa * 2.5 + 0.005); // names and rare tags: the mantle's size does not enter
    // an AND is as narrow as its narrowest side, and is answered exactly
    Store st(fs::temp_directory_path() / "rancho-store-test" / "n80k.store");
    CHECK((st.select("big", "c1:3 AND city:x8 AND NOT org-1555", 2) == std::vector<std::string>{"org-294", "org-2816"}));
    CHECK(st.count("big", "c1:3 && city:x8") == (long long)st.select("big", "c1:3 city:x8").size());
}

} // namespace

int main() {
    const test::Case cases[] = {
        {"strings decode escapes and surrogate pairs", strings_decode_escapes_and_surrogate_pairs},
        {"capture keeps the exact bytes", capture_keeps_the_exact_bytes},
        {"a tiny buffer reads the same as a large one", a_tiny_buffer_reads_the_same_as_a_large_one},
        {"a broken document says where", a_broken_document_says_where},
        {"a long string streams in pieces", a_long_string_streams_in_pieces},
        {"a document comes back as it went in", a_document_comes_back_as_it_went_in},
        {"the store survives being reopened", the_store_survives_being_reopened},
        {"a member that was absent stays absent", a_member_that_was_absent_stays_absent},
        {"a v3 bundle imports its state and pictures", a_v3_bundle_imports_its_state_and_pictures},
        {"a blob can be dropped and its space given back", a_blob_can_be_dropped_and_its_space_given_back},
        {"a failed import leaves the store as it was", a_failed_import_leaves_the_store_as_it_was},
        {"a selection holds its runes and only their edges", a_selection_holds_its_runes_and_only_their_edges},
        {"weights, direction and ports are columns", weights_direction_and_ports_are_columns},
        {"every tag says who wrote it", every_tag_says_who_wrote_it},
        {"a rune keeps its row across imports", a_rune_keeps_its_row_across_imports},
        {"annotated fields become typed columns", annotated_fields_become_typed_columns},
        {"a store Hormiga wrote opens and is migrated", a_store_hormiga_wrote_opens_and_is_migrated},
        {"importing 4x the data does not take 4x the memory",
         importing_four_times_the_data_does_not_take_four_times_the_memory},
        {"a selection and a query cost what they return", a_selection_and_a_query_cost_what_they_return},
    };
    return test::run(cases);
}
