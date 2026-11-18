---
type: Design
title: Beyond one database — ids, mappings, dataspaces and file types
description: "The author's answers of 2026-11-06 on the deed, in two rounds, researched. Everything is indexed by id or spirit; a copy gets new ids, and a pointer is not a new kind of rune. A mantle that moves between files needs a mapping, because nothing about the other file can be assumed. Several databases are worked on together in a dataspace, a live session whose mappings and history are written in a federation database, which can be disbanded and may hand back a small database of its history. A file type is built on the rancho, but a database declares no kind, and Rancho is agnostic about everything that goes into a mantle. Void Core stays small: database things are Rancho's. The prior art, how it fits, what is built, and what is still open."
tags: [status:current, audience:dev, confidence:exploratory, foundation]
timestamp: 2026-11-06T00:00:00Z
---

# 1. The answers

## 1.1 The first round

The author, 2026-11-06, answering [open questions](/design/open-questions.md) §1, §12, §13 and §15:

> 1. *"For names just make sure we're mostly indexing with the ID or Spirit."*
> 2. *"Moving mantles between files would require a holiday. Moving a mantle from one file to
>    another means we can't assume much at all about the other file. Even if they are both miga
>    or ink files. We can't assume that we will be able to do a clean and meaningful placement
>    into the other database without a holiday to actually map them together. This makes me
>    think that the holiday component should be something else entirely? A larger abstracted
>    holiday structure to map databases together? (Of course, a database itself will likely
>    contain holidays inside it as well)."*
> 3. *"When operating on multiple files or multiple databases, there will likely need to be
>    another database to track the sub databases. For example, if we have databases A, B, and
>    C. We want to frequently edit them. We would need another database D that contains A, B,
>    and C on a mantle. Mostly for palabra to keep track of edits and such. Then can always
>    dissolve database D and have A, B, and C separate again (if we need to)."*
> 4. *"Rancho file type is supposed to be like, a parent class almost? Other file types like
>    miga and ink will inherit properties from the rancho file and build themselves. (Each type
>    will be needing its own unique stuff). Theoretically, any filetype could be a hybrid of a
>    rancho if done well enough. Like a csv file, docx file, etc, could all be restructured to
>    be stored as a child of a rancho file."*
>
> *"Let's generally do some more research into these concepts."*

## 1.2 The second round

The same day, answering §18–§24, which the first round raised:

> 1. (§18) *"For copies I feel like they should be different IDs. However, maybe this is a thing
>    for void core, something similar to a "pointer" rune. Unsure how to handle this? Because
>    honestly, there's value in referencing another rune, not copying its data entirely (like
>    for lower storage or higher efficiency). However, practically, I feel like it'd be an
>    error prone mess."*
> 2. (§19) *"I guess maybe we need another type of database? More like a "session" rather than
>    a database? An active dynamic 'thing' where we are storing mappings and such? I believe
>    this already exists, please research into the name this already has, and see if it fits in
>    our structure. Also make sure the names, nouns, and verbs don't overlap."*
> 3. (§20) *"Dissolving a federation can be a sort of function that returns a small micro
>    database of the history? And this can be optional to return."*
> 4. (§21) *"For csv files in general you're going to have to think very abstractly. IDs for
>    every cell, and in a way, each row is a mantle? But also each column is a mantle? Lots of
>    matrix overlap… This is just an example though of how a database COULD be stored… not
>    necessarily that I want those filetypes specifically to be turned into something."*
> 5. (§22) *"How does rancho determine what is recognizable? I feel like rancho would be
>    completely agnostic to anything. Maybe hormiga would need to recognize specific things, but
>    rancho doesn't care if something is even real, so long as it somehow goes into a mantle."*
> 6. (§23) *"That feels like a question that isn't for rancho? Unless if you feel a profile is
>    inherent to a database, but I don't think it is. A database exists on its own."*
> 7. (§24) *"I think void core should have little database things, cuz we have rancho for
>    that."*

## 1.3 The short version

- **Ids** make everything addressable. A copy gets new ones. A pointer is a query or an id in
  data, never a new kind of rune (§2.1).
- **Mappings** translate between vocabularies, as commands (§3).
- **A dataspace** is the live session in which several databases are worked on together. What it
  authors, its mappings and its history, is written in a **federation**, a database. Disbanding
  changes no participant and may hand back a small database of the history (§4).
