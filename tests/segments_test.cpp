/* segments_test.cpp — the shared form: segments, manifests, one pointer
 * (rancho/segments.hpp, okf/concepts/tiers.md).
 *
 * The hash first, against the FIPS 180-4 vectors (block boundaries included):
 * a hand-written hash must be proven. Then the promises: a snapshot restores to
 * the same document, provenance and clusters with it; a second snapshot after
 * a small edit writes what changed and reuses the rest; an older snapshot is
 * still there; a commit that loses a race replaces nothing; a corrupt segment
 * is refused; a failed restore leaves the store as it was. */
#include "check.hpp"

#include "rancho/memory.hpp"
#include "rancho/segments.hpp"
#include "rancho/store.hpp"

#include <chrono>
#include <fstream>
#include <random>

namespace fs = std::filesystem;
using nlohmann::json;
using namespace rancho;
using test::plain;

namespace {

fs::path scratch(const std::string& name) { return test::scratch("segments-test", name); }

void sha256_matches_the_fips_vectors() {
    CHECK(sha256_hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(sha256_hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(sha256_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    CHECK(sha256_hex("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrst"
                     "nopqrstu") == "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1");
    CHECK(sha256_hex(std::string(1000000, 'a')) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    CHECK(sha256_hex(std::string(55, 'x')).size() == 64); // the padding edge: one block, then two
    CHECK(sha256_hex(std::string(55, 'x')) != sha256_hex(std::string(56, 'x')));
}

std::string database(int n = 300) {
    VcCore c;
    c.register_glyph(R"({"glyph":"thing","label":"Thing","fields":["note"]})");
    c.dispatch("mantle new org");
    std::vector<std::string> cmds;
    std::mt19937 rng(3);
    for (int i = 0; i < n; ++i) {
        const std::string r = "r" + std::to_string(i);
        cmds.push_back("rune new thing " + r);
        cmds.push_back("set " + r + " note " + arg("note " + std::to_string(i) + " ☕"));
        cmds.push_back("tag " + r + " +g:" + std::to_string(i / 30));
    }
    for (int i = 0; i < n; ++i)
        for (int k = 0; k < 4; ++k)
            cmds.push_back("link r" + std::to_string(i) + " r" + std::to_string((i / 30) * 30 + (int)(rng() % 30)) +
                           " --weight 0." + std::to_string(1 + k));
    cmds.push_back("link ghost r1"); // a link whose source is no rune: it dangles, and must survive
    CHECK(c.dispatch(batch(cmds)).ok);
    c.dispatch("mantle new notes");
    c.dispatch("rune new thing n1");
    c.dispatch("config set org.name " + arg("Ñandú"));
    return c.export_state();
}

void a_snapshot_restores_to_the_same_document() {
    const std::string doc = database();
    Store a(scratch("a.store"));
    JsonStream in = JsonStream::from_text(doc);
    CHECK(a.import(in).ok);
    CHECK(a.cluster("org", 40).ok);
    a.set_source("person:ada");
    {
        json r = json::parse(a.state())["mantles"][0]["runes"][5];
        r["tags"].push_back("checked");
        CHECK(a.begin());
        a.put_rune(a.mantle_row("org"), "r5", r.dump());
        CHECK(a.commit());
    }
    const fs::path dir = scratch("snap");
    SegmentOptions small;
    small.page_bytes = 8 * 1024; // pages for the unclustered mantle, and leaves split when large
    long long edges = 0;
    for (const auto& m : a.mantles()) edges += m.edges;
    CHECK(edges > 1000); // `link` on an existing pair updates it, so not quite 4 × 300 + 1
    const auto s = snapshot(a, dir, small);
    CHECK(s.ok);
    CHECK(s.snapshot == 1);
    CHECK(s.segments > 5);
    CHECK(s.runes == 301);
    CHECK(s.edges == edges);
    CHECK(current_snapshot(dir) == 1);

    Store b(scratch("b.store"));
    const auto r = restore(dir, b);
    CHECK(r.ok);
    CHECK(r.runes == 301 && r.edges == edges);
    CHECK(b.state() == a.state()); // byte for byte, order and all
    // what the document does not carry comes too: provenance, clusters, rows
    std::map<std::string, std::string> by;
    for (const auto& t : b.tags_of("org", "r5")) by[t.tag] = t.source;
    CHECK(by["checked"] == "person:ada" && by["g:0"] == "import");
    CHECK(b.cluster_of("org", "r17") == a.cluster_of("org", "r17") && !b.cluster_of("org", "r17").empty());
    CHECK(!b.cluster_settings("org").empty() && b.cluster_settings("org") == a.cluster_settings("org"));
    const std::string rid = json::parse(doc)["mantles"][0]["runes"][250]["spirit"]["id"];
    CHECK(b.row_of("org", rid) == a.row_of("org", rid));
    CHECK(b.select("org", "g:3 AND NOT r95").size() == 29);
}

void a_small_edit_writes_what_changed() {
    Store a(scratch("edit.store"));
    JsonStream in = JsonStream::from_text(database());
    CHECK(a.import(in).ok);
    CHECK(a.cluster("org", 40).ok);
    const fs::path dir = scratch("edit-snap");
    const auto first = snapshot(a, dir);
    CHECK(first.ok);
    json r = json::parse(a.state())["mantles"][0]["runes"][42];
    r["content"]["note"] = "changed";
    CHECK(a.begin());
    a.put_rune(a.mantle_row("org"), "r42", r.dump());
    CHECK(a.commit());
    const auto second = snapshot(a, dir);
    CHECK(second.ok);
    CHECK(second.snapshot == 2);
    std::cout << "    first: " << first.written << " segments written; after one edit: " << second.written
              << " written, " << second.reused << " reused\n";
    CHECK(second.written == 1); // r42's leaf
    CHECK(second.reused == second.segments - 1);
    CHECK((snapshots(dir) == std::vector<long long>{1, 2}));
    // history: the first snapshot is still the database as it was
    Store old(scratch("old.store"));
    CHECK(restore(dir, old, 1).ok);
    CHECK(json::parse(old.state())["mantles"][0]["runes"][42]["content"]["note"] != "changed");
    Store now(scratch("now.store"));
    CHECK(restore(dir, now).ok);
    CHECK(now.state() == a.state());
}

void a_commit_that_loses_a_race_replaces_nothing() {
    Store a(scratch("race.store"));
    JsonStream in = JsonStream::from_text(database(60));
    CHECK(a.import(in).ok);
    const fs::path dir = scratch("race-snap");
    CHECK(snapshot(a, dir).ok);
    // someone else committed snapshot 2 between our reading ROOT and our writing it
    std::ofstream(dir / "manifests" / "2.json") << "{}";
    const auto lost = snapshot(a, dir);
    CHECK(!lost.ok);
    CHECK(lost.error.find("someone else") != std::string::npos);
    CHECK(current_snapshot(dir) == 1); // ROOT did not move
}

void a_corrupt_segment_is_refused_and_the_store_stays_as_it_was() {
    Store a(scratch("corrupt-src.store"));
    JsonStream in = JsonStream::from_text(database(60));
    CHECK(a.import(in).ok);
    const fs::path dir = scratch("corrupt-snap");
    CHECK(snapshot(a, dir).ok);
    Store b(scratch("corrupt-dst.store"));
    JsonStream other = JsonStream::from_text(R"({"version":1,"mantles":[{"id":"x","name":"before","runes":[]}]})");
    CHECK(b.import(other).ok);
    const std::string before = b.state();
    for (const auto& e : fs::directory_iterator(dir / "segments")) { // one flipped byte, as a failing disk does
        std::fstream f(e.path(), std::ios::in | std::ios::out | std::ios::binary);
        f.seekp(40);
        f.put('#');
        break;
    }
    const auto r = restore(dir, b);
    CHECK(!r.ok);
    CHECK(r.error.find("corrupt") != std::string::npos);
    CHECK(b.state() == before);
}

/* THE SAME PROMISE AS THE IMPORT'S: a snapshot and a restore hold one segment
 * at a time, so 6×10^4 runes cost the memory of a few hundred. */
void a_large_store_snapshots_and_restores_in_constant_memory() {
    const fs::path src = scratch("big.json");
    test::write_big_state(src, 60000);
    Store a(scratch("big-a.store"));
    {
        JsonStream in(src.string());
        CHECK(a.import(in).ok);
    }
    CHECK(a.cluster("big", 2000).ok);
    const fs::path dir = scratch("big-snap");
    const auto base = memory::process_bytes();
    auto t0 = std::chrono::steady_clock::now();
    const auto s = snapshot(a, dir);
    const double snap_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    const double snap_mb = (double)(memory::process_bytes() - std::min(base, memory::process_bytes())) / (1 << 20);
    CHECK(s.ok);
    Store b(scratch("big-b.store"));
    const auto base2 = memory::process_bytes();
    t0 = std::chrono::steady_clock::now();
    const auto r = restore(dir, b);
    const double rest_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    const double rest_mb = (double)(memory::process_bytes() - std::min(base2, memory::process_bytes())) / (1 << 20);
    CHECK(r.ok);
    std::cout << "    6x10^4 runes, " << s.segments << " segments, " << (s.bytes_written >> 20) << " MB: snapshot "
              << snap_s << " s (+" << snap_mb << " MB), restore " << rest_s << " s (+" << rest_mb << " MB)\n";
    CHECK(snap_mb < 64 && rest_mb < 64);
    CHECK(b.rune_count("big") == 60000);
    CHECK(b.select("big", "org-59999 OR org-0").size() == 2);
}

} // namespace

int main() {
    const test::Case cases[] = {
        {"SHA-256 matches the FIPS vectors", sha256_matches_the_fips_vectors},
        {"a snapshot restores to the same document", a_snapshot_restores_to_the_same_document},
        {"a small edit writes what changed", a_small_edit_writes_what_changed},
        {"a commit that loses a race replaces nothing", a_commit_that_loses_a_race_replaces_nothing},
        {"a corrupt segment is refused, and the store stays as it was",
         a_corrupt_segment_is_refused_and_the_store_stays_as_it_was},
        {"a large store snapshots and restores in constant memory",
         a_large_store_snapshots_and_restores_in_constant_memory},
    };
    return test::run(cases);
}
