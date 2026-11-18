# Bundle Update Log

## 2026-11-09 — The mail answered: Squid's foreign file and overlays, Hormiga's frontier and overview, Maiz's phones
* **The author:** messages should not pile up at the root. When one is done, delete it; a reply is not always needed. So this session read every message, built what each asked, folded it here, and cleared the root (see the end of this entry).
* **IMPORTANT — Void Squid adopted Rancho** (`voidsquid-adopted-and-a-file-that-is-not-yours`, folded and deleted). The author's ruling there: *"the core database structures of squid (and many other applications) should rely on rancho … like a base class sort of, with our own specialties in mind (that rancho should more or less support)."* A game library's catalogue is a store carrying its own deed, a `.squid` project is a rancho, and Squid fills its stores through the bulk load to keep per-tag sources. `void.json` now lists Squid as the second client.
  - **Found by Squid: a Store opened on a file that is not a store changed it before refusing it.** Squid's pre-Rancho `assets.db` shares three table names with a store. `rancho ids` on it turned WAL on, added eight tables, then failed. Now the constructor decides what the file is before its first write: an empty file, a `store_meta` with a format, or Hormiga's format-1 tables, else refused untouched with its tables named. The exclusive-lock case still reads as "in use" (found by `media_test`, when the first draft reported it as foreign). `store_test` "a file that is not a store is refused untouched".
  - A parent's build type is no longer forced: only a top-level Rancho sets `RelWithDebInfo`.
  - The bulk door is documented as a host's door too, for tags that arrive with their own sources.
  - A blobs part at `.` is a promise ([deed](/concepts/deed.md)).
  - The `rancho` tool reads its arguments from the wide command line on Windows, so a path outside the ANSI code page works (checked with `rancho-ñ/año.rancho`).
* **IMPORTANT — Overlays, first form** (`voidsquid-overlays-the-spec-from-our-splice`, folded and deleted). Two part members: `database` (another database's id, which its store must record: a swapped library is a mismatch, Squid's question 1) and `overlay` (the store that keeps a person's edits). `rancho/overlay.hpp` keeps a per-rune difference keyed by spirit id; reading gives base ⊕ overlay; `Database::record_overlay`, `forget_overlay`, `residuals`. `deed_test` checks each of Squid's seven rules across a rebuild of the library. Not routed from a working set yet (§12–13, §35). Squid's question 2, `_baseline`: the store never keeps it (`import` and the working set skip it), so the size Squid measured is the round-trip law and the indexes, not a second copy (§36).
* **IMPORTANT — Void Hormiga's three open messages, built** (`hormiga-the-frontier`, `hormiga-el-lindero`, `hormiga-the-overview`, folded and deleted):
  - **A top anyone can browse.** `ClusterOptions::fanout` (default 250, a lean, §34) adds levels above Leiden's communities by Void Maiz's merge-bundle-slice steps, now `graph::bound_fanout` in the leaf target (an addition to `graph.hpp`, announced to Maiz). Packing is marked.
  - **The reseña:** per tree node, runes and internal and outgoing ties, and the ties between siblings, kept by write-back (link, unlink, delete, restore, filing a rune). `plan_test` checks write-back against a recount after seven kinds of edit.
  - **Small asks:** `clusters_of(names)` in one statement; `plan(..., focus_leaf)`; `find_names` by prefix or by part of a name; `matrix_view` and `write_state` find the always-loaded runes through a partial index (`clusters_always`) and a leaf through its range, so a 3-rune leaf no longer costs a scan of the mantle's cluster rows.
  - **The frontier:** `Selection::fringe` keeps every edge with one end held (dangling, as SPEC §3.7 allows), with a hub's beyond `fringe_cap` counted; `Store::frontier` gives los mojones (with whether each is a rune at all) and la cuenta (complete leaves). **The model-based walk passes with the fringe held, on both paths**: a new case beside the two, which are unchanged.
  - **El rodeo and el embarque** ([rodeo](/concepts/rodeo.md), new): a resumable pass over every rune and edge, a herd at a time, each edge once; and an export as content-named partidas plus a manifest whose hash names it. `rodeo_test`, the eleventh suite.
  - **The bundle's size:** measured on an 8×10⁴-rune store, 2.5× its JSON is the round-trip law (35 of 67.7 MB is Core's own text), tags, the row index and indexes. `pack_into` drops what every open rebuilds.
  - **A second connection:** fine on a local disk through WAL; refused on a network path, even in the same process ([store](/concepts/store.md)).
  - Waiting: the covering closure, passes for recursive questions, the reseña's ranges, and the two tests of the frontier, whose generators Hormiga offered to write in its `stressgen` (accepted).
* **Found while building:**
  1. **The planner, with no statistics, joined the reseña's pass quadratically** (`runes_by_pos` once per edge): `segments_test` ran six minutes instead of twenty seconds. The order is forced, as elsewhere in the store.
  2. **`set.insert({a, b})` with two JSON values picks `insert(initializer_list)`**, converting each string to a pair, and throws. A test's own bug; noted for anyone writing tests here.
  3. **The shell this agent runs collapses `\\` in heredocs**, which put a NUL byte into `tools/rancho.cpp` once. Scripts are now written as files.
