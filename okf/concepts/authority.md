---
type: Concept
title: Authority — which file is in charge
description: "The author's ruling of 2026-11-18: the file a person opens, moves and shares is the database, and everything a host keeps beside it to work fast is a cache that may be lost. One authority per database a person handles; a host's working copy is a cache in a per-user folder, one per authority FILE; a note says how the two stand (new, current, stale, ahead, diverged, orphan); keepsakes (credentials, backups) live per user, outside both; what a person points at stays where it is. The API (rancho::working_copy_for, cache_state, mark_ahead, mark_matched, reassign) and its tests."
resource: src/authority.cpp
tags: [status:current, audience:dev, audience:library, confidence:asserted]
timestamp: 2026-11-18T00:00:00Z
---

# Authority (2026-11-18)

Void Hormiga's author, on why a person who opened a `.miga` should never meet a
`demo-org.json`:

> *"the miga to be the main thing people are opening. we can keep json stuff for
> loading purposes ... they should always see and deal with the miga file, not
> the json too much. the miga file is what will be shared and moved around ...
> If for some reason the json file gets deleted, oh well, we will deal with that.
> the miga file would be almost impossible to replace though ... when we
> eventually get to handling larger files, we will use pointers to paths or other
> locations much more frequently ... this structure on how files are stored and
> what is really in charge belongs to void rancho to set a foundation first."*

So this page is the foundation: **which file is in charge, and what every other
file is allowed to be.**

## Four kinds of file

| kind | what | where | if it is lost |
|---|---|---|---|
| **authority** | the file a person opens, moves and shares: a host's bale (`.miga`), a store carrying its deed, a deed | wherever the person put it | the database is lost. Nothing replaces it |
| **cache** | a host's working copy: a state document, a SQLite mirror, a store, unpacked pictures, tiles, a rendered site | `rancho::cache_root(host)`, one folder per authority file | rebuilt from the authority. Unsaved edits in it are lost, and that is the only loss |
| **keepsake** | what is neither: credentials, backups | `rancho::keep_root(host)`, per user | lost to this machine. A keepsake never travels inside an authority |
| **pointer** | a file the database names and does not hold: a part elsewhere ([deed](/concepts/deed.md)), a picture on a drive, a document beside the authority | where it is | a region not loaded ([the frontier](/concepts/working-set.md)), never a deletion |

**The rules a host keeps:**

1. **A person meets the authority, and only the authority.** A host never puts a
   cache beside it, never shows a cache's file name as the database's, and never
   asks a person to open a cache.
2. **A write reaches the authority.** Edits land in the cache first, for speed;
   saving means the authority. A cache that holds edits the authority has not got
   says so (`ahead`), so nothing unsaved is mistaken for saved.
3. **A cache is one per authority FILE, not per database.** Two copies of one
   database (one id) at two paths are one database and two files; they get two
   caches, or one copy's edits would appear in the other.
4. **A cache knows what it is of.** Its note names the authority, the database id
   when known, and how the authority looked when the two last matched. From the
   authority's stamp alone (size and modification time, read without reading the
   file) a host knows what to do at open:

   | state | meaning | a host |
   |---|---|---|
   | `new` | nothing in it yet | fills it from the authority |
   | `current` | it and the authority match | uses it, unpacking nothing |
   | `stale` | the authority changed since (saved by another copy, replaced) | rebuilds it from the authority |
   | `ahead` | it holds edits the authority has not got | uses it, and says unsaved work was kept |
   | `diverged` | both | keeps the cache's edits aside (a keepsake backup), then rebuilds |
   | `orphan` | the authority is gone (moved, renamed, deleted) | offers what the cache holds; it may be all that is left |

5. **Losing a cache loses nothing saved.** `rancho::forget` is always safe for
   what the authority holds.
6. **Keepsakes are per user and outside the cache.** Credentials never travel
   inside an authority a person shares; backups survive a cleared cache.
7. **A pointer is not copied in.** What a database names elsewhere stays there; a
   missing one is a region not loaded. Relative pointers are relative to the
   authority's folder, never to a cache.

## The API (`include/rancho/authority.hpp`)

- `cache_root(host)`, `keep_root(host)`: per-user folders (overridable with
  `RANCHO_CACHE_DIR` / `RANCHO_KEEP_DIR`, for tests and portable installs).
- `working_copy_for(authority, root)`: the cache for that file, found by an
  index of absolute normal paths (case-folded on Windows), else a new one.
- `read_note`, `write_note` (atomic), `cache_state`, `mark_ahead`,
  `mark_matched`, `reassign` (Save As: the cache now belongs to the new file;
  nothing moves), `forget`, `caches`.
- *Tested* (`tests/authority_test.cpp`): found again for its file and no other;
  two copies get two caches; every state from the stamp alone; Save As; forget.

## What this answers

- **Open question 16** (is `.miga` a bale?): *answered by the ruling.* The
  `.miga` is Hormiga's bale and its authority, in Hormiga's own form; a Rancho
  bale form, when it exists, is offered, not imposed.
- **Open question 17** (whose id?): the authority carries the database's id
  (Hormiga's `sync.database`, travelling in its config); a cache records it, and
  is keyed by the file, not by it (rule 3).

## Not yet

- A bale form of Rancho's own.
- A host's open over a network path (the authority on a share): the cache is
  local by rule, so this already works; saving back over the network is the
  host's and follows [media](/concepts/tiers.md).
- Two processes on one cache: the host's lock (Hormiga's floor) keeps it one.
