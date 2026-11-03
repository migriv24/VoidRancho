/* matrix_test.cpp — the matrix view and the algebra under it
 * (okf/concepts/matrix-view.md, rancho/linalg.hpp).
 *
 * The algorithms are checked against REFERENCE implementations written the
 * ordinary way (a queue for BFS, Dijkstra, a dense power iteration, union–find,
 * a dense product) on seeded random graphs: a semiring formulation that agrees
 * with the textbook on hundreds of graphs is the textbook. The view is checked
 * against a mantle built through a real Core: one matrix per relation, an
 * undirected edge both ways, values with their units, tags with their
 * sources, features by measurement level, and rows that line up across time. */
#include "check.hpp"

#include "rancho/linalg.hpp"
#include "rancho/matrix.hpp"
#include "rancho/store.hpp"

#include <cmath>
#include <deque>
#include <cstring>
#include <fstream>
#include <functional>
#include <numeric>
#include <queue>
#include <random>

namespace fs = std::filesystem;
using nlohmann::json;
using namespace rancho;
using namespace rancho::linalg;

namespace {

Csr random_graph(std::mt19937& rng, std::uint32_t n, std::uint32_t m, bool positive = true) {
    std::vector<Triplet> t;
    for (std::uint32_t k = 0; k < m; ++k) {
        const std::uint32_t a = rng() % n, b = rng() % n;
        const double w = positive ? 0.1 + (double)(rng() % 1000) / 100.0 : (double)(rng() % 1000) / 100.0 - 1.0;
        t.push_back({a, b, w});
    }
    return Csr::from_triplets(n, n, std::move(t), Duplicates::Last);
}

std::vector<std::vector<std::pair<std::uint32_t, double>>> lists(const Csr& A) {
    std::vector<std::vector<std::pair<std::uint32_t, double>>> out(A.rows);
    for (std::uint32_t i = 0; i < A.rows; ++i)
        for (std::uint64_t k = A.offsets[i]; k < A.offsets[i + 1]; ++k) out[i].push_back({A.index[k], A.value[k]});
    return out;
}

// ── the algebra ─────────────────────────────────────────────────────────────

void triplets_build_sorted_rows_and_duplicates_follow_the_rule() {
    const std::vector<Triplet> t = {{1, 2, 1.0}, {0, 3, 2.0}, {1, 0, 3.0}, {1, 2, 5.0}};
    const Csr last = Csr::from_triplets(2, 4, t, Duplicates::Last), sum = Csr::from_triplets(2, 4, t, Duplicates::Sum);
    CHECK(last.nnz() == 3);
    CHECK((last.offsets == std::vector<std::uint64_t>{0, 1, 3}));
    CHECK((last.index == std::vector<std::uint32_t>{3, 0, 2}));
    CHECK(last.at(1, 2) == 5.0);
    CHECK(sum.at(1, 2) == 6.0);
    CHECK(last.at(0, 0, -1) == -1);
    const Csr tt = last.transpose().transpose();
    CHECK(tt.offsets == last.offsets && tt.index == last.index && tt.value == last.value);
    CHECK(last.transpose().at(2, 1) == 5.0);
}

void bfs_agrees_with_a_queue() {
    std::mt19937 rng(1);
    for (int g = 0; g < 200; ++g) {
        const std::uint32_t n = 1 + rng() % 60;
        const Csr A = random_graph(rng, n, rng() % (3 * n));
        const auto adj = lists(A);
        const std::uint32_t s = rng() % n;
        std::vector<int> ref(n, -1);
        std::deque<std::uint32_t> q{s};
        ref[s] = 0;
        while (!q.empty()) {
            const auto u = q.front();
            q.pop_front();
            for (const auto& [v, w] : adj[u])
                if (ref[v] < 0) ref[v] = ref[u] + 1, q.push_back(v);
        }
        CHECK(bfs_levels(A, s) == ref);
    }
}

void min_plus_shortest_paths_agree_with_dijkstra() {
    std::mt19937 rng(2);
    for (int g = 0; g < 200; ++g) {
        const std::uint32_t n = 1 + rng() % 50;
        const Csr A = random_graph(rng, n, rng() % (4 * n));
        const auto adj = lists(A);
        const std::uint32_t s = rng() % n;
        std::vector<double> ref(n, INFINITY);
        ref[s] = 0;
        using Item = std::pair<double, std::uint32_t>;
        std::priority_queue<Item, std::vector<Item>, std::greater<>> pq;
        pq.push({0, s});
        while (!pq.empty()) {
            const auto [d, u] = pq.top();
            pq.pop();
            if (d > ref[u]) continue;
            for (const auto& [v, w] : adj[u])
                if (d + w < ref[v]) ref[v] = d + w, pq.push({ref[v], v});
        }
        const auto got = shortest_paths(A, s);
        CHECK(got.size() == n);
        for (std::uint32_t i = 0; i < n && got.size() == n; ++i)
            CHECK((std::isinf(ref[i]) && std::isinf(got[i])) || std::fabs(ref[i] - got[i]) < 1e-9);
    }
    // a negative cycle is reported, not looped on
    const Csr neg = Csr::from_triplets(3, 3, {{0, 1, 1.0}, {1, 2, -3.0}, {2, 1, 1.0}});
    CHECK(shortest_paths(neg, 0).empty());
}

void pagerank_agrees_with_a_dense_power_iteration() {
    std::mt19937 rng(3);
    for (int g = 0; g < 60; ++g) {
        const std::uint32_t n = 2 + rng() % 30;
        const Csr A = random_graph(rng, n, rng() % (3 * n));
        for (const bool weighted : {false, true}) {
            // the textbook: a dense column-stochastic matrix, dangling nodes to everyone
            std::vector<std::vector<double>> M(n, std::vector<double>(n, 0));
            for (std::uint32_t i = 0; i < n; ++i) {
                double tot = 0;
                for (std::uint64_t k = A.offsets[i]; k < A.offsets[i + 1]; ++k) tot += weighted ? A.value[k] : 1;
                for (std::uint32_t j = 0; j < n; ++j) M[j][i] = tot > 0 ? 0 : 1.0 / n;
                for (std::uint64_t k = A.offsets[i]; k < A.offsets[i + 1]; ++k)
                    M[A.index[k]][i] += (weighted ? A.value[k] : 1) / tot;
            }
            std::vector<double> r(n, 1.0 / n);
            for (int it = 0; it < 300; ++it) {
                std::vector<double> nx(n, 0.15 / n);
                for (std::uint32_t j = 0; j < n; ++j)
                    for (std::uint32_t i = 0; i < n; ++i) nx[j] += 0.85 * M[j][i] * r[i];
                r = nx;
            }
            const auto got = pagerank(A, weighted, 0.85, 300);
            double sum = 0;
            for (std::uint32_t i = 0; i < n; ++i) {
                CHECK(std::fabs(got[i] - r[i]) < 1e-9);
                sum += got[i];
            }
            CHECK(std::fabs(sum - 1.0) < 1e-9);
        }
    }
}

void components_agree_with_union_find() {
    std::mt19937 rng(4);
    for (int g = 0; g < 200; ++g) {
        const std::uint32_t n = 1 + rng() % 80;
        const Csr A = random_graph(rng, n, rng() % (n + 5));
        std::vector<std::uint32_t> parent(n);
        std::iota(parent.begin(), parent.end(), 0u);
        std::function<std::uint32_t(std::uint32_t)> find = [&](std::uint32_t x) {
            return parent[x] == x ? x : parent[x] = find(parent[x]);
        };
        for (std::uint32_t i = 0; i < n; ++i)
            for (std::uint64_t k = A.offsets[i]; k < A.offsets[i + 1]; ++k) {
                const auto a = find(i), b = find(A.index[k]);
                parent[std::max(a, b)] = std::min(a, b);
            }
        std::vector<std::uint32_t> ref(n);
        for (std::uint32_t i = 0; i < n; ++i) ref[i] = find(i);
        // union–find by smallest root gives the smallest member: the same labels
        std::vector<std::uint32_t> smallest(n, UINT32_MAX);
        for (std::uint32_t i = 0; i < n; ++i) smallest[ref[i]] = std::min(smallest[ref[i]], i);
        for (std::uint32_t i = 0; i < n; ++i) ref[i] = smallest[ref[i]];
        CHECK(components(A) == ref);
    }
}

void aggregation_is_the_matrix_product() {
    std::mt19937 rng(5);
    const Csr A = random_graph(rng, 25, 70);
    Dense X(25, 3);
    for (auto& x : X.v) x = (double)(rng() % 100) / 10.0;
    const Dense Y = aggregate(A, X), Ym = aggregate(A, X, true);
    for (std::uint32_t i = 0; i < 25; ++i) {
        double wsum = 0;
        for (std::uint32_t j = 0; j < 25; ++j) wsum += A.at(i, j);
        for (std::uint32_t c = 0; c < 3; ++c) {
            double ref = 0;
            for (std::uint32_t j = 0; j < 25; ++j) ref += A.at(i, j) * X.at(j, c);
            CHECK(std::fabs(Y.at(i, c) - ref) < 1e-9);
            if (wsum != 0) CHECK(std::fabs(Ym.at(i, c) - ref / wsum) < 1e-9);
        }
    }
}

// ── the view ────────────────────────────────────────────────────────────────

const char* kPerson = R"({"glyph":"person","label":"Person","fields":["age","joined","team","note"],)"
                      R"("kinds":{"age":{"level":"ratio","unit":"years"},"joined":{"level":"interval"},)"
                      R"("team":{"level":"nominal"}}})";
const char* kStat = R"({"glyph":"stat","label":"Stat","kind":"measure","fields":[]})";

std::string people() {
    VcCore c;
    c.dispatch("mantle new team");
    CHECK(c.dispatch("glyph declare " + arg(kPerson)).ok);
    CHECK(c.dispatch("glyph declare " + arg(kStat)).ok);
    std::vector<std::string> cmds;
    const char* names[] = {"ada", "bo", "cy", "di"};
    const char* teams[] = {"red", "blue", "red", "green"};
    for (int i = 0; i < 4; ++i) {
        cmds.push_back(std::string("rune new person ") + names[i]);
        cmds.push_back(std::string("set ") + names[i] + " age " + std::to_string(30 + 10 * i));
        cmds.push_back(std::string("set ") + names[i] + " joined 2026-0" + std::to_string(i + 1) + "-01");
        cmds.push_back(std::string("set ") + names[i] + " team " + teams[i]);
        cmds.push_back(std::string("tag ") + names[i] + " +role:" + (i % 2 ? "dev" : "ops"));
    }
    cmds.insert(cmds.end(), {"link ada bo --relation mentors --weight 0.8", "link bo cy --relation mentors --weight 0.5",
                             "link cy di --relation knows --undirected", "link ada di --relation knows",
                             "link ada ghost --relation knows", // a dangling link: legitimate in Core
                             "rune new stat speed", "measure speed --level ratio --unit m/s",
                             "link ada speed --weight 5", "link di speed --weight 7.5"});
    CHECK(c.dispatch(batch(cmds)).ok);
    return c.export_state();
}

void a_mantle_reads_as_one_matrix_per_relation_with_values_tags_and_features() {
    Store st(test::scratch("matrix-test", "view.store"));
    JsonStream in = JsonStream::from_text(people());
    CHECK(st.import(in).ok);
    const MatrixView v = matrix_view(st, "team");
    CHECK(v.ok);
    CHECK((v.names == std::vector<std::string>{"ada", "bo", "cy", "di", "speed"}));
    CHECK((v.rho == std::vector<std::int64_t>{0, 1, 2, 3, 4}));
    const auto i = [&](const char* n) { return (std::uint32_t)v.index_of(n); };
    // relations stay apart; an undirected edge is both directions
    CHECK(v.adjacency.size() == 2);
    const Csr& M = v.adjacency.at("mentors");
    const Csr& K = v.adjacency.at("knows");
    CHECK(M.nnz() == 2 && M.at(i("ada"), i("bo")) == 0.8 && M.at(i("bo"), i("cy")) == 0.5 && M.at(i("bo"), i("ada")) == 0);
    CHECK(K.nnz() == 3 && K.at(i("cy"), i("di")) == 1 && K.at(i("di"), i("cy")) == 1 && K.at(i("ada"), i("di")) == 1);
    CHECK(v.dangling == 1);
    // attribute assertions are V, not adjacency, and carry their unit
    CHECK((v.measures == std::vector<std::string>{"speed"}));
    CHECK((v.units == std::vector<std::string>{"m/s"}));
    CHECK(v.values.at(i("ada"), 0) == 5.0 && v.values.at(i("di"), 0) == 7.5 && v.values.nnz() == 2);
    // tags, with who wrote them
    CHECK((v.tag_names == std::vector<std::string>{"role:dev", "role:ops"}));
    CHECK(v.tags.at(i("bo"), 0) == 1 && v.tags.at(i("ada"), 1) == 1 && v.tags.at(i("ada"), 0) == 0);
    CHECK((v.tag_sources[0] == std::vector<std::string>{"import"}));
    // features by level
    CHECK(v.features.size() == 3);
    std::map<std::string, std::uint32_t> col;
    for (std::uint32_t c = 0; c < v.features.size(); ++c) col[v.features[c].field] = c;
    CHECK(v.X.at(i("cy"), col["age"]) == 50); // ratio: the number
    const auto& joined = v.features[col["joined"]];
    CHECK(joined.level == "interval");
    CHECK(std::fabs(v.X.at(i("ada"), col["joined"]) + v.X.at(i("di"), col["joined"]) +
                    v.X.at(i("bo"), col["joined"]) + v.X.at(i("cy"), col["joined"])) < 1e-6); // centred
    CHECK(v.X.at(i("bo"), col["joined"]) - v.X.at(i("ada"), col["joined"]) == 31 * 86400.0); // differences survive
    const auto& team = v.features[col["team"]];
    CHECK((team.categories == std::vector<std::string>{"blue", "green", "red"}));
    CHECK(v.X.at(i("ada"), col["team"]) == 2 && v.X.at(i("bo"), col["team"]) == 0);
    CHECK(std::isnan(v.X.at(i("speed"), col["age"]))); // a measure has no age
    // and the algorithms run on it: who ada reaches through mentoring
    CHECK((bfs_levels(M, i("ada")) == std::vector<int>{0, 1, 2, -1, -1}));
}

void rows_line_up_across_time() {
    Store st(test::scratch("matrix-test", "time.store"));
    const std::string doc = people();
    JsonStream in = JsonStream::from_text(doc);
    CHECK(st.import(in).ok);
    const MatrixView before = matrix_view(st, "team");
    // a later state: bo is gone, eve is new
    VcCore c(doc);
    c.dispatch("use team");
    CHECK(c.dispatch("rune rm bo").ok);
    CHECK(c.dispatch("rune new person eve").ok);
    JsonStream in2 = JsonStream::from_text(c.export_state());
    CHECK(st.import(in2).ok);
    const MatrixView after = matrix_view(st, "team");
    CHECK((after.names == std::vector<std::string>{"ada", "cy", "di", "speed", "eve"}));
    CHECK((after.rho == std::vector<std::int64_t>{0, 2, 3, 4, 5})); // bo's row is not reused
    for (std::size_t k = 0; k < after.size(); ++k) {
        const long long was = before.index_of(after.names[k]);
        if (was >= 0) CHECK(before.rho[(std::size_t)was] == after.rho[k]); // the same rune, the same row
    }
}

void a_cluster_view_holds_the_cluster_and_its_measures() {
    VcCore c;
    c.dispatch("mantle new g");
    CHECK(c.dispatch("glyph declare " + arg(kStat)).ok);
    std::vector<std::string> cmds;
    for (int i = 0; i < 60; ++i) cmds.push_back("rune new text n" + std::to_string(i));
    for (int i = 0; i < 60; ++i)
        for (int k = 1; k <= 3; ++k) cmds.push_back("link n" + std::to_string(i) + " n" + std::to_string((i / 20) * 20 + (i + k) % 20));
    cmds.push_back("link n0 n25"); // one tie across groups
    cmds.push_back("rune new stat score");
    for (int i = 0; i < 60; ++i) cmds.push_back("link n" + std::to_string(i) + " score --weight " + std::to_string(i));
    CHECK(c.dispatch(batch(cmds)).ok);
    Store st(test::scratch("matrix-test", "cluster.store"));
    JsonStream in = JsonStream::from_text(c.export_state());
    CHECK(st.import(in).ok);
    CHECK(st.cluster("g", 25).ok);
    const std::string leaf = st.cluster_of("g", "n0");
    MatrixOptions o;
    o.clusters = {leaf};
    const MatrixView v = matrix_view(st, "g", o);
    CHECK(v.ok);
    CHECK(v.size() == 21); // twenty runes and the measure
    CHECK(v.index_of("score") >= 0);
    CHECK(v.boundary == 1); // n0 -> n25 leaves the cluster
    CHECK(v.values.nnz() == 20);
    const Csr& A = v.adjacency.at("");
    CHECK(A.nnz() == 60);
}

void the_training_export_writes_arrays_numpy_reads() {
    Store st(test::scratch("matrix-test", "export.store"));
    JsonStream in = JsonStream::from_text(people());
    CHECK(st.import(in).ok);
    const MatrixView v = matrix_view(st, "team");
    const fs::path dir = test::scratch("matrix-test", "export");
    std::string error;
    CHECK(export_npy(v, dir, &error));
    CHECK(error.empty());
    const json m = json::parse(std::ifstream(dir / "manifest.json"));
    CHECK(m["format"] == "voidrancho.matrix-view/1");
    CHECK(m["n"] == 5);
    CHECK(m["adjacency"].size() == 2);
    CHECK(m["values"]["units"][0] == "m/s");
    // the .npy header numpy expects, and the data after it
    std::ifstream f(dir / "rho.npy", std::ios::binary);
    std::string bytes((std::istreambuf_iterator<char>(f)), {});
    CHECK(bytes.compare(0, 6, "\x93NUMPY") == 0);
    const std::size_t hlen = (unsigned char)bytes[8] | ((unsigned char)bytes[9] << 8);
    CHECK((10 + hlen) % 64 == 0);
    CHECK(bytes.find("'descr': '<i8'") != std::string::npos && bytes.find("'shape': (5,)") != std::string::npos);
    CHECK(bytes.size() == 10 + hlen + 5 * 8);
    std::int64_t last = 0;
    std::memcpy(&last, bytes.data() + 10 + hlen + 4 * 8, 8);
    CHECK(last == 4);
    CHECK(fs::exists(dir / "X.npy") && fs::exists(dir / "A0_indptr.npy") && fs::exists(dir / "T_data.npy"));
}

} // namespace

int main() {
    const test::Case cases[] = {
        {"triplets build sorted rows, and duplicates follow the rule",
         triplets_build_sorted_rows_and_duplicates_follow_the_rule},
        {"BFS agrees with a queue", bfs_agrees_with_a_queue},
        {"(min,+) shortest paths agree with Dijkstra", min_plus_shortest_paths_agree_with_dijkstra},
        {"PageRank agrees with a dense power iteration", pagerank_agrees_with_a_dense_power_iteration},
        {"components agree with union-find", components_agree_with_union_find},
        {"aggregation is the matrix product", aggregation_is_the_matrix_product},
        {"a mantle reads as one matrix per relation, with values, tags and features",
         a_mantle_reads_as_one_matrix_per_relation_with_values_tags_and_features},
        {"rows line up across time", rows_line_up_across_time},
        {"a cluster view holds the cluster and its measures", a_cluster_view_holds_the_cluster_and_its_measures},
        {"the training export writes arrays numpy reads", the_training_export_writes_arrays_numpy_reads},
    };
    return test::run(cases);
}
