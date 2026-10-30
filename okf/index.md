---
okf_version: "0.1"
---

# Void Rancho — Knowledge Bundle (dev)

An Open Knowledge Format bundle describing **Void Rancho**: the database layer for Void Core
applications — how runes and mantles are **stored**, **indexed**, **queried**, **paged** and
**read as matrices**, at any size and on any medium. A library, not an application.

This is the **dev** bundle. **Nothing is built.** Every concept is `status:planned`, per the
honesty convention inherited from the Void Core bundle — a concept may not claim
`status:current` without a `resource:` link to the code that backs it.

**Origin.** Founded 2026-10-30 by the author's ruling: the database is a **pillar** of the
Void family (`VoidCore:/design/the-transition.md`, pillar B), it belongs **one layer above
Core** so Core stays pure, and it must be ready for JEPA-class world models (pillar A,
`VoidCore:/design/world-models.md`).

## Read this first

1. **[Boundaries](/design/boundaries.md)** — what Rancho owns, and the five neighbours whose
   jobs it must not take.
2. **[Founding](/design/founding.md)** — the author's direction, and what every application in
   the family already built on its own.
3. **[The matrix view](/concepts/matrix-view.md)** — what a database looks like to a JEPA
   model: a mantle as sparse matrices.

# Concepts — the vocabulary

* [Store](/concepts/store.md) - the database at rest: one row per rune, edge, tag; typed columns derived from glyph declarations
* [Write path](/concepts/write-path.md) - every row explainable by a command; keeping the store in step with Core
* [Working set](/concepts/working-set.md) - what Core holds in memory, sized to the device, loaded by cluster
* [Cluster](/concepts/cluster.md) - the unit of paging, storage, sync and analysis
* [Matrix view](/concepts/matrix-view.md) - a mantle as sparse matrices: adjacency per relation, features from quantities, embeddings beside them
* [Tiers](/concepts/tiers.md) - the same database on a laptop, a NAS and object storage

# Design — the rationale

* [Boundaries](/design/boundaries.md) - **read first**; Core, Palabra, Bicho, Maiz, Hormiga's Antfarm
* [Founding](/design/founding.md) - the ruling, and the family's existing databases
* [Storage at scale — research](/design/storage-research.md) - SQLite and network drives, object storage and table formats, columnar and vector formats, graph engines, sparse linear algebra; what each means for Rancho
* [Open questions](/design/open-questions.md) - what is genuinely undecided

# Planned work

* [Roadmap](/roadmap.md) - the rungs, cheapest-useful-first
* [Log](/log.md) - what has changed in this bundle

# Naming

Concept names here are **descriptive and provisional**, following Void Palabra's and Void
Bicho's precedent. A ranch-flavoured set is offered in [open questions](/design/open-questions.md)
§1 and is the author's to choose.

# Sibling bundles

Void Core's ontology (rune, mantle, glyph, holiday, dispatcher, journal, quantity) is defined
authoritatively at `../VoidCore/okf`. Cross-bundle references
are written `VoidCore:/concepts/mantle.md` (plain-text convention — OKF links are
bundle-root-relative). **Rancho owns none of Core's vocabulary.**
