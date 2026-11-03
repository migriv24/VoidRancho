/* query_test.cpp — QUERY PARITY (okf/concepts/store.md, "Laws").
 *
 * A tag expression evaluated over the store returns exactly the runes Void
 * Core's `ls --tag` returns on a Core holding the whole mantle, in the same
 * order — the test Core's MeshDB holiday passed 7/7, here over thousands of
 * random expressions built to reach the grammar's corners: implicit AND,
 * NOT and !, parentheses left open, operators in any case, `&&` and `||`
 * mid-word (ordinary tag characters there), a stray `)`, name-as-tag,
 * glyph:<g>, a tag of another mantle, a soft-deleted rune. Seeded: a failure
 * prints the expression. */
#include "check.hpp"

#include "rancho/store.hpp"

#include <functional>
#include <random>

using namespace rancho;

namespace {

const char* kThing = R"({"glyph":"thing","label":"Thing","fields":["note"]})";
const char* kPlace = R"({"glyph":"place","label":"Place","fields":["note"]})";

const std::vector<std::string> kTags = {"a", "b", "c", "group:x", "group:y", "status:draft", "a&b", "x||y", "AND!"};

struct World {
    VcCore core;
    Store store;
    explicit World(const std::filesystem::path& db) : store(db) {
        core.register_glyph(kThing);
        core.register_glyph(kPlace);
        std::mt19937 rng(42);
        std::vector<std::string> cmds = {"mantle new other", "rune new thing o1", "tag o1 +a +b +c", "mantle new m"};
        for (int i = 0; i < 160; ++i) {
            const std::string n = "r" + std::to_string(i);
            cmds.push_back(std::string("rune new ") + (i % 3 ? "thing " : "place ") + n);
            std::string t = "tag " + n;
            for (const auto& tag : kTags)
                if (rng() % 4 == 0) t += " +" + arg(tag);
            if (t.size() > 4 + n.size()) cmds.push_back(t);
        }
        CHECK(core.dispatch(batch(cmds)).ok);
        JsonStream in = JsonStream::from_text(core.export_state());
        CHECK(store.import(in).ok);
    }
};

std::vector<std::string> by_core(CoreLink& core, const std::string& expr) {
    return test::ls(core, "--tag " + arg(expr));
}

std::string random_expr(std::mt19937& rng) {
    static const std::vector<std::string> atoms = {"a", "b", "c", "group:x", "group:y", "status:draft", "a&b",
                                                   "x||y", "r7", "r12", "glyph:place", "glyph:thing", "glyph:none",
                                                   "o1", "nothing", "AND!", ")", "!a", "a!"};
    static const std::vector<std::string> ops = {" AND ", " and ", " && ", " OR ", " or ", " || ", " "};
    std::function<std::string(int)> gen = [&](int depth) -> std::string {
        switch (rng() % (depth > 2 ? 3 : 7)) {
        case 0: case 1: case 2: return atoms[rng() % atoms.size()];
        case 3: return (rng() % 2 ? "NOT " : "!") + gen(depth + 1);
        case 4: return "(" + gen(depth + 1) + (rng() % 5 ? ")" : ""); // sometimes never closed
        default: return gen(depth + 1) + ops[rng() % ops.size()] + gen(depth + 1);
        }
    };
    std::string e = gen(0);
    if (rng() % 10 == 0) e += " )"; // a stray closer: Core stops reading there
    if (rng() % 15 == 0) e = "  " + e + " OR";
    return e;
}

void every_hand_picked_expression_agrees() {
    World w(test::scratch("query-test", "hand.store"));
    w.core.dispatch("use m");
    for (const std::string e : {"", "a", "a b", "a AND b", "a OR b", "NOT a", "!a", "a && !b", "(a OR b) AND c",
                                "group:x || group:y", "glyph:place", "glyph:place AND NOT a", "r7", "r7 OR r8",
                                "a&b", "x||y", "o1", "(a", "a )", ")", "nothing", "NOT nothing", "a OR", "AND a"}) {
        const auto want = by_core(w.core, e), got = w.store.select("m", e);
        if (want != got) std::cerr << "    differs: \"" << e << "\" (core " << want.size() << ", store " << got.size() << ")\n";
        CHECK(want == got);
    }
}

void random_expressions_agree_with_ls_tag() {
    World w(test::scratch("query-test", "fuzz.store"));
    w.core.dispatch("use m");
    std::mt19937 rng(20261030);
    int checked = 0, nonempty = 0;
    for (int i = 0; i < 3000; ++i) {
        const std::string e = random_expr(rng);
        if (e.size() > 900) continue; // ls --tag reads up to 1023 bytes
        const auto want = by_core(w.core, e), got = w.store.select("m", e);
        ++checked;
        nonempty += !want.empty();
        if (want != got) {
            std::cerr << "    differs: \"" << e << "\" (core " << want.size() << ", store " << got.size() << ")\n";
            CHECK(want == got);
            if (failures > 5) return;
        }
    }
    std::cout << "    " << checked << " expressions, " << nonempty << " with matches\n";
    CHECK(nonempty > checked / 4); // the walk is not trivially comparing empty answers
}

void limit_offset_and_count_page_the_same_answer() {
    World w(test::scratch("query-test", "page.store"));
    w.core.dispatch("use m");
    const auto all = w.store.select("m", "a OR b");
    CHECK(all.size() > 20);
    CHECK((w.store.select("m", "a OR b", 5, 10) == std::vector<std::string>(all.begin() + 10, all.begin() + 15)));
    CHECK(w.store.count("m", "a OR b") == (long long)all.size());
    CHECK(w.store.select("nope", "a").empty());
}

void a_removed_rune_is_not_answered() {
    World w(test::scratch("query-test", "rm.store"));
    w.core.dispatch("use m");
    const auto before = w.store.select("m", "a");
    CHECK(!before.empty());
    const std::string gone = before.front();
    CHECK(w.store.begin());
    w.store.delete_rune(w.store.mantle_row("m"), gone);
    CHECK(w.store.commit());
    w.core.dispatch("rune rm " + gone);
    CHECK(w.store.select("m", "a") == by_core(w.core, "a"));
    CHECK(w.store.select("m", gone).empty());
}

} // namespace

int main() {
    const test::Case cases[] = {
        {"every hand-picked expression agrees with ls --tag", every_hand_picked_expression_agrees},
        {"random expressions agree with ls --tag", random_expressions_agree_with_ls_tag},
        {"limit, offset and count page the same answer", limit_offset_and_count_page_the_same_answer},
        {"a removed rune is not answered", a_removed_rune_is_not_answered},
    };
    return test::run(cases);
}
