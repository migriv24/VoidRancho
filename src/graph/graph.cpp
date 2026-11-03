/* graph.cpp — see rancho/graph.hpp. Everything here is deterministic: our own
 * generator, nodes and edges in index order, ties broken toward the smaller
 * index or toward staying put. */
#include "rancho/graph.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <map>
#include <numeric>
#include <utility>

namespace rancho::graph {

namespace {

struct Rng { // splitmix64: tiny, fast, and the same on every platform
    std::uint64_t s;
    std::uint64_t next() {
        std::uint64_t z = (s += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    std::uint32_t below(std::uint32_t n) { return (std::uint32_t)(next() % n); }
};

template <class T> void shuffle(std::vector<T>& v, Rng& r) {
    for (std::size_t i = v.size(); i > 1; --i) std::swap(v[i - 1], v[r.below((std::uint32_t)i)]);
}

// Renumber labels 0.. by first appearance; returns the count.
std::uint32_t renumber(std::vector<std::uint32_t>& label) {
    std::vector<std::uint32_t> map(label.size() + 1, UINT32_MAX);
    std::uint32_t next = 0;
    for (auto& l : label) {
        if (l >= map.size()) map.resize(l + 1, UINT32_MAX);
        if (map[l] == UINT32_MAX) map[l] = next++;
        l = map[l];
    }
    return next;
}

std::vector<double> degrees(const Graph& g) {
    std::vector<double> k(g.n);
    for (std::uint32_t v = 0; v < g.n; ++v) k[v] = g.degree(v);
    return k;
}

/* ── phase 1: fast local moving (Leiden's queue: revisit only what changed) ── */
void move_nodes(const Graph& g, const std::vector<double>& k, std::vector<std::uint32_t>& comm,
                double gamma, Rng& rng) {
    const double two_m = 2 * g.total;
    if (two_m <= 0) return;
    std::vector<double> K(g.n, 0.0);
    std::vector<std::uint32_t> size(g.n, 0);
    for (std::uint32_t v = 0; v < g.n; ++v) { K[comm[v]] += k[v]; ++size[comm[v]]; }
    std::vector<std::uint32_t> empty;
    for (std::uint32_t c = 0; c < g.n; ++c)
        if (size[c] == 0) empty.push_back(c);

    std::vector<std::uint32_t> order(g.n);
    std::iota(order.begin(), order.end(), 0u);
    shuffle(order, rng);
    std::deque<std::uint32_t> queue(order.begin(), order.end());
    std::vector<char> queued(g.n, 1);
    std::vector<double> wto(g.n, 0.0);
    std::vector<std::uint32_t> touched;

    while (!queue.empty()) {
        const std::uint32_t v = queue.front();
        queue.pop_front();
        queued[v] = 0;
        const std::uint32_t old = comm[v];
        touched.clear();
        for (std::uint64_t e = g.offsets[v]; e < g.offsets[v + 1]; ++e) {
            const std::uint32_t u = g.adj[e];
            if (u == v) continue;
            const std::uint32_t c = comm[u];
            if (wto[c] == 0.0) touched.push_back(c);
            wto[c] += g.w[e];
        }
        K[old] -= k[v];
        --size[old];
        std::uint32_t best = old;
        double best_gain = wto[old] - gamma * k[v] * K[old] / two_m;
        for (const std::uint32_t c : touched) {
            const double gain = wto[c] - gamma * k[v] * K[c] / two_m;
            if (gain > best_gain + 1e-12 || (std::fabs(gain - best_gain) <= 1e-12 && c < best && best != old)) {
                best = c;
                best_gain = gain;
            }
        }
        if (best_gain < -1e-12) { // alone is better than anywhere: an empty community
            if (size[old] == 0) best = old;
            else if (!empty.empty()) { best = empty.back(); empty.pop_back(); }
        }
        K[best] += k[v];
        ++size[best];
        if (size[old] == 0 && best != old) empty.push_back(old);
        comm[v] = best;
        if (best != old)
            for (std::uint64_t e = g.offsets[v]; e < g.offsets[v + 1]; ++e) {
                const std::uint32_t u = g.adj[e];
                if (!queued[u] && comm[u] != best) { queued[u] = 1; queue.push_back(u); }
            }
        for (const std::uint32_t c : touched) wto[c] = 0.0;
    }
}

/* ── phase 2: refinement ──────────────────────────────────────────────────────
 * Within each community C, start from singletons and merge a node v only into
 * a refined subset S of C that is WELL CONNECTED to the rest of C, and only if
 * v itself is: E(X, C−X) ≥ γ·K_X·(K_C − K_X)/2m. Leiden chooses among improving
 * subsets at random (θ); this chooses the best gain, which keeps the guarantee
 * (every merge is into a well-connected subset) and makes the result
 * reproducible without a temperature to tune. */
std::vector<std::uint32_t> refine(const Graph& g, const std::vector<double>& k,
                                  const std::vector<std::uint32_t>& comm, double gamma, Rng& rng) {
    const double two_m = 2 * g.total;
    std::vector<std::uint32_t> r(g.n);
    std::iota(r.begin(), r.end(), 0u);
    std::vector<double> KC(g.n, 0.0), Kr(k), ext(g.n, 0.0);
    std::vector<std::uint32_t> rsize(g.n, 1);
    for (std::uint32_t v = 0; v < g.n; ++v) KC[comm[v]] += k[v];
    for (std::uint32_t v = 0; v < g.n; ++v)
        for (std::uint64_t e = g.offsets[v]; e < g.offsets[v + 1]; ++e)
            if (g.adj[e] != v && comm[g.adj[e]] == comm[v]) ext[v] += g.w[e];

    std::vector<std::uint32_t> order(g.n);
    std::iota(order.begin(), order.end(), 0u);
    shuffle(order, rng);
    std::vector<double> wto(g.n, 0.0);
    std::vector<std::uint32_t> touched;
    for (const std::uint32_t v : order) {
        if (rsize[r[v]] != 1 || r[v] != v) continue; // only a node still on its own moves
        const std::uint32_t c = comm[v];
        if (ext[v] < gamma * k[v] * (KC[c] - k[v]) / two_m) continue; // v not well connected
        touched.clear();
        for (std::uint64_t e = g.offsets[v]; e < g.offsets[v + 1]; ++e) {
            const std::uint32_t u = g.adj[e];
            if (u == v || comm[u] != c) continue;
            const std::uint32_t s = r[u];
            if (wto[s] == 0.0) touched.push_back(s);
            wto[s] += g.w[e];
        }
        std::uint32_t best = v;
        double best_gain = 0.0;
        for (const std::uint32_t s : touched) {
            if (ext[s] < gamma * Kr[s] * (KC[c] - Kr[s]) / two_m) continue; // S not well connected
            const double gain = wto[s] - gamma * k[v] * Kr[s] / two_m;
            if (gain > best_gain + 1e-12 || (std::fabs(gain - best_gain) <= 1e-12 && best != v && s < best)) {
                best = s;
                best_gain = gain;
            }
        }
        if (best != v) {
            ext[best] = ext[best] + ext[v] - 2 * wto[best];
            Kr[best] += k[v];
            ++rsize[best];
            rsize[v] = 0;
            r[v] = best;
        }
        for (const std::uint32_t s : touched) wto[s] = 0.0;
    }
    return r;
}

Graph aggregate(const Graph& g, const std::vector<std::uint32_t>& part, std::uint32_t np) {
    std::vector<Edge> edges;
    edges.reserve(g.adj.size() / 2 + g.n);
    for (std::uint32_t v = 0; v < g.n; ++v) {
        if (g.self[v] != 0) edges.push_back({part[v], part[v], g.self[v]});
        for (std::uint64_t e = g.offsets[v]; e < g.offsets[v + 1]; ++e)
            if (g.adj[e] > v) edges.push_back({part[v], part[g.adj[e]], g.w[e]});
    }
    return Graph::from_edges(np, edges);
}

} // namespace

Graph Graph::from_edges(std::uint32_t n, const std::vector<Edge>& edges) {
    Graph g;
    g.n = n;
    g.self.assign(n, 0.0);
    struct Half { std::uint32_t a, b; double w; };
    std::vector<Half> h;
    h.reserve(edges.size() * 2);
    for (const auto& e : edges) {
        if (e.a >= n || e.b >= n) continue;
        g.total += e.w;
        if (e.a == e.b) { g.self[e.a] += e.w; continue; }
        h.push_back({e.a, e.b, e.w});
        h.push_back({e.b, e.a, e.w});
    }
    std::sort(h.begin(), h.end(), [](const Half& x, const Half& y) { return x.a != y.a ? x.a < y.a : x.b < y.b; });
    g.offsets.assign(n + 1, 0);
    for (std::size_t i = 0; i < h.size();) {
        std::size_t j = i;
        double w = 0;
        while (j < h.size() && h[j].a == h[i].a && h[j].b == h[i].b) w += h[j++].w;
        g.adj.push_back(h[i].b);
        g.w.push_back(w);
        ++g.offsets[h[i].a + 1];
        i = j;
    }
    for (std::uint32_t v = 0; v < n; ++v) g.offsets[v + 1] += g.offsets[v];
    return g;
}

double Graph::degree(std::uint32_t v) const {
    double k = 2 * self[v];
    for (std::uint64_t e = offsets[v]; e < offsets[v + 1]; ++e) k += w[e];
    return k;
}

std::vector<std::uint32_t> components(const Graph& g) {
    std::vector<std::uint32_t> parent(g.n);
    std::iota(parent.begin(), parent.end(), 0u);
    const auto find = [&](std::uint32_t x) {
        while (parent[x] != x) x = parent[x] = parent[parent[x]];
        return x;
    };
    for (std::uint32_t v = 0; v < g.n; ++v)
        for (std::uint64_t e = g.offsets[v]; e < g.offsets[v + 1]; ++e) {
            const std::uint32_t a = find(v), b = find(g.adj[e]);
            if (a != b) parent[std::max(a, b)] = std::min(a, b);
        }
    std::vector<std::uint32_t> c(g.n);
    for (std::uint32_t v = 0; v < g.n; ++v) c[v] = find(v);
    renumber(c);
    return c;
}

std::vector<double> pagerank(const Graph& g, double d, int max_iters, double tol) {
    std::vector<double> p(g.n, g.n ? 1.0 / g.n : 0), next(g.n), k = degrees(g);
    for (int it = 0; it < max_iters; ++it) {
        double dangling = 0;
        for (std::uint32_t v = 0; v < g.n; ++v)
            if (k[v] == 0) dangling += p[v];
        const double base = (1 - d) / g.n + d * dangling / g.n;
        std::fill(next.begin(), next.end(), base);
        for (std::uint32_t v = 0; v < g.n; ++v) {
            if (k[v] == 0) continue;
            const double share = d * p[v] / k[v];
            next[v] += share * 2 * g.self[v];
            for (std::uint64_t e = g.offsets[v]; e < g.offsets[v + 1]; ++e) next[g.adj[e]] += share * g.w[e];
        }
        double diff = 0;
        for (std::uint32_t v = 0; v < g.n; ++v) diff += std::fabs(next[v] - p[v]);
        p.swap(next);
        if (diff < tol) break;
    }
    return p;
}

double modularity(const Graph& g, const std::vector<std::uint32_t>& c, double gamma) {
    if (g.total <= 0) return 0;
    std::uint32_t nc = 0;
    for (auto x : c) nc = std::max(nc, x + 1);
    std::vector<double> L(nc, 0.0), K(nc, 0.0);
    for (std::uint32_t v = 0; v < g.n; ++v) {
        K[c[v]] += g.degree(v);
        L[c[v]] += g.self[v];
        for (std::uint64_t e = g.offsets[v]; e < g.offsets[v + 1]; ++e)
            if (g.adj[e] > v && c[g.adj[e]] == c[v]) L[c[v]] += g.w[e];
    }
    double q = 0;
    for (std::uint32_t i = 0; i < nc; ++i) q += L[i] / g.total - gamma * (K[i] / (2 * g.total)) * (K[i] / (2 * g.total));
    return q;
}

Partition leiden(const Graph& g, const LeidenOptions& opt) {
    Partition out;
    Rng rng{opt.seed};
    Graph cur = g;
    std::vector<std::uint32_t> node_of(g.n);
    std::iota(node_of.begin(), node_of.end(), 0u);
    std::vector<std::uint32_t> comm(cur.n);
    std::iota(comm.begin(), comm.end(), 0u);
    for (int round = 0; round < opt.max_rounds; ++round) {
        const std::vector<double> k = degrees(cur);
        move_nodes(cur, k, comm, opt.resolution, rng);
        const std::uint32_t ncomm = renumber(comm);
        if (ncomm == cur.n) break; // nothing merged: converged
        std::vector<std::uint32_t> refined = refine(cur, k, comm, opt.resolution, rng);
        const std::uint32_t nref = renumber(refined);
        if (nref == cur.n) break;  // refinement cannot coarsen further: stop at this partition
        std::vector<std::uint32_t> agg_comm(nref);
        for (std::uint32_t v = 0; v < cur.n; ++v) agg_comm[refined[v]] = comm[v];
        for (auto& x : node_of) x = refined[x];
        cur = aggregate(cur, refined, nref);
        comm = std::move(agg_comm);
    }
    out.community.resize(g.n);
    for (std::uint32_t v = 0; v < g.n; ++v) out.community[v] = comm[node_of[v]];
    out.count = renumber(out.community);
    out.modularity = modularity(g, out.community, opt.resolution);
    return out;
}

Hierarchy hierarchy(const Graph& g, std::uint32_t target, const LeidenOptions& opt, int max_depth) {
    Hierarchy h;
    h.path.assign(g.n, {});
    if (g.n == 0) return h; // nothing to split (and no first node to seed from)
    // a work item: the nodes of one community, at a depth
    struct Item { std::vector<std::uint32_t> nodes; int depth; };
    std::vector<Item> work;
    {
        Item all;
        all.nodes.resize(g.n);
        std::iota(all.nodes.begin(), all.nodes.end(), 0u);
        all.depth = 0;
        work.push_back(std::move(all));
    }
    std::vector<std::uint32_t> local(g.n, UINT32_MAX);
    while (!work.empty()) {
        Item it = std::move(work.back());
        work.pop_back();
        if (it.depth >= max_depth || (it.depth > 0 && it.nodes.size() <= target)) continue;
        for (std::uint32_t i = 0; i < it.nodes.size(); ++i) local[it.nodes[i]] = i;
        std::vector<Edge> edges;
        for (const std::uint32_t v : it.nodes) {
            if (g.self[v] != 0) edges.push_back({local[v], local[v], g.self[v]});
            for (std::uint64_t e = g.offsets[v]; e < g.offsets[v + 1]; ++e) {
                const std::uint32_t u = g.adj[e];
                if (u > v && local[u] != UINT32_MAX) edges.push_back({local[v], local[u], g.w[e]});
            }
        }
        const Graph sub = Graph::from_edges((std::uint32_t)it.nodes.size(), edges);
        LeidenOptions o = opt;
        o.seed = opt.seed + (std::uint64_t)it.depth * 1000003ull + it.nodes.front();
        const Partition p = leiden(sub, o);
        for (const std::uint32_t v : it.nodes) local[v] = UINT32_MAX;
        if (p.count <= 1 && it.depth > 0) continue; // will not split: a leaf
        for (std::uint32_t i = 0; i < it.nodes.size(); ++i) h.path[it.nodes[i]].push_back(p.community[i]);
        h.depth = std::max(h.depth, it.depth + 1);
        if (p.count <= 1) continue;
        std::vector<std::vector<std::uint32_t>> groups(p.count);
        for (std::uint32_t i = 0; i < it.nodes.size(); ++i) groups[p.community[i]].push_back(it.nodes[i]);
        for (auto& gr : groups)
            if (gr.size() > target) work.push_back({std::move(gr), it.depth + 1});
    }
    return h;
}

double nmi(const std::vector<std::uint32_t>& a, const std::vector<std::uint32_t>& b) {
    const std::size_t n = std::min(a.size(), b.size());
    if (n == 0) return 0;
    std::map<std::uint32_t, double> pa, pb;
    std::map<std::pair<std::uint32_t, std::uint32_t>, double> pab;
    for (std::size_t i = 0; i < n; ++i) { pa[a[i]] += 1; pb[b[i]] += 1; pab[{a[i], b[i]}] += 1; }
    const auto H = [&](const std::map<std::uint32_t, double>& p) {
        double h = 0;
        for (const auto& [_, c] : p) h -= (c / n) * std::log(c / n);
        return h;
    };
    double I = 0;
    for (const auto& [ab, c] : pab) I += (c / n) * std::log((c / n) / ((pa[ab.first] / n) * (pb[ab.second] / n)));
    const double ha = H(pa), hb = H(pb);
    return ha + hb > 0 ? 2 * I / (ha + hb) : 1.0;
}

} // namespace rancho::graph
