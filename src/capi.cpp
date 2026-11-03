/* capi.cpp — see rancho/rancho.h. */
#include "rancho/rancho.h"

#include "rancho/matrix.hpp"
#include "rancho/segments.hpp"
#include "rancho/store.hpp"

#include "json.hpp"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#ifndef VOIDRANCHO_VERSION
#define VOIDRANCHO_VERSION "0.1.0"
#endif

struct RanchoStore {
    std::unique_ptr<rancho::Store> store;
    std::string error;
};

namespace {

thread_local std::string g_open_error;

/* A UTF-8 path, whatever the platform's narrow encoding. */
std::filesystem::path upath(const char* s) { return std::filesystem::path(reinterpret_cast<const char8_t*>(s)); }

char* dup(const std::string& s) {
    char* p = static_cast<char*>(std::malloc(s.size() + 1));
    if (p) std::memcpy(p, s.c_str(), s.size() + 1);
    return p;
}

int done(RanchoStore* s, bool ok, const std::string& error) {
    s->error = ok ? std::string() : error;
    return ok ? 1 : 0;
}

} // namespace

extern "C" {

const char* rancho_version(void) { return VOIDRANCHO_VERSION; }

RanchoStore* rancho_open(const char* path) {
    if (!path) return nullptr;
    auto st = std::make_unique<rancho::Store>(upath(path));
    if (!st->ok()) {
        g_open_error = st->error();
        return nullptr;
    }
    g_open_error.clear();
    auto* h = new RanchoStore;
    h->store = std::move(st);
    return h;
}

const char* rancho_last_open_error(void) { return g_open_error.c_str(); }

void rancho_close(RanchoStore* s) { delete s; }

const char* rancho_error(RanchoStore* s) { return s ? s->error.c_str() : "no store"; }

int rancho_import_file(RanchoStore* s, const char* path) {
    if (!s || !path) return 0;
    rancho::JsonStream in(path);
    if (!in.ok()) return done(s, false, in.error());
    const auto st = s->store->import(in);
    return done(s, st.ok, st.error);
}

char* rancho_export_state(RanchoStore* s) { return s ? dup(s->store->state()) : nullptr; }

char* rancho_select(RanchoStore* s, const char* mantle, const char* expr) {
    if (!s || !mantle) return nullptr;
    return dup(nlohmann::json(s->store->select(mantle, expr ? expr : "")).dump());
}

char* rancho_mantles(RanchoStore* s) {
    if (!s) return nullptr;
    nlohmann::json out = nlohmann::json::array();
    for (const auto& m : s->store->mantles())
        out.push_back({{"name", m.name}, {"runes", m.runes}, {"edges", m.edges}, {"bytes", m.bytes}});
    return dup(out.dump());
}

int rancho_cluster(RanchoStore* s, const char* mantle, unsigned target) {
    if (!s || !mantle) return 0;
    const auto st = s->store->cluster(mantle, target ? target : 2000u);
    return done(s, st.ok, st.error);
}

long long rancho_snapshot(RanchoStore* s, const char* dir) {
    if (!s || !dir) return 0;
    const auto st = rancho::snapshot(*s->store, upath(dir));
    done(s, st.ok, st.error);
    return st.ok ? st.snapshot : 0;
}

int rancho_restore(RanchoStore* s, const char* dir, long long n) {
    if (!s || !dir) return 0;
    const auto st = rancho::restore(upath(dir), *s->store, n);
    return done(s, st.ok, st.error);
}

int rancho_export_npy(RanchoStore* s, const char* mantle, const char* dir) {
    if (!s || !mantle || !dir) return 0;
    const auto v = rancho::matrix_view(*s->store, mantle);
    std::string error = v.error;
    const bool ok = v.ok && rancho::export_npy(v, upath(dir), &error);
    return done(s, ok, error);
}

void rancho_free_str(char* p) { std::free(p); }

} // extern "C"
