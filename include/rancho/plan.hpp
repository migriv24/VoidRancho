/* rancho/plan.hpp — what a working set holds, on this machine
 * (okf/concepts/working-set.md). Ported unchanged from Void Hormiga.
 *
 * THE BUDGET is a share of the machine's physical memory: an eighth, between
 * 128 MB and 4 GB, so a 2 GB device gives the working set 256 MB. Everything a
 * working set costs counts against it. Measured on the 5×10^5-rune stress
 * database (Hormiga's tests/store_bench.cpp), per byte of state text:
 *   Void Core's tree ×6.1 · the projected scene ×5.8 · each undo frame ×5.5
 *   · the exported text ×1 · a `save`'s baseline ×5.5 · transient parses ×6
 * so a working set costs about (25 + 5.5 × undo depth) times its text. The
 * undo depth is part of the budget: 4 on a small machine, 16 on a large one.
 *
 * THE PLAN: a database that fits is loaded whole, and is exactly what it was
 * before the store. One that does not keeps its small mantles whole
 * (documents, settings, the Antfarm) and loads the large ones by cluster: the
 * leaf the person is looking at, then its neighbours in the hierarchy (leaves
 * of one parent sort together) while they fit. A large mantle with no
 * clustering yet loads its first runes, in order. */
#pragma once

#include "rancho/store.hpp"

#include <cstdint>
#include <string>

namespace rancho {

struct Budget {
    std::uint64_t bytes = 256ull << 20; // what the working set may cost in memory
    int undo_depth = 4;

    static Budget for_machine(std::uint64_t physical_bytes);
    /* The state text that fits in `bytes`. */
    std::uint64_t text_bytes() const;
};

struct Plan {
    Selection sel = Selection::all();
    bool partial = false;          // some runes are not loaded
    std::uint64_t text_bytes = 0;  // the selection's estimated state text
    long long runes = 0;           // runes it loads (estimated for a partial plan)
    long long total_runes = 0;     // runes the database has
    std::string focus;             // the leaf it was built around, "" if none
};

/* `focus_rune` (in `focus_mantle`), when given, is in the plan. */
Plan plan(Store& store, const Budget& budget, const std::string& focus_mantle = {},
          const std::string& focus_rune = {});

} // namespace rancho
