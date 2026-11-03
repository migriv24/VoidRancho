---
okf_version: "0.1"
---

# Void Rancho — Knowledge Bundle (dev)

An Open Knowledge Format bundle describing **Void Rancho**: the database layer for Void Core
applications — how runes and mantles are **stored**, **indexed**, **queried**, **paged**,
**shared** and **read as matrices**, at any size and on any medium. A library, not an
application.

This is the **dev** bundle. **0.1.0 is built** (2026-10-30): rungs 1–5 and the first form of
rung 6, 8 test suites green. The honesty convention is inherited from the Void Core bundle — a
concept may claim `status:current` only with a `resource:` link to the code that backs it, and
every current page says what of it is still planned.

**Origin.** Founded 2026-10-30 by the author's ruling: the database is a **pillar** of the
Void family (`VoidCore:/design/the-transition.md`, pillar B), it belongs **one layer above
Core** so Core stays pure, and it must be ready for JEPA-class world models (pillar A,
`VoidCore:/design/world-models.md`). Rung 1 is Void Hormiga's store, extracted.

## Read this first

1. **[Boundaries](/design/boundaries.md)** — what Rancho owns, and the five neighbours whose
   jobs it must not take. Railguard: *could every row be rebuilt by replaying commands?*
2. **[Building 0.1](/design/building-0.1.md)** — the leans taken to build, the seams chosen,
   what the building found, and what was measured.
3. **[The matrix view](/concepts/matrix-view.md)** — what a database looks like to a JEPA
   model: a mantle as sparse matrices.

# Concepts — the vocabulary

* [Store](/concepts/store.md) - the database at rest: one row per rune, edge, tag; weights, ports, provenance, the row index and typed fields derived from what Core wrote; Hormiga's stores migrated
* [Query](/concepts/query.md) - reading without loading: tag expressions answered exactly as `ls --tag`, decided by Core's evaluator; ranges; pages
* [Write path](/concepts/write-path.md) - every row explainable by a command; keeping the store in step with Core; the actor's provenance
* [Working set](/concepts/working-set.md) - what Core holds in memory, sized to the device, loaded by cluster
* [Cluster](/concepts/cluster.md) - the unit of paging, storage, analysis and export; weights read one relation at a time; settings recorded
* [Matrix view](/concepts/matrix-view.md) - a mantle as sparse matrices; graph algorithms as semiring products; the `.npy` training export
* [Tiers](/concepts/tiers.md) - the same database in memory, on a laptop, on a NAS and in object storage; one host at a time on a network path
* [Segment](/concepts/segment.md) - the shared form: content-named immutable files, a manifest per snapshot, one pointer

# Design — the rationale

* [Boundaries](/design/boundaries.md) - **read first**; Core, Palabra, Bicho, Maiz, Hormiga's Antfarm
* [Founding](/design/founding.md) - the ruling, and the family's existing databases
* [Building 0.1](/design/building-0.1.md) - leans taken, seams chosen, four findings, measurements
* [Storage at scale — research](/design/storage-research.md) - SQLite and network drives, object storage and table formats, columnar and vector formats, graph engines, sparse linear algebra; what each means for Rancho
* [Open questions](/design/open-questions.md) - what is genuinely undecided, and which leans 0.1 took provisionally

# Planned work

* [Roadmap](/roadmap.md) - the rungs, what remains of each, what comes next
* [Log](/log.md) - what has changed in this bundle

# The code, in one line each

`include/rancho/` — `store.hpp` (the store, queries, clusters), `working_set.hpp` (write-back),
`plan.hpp` (the budget), `core.hpp` (the CoreLink seam), `matrix.hpp` + `linalg.hpp` (the
matrix view and its algebra), `segments.hpp` (snapshots), `media.hpp` (network paths),
`json_stream.hpp` (reading any size), `rancho.h` (the C ABI). Tests in `tests/`, one promise per
function.

# Naming

Concept names here are **descriptive and provisional**, following Void Palabra's and Void
Bicho's precedent. A ranch-flavoured set is offered in [open questions](/design/open-questions.md)
§1 and is the author's to choose.

# Sibling bundles

Void Core's ontology (rune, mantle, glyph, holiday, dispatcher, journal, quantity) is defined
authoritatively at `..\VoidCore\okf`. Cross-bundle references are written
`VoidCore:/concepts/mantle.md` (plain-text convention — OKF links are bundle-root-relative).
**Rancho owns none of Core's vocabulary.**
