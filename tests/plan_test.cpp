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
    };
    return test::run(cases);
}
