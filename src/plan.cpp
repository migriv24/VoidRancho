/* plan.cpp — see rancho/plan.hpp. Ported unchanged from Void Hormiga. */
#include "rancho/plan.hpp"

#include <algorithm>

namespace rancho {

Budget Budget::for_machine(std::uint64_t physical) {
    Budget b;
    if (physical == 0) return b; // unknown: plan for the smallest promised machine
    b.bytes = std::clamp<std::uint64_t>(physical / 8, 128ull << 20, 4ull << 30);
    b.undo_depth = physical < (4ull << 30) ? 4 : physical < (16ull << 30) ? 8 : 16;
    return b;
}

std::uint64_t Budget::text_bytes() const {
    // (25 + 5.5 × depth) per byte of text, in halves to stay in integers
    return bytes * 2 / (50 + 11 * (std::uint64_t)std::max(undo_depth, 0));
}

Plan plan(Store& st, const Budget& budget, const std::string& focus_mantle, const std::string& focus_rune) {
    Plan p;
    const auto mantles = st.estimates();
    std::uint64_t total = 0;
    for (const auto& m : mantles) {
        total += (std::uint64_t)m.bytes;
        p.total_runes += m.runes;
    }
    const std::uint64_t cap = budget.text_bytes();
    p.text_bytes = total;
    p.runes = p.total_runes;
    if (total <= cap) return p; // it fits: the whole database, as before the store

    p.partial = true;
    p.sel = Selection::none();
    p.text_bytes = 0;
    p.runes = 0;
    std::vector<const MantleInfo*> large;
    for (const auto& m : mantles) {
        if ((std::uint64_t)m.bytes <= cap / 8) {
            p.sel.whole.insert(m.name);
            p.text_bytes += (std::uint64_t)m.bytes;
            p.runes += m.runes;
        } else {
            large.push_back(&m);
        }
    }
    // the mantle the person is looking at first, then the largest
    std::sort(large.begin(), large.end(), [&](const MantleInfo* a, const MantleInfo* b) {
        if ((a->name == focus_mantle) != (b->name == focus_mantle)) return a->name == focus_mantle;
        return a->bytes > b->bytes;
    });

    for (const MantleInfo* m : large) {
        const std::uint64_t room = cap > p.text_bytes ? cap - p.text_bytes : 0;
        if (room == 0) break;
        // a rune's share of its mantle's edges comes with it
        const double scale = m->rune_bytes > 0 ? (double)m->bytes / (double)m->rune_bytes : 1.0;
        // the first runes, in order: for a mantle not clustered yet, or whose smallest leaf is too big
        auto first_page = [&] {
            const auto pg = st.page(m->name, 0, (std::uint64_t)((double)room / scale));
            p.sel.by_name[m->name] = pg.names;
            p.text_bytes += (std::uint64_t)((double)pg.bytes * scale);
            p.runes += (long long)pg.names.size();
        };
        if (!st.clustered(m->name)) {
            first_page();
            continue;
        }
        const auto leaves = st.leaf_estimates(m->name);
        if (leaves.empty()) continue;
        auto cost = [&](const ClusterLeaf& l) { return (std::uint64_t)((double)l.bytes * scale); };

        std::size_t start = 0;
        const std::string want = m->name == focus_mantle && !focus_rune.empty() ? st.cluster_of(m->name, focus_rune)
                                                                               : std::string();
        bool found = false;
        for (std::size_t i = 0; i < leaves.size() && !want.empty(); ++i)
            if (leaves[i].path == want) {
                start = i;
                found = true;
            }
        if (!found) // the largest leaf that fits: the densest part of the graph
            for (std::size_t i = 0; i < leaves.size(); ++i)
                if (cost(leaves[i]) <= room && (cost(leaves[start]) > room || leaves[i].runes > leaves[start].runes))
                    start = i;
        if (cost(leaves[start]) > room) { // not one leaf fits this machine: a page that does
            first_page();
            if (found) { // and the rune in focus, whatever page it is on
                auto& names = p.sel.by_name[m->name];
                if (std::find(names.begin(), names.end(), focus_rune) == names.end()) names.push_back(focus_rune);
                p.focus = want;
            }
            continue;
        }

        std::vector<std::string> chosen{leaves[start].path};
        std::uint64_t used = cost(leaves[start]);
        long long runes = leaves[start].runes;
        // grow outwards in path order: the nearest leaves share the longest prefix
        std::size_t lo = start, hi = start;
        for (bool grew = true; grew;) {
            grew = false;
            if (hi + 1 < leaves.size() && used + cost(leaves[hi + 1]) <= room) {
                ++hi;
                used += cost(leaves[hi]);
                runes += leaves[hi].runes;
                chosen.push_back(leaves[hi].path);
                grew = true;
            }
            if (lo > 0 && used + cost(leaves[lo - 1]) <= room) {
                --lo;
                used += cost(leaves[lo]);
                runes += leaves[lo].runes;
                chosen.push_back(leaves[lo].path);
                grew = true;
            }
        }
        if (m->name == focus_mantle || p.focus.empty()) p.focus = leaves[start].path;
        p.sel.by_cluster[m->name] = std::move(chosen);
        p.text_bytes += used;
        p.runes += runes;
    }
    return p;
}

} // namespace rancho
