---
type: Roadmap
title: Roadmap
description: The rungs of Void Rancho, cheapest-useful-first. Nothing is built.
tags: [status:current, audience:dev, confidence:asserted, roadmap]
timestamp: 2026-10-30T00:00:00Z
---

**Important, by the author's ruling (2026-10-30):** Rancho is pillar B of Void Core's
transition (`VoidCore:/design/the-transition.md`). Development of the family begins with it
and with pillar A (world models).

# Rungs

0. **The OKF** — this bundle. *Done 2026-10-30.* Next: agree the Palabra line
   ([open questions](/design/open-questions.md) §2) and the graph-analytics home (§3).
1. **The store, extracted from Hormiga.** Schema, streaming import of a state document, export
   back. *Test:* the round-trip law; query parity with Core's `ls --tag`; importing 4× the
   runes does not grow peak memory more than a constant.
2. **The write path.** Write-back after each dispatch. *Test:* Hormiga's model-based test
   against a Core holding everything. *Depends on Core:* `touched` on the journal (Core's world
   models phase 1) makes this exact instead of re-asking Core rune by rune.
3. **The working set and clusters.** Budget, cluster paging, Leiden with recorded settings.
   *Test:* Hormiga's stress database opens and takes edits under a 2 GB memory cap.
4. **The matrix view.** Row index, per-relation CSR adjacency, values, features, tags.
   *Test:* PageRank and BFS agree with a reference implementation; a linear readout recovers
   typed fields.
5. **Segments and tiers.** Cluster segments + manifest; a NAS path; an S3-compatible bucket.
6. **The training export** and embeddings storage — the bridge to Void Bicho.

# First clients

Void Hormiga (rung 1 is its store), Void Squid (`assets.db`), Void Loops Studio.
