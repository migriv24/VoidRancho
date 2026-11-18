---
type: Concept
title: Deed — the .rancho file
description: "The small JSON file that says who a database is and where its bytes live: an id minted once, and parts (a live store, pinned or live segments, a blobs folder) with the mantles each holds, by id. A store may carry its own deed. A deed holds Rancho's members only, and a database declares no kind: a host brings its own expectations. Format 1, the rules a reader keeps, the statuses a part can have, the API (rancho::Deed, rancho::Database, rancho::Expectation), the `rancho` tool, the tests, and what is planned."
resource: src/deed.cpp
tags: [status:current, audience:dev, audience:library, confidence:asserted]
timestamp: 2026-11-06T00:00:00Z
---

A **deed** (*la escritura*) is a database's identity and placement, written as a small
JSON file with the extension **`.rancho`**. The rationale is
[one database, many files](/design/one-database-many-files.md), and the words are in the
[language](/concepts/language.md).

```json
{
  "rancho": 1,
  "id": "3f9c0a52e1b84d7d9a6c2f0e5b7d1a48",
  "name": "The database",
  "home": "main",
  "parts": [
    { "id": "main",     "form": "store",    "path": "main.db" },
    { "id": "archive",  "form": "segments", "path": "//nas/share/archive",
      "mantles": ["mantle_5e0c2a91d4"], "pin": "sha256:<64 hex>" },
    { "id": "pictures", "form": "blobs",    "path": "pictures" }
  ]
}
```

# Format 1

| member | | meaning |
|---|---|---|
| `rancho` | required | the format number. A reader reads a newer format for what it knows, notes it, and **never rewrites it** |
| `id` | required | who the database is. Minted once (`mint_database_id`: 128 bits as hex), and kept by every copy of the deed and every re-placement. Any non-empty text is accepted, so a host with ids of its own may use them |
| `name` | | for a person; never an identity |
| `home` | | the part with the top-level keys, which a working set writes. Absent: the first store part, else the first segments part. A segments home makes a **read-only database**: `home()` is null |
| `parts` | | the places the bytes live, in reading order |

A part:

| member | | meaning |
|---|---|---|
| `id` | required | unique in the deed |
| `form` | required | `store`, `segments` or `blobs`. Any other form is kept, and the part reads as **not openable** |
| `path` | for the three forms | relative to the deed's folder (portable), or absolute (noted: it opens only where that path exists), or a UNC path (a network part, opened as `rancho/media.hpp` says) |
| `mantles` | store, segments | the mantles the part holds, **by key**: Core's `mantle.id`, which a rename keeps, or `name:<name>` for a mantle written without one. A bare name is read as a fallback, and noted. Absent: every mantle that no other part names. At most one part may hold the rest |
| `pin` | segments only | `sha256:` and the hex of the manifest this part must be. Absent: the part follows its `ROOT` |
| `database` | store, segments | another database's id, when the part is that database's: its store must record it, else the part is a mismatch (see Overlays below) |
| `overlay` | store, segments | the store part that keeps a person's edits over this one, read as base ⊕ overlay (see Overlays below) |

**Rancho is agnostic** (the author, 2026-11-06: *"rancho doesn't care if something is even
real, so long as it somehow goes into a mantle"*). A deed holds **Rancho's own members and
nothing else**. Whatever a host has to say about its database is data, and belongs in a mantle,
where Core's dispatcher journals it. So:
- **A member Rancho does not know is passed through unchanged, and noted.** It is never read,
  never interpreted and never dropped (`deed_test`).
- **A deed written by a newer Rancho is read for what this version knows, and never rewritten
  by it.** `write_deed` and `embed_deed` refuse it, and so does `rancho ids`. An older Rancho
  cannot tell which of a newer one's members an edit would make false (PNG's *safe-to-copy*
  problem: [beyond one database](/design/beyond-one-database.md) §5.5), so it does not edit.
  Nothing it cannot judge is ever left behind, stale.
