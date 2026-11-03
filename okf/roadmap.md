---
type: Roadmap
title: Roadmap
description: The rungs of Void Rancho, cheapest-useful-first. Rungs 1–5 and the first form of rung 6 are built (0.1.0, 2026-10-30); what remains of each, and what comes next.
tags: [status:current, audience:dev, confidence:asserted, roadmap]
timestamp: 2026-11-02T00:00:00Z
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

# Next, in order

1. **Hormiga adopted Rancho** (2026-11-02): its library builds on ours, the 2 GB exit test
   passed. Its app's own move to a store (Hormiga's stage 5) is next, and the NAS question
   (UNC paths or mapped drives) waits on the author.
2. **Core's `touched`** (world models phase 1) — Rancho's one standing ask of Core.
3. **The Palabra line** ([open questions](/design/open-questions.md) §2) — a message to
   Palabra. (The graph analytics home, §3, was ruled and moved on 2026-11-02.)
4. **Phones** (the author, 2026-11-02, relayed by Void Maiz): Android compiles and links
   unchanged; still to do are a refusal to build Core's shared library on a phone with no
   parent `voidcore`, SQLite's temp directory, which files go in `data_dir` and which in
   `cache_dir`, the call that makes a working set durable on backgrounding, and an iOS and
   Android compile in CI.
5. **Bicho's first world-model experiment** reads the matrix-view export.
6. Void Squid's `assets.db`, then Void Loops Studio.

# First clients

Void Hormiga (rung 1 is its store), Void Squid (`assets.db`), Void Loops Studio.
