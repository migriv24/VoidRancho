---
type: Design
title: One database, many files — the .rancho file
description: "The author's ask of 2026-11-06: a base file, an abstract configuration for storing a database, thought through for a database split between many files and for every kind of software that will use one. What a database is (five separate questions), why one becomes many files, the clean cut Void Core gives (by mantle), how Hormiga, Squid, Loops Studio, a world-model corpus and a test would each use it, the ideas past the first form in order of how sure we are, and what the first form does not solve."
tags: [status:current, audience:dev, confidence:exploratory, foundation]
timestamp: 2026-11-06T00:00:00Z
---

# 1. The ask

The author, 2026-11-06:

> *"I essentially want to work on a BASE file, maybe I should call it like ".RANCHO" or
> something. This is an abstract configuration for storing a database. Obviously we need to
> REALLY think outside the box, like what if a single database gets split between multiple
> files? How might hormiga use it, vs void squid, vs other future software? It's super
> abstract, but it could be helpful in designing databases in the future."*

Until now, a database in the family was **one file**: Hormiga's store, Squid's `assets.db`.
A snapshot was a folder of segments, but it was a copy of that one file, not a part of the
database. This page separates *the database* from *its files*, and the
[deed](/concepts/deed.md) is the file that joins them again.

# 2. What a database is

A database answers five separate questions ([language](/concepts/language.md) §1):

1. **who it is** (identity);
2. **what it says** (the record);
3. **what has been worked out from it** (derived data);
4. **where its bytes live** (placement);
5. **what is in hand now** (residency).

A single file answers all five at once. So a copy of that file is a new database, and also
the same one; a cache is mixed with the record, and so is the backup; what a person had
loaded lives beside what they wrote. **The deed records only 1 and 4.** The record stays in the
parts, and is Core's vocabulary. Derived data is kept beside the record and can be rebuilt.
Residency belongs to one process and is never written down.

# 3. Why one database becomes many files

Each reason has been seen in the family or is plainly coming:

