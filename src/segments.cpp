/* segments.cpp — see rancho/segments.hpp. */
#include "rancho/segments.hpp"

#include "sql.hpp"

#include "json.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <optional>
#include <random>
#include <sstream>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace rancho {

// ── SHA-256 (FIPS 180-4) ─────────────────────────────────────────────────────

namespace {

constexpr std::uint32_t kK[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

std::uint32_t rotr(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

void block(std::array<std::uint32_t, 8>& h, const unsigned char* p) {
    std::uint32_t w[64];
    for (int i = 0; i < 16; ++i)
        w[i] = (std::uint32_t)p[4 * i] << 24 | (std::uint32_t)p[4 * i + 1] << 16 | (std::uint32_t)p[4 * i + 2] << 8 |
               (std::uint32_t)p[4 * i + 3];
    for (int i = 16; i < 64; ++i) {
        const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], k = h[7];
    for (int i = 0; i < 64; ++i) {
        const std::uint32_t t1 = k + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + kK[i] + w[i];
        const std::uint32_t t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        k = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    h[0] += a, h[1] += b, h[2] += c, h[3] += d, h[4] += e, h[5] += f, h[6] += g, h[7] += k;
}

} // namespace

std::string sha256_hex(std::string_view bytes) {
    std::array<std::uint32_t, 8> h = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                      0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    const auto* p = reinterpret_cast<const unsigned char*>(bytes.data());
    const std::size_t n = bytes.size();
    std::size_t i = 0;
    for (; i + 64 <= n; i += 64) block(h, p + i);
    unsigned char tail[128] = {};
    const std::size_t rest = n - i;
    if (rest) std::memcpy(tail, p + i, rest);
    tail[rest] = 0x80;
    const std::size_t len = rest + 1 + 8 <= 64 ? 64 : 128;
    const std::uint64_t bits = (std::uint64_t)n * 8;
    for (int b = 0; b < 8; ++b) tail[len - 1 - b] = (unsigned char)(bits >> (8 * b));
    block(h, tail);
    if (len == 128) block(h, tail + 64);
    static const char* hexd = "0123456789abcdef";
    std::string out;
    for (const std::uint32_t x : h)
        for (int s = 28; s >= 0; s -= 4) out += hexd[(x >> s) & 0xF];
    return out;
}

// ── the directory ────────────────────────────────────────────────────────────

namespace {

std::string read_file(const fs::path& p, bool& ok) {
    std::ifstream in(p, std::ios::binary);
    ok = (bool)in;
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

/* Write `bytes` to `p` by way of a temporary file in the same folder and a
 * rename, so a reader never sees a half-written file under its real name. */
bool write_atomically(const fs::path& p, const std::string& bytes) {
    std::random_device rd;
    const fs::path tmp = p.parent_path() / ("." + p.filename().string() + ".tmp-" + std::to_string(rd()));
    {
        std::ofstream o(tmp, std::ios::binary | std::ios::trunc);
        o.write(bytes.data(), (std::streamsize)bytes.size());
        if (!o) return false;
    }
    std::error_code ec;
    fs::rename(tmp, p, ec);
    if (ec) fs::remove(tmp, ec);
    return !ec;
}

/* Create `p` only if it does not exist: the commit's serialization point. */
bool create_exclusive(const fs::path& p, const std::string& bytes, bool& existed) {
    existed = false;
#if defined(_WIN32)
    const int fd = _wopen(p.wstring().c_str(), _O_CREAT | _O_EXCL | _O_WRONLY | _O_BINARY, _S_IREAD | _S_IWRITE);
#else
    const int fd = ::open(p.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0644);
#endif
    if (fd < 0) {
        existed = errno == EEXIST;
        return false;
    }
    std::size_t done = 0;
    while (done < bytes.size()) {
#if defined(_WIN32)
        const int w = _write(fd, bytes.data() + done, (unsigned)(bytes.size() - done));
#else
        const ssize_t w = ::write(fd, bytes.data() + done, bytes.size() - done);
#endif
        if (w <= 0) break;
        done += (std::size_t)w;
    }
#if defined(_WIN32)
    _close(fd);
#else
    ::close(fd);
#endif
    return done == bytes.size();
}

std::string q(const std::string& s) { return nlohmann::json(s).dump(); }

/* A REAL column as JSON: integral positions (the common case) without a fraction. */
std::string num(double x) {
    if (x == (double)(long long)x && x > -9e15 && x < 9e15) return std::to_string((long long)x);
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.17g", x);
    return buf;
}

fs::path manifest_path(const fs::path& dir, long long n) { return dir / "manifests" / (std::to_string(n) + ".json"); }

} // namespace

long long current_snapshot(const fs::path& dir) {
    bool ok = false;
    const std::string s = read_file(dir / "ROOT", ok);
    if (!ok) return 0;
    try {
        return std::stoll(s);
    } catch (...) {
        return 0;
    }
}

std::vector<long long> snapshots(const fs::path& dir) {
    std::vector<long long> out;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir / "manifests", ec)) {
        const std::string stem = e.path().stem().string();
        if (e.path().extension() != ".json" || stem.empty() || stem.find_first_not_of("0123456789") != std::string::npos)
            continue;
        out.push_back(std::stoll(stem));
    }
    std::sort(out.begin(), out.end());
    return out;
}

SnapshotStats snapshot(Store& store, const fs::path& dir, const SegmentOptions& opt) {
    SnapshotStats st;
    sqlite3* db = store.handle();
    if (!db) {
        st.error = "the store is not open";
        return st;
    }
    std::error_code ec;
    fs::create_directories(dir / "segments", ec);
    fs::create_directories(dir / "manifests", ec);
    if (ec) {
        st.error = "cannot create " + dir.string() + ": " + ec.message();
        return st;
    }
    const long long parent = current_snapshot(dir);
    st.snapshot = parent + 1;

    /* One segment out: named by its bytes, written unless already there. */
    auto emit = [&](const std::string& bytes) -> std::string {
        const std::string name = sha256_hex(bytes);
        const fs::path p = dir / "segments" / (name + ".json");
        ++st.segments;
        std::error_code e;
        if (fs::exists(p, e)) {
            bool ok = false;
            if (read_file(p, ok) == bytes && ok) {
                ++st.reused;
                return name;
            }
            st.error = "segment " + name + " is on disk with different bytes: the shared folder is corrupt";
            return {};
        }
        if (!write_atomically(p, bytes)) {
            st.error = "cannot write segment " + p.string();
            return {};
        }
        ++st.written;
        st.bytes_written += bytes.size();
        return name;
    };

    std::ostringstream man;
    man << "{\"format\":\"voidrancho.manifest/1\",\"snapshot\":" << st.snapshot
        << ",\"parent\":" << (parent ? std::to_string(parent) : std::string("null")) << ",\"top\":[";
    {
        Stmt t(db, "SELECT key, json FROM top ORDER BY pos");
        bool first = true;
        while (t.row()) {
            man << (first ? "" : ",") << "{\"key\":" << q(std::string(t.col(0)))
                << ",\"json\":" << (t.is_null(1) ? std::string("null") : std::string(t.col(1))) << "}";
            first = false;
        }
    }
    man << "],\"mantles\":[";

    Stmt ms(db, "SELECT id, name, COALESCE(mid,''), json, layout, has_runes, has_edges FROM mantles WHERE deleted=0"
                " ORDER BY pos");
    Stmt runes(db, "SELECT r.id, r.pos, r.name, r.json, COALESCE(c.path,''), COALESCE(x.row,-1) FROM runes r"
                   " LEFT JOIN clusters c ON c.rune=r.id LEFT JOIN rows x ON x.mkey=?2 AND x.rid=r.rid"
                   " WHERE r.mantle=?1 AND r.deleted=0 ORDER BY COALESCE(c.path,''), r.pos");
    Stmt tags(db, "SELECT tag, source FROM tags WHERE rune=? ORDER BY rowid");
    Stmt out_edges(db, "SELECT pos, json FROM edges INDEXED BY edges_by_src WHERE mantle=? AND src=? AND deleted=0"
                       " ORDER BY pos");
    Stmt loose(db, "SELECT e.pos, e.json FROM edges e WHERE e.mantle=?1 AND e.deleted=0 AND NOT EXISTS("
                   "SELECT 1 FROM runes r WHERE r.mantle=?1 AND r.name=e.src AND r.deleted=0) ORDER BY e.pos");
    bool first_m = true;
    while (ms.row() && st.error.empty()) {
        const long long mid = ms.col_int(0);
        const std::string mname(ms.col(1));
        man << (first_m ? "" : ",") << "{\"name\":" << q(mname) << ",\"mid\":" << q(std::string(ms.col(2)))
            << ",\"has_runes\":" << (ms.col_int(5) ? "true" : "false")
            << ",\"has_edges\":" << (ms.col_int(6) ? "true" : "false") << ",\"skeleton\":" << ms.col(3)
            << ",\"layout\":" << (ms.is_null(4) ? std::string("null") : std::string(ms.col(4)));
        const std::string settings = store.cluster_settings(mname);
        man << ",\"cluster_settings\":" << (settings.empty() ? std::string("null") : settings) << ",\"segments\":[";
        first_m = false;

        // the segment being filled, and its statistics
        std::string body_runes, body_edges, group;
        long long n_runes = 0, n_edges = 0;
        std::int64_t lo = -1, hi = -1;
        std::uint64_t bytes = 0;
        std::map<std::string, long long> tag_counts;
        bool first_seg = true;
        auto flush = [&] {
            if (!n_runes && !n_edges) return;
            const std::string seg = "{\"format\":\"voidrancho.segment/1\",\"runes\":[" + body_runes +
                                    "],\"edges\":[" + body_edges + "]}";
            const std::string name = emit(seg);
            if (name.empty()) return;
            nlohmann::json tc = tag_counts;
            man << (first_seg ? "" : ",") << "{\"name\":" << q(name) << ",\"cluster\":" << q(group)
                << ",\"runes\":" << n_runes << ",\"edges\":" << n_edges << ",\"bytes\":" << seg.size()
                << ",\"rho\":[" << lo << "," << hi << "],\"tags\":" << tc.dump() << "}";
            first_seg = false;
            st.runes += n_runes;
            st.edges += n_edges;
            body_runes.clear();
            body_edges.clear();
            n_runes = n_edges = 0;
            lo = hi = -1;
            bytes = 0;
            tag_counts.clear();
        };
        auto add_edge = [&](double pos, std::string_view json) {
            body_edges += (n_edges ? "," : "");
            body_edges += "{\"pos\":" + num(pos) + ",\"edge\":" + std::string(json) + "}";
            ++n_edges;
            bytes += json.size();
        };

        runes.integer(1, mid).text(2, store.mantle_key(mid));
        while (runes.row() && st.error.empty()) {
            const std::string path(runes.col(4));
            if ((path != group || bytes >= opt.page_bytes) && (n_runes || n_edges)) flush();
            group = path;
            const long long rid = runes.col_int(0);
            const std::string rname(runes.col(2));
            std::map<std::string, std::string> sources;
            tags.integer(1, rid);
            while (tags.row()) {
                const std::string t(tags.col(0));
                ++tag_counts[t];
                if (!tags.is_null(1)) sources.emplace(t, std::string(tags.col(1)));
            }
            tags.reset();
            body_runes += (n_runes ? "," : "");
            body_runes += "{\"pos\":" + num(runes.col_real(1));
            if (!path.empty()) body_runes += ",\"cluster\":" + q(path);
            if (!sources.empty()) body_runes += ",\"sources\":" + nlohmann::json(sources).dump();
            body_runes += ",\"rune\":" + std::string(runes.col(3)) + "}";
            ++n_runes;
            bytes += runes.col(3).size();
            const std::int64_t rho = runes.col_int(5);
            if (rho >= 0) {
                lo = lo < 0 ? rho : std::min(lo, rho);
                hi = std::max(hi, rho);
            }
            out_edges.integer(1, mid).text(2, rname);
            while (out_edges.row()) add_edge(out_edges.col_real(0), out_edges.col(1));
            out_edges.reset();
        }
        runes.reset();
        flush();
        // edges whose source is not a rune: links may dangle (SPEC §3.7), and must survive
        group = "~loose";
        loose.integer(1, mid);
        while (loose.row()) {
            if (bytes >= opt.page_bytes) flush();
            add_edge(loose.col_real(0), loose.col(1));
        }
        loose.reset();
        flush();
        man << "]}";
    }
    man << "]}";
    if (!st.error.empty()) return st;

    bool existed = false;
    if (!create_exclusive(manifest_path(dir, st.snapshot), man.str(), existed)) {
        st.error = existed ? "snapshot " + std::to_string(st.snapshot) +
                                 " was committed by someone else meanwhile; nothing was replaced (its segments stay)"
                           : "cannot write the manifest in " + (dir / "manifests").string();
        return st;
    }
    if (!write_atomically(dir / "ROOT", std::to_string(st.snapshot) + "\n")) {
        st.error = "the manifest is written but ROOT could not be moved to it";
        return st;
    }
    st.ok = true;
    return st;
}

RestoreStats restore(const fs::path& dir, Store& into, long long n) {
    RestoreStats st;
    st.snapshot = n ? n : current_snapshot(dir);
    if (!st.snapshot) {
        st.error = "no snapshot in " + dir.string();
        return st;
    }
    JsonStream m(manifest_path(dir, st.snapshot).string());
    if (!m.ok()) {
        st.error = m.error();
        return st;
    }
    if (!into.begin_bulk()) {
        st.error = into.error();
        return st;
    }
    auto as_string = [](const std::string& raw) {
        const auto j = nlohmann::json::parse(raw, nullptr, false);
        return j.is_string() ? j.get<std::string>() : std::string();
    };
    std::string raw;
    int mantle_pos = 0;
    auto read_segment = [&](long long mantle, const std::string& name) -> bool {
        bool ok = false;
        const std::string bytes = read_file(dir / "segments" / (name + ".json"), ok);
        if (!ok) return m.fail("segment " + name + " is missing");
        if (sha256_hex(bytes) != name) return m.fail("segment " + name + " does not match its name: it is corrupt");
        JsonStream s = JsonStream::from_text(bytes);
        const bool good = s.object([&](const std::string& k) -> bool {
            if (k == "runes")
                return s.array([&] {
                    double pos = 0;
                    std::string rune, cluster;
                    std::map<std::string, std::string> sources;
                    const bool r = s.object([&](const std::string& rk) -> bool {
                        std::string v;
                        if (!s.capture(v)) return false;
                        if (rk == "pos") pos = std::stod(v);
                        else if (rk == "rune") rune = std::move(v);
                        else if (rk == "cluster") cluster = as_string(v);
                        else if (rk == "sources") {
                            const auto j = nlohmann::json::parse(v, nullptr, false);
                            if (j.is_object())
                                for (const auto& [t, src] : j.items())
                                    if (src.is_string()) sources.emplace(t, src.get<std::string>());
                        }
                        return true;
                    });
                    if (!r) return false;
                    into.bulk_rune(mantle, pos, rune, sources, cluster);
                    ++st.runes;
                    return true;
                });
            if (k == "edges")
                return s.array([&] {
                    double pos = 0;
                    std::string edge;
                    const bool r = s.object([&](const std::string& ek) -> bool {
                        std::string v;
                        if (!s.capture(v)) return false;
                        if (ek == "pos") pos = std::stod(v);
                        else if (ek == "edge") edge = std::move(v);
                        return true;
                    });
                    if (!r) return false;
                    into.bulk_edge(mantle, pos, edge);
                    ++st.edges;
                    return true;
                });
            return s.skip();
        });
        if (!good) return m.fail("segment " + name + ": " + s.error());
        ++st.segments;
        return true;
    };
    const bool ok = m.object([&](const std::string& key) -> bool {
        if (key == "top") {
            int pos = 0;
            return m.array([&] {
                std::string k;
                std::optional<std::string> json;
                const bool r = m.object([&](const std::string& tk) -> bool {
                    if (!m.capture(raw)) return false;
                    if (tk == "key") k = as_string(raw);
                    else if (tk == "json" && raw != "null") json = raw;
                    return true;
                });
                if (r) into.bulk_top(k, pos++, json);
                return r;
            });
        }
        if (key == "mantles")
            return m.array([&] {
                MantleRow row;
                long long id = -1;
                std::string settings;
                const bool good = m.object([&](const std::string& mk) -> bool {
                    if (mk == "segments") {
                        if (id < 0) id = into.bulk_mantle(row, mantle_pos++);
                        return m.array([&] {
                            std::string name;
                            const bool r = m.object([&](const std::string& sk) -> bool {
                                if (!m.capture(raw)) return false;
                                if (sk == "name") name = as_string(raw);
                                return true;
                            });
                            return r && read_segment(id, name);
                        });
                    }
                    if (!m.capture(raw)) return false;
                    if (mk == "name") row.name = as_string(raw);
                    else if (mk == "mid") row.mid = as_string(raw);
                    else if (mk == "skeleton") row.skeleton = raw;
                    else if (mk == "layout") row.layout = raw == "null" ? std::nullopt : std::optional<std::string>(raw);
                    else if (mk == "has_runes") row.has_runes = raw == "true";
                    else if (mk == "has_edges") row.has_edges = raw == "true";
                    else if (mk == "cluster_settings" && raw != "null") settings = raw;
                    return true;
                });
                if (good && !settings.empty()) into.bulk_cluster_settings(id, settings);
                return good;
            });
        return m.skip();
    });
    if (!ok) {
        st.error = m.error().empty() ? "the manifest could not be read" : m.error();
        into.abort_bulk();
        return st;
    }
    if (!into.end_bulk()) {
        st.error = into.error();
        return st;
    }
    st.ok = true;
    return st;
}

} // namespace rancho
