/* plan_test.cpp — the budget, the clustering, and the plan
 * (okf/concepts/working-set.md, okf/concepts/cluster.md). Ported from Void
 * Hormiga's tests/plan_test.cpp, plus Rancho's own: weights as first-class
 * clustering input, and the settings recorded beside the result.
 *
 * The clustering is tested on databases with PLANTED communities, so there is
 * a right answer: the leaves must recover the groups. The plan is tested
 * against the promise it makes: a database that fits is loaded whole; one that
 * does not is loaded within the budget, around the rune the person is looking
 * at, with its measures. */
#include "check.hpp"

#include "rancho/plan.hpp"
#include "rancho/store.hpp"
#include "rancho/working_set.hpp"

#include "rancho/graph.hpp" // nmi: a clustering measured against the planted truth

#include <map>
#include <random>
#include <set>

namespace fs = std::filesystem;
using nlohmann::json;
using namespace rancho;

namespace {

constexpr std::uint64_t MB = 1024ull * 1024ull;

fs::path scratch(const std::string& name) { return test::scratch("plan-test", name); }

/* `groups` groups of `size` runes: each rune links to 6 others in its group
 * and, rarely, to one outside. Plus a measure every rune has a value on. */
std::string planted(int groups, int size, std::map<std::string, int>* truth = nullptr) {
    VcCore c;
    c.register_glyph(R"({"glyph":"thing","label":"Thing","fields":["note"]})");
    c.dispatch("mantle new org");
    CHECK(c.dispatch("glyph declare " + arg(R"({"glyph":"metric","label":"Metric","kind":"measure","fields":[]})")).ok);
    std::mt19937 rng(5);
    std::vector<std::string> cmds;
    const int n = groups * size;
    for (int i = 0; i < n; ++i) {
        cmds.push_back("rune new thing r" + std::to_string(i));
        cmds.push_back("set r" + std::to_string(i) + " note " + arg(std::string(200, 'x')));
        if (truth) (*truth)["r" + std::to_string(i)] = i / size;
    }
    cmds.push_back("rune new metric score");
    for (int i = 0; i < n; ++i) {
        const int g = i / size;
        for (int k = 0; k < 6; ++k) {
            const int j = g * size + (int)(rng() % (unsigned)size);
            if (j != i) cmds.push_back("link r" + std::to_string(i) + " r" + std::to_string(j));
        }
        if (rng() % 20 == 0) cmds.push_back("link r" + std::to_string(i) + " r" + std::to_string(rng() % (unsigned)n));
        cmds.push_back("link r" + std::to_string(i) + " score --weight " + std::to_string(i % 9));
    }
    CHECK(c.dispatch(batch(cmds)).ok); // one failed command would roll the whole batch back
    c.dispatch("mantle new notes");
    c.dispatch("rune new thing note1");
    c.dispatch("use org");
    return c.export_state();
}

void import(Store& st, const std::string& doc) {
    JsonStream in = JsonStream::from_text(doc);
    CHECK(st.import(in).ok);
}

double nmi_with(Store& st, const std::map<std::string, int>& truth) {
    std::vector<std::uint32_t> a, b;
    std::map<std::string, std::uint32_t> ids;
    for (const auto& [name, group] : truth) {
        const std::string path = st.cluster_of("org", name);
        a.push_back((std::uint32_t)group);
        b.push_back(ids.emplace(path, (std::uint32_t)ids.size()).first->second);
    }
    return rancho::graph::nmi(a, b);
}

/* The names Core holds in `mantle`, read from an export. */
std::vector<std::string> held(VcCore& core, const char* mantle, std::vector<std::pair<std::string, std::string>>* edges = nullptr) {
    std::vector<std::string> out;
    const json doc = json::parse(core.export_state());
    for (const auto& m : doc["mantles"]) {
        if (m["name"] != mantle) continue;
        for (const auto& r : m["runes"]) out.push_back(r["spirit"]["name"]);
        if (edges)
            for (const auto& e : m["layout"]["edges"]) edges->emplace_back(e["from"], e["to"]);
    }
    return out;
}

// ── the budget ──────────────────────────────────────────────────────────────

void the_budget_is_an_eighth_of_the_machine_within_bounds() {
    const auto two = Budget::for_machine(2048 * MB);
    CHECK(two.bytes == 256 * MB);
    CHECK(two.undo_depth == 4);
    CHECK(Budget::for_machine(512 * MB).bytes == 128 * MB);             // never below 128 MB
    CHECK(Budget::for_machine(64ull * 1024 * MB).bytes == 4096 * MB);   // never above 4 GB
    CHECK(Budget::for_machine(32ull * 1024 * MB).undo_depth == 16);
    CHECK(Budget::for_machine(0).bytes == 256 * MB); // unknown: the smallest promised machine
    // 256 MB at depth 4 holds about 5.4 MB of text: (25 + 5.5 × 4) per byte
    CHECK(two.text_bytes() > 5 * MB && two.text_bytes() < 6 * MB);
}

// ── the clustering ──────────────────────────────────────────────────────────

void clustering_recovers_planted_groups() {
    std::map<std::string, int> truth;
    Store st(scratch("cluster.store"));
    import(st, planted(8, 60, &truth));
    CHECK(!st.clustered("org"));
    const auto stats = st.cluster("org", 100);
    CHECK(stats.ok);
    CHECK(stats.runes == 481);
    CHECK(stats.always == 1); // the measure
    CHECK(st.clustered("org"));
    const double nmi = nmi_with(st, truth);
    std::cout << "    " << stats.leaves << " leaves, NMI with the planted groups " << nmi << "\n";
    CHECK(nmi > 0.9);
    CHECK(st.cluster_of("org", "score") == "!");
    for (const auto& l : st.leaves("org")) CHECK(l.runes <= 100); // every leaf at or under the target
}

void the_same_database_clusters_the_same_way_twice() {
    const std::string doc = planted(6, 50);
    Store a(scratch("twice-a.store")), b(scratch("twice-b.store"));
    import(a, doc);
    import(b, doc);
    a.cluster("org", 80);
    b.cluster("org", 80);
    bool same = true;
    for (int i = 0; i < 300; ++i)
        same = same && a.cluster_of("org", "r" + std::to_string(i)) == b.cluster_of("org", "r" + std::to_string(i));
    CHECK(same); // a device that clusters again gets the same leaves
}

/* Rancho: WEIGHTS DECIDE. Two partitions are planted, at right angles: the
 * true groups (i / 40) are tied by a few STRONG `knows` (5.0), and a decoy
 * (i % 6) by many WEAK ones (0.05). By shape alone the decoy is denser and is
 * what clustering finds; read with the weights, the true groups are. */
void weights_are_what_a_weighted_clustering_reads() {
    VcCore c;
    c.register_glyph(R"({"glyph":"thing","label":"Thing","fields":[]})");
    c.dispatch("mantle new org");
    std::mt19937 rng(9);
    std::map<std::string, int> truth;
    std::vector<std::string> cmds;
    const int groups = 6, size = 40, n = groups * size;
    for (int i = 0; i < n; ++i) {
        cmds.push_back("rune new thing r" + std::to_string(i));
        truth["r" + std::to_string(i)] = i / size;
    }
    std::set<std::pair<int, int>> made;
    auto link = [&](int a, int b, const char* w) {
        if (a == b || !made.insert({std::min(a, b), std::max(a, b)}).second) return;
        cmds.push_back("link r" + std::to_string(a) + " r" + std::to_string(b) + " --relation knows --weight " + w);
    };
    for (int i = 0; i < n; ++i) {
        for (int k = 0; k < 3; ++k) link(i, (i / size) * size + (int)(rng() % (unsigned)size), "5.0");
        for (int k = 0; k < 8; ++k) link(i, (int)(rng() % (unsigned)size) * groups + i % groups, "0.05");
    }
    cmds.push_back("link r0 r1 --relation likes --weight 900"); // another relation: not read
    CHECK(c.dispatch(batch(cmds)).ok);
    Store st(scratch("weighted.store"));
    import(st, c.export_state());

    ClusterOptions o;
    o.target = 60;
    o.relations = {"knows"};
    o.weighted = true;
    const auto w = st.cluster("org", o);
    CHECK(w.ok);
    const double with = nmi_with(st, truth);
    CHECK(json::parse(st.cluster_settings("org"))["weighted"] == true);
    CHECK(json::parse(st.cluster_settings("org"))["relations"] == json::array({"knows"}));
    o.weighted = false;
    CHECK(st.cluster("org", o).ok);
    const double without = nmi_with(st, truth);
    std::cout << "    NMI with the weights " << with << ", without " << without << "\n";
    CHECK(with > 0.9);
    CHECK(without < 0.5); // the decoy

    // the rules a weighted clustering keeps
    ClusterOptions two;
    two.weighted = true;
    two.relations = {"knows", "likes"};
    const auto refused = st.cluster("org", two);
    CHECK(!refused.ok);
    CHECK(refused.error.find("commensurable") != std::string::npos);
    two.relations.clear();
    CHECK(!st.cluster("org", two).ok); // "every relation" is more than one
}

// ── the plan ────────────────────────────────────────────────────────────────

void a_database_that_fits_is_loaded_whole() {
    Store st(scratch("fits.store"));
    import(st, planted(4, 20));
    const Plan p = plan(st, Budget::for_machine(2048 * MB));
    CHECK(!p.partial);
    CHECK(p.sel.everything);
    CHECK(p.runes == p.total_runes);
}

void a_database_that_does_not_fit_is_loaded_within_the_budget() {
    Store st(scratch("big.store"));
    import(st, planted(8, 60));
    st.cluster("org", 60);
    Budget tiny;
    tiny.bytes = 8 * MB; // about 170 KB of text: a few leaves, not the database
    tiny.undo_depth = 4;
    const Plan p = plan(st, tiny);
    CHECK(p.partial);
    CHECK(p.sel.whole.count("notes")); // small mantles whole
    CHECK(p.sel.by_cluster.count("org"));
    CHECK(p.runes < p.total_runes);
    const std::string text = st.state(p.sel);
    std::cout << "    budget " << tiny.text_bytes() / 1024 << " KB of text; the plan's state is "
              << text.size() / 1024 << " KB, " << p.runes << " of " << p.total_runes << " runes\n";
    CHECK(text.size() <= tiny.text_bytes() * 5 / 4); // the estimate holds, within a quarter

    VcCore core;
    WorkingSet ws(st, core);
    CHECK(ws.load(p.sel));
    std::vector<std::pair<std::string, std::string>> edges;
    const auto names = held(core, "org", &edges);
    CHECK(std::find(names.begin(), names.end(), "score") != names.end()); // the measure comes with any cluster
    std::size_t values = 0;
    for (const auto& [from, to] : edges) values += to == "score";
    CHECK(values + 1 == names.size()); // every loaded rune's value edge is there
}

void the_rune_in_focus_is_always_in_the_plan() {
    Store st(scratch("focus.store"));
    import(st, planted(8, 60));
    st.cluster("org", 60);
    Budget tiny;
    tiny.bytes = 2 * MB;
    for (const char* who : {"r5", "r250", "r479"}) {
        const Plan p = plan(st, tiny, "org", who);
        VcCore core;
        WorkingSet ws(st, core);
        CHECK(ws.load(p.sel));
        core.dispatch("use org");
        CHECK(core.dispatch(std::string("cat ") + who).ok);
        CHECK(p.focus == st.cluster_of("org", who));
    }
}

void a_large_mantle_with_no_clustering_loads_its_first_runes() {
    Store st(scratch("pages.store"));
    import(st, planted(8, 60));
    Budget tiny;
    tiny.bytes = 2 * MB;
    const Plan p = plan(st, tiny);
    CHECK(p.partial);
    CHECK(p.sel.by_name.count("org"));
    const auto& names = p.sel.by_name.at("org");
    CHECK(!names.empty() && names.front() == "r0");
    CHECK(st.state(p.sel).size() <= tiny.text_bytes() * 5 / 4);
}

// ── the tree's top and the reseña (Rancho, 2026-11-09) ─────────────────────

/* Many small pieces with nothing between most of them: what made the stress
 * database's root 23,607 wide. `pairs` pairs, every tenth tied to the next. */
std::string islands(int pairs) {
    VcCore c;
    c.register_glyph(R"({"glyph":"thing","label":"Thing","fields":[]})");
    c.dispatch("mantle new org");
    std::vector<std::string> cmds;
    for (int i = 0; i < 2 * pairs; ++i) cmds.push_back("rune new thing r" + std::to_string(i));
    for (int i = 0; i < pairs; ++i) {
        cmds.push_back("link r" + std::to_string(2 * i) + " r" + std::to_string(2 * i + 1));
        if (i % 10 == 0 && i + 1 < pairs) cmds.push_back("link r" + std::to_string(2 * i + 1) + " r" + std::to_string(2 * i + 2));
    }
    CHECK(c.dispatch(batch(cmds)).ok);
    c.dispatch("use org");
    return c.export_state();
}

/* Every node of the tree, read through the reseña's own queries. */
struct Seen {
    std::map<std::string, std::tuple<long long, long long, long long, std::string>> nodes; // runes, internal, outgoing, kind
    std::map<std::tuple<std::string, std::string, std::string>, long long> ties;
    long long widest = 0;
};

void walk(Store& st, const std::string& path, Seen& s) {
    const auto n = st.cluster_node("org", path);
    CHECK(n.has_value());
    if (!n) return;
    s.nodes[path] = {n->runes, n->internal, n->outgoing, n->kind};
    const auto kids = st.cluster_children("org", path);
    CHECK((long long)kids.size() == n->children);
    s.widest = std::max(s.widest, (long long)kids.size());
    long long under = 0;
    for (const auto& k : kids) {
        CHECK(k.parent == path);
        under += k.runes;
        walk(st, k.path, s);
    }
    if (!kids.empty()) CHECK(under == n->runes); // a node holds exactly what its children hold
    for (const auto& t : st.cluster_ties("org", path)) s.ties[{path, t.a, t.b}] = t.links;
}

Seen seen(Store& st) {
    Seen s;
    walk(st, "", s);
    return s;
}

void a_tree_has_a_top_anyone_can_browse() {
    Store st(scratch("islands.store"));
    import(st, islands(600));
    ClusterOptions o;
    o.target = 50;
    o.fanout = 16;
    const auto stats = st.cluster("org", o);
    CHECK(stats.ok);
    CHECK(stats.roots <= 16);
    const Seen s = seen(st);
    CHECK(s.widest <= 16);                              // no node opens onto more than the fanout
    CHECK(std::get<0>(s.nodes.at("")) == 1200);         // every rune is under the mantle
    bool packing = false;
    for (const auto& [p, n] : s.nodes) packing = packing || std::get<3>(n) != "community";
    CHECK(packing);                                     // islands with nothing between them are bundled, and say so
    // a pair is still one herd: the levels were added above, never through, Leiden's communities
    int together = 0;
    for (int i = 0; i < 600; i += 7)
        together += st.cluster_of("org", "r" + std::to_string(2 * i)) == st.cluster_of("org", "r" + std::to_string(2 * i + 1));
    CHECK(together >= 80);
    // with no fanout, the tree is Leiden's own: hundreds wide at the top
    Store flat(scratch("islands-flat.store"));
    import(flat, islands(600));
    ClusterOptions f = o;
    f.fanout = 0;
    CHECK(flat.cluster("org", f).roots > 100);
    // a selection by an upper path loads every leaf under it
    const auto tops = st.cluster_children("org", "");
    CHECK(!tops.empty());
    Selection sel = Selection::none();
    sel.by_cluster["org"] = {tops.front().path};
    const json doc = json::parse(st.state(sel));
    CHECK((long long)doc["mantles"][0]["runes"].size() == tops.front().runes);
}

void the_resena_counts_every_tie_and_write_back_keeps_it() {
    Store st(scratch("resena.store"));
    import(st, planted(8, 60));
    ClusterOptions o;
    o.target = 40;
    o.fanout = 3; // forces levels above and slices below
    CHECK(st.cluster("org", o).ok);
    const Seen first = seen(st);
    // the mantle's internal ties are every edge between two clustered runes (value edges to the measure are not)
    long long ties = 0;
    const json whole = json::parse(st.state()); // held: a loop over a temporary's member reads freed memory
    for (const auto& m : whole["mantles"])
        if (m["name"] == "org")
            for (const auto& e : m["layout"]["edges"]) ties += e["to"] != "score";
    CHECK(std::get<1>(first.nodes.at("")) == ties);
    CHECK(std::get<2>(first.nodes.at("")) == 0);
    CHECK(st.refresh_summaries("org"));
    CHECK(seen(st).nodes == first.nodes && seen(st).ties == first.ties);

    VcCore core;
    core.register_glyph(R"({"glyph":"thing","label":"Thing","fields":["note"]})");
    WorkingSet ws(st, core);
    CHECK(ws.load(Selection::all()));
    ws.dispatch("use org");
    auto agrees = [&](const char* what) {
        CHECK(ws.sync());
        const Seen kept = seen(st);
        CHECK(st.refresh_summaries("org"));
        const Seen counted = seen(st);
        const bool same = kept.nodes == counted.nodes && kept.ties == counted.ties;
        if (!same) std::printf("    the reseña drifted after: %s\n", what);
        CHECK(same);
    };
    CHECK(ws.dispatch("link r1 r470").ok);
    agrees("a link across the tree");
    CHECK(ws.dispatch("link r1 r470 --relation knows --weight 2.5").ok);
    agrees("a second relation, weighted");
    CHECK(ws.dispatch("unlink r1 r470").ok);
    agrees("an unlink");
    CHECK(ws.dispatch("rune rm r30").ok);
    agrees("a rune removed with its ties");
    CHECK(ws.dispatch("undo").ok);
    agrees("the removal undone");
    CHECK(ws.dispatch("rune new thing fresh").ok);
    CHECK(ws.dispatch("link fresh r2").ok);
    agrees("a new rune, not yet filed");
    st.assign_cluster("org", "fresh", st.cluster_of("org", "r2"));
    agrees("the new rune filed under a leaf");
    CHECK(std::get<0>(seen(st).nodes.at("")) == 481);
}

void a_herd_is_found_by_path_and_runes_by_name() {
    Store st(scratch("herd.store"));
    import(st, planted(8, 60));
    st.cluster("org", 60);
    // clusters_of answers what cluster_of answers, in one statement
    std::vector<std::string> names;
    for (int i = 0; i < 480; i += 13) names.push_back("r" + std::to_string(i));
    names.push_back("nobody");
    const auto paths = st.clusters_of("org", names);
    CHECK(paths.size() == names.size() - 1);
    for (const auto& n : names)
        if (n != "nobody") CHECK(paths.at(n) == st.cluster_of("org", n));
    // names: a prefix is an index range, in name order; a part of a name, in any case
    const auto pre = st.find_names("org", "r4", NameMatch::Prefix, 0);
    CHECK(pre.size() == 1 + 10 + 80); // r4, r40..r49, r400..r479
    CHECK(std::is_sorted(pre.begin(), pre.end()));
    CHECK(st.find_names("org", "R47", NameMatch::Contains, 0).size() == 1 + 10); // r47, r470..r479
    CHECK(st.find_names("org", "7", NameMatch::Contains, 5).size() == 5);
    CHECK(st.find_names("org", "%", NameMatch::Contains, 0).empty()); // a wildcard is a character
    // a plan grows from a herd chosen by its path
    Budget small;
    small.bytes = 8 * MB; // room for a few herds, not for all eight
    const auto leaves = st.leaf_estimates("org");
    const std::string herd = leaves.back().path;
    const Plan p = plan(st, small, "org", {}, herd);
    CHECK(p.partial);
    CHECK(p.focus == herd);
    const auto& chosen = p.sel.by_cluster.at("org");
    CHECK(std::find(chosen.begin(), chosen.end(), herd) != chosen.end());
}

} // namespace

int main() {
    const test::Case cases[] = {
        {"the budget is an eighth of the machine, within bounds", the_budget_is_an_eighth_of_the_machine_within_bounds},
        {"clustering recovers planted groups", clustering_recovers_planted_groups},
        {"the same database clusters the same way twice", the_same_database_clusters_the_same_way_twice},
        {"weights are what a weighted clustering reads", weights_are_what_a_weighted_clustering_reads},
        {"a database that fits is loaded whole", a_database_that_fits_is_loaded_whole},
        {"a database that does not fit is loaded within the budget",
         a_database_that_does_not_fit_is_loaded_within_the_budget},
        {"the rune in focus is always in the plan", the_rune_in_focus_is_always_in_the_plan},
        {"a large mantle with no clustering loads its first runes",
         a_large_mantle_with_no_clustering_loads_its_first_runes},
        {"a tree has a top anyone can browse", a_tree_has_a_top_anyone_can_browse},
        {"the reseña counts every tie, and write-back keeps it", the_resena_counts_every_tie_and_write_back_keeps_it},
        {"a herd is found by path, and runes by name", a_herd_is_found_by_path_and_runes_by_name},
    };
    return test::run(cases);
}
