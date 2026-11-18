---
type: Concept
title: The language — how a database is made
description: Rancho's first job, by the author's ruling of 2026-11-06. The words the Void family uses for what a database is, where it lives, what is in hand, how sure an answer is, and how the whole of it is traversed or shipped. Each word has its descriptive name (used in code), the ranch name offered for it, and whether anything is built behind it. Then the laws those words obey.
resource: include/rancho/deed.hpp
tags: [status:current, audience:all, confidence:asserted, foundation]
timestamp: 2026-11-06T00:00:00Z
---

# Why a language first

The author, 2026-11-06: *"Rancho should primarily have shared language on how databases
are made. And then secondly have the tools to make them."*

Void Hormiga, Void Squid and Void Loops Studio each built a database before Rancho existed,
and each had to invent words for the same things: a part of the database held in memory,
a group of runes loaded together, an export of everything. A word on this page is a
**promise about behaviour**. A host that uses it means what is written here, and a
library that offers it keeps the laws in §8. Every word was checked against the family's
other projects, so that none means two things
([beyond one database](/design/beyond-one-database.md) §4.7).

Every word has two names:
- the **descriptive** one, which code and the rest of this bundle use;
- the **ranch** one, offered by Void Hormiga for the author on 2026-11-03 (*"bonus points if
  it's in spanish"*), with a few added on 2026-11-06 for the new words. They are used beside
  the descriptive names. **The author ruled on names on 2026-11-06:** *"just make sure we're
  mostly indexing with the ID or Spirit"*. Names of either kind are labels, and what matters
  is that nothing refers by one (law 9; [open questions](/design/open-questions.md) §1).

**Built** means code and a test stand behind the word. **Planned** means the word is
reserved and its meaning is fixed, but nothing implements it yet.

# 1. Five questions every database answers

| question | descriptive | ranch | owned by | in the deed? |
|---|---|---|---|---|
| Who is it? | **identity** | *el hierro* (the brand) | Rancho: an id minted once | yes |
| What does it say? | **the record** | *la hacienda* (the stock) | Void Core's vocabulary: mantles, runes, edges, tags, the top-level keys. Rancho owns none of it | no: its parts hold it |
| What has been worked out from it? | **derived data** | *el pajar* (the hayloft) | Rancho: clusters, estimates, the row index, typed columns, summaries, matrix exports | planned, as derived parts |
| Where do its bytes live? | **placement** | *las parcelas* | the [deed](/concepts/deed.md) | yes |
| What is in hand right now? | **residency** | *el corral* | one process on one device: view state | **never** |

These five are separate on purpose, and each has its own rule:
- **The record is the only thing authored**, and only through Void Core's dispatcher (the
  railguard: [boundaries](/design/boundaries.md)).
- **Identity is minted once.** Names are for people.
- **Derived data can always be thrown away**, and a pass rebuilds it. That is why it belongs
  in a phone's `cache_dir`, while the record belongs in `data_dir`.
- **Placement is configuration.** Moving bytes between parts never changes the document
  (the placement law, §8).
- **Residency is never data.** What a person had loaded is not journaled and is not written
  into the deed.

# 2. The whole and its parts

