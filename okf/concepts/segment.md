---
type: Concept
title: Segment
description: The shared form of a database — immutable files named by the SHA-256 of their bytes, one per cluster leaf, listed by a manifest per snapshot, committed by an exclusive manifest and an atomic pointer swap. Restores byte for byte, with provenance, clusters and rows.
resource: src/segments.cpp
tags: [status:current, audience:dev, audience:library, confidence:asserted]
timestamp: 2026-10-30T00:00:00Z
---

A **segment** is a piece of a database written once and never changed: the runes of one
[cluster](/concepts/cluster.md) leaf (or of one page, for a mantle not clustered yet), each with
its position, its tags' sources and its leaf, plus the edges they are the source of. Edges
whose source is no rune — Core lets a link dangle — get segments of their own. A snapshot of a
store (`rancho::snapshot`) is a directory:

```
<dir>/segments/<sha256>.json   the bytes; the name is their SHA-256 — Palabra's hash, so
                               identical content has one name across the family
<dir>/manifests/<n>.json       snapshot n: the document's other keys, each mantle's skeleton
                               and its segments in order, with per-segment statistics
                               (runes, edges, bytes, the ρ range, tag counts), the cluster
                               settings, and the parent snapshot
<dir>/ROOT                     the current snapshot's number — the one mutable thing
```

# The commit

1. write each segment not already there (to a temporary name, then renamed — a reader never
   sees half a file under its real name); a segment already there is checked byte for byte;
2. **create** `manifests/<n+1>.json` **exclusively** — a second committer racing for the same
   number fails here and replaces nothing;
3. swap `ROOT` by an atomic rename.

Readers read `ROOT`, then only files that never change: the old snapshot or the new one, never
half, with no lock. On a bucket, step 2 is create-if-absent and step 3 compare-and-swap, both
in S3 since 2024 — the adapter is planned ([tiers](/concepts/tiers.md)).

# Promises (`tests/segments_test.cpp`)

- **A snapshot restores to the same document, byte for byte** — and with what the document
  does not carry: tag sources, cluster leaves and their settings, the row index.
- **A small edit writes what changed**: one edited rune after a 12-segment snapshot wrote 1
  segment and reused 11. History is free; snapshot 1 restores after snapshot 2.
- **A commit that loses a race replaces nothing**: `ROOT` does not move.
- **A corrupt segment is refused** (its bytes no longer match its name), and the store being
  restored into stays as it was — one transaction.
- **Constant memory**: 6×10⁴ runes in 366 segments (26 MB) snapshot in 3.4 s and restore in
  3.4 s, holding one segment at a time (+0 MB / +11 MB).
- **SHA-256 is proven, not assumed**: the FIPS 180-4 vectors, block boundaries and the
  million-`a` message.

# Status

`current`. Not yet: the bucket adapter; garbage collection of segments no manifest names;
Palabra's canonical form for segment content (today the bytes are the store's own JSON, so
"identical" means identical as written).
