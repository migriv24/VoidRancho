/* rancho/working_set.hpp — what Void Core holds of a database, and keeping the
 * store in step with it (okf/concepts/working-set.md, okf/concepts/write-path.md).
 *
 * `load` gives Core a state document the store writes for a Selection: the
 * mantles chosen whole, the clusters chosen of the rest, and edges whose both
 * ends are loaded. Every edit is still a dispatcher command on that Core — the
 * one door. `sync`, called after commands land, makes the store say what Core
 * now says:
 *
 *  - what Core holds is compared with what it held at the last sync, rune by
 *    rune and edge by edge, and only the difference is written;
 *  - a rune is followed by its spirit id, so a rename is a rename (every edge
 *    that names it changes too, loaded or not) and an undo of a removal brings
 *    back what the removal took, including edges to runes never loaded;
 *  - runes Core never held are never touched.
 *
 * The cost of a sync is proportional to the working set, never to the
 * database. A NEW rune may not take the name of a rune that is not loaded:
 * Core cannot see it, so `name_taken` is what a caller minting names asks, and
 * `sync` refuses such a write rather than overwrite a rune nobody looked at.
 *
 * Ported from Void Hormiga's src/store/working_set.hpp (2026-10-27); the Core
 * is any CoreLink (rancho/core.hpp) rather than a maiz::Core, and the tags a
 * sync writes carry the working set's ACTOR as their source.
 *
 * Rancho's railguard, mechanically: every row this writes is one Core already
 * holds. Nothing here can say what a command did that Core does not. */
#pragma once

#include "rancho/core.hpp"
#include "rancho/store.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace rancho {

class WorkingSet {
public:
    WorkingSet(Store& store, CoreLink& core) : store_(store), core_(core) {}

    /* Replace what Core holds with `sel`'s part of the store. Core's undo
     * history starts over (a working set is a new document to Core), so rows
     * the last session soft-deleted are purged first. */
    bool load(const Selection& sel);
    /* The same, when Core already holds `sel`'s document (it was built from
     * `store.state(sel)` elsewhere, on a worker, say): the snapshot only. Core
     * must not have changed since that document was written. */
    bool adopt(const Selection& sel);
    /* Dispatch through the working set. The same as `core.dispatch`, except
     * that the working set learns what the command named, so `sync` can write
     * those runes and edges alone: the common edits (set, setjson, facet, tag,
     * measure, rune new, link, unlink, use, config set, and batches of them)
     * cost what they touch, not what is loaded. */
    Reply dispatch(const std::string& command);
    /* Bring the store into step with Core. Call after any command that may
     * have changed something; calling it when nothing did writes nothing.
     * Commands that did not come through `dispatch`, or that it cannot read,
     * are written back by comparing everything Core holds. */
    bool sync();

    /* True if `name` is a live rune of `mantle` in the store, loaded or not. */
    bool name_taken(const std::string& mantle, const std::string& name);

    /* Who the edits are by — a person, "agent:needle", "antfarm:<connector>" —
     * recorded as the source of every tag a sync writes for the first time. */
    void set_actor(const std::string& actor) { actor_ = actor; }
    const std::string& actor() const { return actor_; }

    const Selection& selection() const { return sel_; }
    const std::string& error() const { return error_; }
    struct Stats {
        long long syncs = 0, runes_written = 0, runes_removed = 0, renames = 0, restores = 0, edge_keys = 0;
        long long refused = 0; // new runes that would have overwritten an unloaded one
        long long targeted = 0, full = 0; // syncs that wrote what commands named / compared everything
    } stats;

private:
    struct RuneSnap {
        std::string id;
        std::uint64_t hash = 0;
    };
    struct MantleSnap {
        std::string name, mid;
        long long row = -1;
        std::uint64_t shape = 0; // skeleton, layout and flags
        std::unordered_map<std::string, RuneSnap> runes;
        std::unordered_map<std::string, std::vector<std::uint64_t>> edges; // key -> sorted hashes
    };
    struct Snapshot {
        std::unordered_map<std::string, std::uint64_t> top;
        std::vector<MantleSnap> mantles;
    };

    bool sync_full();
    bool sync_targeted();
    bool note(const std::vector<std::string>& argv); // false: this command needs the full comparison

    /* What commands through `dispatch` named since the last sync. Void Core's
     * journal records the action and not its effect, so this reads the
     * command itself; when Core records what a command `touched` (its world
     * models phase 1), this becomes a read of that. */
    struct Touched {
        std::unordered_map<std::string, std::vector<std::string>> runes; // mantle -> names
        std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> pairs; // mantle -> ends
        bool config = false; // `config set`: the state's config object
    } touched_;
    bool full_ = false;              // something needs the full comparison
    std::uint64_t expected_rev_ = 0; // Core's revision after the last command through `dispatch`
    std::string active_;             // the mantle Core's verbs act on
    std::string actor_;

    Store& store_;
    CoreLink& core_;
    Selection sel_ = Selection::none();
    Snapshot snap_;
    std::uint64_t synced_rev_ = 0;
    std::string error_;
};

} // namespace rancho
