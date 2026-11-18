/* rancho/graph.hpp — graph analysis a C++ host can afford at 10^5–10^6 runes.
 *
 * Void Core's graph analytics (its okf/concepts/graph-analytics.md) are a Python
 * holiday by design — "heavy algorithms never enter the core" — which no C++
 * host can call, and whose betweenness is Brandes, O(V·E): hours at Void
 * Hormiga's stress scale (5×10^5 runes, ~10^6 links). This is the host-side
 * half that SPEC places in hosts, written for that scale and for this family's
 * one non-negotiable property: the same graph gives the same answer, every run,
 * on every machine (fixed node order, a fixed-seed generator of our own, no
 * iteration over unordered containers), so a clustering can be replayed.
 *
 *  - a compact CSR graph (offsets + neighbours + weights): ~12 bytes per edge end;
 *  - connected components (union–find, O(E α(V)));
 *  - PageRank (power iteration, O(k·E));
 *  - COMMUNITIES by the Leiden algorithm (Traag, Waltman & van Eck, Scientific
 *    Reports 9:5233, 2019), optimising modularity with a resolution γ:
 *        Q = 1/(2m) Σ_ij [A_ij − γ k_i k_j / (2m)] δ(c_i, c_j).
 *    Leiden is Louvain (Blondel et al. 2008) plus a REFINEMENT phase that only
 *    merges well-connected subsets, which is what guarantees every community is
 *    connected — Louvain can return communities that fall apart internally,
 *    and a cluster a person browses as "one group" must not;
 *  - a HIERARCHY: Leiden again inside every community larger than a target
 *    size, recursively, until each is small. "Smaller and smaller clusters" is
 *    exactly this tree; a view can show level 1 and expand one community at a
 *    time, so what it draws is bounded by a cluster's size, not the mantle's
 *    (hierarchical aggregation, Elmqvist & Fekete, IEEE TVCG 2010).
 *  - NMI, to measure a clustering against known groups.
 *
 * VOID RANCHO'S SINCE 2026-11-02 (the author's ruling on Void Maiz's Q43 and
 * Rancho's open question §3): written by Void Maiz as voidmaiz/graph.hpp, moved
 * here unchanged but for its namespace. It is the leaf target `voidrancho_graph`
 * (cmake/voidrancho_graph.cmake), which links NOTHING — no SQLite, no Void
 * Core — so a host can take the graph analysis without the database. Void
 * Maiz's voidmaiz/graph.hpp forwards to it (`maiz::graph` names
 * `rancho::graph`) until its hosts move; Maiz's own copy is frozen. A change to
 * a signature here is announced to Void Maiz before it lands: its bridge and
 * tests compile against this header. Depends on nothing; keep it so.
 *
 * The matrix view's PageRank and components (rancho/linalg.hpp) are the same
 * questions asked as semiring products over a directed CSR; these are the
 * undirected, weighted forms clustering reads. Both are kept. */
#pragma once


#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace rancho::graph {

struct Edge {
    std::uint32_t a = 0, b = 0;
    double w = 1.0;
};

/* Undirected, weighted, compressed sparse rows. Parallel edges are summed and
 * self-loops kept (they matter to aggregated graphs). */
struct Graph {
    std::uint32_t n = 0;
    std::vector<std::uint64_t> offsets; // n + 1
    std::vector<std::uint32_t> adj;
    std::vector<double> w;
    std::vector<double> self;           // self-loop weight per node
    double total = 0;                   // m: total edge weight, each edge once

    static Graph from_edges(std::uint32_t n, const std::vector<Edge>& edges);
    double degree(std::uint32_t v) const; // k_v: neighbours' weights + 2 × self-loop
};

/* Component id per node, numbered 0.. in order of each component's smallest node. */
std::vector<std::uint32_t> components(const Graph& g);

std::vector<double> pagerank(const Graph& g, double damping = 0.85, int max_iters = 100,
                             double tolerance = 1e-10);

struct Partition {
    std::vector<std::uint32_t> community; // per node, numbered 0..count-1 by first appearance
    std::uint32_t count = 0;
    double modularity = 0;
};

struct LeidenOptions {
    double resolution = 1.0;      // γ: larger finds smaller communities
    std::uint64_t seed = 1;
    int max_rounds = 20;          // move–refine–aggregate rounds
};

Partition leiden(const Graph& g, const LeidenOptions& options = {});

double modularity(const Graph& g, const std::vector<std::uint32_t>& community, double resolution = 1.0);

/* A tree of communities. `path[v]` is node v's chain from the root: {12, 4, 3}
 * means level-1 community 12, its sub-community 4, and so on. A community at
 * or below `target_size` is a leaf; so is one Leiden will not split. */
struct Hierarchy {
    std::vector<std::vector<std::uint32_t>> path;
    int depth = 0;
};

Hierarchy hierarchy(const Graph& g, std::uint32_t target_size, const LeidenOptions& options = {},
                    int max_depth = 6);

/* Rancho (2026-11-09, Void Hormiga's "the overview"): A TREE ANYONE CAN
 * BROWSE. Leiden at γ = 1 is right that a sparse graph has many small, nearly
 * separate communities (23,607 at the top of Hormiga's stress database), but a
 * root with 23,607 children is no hierarchy. `bound_fanout` adds levels so no
 * node has more than `fanout` children, by Void Maiz's build_tree steps 2-4
 * (voidmaiz/overview.cpp, offered by Hormiga):
 *  1. the tops are merged along their strongest ties per member, none past a
 *     size cap of max(largest, 4·n/fanout), until at most `fanout` remain;
 *  2. what still exceeds it is BUNDLED: the largest half stand alone, the rest
 *     share bundles (packing: nothing links them);
 *  3. any node with more than `fanout` children is SLICED (packing).
 * The new levels are prefixes and inserted digits; a leaf stays a leaf, and
 * the order of leaves under one parent is kept. Deterministic. */
enum class NodeKind : std::uint8_t { Community = 0, Bundle = 1, Slice = 2 };

struct Bounded {
    std::vector<std::vector<std::uint32_t>> path;            // per node, from the root
    std::map<std::vector<std::uint32_t>, NodeKind> packing;  // inner nodes that are packing, by path
    int depth = 0;
};

Bounded bound_fanout(const Graph& g, const Hierarchy& h, std::uint32_t fanout);

/* Normalised mutual information between two labelings, in [0, 1]. */
double nmi(const std::vector<std::uint32_t>& a, const std::vector<std::uint32_t>& b);

} // namespace rancho::graph
