---
type: Design
title: Open questions
description: What is genuinely undecided about Void Rancho, each with a lean, for the author — and which leans 0.1 took in order to build, provisionally.
tags: [status:current, audience:dev, confidence:exploratory]
timestamp: 2026-11-06T00:00:00Z
---

# Open questions

**Taken in 0.1** marks a lean the first build needed and took
([building 0.1](/design/building-0.1.md)); the author may still overrule it, and nothing links
Rancho yet, so none is expensive to reverse.

1. **Names.** Descriptive names are used throughout, per Palabra's and Bicho's precedent. A
   ranch set was offered at founding: the **corral** (the working set — the herd penned close at hand), the
   **pasture** (the cold tiers: [segments](/concepts/segment.md)), a **herd** (a cluster), the
   **brand** (a row's stable index ρ — but Core's `spirit` already names identity, and Rancho
   must not rename Core's words). **Void Hormiga relayed (2026-11-03) that the author wants names
   of its own, "bonus points if its in spanish"**, and offered a rancher's set: *potrero, corral,
   manada, lindero, mojón, arrear/soltar, cuenta, cabal/creciente/suelto, reseña, rodeo, embarque,
   partida, molino*. The deed (2026-11-06) added *rancho, escritura, parcela, casa, hierro (now the
   database's id, not ρ), hacienda, pajar, parcela sellada, servidumbre, mejoras, manantial,
   contramarca, fardo*. **Taken to build (2026-11-06):** the whole set is written in
   [the language](/concepts/language.md) **beside** the descriptive names, which code keeps.
   **Ruled 2026-11-06 by the author:** *"For names just make sure we're mostly indexing with
   the ID or Spirit."* The names are labels, and both sets stay as written. What matters is that
   nothing refers by name: the database, its parts, its mantles and its runes are referenced by
   id. The deed now names mantles by Core's `mantle.id`, because naming them by name lost a
   renamed mantle ([beyond one database](/design/beyond-one-database.md) §2). Closed.
2. **The line with Palabra's persistence.** [Boundaries](/design/boundaries.md) §4 proposes
   Palabra = at rest as a whole, Rancho = at rest as a live indexed database. Lean: agree it
   with Palabra by message. *0.1:* segments are named by Palabra's hash (SHA-256), so a
   segment and a Palabra block can be the same object later; nothing else assumed.
3. **Where graph analytics lives** — Rancho, Void Maiz, or Core. Lean: Rancho, which holds the
   whole graph. *0.1:* nothing moved. Rancho compiles Maiz's `graph.cpp` in place for Leiden,
   and has its own sparse algebra (BFS, shortest paths, PageRank, components, A·X) over the
   matrix view — which overlaps Maiz's PageRank and components. Needs Maiz's agreement and the
   author's before either copy is retired. **Maiz, 2026-10-31 (its Q43): its lean agrees —
   Rancho — on two conditions:** graph analysis moves as a **leaf target with no SQLite and no
   Core**, so a view-only Maiz host can cluster a drawn scene without taking a database; and
   Maiz's `graph.hpp` **forwards** until hosts move (as networking did to Void Hammer). Both are
   compatible with Rancho's lean. Since 2026-10-31 Rancho links Maiz's `voidmaiz_graph` target
   rather than compiling the file, so a move is a path change in one CMake fragment.
   **Ruled 2026-11-02 by the author: Rancho.** Moved that day as `voidrancho_graph`
   (`rancho/graph.hpp`, a leaf). Maiz's `graph.hpp` forwards to it. The overlap with the matrix
   view's PageRank and components is kept: the undirected, weighted forms clustering reads
   beside the directed semiring forms of the matrix view. Closed.
4. **The system of record at rest.** *Taken in 0.1:* the store is the baseline; a full
   document is an export, and an import replaces the store.
5. **Language.** *Taken in 0.1:* C++20 behind a small C ABI (`rancho/rancho.h`); SQLite
   vendored; GraphBLAS **not** vendored — a minimal CSR of Rancho's own, written as semiring
   products so SuiteSparse can stand behind the same functions later. A Python binding waits on
   a Python host.
6. **Shared media policy.** *Taken in 0.1:* detect a network path (spelling, then the OS);
   open it single-host in rollback mode with an exclusive lock held until close; steer sharing
   to segments. Open: whether a host may override to WAL on a path it knows is local but the OS
   reports remote (some sync clients mount that way).
7. **The training export format.** *0.1:* NumPy `.npy` + a JSON manifest, first — readable
   with nothing but numpy. Lean unchanged for the second form: Lance, because stable-worldmodel
   (and so LeWorldModel's tooling) reads it; Parquet for compatibility.
8. **Embeddings.** Stored per encoder with Bicho's provenance stamp; which vector index (HNSW,
   IVF-PQ) and whether it lives in SQLite or beside it — undecided.
9. **Undo across a reload.** Changing the working set rebuilds Core and loses memento undo.
   Waits on Core's reified commands; until then, the working-set budget should prefer not to
   evict what the person is editing.
10. **New, from building: clusters as tags.** Hormiga's original proposal wrote each rune's path
    as `c1:`/`c2:` tags; the store keeps it as derived data instead. Writing tags would make the
    clusters visible to every tag query and to Core — and would be data, so it would have to go
    through the dispatcher as commands, and change whenever clustering is re-run. Lean: keep
    derived; offer a command generator for a database that wants them.
11. **New, from building: who may read the store's connection.** `Store::handle()` gives
    Rancho's read-side modules (matrix view, segments) the SQLite connection, documented
    read-only. A write through it would be a second door. Lean: keep it, and keep it out of the
    C ABI; a lint like Hormiga's `lint_host_seams.py` if a second module starts using it.
12. **New, from the deed: moving a mantle between parts.** A copy is easy and the placement law
    checks it. Letting the old part forget the mantle's rows is a write no command made.
    Lean: a re-placement is configuration, not an edit, so it is not journaled and has no
    undo. Rancho copies the rows, checks the copy against the original, writes the new deed
    atomically, and only then lets the old part release them. The release is a call that refuses
    unless the deed already places the mantle elsewhere and the copy matched, so every row stays
    rebuildable.
    **Ruled 2026-11-06 by the author:** a move between files needs a holiday-like structure to
    map them, because *"we can't assume much at all about the other file. Even if they are both
    miga or ink files."* Folded into [beyond one database](/design/beyond-one-database.md) §3:
    - **Between databases**, a move is a **mapping** of commands: the mantle as the commands that
      would make it, translated, run through the other database's dispatcher.
    - **Between parts of one database**, the identity mapping is **proved** first (same
      declarations, domain present, key and name free), and only then is the move the
      placement above.

    Not built yet.
13. **New, from the deed: writing to more than one part.** *Taken in the first form:* the
    working set writes the home, and other parts are read. Lean for the next form: one
    working set per writable part, each syncing only the mantles the deed places in it. Core
    may hold mantles from several parts. An edit to a pinned or shared part is refused with a
    reason, or goes to an overlay if the deed has one.
    **The author, 2026-11-06, for several databases:** *"there will likely need to be another
    database to track the sub databases… D that contains A, B, and C on a mantle. Mostly for
    palabra to keep track of edits… Then can always dissolve database D."* That is the
    **federation** ([beyond one database](/design/beyond-one-database.md) §4). It holds no
    member's rows, every edit lands in a member through that member's own dispatcher, and a
    change across members is a saga.
    Still open, for **one** database in several writable parts: lean that the database is
    already the one identity and the one history, so no tracker is needed, and the lean above
    stands. The author to confirm.
14. **New, from the deed: JSON, or a Core document?** A deed written as runes could be edited
    by commands and drawn by Maiz. Lean, *taken:* plain JSON. Placement is configuration, and
    journaling a file move as an edit would give it an undo and put it in the record. A host
    that wants to show the deed draws it from `Database::placement()`. *Refined by §13's
    ruling:* a **federation's** membership and mappings are authored and have a history, so they
    are runes in a database. One database's placement stays JSON. The line is: *what is
    authored is data; where bytes sit is configuration.*
15. **New, from the deed: the file's name and extension.** *Taken:* `.rancho`, the author's
    suggestion, and "deed" in code. The ranch name is *escritura*. A store's own extension stays
    whatever its host chooses (Hormiga's `-store.db`, Squid's `assets.db`).
    **Ruled 2026-11-06 by the author:** the rancho is *"like, a parent class almost? Other file
    types like miga and ink will inherit properties from the rancho file and build
    themselves"*, and *"any filetype could be a hybrid of a rancho if done well enough. Like a
    csv file, docx file."* Folded into [beyond one database](/design/beyond-one-database.md) §5,
    as **profiles**.
    - **Built:** a store that carries its own deed is the first **hybrid**.
    - *Superseded the same day by the second round* (§23): the deed's `profiles` were removed,
      because a database declares no kind. A host brings `rancho::Expectation`s instead.
    - *No adopted format is planned:* the CSV and the docx were examples of how a database could
      be stored (§21).
16. **New, from the deed: a bale, and Hormiga's `.miga` v4.** A database packed into one file
    to carry it (*el fardo*). `.miga` v4 already is one: a store and its pictures. Lean: a
    Rancho bale form (a deed and its parts in one container), which Hormiga may adopt or keep
    its own. That is Hormiga's call, to ask by message once the form exists.
    **Answered 2026-11-18 by the author's ruling** ([authority](/concepts/authority.md)): the
    `.miga` is Hormiga's bale and its database's authority, in Hormiga's own form; the JSON and
    SQLite files are caches of it. A Rancho bale form, when one exists, is offered, not imposed.
17. **New, from the deed: whose id.** Hormiga already gives each database a sync id ("a
    database is its own room"). Lean, *taken:* the deed's `id` accepts any non-empty text, so a
    host may use its own id as the deed's. A host minting none gets `mint_database_id`.
    *2026-11-18:* the authority carries the id (Hormiga's `sync.database`, in its config); a
    cache records it but is keyed by the authority FILE, because two copies of one database are
    two files ([authority](/concepts/authority.md), rule 3).
18. **New, from the mapping: carry a rune's id or mint a new one?** A rune moved by a mapping
    can keep its `spirit.id`. Then it is *the same rune*, and Palabra's join can later merge the
    two databases' copies. Or it can get a new id, with the old one recorded beside it, as a
    different rune that came from there. Lean: **carry on a move, mint on a copy.** A move
    leaves one rune in the world, and a copy leaves two, which must not collide when the
    databases meet again ([beyond one database](/design/beyond-one-database.md) §3.3).
    **Ruled 2026-11-06 by the author:** *"For copies I feel like they should be different
    IDs."* A copy mints; a move carries. The author also asked about a *"pointer" rune* for
    storage and efficiency, and expected it to be *"an error prone mess"*. Folded into
    [beyond one database](/design/beyond-one-database.md) §2.1. A pointer asks for three things, each already answered without a new
    rune: storage by content addressing beneath identity, meaning by an id in data that may
    dangle, and a live view by Core's query-backed mantle. Lean: **no pointer rune.**
    *Still open:* the shape of a reference to another database's rune. Lean: three ids
    (database, mantle, rune) in a host's own glyph, written by commands and resolved through a
    dataspace, and absent when its database is not enlisted. Not a Core glyph, by §24's ruling.
19. **New, from the mapping: where a mapping lives.** It is authored, so it is data. It could
    live in the receiving database, in the federation, or in a database of mappings that many
    federations share. Lean: in the federation when there is one, else in the receiving
    database. It is named by id, and kept as runes so that it has a history.
    **Ruled 2026-11-06 by the author:** *"maybe we need another type of database? More like a
    "session"… where we are storing mappings"*, with its established name researched. It is
    a **dataspace** (Franklin, Halevy and Maier, 2005): a live session of participants and the
    mappings between them, added pay-as-you-go. What it authors is written in its
    **federation**, a database, because a mapping must be replayable
    ([beyond one database](/design/beyond-one-database.md) §4.1–4.2). So a mapping lives in the federation of the dataspace that uses it.
    Names were checked against the family (§4.7): *session*, *workspace*, *join*, *attach*,
    *dissolve*, *profile*, *member* and *unlist* are taken, so the verbs are **enlist**,
    **discharge** and **disband**.
20. **New, from the federation: what dissolving keeps.** Dissolving changes no member (law 5).
    The open part is whether the federation's own history, the record that changes happened
    together, is archived somewhere or simply dropped. Lean: the federation is a database like
    any other, so dissolving it is closing it. Its file may be kept as an archive or deleted, and
    nothing in a member depends on it. Needs Palabra's view of a history that names other
    histories' utterances.
    **Ruled 2026-11-06 by the author:** *"Dissolving a federation can be a sort of function that
    returns a small micro database of the history? And this can be optional to return."*
    `disband(federation, keep_history)` changes no participant. Asked to, it returns the
    **history** (*el acta*), a new small database: a fork with its lineage naming the federation,
    holding the changes that crossed participants and the mappings, and no participation, so it
    cannot be reopened as a dataspace ([beyond one database](/design/beyond-one-database.md) §4.6). Palabra's view is still asked for.
21. **New, from adoption: identity for a file that has none** (a CSV row). Core forbids deriving
    a spirit id from content. Lean: a declared key column, with minted ids kept in an overlay
    part beside the file; with no key, the file is read-only through its mapping and is never
    merged ([beyond one database](/design/beyond-one-database.md) §5.4).
    **Ruled 2026-11-06 by the author:** think abstractly (*"IDs for every cell… each row is a
    mantle? But also each column is a mantle?"*), and the CSV is only an example of how a
    database could be stored, not a format to build. Folded into [beyond one database](/design/beyond-one-database.md) §5.4, as
    **one owner, many views**. A rune lives in one mantle (SPEC §3.4), so every cell is a rune
    with a minted id, one grouping owns it, and every other grouping is a tag axis and a
    query-backed mantle that selects by it. The key-column lean is withdrawn: it derived
    identity from content. Ids are kept wherever the shape can hold them, and a shape with
    nowhere is read-only. Closed.
22. **New, from PNG's safe-to-copy rule: unknown members after an edit.** A deed keeps every
    member it does not know. That is right for reading, but wrong when the member depends on
    what Rancho just changed. Lean: when Rancho changes a part's form, path or pin, it drops that
    part's unknown members and notes it. The deed's own unknown members are kept.
    **Ruled 2026-11-06 by the author:** *"rancho would be completely agnostic to anything…
    rancho doesn't care if something is even real, so long as it somehow goes into a mantle."*
    The question was only about the deed file's own fields, never about content. Answered by
    agnosticism ([beyond one database](/design/beyond-one-database.md) §5.5): a deed holds Rancho's members only, and anything a host says
    about its database belongs in a mantle. A member Rancho does not know is passed through and
    noted, never dropped. A deed of a newer format is read and **never rewritten**, so nothing is
    left stale. The drop lean is withdrawn. **Built** (`deed_test`). Closed.
23. **New, from profiles: who names them.** A profile is `"<name>/<version>"`. Two projects
    must not choose the same name. Lean: a profile is named by the project that owns it (`ink`
    is Jelly's, `miga` is Hormiga's), as Void Mago already registers projects. A profile
    published for others to read writes its checks down in that project's OKF.
    **Ruled 2026-11-06 by the author:** *"that feels like a question that isn't for rancho?
    Unless if you feel a profile is inherent to a database, but I don't think it is. A
    database exists on its own."* Agreed: a database declares no kind. The deed's `profiles`
    member is removed, and a host brings `rancho::Expectation`s, its own names for what it
    relies on, which `Database::open` checks ([beyond one database](/design/beyond-one-database.md) §5.2). Rancho keeps no registry.
    **Built.** Closed.
24. **New, from the mapping: should Void Core have a *database holiday*?** The author asked
    whether the holiday should become *"something else entirely… a larger abstracted holiday
    structure to map databases together"*. Lean: Core's holiday stays the door, and Rancho's
    mapping is the dictionary that rides on it. If the author wants the word in Core, it is a
    message to Core: a holiday whose `describe()` returns a vocabulary (glyphs, tag axes,
    domains), against which a mapping is checked. Rancho owns none of Core's words, so it does
    not stretch this one itself.
    **Ruled 2026-11-06 by the author:** *"I think void core should have little database things,
    cuz we have rancho for that."* No database holiday, and no message to Core. The holiday stays
    Core's door, and mappings, dataspaces and federations are Rancho's. Closed.
25. **New, from the dataspace: one that is never saved.** A dataspace opened for a moment (to
    compare two databases, or to move one mantle) still authors mappings and runs sagas. Lean:
    it still has a federation, made in the cache folder and disbanded without its history when
    the dataspace closes. The railguard then holds while it is open, and a saga interrupted by a
    crash can be finished or compensated. The other choice, a federation held in memory only,
    would lose a saga half done. The author to confirm.
26. **New, from research after the dataspace: crossings as update exchange.** ORCHESTRA's
    collaborative data sharing (VLDB 2007, TODS 2013) is the strongest successor to the dataspace,
    and the closest to the family ([after the dataspace](/design/after-the-dataspace.md) §2). Lean:
    - a participant **consigns** commands from its journal, through the mapping;
    - another **takes delivery** of them, by its own commands, and may refuse or
      override any of them, and its override holds against later updates;
    - every crossed rune carries its **provenance** (participants and mappings, by id), written
      as data;
    - a participant's **trust policy** is data in the federation, checked on delivery.

    A move is then a consignment, a delivery and the source's own `mantle rm`, still a saga.
    (*Consign* and *take delivery* replace ORCHESTRA's *publish* and *reconcile*, which are
    Hormiga's, Mago's and Palabra's words: [the algebra](/design/the-algebra.md) §7.)
27. **New, from model management and categorical data integration: what a mapping may be.**
    Mapping composition can need more power than its parts, and exact inverses rarely exist
    ([after the dataspace](/design/after-the-dataspace.md) §3). Lean: a mapping is data in a **small checkable language** (renames, field
    correspondences, selections), verified completely against both vocabularies before use. A
    composed mapping is a proposal. A move back is checked, not assumed, and a crossing that would
    lose something records the loss or is refused. Arbitrary transformation code is not a
    mapping: it is a host's program, run by its own commands.
    **The author, 2026-11-06:** *"mappings as its own form of math, with operations… rancho
    should hold the logic for how to operate over these mappings"*, and *"see how CQL works"*.
    Folded into [the algebra](/design/the-algebra.md). The small checkable language is **CQL's**: a mapping is a schema
    morphism (entities to entities, foreign keys to paths, attributes to terms), and a tag rewrite
    is a query. A Core document's schema is a finite acyclic category, so every check is decided
    by enumeration, and "lossless" is "fully faithful" (§5.3 there). Composition of schema
    morphisms is always a schema morphism, so §3.1's warning applies only to general rules. Lean
    taken; the kernel is planned in §11 there.
28. **New, from sovereign data spaces and local-first access control: who may consign and
    take delivery.** When participants belong to different people, permission must travel with the
    data, as Keyhive's convergent capabilities do ([after the dataspace](/design/after-the-dataspace.md) §5). Lean: this is Void
    Palabra's, which owns signing and peers. Rancho records in the federation which capability
    each crossing used. To ask Palabra by message once §26 is agreed.
29. **New, from foundation models for integration: who proposes a mapping.** Models now do schema
    and entity matching well ([after the dataspace](/design/after-the-dataspace.md) §7). Lean: a model may **propose** a mapping as a
    rune, with Void Bicho's provenance stamp; a person accepts it by a command; §27's checker
    verifies it. A model never applies a mapping.
    **The author, 2026-11-06:** *"Hormiga could implement AI, however Rancho is giving structure
    to do it."* Folded into [the algebra](/design/the-algebra.md) §10: Rancho owns the input (schemas), the proposal's
    data shape (correspondences with a score and a proposer's provenance), the check and the
    accepting command, and may ship a deterministic baseline (Similarity Flooding, no model).
    Models are a host's or Void Bicho's, behind the same *propose* contract Void Squid asked
    Bicho for. Rancho names and links no model. A message to Bicho is drafted locally.
30. **New, from the algebra: a schema across a holiday's door.** A holiday's `describe()` returns
    `{capabilities, kind, status}` (Core SPEC §10.1). Lean: Rancho defines a schema data shape
    (`rancho-schema/1`), and a holiday that can be mapped returns it inside `capabilities`. Core's
    shape is unchanged and Core never reads it, which keeps §24's ruling ([the algebra](/design/the-algebra.md) §9).
31. **New, from the algebra: the interface law with Void Palabra.** Σ and Δ preserve colimits, so
    wherever Palabra's join is a colimit of instances (a union keyed by id, conflicts aside),
    merging then mapping equals mapping then merging ([the algebra](/design/the-algebra.md) §7). Lean: state it as the
    law between the two libraries, and test it once both sides exist. Palabra must say whether
    its join is a colimit in this sense, and where it is not. A message is drafted locally.
32. **New, from the algebra: provenance and Void Allomone.** Provenance through mappings is a
    semiring of polynomials (Green, Karvounarakis and Tannen, 2007), and a trust policy is its
    evaluation ([the algebra](/design/the-algebra.md) §8). Allomone owns annotations and their lattice merge. Lean:
    provenance through mappings is Rancho's, because it follows the mappings, and Allomone is told
    before it is built, in case its annotations should carry it.
33. **New, from the author: is Void Allomone already this algebra?** *"Perhaps we should strengthen
    Void Allomone more such that it can truly be this algebraic step? Or is it worth distinguishing
    them?"* Read from Allomone's own bundle, it is a derive-only annotation language over one
    document: order theory (join semilattices, one stratum of Datalog, CALM), never writing, never
    iterating to a fixpoint. Rancho's algebra is category theory between databases, and it must
    write (Σ, by commands) and iterate (the chase). Lean: **keep them distinct**. Making Allomone
    the algebra would break its two load-bearing properties. They are one idea, composition by
    law with disagreement surfaced, at three levels: Allomone for opinions about one database,
    Palabra for one database's states, and Rancho between databases. They meet at three data
    shapes: scripts read through Δ, proposals combined as Allomone sources with ⊤ for a person to
    settle, and simple trust policies over provenance tags ([the algebra](/design/the-algebra.md)
    §12). The pair is recorded in Void Mago as proposed, and a message to Allomone is drafted
    locally. §32 is answered by the same reading: the full provenance semiring stays Rancho's,
    and its monotone policies can be Allomone scripts. The author to rule.
34. **New, from Void Hormiga's "the overview": how wide may a node of the cluster tree be?**
    **Lean taken to build (2026-11-09):** `ClusterOptions::fanout` = 250 by default, the number
    Hormiga's overview uses, with levels added above Leiden's communities by Void Maiz's
    merge-bundle-slice steps (`graph::bound_fanout`). Paths change when a mantle is clustered
    again, as they always could. Overrulable: another default, or 0 (Leiden's own tree) by
    default.
35. **New, from Void Squid: overlays, first form.** **Lean taken to build (2026-11-09):** an
    overlay is a part member (`overlay`, naming the store that keeps the edits) and a per-rune
    difference keyed by spirit id, with Squid's splice as its rules. A host records runes with
    `Database::record_overlay`, because a working set does not yet write to more than one part
    (§12–13). Open: routing a working set's write-back of base mantles into the overlay;
    overlays of edges and of a base rune's removal; whether `overlay` belongs on the base part
    (as built) or on the overlay part (Squid's sketch).
36. **New, from Void Squid: a document's `_baseline`.** Void Maiz's Session keeps `revert` and
    `diff` working through Core's `_baseline`, a full copy of the document as last saved. The
    store never keeps it: `import` and the working set skip it ("the store is the baseline"), so
    a store-backed host's baseline is the last state written, read back on load. Lean: no
    change in Rancho; a Session host that wants `revert` across a reload reads the store's state
    as its baseline, and a pinned snapshot ([segment](/concepts/segment.md)) is the baseline
    that survives later saves.
37. **New, from the frontier: the loading words.** Void Hormiga offered a rancher's vocabulary
    (*el potrero, el corral, la manada, el lindero, el mojón, arrear/soltar, la cuenta,
    cabal/creciente/suelto, la reseña, el rodeo, el embarque, la partida, el molino*), and
    [the language](/concepts/language.md) uses them beside the descriptive names. The code
    uses descriptive names (`Stub`, `Frontier`, `Rodeo`, `EmbarqueWriter`), with the ranch word
    in each comment. The author to choose; the code follows.