- The members of a part in a form this version does not read (a `bucket`'s `url`) belong to
  that form, and are kept with it.

# Rules a reader keeps

- **Everything is named by its id** (the author, 2026-11-06: *"mostly indexing with the ID or
  Spirit"*). A part names its mantles by key. A key match in any part wins over a name match,
  so a name left in an older part never takes a mantle another part names by id. A mantle
  named by its name is found, with a note, and `rancho ids` rewrites the deed with keys.
  **Why:** named by name, a mantle renamed in its part fell to the home and left the document
  (`deed_test`).
- **A mantle lives in one part:** the first part that names it, else the part that holds the
  rest. A part of a form the reader does not know still holds the mantles it names, so a stale
  copy elsewhere is never read in their place. Such a part never holds the rest.
- **The home holds the top-level keys** (config, active, scripts, bindings, version).
  **Declared glyphs and domains** are the union of every present part's, and the home's win.
  A disagreement is noted, because a mantle read from a part needs that part's declarations to
  mean what it meant.
- **Mantles come in the deed's part order**, and within a part in that part's order. Order
  carries no meaning (SPEC §4).
- **A part's rows for a mantle the deed places elsewhere are not read**, and a note says so.
  This happens to a copy left behind by a move.
- **Opening creates nothing** but the cache, and a part's file is never made by an open.
  The cache is `DatabaseOptions::cache_dir`, else the temp folder, else (on a device with
  none) `.rancho-cache` beside the deed.
  `Database::create` alone makes a home store.

# A store that carries its own deed

A store can hold its deed in its own meta (`rancho.deed`, written by `embed_deed` or
`rancho embed`). Then the one SQLite file is the whole rancho, with no file beside it. It is the
first **hybrid** ([beyond one database](/design/beyond-one-database.md) §5.3): fully a store and
fully a rancho.
- `Database::open` takes the store itself.
- The embedded deed's home is **whichever file carries it**, so a copied or renamed file still
  finds itself.
- A store that carries no deed opens as a database of one part. Its deed (`deed_for_store`) is
  held in memory and written nowhere.
- `embed_deed` refuses a deed whose id is not the one the store records.

# Expectations: a database declares no kind

A **file type built on the rancho** (`.miga`, `.ink`) is its host's: its extension, its bale,
its vocabulary, and the assumptions it makes about what it opens. **The database declares none
of them** (the author, 2026-11-06: *"A database exists on its own"*). The host checks what it
relies on, structurally, rather than trusting a label in the file
([beyond one database](/design/beyond-one-database.md) §5.2).

- `rancho::Expectation {name, builds_on, check}` is one thing a host relies on, given in
  `DatabaseOptions::expect`. The name is the host's own, and Rancho keeps no registry of them.
- `Database::open` runs each expectation's check after the ones it builds on, each once. A
  cycle holds nothing, and one that builds on an expectation the host did not give does not
  hold.
- `Database::meets(name)` is true when that expectation and everything it builds on found no
  error.
- `expectation_check()` holds what the checks found, each line prefixed with its name. A
  database that falls short still opens, and says what is missing.

Rancho decides nothing about what a host expects. That is the host's.

# Statuses

| status | when | the database |
|---|---|---|
| `present` | the part is there and is what the deed names | reads it |
| `missing` | not at its path, or cannot be opened (the reason says which) | opens without it: its mantles are absent |
| `not openable` | a form this version does not read | opens without it, keeping the part in the deed |
| `mismatch` | there, but not what the deed names: no snapshot matches the pin, a home store that records another database's id, or a blobs "folder" that is a file | opens without it, and never writes to it |

`Database::complete()` is true when every part is present. That is the head count (*la
cuenta*) at the level of parts. A database that is not complete answers for what it holds,
and **what it does not hold is unknown, never deleted**.

# The API (`include/rancho/deed.hpp`)

- `Deed`, `Part`, `parse_deed`, `deed_json`, `read_deed`, `write_deed` (atomic),
  `check` (errors and notes), `deed_for_store` (a deed for a store that has none),
  `mint_database_id`.
- `Database::open(file, options)` opens every part it can and reports each one's status.
  `file` is a deed or a store.
  `Database::create(file, name)` makes a new database with a home store beside the deed.
  Their reads:
  - `home()` (the store a working set writes; null for a read-only database);
  - `store_of(mantle)`;
  - `placement()` (each mantle and its part, absent ones included);
  - `write_state` / `state` (the document across parts, streamed row by row through
    `Store::write_mantles`);
  - `blob(path)` (from a blobs part, else the home's blobs; a path that climbs out of its part
    is refused);
  - `notes()`;
  - `meets(name)`, `expectation_check()`;
  - `deed_by_id()` (the deed with its mantles named by key).
- `embed_deed(store, deed)` puts a deed inside its home store.
- **A segments part is read through a restored copy** in `DatabaseOptions::cache_dir`. The
  copy is named by the manifest's hash and restored only when it is not there yet. It is
  derived data, so it belongs in a phone's `cache_dir`.
- **The home records whose it is** (store meta `rancho.database`). A deed that points another
  database's id at that store reads it as a mismatch and does not write to it.

Added to `Store` for this, all additive: `write_mantles`, `mantle_names`, `mantle_refs` (with
each mantle's key, `MantleRef`), `top_keys`, and `top`. `write_state` is now written through `write_mantles`, with the same bytes.

# The tool

`rancho` (`tools/rancho.cpp`, built when Rancho is on its own or with
`-DRANCHO_BUILD_TOOLS=ON`):

```
rancho new   <file.rancho> [name]      a new database: a deed and its store
rancho deed  <store.db> [file.rancho]  a deed for a store that has none
rancho check <file.rancho|store.db>    its parts, where each mantle lives, what is absent
rancho ids   <file.rancho>             write the mantles it names by name as their ids
rancho embed <file.rancho>             put the deed inside its home store: one file
```

`check` exits 0 when the database is complete, 1 when it opened incomplete, and 2 when the
file is not a deed.

# Promises (`tests/deed_test.cpp`)

1. A deed round-trips and keeps what it does not know. A newer deed is read and never
   rewritten (`write_deed` and `embed_deed` refuse it), and a member Rancho does not know in its
   own format is noted.
2. A text that is not a deed is refused with a reason.
3. The check finds what is wrong: two parts with one id, a mantle in two parts, two parts that
   hold the rest, a pin on a live store, a malformed pin, mantles on a blobs part, a home that
   is not a part or holds no rows.
4. A new database has an id and a home, and a deed is never overwritten.
5. One store is a database of one part, and its document is the store's.
6. **The placement law:** one part or three give the same document (a selection's included),
   and Core reads it as the whole one. The three are a home store, a second store, and a
   segments part restored through the cache. The cache is reused on the next open.
7. A missing part is a region not loaded: the database opens, the home stays writable, the
   absent mantles are listed, and no part's file is created.
8. A pinned part is that snapshot or nothing. A pin holds the first snapshot after `ROOT`
   moves. Without a pin the part follows `ROOT`, and a wrong pin is a mismatch. A database
   whose home is a snapshot is read-only.
9. A form this version does not read is kept and skipped, and still holds what it names.
10. Another database's store is not taken as a home, and is left untouched.
11. Declarations travel with their part.
12. A blob is read from its part and never from outside it.
13. **A mantle is placed by its id, and a rename keeps its place.** Named by name, a mantle
    renamed in its part leaves the document; named by key, it stays. A name is read, and noted.
14. A database declares no kind, and a host brings its expectations. What an expectation
    builds on is checked first, each once; a complaint is reported while the database still
    opens; one that builds on what the host did not give does not hold; and a host that
    expects nothing opens the same database the same way.
15. A store can carry its own deed. Renamed, with no file beside it, it opens with its other
    parts. A store with none is a database of one part, and a file that is neither is refused.

# Overlays: a person's edits over a part they do not own (2026-11-09)

Void Squid's forcing case. A game library is its own rancho, rebuilt by a toolbox. A project
reads it as a read-only part and a person edits its runes. Each edit must survive the rebuild.
Two part members:

| member | | meaning |
|---|---|---|
| `database` | store, segments | the id of **another** database whose part this is. Its store must record that id (`rancho.database`): a library moved, or swapped for another, is a **mismatch**, never a silent substitute (Squid's question 1) |
| `overlay` | store, segments | the id of the store part (usually the home) that keeps a person's edits over this part. Not the home itself: the home is written, an overlaid part only read |

The overlay store keeps, per base rune, keyed by its mantle's key and its **spirit id**, the
difference between the base and what the person's Core says (`rancho/overlay.hpp`): tags added
with their source, tags removed, members set (a content field, a facet, the glyph, the
placement, the name), or the whole rune when the person made it. Reading the part gives
**base ⊕ overlay**. Squid's splice is the specification, and `deed_test` checks each of its
rules across a rebuild:

1. a tag the person added stays, with its source;
2. a tag the person removed stays removed, even when the rebuilt base has it again;
3. a member the person set wins over the base's new value;
4. a rune the person made inside a base mantle stays;
5. identity is kept: everything is keyed by spirit id;
6. the placement is the person's;
7. an edit to a rune the rebuild removed is **never dropped**: it is kept, and named by
   `Database::residuals()`.

**In this first form** a host records each rune it changed with `Database::record_overlay`
(and `forget_overlay` to revert). A working set does not yet route the rows of a base mantle
there: that is writing to more than one part ([open questions](/design/open-questions.md)
§12–13). A mantle with an overlay is held in memory while the document is written (a library's
catalogue, not a 5×10⁵-rune mantle). Edges, and the removal of a base rune, are not overlaid
yet.

**A blobs part at `.`**, the deed's own folder, is how a library holds its files beside its
catalogue. It is a promise, not an accident: `check` says nothing about it, and a path that
climbs out of the folder is refused as for any blobs part.

# Status

`current`: the first form. **Planned** ([one database, many files](/design/one-database-many-files.md)
§7–8):
- writing to more than one part, and moving a mantle between parts: the identity mapping,
  proved, then placement ([open questions](/design/open-questions.md) §12–13);
- mappings between databases, and the dataspace with its federation
  ([beyond one database](/design/beyond-one-database.md));
- splitting within a mantle;
- shared parts and generated parts; overlays routed from a working set, overlays of edges, and
  of a base rune's removal;
- remote parts (the bucket form of [tiers](/concepts/tiers.md));
- derived parts, which take the store's derived tables out of the record's file;
- a bale form;
- the deed in the C ABI.
