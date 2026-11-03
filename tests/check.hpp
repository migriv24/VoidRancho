/* check.hpp — what every Rancho test shares: CHECK, a scratch folder, a
 * runner, and the canonical form two state documents are compared in. One
 * function per promise, each named for what it promises (Void Hormiga's
 * convention, kept). */
#pragma once

#include "rancho/core.hpp"

#include "json.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>
#include <iostream>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            ++failures;                                                              \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                            \
    } while (0)

namespace test {

namespace fs = std::filesystem;

/* A fresh path under the temp folder: the file and its -wal/-shm are gone. */
inline fs::path scratch(const std::string& suite, const std::string& name) {
    const fs::path d = fs::temp_directory_path() / ("rancho-" + suite);
    fs::create_directories(d);
    const fs::path p = d / name;
    std::error_code ec;
    for (const char* s : {"", "-wal", "-shm", "-journal"}) fs::remove(p.string() + s, ec);
    fs::remove_all(p, ec);
    return p;
}

/* A state document without what the store does not keep (`_baseline`). */
inline nlohmann::json plain(const std::string& text) {
    nlohmann::json j = nlohmann::json::parse(text);
    j.erase("_baseline");
    return j;
}

/* Ids are minted at random by each core, so they differ between two cores fed
 * the same commands; and the order of runes and of edges carries no meaning
 * (SPEC §4). The form two such documents are compared in. */
inline nlohmann::json canonical(const std::string& text) {
    nlohmann::json j = plain(text);
    for (auto& m : j["mantles"]) {
        m.erase("id");
        if (m.contains("runes")) {
            for (auto& r : m["runes"]) r["spirit"].erase("id");
            std::sort(m["runes"].begin(), m["runes"].end(), [](const nlohmann::json& a, const nlohmann::json& b) {
                return a["spirit"]["name"].get<std::string>() < b["spirit"]["name"].get<std::string>();
            });
        }
        if (m.contains("layout") && m["layout"].contains("edges"))
            std::sort(m["layout"]["edges"].begin(), m["layout"]["edges"].end(),
                      [](const nlohmann::json& a, const nlohmann::json& b) { return a.dump() < b.dump(); });
    }
    return j;
}

/* The names `ls` lists in the active mantle of `core`. */
inline std::vector<std::string> ls(rancho::CoreLink& core, const std::string& args = {}) {
    std::vector<std::string> out;
    const auto r = core.dispatch("ls" + (args.empty() ? std::string() : " " + args));
    const auto j = nlohmann::json::parse(r.data, nullptr, false);
    if (j.is_array())
        for (const auto& n : j)
            if (n.is_string()) out.push_back(n.get<std::string>());
    return out;
}

/* Synthetic, large, and written as text: a core is too slow to build 10^5 runes. */
inline void write_big_state(const fs::path& p, int runes) {
    std::ofstream o(p, std::ios::binary);
    o << R"({"version":1,"domains":{},"mantles":[{"id":"m","name":"big","domain":null,"runes":[)";
    std::mt19937 rng(7);
    for (int i = 0; i < runes; ++i) {
        if (i) o << ",";
        o << R"({"content":{"display_name":"Organization number )" << i << R"(","geo":"45.)" << rng() % 100000
          << R"(,-122.)" << rng() % 100000 << R"("},"facets":{},"glyph":"organization","placement":null,)"
          << R"("relations":[],"spirit":{"id":"rune_)" << i << R"(","name":"org-)" << i
          << R"("},"tags":["type:organization","c1:)" << i % 97 << R"(","city:x)" << i % 13 << R"("]})";
    }
    o << R"(],"tags":{},"layout":{"edges":[)";
    for (int i = 0; i + 1 < runes; ++i) {
        if (i) o << ",";
        o << R"({"from":"org-)" << i << R"(","to":"org-)" << (i * 7 + 1) % runes
          << R"(","relation":"partners","weight":1,"directed":true})";
    }
    o << R"(]},"rules":[]}],"config":{},"active":{"mantle":"big","domain":null},"bindings":[]})";
}

struct Case {
    const char* name;
    void (*fn)();
};

template <std::size_t N> int run(const Case (&cases)[N]) {
    for (const auto& t : cases) {
        const int before = failures;
        t.fn();
        std::cout << (failures == before ? "ok   " : "FAIL ") << t.name << "\n";
    }
    if (failures) std::cerr << failures << " failure(s)\n";
    return failures ? 1 : 0;
}

} // namespace test
