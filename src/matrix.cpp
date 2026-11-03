/* matrix.cpp — see rancho/matrix.hpp. Reads the store; writes nothing to it. */
#include "rancho/matrix.hpp"

#include "sql.hpp"

#include "json.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <numeric>
#include <set>
#include <unordered_map>

namespace fs = std::filesystem;

namespace rancho {

using linalg::Csr;
using linalg::Dense;
using linalg::Triplet;

long long MatrixView::index_of(const std::string& name) const {
    for (std::size_t i = 0; i < names.size(); ++i)
        if (names[i] == name) return (long long)i;
    return -1;
}

MatrixView matrix_view(Store& store, const std::string& mantle, const MatrixOptions& opt) {
    MatrixView v;
    v.mantle = mantle;
    sqlite3* db = store.handle();
    const long long mid = store.mantle_row(mantle);
    if (!db || mid < 0) {
        v.error = "no such mantle: " + mantle;
        return v;
    }
    const std::string key = store.mantle_key(mid);
    const std::set<std::string> measure_glyphs = store.measure_glyphs();

    // ── the rows ──
    struct R {
        long long id;
        std::int64_t rho;
        double pos;
        std::string name, rid, glyph, json;
    };
    std::vector<R> rs;
    {
        std::string sql = "SELECT r.id, COALESCE(x.row, -1), r.pos, r.name, COALESCE(r.rid,''), COALESCE(r.glyph,''),"
                          " r.json FROM runes r LEFT JOIN rows x ON x.mkey=?1 AND x.rid=r.rid"
                          " WHERE r.mantle=?2 AND r.deleted=0";
        if (!opt.clusters.empty()) {
            // the same index range Store::write_state reads a cluster path with
            sql += " AND r.id IN (SELECT c.rune FROM clusters c WHERE c.mantle=?2 AND (c.always=1";
            for (std::size_t k = 0; k < opt.clusters.size(); ++k) {
                const std::string p = "?" + std::to_string(k + 3);
                sql += " OR (c.path>=" + p + " AND c.path<=" + p + "||'.~' AND (c.path=" + p + " OR c.path>" + p +
                       "||'.'))";
            }
            sql += "))";
        }
        Stmt q(db, sql);
        q.text(1, key).integer(2, mid);
        for (std::size_t k = 0; k < opt.clusters.size(); ++k) q.text((int)k + 3, opt.clusters[k]);
        while (q.row())
            rs.push_back({q.col_int(0), (std::int64_t)q.col_int(1), q.col_real(2), std::string(q.col(3)),
                          std::string(q.col(4)), std::string(q.col(5)), std::string(q.col(6))});
    }
    // by ρ; a rune without one after every rune with one, in document order
    std::stable_sort(rs.begin(), rs.end(), [](const R& a, const R& b) {
        if ((a.rho < 0) != (b.rho < 0)) return a.rho >= 0;
        return a.rho >= 0 ? a.rho < b.rho : a.pos < b.pos;
    });
    const std::uint32_t n = (std::uint32_t)rs.size();
    std::unordered_map<std::string, std::uint32_t> by_name;
    std::unordered_map<long long, std::uint32_t> by_row;
    for (std::uint32_t i = 0; i < n; ++i) {
        v.rho.push_back(rs[i].rho);
        v.ids.push_back(rs[i].rid);
        v.names.push_back(rs[i].name);
        v.glyphs.push_back(rs[i].glyph);
        by_name.emplace(rs[i].name, i);
        by_row.emplace(rs[i].id, i);
    }

    // ── measures: V's columns, in row order ──
    std::unordered_map<std::uint32_t, std::uint32_t> measure_col; // row -> column
    for (std::uint32_t i = 0; i < n; ++i) {
        if (!measure_glyphs.count(rs[i].glyph)) continue;
        measure_col.emplace(i, (std::uint32_t)v.measures.size());
        v.measures.push_back(rs[i].name);
        const auto j = nlohmann::json::parse(rs[i].json, nullptr, false);
        std::string unit;
        if (j.is_object())
            if (const auto q = j.find("quantity"); q != j.end() && q->is_object()) unit = q->value("unit", "");
        v.units.push_back(unit);
    }

    // ── edges: A_r and V ──
    if (opt.adjacency || opt.values) {
        std::map<std::string, std::vector<Triplet>> adj;
        std::vector<Triplet> vals;
        std::set<std::pair<std::string, std::pair<std::uint32_t, std::uint32_t>>> seen;
        auto take = [&](const std::string& src, const std::string& dst, const std::string& rel, double w, bool directed) {
            const auto a = by_name.find(src), b = by_name.find(dst);
            if (a == by_name.end() || b == by_name.end()) {
                // whole mantle: an end that is no rune dangles (SPEC §3.7). A cluster view does not
                // look further, so there any unplaced end — out of the cluster, or nowhere — is boundary
                ++(opt.clusters.empty() ? v.dangling : v.boundary);
                return;
            }
            if (!seen.insert({rel, {a->second, b->second}}).second) ++v.duplicates;
            if (const auto m = measure_col.find(b->second); m != measure_col.end()) {
                // an attribute assertion: the weight IS the value (SPEC §3.7.1); `to` names the measure
                vals.push_back({a->second, m->second, w});
                return;
            }
            adj[rel].push_back({a->second, b->second, w});
            if (!directed && a->second != b->second) adj[rel].push_back({b->second, a->second, w});
        };
        if (opt.clusters.empty()) {
            Stmt q(db, "SELECT src, dst, COALESCE(relation,''), weight, directed FROM edges WHERE mantle=? AND deleted=0"
                       " ORDER BY pos");
            q.integer(1, mid);
            while (q.row())
                take(std::string(q.col(0)), std::string(q.col(1)), std::string(q.col(2)), q.col_real(3),
                     q.col_int(4) != 0);
        } else {
            /* A cluster view costs what it holds: its edges are found from its
             * runes through the source index, never by reading the mantle's. */
            Stmt q(db, "SELECT dst, COALESCE(relation,''), weight, directed FROM edges INDEXED BY edges_by_src"
                       " WHERE mantle=? AND src=? AND deleted=0 ORDER BY pos");
            for (std::uint32_t i = 0; i < n; ++i) {
                q.integer(1, mid).text(2, v.names[i]);
                while (q.row())
                    take(v.names[i], std::string(q.col(0)), std::string(q.col(1)), q.col_real(2), q.col_int(3) != 0);
                q.reset();
            }
        }
        if (opt.adjacency)
            for (auto& [rel, t] : adj) v.adjacency[rel] = Csr::from_triplets(n, n, std::move(t));
        if (opt.values) v.values = Csr::from_triplets(n, (std::uint32_t)v.measures.size(), std::move(vals));
    }
    if (!opt.values) v.measures.clear(), v.units.clear();

    // ── tags ──
    if (opt.tags) {
        std::map<std::string, std::set<std::string>> sources;
        std::vector<std::pair<std::uint32_t, std::string>> cells;
        Stmt q(db, "SELECT tag, COALESCE(source,'') FROM tags WHERE rune=?");
        for (std::uint32_t i = 0; i < n; ++i) {
            q.integer(1, rs[i].id);
            while (q.row()) {
                const std::string t(q.col(0));
                sources[t].insert(std::string(q.col(1)));
                cells.emplace_back(i, t);
            }
            q.reset();
        }
        std::unordered_map<std::string, std::uint32_t> col;
        for (const auto& [t, s] : sources) { // tags in sorted order: the same view gives the same columns
            col.emplace(t, (std::uint32_t)v.tag_names.size());
            v.tag_names.push_back(t);
            v.tag_sources.emplace_back(s.begin(), s.end());
        }
        std::vector<Triplet> t;
        for (const auto& [i, tag] : cells) t.push_back({i, col.at(tag), 1.0});
        v.tags = Csr::from_triplets(n, (std::uint32_t)v.tag_names.size(), std::move(t));
    }

    // ── features ──
    if (opt.features) {
        const auto levels = store.field_levels();
        std::set<std::string> present(v.glyphs.begin(), v.glyphs.end());
        for (const auto& [glyph, fields] : levels) {
            if (!present.count(glyph)) continue;
            for (const auto& [field, level] : fields)
                if (!level.empty()) v.features.push_back({glyph, field, level, {}, 0});
        }
        v.X = Dense(n, (std::uint32_t)v.features.size(), std::numeric_limits<double>::quiet_NaN());
        std::map<std::pair<std::string, std::string>, std::uint32_t> fcol;
        for (std::uint32_t c = 0; c < v.features.size(); ++c) fcol[{v.features[c].glyph, v.features[c].field}] = c;
        std::vector<std::map<std::string, std::uint32_t>> cats(v.features.size());
        std::vector<std::vector<std::pair<std::uint32_t, std::string>>> nominal(v.features.size());
        Stmt q(db, "SELECT field, num, text FROM fields WHERE rune=?");
        for (std::uint32_t i = 0; i < n; ++i) {
            q.integer(1, rs[i].id);
            while (q.row()) {
                const auto f = fcol.find({rs[i].glyph, std::string(q.col(0))});
                if (f == fcol.end()) continue;
                const Feature& feat = v.features[f->second];
                if (feat.level == "nominal") {
                    if (!q.is_null(2)) nominal[f->second].emplace_back(i, std::string(q.col(2)));
                    else if (!q.is_null(1)) nominal[f->second].emplace_back(i, std::to_string(q.col_real(1)));
                } else if (!q.is_null(1)) {
                    v.X.at(i, f->second) = q.col_real(1);
                }
            }
            q.reset();
        }
        for (std::uint32_t c = 0; c < v.features.size(); ++c) {
            Feature& feat = v.features[c];
            if (feat.level == "nominal") {
                std::set<std::string> vals;
                for (const auto& [_, s] : nominal[c]) vals.insert(s);
                feat.categories.assign(vals.begin(), vals.end());
                for (std::uint32_t k = 0; k < feat.categories.size(); ++k) cats[c][feat.categories[k]] = k;
                for (const auto& [i, s] : nominal[c]) v.X.at(i, c) = (double)cats[c][s];
            } else if (feat.level == "interval") { // no true zero: only differences mean anything
                double sum = 0;
                long long count = 0;
                for (std::uint32_t i = 0; i < n; ++i)
                    if (!std::isnan(v.X.at(i, c))) sum += v.X.at(i, c), ++count;
                feat.centre = count ? sum / (double)count : 0;
                for (std::uint32_t i = 0; i < n; ++i)
                    if (!std::isnan(v.X.at(i, c))) v.X.at(i, c) -= feat.centre;
            }
        }
    }
    v.ok = true;
    return v;
}

// ── the training export ──────────────────────────────────────────────────────

namespace {

/* NumPy's .npy format, version 1.0: magic, a little-endian header length, a
 * Python dict literal padded so the data starts on a 64-byte boundary. */
bool write_npy(const fs::path& p, const char* descr, const std::string& shape, const void* data, std::size_t bytes) {
    std::string header = "{'descr': '" + std::string(descr) + "', 'fortran_order': False, 'shape': " + shape + ", }";
    const std::size_t total = 10 + header.size() + 1;
    header.append((64 - total % 64) % 64, ' ');
    header += '\n';
    std::ofstream o(p, std::ios::binary | std::ios::trunc);
    const unsigned short len = (unsigned short)header.size();
    o.write("\x93NUMPY\x01\x00", 8);
    const char le[2] = {(char)(len & 0xFF), (char)(len >> 8)};
    o.write(le, 2);
    o << header;
    if (bytes) o.write(static_cast<const char*>(data), (std::streamsize)bytes);
    return (bool)o;
}

std::string shape1(std::size_t n) { return "(" + std::to_string(n) + ",)"; }

template <class T> bool npy(const fs::path& p, const char* descr, const std::vector<T>& v) {
    return write_npy(p, descr, shape1(v.size()), v.data(), v.size() * sizeof(T));
}

bool little_endian() {
    const unsigned x = 1;
    return *reinterpret_cast<const unsigned char*>(&x) == 1;
}

} // namespace

bool export_npy(const MatrixView& v, const fs::path& dir, std::string* error) {
    auto fail = [&](const std::string& why) {
        if (error) *error = why;
        return false;
    };
    if (!v.ok) return fail("the view is not built: " + v.error);
    if (!little_endian()) return fail("the export writes little-endian arrays; this machine is big-endian");
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) return fail("cannot create " + dir.string() + ": " + ec.message());