- **A file type is built on the rancho**, but **a database declares no kind**: a host brings its
  own expectations. **Rancho is agnostic** about whatever goes into a mantle (§5).
- **Void Core stays small.** Every word on this page is Rancho's, and the holiday stays Core's
  door (§3.5).

The mapping is still the hinge: a move, a dataspace and an adopted file all need it.

# 2. Identity first

Void Core already has an id at every level it owns, minted once and frozen. A rune has
`spirit.id` (SPEC §3.1), a mantle has `mantle.id` (§3.4, minted by `mantle new`), and a binding
has `bind_…`. Every one of them **keeps its id when it is renamed**. A name is a handle for a
person, unique only where it lives: a rune's name within its mantle, a mantle's name within one
document. Rancho adds the two levels above:

| level | its id | minted by | a name for a person |
|---|---|---|---|
| database | the deed's `id` | `mint_database_id` (or a host's own, §17) | `name` |
| part | the part's `id` | the deed's author (unique in the deed) | none |
| mantle | `mantle.id` | Void Core | `mantle.name` |
| rune | `spirit.id` | Void Core | `spirit.name` |

**What changed.** The first deed named a part's mantles by name, and that was a real bug. Here
is how it happens:
1. A mantle in a second part is renamed, as Core renames: the id stays and the name changes.
2. The deed no longer names it.
3. It falls to the part that holds the rest, which does not hold it.
4. So it leaves the document, and nothing was deleted.

`deed_test` reproduces it. **A deed now names a mantle by its key**: Core's `mantle.id`, or
`name:<name>` for a mantle written without one. That is the key the store's row index already
used (`Store::mantle_key`). The rules:
- A key match wins over a name match.
- A bare name is still read, so a hand-written deed works, and the database notes it.
- `rancho ids` rewrites the names in a deed as keys.

**The rule, as a law of [the language](/concepts/language.md):** anything that crosses a file
boundary refers by id. A name may be shown, and may be typed and resolved at once, but it is
never stored as a reference outside the document that keeps it unique.

## 2.1 Copies, moves and references (second round, answer 1)

