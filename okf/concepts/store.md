---
type: Concept
title: Store
description: The database at rest — one row per rune, edge and tag, the whole JSON kept beside typed columns derived from what Core already wrote; the round-trip law that makes it safe; weights, ports, provenance, the row index and glyph-typed fields; and a store Void Hormiga wrote, migrated in place.
resource: src/store.cpp
tags: [status:current, audience:dev, audience:library, confidence:asserted, foundation]
timestamp: 2026-11-02T00:00:00Z
---

The **store** is a database at rest: everything a Void Core state document holds, laid out so
that one rune can be found among millions without reading the rest. It is Void Hormiga's
store (`VoidHormiga:/concepts/platform/store.md`, 2026-10-27) extracted and generalized —
`include/rancho/store.hpp` keeps every member Hormiga's `store.hpp` had, with the same
meaning, so Hormiga can move to it by changing a namespace.

# Shape (format 2)

| table | one row per | indexed by | keeps |
|---|---|---|---|
| `runes` | rune | mantle+name, mantle+pos, mantle+spirit id, mantle+glyph | its **whole JSON as Core wrote it**, its bytes, a soft-delete mark |
| `edges` | edge | mantle+from, mantle+to, mantle+relation, mantle+pos | whole JSON, plus **`weight` (float64), `directed`, `from_port`/`to_port`** (a relation spelled `i:j`) |
| `tags` | (rune, tag) | tag+rune, rune | the tag's **`source`**: who wrote it |
| `mantles` | mantle | — | its skeleton (everything but runes and edges), layout, id |
| `top` | other top-level key | key | `version`, `config`, `glyphs`, `active`, `scripts`, `domains`, `bindings` |
| `rows` | (mantle id, spirit id) | mantle+row | the **row index ρ** — Rancho's own, never reused |
| `fields` | (rune, annotated field) | field+number | **typed columns** from the glyph's `kinds` |
| `clusters`, `cluster_runs` | rune / mantle | mantle+path | the leaf a rune is in; the settings that made the leaves |
| `blobs` | asset | path | the bytes of pictures a bundle carried |
| `est_*`, `store_meta` | — | — | planning estimates; the format and notes |

Everything Rancho added is **derived from what Core wrote**, never authored here: a weight is
read off the edge object, a port pair off its relation, a typed field off the content under
the glyph's own `kinds` annotation. **Rancho never declares a schema of its own** — a second
copy of the schema is the failure Core's glyph declarations exist to end. Glyphs a host
*registers* (rather than declares in the document) do not travel (SPEC §3.3.3), so a host
hands them to `Store::set_registered_glyphs` if their `kinds` should be columns.

