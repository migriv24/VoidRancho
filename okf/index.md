---
okf_version: "0.1"
---

# Void Rancho — Knowledge Bundle (dev)

An Open Knowledge Format bundle describing **Void Rancho**: the database layer for Void Core
applications — how runes and mantles are **stored**, **indexed**, **queried**, **paged**,
**shared** and **read as matrices**, at any size and on any medium. A library, not an
application.

This is the **dev** bundle. **0.1.0 is built** (2026-10-30): rungs 1–5 and the first form of
rung 6, 8 test suites green; a ninth since 2026-11-02, when graph analysis moved here; a tenth since 2026-11-06, the **deed**: one database, many files, and the language the family designs databases in; an eleventh since 2026-11-09, the **rodeo** and the **embarque**. Void Squid is the second client since 2026-11-09. The honesty convention is inherited from the Void Core bundle — a
concept may claim `status:current` only with a `resource:` link to the code that backs it, and
every current page says what of it is still planned.

**Origin.** Founded 2026-10-30 by the author's ruling: the database is a **pillar** of the
Void family (`VoidCore:/design/the-transition.md`, pillar B), it belongs **one layer above
Core** so Core stays pure, and it must be ready for JEPA-class world models (pillar A,
`VoidCore:/design/world-models.md`). Rung 1 is Void Hormiga's store, extracted.

## Read this first

0. **[The language](/concepts/language.md)** — how a database is made: the words, and the laws
   they obey. Rancho's first job, by the author's ruling (2026-11-06): *"primarily ... shared
   language on how databases are made. And then secondly have the tools to make them."*
1. **[Boundaries](/design/boundaries.md)** — what Rancho owns, and the five neighbours whose
   jobs it must not take. Railguard: *could every row be rebuilt by replaying commands?*
2. **[Building 0.1](/design/building-0.1.md)** — the leans taken to build, the seams chosen,
   what the building found, and what was measured.
3. **[The matrix view](/concepts/matrix-view.md)** — what a database looks like to a JEPA
   model: a mantle as sparse matrices.

# Concepts — the vocabulary

* [The language](/concepts/language.md) - **read first**: identity, record, derived data, placement, residency; the whole and its parts; what is in hand; how sure an answer is; passes and exports; the laws. Descriptive and ranch names side by side
* [Authority](/concepts/authority.md) - which file is in charge (2026-11-18, the author's ruling): the file a person opens, moves and shares is the database; a host's working copy is a cache in a per-user folder, one per authority file, with a note saying how they stand; keepsakes per user; pointers stay where they are. `rancho::working_copy_for`, `cache_state`
* [Deed](/concepts/deed.md) - the `.rancho` file: who a database is and where its bytes live; parts split by mantle, named by id; a missing part is a region not loaded; pins; a deed holds Rancho's members only and a database declares no kind (a host's expectations); a store that carries its own deed; `rancho::Database`; the `rancho` tool
* [Store](/concepts/store.md) - the database at rest: one row per rune, edge, tag; weights, ports, provenance, the row index and typed fields derived from what Core wrote; Hormiga's stores migrated
* [Query](/concepts/query.md) - reading without loading: tag expressions answered exactly as `ls --tag`, decided by Core's evaluator; ranges; pages
* [Write path](/concepts/write-path.md) - every row explainable by a command; keeping the store in step with Core; the actor's provenance
* [Working set](/concepts/working-set.md) - what Core holds in memory, sized to the device, loaded by cluster
* [Cluster](/concepts/cluster.md) - the unit of paging, storage, analysis and export; weights read one relation at a time; settings recorded
* [Matrix view](/concepts/matrix-view.md) - a mantle as sparse matrices; graph algorithms as semiring products; the `.npy` training export
* [Tiers](/concepts/tiers.md) - the same database in memory, on a laptop, on a NAS and in object storage; one host at a time on a network path
* [Segment](/concepts/segment.md) - the shared form: content-named immutable files, a manifest per snapshot, one pointer
* [Rodeo and embarque](/concepts/rodeo.md) - a pass over the whole potrero in bounded memory, resumable; an export as one thing in many content-named partidas

# Design — the rationale

* [Boundaries](/design/boundaries.md) - **read first**; Core, Palabra, Bicho, Maiz, Hormiga's Antfarm
* [Founding](/design/founding.md) - the ruling, and the family's existing databases
* [Building 0.1](/design/building-0.1.md) - leans taken, seams chosen, four findings, measurements
* [Storage at scale — research](/design/storage-research.md) - SQLite and network drives, object storage and table formats, columnar and vector formats, graph engines, sparse linear algebra; what each means for Rancho
* [Beyond one database](/design/beyond-one-database.md) - the author's answers of 2026-11-06, in two rounds, researched: everything indexed by id, a copy minting new ones and no pointer rune; a mapping (a dictionary riding on a holiday) for any move between files; a dataspace, the live session over several databases, and its federation, the database that records it, which can be disbanded and hand back its history; file types built on the rancho while a database declares no kind; Rancho agnostic; Core kept small. Prior art from dataspaces, mediators, data integration, functorial data migration, lenses, Cambria, federated databases, sagas, ISOBMFF, UTI, OPC and PNG
* [The algebra](/design/the-algebra.md) - schemas, mappings and their operations, after CQL: a Core document's schema is a finite acyclic category and its ids are labelled nulls; compose, Δ, Σ, Π and their laws; the round trip that measures loss; pushouts; commands as deltas, translated exactly where a mapping is linear; the line with Palabra as a law (merging and mapping commute); provenance as a semiring; a schema across a holiday's door; AI as proposals the algebra checks; the build order
* [After the dataspace](/design/after-the-dataspace.md) - what has followed dataspaces, and what is stronger: collaborative data sharing with update exchange, provenance and trust (ORCHESTRA); an algebra of mappings and mappings with proofs (model management, CQL); one commit across many tables (lakehouse catalogs, Nessie); sovereignty that travels with the data (the European data spaces, Keyhive); incremental maintenance (DBSP); models that propose mappings. Leans in open questions §26–§29
* [One database, many files](/design/one-database-many-files.md) - the deed's rationale: five questions a database answers, why one becomes many files, the frontier at every scale, the cut by mantle, how Hormiga, Squid, Loops Studio, a corpus and a test would use it, and what comes after
* [Open questions](/design/open-questions.md) - what is genuinely undecided, and which leans 0.1 took provisionally

# Planned work

* [Roadmap](/roadmap.md) - the rungs, what remains of each, what comes next
* [Log](/log.md) - what has changed in this bundle

# The code, in one line each

`include/rancho/` — `store.hpp` (the store, queries, clusters), `working_set.hpp` (write-back),
`plan.hpp` (the budget), `core.hpp` (the CoreLink seam), `graph.hpp` (Leiden, its hierarchy,
PageRank, components, NMI: the leaf target `voidrancho_graph`, from Void Maiz since 2026-11-02),
`matrix.hpp` + `linalg.hpp` (the matrix view and its algebra), `segments.hpp` (snapshots), `media.hpp` (network paths),
`json_stream.hpp` (reading any size), `deed.hpp` (the `.rancho` file and a database of many parts),
`overlay.hpp` (a person's edits over a part they do not own), `rodeo.hpp` (a pass over everything, and
the embarque), `rancho.h` (the C ABI); `tools/rancho.cpp`, the `rancho` tool. Tests in `tests/`, one promise per
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