**Ruled:** a copy gets new ids. A move carries them: it leaves one rune in the world, and Void
Palabra's join may later meet the same rune from both sides. A copy leaves two, which must not
collide when the databases meet again. The copy records where each rune came from, as data its
commands write (the mapping's *ids* row, §3.3).

**The pointer.** The author sees the value (*"lower storage or higher efficiency"*) and the
danger (*"an error prone mess"*). Both are real, and the history of pointers across files says
which part is dangerous:
- OLE's *linked* objects, a spreadsheet's references to another workbook, and symbolic links
  all break the same way. **A reference that is followed automatically turns every move,
  rename or delete on the far side into breakage on this side**, and the reader finds out
  late (a `#REF!`, a missing picture, a dangling link).
- Ted Nelson's transclusion avoided that only by making everything addressable forever and
  nothing deletable, a price no database here can pay.

A "pointer" asks for three different things, and the family already answers each of them
without a new kind of rune:

| what the pointer is for | the answer | where it lives |
|---|---|---|
| **storage**: not keeping the same bytes twice | **content addressing, beneath identity.** A copy is a new rune with new ids, but its bytes need not be new. Rancho's segments are content-named, and Palabra's blocks are content-addressed, so identical content is stored once whatever ids point at it. Nothing can dangle, because nothing in the model points | Rancho's segments and Palabra's blocks today. A store part does not yet share bytes between rows |
| **meaning**: "this is that one" | **an id in data.** Inside one document Core already has links (SPEC §3.7) and bindings (§3.6), which may dangle. Across databases, a reference names three ids (database, mantle, rune). It may dangle too, and absent is not deleted (law 4) | the referring rune's content, written by commands |
| **a live view**: seeing another's runes as they are now | **Core's query-backed mantle** (SPEC §10.2). Its runes are not stored but fetched through a holiday when read, and "snapshotting" one at save is the copy | Void Core, already |

**Lean:** no pointer rune, in Core or in Rancho. Efficiency comes from content addressing, which
cannot break. Meaning is an id that may dangle and says so. A live view is a query through a
holiday, which may come back empty and never deletes. What stays to design is the *shape* of a
cross-database reference (§18).

# 3. The mapping

## 3.1 Why nothing can be assumed

A mantle means what it means because of things that are not in the mantle:

| what the mantle depends on | where it lives | what can go wrong in another file |
|---|---|---|
| its glyphs' declarations | the document's `glyphs` | the same glyph name declared with other fields, or not at all |
| its tag axes | free text on runes, meaning by convention | `type:` in one database is `kind:` in another |
| its domain | the document's `domains` | absent, or a different target |
| its name | unique in one document | taken in the other |
| its runes' ids | random, frozen | the same id already there: is that the same rune or a collision? |
| bindings into it | top-level, by name | they dangle (Core allows it), or are silently re-aimed by a same-named mantle |
| the holidays it reaches | the host's holiday registry | not registered in the other |
| its rules | the mantle's `rules` and `config.transform` | read by an engine with other settings |

Two `.miga` files share Hormiga's glyphs but not each other's tags, domains or names. Two `.ink`
files share Jelly's vocabulary and nothing about their scenes. **The author is right that a
move between files cannot assume, even between files of one kind.**

## 3.2 A door and a dictionary

Core's **holiday** (SPEC §10.1) is *"the protocol interface to an external system the
application does not own"*: `query`, `get`, `insert` and `describe`. It is a **door**. It says
how to reach the other side, and `describe()` says what the other side can do. It does not say
what the other side's words mean in yours.

What a move needs is a **dictionary**. The research literature has called it a **schema
mapping** for twenty years:

- **Data integration** (Lenzerini, *Data Integration: A Theoretical Perspective*, PODS 2002)
  separates the sources, the global schema and the *mapping* between them. That gives
  global-as-view or local-as-view, depending on which side is defined in terms of the other.
  Data exchange (Fagin, Kolaitis, Miller and Popa, 2005) is the same thing, used to move data
  rather than to query it.
- **Functorial data migration** (Spivak, 2012) treats a schema as a category and a mapping as a
  functor F between two schemas. A functor gives exactly three canonical ways to move data:
  - **Δ** pulls data back along F;
  - **Σ** pushes it forward and unions it;
  - **Π** pushes it forward and joins it.

  Moving a mantle is a Σ: the receiving database gains it. Seeing a foreign file through
  Core's eyes is a Δ. This tells us there is no fourth kind of move to design for.
- **Bidirectional lenses** (Foster, Greenwald, Moore, Pierce and Schmitt, TOPLAS 2007) are a
  mapping with a `get` and a `put` that obey round-trip laws. Void Core's `scry` `Lens` is
  already one, and Void Jelly uses it for `.blend → .ink → .blend`.
- **Cambria** (Ink & Switch; Litt et al., *Cambria: Schema Evolution in Distributed Systems
  with Edit Lenses*, PaPoC 2021) is the closest precedent. It translates *edits*, not
  documents: every operation in the log is tagged with the schema it was written in, and a
  lens converts the patch to the reader's schema before applying it.
- **SQL/MED and PostgreSQL's foreign data wrappers** keep the two halves apart: a *server* (the
  door) and *foreign tables* and *imported schemas* (the dictionary).

**Cambria's choice fits the family exactly.** Every change here is a dispatcher command, and
every row can be rebuilt by replaying commands (the railguard). So a mapping between two
databases should translate **commands**, not rows:

> **A mantle moves to another database as commands.** The mantle is written as the commands
> that would make it (`mantle new`, `rune new`, `set`, `tag`, `link`, `place`). They pass
> through the mapping into the other database's vocabulary, and run there through its own
> dispatcher.

The railguard then holds on both sides. The receiving database's every row was made by a
command it journaled, and the sending database forgets the mantle by its own `mantle rm`. Nothing
writes rows behind a dispatcher's back.

## 3.3 What a mapping says

A **mapping** (ranch name offered: *la guía*, the transit permit that cattle travel with from one
property to another, naming origin, destination and brand) is authored, so it is data, kept in
a database as runes: in the federation of the dataspace it is used in (§4). In outline:

| member | says |
|---|---|
| from, to | two databases, by id, and the vocabularies they were read with |
| glyphs | each source glyph → a target glyph and how its content's fields correspond, or *carried as is*, or *refused* |
| tags | axis renames (`type:` → `kind:`), value maps, or *kept* |
| names | what to do with a taken name: refuse, or rename by a rule |
| ids | **a move carries each spirit id; a copy mints new ones** (ruled, §2.1) and records where each came from |
| bindings | re-aim them by id, leave them dangling and report, or refuse |
| domains, holidays | which target domain and which registered holiday stand for the source's |