* **Void Maiz's phone asks, built** (`maiz-graph-analysis-is-yours-and-phones`, part 2; folded and deleted): a phone build with no parent `voidcore` stops at configure, because Core builds only a shared library (Maiz asked Core for a static one); `OpenOptions::temp_dir`; `release_memory()`; `phys_footprint` on Apple; where each file goes and the one call on backgrounding ([tiers](/concepts/tiers.md)); the unused `Stmt::db_` gone; a CI (`.github/workflows/ci.yml`) with desktop builds and tests and Android and iOS compiles, **not yet run**.
* **Void Mago's note** (`voidmago-published-and-registered`) and **Hormiga's adoption** (`hormiga-adopted-and-your-exit-test-passed`) were already folded on 2026-11-02; deleted.
* **The root is clear.** Replies were written into each recipient's root (gitignored there): Squid, Hormiga, Maiz. Our earlier drafts that were never answered were moved the same way (Core: `touched`; Hormiga: the deed; Palabra: the line and federations; Bicho: a proposer; Allomone: two algebras; Maiz: graph moved). Drafts already answered were deleted (Hormiga: store library, blobs; Maiz: Leiden in place, all three taken).
* **The suite is 11/11.**

## 2026-11-06 (late night) — Is Void Allomone already the algebra?
* **The author asked:** *"Void Allomone might actually be our system of algebra already? Perhaps we should strengthen Void Allomone more such that it can truly be this algebraic step? Or is it worth distinguishing them?"* Answered in [the algebra](/design/the-algebra.md) §12, from Allomone's own bundle (start-here, foundations, composition, materialization).
* **Found:**
  - Allomone is **order theory over one document**: a derive-only annotation language whose independent sources are merged by join-semilattice laws, with a real tie surfaced as ⊤. Its foundations are one stratum of Datalog, CALM and a subsumption order. It never writes, and it refuses fixpoints for now (*"the hinge of the whole design"*).
  - Rancho's algebra is **category theory between databases**, and must both write (Σ, by commands) and iterate (the chase).
  - Making Allomone the algebra would break its two load-bearing properties, and its vocabulary-agnosticism.
* **Lean (§33): keep them distinct.** They are one idea at three levels, composition by law with disagreement surfaced:
  - Allomone: opinions about one database;
  - Palabra: one database's states over time;
  - Rancho: different databases.

  A lattice is a poset category and a join is a colimit, so the levels stack (§7). They meet at three data shapes:
  - scripts read through Δ, so one script serves many databases;
  - proposed mappings combined as Allomone sources, with ⊤ for a person to settle, which is Allomone's own *propose, do not generate*;
  - monotone trust policies as scripts over provenance tags.

  §32 is answered the same way: the full semiring is Rancho's.
* **Recorded:** the pair `voidrancho`–`voidallomone` in Void Mago's `family.json`, with status proposed and seam *data shapes*. Allomone is not a repository this agent may edit, so a message is drafted locally: `rancho-two-algebras-and-where-they-meet-2026-11-06`.
* **No code changed.**

