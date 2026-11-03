---
type: Design
title: Open questions
description: What is genuinely undecided about Void Rancho, each with a lean, for the author — and which leans 0.1 took in order to build, provisionally.
tags: [status:current, audience:dev, confidence:exploratory]
timestamp: 2026-11-02T00:00:00Z
---

# Open questions

**Taken in 0.1** marks a lean the first build needed and took
([building 0.1](/design/building-0.1.md)); the author may still overrule it, and nothing links
Rancho yet, so none is expensive to reverse.

1. **Names.** Descriptive names are used throughout, per Palabra's and Bicho's precedent. A
   ranch set, offered: the **corral** (the working set — the herd penned close at hand), the
   **pasture** (the cold tiers: [segments](/concepts/segment.md)), a **herd** (a cluster), the
   **brand** (a row's stable index ρ — but Core's `spirit` already names identity, and Rancho
   must not rename Core's words). The author chooses; nothing is renamed until then.
2. **The line with Palabra's persistence.** [Boundaries](/design/boundaries.md) §4 proposes
   Palabra = at rest as a whole, Rancho = at rest as a live indexed database. Lean: agree it
   with Palabra by message. *0.1:* segments are named by Palabra's hash (SHA-256), so a
   segment and a Palabra block can be the same object later; nothing else assumed.
3. **Where graph analytics lives** — Rancho, Void Maiz, or Core. Lean: Rancho, which holds the
   whole graph. *0.1:* nothing moved. Rancho compiles Maiz's `graph.cpp` in place for Leiden,
   and has its own sparse algebra (BFS, shortest paths, PageRank, components, A·X) over the
   matrix view — which overlaps Maiz's PageRank and components. Needs Maiz's agreement and the
   author's before either copy is retired. **Maiz, 2026-10-31 (its Q43): its lean agrees —
   Rancho — on two conditions:** graph analysis moves as a **leaf target with no SQLite and no
   Core**, so a view-only Maiz host can cluster a drawn scene without taking a database; and
   Maiz's `graph.hpp` **forwards** until hosts move (as networking did to Void Hammer). Both are
   compatible with Rancho's lean. Since 2026-10-31 Rancho links Maiz's `voidmaiz_graph` target
   rather than compiling the file, so a move is a path change in one CMake fragment.
   **Ruled 2026-11-02 by the author: Rancho.** Moved that day as `voidrancho_graph`
   (`rancho/graph.hpp`, a leaf). Maiz's `graph.hpp` forwards to it. The overlap with the matrix
   view's PageRank and components is kept: the undirected, weighted forms clustering reads
   beside the directed semiring forms of the matrix view. Closed.
4. **The system of record at rest.** *Taken in 0.1:* the store is the baseline; a full
   document is an export, and an import replaces the store.
5. **Language.** *Taken in 0.1:* C++20 behind a small C ABI (`rancho/rancho.h`); SQLite
   vendored; GraphBLAS **not** vendored — a minimal CSR of Rancho's own, written as semiring
   products so SuiteSparse can stand behind the same functions later. A Python binding waits on
   a Python host.
6. **Shared media policy.** *Taken in 0.1:* detect a network path (spelling, then the OS);
   open it single-host in rollback mode with an exclusive lock held until close; steer sharing
   to segments. Open: whether a host may override to WAL on a path it knows is local but the OS
   reports remote (some sync clients mount that way).
7. **The training export format.** *0.1:* NumPy `.npy` + a JSON manifest, first — readable
   with nothing but numpy. Lean unchanged for the second form: Lance, because stable-worldmodel
   (and so LeWorldModel's tooling) reads it; Parquet for compatibility.
8. **Embeddings.** Stored per encoder with Bicho's provenance stamp; which vector index (HNSW,
   IVF-PQ) and whether it lives in SQLite or beside it — undecided.
9. **Undo across a reload.** Changing the working set rebuilds Core and loses memento undo.
   Waits on Core's reified commands; until then, the working-set budget should prefer not to
   evict what the person is editing.
10. **New, from building: clusters as tags.** Hormiga's original proposal wrote each rune's path
    as `c1:`/`c2:` tags; the store keeps it as derived data instead. Writing tags would make the
    clusters visible to every tag query and to Core — and would be data, so it would have to go
    through the dispatcher as commands, and change whenever clustering is re-run. Lean: keep
    derived; offer a command generator for a database that wants them.
11. **New, from building: who may read the store's connection.** `Store::handle()` gives
    Rancho's read-side modules (matrix view, segments) the SQLite connection, documented
    read-only. A write through it would be a second door. Lean: keep it, and keep it out of the
    C ABI; a lint like Hormiga's `lint_host_seams.py` if a second module starts using it.