A mapping is **checked before it runs**. Every glyph, tag axis and binding the mantle uses
must be covered, or the move is refused with the list of what is not. That is the "cannot assume"
made into a procedure.

## 3.4 Within one database, the trivial mapping is proved, not assumed

Two parts of **one** database share one identity and one assembled set of declarations, but
each part keeps its own (`deed_test` "declarations travel with their part"). So even inside one
deed, a move between parts first proves that **the identity mapping** holds:
- every glyph the mantle uses is declared the same in both parts;
- its domain is there;
- its key and name are free in the target.

If the proof passes, the move is placement: rows are copied, checked, the deed is rewritten and
the old copy released ([open questions](/design/open-questions.md) §12). If it fails, the move
is a real mapping, made of commands. Nothing is ever assumed.

## 3.5 Should the holiday become something else? Ruled: no

The first round asked whether the holiday should become *"a larger abstracted holiday
structure to map databases together"*. **Ruled in the second round** (*"void core should have
little database things, cuz we have rancho for that"*): Core's holiday stays the door, as it
is, and everything about databases is Rancho's. The larger structure the author described is
real, but it is Rancho's. It is the **mapping** riding on a holiday, used inside a **dataspace**
(§4): the door's `describe()` tells the mapping what the other side has, and the mapping is
checked against that. No message to Void Core is needed.

The parenthesis is right too. A database already holds holidays: Hormiga's `.miga` holds its
topology, the holiday registry. A federation will likewise hold its mappings.

# 4. The dataspace and its federation

## 4.1 The name this already has

The author asked for *"a "session" rather than a database… an active dynamic 'thing' where we
are storing mappings"*, and guessed it already had a name. It does. Franklin, Halevy and Maier
called it a **dataspace** (*From Databases to Dataspaces: A New Abstraction for Information
Management*, SIGMOD Record 2005; *Principles of Dataspace Systems*, PODS 2006):
- a dataspace is a set of **participants** (data sources) and the **relationships** between
  them;
- its support platform *"does not assume complete control over the data"*: each participant
  stays itself;
- integration is **pay-as-you-go**. Participants are usable side by side at once, before any
  mapping exists, and a mapping is added only where something has to cross. Answers are
  best-effort until it does.

That is the author's session, nearly word for word. It also answers the first round's *"can't
assume much at all about the other file"*. A dataspace never assumes, because it starts with no
mappings and grows them as they are needed.

The neighbours, for the record:
- Wiederhold's **mediator** (IEEE Computer, 1992) is a software layer between sources and
  applications. It is what a dataspace *does*, not what it is.
- Sheth and Larson's **federated database system** (ACM Computing Surveys, 1990) is the
  persistent, schema-bearing form. That is our federation (§4.2), and their *loosely coupled*
  federation, with no global schema, is close to a dataspace.
- **Peer data management** (Piazza; Halevy, Ives, Suciu and Tatarinov, ICDE 2003) has mappings
  between pairs of peers, without a centre. That is what mappings in a dataspace are: pairwise.

**What came after, and what is stronger,** is its own page: [after the
dataspace](/design/after-the-dataspace.md). The closest successor is ORCHESTRA's collaborative data
sharing, in which edits cross through mappings, and each participant takes delivery of them, curates
them and trusts them by provenance. Its leans are open questions §26–§29.

## 4.2 How it fits: a session, and the database it writes in

The two rounds asked for two things: a *"session rather than a database"*, and *"another
database D that contains A, B, and C on a mantle"*. They are the two halves of one thing, split
by the line every page here already draws, between what is in hand and what is authored:

| | the **dataspace** (ranch name offered: *la feria*, the fair, where several ranches bring stock for a while, trade under agreed terms, and go home) | its **federation** (*la mancomunidad*, a union of towns that each may leave) |
|---|---|---|
| what it is | the live session: the databases opened together, the mappings in force, the sagas in progress | a database, the dataspace's record: one rune per participant, the mappings, the history of changes across participants |
| which of the five questions | **residency**: one process on one device, never data | **the record**: authored, through a dispatcher, replayable |
| survives a restart | no: it is opened again from its federation | yes |
| holds a participant's rows | never | never |

So the author's session is the **dataspace**, and the author's D is its **federation**. The
railguard makes the split necessary: a mapping is authored, so it must be replayable, so it is
written as commands into a database. The session cannot hold it by itself. A dataspace that
nobody means to keep still has a federation, made in the cache folder and disbanded when it
closes.