**Typed columns** take each annotated field by its measurement level: a number, a numeric
string or (for every level but nominal) an ISO 8601 date indexed as Unix seconds; nominal
fields as text. Dates are points, not quantities (Core's quantity page) — indexed as numbers
so a calendar can ask for a month (`Store::range`), never added or scaled.

**Provenance.** Every tag row says who wrote it: `import` for an import, the working set's
**actor** for what a sync writes for the first time (`agent:needle`, `person:ada`,
`antfarm:<connector>`). A tag that was already there keeps its source through every rewrite
of its rune. Void Squid's finding — *tags launder* — made into a column.

**The row index.** ρ is assigned to a spirit id the first time the store sees it, in
document order, keyed by the mantle's id; an import keeps it; a removed rune's row is never
given to another. It is what makes two [matrix views](/concepts/matrix-view.md) of one
database at two times line up.

# Laws, and the tests that hold them

- **Round trip.** Writing the store back out, whole or as a selection, gives back what went
  in; a Core reads it to the same document (`store_test`: *a document comes back as it went
  in*, *a member that was absent stays absent*). A [snapshot](/concepts/segment.md)
  restores **byte for byte** (`segments_test`).
- **Query parity.** A tag expression over the store answers exactly what `ls --tag` answers,
  in the same order — 3 000 random expressions plus the grammar's corners
  (`query_test`). See [query](/concepts/query.md).
- **Order is not meaning.** Core preserves rune order but does not give it meaning
  (`VoidCore:SPEC.md §4`); the store keeps a position for the round trip and sorts by it for
  nothing else.
- **Memory does not grow with the file.** Importing 4× the runes costs the same extra memory
  — measured +10.0 MB at 2×10⁴ and +10.4 MB at 8×10⁴ (`store_test`). A selection, and a
  query by names or rare tags, cost what they return: 0.19 ms at both sizes.
- **A failed import changes nothing**, its indexes and the row index included.

# A store Void Hormiga wrote

A format-1 file (Hormiga 0.2.3's tables) opens and is **migrated in place**: the columns are
added and filled from the JSON every row already keeps, every rune is given its row, and the
document it writes is unchanged (`store_test`: *a store Hormiga wrote opens and is
migrated*). It is not a copy: the file is the same database, one format later.

# One host at a time

A store on a network path — a UNC path, a mapped network drive, NFS/SMB — is opened in
rollback-journal mode with an **exclusive lock held until close**: a second machine is
refused at open (*"the store is in use by another process or machine"*) instead of being
allowed to corrupt it. Elsewhere it is WAL. See [tiers](/concepts/tiers.md) and
`include/rancho/media.hpp`.

# Status

`current` (0.1.0). Since 2026-11-02 (Void Hormiga's ask) a host that keeps its pictures elsewhere
drops the store's copy with `remove_blob(path)` or `clear_blobs()`. Both change no rune, edge or
tag, because blobs are not part of the document. `compact()` (SQLite's `VACUUM`) gives the freed
pages back to the disk. It rewrites the whole file, so the host decides when to pay for it.
Since 2026-11-06 (the [deed](/concepts/deed.md)) a store can write its mantles alone
(`write_mantles`, which `write_state` is now written through, byte for byte the same), and lists its mantle names
and top-level keys cheaply (`mantle_names`, `top_keys`, `top`), so a database of several stores is
written as one document, row by row. `leaves` now says in its comment that it is exact and
reads every cluster row; a list for a person is `leaf_estimates`.

**Since 2026-11-09:**
- **A file that is not a store is refused untouched** (Void Squid's finding). Opening used to
  write first: WAL on, eight tables added, and only then a failure, so Squid's pre-Rancho
  `assets.db` (three of its table names are ours) was changed by `rancho ids`. Now the
  constructor decides first, writing nothing: a store is an empty file, a file whose
  `store_meta` names a format, or a file with Hormiga's format-1 tables. Anything else, a
  foreign SQLite or not a database at all, is refused with its tables named, byte for byte as
  it was (`store_test`). A file another connection holds exclusively is reported as in use,
  as before.
- **`pack_into`**: a copy without the indexes every open rebuilds, for a file that is carried.
  Void Hormiga asked why a v4 bundle is 2.5 times its v3 one. Measured on an 8×10⁴-rune store
  (26.7 MB of JSON, 67.7 MB of store), it is the round-trip law, not waste: runes and edges are
  35 MB (Core's own text, kept byte for byte, with its columns), tags 14 MB (rows and two
  indexes), the row index 4.7 MB, and the other indexes 11 MB. Packed, the indexes go, and the
  first open of the copy builds them again.
- **`find_names`**: by prefix (one index range, in name order) or by part of a name (ASCII
  letters in any case, reading names until the limit). **`clusters_of(names)`**: leaves in one
  statement. The [cluster](/concepts/cluster.md) tree's upper levels and its reseña.
- **Phones** (Void Maiz's asks): `OpenOptions::temp_dir` sets SQLite's process-wide spill
  folder (an Android app outside Maiz's shell has none), and `release_memory()` gives the page
  cache back on a low-memory event. A working set is durable when `WorkingSet::sync` returns
  (WAL, synchronous=NORMAL), so backgrounding needs that one call, which costs what the edits
  since the last sync touched. See [tiers](/concepts/tiers.md) for which files go where.
- **Overlays** are kept in a store (`overlays`, `put_overlay`), written only by the
  [deed](/concepts/deed.md)'s `record_overlay`.
- **Two connections** to one store work on a local disk, through WAL: one writer at a time,
  the other waiting up to two seconds. On a network path a store is opened single-host, with
  an exclusive lock held until close, so a second connection, **even in the same process**, is
  refused. A host that reads on a worker there shares one `Store` and serializes its calls.

Not yet: blobs by content hash (they are keyed by path; Palabra's
content-addressed blocks are the plan), and `ls --kind` as a query filter.
