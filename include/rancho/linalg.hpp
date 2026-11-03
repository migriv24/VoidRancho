/* rancho/linalg.hpp — graph algorithms as sparse linear algebra
 * (okf/concepts/matrix-view.md, okf/design/storage-research.md §5).
 *
 * The GraphBLAS formulation (Davis, SuiteSparse:GraphBLAS, ACM TOMS Algorithms
 * 1000 and 1037): most of what a database asks of a graph is a sparse
 * matrix–vector product over a chosen SEMIRING — an "add" that combines and a
 * "multiply" that extends along an edge.
 *
 *   reachability, BFS     boolean (∨, ∧)        "everything connected to this contact"
 *   shortest paths        tropical (min, +)     distances along weights
 *   PageRank, centrality  arithmetic (+, ×)     importance
 *   components            (min, second)         weakly connected groups
 *   A·X                   arithmetic (+, ×)     one message-passing (GNN) layer
 *
 * This is the MINIMAL CSR of Rancho's own the research named as the fallback
 * for small devices: compressed sparse rows, float64 values, deterministic
 * (index order everywhere, no unordered containers), no threads. The
 * algorithms are written as products over semirings on purpose, so that
 * SuiteSparse:GraphBLAS (Apache-2.0) can be put behind the same functions when
 * a database outgrows this — the matrices are already the shape it takes.
 *
 * Convention: row = FROM, column = TO. A_ij is the edge i → j. */
#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace rancho::linalg {

struct Triplet {
    std::uint32_t row = 0, col = 0;
    double value = 1.0;
};

/* What to do when two triplets name one cell. */
enum class Duplicates { Last, Sum, Max };

struct Csr {
    std::uint32_t rows = 0, cols = 0;
    std::vector<std::uint64_t> offsets; // rows + 1
    std::vector<std::uint32_t> index;   // the column of each entry, ascending within a row
    std::vector<double> value;

    std::uint64_t nnz() const { return index.size(); }
    /* Triplets in any order; within a cell, `dup` decides (Last: the triplet
     * given last). Entries are sorted by row, then column. */
    static Csr from_triplets(std::uint32_t rows, std::uint32_t cols, std::vector<Triplet> t,
                             Duplicates dup = Duplicates::Last);
    static Csr identity(std::uint32_t n);
    Csr transpose() const;
    /* A_ij where stored, else `absent`. O(log row length). */
    double at(std::uint32_t i, std::uint32_t j, double absent = 0.0) const;
    std::uint64_t row_length(std::uint32_t i) const { return offsets[i + 1] - offsets[i]; }
};

/* A row-major dense matrix: features, embeddings, aggregations. */
struct Dense {
    std::uint32_t rows = 0, cols = 0;
    std::vector<double> v;
    Dense() = default;
    Dense(std::uint32_t r, std::uint32_t c, double fill = 0.0) : rows(r), cols(c), v((std::size_t)r * c, fill) {}
    double& at(std::uint32_t r, std::uint32_t c) { return v[(std::size_t)r * cols + c]; }
    double at(std::uint32_t r, std::uint32_t c) const { return v[(std::size_t)r * cols + c]; }
};

// ── semirings ───────────────────────────────────────────────────────────────
struct PlusTimes {
    static double zero() { return 0.0; }
    static double add(double a, double b) { return a + b; }
    static double mul(double a, double b) { return a * b; }
};
struct MinPlus {
    static double zero() { return std::numeric_limits<double>::infinity(); }
    static double add(double a, double b) { return a < b ? a : b; }
    static double mul(double a, double b) { return a + b; }
};
struct OrAnd { // values are 0 or 1
    static double zero() { return 0.0; }
    static double add(double a, double b) { return (a != 0 || b != 0) ? 1.0 : 0.0; }
    static double mul(double a, double b) { return (a != 0 && b != 0) ? 1.0 : 0.0; }
};
struct MinSecond { // label propagation: take the smallest label reaching a node
    static double zero() { return std::numeric_limits<double>::infinity(); }
    static double add(double a, double b) { return a < b ? a : b; }
    static double mul(double, double b) { return b; }
};

/* y_i = ⊕_j A_ij ⊗ x_j : PULL along out-edges (what i reaches). */
template <class S> std::vector<double> mxv(const Csr& A, const std::vector<double>& x) {
    std::vector<double> y(A.rows, S::zero());
    for (std::uint32_t i = 0; i < A.rows; ++i) {
        double acc = S::zero();
        for (std::uint64_t k = A.offsets[i]; k < A.offsets[i + 1]; ++k) acc = S::add(acc, S::mul(A.value[k], x[A.index[k]]));
        y[i] = acc;
    }
    return y;
}

/* y_j = ⊕_i x_i ⊗ A_ij : PUSH along out-edges (what reaches j). */
template <class S> std::vector<double> vxm(const std::vector<double>& x, const Csr& A) {
    std::vector<double> y(A.cols, S::zero());
    for (std::uint32_t i = 0; i < A.rows; ++i) {
        if (x[i] == S::zero()) continue;
        for (std::uint64_t k = A.offsets[i]; k < A.offsets[i + 1]; ++k)
            y[A.index[k]] = S::add(y[A.index[k]], S::mul(x[i], A.value[k]));
    }
    return y;
}

// ── algorithms ──────────────────────────────────────────────────────────────

/* Hops from `source` along out-edges; -1 where unreachable. Boolean BFS, one
 * vxm per level. */
std::vector<int> bfs_levels(const Csr& A, std::uint32_t source);

/* Shortest distance from `source` along out-edges, by (min,+) relaxation
 * (Bellman–Ford: one vxm per round, at most n−1 rounds). Infinity where
 * unreachable. Empty if a negative cycle is reachable. */
std::vector<double> shortest_paths(const Csr& A, std::uint32_t source);

/* PageRank over out-edges, each edge's share proportional to its weight
 * (`weighted`) or equal. Dangling rows spread their rank evenly. */
std::vector<double> pagerank(const Csr& A, bool weighted = false, double damping = 0.85, int max_iters = 100,
                             double tolerance = 1e-12);

/* Weakly connected components: each node labelled with the smallest index in
 * its component, by (min, second) label propagation over A and Aᵀ. */
std::vector<std::uint32_t> components(const Csr& A);

/* One message-passing step, Â·X: each row the sum (or, `mean`, the average) of
 * its out-neighbours' rows, weighted by the edge. */
Dense aggregate(const Csr& A, const Dense& X, bool mean = false);

/* A + Aᵀ with the given duplicate rule: an undirected view of a directed matrix. */
Csr symmetrize(const Csr& A, Duplicates dup = Duplicates::Max);

} // namespace rancho::linalg