**Verbs:**
- a dataspace is **opened** over a federation, and **closed** (its federation is kept);
- a database is **enlisted** in it, and **discharged** from it;
- a federation is **disbanded** (§4.6).

Opening a dataspace opens each participant through its own deed. A participant whose files are
not reachable is a region not loaded, as a missing part is.

## 4.3 Its laws

1. **A federation holds no participant's rows.** It holds participation, mappings and history.
   So disbanding it moves no byte, and A, B and C are exactly what they were.
2. **Every edit lands in a participant through that participant's own dispatcher**, and is
   journaled there. The railguard holds per participant. A dataspace never writes a
   participant's store.
3. **A change across participants is one change in the federation's history** and one or more in
   each participant's. It is never only the federation's.
4. **A participant that is absent is a region not loaded** (the frontier, again). A change that
   needs it waits. It is not applied to the participants that are present and reported as done.
5. **Disbanding changes no participant.**
6. **Pay as you go.** Participants are usable together before any mapping exists. A mapping is
   needed only for what crosses between them, and a crossing with no mapping is refused, never
   guessed.

## 4.4 History: what Palabra tracks

The author: *"mostly for palabra to keep track of edits"*. History is Palabra's, so this is
Rancho's lean for Palabra to answer by message:

- **A participant's edits stay in the participant's own history.** Palabra's utterances are
  content-addressed, so the federation can name them without holding them.
- **A version of the federation is a cut across its participants**: one version of each.
  Palabra already names a version as a cut of one history
  (`VoidPalabra:/concepts/version-as-cut.md`); this is the same idea one level up. Git's
  superproject does it with submodules: a superproject commit records one commit per submodule.
  A deed whose every part is pinned does it for parts (*"a commit names a tree"*).
- **A change across participants** (a move from A to B, say) is a federation utterance whose
  parents are the participant utterances it caused.

## 4.5 Atomicity across participants: sagas, not transactions

Participants may be on different machines or media, and some may be absent, so a change across
them cannot be one transaction.
- SQLite's `ATTACH` makes a transaction across files atomic only in rollback mode: *"if the
  journal_mode is WAL, then transactions continue to be atomic within each individual
  database file"* only, and a crash mid-commit can leave some files changed and not others.
- Across machines there is no `ATTACH` at all.

The established answer is a **saga** (Garcia-Molina and Salem, SIGMOD 1987). A long change is a
sequence of local transactions, each with a compensating one. Here every step and every
compensation is a command, so a move from A to B in a dataspace goes like this:
1. B runs the mantle's commands through the mapping (the copy);
2. the copy is checked against the source, through the mapping;
3. A runs `mantle rm`.

The federation records the three as one change, so a saga in progress survives a restart. If B
is absent, step 1 waits. If step 2 fails, B's compensation is its own `mantle rm`, and A is
untouched.

## 4.6 Disbanding, and the history it may hand back (second round, answer 3)

**Ruled:** disbanding is a function that may return *"a small micro database of the history"*,
and returning it is optional.

`disband(federation, keep_history)`:
- changes no participant (law 5);
- with `keep_history`, returns a **new, small database**, the **history** (ranch name offered:
  *el acta*, the minutes a meeting leaves). It is a fork in the language's sense (*la
  contramarca*): a new id, with its lineage naming the federation. It holds the changes that
  crossed participants (each naming the participant utterances it caused, by hash), and the
  mappings as they stood. It holds **no participation**, so it claims no database and cannot be
  reopened as a dataspace. It is a record, and nothing in a participant depends on it;
- without it, the federation is deleted, and nothing is returned.

So the history is a database like any other, and obeys every law. It can be read, kept, merged
by Palabra, or thrown away.

## 4.7 What this settles

- [One database, many files](/design/one-database-many-files.md) §7.7 asked whether *"a deed as
  a part"* could make a database of databases. **The answer is a federation, and it is a
  database, not a deed.** Participation is authored and has a history, so it is data.
- **The deed of one database stays JSON** ([open questions](/design/open-questions.md) §14).
  Where one database's bytes sit is configuration, and there is no history to keep. The line
  is: *what is authored (which databases work together, how their words correspond) is data;
  where bytes sit is configuration.*
