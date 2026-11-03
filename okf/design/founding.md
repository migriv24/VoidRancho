---
type: Design
title: Founding — the ruling, and the databases the family already built
description: "The author's direction of 2026-10-30, verbatim where it matters; a survey of how Void Hormiga, Void Squid, Void Loops Studio and Void Core's own holidays store data today; and what Rancho takes from each."
tags: [status:current, audience:dev, confidence:asserted, foundation]
timestamp: 2026-10-30T00:00:00Z
---

# Founding

## 1. The ruling

The author, 2026-10-30:

> *"database management is crucial to hormiga, and it will be for any other application. If
> we look at void loops studio, the audio and such and the node graph will be a database. if
> we look at void squid, right now its dealing a lot with databases … a database seems to be
> an important concept that every single void core application will have."*

> *"this is different than the current 'antfarm' within hormiga. The antfarm is all about
> ways to connect with other external apis and such and to use them in real time to update a
> database. However, the database logic itself has been an afterthought, when in reality its
> one of the most important parts of any application."*

> *"what would a database look like to a jepa model? i think it would look very similar to
> our runes and mantle structure!"*

> *"Void Rancho will effectively be copying a lot of what hormiga already has, with a higher
> degree of dedication to using clustering and edge weights as floats and such. We will also
> be preparing Void Rancho to eventually work really well with JEPA models, meaning we need to
> consider implementing a lot of matrix math … We have to start thinking about databases at
> scale as well, and what it might mean to share data between network drives or on a NAS …
> some of the clients using hormiga are already running into that being their biggest
> bottleneck."*

Five requirements, then: **(1)** generalize Hormiga's store; **(2)** clustering and float
edge weights as first-class; **(3)** a matrix view for learned models; **(4)** scale, speed
and shared media; **(5)** stay out of Core.

## 2. What the family already built (read 2026-10-30)

### Void Hormiga — the most complete, and rung 1

`VoidHormiga:/concepts/platform/store.md` (decided 2026-10-27) replaced "the whole database is
one Core state document" after a client's 5×10⁵-rune stress database (296 MB on disk) **peaked
at 22 GB of memory**. What it built, which Rancho should take nearly whole:

- **SQLite, one file per database**: one row per rune (mantle, order, name, glyph, id, and
  its whole JSON as Core writes it), one per edge, one per tag (indexed; the tag grammar
  compiles to SQL), one per mantle, one per remaining top-level key, pictures as blobs.
- **Round-trip law**: the store writes the state document back out, whole or in part, equal
  to what went in. Core's `_baseline` is not kept — *"the store is the baseline."*
- **A working set** sized to the device (an eighth of physical memory, 128 MB–4 GB) holding
  small mantles whole and large mantles **by cluster** (Leiden `c1:`/`c2:` tags); changing the
  working set rebuilds Core from the store.
- **Write-back after every dispatch**: common edits are written for the runes they name by
  re-asking Core (`cat`, `links`); deletions and renames are applied in SQL to unloaded rows;
  anything else re-syncs the working set from one export.
- **A model-based test**: random commands against a working set and against a Core holding
  everything must export the same document after every command.
- **Views read the store, not Core**: the data tab by page, the calendar by month, the map by
  viewport, publishing by query.
- Also from Hormiga: **a database is its own room** (`database-separation.md`, 2026-10-18) —
  a database's identity is its sync id, and a copy is a new database.

### Void Squid — the copy

`VoidSquid:/design/four-games-and-the-asset-database.md`: an `assets.db` in each world with
**"Void Hormiga's schema (runes, fields, tags, links)"**, mantles for games, kinds, wiki,
turkeys, vocabulary, tracks, maps. Its sharpest finding is one Rancho must design for:
**tags launder** — a builder wrote its own tags back into the catalogue, and the next run read
them as the game's own. Every tag there now records its source (`game:`, `wiki:`, `vision:`,
person). Provenance per tag is a store concern.

### Void Loops Studio — the next one

A project is a mantle drawn as a dataflow graph; audio is the host's. Its collaboration
design (`VoidLoopsStudio:/concepts/collaboration.md`) syncs over Palabra. A project with
recorded audio is a database of blobs plus a graph.

### Void Core's own holidays — the prior art at home

- **MeshDB** (`VoidCore:/components/meshdb-holiday.md`, which passed its own smoke test on 2026-06-18): a local graph
  BaaS over Bolt/Cypher; `(:Rune)-[:TAGGED]->(:Tag)`; the tag grammar compiled to Cypher with
  **7/7 parity against `ls --tag`**. Never wired into an application. Its lesson is the parity
  test: a store's query must answer exactly what Core's grammar answers.
- **LocalJSON** — a record⇄rune `Lens` with the round-trip law.

## 3. What Rancho takes

| from | takes |
|---|---|
| Hormiga | the schema, the round-trip law, the working set and its budget, write-back, the model-based test, views over the store, "a database is its own room" |
| Squid | provenance on every tag |
| Core's MeshDB holiday | query parity against Core's own grammar as a conformance test |
| Core | `vc_tag_match` as the one grammar; glyph declarations as the one schema; the journal as the change record |
| the author | clustering and float weights first-class; the matrix view; scale and shared media |
