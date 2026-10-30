---
type: Concept
title: Cluster
description: A connected group of runes of bounded size, found from the graph itself (Leiden, recursively) — the one unit Rancho pages, stores as a segment, analyses, exports for training, and could sync.
tags: [status:planned, audience:dev, audience:library, confidence:asserted]
timestamp: 2026-10-30T00:00:00Z
---

A **cluster** is a connected set of runes, small enough for a weak device, found from the
mantle's own edges. Void Hormiga's proposal (2026-10-25) and Void Maiz's implementation: run
**Leiden** (Traag, Waltman & van Eck 2019 — unlike Louvain, every community it returns is
connected) recursively inside each community until each is under a target size, and write
each rune's path as tags (`c1:12`, `c2:12.4`).

# One unit, five jobs

| job | why the cluster |
|---|---|
| paging the [working set](/concepts/working-set.md) | an edit costs O(cluster), not O(database) |
| a segment on disk or in a bucket ([tiers](/concepts/tiers.md)) | immutable, content-named, written once |
| analysis | most edges are inside it, so local algorithms run on one block of the matrix |
| a training example ([matrix view](/concepts/matrix-view.md)) | a bounded, connected subgraph is what a model batch holds |
| sync (to discuss with Palabra) | a peer asks for the clusters it shows, not the database |

# Weights matter here

Leiden optimizes modularity over **edge weights**. Which relations' weights count — and the
rule that weights of different relations are not commensurable
(`VoidCore:/concepts/quantity.md`) — is a choice per database, recorded beside the cluster tags
so a re-run is reproducible (seed, relations, target size). This is the author's "clustering
and edge weights as floats" made a first-class setting rather than a constant.

# Status

`planned`. The implementation exists in Void Maiz (`voidmaiz/graph.hpp`), measured on Hormiga's
stress database; where it should live is [boundaries](/design/boundaries.md) §3.
