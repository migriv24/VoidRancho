---
type: Design
title: Boundaries — what Rancho owns and what it refuses
description: The division of labour across Void Core, Void Palabra, Void Bicho, Void Maiz and Void Hormiga's Antfarm; Rancho's railguard; and why the database is a library above Core rather than a part of it.
tags: [status:current, audience:dev, confidence:asserted, foundation]
timestamp: 2026-10-30T00:00:00Z
---

# Boundaries

> Void Core's railguard: *"are we making the core compute the application's results?"*
> Void Palabra's: *"are we assuming an order we do not actually know?"* Void Bicho's: *"are
> we deciding something a caller should have decided?"* This is Rancho's:
>
> **Could every row be rebuilt by replaying commands?**
>
> If a row exists that no dispatcher command (or imported batch) produced, Rancho has become
> a second way to change the data — a second door — and every guarantee the family built on
> "the dispatcher is the only door" is gone.

---

## 1. Why above Core, not inside it

The author's instinct, 2026-10-30: *"it's always best to keep the void core 'pure' and to
have a separate library that builds off it dedicated to database management."* The reasons
in Core's own terms:

- **Core does no I/O, by definition** (`VoidCore:SPEC.md §9`). A store is I/O. Inside Core it
  would be the first file Core opens.
- **Core is the structure** — rank 1 of its four aspects (`VoidCore:/design/the-transition.md`
  §2). A database is one *use* of the structure, as a view (Maiz) and a sync (Palabra) are.
- **Not every peer needs one.** An ESP32 `State`-tier peer holds a document, not an index.

## 2. The division, stated once

| question | owner | why not Rancho |
|---|---|---|
| what a rune, mantle, glyph or edge *is* | **Void Core** | Rancho owns none of Core's vocabulary |
| how a change is expressed, undone, journaled | **Void Core** (the dispatcher) | the one door |
| store, index, query and page runes at scale | **Rancho** | — |
| read a mantle as matrices; graph analytics | **Rancho** (proposed — see §3) | — |
| canonical bytes and the name of a state | **Void Palabra** | Palabra's canonical form is the family's |
| merge two peers' changes; the history graph | **Void Palabra** | a CRDT is not a storage engine |
| the archive container (`save`, `load`, a VPAL file) | **Void Palabra** | see §4 — the overlap to settle |
| move data between devices (Reticulum) | **Void Palabra** | Rancho is local to one device or one shared medium |
| compute an embedding, run a model | **Void Bicho** | Rancho stores and indexes vectors; it never produces them |
| draw a mantle; gestures → commands | **Void Maiz** | — |
| connect a database to external APIs live | **the application** (Hormiga's Antfarm) | connectors emit commands; Rancho stores their results |

## 3. Graph analytics — a proposal, not a ruling

Void Hormiga asked Core on 2026-10-25 whether graph analysis belongs in Core or in the view
library, and Void Maiz already ships a C++ Leiden with refinement, a recursive hierarchy,
PageRank and components (`voidmaiz/graph.hpp`). **Rancho is a third answer**: the place that
holds the whole graph, not only the drawn part, is the place where a whole-graph algorithm
can run without loading it all into a view. Proposed; the author rules, and Maiz is asked
before anything moves. **Maiz was asked, and its lean agrees (2026-10-31)**, on two
conditions: the algorithms move as a leaf target that depends on neither SQLite nor Core, and
`voidmaiz/graph.hpp` forwards until hosts move ([open questions](/design/open-questions.md) §3).

## 4. The overlap with Palabra's "persistence" — to settle with Palabra

`VoidPalabra:/concepts/persistence.md` (2026-07-24) claims the container format, codecs,
filesystem mapping and platform differences — *"how Void Core state becomes bytes on a real
device."* Palabra's archive is a linear list of named cuts and a deduplicated VPAL file.

Hormiga still had to build its own **queryable** store, because a container answers "save
and load this state", and a database answers "find these runes among 10⁶ without reading the
rest". The proposed line:

- **Palabra:** bytes *at rest as a whole* — the container, its blocks, codecs, names, merge.
- **Rancho:** bytes *at rest as a live, indexed, partially-loaded database* — rows, indexes,
  pages, matrices — using Palabra's content-addressed blocks for blobs (pictures, audio) and
  Palabra's canonical names where a segment needs one.

To be put to Palabra as a message once the author agrees with the line.

## 5. The test for any future coupling

Inherited from Bicho: **if a proposed shared thing is a data shape, it is fine; if it is a
function call, one library is about to depend on the other.** Rancho reads Core's state
document and journal (data); it may *link* Core to drive a working-set manager, as every host
does. It must never be linked *by* Core.
