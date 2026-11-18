---
type: Roadmap
title: Roadmap
description: The rungs of Void Rancho, cheapest-useful-first. Rungs 1–5 and the first form of rung 6 are built (0.1.0, 2026-10-30), and the first form of rung 7, the deed, on 2026-11-06; what remains of each, and what comes next.
tags: [status:current, audience:dev, confidence:asserted, roadmap]
timestamp: 2026-11-09T00:00:00Z
---

**Important, by the author's ruling (2026-10-30):** Rancho is pillar B of Void Core's
transition (`VoidCore:/design/the-transition.md`). Development of the family begins with it
and with pillar A (world models).

# Rungs

0. **The OKF** — *done 2026-10-30.*
1. **The store, extracted from Hormiga** — *built 0.1.0.* Schema, streaming import, export back;
   weights/ports/provenance/ρ/typed fields; format-1 migration. *Tests held:* the round-trip
   law; query parity with `ls --tag` (3 000 random expressions); 4× the runes at the same
   memory. [Store](/concepts/store.md), [query](/concepts/query.md).
2. **The write path** — *built 0.1.0.* Write-back after each dispatch, through any CoreLink;
   the actor's name on new tags. *Test held:* Hormiga's model-based test, both paths.
   *Remaining:* exactness for every verb — **waits on Core's `touched`**.
   [Write path](/concepts/write-path.md).
3. **The working set and clusters** — *built 0.1.0.* Budget, plan, cluster paging; Leiden with
   recorded settings and weights read one relation at a time. *Remaining:* the test the roadmap
   named — Hormiga's 5×10⁵-rune stress database opening and taking edits under a 2 GB cap — is
   Hormiga's to run once it links Rancho (its `tools/lowmem` harness).
   [Working set](/concepts/working-set.md), [cluster](/concepts/cluster.md).
4. **The matrix view** — *built 0.1.0.* ρ, per-relation CSR `A_r`, V with units, T with
   sources, X by level; BFS, shortest paths, PageRank, components, A·X over semirings. *Tests
   held:* each algorithm against a textbook implementation. *Remaining:* the tag graph S, a
   port-pair split, a linear readout recovering typed fields (the second half of the named
   test). [Matrix view](/concepts/matrix-view.md).
5. **Segments and tiers** — *built 0.1.0 for tiers 1–2.* Snapshots as content-named segments
   + manifests + an atomic root; the network-path open policy. *Remaining:* the bucket adapter
   (tier 3); garbage collection of unnamed segments. [Tiers](/concepts/tiers.md),
   [segment](/concepts/segment.md).
6. **The training export** and embeddings — *first form built:* `.npy` arrays + manifest.
   *Remaining:* Lance; embeddings storage with Bicho's provenance; transitions on `touched`.
7. **One database, many files: the deed** (the author, 2026-11-06) — *first form built.* The
   [language](/concepts/language.md) the family designs databases in. The `.rancho` file: an
   identity and parts (store, segments, blobs), split by mantle. `rancho::Database` reads across
   parts; a missing part is a region not loaded; pins; the `rancho` tool. *Tests held:* the
   placement law, and fourteen more. *Since the author's answers (2026-11-06, two rounds):* mantles are
   named by id; a database declares no kind, and a host brings its **expectations**; a deed holds
   Rancho's members only and a newer one is never rewritten; a store can carry its own deed
   (the first hybrid). *Remaining:* writing to more than one part,
   moving a mantle between parts (the identity mapping, proved), splitting within a mantle,
   shared parts, overlays, generated parts, remote parts, derived parts, a bale form, the deed in
   the C ABI.
   [Deed](/concepts/deed.md), [one database, many files](/design/one-database-many-files.md).
8. **Between databases: mappings, dataspaces and federations** (the author, 2026-11-06).
   Planned:
   - a **mapping** that translates a mantle's commands from one database's vocabulary into
     another's, checked before it runs, riding on a holiday;
   - a **dataspace**, the live session in which several databases are worked on together, and
     its **federation**, the database that records its mappings and history, for Palabra to track
     edits across them;
   - `disband`, which may return the history as a small database;
   - the shape of a reference to another database's rune.

   **The algebra comes first** (the author, 2026-11-06: *"having actual algebra would greatly
   benefit us"*): schemas, mappings, Δ and Σ with their laws, the round-trip loss, pushouts,
   provenance and the proposal loop, in the order of [the algebra](/design/the-algebra.md) §11.

   [Beyond one database](/design/beyond-one-database.md).

# Next, in order

1. **Void Hormiga's and Void Squid's asks**, the family's two clients. **Built 2026-11-09:** upper
   cluster levels (`fanout`), the reseña kept by write-back, `clusters_of`, `plan` around a
   leaf, `find_names`, `matrix_view` costing its leaf, the fringe with its mojones and la cuenta,
   the rodeo and the embarque, `pack_into`, a foreign file refused untouched, and overlays
   (first form) with another database's part named by id. **Still to do:**
   - the reseña's ranges and counts by glyph and tag; sketches;
   - the covering closure (a map shape covering a rune, from a spatial index);
   - passes for recursive questions, kept between passes (dynamic-frontier Leiden);
   - the two tests of "the frontier", the interwoven and the endless database: Hormiga writes
     their generators into its `stressgen`, and both run under its 512 MB harness;
   - overlays routed from a working set (item 2), of edges, and of a removal.
2. **The deed's next form**: the identity-mapping proof, then moving a mantle between parts,
   then writing to more than one part ([open questions](/design/open-questions.md) §12–13).
   Then offering Hormiga and Squid a deed for the stores they already have. A store can now
   carry its own (`embed_deed`), so neither needs a file beside its store.
3. **The algebra, then mappings, then dataspaces** ([the algebra](/design/the-algebra.md) §11;
   [beyond one database](/design/beyond-one-database.md) §7):
   - the schema of a database, the mapping as a schema morphism, identity, composition and the
     check, Δ, then Σ with the round-trip loss, each step with its laws as tests;
   - a mapping's data shape, and a mantle written as the commands that would make it;
   - a message to Void Palabra about a federation's history (drafted locally);
   - the dataspace and its federation, then `disband` with the optional history;
   - nothing for Void Core: the author ruled that database things are Rancho's (§24).
4. **Core's `touched`** (world models phase 1). This is Rancho's one standing ask of Core.
5. **The Palabra line** ([open questions](/design/open-questions.md) §2): a message to Palabra.
   It now includes the deed, which sits beside Palabra's archive container.
6. **Phones** (the author, 2026-11-02, relayed by Void Maiz). **Built 2026-11-09:** the refusal to
   build Core's shared library on a phone with no parent `voidcore`, `OpenOptions::temp_dir`,
   `release_memory`, `phys_footprint`, where each file goes ([tiers](/concepts/tiers.md)), and a
   CI with Android and iOS compiles. **Still to do:** run that CI (the phone jobs do not gate
   until they have been green once), and run the tests on a device.
7. **Bicho's first world-model experiment** reads the matrix-view export. A corpus can be a
   deed of pinned parts.
8. Void Loops Studio. (Void Squid adopted Rancho on 2026-11-09.)

# First clients

Void Hormiga (rung 1 is its store), Void Squid (`assets.db`), Void Loops Studio.
