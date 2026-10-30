---
type: Design
title: Open questions
description: What is genuinely undecided about Void Rancho, each with a lean, for the author.
tags: [status:planned, audience:dev, confidence:exploratory]
timestamp: 2026-10-30T00:00:00Z
---

# Open questions

1. **Names.** Descriptive names are used throughout, per Palabra's and Bicho's precedent. A
   ranch set, offered: the **corral** (the working set — the herd penned close at hand), the
   **pasture** (the cold tiers), a **herd** (a cluster), the **brand** (a row's stable index —
   but Core's `spirit` already names identity, and Rancho must not rename Core's words). The
   author chooses; nothing is renamed until then.
2. **The line with Palabra's persistence.** [Boundaries](/design/boundaries.md) §4 proposes
   Palabra = at rest as a whole, Rancho = at rest as a live indexed database. Lean: agree it
   with Palabra by message before any code.
3. **Where graph analytics lives** — Rancho, Void Maiz, or Core. Lean: Rancho, which holds the
   whole graph; Maiz keeps what it needs to draw. Needs Maiz's agreement and the author's.
4. **The system of record at rest.** Hormiga already says *"the store is the baseline"*: at
   rest the store, not a state document, is the database. Lean: yes — Core's state document
   becomes the working set's form, and a full document is an export.
5. **Language.** Lean: C++20 behind a C ABI, like Palabra, Bicho's planned native path and
   Maiz; SQLite vendored as the amalgamation; GraphBLAS vendored (Apache-2.0) with a minimal
   CSR fallback for small devices; a Python binding later. Never linked by Core.
6. **Shared media policy.** Refuse to open a store in WAL mode on a network path, or refuse a
   second host outright? Lean: detect the network path, open single-host in rollback mode, and
   steer sharing to segments ([tiers](/concepts/tiers.md)).
7. **The training export format.** Lean: Lance first, because stable-worldmodel (and so
   LeWorldModel's tooling) reads it; Parquet as the compatibility format.
8. **Embeddings.** Stored per encoder with Bicho's provenance stamp; which vector index
   (HNSW, IVF-PQ) and whether it lives in SQLite (an extension) or beside it — undecided.
9. **Undo across a reload.** Changing the working set rebuilds Core and loses memento undo.
   Waits on Core's reified commands; until then, the working-set budget should prefer not to
   evict what the person is editing.
