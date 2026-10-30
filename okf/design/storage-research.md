---
type: Design
title: Storage at scale — research, and what it means for Rancho
description: "Research of 2026-10-30. SQLite and why it must not be shared across hosts on a network drive; object storage (S3 is a flat key space — the graph lives in the metadata) and table formats (Iceberg's catalog → manifests → files, commits by atomic pointer swap, now possible on S3 itself with conditional writes); columnar and vector formats (Parquet, Lance); embedded engines (DuckDB, the Kuzu archival and its forks, our own MeshDB holiday); replication (Litestream); sparse linear algebra (GraphBLAS). Each with its consequence for Rancho."
tags: [status:planned, audience:dev, confidence:asserted]
timestamp: 2026-10-30T00:00:00Z
---

# Storage at scale — research

Read 2026-10-30 from primary documentation where noted and from secondary sources otherwise;
`confidence:asserted` until each claim is checked against the artifact before it is built on.

---

## 1. SQLite — right for one host, wrong for a shared drive

Hormiga chose SQLite (2026-10-27) and was right to: one file, no server, a bounded page cache
(reading a 20 GB store costs the memory of reading a small one), public domain, already in
the family.

**The constraint that decides the NAS question** — SQLite's own documentation:

- *WAL mode*: "All processes using a database must be on the same host computer; WAL does not
  work over a network filesystem," because the WAL index is shared memory
  (sqlite.org/wal.html).
- *Any mode*: SQLite depends on the filesystem's locks, and network filesystems — NFS in
  particular — have locking bugs that can corrupt a database when two processes write
  (sqlite.org/howtocorrupt.html). `PRAGMA journal_mode=WAL` can even report success on a
  filesystem that cannot support it.
- SQLite's advice when data is separated from the application by a network: use a
  client/server database.

**Consequence.** A Rancho store is **opened by one host at a time**. A NAS or network drive is
a place where a store *lives* or is *published*, not a place several machines write one file.
Two machines sharing data is Palabra's merge over Reticulum, not two SQLite connections to one
file. Rancho must **detect** a network path and refuse WAL there (or refuse a second opener),
rather than trust the PRAGMA.

## 2. Object storage and table formats — where "everything is a graph" is true

The author's intuition: *"our structure is similar to AWS buckets and stuff (where everything
is on a graph)."* The precise version:

- **S3 itself is not a graph.** It is a flat map from key to bytes; "folders" are key prefixes.
  Since December 2020 it is strongly consistent for read-after-write; since **August 2024** a
  write can be **create-if-absent** (`If-None-Match: *`) and since **November 2024**
  **compare-and-swap** on an ETag (`If-Match`), failing with HTTP 412 (AWS announcements and
  docs).
- **The graph lives in the metadata above it.** Apache Iceberg (iceberg.apache.org/spec): a
  *catalog* points at the current *metadata file*; it lists *snapshots*; a snapshot's
  *manifest list* names *manifests*; each manifest names immutable *data files* with column
  min/max statistics so readers skip files without opening them. Every write produces a new
  metadata file and **atomically swaps one pointer**; readers see the old snapshot or the new
  one, never half. Manifests are reused across snapshots. That is a Merkle-DAG of immutable
  pieces with one mutable root — **Void Palabra's history graph and content-addressed blocks
  have the same shape**, and a Core mantle (runes as files, `layout.edges` as structure) is
  the data inside it.
- With conditional writes the pointer swap no longer needs a separate coordination service;
  S3 itself serializes it.

**Consequence.** Rancho's at-scale layout is **immutable segments plus a manifest per
snapshot**:
- a **segment** is a [cluster](/concepts/cluster.md) of runes with their edges, written once;
- a **manifest** lists the live segments with per-segment statistics (tag counts, id ranges,
  quantity min/max);
- a **commit** writes new segments and a new manifest, then swaps the root pointer —
  create-if-absent on object storage, rename-into-place on a single-writer local or NAS
  directory.

This works the same on a laptop, a NAS share and a bucket, needs no locks for readers, gives
history for free, and names segments by Palabra's canonical hash so identical clusters are
stored once. The live SQLite store (§1) is the **hot, single-host** form; segments are the
**cold, shared** form. See [tiers](/concepts/tiers.md).

## 3. Columnar and vector formats — for analytics and training

- **Parquet** — the lakehouse default; columnar, compressed, stats per row group. Fine for
  scans; random access reads a whole row group.
