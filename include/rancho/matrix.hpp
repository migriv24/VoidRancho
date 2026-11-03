/* rancho/matrix.hpp — a mantle as the arrays a model reads
 * (okf/concepts/matrix-view.md).
 *
 * The author's question — "what would a database look like to a JEPA model?" —
 * has a specific answer: entities with identity across time, typed relations
 * between ordered pairs, typed numbers, and changes labelled by actions. A
 * mantle has all four by construction; what it lacks is the ARRAY FORM a
 * model, a clustering algorithm or a GPU reads. This is that form, DERIVED
 * from the store's rows on every call and never stored as a second truth.
 *
 * For a mantle (or some of its clusters) with n runes:
 *
 *   rho        n        the persistent row ρ of each rune (Store::row_of):
 *                       stable across loads, so two views of one database at
 *                       two times line up row for row. -1 for a rune with no
 *                       spirit id. Local index i is ordered by ρ.
 *   adjacency  n × n    ONE matrix per relation, float64 weights, row = from.
 *                       Never summed across relations: "supports, 0.8" and
 *                       "speed, 900" are not commensurable (Core's quantity page).
 *                       An undirected edge is stored both ways. Edges to a
 *                       measure are not here — they are V.
 *   values V   n × m    entity × measure → value: every attribute-assertion edge
 *                       (SPEC §3.7.1; its `to` is a measure rune). Units with it.
 *   tags T     n × k    boolean incidence, with each tag's sources.
 *   features X n × f    glyph fields annotated in `kinds`, by measurement level:
 *                       ratio → the number; interval → centred on its mean
 *                       (no true zero); ordinal → the number when it is one;
 *                       nominal → a category index. NaN where absent. Fields
 *                       with no level are not features.
 *
 * Embeddings are produced by Void Bicho and not here; transitions wait on Void
 * Core recording what a command `touched` (its world models phase 1).
 *
 * What this is NOT: a way to change data. An analytic result that should
 * change something (a cluster tag, a suggested link) goes back as a
 * dispatcher command, like every other change. */
#pragma once

#include "rancho/linalg.hpp"
#include "rancho/store.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace rancho {

struct MatrixOptions {
    /* Cluster paths (Store::leaves): a path selects every leaf under it, and
     * the mantle's measures come with any. Empty: the whole mantle. */
    std::vector<std::string> clusters;
    bool adjacency = true, values = true, tags = true, features = true;
};

struct Feature {
    std::string glyph, field, level;
    std::vector<std::string> categories; // nominal: the value of each index
    double centre = 0;                   // interval: the mean that was subtracted
};

struct MatrixView {
    bool ok = false;
    std::string error, mantle;
    std::vector<std::int64_t> rho;
    std::vector<std::string> ids, names, glyphs;

    std::map<std::string, linalg::Csr> adjacency; // relation -> A_r

    std::vector<std::string> measures, units; // V's columns
    linalg::Csr values;

    std::vector<std::string> tag_names;
    std::vector<std::vector<std::string>> tag_sources; // per tag: who wrote it, distinct
    linalg::Csr tags;

    std::vector<Feature> features;
    linalg::Dense X;

    /* Edges the view could not place. Whole mantle: `dangling`, an end that
     * is no rune (legitimate in Core). Cluster view: `boundary`, an end not in
     * the view, whether it is a rune elsewhere or none. `duplicates`: a second
     * edge of one relation between one ordered pair (the last one is kept). */
    long long dangling = 0, boundary = 0, duplicates = 0;

    std::uint32_t size() const { return (std::uint32_t)names.size(); }
    /* Local index of a rune by name, -1 if not in the view. */
    long long index_of(const std::string& name) const;
};

MatrixView matrix_view(Store& store, const std::string& mantle, const MatrixOptions& options = {});

/* THE TRAINING EXPORT, first form (okf/roadmap.md rung 6): every array as a
 * NumPy .npy file in `dir`, CSR matrices as their three arrays, and a
 * manifest.json naming them, the rows, the relations, the units and the
 * feature levels — readable with numpy and scipy.sparse and nothing else:
 *
 *     m = json.load(open("manifest.json"))
 *     A = scipy.sparse.csr_matrix((np.load(a["data"]), np.load(a["indices"]),
 *                                  np.load(a["indptr"])), shape=a["shape"])
 *
 * Lance, which the world-model tooling reads, is the lean for the second
 * form (okf/design/open-questions.md §7). False with `error` set on failure. */
bool export_npy(const MatrixView& view, const std::filesystem::path& dir, std::string* error = nullptr);

} // namespace rancho