- **Names checked against the family** (the author: *"make sure the names, nouns, and verbs
  don't overlap"*). Taken, so not used:
  - *session* and *workspace* (Core, Maiz's host pattern, Palabra, Mago and every host);
  - *join* and *merge* (Palabra's);
  - *attach* (SQLite's, and used in Core, Maiz and Palabra);
  - *dissolve* (Core's word for a box boundary that vanishes on composition,
    `VoidCore:/design/mantle-composition.md` §4);
  - *profile* (Maiz's and Palabra's person profile, Core's dispatcher profile);
  - *member* (Hormiga's members are people);
  - *catalog*, *roster*, *minutes*, *withdraw*, *unlist* (Hormiga's unlisted issues and
    pages), *dismiss*.

  Free in every project and used here: *dataspace*, *enlist*, *discharge*, *disband*, *la
  feria* and *el acta*. *Participant* is the dataspace literature's own word, and appears elsewhere in
  the family only in passing prose.

# 5. File types built on the rancho

## 5.1 Formats that already work this way

The author's "parent class" has strong precedents. In each one, a base format fixes structure,
and a child adds meaning:

| family | the parent | children | how a child says so |
|---|---|---|---|
| ISO base media file format | boxes, the `ftyp` box | MP4, HEIF/AVIF, 3GP, CMAF | a major brand plus a list of **compatible brands**: one file, several kinds |
| Apple's Uniform Type Identifiers | `public.data` | `public.text` → `public.delimited-values-text` → `public.comma-separated-values-text` | a **conformance** list. Multiple parents are allowed |
| Open Packaging Conventions (ECMA-376 Part 2) | a ZIP of parts, `[Content_Types].xml`, relationships | `.docx`, `.xlsx`, `.pptx` | content types and relationship types |
| SQLite as an application file format | the SQLite file | GeoPackage, MBTiles, Fossil | the header's **`application_id`** (offset 68; the registry is SQLite's `magic.txt`), then the app's own tables |
| Frictionless Data Package | `datapackage.json` | the Camera Trap and Fiscal data packages | a `$schema` naming the profile |
| RIFF | tagged chunks | WAV, AVI, WebP | the form type |

The lesson that carries over: **a reader that knows only the parent still reads the child** as
the parent. A tool that does not know HEIF still walks its boxes, and a reader that does not
know `.ink` still opens it as a rancho.

## 5.2 The file type is the host's; the database declares no kind (second round, answer 6)

Every precedent above labels the file: a brand, a conformance list, an `application_id`. The
first build copied them, with a `profiles` member in the deed. **The author ruled against it:**
*"a profile [is not] inherent to a database… A database exists on its own."*

That is the difference between **nominal** typing (a thing is what its label says) and
**structural** typing (a thing is whatever it has what you need). The ruling chooses structural,
and it fits the family better than the precedents do:
- A label can be wrong. A database that has been edited, moved or adopted may no longer be what
  its label says, and a host that trusted the label finds out late.
- A database here is open-ended and may be infinite. Two hosts can read the same database for
  different purposes, and neither is its "kind".
- A label would be a thing a host says about a database, which is data (§5.5), and the deed
  holds none.

So:
- **The file type is the host's**: its extension (`.miga`, `.ink`), its bale, its vocabulary, its
  part forms. It inherits everything Rancho defines: the five questions, the laws, the deed and
  its part forms, the record in Core's vocabulary.
- **The database carries no kind.** A host brings an **expectation** for each thing it relies on
  (*"a scene has a mantle called 'scene'"*), and `Database::open` checks them, the ones it
  builds on first. A database that falls short still opens, and says what is missing.
- **Who names them** (§23) is therefore not Rancho's question: an expectation is the host's,
  named in the host's own code, and Rancho keeps no registry.

**Built:**
- the deed's `profiles` member is gone (it was added the same day, and never released);
- `rancho::Expectation {name, builds_on, check}`;
- `DatabaseOptions::expect`;
- `Database::meets` and `Database::expectation_check`.

## 5.3 Born and adopted

There are two ways for a file to be built on the rancho.

1. **Born.** The format is built from rancho parts and carries a deed: `.miga`, `.ink`, a Squid
   world, a federation.
2. **Adopted.** The file was not built as a rancho, and **its bytes stay exactly what its own
   tools expect**. A **mapping** (§3, the same thing that moves a mantle) presents it in Core's
   vocabulary: Spivak's Δ, a view.

**The author's CSV and docx were examples of how a database *could* be stored, not file types to
build** (second round, answer 4). No adopted format is planned. What the examples teach is
general, and is §5.4.

**Where an adopted file's deed goes.** There are two places, and the second is what makes a
file a **hybrid**: fully its own format and fully a rancho at once.
- **Beside it, as a sidecar.** This is the photographer's XMP file next to a camera raw that
  must not be modified.
- **Inside it, in the format's own channel for what a reader may ignore:**
  - an extra part in an OPC package;
  - an ancillary chunk in a PNG;
  - a row in a SQLite file's own tables;
  - an extra member in a JSON file.

**Built: the SQLite case.** A store can carry its own deed (`embed_deed`; store meta
`rancho.deed`), and `Database::open` takes the store itself. Its embedded home is whichever
file carries it, so a copied or renamed file still finds itself. A store with no deed opens as a
database of one part. `rancho embed` puts a deed inside its home store.

## 5.4 Groupings that overlap: one owner, many views (second round, answer 4)

The author's CSV, thought abstractly: *"IDs for every cell, and in a way, each row is a mantle?
But also each column is a mantle? Lots of matrix overlap."* The overlap is the general problem,
and a table is only its plainest case. A calendar is by day and by person. A graph has its
communities and its layers. A newsletter's items are by issue and by topic.

Void Core fixes one thing: **a rune lives in exactly one mantle** (SPEC §3.4, a mantle's
`runes`). So a cell cannot be in its row's mantle and its column's mantle at once. Core already
gives everything else that is needed:
- **Every cell is a rune, with its own id, minted and never derived** from its content or its
  position (SPEC §3.1). A position changes when someone sorts the sheet.
- **One grouping owns.** The whole table can be one mantle, or each row can be one. Which is a
  choice of placement and of how the data is loaded, not of meaning.
- **Every other grouping is a selection.** Tags place the cell on each axis (`row:12`,
  `col:email`), and a tag expression (`@col:email`) selects a whole column. **Core's
  query-backed mantle** (SPEC §10.2) makes a selection a mantle that is not stored, filtered by
  a tag expression. So *"each row is a mantle and each column is a mantle"* is true, as one
  owner and any number of views, and no cell is in two places.
- Rancho's [clusters](/concepts/cluster.md) are already a grouping of this kind: derived, and
  owning nothing.

The identity of adopted bytes follows the same rule. Ids are minted once and kept wherever the
shape can hold them: inside, for a hybrid, or beside it, in an overlay part. A shape with
nowhere to keep them is read through its mapping, read-only, and never merged (§21).

## 5.5 Rancho is agnostic (second round, answer 5)

**What "unknown" meant.** The first round's question was only ever about **the deed file's own
fields**, never about what a database holds. A deed is JSON, and a newer Rancho might add a
field to it (a part's `replicas: 2`, say). The worry was what an older Rancho does with that
field when it rewrites the deed. PNG has the classic version of the problem:
- each chunk carries an **ancillary** bit (a reader may skip it) and a **safe-to-copy** bit (an
  editor may carry it into a file whose critical data it changed);
- an editor *"must not copy unrecognized unsafe chunks"*, because they may depend on what
  changed.

**The ruling** (*"rancho would be completely agnostic to anything… so long as it somehow goes
into a mantle"*) settles it, more simply than PNG does:
- **Rancho never decides whether content is real, valid or recognisable.** It holds whatever
  goes into a mantle. Core's glyphs and a host's expectations interpret content, and Rancho
  does not.
- **A deed holds Rancho's members and nothing else.** Whatever a host has to say about its
  database, including what kind it is (§5.2), is data, and belongs in a mantle, where Core's
  dispatcher journals it.
- **A member Rancho does not know is passed through unchanged, and noted.** It is never read,
  never interpreted, and never dropped.
- **A deed written by a newer Rancho is read for what this version knows, and never rewritten by
  it.** This version cannot tell which of the newer members an edit would make false, so it does
  not edit. Nothing it cannot judge is ever left behind, stale. That is PNG's problem answered by
  not editing, rather than by a bit on every member.

**Built:** `write_deed`, `embed_deed` and so `rancho ids` refuse a deed of a newer format.
`check` notes a member Rancho does not know in its own format, and passes it through (`deed_test`).

# 6. How it fits

| | identity | mapping | dataspace and federation | file type |
|---|---|---|---|---|
| **a move between parts** | by key | the identity mapping, proved | none | none |
| **a move between databases** | carried; a copy mints | required, of commands | a saga, recorded as one change | each host checks its own expectations |
| **working on A, B and C together** | each keeps its own | pairwise, added as needed | the session, and its record | none |
| **a `.miga` or an `.ink`** | the deed's id | none | may be a participant | the host's; the database declares none |
| **a shape with overlapping groupings** | every cell its own | the view that adopts it | may be a participant | none |
| **a reference to another database's rune** | three ids, which may dangle | none | none | none |

A dataspace's crossings, a mantle in transit and an adopted file all need the mapping, so **the
mapping is still the next thing to design in detail**, with a holiday as its door.

# 7. Built, planned, open

**Built**, with tests in `deed_test`:
- mantles placed by key, with names read as a fallback and noted (`rancho ids`);
- a database that declares no kind, and a host's expectations, checked at open;
- Rancho's agnosticism about the deed: a newer deed is read and never rewritten, and a member
  Rancho does not know is noted and passed through;
- a store that carries its own deed and opens alone (`rancho embed`).

**Planned, in order:**
1. The identity-mapping proof for a move between parts (§3.4), then the move itself
   ([open questions](/design/open-questions.md) §12).
2. A mapping's data shape, and translating a mantle into commands (§3.3).
3. The dataspace and its federation, with Void Palabra's view of their history (§4.4; a
   drafted message), and `disband` with the optional history (§4.6).
4. The shape of a cross-database reference (§2.1, §18).

**Open**, in [open questions](/design/open-questions.md): §13 (writing to several parts of one
database), §16, §17, §18 (the reference's shape), §25 (a dataspace that is never saved).

# References

- Void Core, `SPEC.md` §3.1 (spirit), §3.4 (mantle), §3.6 (binding), §3.7 (link), §10.1
  (holiday), §10.2 (query-backed mantle); `okf/design/mantle-composition.md` §4 (box or holiday).
- M. J. Franklin, A. Y. Halevy, D. Maier, *From Databases to Dataspaces: A New Abstraction for
  Information Management*, SIGMOD Record 34(4), 2005.
- A. Y. Halevy, M. J. Franklin, D. Maier, *Principles of Dataspace Systems*, PODS 2006.
- G. Wiederhold, *Mediators in the Architecture of Future Information Systems*, IEEE Computer
  25(3), 1992.
- A. Y. Halevy, Z. G. Ives, D. Suciu, I. Tatarinov, *Schema Mediation in Peer Data Management
  Systems*, ICDE 2003.
- M. Lenzerini, *Data Integration: A Theoretical Perspective*, PODS 2002.
- R. Fagin, P. Kolaitis, R. J. Miller, L. Popa, *Data Exchange: Semantics and Query Answering*,
  Theoretical Computer Science 336, 2005.
- D. I. Spivak, *Functorial Data Migration*, Information and Computation 217, 2012.
- J. N. Foster, M. B. Greenwald, J. T. Moore, B. C. Pierce, A. Schmitt, *Combinators for
  Bidirectional Tree Transformations*, ACM TOPLAS 29(3), 2007.
- G. Litt, P. van Hardenberg, O. Henry, *Cambria: Schema Evolution in Distributed Systems with
  Edit Lenses*, PaPoC 2021; Ink & Switch, *Project Cambria* (inkandswitch.com/cambria).
- A. Sheth, J. Larson, *Federated Database Systems for Managing Distributed, Heterogeneous, and
  Autonomous Databases*, ACM Computing Surveys 22(3), 1990.
- H. Garcia-Molina, K. Salem, *Sagas*, SIGMOD 1987.
- T. H. Nelson, *Literary Machines* (transclusion), 1981.
- SQLite, *ATTACH DATABASE* (atomicity across attached files) and *Database File Format*
  (the database header: the application ID at offset 68).
- PNG Specification 1.2, §3.3 (chunk naming conventions: ancillary and safe-to-copy bits).
- ISO/IEC 14496-12 (ISO base media file format: brands); ECMA-376 Part 2 (Open Packaging
  Conventions); Apple, *Uniform Type Identifiers* (conformance); Frictionless Data, *Data
  Package* (`$schema` profiles); RFC 8493 (BagIt). A bag with files listed in `fetch.txt` and
  not yet fetched is *not complete*, and is still a bag: the same rule as a missing part.