- **Lance** (github.com/lance-format/lance, Rust, Apache-2.0) — a columnar format, table format
  and catalog spec for multimodal ML: a row-offset index for single-seek random access, native
  fixed-size vector columns, IVF/IVF-PQ/HNSW indexes, versioning, and adding columns without
  rewriting. Most speed claims ("100× Parquet for random access") are LanceDB's own; their
  engineering blog concedes Parquet can do random access and the gap is row groups and memory.
  Notably, **stable-worldmodel** — the platform LeWorldModel trains through — stores its
  datasets as Lance by default.

**Consequence.** Rancho's **export for learning** — the [matrix view](/concepts/matrix-view.md)
materialized per cluster, plus embeddings — targets a columnar file; Lance is the first
candidate because the world-model tooling already reads it. Not the primary store.

## 4. Engines considered

| engine | what it is | for Rancho |
|---|---|---|
| **SQLite** | embedded row store | **rung 1** — Hormiga's proven schema; single host |
| **DuckDB** | embedded analytical (columnar) engine; reads and writes SQLite files and Parquet directly (`ATTACH … (TYPE SQLITE)`) | an **analytics** companion over the same store and over segments; its graph extension (DuckPGQ, SQL/PGQ) is still experimental and pinned to an older DuckDB |
| **Kùzu** | embedded graph DB (Cypher, vectors) | **do not depend on it.** Upstream archived 2025-10-10 (v0.11.3) after an acquisition; forks: LadybugDB (the active successor, FreeBSD moved to it 2026-05), Vela's fork (multi-writer), Bighorn and RyuGraph (stalled). Watch LadybugDB, adopt nothing yet |
| **MeshDB** (our holiday) | Rust graph server over Bolt/Cypher | proved tag-grammar parity 7/7 in June; a client/server option where a shared multi-writer server is wanted; heavy for a phone |
| **RocksDB** | embedded LSM key-value store | an option for an index layer if SQLite's B-trees become the bottleneck; not first |
| **Lance / LanceDB** | columnar + vector | export and vector search (§3) |

## 5. Graph algorithms as sparse linear algebra

**GraphBLAS** (graphblas.org; SuiteSparse:GraphBLAS by Tim Davis, ACM TOMS Algorithms 1000 and
1037; Apache-2.0) expresses graph algorithms as sparse matrix operations over semirings: BFS on
(∨,∧), shortest paths on (min,+), PageRank and triangle counting on (+,×), in CSR/CSC and
hypersparse formats, parallelized with OpenMP. The graph database FalkorDB is built on it.

**Consequence.** This is how *"a lot of matrix math for how to deal with runes and mantles"*
is done without inventing it: the [matrix view](/concepts/matrix-view.md)'s `A_r` are exactly
GraphBLAS matrices. Vendoring SuiteSparse:GraphBLAS (C, Apache-2.0) is the default proposal;
a minimal CSR of our own is the fallback for small devices. Leiden stays combinatorial (Void
Maiz's implementation).

## 6. Replication — what not to build

**Litestream** (v0.5, October 2025) ships SQLite's changed pages to object storage every second
and restores to any point in time; read replicas through a VFS. It is a backup of **one**
writer, not a merge of two. **LiteFS** is in maintenance mode.

**Consequence.** Rancho does not build replication. A backup of a store is a segment snapshot
(§2) or a Litestream-style page stream; **merging two people's edits is Palabra's**, by its
CRDT, and arrives as commands.

## 7. Speed — where the time actually goes (from Hormiga's measurements)

At 5×10⁵ runes Hormiga measured one edit at ~2.4 s in Core's undo memento, ~1.6 s exporting
the document, ~7 s projecting the data mantle. Core 0.2.15 removed the per-command ref scans
(an import at 10⁴ runes went from 8.8 s to 0.07 s) and `validate`'s quadratic. What remains is
**proportional to what is held**, which is why the working set — not a faster export — is the
fix, and why Core's planned `touched` record matters: write-back today re-asks Core about each
rune because the journal records the action and not its effect.

## Sources

sqlite.org/wal.html · sqlite.org/howtocorrupt.html · iceberg.apache.org/spec ·
aws.amazon.com/about-aws/whats-new/2024/11/amazon-s3-functionality-conditional-writes ·
github.com/lance-format/lance · lancedb.com/blog/columnar-file-readers-in-depth-structural-encoding ·
duckdb.org/docs/stable/guides/sql_features/graph_queries ·
github.com/DrTimothyAldenDavis/GraphBLAS · dl.acm.org/doi/10.1145/3577195 ·
simonwillison.net/2025/Oct/3/litestream · gdotv.com/blog/yearly-edge-graph-technology-news-recap-2025 ·
github.com/galilai-group/stable-worldmodel