    nlohmann::json m;
    m["format"] = "voidrancho.matrix-view/1";
    m["mantle"] = v.mantle;
    m["n"] = v.size();
    m["names"] = v.names;
    m["ids"] = v.ids;
    m["glyphs"] = v.glyphs;
    m["unplaced_edges"] = {{"dangling", v.dangling}, {"boundary", v.boundary}, {"duplicates", v.duplicates}};
    if (!npy(dir / "rho.npy", "<i8", v.rho)) return fail("cannot write rho.npy");
    m["rho"] = "rho.npy";

    auto csr = [&](const Csr& a, const std::string& stem) -> nlohmann::json {
        std::vector<std::int64_t> indptr(a.offsets.begin(), a.offsets.end()), indices(a.index.begin(), a.index.end());
        const bool ok = npy(dir / (stem + "_indptr.npy"), "<i8", indptr) &&
                        npy(dir / (stem + "_indices.npy"), "<i8", indices) &&
                        npy(dir / (stem + "_data.npy"), "<f8", a.value);
        if (!ok) return nullptr;
        return {{"shape", {a.rows, a.cols}},
                {"indptr", stem + "_indptr.npy"},
                {"indices", stem + "_indices.npy"},
                {"data", stem + "_data.npy"}};
    };

    nlohmann::json rels = nlohmann::json::array();
    std::size_t k = 0;
    for (const auto& [rel, a] : v.adjacency) { // file names by index: a relation is any string
        const auto j = csr(a, "A" + std::to_string(k++));
        if (j.is_null()) return fail("cannot write the adjacency of relation \"" + rel + "\"");
        nlohmann::json e = j;
        e["relation"] = rel;
        rels.push_back(e);
    }
    m["adjacency"] = rels;
    if (!v.measures.empty()) {
        auto j = csr(v.values, "V");
        if (j.is_null()) return fail("cannot write V");
        j["measures"] = v.measures;
        j["units"] = v.units;
        m["values"] = j;
    }
    if (!v.tag_names.empty()) {
        auto j = csr(v.tags, "T");
        if (j.is_null()) return fail("cannot write T");
        j["tags"] = v.tag_names;
        j["sources"] = v.tag_sources;
        m["tags"] = j;
    }
    if (!v.features.empty()) {
        if (!write_npy(dir / "X.npy", "<f8", "(" + std::to_string(v.X.rows) + ", " + std::to_string(v.X.cols) + ")",
                       v.X.v.data(), v.X.v.size() * sizeof(double)))
            return fail("cannot write X.npy");
        nlohmann::json fs_ = nlohmann::json::array();
        for (const auto& f : v.features) {
            nlohmann::json e = {{"glyph", f.glyph}, {"field", f.field}, {"level", f.level}};
            if (f.level == "nominal") e["categories"] = f.categories;
            if (f.level == "interval") e["centre"] = f.centre;
            fs_.push_back(e);
        }
        m["features"] = {{"file", "X.npy"}, {"columns", fs_}};
    }
    std::ofstream o(dir / "manifest.json", std::ios::binary | std::ios::trunc);
    o << m.dump(1);
    if (!o) return fail("cannot write manifest.json");
    return true;
}

} // namespace rancho
