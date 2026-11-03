/* linalg.cpp — see rancho/linalg.hpp. */
#include "rancho/linalg.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace rancho::linalg {

Csr Csr::from_triplets(std::uint32_t rows, std::uint32_t cols, std::vector<Triplet> t, Duplicates dup) {
    // stable: among equal cells the original order survives, so Last means the last given
    std::stable_sort(t.begin(), t.end(), [](const Triplet& a, const Triplet& b) {
        return a.row != b.row ? a.row < b.row : a.col < b.col;
    });
    Csr m;
    m.rows = rows;
    m.cols = cols;
    m.offsets.assign((std::size_t)rows + 1, 0);
    for (std::size_t k = 0; k < t.size();) {
        std::size_t e = k + 1;
        double v = t[k].value;
        while (e < t.size() && t[e].row == t[k].row && t[e].col == t[k].col) {
            if (dup == Duplicates::Last) v = t[e].value;
            else if (dup == Duplicates::Sum) v += t[e].value;
            else v = std::max(v, t[e].value);
            ++e;
        }
        m.index.push_back(t[k].col);
        m.value.push_back(v);
        ++m.offsets[(std::size_t)t[k].row + 1];
        k = e;
    }
    for (std::uint32_t i = 0; i < rows; ++i) m.offsets[(std::size_t)i + 1] += m.offsets[i];
    return m;
}

Csr Csr::identity(std::uint32_t n) {
    std::vector<Triplet> t;
    for (std::uint32_t i = 0; i < n; ++i) t.push_back({i, i, 1.0});
    return from_triplets(n, n, std::move(t));
}

Csr Csr::transpose() const {
    Csr m;
    m.rows = cols;
    m.cols = rows;
    m.offsets.assign((std::size_t)cols + 1, 0);
    for (const auto c : index) ++m.offsets[(std::size_t)c + 1];
    for (std::uint32_t j = 0; j < cols; ++j) m.offsets[(std::size_t)j + 1] += m.offsets[j];
    m.index.resize(index.size());
    m.value.resize(value.size());
    std::vector<std::uint64_t> next(m.offsets.begin(), m.offsets.end() - 1);
    for (std::uint32_t i = 0; i < rows; ++i) // rows in order, so each output row stays sorted
        for (std::uint64_t k = offsets[i]; k < offsets[(std::size_t)i + 1]; ++k) {
            const std::uint64_t at = next[index[k]]++;
            m.index[at] = i;
            m.value[at] = value[k];
        }
    return m;
}

double Csr::at(std::uint32_t i, std::uint32_t j, double absent) const {
    const auto b = index.begin() + (std::ptrdiff_t)offsets[i], e = index.begin() + (std::ptrdiff_t)offsets[(std::size_t)i + 1];
    const auto it = std::lower_bound(b, e, j);
    return it != e && *it == j ? value[(std::size_t)(it - index.begin())] : absent;
}

std::vector<int> bfs_levels(const Csr& A, std::uint32_t source) {
    std::vector<int> level(A.rows, -1);
    if (source >= A.rows) return level;
    std::vector<double> frontier(A.rows, 0.0);
    frontier[source] = 1.0;
    level[source] = 0;
    for (int depth = 1;; ++depth) {
        std::vector<double> next = vxm<OrAnd>(frontier, A);
        bool any = false;
        for (std::uint32_t j = 0; j < A.rows; ++j) {
            // mask: only the not-yet-visited (GraphBLAS's complemented mask)
            if (next[j] != 0 && level[j] < 0) {
                level[j] = depth;
                any = true;
            } else {
                next[j] = 0;
            }
        }
        if (!any) break;
        frontier.swap(next);
    }
    return level;
}

