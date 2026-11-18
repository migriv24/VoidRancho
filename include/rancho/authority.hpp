/* rancho/authority.hpp — which file is in charge, and where everything else
 * lives (okf/concepts/authority.md).
 *
 * THE AUTHOR'S RULING (2026-11-18): the file a person opens, moves and shares
 * is the database. Everything a host keeps beside it to work fast is a cache
 * of it, and may be lost: *"If for some reason the json file gets deleted, oh
 * well, we will deal with that. the miga file would be almost impossible to
 * replace though."*
 *
 * So a database a person handles has one AUTHORITY: a file (a host's bale, a
 * store carrying its deed) that is the whole database or names its parts. A
 * host's working copy of it is a CACHE in a per-user cache folder, never beside
 * the authority, one per authority FILE: two copies of one database on one
 * machine are the same database (one id) and two files, and must not share a
 * working copy. A NOTE in each cache says which authority it is of, how that
 * file looked when the two last matched, and whether the cache holds edits the
 * authority does not. From that note alone a host knows, at open, whether the
 * cache can be used, must be rebuilt, or holds unsaved work.
 *
 * What a host keeps that is neither the authority nor a cache (credentials,
 * backups) is a KEEPSAKE: per user, outside the cache, never inside a file a
 * person shares.
 *
 * Rancho decides none of a host's formats: the authority's bytes and the
 * cache's contents are the host's. This is where they are, and how they stand.
 * One process at a time works in one cache (the host holds its own lock); the
 * index of caches is written atomically. */
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rancho {

/* The per-user folder a host's caches live in. `RANCHO_CACHE_DIR` overrides it
 * (tests, a portable install); else the system's cache location: Windows
 * `%LOCALAPPDATA%\Void\<host>\cache`, macOS `~/Library/Caches/Void/<host>`,
 * elsewhere `$XDG_CACHE_HOME/void/<host>` or `~/.cache/void/<host>`; else the
 * temp folder. Not created by asking. */
std::filesystem::path cache_root(std::string_view host);

/* The per-user folder for keepsakes: what is neither the authority nor a cache.
 * `RANCHO_KEEP_DIR` overrides it; else Windows `%LOCALAPPDATA%\Void\<host>\keep`,
 * macOS `~/Library/Application Support/Void/<host>`, elsewhere
 * `$XDG_DATA_HOME/void/<host>` or `~/.local/share/void/<host>`. */
std::filesystem::path keep_root(std::string_view host);

/* How a file looked: enough to tell that it changed, read without reading it. */
struct Stamp {
    bool exists = false;
    std::uintmax_t size = 0;
    std::int64_t mtime = 0; // the file clock's ticks: compared, never shown
    bool operator==(const Stamp&) const = default;
};
Stamp stamp_of(const std::filesystem::path& file);

/* What a cache says about itself (`cache.json` in it). */
struct CacheNote {
    std::filesystem::path authority; // absolute
    std::string database;            // the database's id, when the host knows one
    Stamp seen;                      // the authority as it was when the cache last matched it
    bool ahead = false;              // the cache holds edits the authority does not
};

enum class CacheState {
    New,      // just made: nothing in it yet; fill it from the authority
    Current,  // it and the authority match: use it
    Stale,    // the authority changed since (saved elsewhere, replaced): rebuild from it
    Ahead,    // the cache holds edits the authority has not got: use it, and save them
    Diverged, // both: the authority changed AND the cache holds edits; keep the edits aside, then rebuild
    Orphan,   // the authority is gone (moved, renamed, deleted): the cache may be all that is left
};
const char* state_name(CacheState s);

/* The cache for `authority` under `root`: the one the index names for that
 * file, else a new folder (state New). An authority is named by its absolute,
 * normal path (case-folded on Windows). */
std::filesystem::path working_copy_for(const std::filesystem::path& authority, const std::filesystem::path& root);

std::optional<CacheNote> read_note(const std::filesystem::path& cache);
bool write_note(const std::filesystem::path& cache, const CacheNote& note); // atomic
CacheState cache_state(const std::filesystem::path& cache);

/* The host changed the cache and has not written the authority. */
bool mark_ahead(const std::filesystem::path& cache);
/* The host wrote the authority from the cache, or filled the cache from it:
 * they match now. */
bool mark_matched(const std::filesystem::path& cache);
/* Save As: the cache now belongs to another file (the index follows; nothing
 * is moved). The old file keeps no cache. */
bool reassign(const std::filesystem::path& cache, const std::filesystem::path& new_authority,
              const std::filesystem::path& root);
/* Forget a cache: its folder and its line in the index. */
bool forget(const std::filesystem::path& cache, const std::filesystem::path& root);
/* Every cache under `root`, for a tool that lists or clears them. */
std::vector<std::filesystem::path> caches(const std::filesystem::path& root);

} // namespace rancho