| reason | example |
|---|---|
| **size** | the premise since 2026-11-03 is an infinite database. No single file, disk or machine holds one |
| **media** | a live part on this laptop, last decade's records on a NAS, cold history in a bucket |
| **lifetime** | the record must be backed up. Derived data can be thrown away: on a phone that is `data_dir` against `cache_dir` (Void Maiz's question of 2026-11-02) |
| **ownership** | a reference set that many databases read, such as map shapes, a vocabulary or a game's extracted assets. Each database holding a copy is how they drift apart |
| **writers** | SQLite allows one writer per file and one host per network file. Several writers need several parts |
| **carrying** | some parts travel with a person and others stay home. A phone carries the part it edits |

# 4. The frontier at every scale

The most useful idea here was already Rancho's. Void Hormiga's *frontier* (2026-11-03) is the
line between what Core holds and the rest of the store: a working set acts honestly on a
part, and what it does not hold is unknown, never deleted.

**A database of many files has the same line, one scale up.** A part that is not reachable,
such as a USB stick not plugged in or a NAS that is down, is a region that is not loaded:
- the database still opens;
- it says which parts are absent and why;
- its document holds the mantles that are here;
- it is not complete, and nothing reads the absence as deletions.

The same words cover memory and disk, disk and disk, and this device and the network:
*corral, lindero, mojón, cuenta* ([language](/concepts/language.md) §3). That unification is
the main argument for the design. A database that is not all here is the normal case, at
every scale.

# 5. The clean cut: by mantle

Void Core links only rune to rune **within one mantle** (SPEC §3.7). What connects two mantles
is a binding, which is top-level and may dangle (§3.6). So a part that holds whole mantles
never cuts an edge, and a database split by mantle is still a set of complete graphs. **The
first form splits by mantle only.**

Splitting *within* a mantle, by herd or by time, cuts edges. That is the frontier's own
problem, and the mojones answer it. Inside one part, segments already split a mantle by
cluster leaf. Across parts, a within-mantle split waits until the frontier's stubs are built.

# 6. How each kind of software would use it

These are cases to design against, not specifications for anyone.

**Void Hormiga.**
- *Today*: one store and a folder of pictures. That is a deed with a home store and a blobs
  part.
- *Archive*: past years kept on the office NAS as a pinned segments part, read by everyone
  and written by nobody.
- *Reference*: map shapes for a region, maintained once, could be a shared part that several
  organizations' databases read.
- *Phone*: carries the home, with the rest left on the NAS as absent parts.
- *Carrying*: its `.miga` v4 file is already a bale.

**Void Squid.**
- *Today*: an `assets.db` per world.
- *Assets*: models, textures and sounds are huge and identical across worlds. A
  content-addressed blobs part shared by every world stores each one once.
- *Base and edits*: a game's extracted base could be a pinned part, with the world's own edits
  in an overlay, so the base never changes and a rebuild starts from it.
- *Tags*: Squid's finding that tags launder becomes a placement rule. A source that writes
  tags writes them to its own part.
- *Builds*: a build of a game is an embarque. One identity, many partidas, and possibly
  enormous.

**Void Loops Studio.**
- *Today*: a project is a small graph (the home).
- *Audio*: recorded takes are large and only ever added, so they belong in a blobs part.
- *Renders*: rendered stems are derived data, rebuilt whenever they are missing.
- *Collaboration*: travels over Void Palabra, as commands. Placement does not change that.

**A world-model corpus (Void Bicho).**
- *The corpus*: a deed whose parts are pinned snapshots of many databases, so a training run
  names exactly what it read and can be reproduced.
- *Exports*: matrix exports are derived parts beside the corpus.

**Tests.**
- *The endless database*: a generated part, where rune *n* and its links are a function of
  *n*. There is no last rune, and every view must still finish with bounded work.
- *The interwoven database*: an ordinary part with random links.

**A small device.** A State-tier peer, such as an ESP32, holds a document and needs no deed.
**The deed is optional everywhere.** A store without one is a database of one part
(`deed_for_store`).

# 7. Past the first form, in order of how sure we are

1. **A missing part is a region not loaded.** *Built* (§4).
2. **Placement is not data.** *Built*: the placement law is a test. Moving bytes is
   configuration, never an edit, so it is not journaled, has no undo and leaves the record
   unchanged.
3. **Pinned parts give reproducible databases, and content names give sharing without
   copying.** *Built for segments.* A deed whose every part is pinned names one exact
   database, as a commit names a tree.
4. **Shared parts (*servidumbres*).** Databases interwoven by sharing parts rather than by
   copying them. This is how the author's *"interwoven"* databases (2026-11-03) can reach
   each other without merging. *Planned.*
5. **Overlays (*mejoras*).** A cheap fork: a new deed, the same pinned base parts and an
   empty writable part on top. A deletion of a base rune is written in the overlay as the
   store's existing soft-deletion mark. Precedents: union filesystems, container image
   layers, and table branches in Dolt and Iceberg. *Planned.*
6. **Generated parts (*manantiales*).** A part whose content is a function, so that
   "infinite" can be tested. It needs a pure, seeded generator and is read-only. Core's
   query-backed mantle is the same shape through a holiday. *Planned.*
7. **A deed as a part.** A database of databases, with cycles refused and the depth bounded.
   *Speculative:* it may be the same thing as a shared part. **Answered by the author
   (2026-11-06):** a database of databases is a **federation**, a real database whose mantle
   holds the others, not a deed ([beyond one database](/design/beyond-one-database.md) §4).
8. **The deed as runes.** *Considered and declined, as a lean:* if the deed were a Core
   document, moving a file would be journaled as an edit and gain an undo, which is wrong for
   configuration. It stays plain JSON ([open questions](/design/open-questions.md) §14).

# 8. What the first form does not solve

- **Writing to more than one part.** The working set writes the home. A mantle in another
  part is read through the database, not given to Core (`Database::home`'s comment says why).
  See [open questions](/design/open-questions.md) §13.
- **Moving a mantle between parts.** A copy is easy, and the placement law checks it. Letting
  the old part forget its rows is a write that no command made. §12 has the lean. The author
  ruled that nothing about the other file can be assumed, so the move first proves the identity
  mapping, and a move between databases is a mapping of commands
  ([beyond one database](/design/beyond-one-database.md) §3).
- **Splitting within a mantle** (§5).
- **Remote parts** (buckets, peers) and **garbage collection** of segments that no deed or
  manifest names.
- **A C ABI** for the deed. A host in another language reads the JSON today.