std::vector<double> shortest_paths(const Csr& A, std::uint32_t source) {
    const double inf = MinPlus::zero();
    std::vector<double> d(A.rows, inf);
    if (source >= A.rows) return d;
    d[source] = 0.0;
    for (std::uint32_t round = 0; round < A.rows; ++round) {
        const std::vector<double> relaxed = vxm<MinPlus>(d, A);
        bool changed = false;
        for (std::uint32_t j = 0; j < A.rows; ++j)
            if (relaxed[j] < d[j]) {
                d[j] = relaxed[j];
                changed = true;
            }
        if (!changed) return d;
    }
    return {}; // still improving after n rounds: a negative cycle
}

std::vector<double> pagerank(const Csr& A, bool weighted, double damping, int max_iters, double tolerance) {
    const std::uint32_t n = A.rows;
    if (n == 0) return {};
    // P: each row's out-edges as shares that sum to one
    Csr P = A;
    std::vector<char> dangling(n, 0);
    for (std::uint32_t i = 0; i < n; ++i) {
        double total = 0;
        for (std::uint64_t k = P.offsets[i]; k < P.offsets[(std::size_t)i + 1]; ++k)
            total += weighted ? std::max(0.0, P.value[k]) : 1.0;
        if (total <= 0) {
            dangling[i] = 1;
            continue;
        }
        for (std::uint64_t k = P.offsets[i]; k < P.offsets[(std::size_t)i + 1]; ++k)
            P.value[k] = (weighted ? std::max(0.0, P.value[k]) : 1.0) / total;
    }
    std::vector<double> r(n, 1.0 / n);
    for (int it = 0; it < max_iters; ++it) {
        double lost = 0;
        for (std::uint32_t i = 0; i < n; ++i)
            if (dangling[i]) lost += r[i];
        std::vector<double> next = vxm<PlusTimes>(r, P);
        double diff = 0;
        for (std::uint32_t j = 0; j < n; ++j) {
            next[j] = (1 - damping) / n + damping * (next[j] + lost / n);
            diff += std::fabs(next[j] - r[j]);
        }
        r.swap(next);
        if (diff < tolerance) break;
    }
    return r;
}

std::vector<std::uint32_t> components(const Csr& A) {
    const Csr S = symmetrize(A);
    std::vector<double> label(S.rows);
    std::iota(label.begin(), label.end(), 0.0);
    for (;;) {
        const std::vector<double> got = mxv<MinSecond>(S, label);
        bool changed = false;
        for (std::uint32_t i = 0; i < S.rows; ++i)
            if (got[i] < label[i]) {
                label[i] = got[i];
                changed = true;
            }
        if (!changed) break;
    }
    return std::vector<std::uint32_t>(label.begin(), label.end());
}

Dense aggregate(const Csr& A, const Dense& X, bool mean) {
    Dense Y(A.rows, X.cols, 0.0);
    for (std::uint32_t i = 0; i < A.rows; ++i) {
        double wsum = 0;
        for (std::uint64_t k = A.offsets[i]; k < A.offsets[(std::size_t)i + 1]; ++k) {
            const double w = A.value[k];
            wsum += w;
            for (std::uint32_t c = 0; c < X.cols; ++c) Y.at(i, c) += w * X.at(A.index[k], c);
        }
        if (mean && wsum != 0)
            for (std::uint32_t c = 0; c < X.cols; ++c) Y.at(i, c) /= wsum;
    }
    return Y;
}

Csr symmetrize(const Csr& A, Duplicates dup) {
    std::vector<Triplet> t;
    t.reserve(A.nnz() * 2);
    for (std::uint32_t i = 0; i < A.rows; ++i)
        for (std::uint64_t k = A.offsets[i]; k < A.offsets[(std::size_t)i + 1]; ++k) {
            t.push_back({i, A.index[k], A.value[k]});
            t.push_back({A.index[k], i, A.value[k]});
        }
    const std::uint32_t n = std::max(A.rows, A.cols);
    return Csr::from_triplets(n, n, std::move(t), dup);
}

} // namespace rancho::linalg
