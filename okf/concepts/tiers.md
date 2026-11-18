---
type: Concept
title: Tiers
description: The same database in memory, on a local disk, on a NAS or network drive, and in object storage — one live single-host store, and immutable content-named segments with a manifest for everything shared. Tiers 0–2 built; the bucket adapter for tier 3 planned.
resource: src/segments.cpp
tags: [status:current, audience:dev, audience:library, confidence:asserted]
timestamp: 2026-10-30T00:00:00Z
---

| tier | medium | form | who writes | built |
|---|---|---|---|---|
| 0 | memory | the [working set](/concepts/working-set.md) in a Core manager | the dispatcher | yes |
| 1 | local disk | the live [store](/concepts/store.md) (SQLite, WAL) | one host | yes |
| 2 | NAS / network drive | **[segments](/concepts/segment.md) + manifest**, or a store opened by **one host only**, never in WAL mode | one committer at a time; readers need no lock | yes |
| 3 | object storage (S3, R2, MinIO) | the same segments + manifest as keys; commit by create-if-absent on the manifest, compare-and-swap on the root | one committer per root | planned: the layout is ready, the adapter is not |

# Why two forms

SQLite is the best tool for a live database on one machine and the wrong one for a file two
machines write: WAL needs shared memory on one host, and network filesystem locks can corrupt
a database ([storage research](/design/storage-research.md) §1). So:

- **A store on a network path** is detected — by the path's spelling (UNC) and by asking the
  OS (`GetDriveTypeW` for mapped drives; `statfs` for NFS/SMB/CIFS/AFS/Ceph/Lustre) rather
  than trusting `PRAGMA journal_mode=WAL`, which can report success where it cannot work —
  and opened in rollback mode under an **exclusive lock held until close**. A second machine
  is refused at open with a sentence that says why. `Sharing::SingleHost` asks for the same
  anywhere (a USB stick moved between machines). `include/rancho/media.hpp`.
- **Anything shared or archived** is a [snapshot](/concepts/segment.md): immutable segments,
  one per cluster leaf, named by their SHA-256, plus a manifest per snapshot and one mutable
  pointer swapped by an atomic rename — the Iceberg pattern. Readers never lock; identical
  clusters are stored once; history is the chain of manifests.

A finding on the way, worth every host's attention: on MinGW, `std::filesystem::absolute`
turns `\\nas\share\x` into `C:\nas\share\x`. Code that normalizes a path before asking where
it lives will call every NAS path local — and open it in WAL. Rancho reads the spelling first
and resolves with `GetFullPathNameW` (`media_test`).

# One database on several tiers

Since 2026-11-06 a database's parts may sit on different tiers at once: a live store on this
disk, a pinned segments part on a NAS, and later a bucket. The [deed](/concepts/deed.md) says
which part is where. A part on a tier that cannot be reached right now is a region not loaded,
and the database still opens.

# What tiers are not

**Not sync.** Two people editing one database on two machines is Void Palabra's merge over
Reticulum, arriving as commands. A shared drive holding segments lets a second machine *open*
a database fast; it does not let two machines *write* one file.

# On a phone (2026-11-09, Void Maiz's asks)

- **Where each file goes.** A store, a deed and a blobs part are the record: `data_dir()`
  (private, backed up). A segments part's restored copy (`DatabaseOptions::cache_dir`), a
  `pack_into` copy made to be sent, and SQLite's spill files are derived or temporary:
  `cache_dir()`, which the system may empty.
- **SQLite's spill folder.** `OpenOptions::temp_dir` (and `DatabaseOptions::temp_dir`). An
  Android app process has none of SQLite's defaults outside Void Maiz's shell, which sets
  `TMPDIR`; iOS sets `TMPDIR` itself.
- **Backgrounding.** A working set is durable when `WorkingSet::sync` returns: every
  committed transaction survives an app kill under WAL with synchronous=NORMAL. So the one
  call on `AppEvent::Background` is `sync`, and it costs what the edits since the last sync
  touched (Hormiga measured 0.5 ms for an edit at 5×10⁵ runes).
- **Memory.** `release_memory()` on a low-memory event. On Apple, `memory::process_bytes` reads
  `phys_footprint`, the number jetsam acts on.
- **One static Core.** Core's CMake builds only a shared library. On Android or iOS, Rancho
  refuses to build it, and asks for a parent's static `voidcore` (Void Maiz defines one) until
  Core builds a static one itself.
- **App Group containers.** A store opened single-host holds `locking_mode=EXCLUSIVE`, and iOS
  ends a suspended app that holds a file lock in a container shared with an extension
  (`0xdead10cc`). Keep stores in the app's own container, or close them on backgrounding.
- **CI** (`.github/workflows/ci.yml`): Linux, macOS and Windows build and test; Android (NDK,
  arm64-v8a) and iOS (device) compile, through a superproject that adds Void Maiz first. The
  phone jobs do not gate until they have been green once.

# Status

`current` for tiers 0–2; tier 3 planned.
