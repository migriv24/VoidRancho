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
Not yet: blobs by content hash (they are keyed by path; Palabra's
content-addressed blocks are the plan), and `ls --kind` as a query filter.