| descriptive | ranch | what it is | |
|---|---|---|---|
| **database** | *el rancho* | one identity and one record, however many places its bytes live | built |
| **deed** | *la escritura* (the title deed) | the `.rancho` file: the database's identity and its parts. It never lists rows, segments or files, so it stays the same size however large the database grows | **built** ([deed](/concepts/deed.md)) |
| **part** | *la parcela* | one place a database's bytes live, in one **form**. A part holds whole mantles, or holds blobs | **built** for three forms |
| form: **store** | | a live SQLite store, written by one host ([store](/concepts/store.md)) | built |
| form: **segments** | | an immutable snapshot: content-named files and a manifest ([segment](/concepts/segment.md)) | built |
| form: **blobs** | | a folder of pictures, audio and models, by path | built |
| **home** | *la casa* | the part that holds the top-level keys, and the one a working set writes (in the deed's first form) | built |
| **the whole at rest** | *el potrero* | everything a database holds at rest, across all its parts | built |
| **pinned part** | *parcela sellada* (sealed) | a part fixed to one snapshot by the SHA-256 of its manifest. It never changes, so a deed that pins all its parts names one exact database | built for segments |
| **shared part** | *la servidumbre* (an easement) | a part this database reads but does not own: a reference set or an asset pool that many databases share, stored once | planned |
| **overlay** | *las mejoras* (improvements on another's land) | a writable part over a read-only one. Edits land in the overlay, and the base never changes | planned |
| **generated part** | *el manantial* (a spring) | a part whose runes are computed, not stored: rune *n* is a function of *n*. It is how an endless database is tested. Void Core's query-backed mantle (SPEC §10.2) is the same idea reached through a holiday | planned |
| **fork** | *la contramarca* (the counter-brand a new owner adds) | a new database made from another's parts: a new id, with its lineage recorded. **A copy of the deed is the same database. A copy of the parts under a new id is a new one** | planned (`lineage` is kept, not read) |
| **bale** | *el fardo* | a database packed into one file to carry it on a USB stick or by email. Void Hormiga's `.miga` v4 is a bale in Hormiga's own form | planned in Rancho's form |
| **file type** | *la raza* (the breed) | a format built on the rancho: `.miga`, `.ink`, a Squid world. It is its host's: its extension, its bale, its vocabulary and part forms. It inherits the five questions, the laws and the deed. **A database declares no kind** (the author, 2026-11-06: *"A database exists on its own"*) ([beyond one database](/design/beyond-one-database.md) §5.2) | the host's |
| **authority** | | the file a person opens, moves and shares, which is the database: a host's bale, a store carrying its deed, a deed. Everything else a host keeps beside it is a cache ([authority](/concepts/authority.md)) | **built** (2026-11-18) |
| **cache** | | a host's working copy of an authority, in a per-user folder, one per authority file, with a note saying how the two stand: new, current, stale, ahead, diverged, orphan | **built** (`working_copy_for`, `cache_state`) |
| **keepsake** | | what is neither authority nor cache: credentials, backups. Per user, never inside an authority | **built** (`keep_root`) |
| **expectation** | | one thing a host relies on in a database it opens (*"a scene has a mantle called 'scene'"*), named by the host and checked at open, the ones it builds on first. A database that falls short still opens, and says what is missing | **built** (`Expectation`, `meets`) |
| **born / adopted** | | a born file is built from rancho parts and carries a deed. An adopted one keeps its bytes as its own tools expect, and a mapping presents it in Core's vocabulary. No adopted format is planned: the author's CSV and docx were examples | born: built |
| **hybrid** | | a file that is fully its own format and fully a rancho, because it carries its deed in its format's channel for what a reader may ignore. A store carrying its deed in its meta is the first | **built** (`embed_deed`) |
| **one owner, many views** | | how groupings that overlap are held: a rune lives in one mantle (Void Core), and every other grouping is a tag axis and a query-backed mantle that selects by it. A table's cells are runes; a row or a column is a view ([beyond one database](/design/beyond-one-database.md) §5.4) | Core's, already |
| **key** | | how a deed names a mantle: Core's `mantle.id`, which a rename keeps, or `name:<name>` for a mantle without one | **built** |

# 3. What is in hand

These are the [frontier](/design/one-database-many-files.md#4-the-frontier-at-every-scale)'s
words, from Void Hormiga's messages of 2026-11-03.

| descriptive | ranch | what it is | |
|---|---|---|---|
| **working set** | *el corral* | the part of the database Void Core holds ([working set](/concepts/working-set.md)) | built |
| **cluster** | *la manada* (the herd) | the unit of loading, storage and analysis ([cluster](/concepts/cluster.md)) | built |
| **frontier** | *el lindero* (the boundary line) | the line between what is held and the rest, and the practice of working across it. **It is the same line at every scale**: memory and disk, one part and another, this device and the network | built at the part level: a missing part is a region not loaded |
| **stub** | *el mojón* (a boundary stone) | the far end of an edge that leaves what is held: a name, a glyph, its tags, its leaf, and whether it is a rune at all | built (2026-11-09): `Selection::fringe`, `Store::frontier` ([working set](/concepts/working-set.md)) |
| **load / drop** | *arrear / soltar* (drive in / turn out) | change what is held. A reload, never an edit | built (by cluster) |
| **certificate** | *la cuenta* (the head count) | what the corral holds completely: which regions have all their runes and all their edges | built at the part level (`Database::complete`) and the leaf level (`Store::frontier`, 2026-11-09) |

# 4. How sure an answer is

| descriptive | ranch | what it means |
|---|---|---|
| **settled** | *cabal* | no load can change it |
| **growing** | *creciente* | a load can only add to it (a monotone question asked of a part) |
| **open** | *suelto* | it can change either way; shown as a bound |
| **summary** | *la reseña* | per cluster node: its counts, its ranges, its links to its neighbours. It answers the decomposable questions without a load |

**Built (2026-11-09):** the reseña, per node of a mantle's cluster tree: runes, internal and
outgoing ties, and the ties between siblings, kept by write-back ([cluster](/concepts/cluster.md)).
Every query the store answers (`select`, `count`, `range`, `find_names`) reads the whole potrero,
so it is cabal. An answer a host computes from the corral is cabal when la cuenta says the
region it read is complete, and creciente or suelto otherwise; the statuses themselves are the
host's to show. Still planned: the reseña's ranges and counts by glyph and tag, and sketches.

# 5. Traversing and shipping

| descriptive | ranch | what it is | |
|---|---|---|---|
| **pass** | *el rodeo* (the roundup) | a visit to the whole of a database in bounded memory, in storage order, resumable. Any partial result is open until the pass finishes | built (2026-11-09): `rancho::Rodeo` ([rodeo](/concepts/rodeo.md)) |
| **complete export** | *el embarque* (the shipment) | an output that read the whole database. It is one thing in identity and many in storage: a manifest naming content-named pieces, exactly as segments are | built (2026-11-09): `rancho::EmbarqueWriter` |
| **piece** | *la partida* (a lot) | one piece of an export. The output's own structure cuts the pieces: a site's pages, a game's levels | built |
| **output on demand** | *el molino* (the mill) | an output computed from the database each time it is asked for, never stored whole | a pattern, not a library |

# 6. Between databases

From the author's answers of 2026-11-06 ([beyond one database](/design/beyond-one-database.md)).

| descriptive | ranch | what it is | |
|---|---|---|---|
| **copy / move** | | a copy mints new ids and records where each rune came from; a move carries them. There is no pointer rune: storage is shared by content addressing, meaning by an id that may dangle, a live view by Core's query-backed mantle (§2.1) | ruled |
| **mapping** | *la guía* (the transit permit cattle travel with, naming origin, destination and brand) | how one database's words correspond to another's: glyphs, tag axes, names, ids, bindings, domains, holidays. It translates **commands**, as Cambria's lenses translate edits, so a mantle moves to another database as the commands that would make it, run through the other's dispatcher. It rides on a holiday (Void Core's door) and is checked against what the holiday describes. Inside one database it is the identity mapping, **proved** rather than assumed | planned |
| **dataspace** | *la feria* (the fair, where several ranches bring stock for a while, trade under agreed terms, and go home) | the live session in which several databases, its **participants**, are worked on together, with the mappings between them added as they are needed (Franklin, Halevy and Maier's term; not the European *data spaces* for sharing between organizations, [after the dataspace](/design/after-the-dataspace.md) §5.1). It is residency, never data: what it authors is written in its federation. A database is **enlisted** in it and **discharged** from it | planned |
| **federation** | *la mancomunidad* (a union of towns that each may leave) | a dataspace's record: a database holding one rune per participant, the mappings, and the changes that crossed participants, so Void Palabra can track edits across them. It holds no participant's rows | planned |
| **disband** | | end a federation for good. No participant changes. Asked to, it returns the **history** (*el acta*, the minutes a meeting leaves): a small new database holding what crossed participants, and no participation | planned |
| **consign / take delivery** | | how an edit crosses between participants (ORCHESTRA's publication and reconciliation, renamed because *reconciliation* is Void Palabra's): a participant consigns commands through a mapping, and another takes delivery by its own commands, refusing or overriding any ([after the dataspace](/design/after-the-dataspace.md) §2) | planned |
| **saga** | | a change across participants: a step in each, each step a command with a compensating command. A step whose participant is absent waits | planned |

# 7. The algebra

From the author's ask of 2026-11-06 for *"actual algebra"* ([the algebra](/design/the-algebra.md)).

| descriptive | what it is | |
|---|---|---|
| **schema** | in the algebra's sense, not the store's SQL tables: a database's entities, foreign keys, attributes and equations, read off its declarations. A Void Core document's schema is a finite acyclic category, so every question about it is decidable | planned |
| **mapping** (as an arrow) | a schema morphism: entities to entities, foreign keys to paths, attributes to terms, preserving equations. Mappings compose, with identities | planned |
| **Δ, Σ, Π** | the three migrations a mapping gives: read back, move forward (carrying ids, minting what the target requires), join forward. Σ ⊣ Δ ⊣ Π | planned |
| **loss** | the failure of the round trip's unit, row by row. A fully faithful mapping loses nothing | planned |
| **pushout** | two schemas merged along their overlap | planned |
| **proposal** | correspondences between two schemas with scores and a proposer's provenance, from a model or the deterministic baseline. Data, checked by the algebra, accepted by a person's command | planned |

# 8. The laws

These hold for every database, whatever its form, its size or its host. Where a test holds one,
it is named.

1. **Every row can be rebuilt by replaying commands.** Rancho never changes data. Placement,
   derived data and residency are not data
   ([boundaries](/design/boundaries.md); `working_set_test`).
2. **The round trip.** Whatever a store took in, it gives back as the same document
   (`store_test`, `segments_test`).
3. **The placement law.** The document does not depend on where its bytes are. The same rows
   in one part or in three assemble to the same document, and Core reads it the same way
   (`deed_test`).
4. **Absent is not deleted.** A part not reachable, or a cluster not loaded, is *unknown*.
   Code that reads an absence as a deletion is wrong, however convenient (`deed_test`; the
   frontier's first lesson, as Void Hormiga found it).
5. **Every database opens.** A database that does not open is a bug, whatever its size and
   however many of its parts are missing. The budget decides how much of it is held, never
   whether it opens (the author, 2026-11-03).
6. **Nothing grows with the database where a bound is possible.** The deed lists parts, not
   rows. A part lists its segments, and a view reads what it shows. Assume every database is
   infinite and densely interwoven (the author, 2026-11-03).
7. **A pinned part never changes, and a live part has one writer.** Two machines never write
   one file. Sharing is done by immutable parts or by Void Palabra's merge
   ([tiers](/concepts/tiers.md)).
8. **Identity is not location and not name.** A database keeps its id when its parts move, and
   a store that is another database's home is not taken as this one's (`deed_test`).
9. **Everything is referenced by its id** (the author, 2026-11-06: *"mostly indexing with the
   ID or Spirit"*). A database, a part, a mantle and a rune each have an id that a rename keeps.
   Whatever crosses a file boundary names by id. A name may be shown, and may be typed and
   resolved at once. A deed that names a renamed mantle by its key keeps it in place, where one
   that named it by name lost it (`deed_test`).
10. **Rancho is agnostic** (the author, 2026-11-06: *"rancho doesn't care if something is even
    real, so long as it somehow goes into a mantle"*). Rancho never decides whether content is
    real, valid or recognisable, and a database declares no kind. A deed holds Rancho's members
    only. Whatever a host has to say about its database is data, in a mantle. A member Rancho
    does not know is passed through and noted, and a deed of a newer format is never rewritten
    (`deed_test`).

# 9. Words Rancho uses but does not own

*mantle, rune, spirit, glyph, edge (link), tag, binding, holiday, query-backed mantle,
dispatcher, journal* are Void Core's (`VoidCore:SPEC.md`). *Canonical bytes, merge, history* are Void Palabra's.
*Embedding, encoder* are Void Bicho's. Rancho uses them with their owners' meanings and never
redefines them.
