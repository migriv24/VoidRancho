/* authority.cpp — the authority, its cache, and keepsakes. See
 * rancho/authority.hpp and okf/concepts/authority.md. */
#include "rancho/authority.hpp"

#include "json.hpp"

#include <cstdlib>
#include <fstream>
#include <random>
#include <sstream>

namespace rancho {

namespace fs = std::filesystem;
using nlohmann::json;

namespace {

fs::path env_path(const char* name) {
    const char* v = std::getenv(name);
    return v && *v ? fs::path(v) : fs::path();
}

#ifndef _WIN32
fs::path home() { return env_path("HOME"); } // the system roots below are LOCALAPPDATA's on Windows
#endif

/* `<base>/Void/<host>/<leaf>` on Windows, as the system's folders are laid out. */
fs::path system_root(std::string_view host, bool cache) {
    const std::string h(host);
#ifdef _WIN32
    if (fs::path la = env_path("LOCALAPPDATA"); !la.empty()) return la / "Void" / h / (cache ? "cache" : "keep");
#elif defined(__APPLE__)
    if (fs::path hm = home(); !hm.empty())
        return cache ? hm / "Library" / "Caches" / "Void" / h : hm / "Library" / "Application Support" / "Void" / h;
#else
    if (fs::path x = env_path(cache ? "XDG_CACHE_HOME" : "XDG_DATA_HOME"); !x.empty()) return x / "void" / h;
    if (fs::path hm = home(); !hm.empty())
        return cache ? hm / ".cache" / "void" / h : hm / ".local" / "share" / "void" / h;
#endif
    std::error_code ec;
    return fs::temp_directory_path(ec) / "void" / h / (cache ? "cache" : "keep");
}

/* An authority's name in the index: absolute and normal, case-folded where the
 * file system folds case, with forward slashes. */
std::string key_of(const fs::path& authority) {
    std::error_code ec;
    fs::path a = fs::absolute(authority, ec);
    if (ec) a = authority;
    const auto u8 = a.lexically_normal().generic_u8string();
    std::string s(u8.begin(), u8.end());
#ifdef _WIN32
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
#endif
    return s;
}

fs::path from_u8(const std::string& s) {
    return fs::path(std::u8string(s.begin(), s.end()));
}
std::string to_u8(const fs::path& p) {
    const auto u = p.generic_u8string();
    return std::string(u.begin(), u.end());
}

bool write_atomic(const fs::path& file, const std::string& text) {
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    const fs::path tmp = file.string() + ".tmp";
    {
        std::ofstream o(tmp, std::ios::binary | std::ios::trunc);
        if (!o) return false;
        o << text;
        if (!o.good()) return false;
    }
    fs::rename(tmp, file, ec);
    if (ec) { // a file in the way on some systems: replace it
        fs::remove(file, ec);
        fs::rename(tmp, file, ec);
    }
    return !ec;
}

json read_json(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return json::object();
    std::stringstream ss;
    ss << in.rdbuf();
    json j = json::parse(ss.str(), nullptr, false);
    return j.is_object() ? j : json::object();
}

fs::path index_file(const fs::path& root) { return root / "index.json"; }
fs::path note_file(const fs::path& cache) { return cache / "cache.json"; }

std::string fresh_name() {
    std::random_device rd;
    std::mt19937_64 g(((std::uint64_t)rd() << 32) ^ rd());
    char b[32];
    std::snprintf(b, sizeof b, "wc-%012llx", (unsigned long long)(g() & 0xffffffffffffULL));
    return b;
}

json stamp_json(const Stamp& s) { return {{"exists", s.exists}, {"size", s.size}, {"mtime", s.mtime}}; }
Stamp stamp_from(const json& j) {
    Stamp s;
    if (!j.is_object()) return s;
    s.exists = j.value("exists", false);
    s.size = j.value("size", (std::uintmax_t)0);
    s.mtime = j.value("mtime", (std::int64_t)0);
    return s;
}

} // namespace

fs::path cache_root(std::string_view host) {
    if (fs::path o = env_path("RANCHO_CACHE_DIR"); !o.empty()) return o / std::string(host);
    return system_root(host, true);
}

fs::path keep_root(std::string_view host) {
    if (fs::path o = env_path("RANCHO_KEEP_DIR"); !o.empty()) return o / std::string(host);
    return system_root(host, false);
}

Stamp stamp_of(const fs::path& file) {
    Stamp s;
    std::error_code ec;
    if (!fs::is_regular_file(file, ec)) return s;
    s.exists = true;
    s.size = fs::file_size(file, ec);
    const auto t = fs::last_write_time(file, ec);
    s.mtime = ec ? 0 : (std::int64_t)t.time_since_epoch().count();
    return s;
}

const char* state_name(CacheState s) {
    switch (s) {
    case CacheState::New: return "new";
    case CacheState::Current: return "current";
    case CacheState::Stale: return "stale";
    case CacheState::Ahead: return "ahead";
    case CacheState::Diverged: return "diverged";
    case CacheState::Orphan: return "orphan";
    }
    return "?";
}

fs::path working_copy_for(const fs::path& authority, const fs::path& root) {
    const std::string key = key_of(authority);
    json index = read_json(index_file(root));
    if (index.contains(key) && index[key].is_string()) {
        const fs::path c = root / from_u8(index[key].get<std::string>());
        std::error_code ec;
        if (fs::is_directory(c, ec) && read_note(c)) return c;
    }
    fs::path c;
    std::error_code ec;
    for (int tries = 0; tries < 8; ++tries) {
        c = root / fresh_name();
        if (!fs::exists(c, ec)) break;
    }
    fs::create_directories(c, ec);
    CacheNote n;
    n.authority = fs::absolute(authority, ec);
    write_note(c, n); // seen = nothing: New until the host fills it
    index[key] = to_u8(c.filename());
    write_atomic(index_file(root), index.dump(1));
    return c;
}

std::optional<CacheNote> read_note(const fs::path& cache) {
    std::error_code ec;
    if (!fs::is_regular_file(note_file(cache), ec)) return std::nullopt;
    const json j = read_json(note_file(cache));
    if (!j.contains("authority")) return std::nullopt;
    CacheNote n;
    n.authority = from_u8(j.value("authority", std::string()));
    n.database = j.value("database", std::string());
    n.seen = stamp_from(j.value("seen", json::object()));
    n.ahead = j.value("ahead", false);
    return n;
}

bool write_note(const fs::path& cache, const CacheNote& n) {
    json j = {{"rancho_cache", 1},
              {"authority", to_u8(n.authority)},
              {"database", n.database},
              {"seen", stamp_json(n.seen)},
              {"ahead", n.ahead}};
    return write_atomic(note_file(cache), j.dump(1));
}

CacheState cache_state(const fs::path& cache) {
    const auto n = read_note(cache);
    if (!n || !n->seen.exists) return CacheState::New;
    const Stamp now = stamp_of(n->authority);
    if (!now.exists) return CacheState::Orphan;
    const bool changed = !(now == n->seen);
    if (changed && n->ahead) return CacheState::Diverged;
    if (changed) return CacheState::Stale;
    return n->ahead ? CacheState::Ahead : CacheState::Current;
}

bool mark_ahead(const fs::path& cache) {
    auto n = read_note(cache);
    if (!n) return false;
    if (n->ahead) return true;
    n->ahead = true;
    return write_note(cache, *n);
}

bool mark_matched(const fs::path& cache) {
    auto n = read_note(cache);
    if (!n) return false;
    n->seen = stamp_of(n->authority);
    n->ahead = false;
    return write_note(cache, *n);
}

bool reassign(const fs::path& cache, const fs::path& new_authority, const fs::path& root) {
    auto n = read_note(cache);
    if (!n) return false;
    json index = read_json(index_file(root));
    index.erase(key_of(n->authority));
    std::error_code ec;
    n->authority = fs::absolute(new_authority, ec);
    // a cache already recorded for the new file is no longer its: this one is
    const std::string key = key_of(new_authority);
    if (index.contains(key) && index[key].is_string() && index[key].get<std::string>() != to_u8(cache.filename())) {
        const fs::path old = root / from_u8(index[key].get<std::string>());
        fs::remove_all(old, ec);
    }
    index[key] = to_u8(cache.filename());
    return write_note(cache, *n) && write_atomic(index_file(root), index.dump(1));
}

bool forget(const fs::path& cache, const fs::path& root) {
    json index = read_json(index_file(root));
    for (auto it = index.begin(); it != index.end();) {
        if (it->is_string() && it->get<std::string>() == to_u8(cache.filename())) it = index.erase(it);
        else ++it;
    }
    std::error_code ec;
    fs::remove_all(cache, ec);
    return write_atomic(index_file(root), index.dump(1)) && !ec;
}

std::vector<fs::path> caches(const fs::path& root) {
    std::vector<fs::path> out;
    std::error_code ec;
    if (!fs::is_directory(root, ec)) return out;
    for (const auto& e : fs::directory_iterator(root, ec))
        if (e.is_directory(ec) && read_note(e.path())) out.push_back(e.path());
    return out;
}

} // namespace rancho