## 2026-11-06 (night) — The algebra of databases; the line with Palabra
* **IMPORTANT — The author asked for actual algebra:** *"mappings as its own form of math, with operations… rancho should hold the logic for how to operate over these mappings"*; *"see how CQL works, how it can work with our structure"*; *"Having actual algebra would greatly benefit us."* Researched and designed in [the algebra](/design/the-algebra.md) (new):
  - **How CQL works:** typesides, schemas (finitely presented categories with attributes), instances presented by generators (labelled nulls), schema mappings, and the three migrations Σ ⊣ Δ ⊣ Π, with Σ computed by the chase. Integration is by pushout.
  - **Why Core is an easy case:** a Core document's schema (Mantle, Rune, one entity per glyph, Link, Tag, Binding) is a **finite acyclic category**, so every equation question is decided by enumeration, with no theorem prover. Core's rule that `spirit.id` must not be derived from content makes ids **labelled nulls** already. So "a move carries, a copy mints" is the algebra's rule (Σ carries generators and mints what the target requires), not ours.
  - **The laws:** the category laws; Δ composes backwards and Σ and Π forwards; **the round trip's unit measures loss**, and a fully faithful mapping loses nothing. That makes "a move back is checked, not assumed" exact.
  - **Commands are deltas** (DBSP's Z-sets). Δ is always linear, so a command crosses it by itself. Σ is linear when it merges nothing. Π needs state. So *"a mantle moves as the commands that would make it"* becomes a decision the mapping alone settles.
  - **The line with Void Palabra, as a law:** Palabra works inside one schema (instances and their join), Rancho between schemas (mappings and migrations). Σ and Δ preserve colimits, so wherever Palabra's join is a colimit, merging and mapping commute. ORCHESTRA splits on the same line.
  - **Provenance is a semiring** (N[X]); a trust policy is its evaluation.
  - **A holiday carries a schema** inside `describe()`'s capabilities. Core's shape is unchanged.
  - **AI is a proposer outside the algebra:** the propose, check and accept loop, with correspondences as data. Rancho may ship a deterministic baseline (Similarity Flooding). Models are a host's or Void Bicho's: MiniLM-class embedders, Magneto's small retriever, Unicorn, Ditto, a JEPA-style joint embedding.
  - **CQL cannot be vendored** (BSD-3 for non-commercial use only, or AGPL-3.0 for the Haskell implementation). The kernel the family needs is small and is written from the papers. Build order: §11 there.
* **The line with Palabra, made explicit** at the author's direction (*"rancho and palabra are very closely related. We should explicitly distinguish them… label in void mago the interaction"*). [Boundaries](/design/boundaries.md) §4 is rewritten. The pair is recorded in Void Mago's `family.json` through `mago pair voidrancho voidpalabra`, with status **proposed**, seam *content names*, what it shares, and the line. It is the only change in Mago.
* **Found:**
  1. **ORCHESTRA's *reconciliation* is Palabra's word** (its have/want protocol), and *publish* is Hormiga's and Mago's. The steps of update exchange are **consign** and **take delivery**, both free in every project. §26 and [after the dataspace](/design/after-the-dataspace.md) §2 are updated.
  2. **"Schema" collides inside Rancho**, with the store's SQL tables. Every alternative checked (*vocabulary*, *shape*, *sketch*, *signature*, *presentation*) collides somewhere, and *vocabulary* collides with "Core's vocabulary" on this very bundle. Kept *schema*, the research term, defined in [the language](/concepts/language.md) §7 as "in the algebra's sense, not the store's SQL tables".
  3. **The language page grew a "6½" heading**, the same mistake as the "5½" fixed earlier today. It is renumbered: the algebra §7, the laws §8, the words Rancho does not own §9.
* **Open questions:** §27 and §29 have the author's direction folded in. New: §30 (a schema across a holiday's door), §31 (the interface law with Palabra) and §32 (provenance and Void Allomone).
* **Messages (local):** to Void Palabra, `rancho-the-line-between-us-2026-11-06` (agree the line, mark the pair agreed, and is the join a colimit?); to Void Bicho, `rancho-a-mapping-proposer-2026-11-06` (a `propose` contract: schemas in, ordered correspondences out, the same shape as Squid's).
* **No code changed.** The kernel (schema, mapping, compose, check, Δ, then Σ with its loss) is the next rung.

## 2026-11-06 (evening) — After the dataspace: what came next, and what is stronger
* **The author asked:** *"Data space systems have existed for a long time, can you do some research to find if anything has come after it? Any newer novel idea? Have people suggested anything stronger?"* Researched in [after the dataspace](/design/after-the-dataspace.md) (new).
* **Found:** the dataspace (2005) was weak on purpose: best-effort queries, and nothing about edits, trust, history, proof, permission or cost. Six lines have strengthened it:
  1. **Collaborative data sharing** (ORCHESTRA, 2005–2013), the closest to the family. Edits cross through mappings as update exchange (publication, then reconciliation). Each peer curates what arrives, and trusts it by provenance (provenance semirings). The first round's *"a mapping translates commands"* is this design, reached independently.
  2. **Model management** (Bernstein et al., 2000–2007), an algebra of mappings: match, compose, invert, merge, diff. Its theory warns that composition can need more power than its parts, and that exact inverses rarely exist. So a move back must be checked.
  3. **Categorical data integration** (CQL): mappings that provably preserve constraints. So a mapping limited to a small language can be verified, not linted.
  4. **Lakehouse catalogs** (Iceberg, Nessie): one commit across many tables, as a snapshot per table. This confirms *"a version of a federation is a cut"*. Iceberg's own documentation says Nessie's branch merge *"isn't a true multi-table transaction"*, which confirms the saga lean.
  5. **Sovereignty:** the European data spaces (IDS, Gaia-X, the Dataspace Protocol at 1.0 under Eclipse, 2024) carry usage policies with the data. Local-first access control (Ink & Switch's Keyhive, 2024–) gives convergent capabilities with no central guard.
  6. **Incremental computation** (differential dataflow, DBSP, VLDB 2023 best paper): every view is maintained from changes. That is the strongest answer to *"nothing O(database)"* for views across participants, and the family is already made of changes.

  Also: foundation models now do schema and entity matching well (PVLDB 2022). Polystores, data mesh and Linked Data are weaker fits, recorded on the page.
* **Naming:** *data space* now usually means the IDS kind, sharing between organizations. We keep *dataspace*, written as one word as Franklin and colleagues did. [The language](/concepts/language.md) now says which one it means.
* **New leans:**
  - §26: crossings are update exchange, with local curation, provenance and trust policies;
  - §27: a mapping is a small checkable language, its inverse is checked, and a loss is recorded or refused;
  - §28: who may publish and reconcile is Palabra's, with capabilities that travel with the data;
  - §29: a model may propose a mapping, never apply one.
* **No code changed.**

## 2026-11-06 (later still) — The second round: copies, the dataspace, disbanding, agnosticism
* **IMPORTANT — The author answered §18–§24, the questions the first round raised.** Quoted in [beyond one database](/design/beyond-one-database.md) §1.2, and folded in:
  1. **Copies** (§18): a copy gets new ids, and a move carries them. The author wondered about a *"pointer" rune* in Core, for storage and efficiency, and expected *"an error prone mess"*. The research agrees about the mess: OLE links, spreadsheet references to other workbooks and symbolic links all break when the far side moves. A pointer asks for three things, and each is already answered without a new rune (§2.1): storage by content addressing beneath identity, meaning by an id in data that may dangle, and a live view by Core's query-backed mantle (SPEC §10.2). Lean: no pointer rune. The shape of a cross-database reference stays open.
  2. **The session** (§19): *"More like a "session" rather than a database… I believe this already exists, please research into the name."* It is a **dataspace** (Franklin, Halevy and Maier, SIGMOD Record 2005; PODS 2006): participants and the mappings between them, added pay-as-you-go, with no control over any participant. It fits as residency, while its mappings and history are written in its **federation**, a database, because a mapping must be replayable. So the first round's D is the dataspace's record, and the session is the dataspace (§4.1–4.2). The names were checked against every project in the family (§4.7): *session*, *workspace*, *join*, *merge*, *attach*, *dissolve* (Core's, for a box boundary), *profile* (Maiz's and Palabra's person profile), *member* (Hormiga's people), *unlist* (Hormiga's unlisted pages), *minutes* and *withdraw* are taken. The new words are **dataspace** (*la feria*), **enlist**, **discharge**, **disband** and **the history** (*el acta*).
  3. **Disbanding** (§20): *"a sort of function that returns a small micro database of the history… optional."* `disband(federation, keep_history)` changes no participant, and may return the history: a fork with its own id, holding what crossed participants and the mappings, and no participation (§4.6).
  4. **Overlapping groupings** (§21): *"IDs for every cell… each row is a mantle? But also each column is a mantle?"*, and the CSV was an example, not a format to build. It is **one owner, many views**: a rune lives in one mantle (SPEC §3.4), so a cell is a rune with a minted id, one grouping owns it, and every other grouping is a tag axis and a query-backed mantle (§5.4). The key-column lean is withdrawn, because it derived identity from content. No adopted format is planned.
  5. **Agnosticism** (§22): *"rancho would be completely agnostic to anything… rancho doesn't care if something is even real, so long as it somehow goes into a mantle."* The question had only ever been about the deed file's own fields. Now a deed holds Rancho's members only, and whatever a host says about its database belongs in a mantle. A member Rancho does not know is passed through and noted, and a newer deed is read and never rewritten. That answers PNG's safe-to-copy problem by not editing (§5.5). It is law 10 of [the language](/concepts/language.md).
  6. **Profiles** (§23): *"A database exists on its own."* A database declares no kind, so this is structural typing, not nominal: a host checks what it relies on rather than trusting a label (§5.2). The file type (`.miga`, `.ink`) stays the host's. Closed: it is not Rancho's question.
  7. **Core** (§24): *"void core should have little database things, cuz we have rancho for that."* There is no database holiday and no message to Core. Closed.
* **Built:**
  - **The deed's `profiles` member is removed.** It was added earlier the same day and never released. `rancho::Profile` became `rancho::Expectation {name, builds_on, check}`, given in `DatabaseOptions::expect`, and `conforms`/`profile_check` became `meets`/`expectation_check`. An expectation that builds on one the host did not give does not hold, and a cycle holds nothing. `rancho check` no longer prints "a kind of".
  - **A newer deed is never rewritten:** `write_deed` and `embed_deed` refuse it, and so `rancho ids` does too.
  - **A member Rancho does not know, in its own format, is noted** ("is not Rancho's… belongs in a mantle") and passed through.
  - `deed_test` promises 1 and 14 are rewritten. Rancho's suite is 10/10.
* **Found:** **"unlist" was nearly a second meaning.** It was free in Core, Maiz and Palabra, but Hormiga has unlisted issues and pages, which a grep across every project caught before it was used. The pair is *enlist / discharge*. The same check caught *member*, which in Hormiga is a person.
* **New question §25:** a dataspace that is never saved. Lean: it still has a federation, in the cache folder, disbanded at close. Closed: §21, §22, §23, §24. Ruled, with something left open: §18 (the reference's shape), §19 and §20 (Palabra's view).
* **Messages (local):** the drafts to Void Palabra (`rancho-federations-and-their-history-2026-11-06`) and to Void Hormiga (`rancho-the-deed-2026-11-06`) are updated: the dataspace, `disband` and its history, and a database that declares no kind.

## 2026-11-06 (later) — The author's four answers: ids, mappings, federations, profiles
* **IMPORTANT — The author answered four of the deed's questions, and asked for research before more work.** Quoted and researched in [beyond one database](/design/beyond-one-database.md) (new):
  1. **Names** (§1): *"just make sure we're mostly indexing with the ID or Spirit."* Names of either kind are labels. Nothing refers by one: the database, its parts, its mantles and its runes are referenced by id. This is law 9 of [the language](/concepts/language.md). Closed.
  2. **Moving a mantle** (§12): a move between files needs *"a holiday to actually map them together"*, because nothing about the other file can be assumed, *"even if they are both miga or ink files"*. The author asked whether the holiday should become something larger. The lean is a **mapping**, a dictionary riding on Core's holiday, which stays the door. It translates **commands**, as Cambria's edit lenses translate patches, so the railguard holds on both sides. Inside one database, the identity mapping is proved before a move, never assumed. Whether Core should gain a *database holiday* is new question §24.
  3. **Several databases** (§13): *"another database D that contains A, B, and C on a mantle. Mostly for palabra to keep track of edits… Then can always dissolve database D."* This is a **federation** (Sheth and Larson's term, defined by member autonomy). It holds no member's rows, every edit lands through the member's own dispatcher, and a change across members is a saga. A version of D is a cut across its members. This answers *"a deed as a part"* (one database, many files §7.7): a database of databases is a database, not a deed. §14 is refined: what is authored is data, and where bytes sit is configuration.
  4. **The file type** (§15): `.rancho` is *"like, a parent class almost"*, with `.miga` and `.ink` as children and *"any filetype could be a hybrid of a rancho"*. These are **profiles**, on the precedent of ISOBMFF brands, UTI conformance, OPC (docx, xlsx and pptx under one package format), SQLite's `application_id` (GeoPackage), Frictionless profiles and RIFF. A reader that knows no profile reads the file as the parent. Children are **born** or **adopted**: an adopted file keeps its bytes, and a mapping presents it in Core's vocabulary. A **hybrid** carries its deed in its format's channel for what a reader may ignore.
* **Built:**
  - **Mantles are named by key** (`Store::MantleRef`, `mantle_refs`; `Deed::part_of(MantleRef)`). The key is Core's `mantle.id`, or `name:<name>`. A key match in any part beats a name match, a name is read and noted, and `Database::deed_by_id` / `rancho ids` rewrite a deed with keys. `write_mantles`' filter takes the key. That API was added this morning and never released, so this is not a rename.
  - **Profiles:** the deed's `profiles`; `rancho::Profile {name, extends, check}`; `DatabaseOptions::profiles`; `Database::conforms` and `profile_check`. Parents' checks run first, each once.
  - **A store that carries its own deed** (`embed_deed`, store meta `rancho.deed`; `rancho embed`). `Database::open` takes a store, its embedded home is whichever file carries it, and a store with no deed opens as a database of one part.
  - Three new promises in `deed_test` (15 now). The suite is 10/10. The tool was tried on a scratch database: `new`, a profile and a blobs part, `check`, `embed`, then `check` of the store alone, with the deed file deleted.
* **Found:**
  1. **Naming mantles by name lost a renamed mantle.** Core's `mantle rename` keeps the id, but a deed that named the mantle no longer matched it. It fell to the home, which did not hold it, and left the document with nothing deleted. `deed_test` reproduces it on the by-name deed and shows the by-key deed keeping it. It is the case for answer 1, found by building it.
  2. **PNG's safe-to-copy bit marks a flaw in "unknown members are kept".** That is right for reading and wrong after an edit, since a newer member may depend on what changed. The lean is new question §22.
  3. **Cross-file atomicity is not available where it would be wanted.** SQLite's `ATTACH` is atomic across files only outside WAL, and members on other machines have no `ATTACH`. So a change across a federation's members is a saga of commands. Found reading the SQLite documentation, not in a test.
* **New questions** (§18–§24), each with a lean:
  - carry a rune's id on a move, mint on a copy;
  - where a mapping lives;
  - what dissolving a federation keeps;
  - identity for a CSV row;
  - unknown members after an edit;
  - who names a profile;
  - a *database holiday* in Core.

  §1, §12 and §15 are ruled; §13 and §14 are refined.
* **Drafted (local):** to Void Palabra, `rancho-federations-and-their-history-2026-11-06`: what the federation needs from Palabra's history. The message to Hormiga (`rancho-the-deed-2026-11-06`) now also says that `.miga` would be a profile and that a store can carry its deed.

## 2026-11-06 — The language, and the deed: one database, many files
* **IMPORTANT — The author's ruling on what Rancho is for:** *"Rancho should primarily have shared language on how databases are made. And then secondly have the tools to make them."* And the ask: *"a BASE file, maybe I should call it like ".RANCHO" ... an abstract configuration for storing a database ... what if a single database gets split between multiple files? How might hormiga use it, vs void squid, vs other future software?"* The author also opened this repository to direct work from Hormiga's agent, kept apart from Hormiga's own work.
* **The language** ([language](/concepts/language.md), new, first in "Read this first"). A database answers five separate questions: identity, record, derived data, placement and residency. Only the record is authored, and only through Core. The words for the whole and its parts, for what is in hand (the frontier's), for how sure an answer is, and for passes and exports. Each has a descriptive name for code and the ranch name offered for the author: Hormiga's set of 2026-11-03, plus *escritura, parcela, casa, hierro, servidumbre, mejoras, manantial, contramarca, fardo*. Eight laws, with the tests that hold them. [Open questions](/design/open-questions.md) §1 records that the ranch names are used beside the descriptive ones until the author rules.
* **The rationale** ([one database, many files](/design/one-database-many-files.md), new):
  - **why one database becomes many files:** size, media, lifetime, ownership, writers, carrying;
  - **the main idea:** the frontier at every scale. A part that is not reachable is a region not loaded, exactly as a cluster not in the corral is;
  - **the clean cut Void Core gives:** links are rune to rune within one mantle (SPEC §3.7), so a split by mantle cuts no edge;
  - **cases:** Hormiga, Squid, Loops Studio, a world-model corpus, the endless and interwoven test databases, a small device;
  - **ideas past the first form, in order of confidence:** shared parts, overlays, generated parts, a deed as a part, and the deed as runes (declined).
* **Built: the deed, first form** ([deed](/concepts/deed.md), new).
  - **The file:** `include/rancho/deed.hpp`, `src/deed.cpp`. A `.rancho` file is JSON: format, id, name, home, and parts in three forms (store, segments, blobs). Each part has a path relative to the deed's folder and the mantles it holds; a pin is optional and applies to segments only. Unknown members are kept.
  - **`rancho::Database`** opens every part it can. Each part is `present`, `missing`, `not openable` or `mismatch`, with a reason. The database opens whatever is absent, and `complete()` says whether anything is. It writes the document across parts, row by row. Declared glyphs and domains are the union, with the home's winning. A segments part is read through a restored copy in the cache, named by its manifest's hash. The home records its database's id, so a store is never taken as a second database's home.
  - **The tool:** `rancho new | deed | check` (`tools/rancho.cpp`, option `RANCHO_BUILD_TOOLS`).
  - **Store, additive:**
    - `write_mantles`, which `write_state` is now written through, with the same bytes;
    - `mantle_names`, `top_keys` and `top`;
    - `leaves`' comment now says it is exact and slow, and points to `leaf_estimates` (Hormiga's 2026-11-05 finding).
  - **`deed_test`, 12 promises.** Among them the **placement law**: one part or three, including a segments part restored through the cache, give the same document, and Core reads it as the whole one.
  - **The suite is 10/10.** The model-based write-path test and the query-parity fuzz pass on the refactored `write_state`.
* **Found while building:**
  1. **A part of an unknown form must still own the mantles it names.** Found by trying the tool. If it does not, a stale copy in the home is read in its place. Now such a part holds what it names and is never the part that holds the rest, and the test covers it.
  2. **The home guard works on the tests that forgot it.** Every test deed minted a new id over the same home store, and the store refused the second database. A re-placed database keeps its id. The tests now do too.
  3. **A loop over a temporary's member reads freed memory.** In C++20, `for (auto& m : json::parse(t)["mantles"])` destroys the parsed object before the loop begins (C++23's P2718 would extend its life). The test's helper silently looped over nothing. The library holds every parsed object in a variable.
* **Read, not yet answered:** Hormiga's `hormiga-the-frontier-2026-11-03` (issue #2), `hormiga-el-lindero-2026-11-03` (issue #3) and the local draft `hormiga-the-overview-2026-11-05` (upper cluster levels, the reseña, measured costs). They are now the first item of [the roadmap](/roadmap.md)'s next steps. The language takes their vocabulary, and the deed takes the frontier's first rule up one scale. Their files stay at the root until answered. `hormiga-adopted-and-your-exit-test-passed-2026-11-02` was folded on 2026-11-02; its file is still at the root.
* Drafted for Hormiga (local): `rancho-the-deed-2026-11-06`. It says what is new, that nothing has to change on its side, and asks two questions only Hormiga can answer: is `.miga` v4 a bale (§16), and can its sync id be the deed's id (§17)?

## 2026-11-02 (later) — Hormiga adopted us; graph analysis moved here; blobs can be dropped
* **IMPORTANT — Void Hormiga adopted Rancho, and the rung-3 exit test passed** (`MESSAGE_FOR_VOIDRANCHO_hormiga-adopted-and-your-exit-test-passed-2026-11-02`, folded here and deleted). Hormiga's library keeps no copy of the store: its `src/store` and those tests are deleted, since ours run them. Its one SQLite is ours (`sqlite3` aliases `rancho_sqlite3`), and its working sets take the app's Core through `maiz::RanchoLink`. The `.miga` v4 bundle stays Hormiga's, written on our API: `vacuum_into`, then `purge` and `put_blob` on the copy; `blob_paths` and `blob` on open. Hormiga's app does not use a store yet; its move (Hormiga's stage 5) restarts on us. **The 2 GB harness** (`tools/lowmem`, a 512 MB cap, planned for a 2 GB machine) on the 5×10⁵-rune, 8.4×10⁵-edge stress database: import 32.4 s at a 13 MB peak (Hormiga's own took 20.5 s; the difference is our typed fields and tag sources); cluster 11.8 s, 25 998 leaves, Q 0.977, 79 MB peak; plan + load 3.3 s, 9 276 of 500 000 runes; an edit costs 50.9 ms in Core and 0.5 ms writing back. **The whole process peaked at 290 MB under the 512 MB cap and exited 0.** The NAS question (UNC paths or mapped drives) is with the author; until Hormiga's working copy is a store, none of its clients is on WAL through us.
* **Blobs can be dropped** (Hormiga's ask 1). Opening a v4 bundle writes its pictures beside the working copy, and the copied store kept them in `blobs` as well, so every picture was on disk twice. `Store::remove_blob(path)` (true if there was one) and `Store::clear_blobs()` (how many) change no rune, edge or tag, because blobs are not part of the document. **Found on the way:** dropping rows frees pages inside the file but does not shrink it, so the ask would not have saved a byte on its own. `Store::compact()` (SQLite's `VACUUM`) gives the space back. It rewrites the whole file, so the host decides when to pay for it. Test: `store_test` "a blob can be dropped and its space given back" (four 1 MB blobs; the file does not shrink on dropping and shrinks by more than 3 MB on compacting; the document is unchanged). [Store](/concepts/store.md).
* **IMPORTANT — Graph analysis is Rancho's** (Hormiga's ask 2; Void Maiz's ask 1 in `MESSAGE_FOR_VOIDRANCHO_maiz-graph-analysis-is-yours-and-phones-2026-11-02`). **The author ruled on 2026-11-02:** *"graph analysis code should live in void rancho"*. That closes [open questions](/design/open-questions.md) §3 and Maiz's Q43. It moved with the names Maiz's bridge keys on. `include/rancho/graph.hpp` holds namespace `rancho::graph`. `src/graph/graph.cpp` is Maiz's, unchanged except for its namespace. The target is **`voidrancho_graph`**: STATIC, PIC, C++20, linking nothing. `cmake/voidrancho_graph.cmake` is guarded and stands on its own. `tests/graph_test.cpp` is Maiz's `graph_smoke` split into seven promises, and it links the leaf target alone. `voidrancho` links it PUBLIC. `RANCHO_HAVE_LEIDEN`, the `graph.cpp` fallback and `VOIDMAIZ_ROOT` are gone, and so is Maiz from `void.json`'s `requires`. Clustering's recorded settings now say `"implementation": "rancho/graph.hpp"`. The matrix view's PageRank and components are kept beside these: the matrix view asks the directed, semiring forms, and clustering reads the undirected, weighted ones. **Checked:** our suite is 9/9. Void Maiz configured on its own beside us reports `voidmaiz_graph: Void Rancho (voidrancho_graph)`, its `graph_smoke` and `rancho_link_smoke` pass on our target, and exactly one archive in that build (`libvoidrancho_graph.a`) defines `rancho::graph::leiden`.
* **Phones** (the author, relayed by Maiz: *"void rancho should ready itself to work for ios and android as well"*). Maiz built `voidrancho` and our 8 tests with NDK r28.2 (`android-26`, arm64). They compile and link unchanged but have not been run. Its only warning was the unused `db_` at `src/sql.hpp:65`. `media.cpp`, `segments.cpp` and `memory.hpp` read correctly on both phones. **Not yet done, recorded in the [roadmap](/roadmap.md):**
  1. On a phone with no parent `voidcore`, refuse to build Core's shared library. Adding Rancho first, or alone, gives Android a `libvoidcore.so` beside the app; Maiz asked Core for a static phone build.
  2. SQLite's temp files. Our `temp_store=FILE` has nowhere writable on Android outside Maiz's shell, which sets `TMPDIR`; iOS sets it itself. The fix is a doc line or an `OpenOptions::temp_dir`.
  3. Which of our files belong in `data_dir` and which in `cache_dir`, plus the call that makes a working set durable when the app is backgrounded, and how long it takes at 5×10⁵ runes.
  4. Optionally, `phys_footprint` on Apple (what jetsam counts) and a `release_memory()` for low-memory events.
  5. A note that `locking_mode=EXCLUSIVE` inside an iOS App Group container gets a suspended app killed (`0xdead10cc`).
  6. Now that we are public, a CI with an iOS compile and an Android NDK compile.

  The Maiz message stays at the root until these are answered.
* Replies drafted (local) to Hormiga (blobs, the fragment) and Maiz (moved, with its names).

## 2026-11-02 — beside Void Maiz in one build
* **Void Maiz's reply folded in** (`MESSAGE_FOR_VOIDRANCHO_maiz-one-core-one-leiden-and-a-link-2026-10-31`). Maiz built the case Rancho's first client will be in: one superproject that adds both libraries (Void Hormiga, once it moves its store). Three things broke or nearly broke there, and Maiz fixed its side of each. Rancho's side, in `CMakeLists.txt`:
  1. **`VOIDCORE_ROOT` in either spelling.** Rancho reads it as Core's repository, Maiz and Squid as Core's `core/` directory, and one cache variable cannot satisfy both. Rancho now accepts both, shadowing the cache in its own directory only, and skips the existence check when a parent already has a `voidcore` target. Maiz now wraps Core's prebuilt as an imported `voidcore`, so the guard sees it: **one process, one Core**.
  2. **Leiden linked once.** `graph.cpp` compiled into `libvoidrancho.a` put a second `maiz::graph::leiden` on a host's link line, and the linker silently kept the first. Rancho now links Maiz's `voidmaiz_graph` target (`VoidMaiz:cmake/voidmaiz_graph.cmake`, defined once per build, in any order), and uses a parent's target without needing `VOIDMAIZ_ROOT`. Compiling in place remains only for a Maiz older than the fragment. Maiz records `graph.hpp` as a contract with Rancho.
  3. **`RANCHO_BUILD_TESTS`**, on by default only when Rancho is the top-level project, so a host's ctest does not inherit Rancho's eight tests unless it asks. This differs from Maiz's patch, which defaulted to on.

  Checked in a scratch superproject: both add orders × both spellings configure. In every build there is one `voidcore` (Maiz's imported prebuilt when Maiz comes first, Core's source target when Rancho does) and exactly one archive defining `leiden` (`libvoidmaiz_graph.a`). `plan_test` passes in both orders, `working_set_test` (the model-based guard) passes against the imported prebuilt, and Maiz's `rancho_link_smoke` passes. The standalone suite is 8/8.

  **The adapter is Maiz's:** `maiz::RanchoLink` (`voidmaiz/rancho.hpp`, header-only) makes a `maiz::Core` a `CoreLink`. Its export is the host's per-revision cached one, and its revision does not move on a sync's reads. Recorded in `rancho/core.hpp` and [write path](/concepts/write-path.md). `VcCore`'s read list gains `related` and `bindings` (both read-only in Core's `verbs_graph.c`; Maiz's list already had them).

  **§3, where graph analytics lives:** Maiz's lean (its Q43) agrees with Rancho's, on two conditions: a leaf target with no SQLite and no Core, and `graph.hpp` forwarding until hosts move. Recorded in [open questions](/design/open-questions.md) §3 and [boundaries](/design/boundaries.md) §3; the author's to rule. Maiz also reports that Void Hormiga's `HEAD` does not compile on its own (`src/app/app.cpp` against `save_writer.hpp`). That is Hormiga's, unrelated to the move.

  Reply drafted for Maiz (local). Seen while testing, not Rancho's: Maiz's executables link libstdc++ dynamically and exit 127 on a PATH that puts msys2's `mingw64` before `ucrt64`. Told to Maiz.

## 2026-10-30 (later) — 0.1.0
* **IMPORTANT — Built: rungs 1–5 and the first form of rung 6 (0.1.0; [building 0.1](/design/building-0.1.md)).** The author's direction: begin full development, copying Void Hormiga's database work, with the family in mind. A C++20 library with a C ABI, linking Void Core 0.2.15 at its C ABI and compiling Void Maiz's `graph.cpp` in place; SQLite 3.53.3 and nlohmann/json 3.12.0 vendored from Hormiga. 8 test suites, all green on windows-x64.

  **(1) The store** ([store](/concepts/store.md)): Hormiga's `src/store` extracted with its API and meanings intact (`Store`, `Selection`, `JsonStream`, `Plan`, `WorkingSet`), then generalized — edge `weight` as float64 with `directed` and `i:j` ports as columns; a `source` on every tag; the persistent row index ρ, kept across imports and never reused; typed columns for fields a glyph annotates in `kinds` (numbers, numeric strings, ISO dates as seconds) with range queries; blobs; format 2, and **a format-1 store Hormiga wrote migrates in place on open**, its document unchanged. Memory flat: +10.0 MB importing 2×10⁴ runes, +10.4 MB at 8×10⁴.

  **(2) Queries** ([query](/concepts/query.md)): SPEC §5 requires hosts to evaluate through `vc_tag_match`, so compiling the grammar to SQL (this bundle's original plan) was dropped — SQL narrows to a superset from the expression's AND/OR shape, Core decides every candidate. **Parity with `ls --tag` on 3 000 random expressions** built for the grammar's corners. A query by names costs 0.19 ms at 2×10⁴ and 8×10⁴ runes — after fixing a planner trap that made it scan the mantle (13× at 8×10⁴).

  **(3) The write path** ([write path](/concepts/write-path.md)): Hormiga's `WorkingSet` over a `CoreLink` interface (any host Core; `VcCore` the default) instead of `maiz::Core`, so a database library does not depend on a view library; the working set's **actor** is the source of the tags it writes. Hormiga's model-based test passes: 399 random commands per walk, the store equal to a whole-database Core after each, 268 through the targeted path.

  **(4) The working set and clusters** ([working set](/concepts/working-set.md), [cluster](/concepts/cluster.md)): budget and plan ported unchanged; `ClusterOptions` makes weights first-class — one relation read weighted at a time (mixing is refused: not commensurable), positive weights only, settings recorded beside the leaves. A test plants a decoy partition: unweighted Leiden finds the decoy (NMI 0.001), weighted finds the truth (NMI 1.0).

  **(5) The matrix view** ([matrix view](/concepts/matrix-view.md)): ρ, one CSR `A_r` per relation, V (entity × measure, with units), T (with sources), X (by measurement level: interval centred, nominal as categories); a minimal CSR with semiring products — BFS, (min,+) shortest paths, PageRank, components, A·X — each checked against a textbook implementation on hundreds of seeded graphs; a `.npy` + manifest training export, verified by loading it with numpy 2.5.

  **(6) Tiers and segments** ([tiers](/concepts/tiers.md), [segment](/concepts/segment.md)): a store on a network path opens in rollback mode under an exclusive lock held until close, and a second machine is refused with a reason; snapshots are SHA-256-named segments (one per cluster leaf) + a manifest created exclusively + an atomically swapped `ROOT` — restoring byte for byte with provenance, clusters and rows; one edit after a 12-segment snapshot writes 1 segment; a lost commit race replaces nothing; a corrupt segment is refused; 6×10⁴ runes snapshot and restore in 3.4 s each with flat memory.

  **Found on the way:** MinGW's `std::filesystem::absolute` turns `\\nas\share\x` into `C:\nas\share\x` — a first draft of the network check would have opened every NAS store in WAL, the exact failure the research warns of; caught by `media_test`, told to Hormiga. **Leans taken to build**, provisional until the author rules: [open questions](/design/open-questions.md) §4–§7 (and two new, §10–§11). **Not built:** transitions (wait on Core's `touched`), the bucket adapter, embeddings, Lance, a Python binding. `void.json` now `alpha` with its real `requires`. Messages drafted for Hormiga (adoption + the NAS finding), Core (status; `touched`), Maiz (`graph.cpp` compiled in place).

## 2026-10-30
* **IMPORTANT — Founding: the database becomes a pillar of the Void family.** The author's ruling (`VoidCore:/design/the-transition.md`, pillar B): every application on Void Core built its own database — Void Hormiga's SQLite store and cluster-paged working set (2026-10-27, after a 5×10⁵-rune database peaked at 22 GB), Void Squid's `assets.db` copying Hormiga's schema — and the database belongs in a dedicated library one layer above Core, which stays pure. Named **Void Rancho**. Distinct from Hormiga's Antfarm (connectors to external APIs); Rancho *is* the database. Founding direction: generalize Hormiga's store; clustering and float edge weights first-class; a **matrix view** of a mantle for JEPA-class world models (pillar A); scale, speed, network drives, NAS and object storage. Bundle written: [boundaries](/design/boundaries.md) (railguard: *could every row be rebuilt by replaying commands?*), [founding](/design/founding.md) (survey of the family's databases), [storage research](/design/storage-research.md) (SQLite must not be shared across hosts on a network drive; S3 is flat and the graph lives in table-format metadata; Iceberg's manifest/pointer-swap commit, now possible on S3 itself; Lance; DuckDB; Kùzu archived 2025-10-10, forks; GraphBLAS; Litestream), six concepts, [open questions](/design/open-questions.md), [roadmap](/roadmap.md). Registered with Void Mago (`void.json`). Nothing is built.


# Authority: which file is in charge (2026-11-18)

From Void Hormiga's author, after a person who opened a `.miga` kept meeting a
`demo-org.json`: *"they should always see and deal with the miga file ... If for
some reason the json file gets deleted, oh well ... the miga file would be almost
impossible to replace ... this structure on how files are stored and what is
really in charge belongs to void rancho to set a foundation first."* Permission to
edit Rancho was given with it.

**Ruled and written down** ([authority](/concepts/authority.md)): four kinds of
file (authority, cache, keepsake, pointer) and seven rules. A person meets only
the authority; writes reach it; a cache is one per authority FILE (two copies of
one database are two files); a cache's note says how it stands from the
authority's stamp alone (new, current, stale, ahead, diverged, orphan); losing a
cache loses nothing saved; keepsakes live per user outside the cache; pointers
are not copied in, and relative ones are relative to the authority.

**Built:** `rancho/authority.hpp` (`cache_root`, `keep_root`, `working_copy_for`
with an index of normal paths, `read_note`/`write_note`, `cache_state`,
`mark_ahead`, `mark_matched`, `reassign` for Save As, `forget`, `caches`).
`tests/authority_test.cpp`, six promises. Suite 12/12.

**Answered:** open questions 16 (the `.miga` is Hormiga's bale and its
authority, in its own form) and 17 (the authority carries the id; a cache is
keyed by the file). The language gains *authority*, *cache* and *keepsake*.

This entry sits beside the uncommitted 2026-11-09 session in the same working
tree; neither touches the other's files except `CMakeLists.txt` (one source,
one test name), the index, the language page and the open questions.
