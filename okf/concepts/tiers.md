---
type: Concept
title: Tiers
description: The same database in memory, on a local disk, on a NAS or network drive, and in object storage — one live single-host store, and immutable cluster segments with a manifest for everything shared.
tags: [status:planned, audience:dev, audience:library, confidence:exploratory]
timestamp: 2026-10-30T00:00:00Z
---

| tier | medium | form | who writes |
|---|---|---|---|
| 0 | memory | the [working set](/concepts/working-set.md) in a Core manager | the dispatcher |
| 1 | local disk | the live [store](/concepts/store.md) (SQLite, WAL) | one host |
| 2 | NAS / network drive | **segments + manifest**, or a store opened by **one host only** and never in WAL mode | one writer at a time; readers need no lock |
| 3 | object storage (S3, R2, MinIO) | segments + manifest; commit by create-if-absent or compare-and-swap on the root | one committer per root; the store serializes it |

# Why two forms

SQLite is the best tool for a live database on one machine and the wrong one for a file two
machines write: WAL needs shared memory on one host, and network filesystem locks can corrupt
a database ([storage research](/design/storage-research.md) §1). So anything **shared or
archived** is written as **immutable segments** — one per [cluster](/concepts/cluster.md),
named by its canonical hash — plus a **manifest** per snapshot, the Iceberg pattern (§2 there).
Readers never lock; a commit is one pointer swap; identical clusters are stored once; history
is the chain of manifests.

# What tiers are not

**Not sync.** Two people editing one database on two machines is Void Palabra's merge over
Reticulum, arriving as commands. A shared drive holding segments lets a second machine *open*
a database fast; it does not let two machines *write* one file.

# Status

`planned`; `confidence:exploratory` — the segment format is unwritten.
