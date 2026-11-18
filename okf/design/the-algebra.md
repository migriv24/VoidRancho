---
type: Design
title: The algebra of databases — schemas, mappings and their operations
description: "The author, 2026-11-06: mappings as their own form of math, with operations; Rancho holds the logic for operating over them; see how CQL works and how it fits; 'having actual algebra would greatly benefit us'. How CQL's categorical data model works. Why a Void Core document is an unusually easy case for it: its schema is a finite acyclic category, and Core's random ids are already labelled nulls. The operations and their laws: compose, the three migrations Δ, Σ and Π, the round trip that measures loss, pushouts that merge schemas. Commands as deltas, so a mapping translates them exactly where it is linear. The line with Void Palabra stated as mathematics. Provenance as a semiring. Where a holiday carries a schema, and where AI proposes mappings without being part of the algebra. The order to build it in."
tags: [status:current, audience:dev, confidence:exploratory, foundation]
timestamp: 2026-11-06T00:00:00Z
---

# 1. The ask

The author, 2026-11-06, after [after the dataspace](/design/after-the-dataspace.md):

> *"mappings as its own form of math, with operations and such makes so much sense. Our holiday
> structure should be built to handle that, and rancho should hold the logic for how to operate
> over these mappings."*
>
> *"We should see how CQL works, how it can work with our structure, etc. I feel like the
> mathematics of our databases is a big thing we need to overcome. Having actual algebra would
> greatly benefit us."*

And, on AI: *"Hormiga could implement AI, however Rancho is giving structure to do it."*

So:
- **Rancho holds the algebra**: the objects (schemas), the arrows (mappings), the operations on
  them and the laws they obey.
- **A holiday carries a schema** across its door (§9).
- **A host, or Void Bicho, brings any model** that proposes a mapping. Rancho gives the structure
  a proposal goes into and the check it must pass (§10).

# 2. What an algebra buys

Without one, a mapping is a script and every question about it is answered by running it and
looking. With one:
- **Questions become decidable before any data moves.** Does this mapping fit both databases?
  Does moving a mantle and moving it back give the original? Can these two mappings be composed?
- **Mappings compose.** A→B and B→C give A→C, by a rule, with its laws.
- **Loss is a number, not a surprise.** The round trip has a precise measure (§5.3).
- **Merging and mapping commute** where the algebra says so. That is the line with Palabra (§7).
- **A command crosses a mapping exactly** where the mapping is linear, and the algebra says where
  that is (§6).
- **Laws are tests.** Every law in §5 is a property a test can check on random databases, as the
  placement law is checked today.

# 3. How CQL works

**CQL**, the Categorical Query Language (Schultz, Spivak, Vasilakopoulou and Wisnesky,
*Algebraic Databases*, 2017; Schultz and Wisnesky, *Algebraic Data Integration*, 2017;
categoricaldata.net), is the working form of Spivak's functorial data migration. Its pieces:

| piece | what it is | in plain terms |
|---|---|---|
| **typeside** | an algebraic theory of base types and functions (`String`, `Int`, `+`, `concat`) with equations | the kinds of values a field can hold |
| **schema** | entities, **foreign keys** (arrows from entity to entity), **attributes** (arrows from an entity to a type), and **path equations**. A finitely presented category with attributes | what kinds of thing exist, how they point at each other, and what must always be true |
| **instance** | a functor from the schema to sets, given by **generators** (rows) and equations, then saturated by a decision procedure | the data |
| **mapping** F: S → T | sends each entity to an entity, each foreign key to a path, each attribute to a term, and must preserve every equation (CQL checks this with a theorem prover) | a dictionary that respects the rules |
| **Δ_F** | pull an instance of T back to S: reindex | read the other database in your own words |
| **Σ_F** | push an instance of S forward to T: the left Kan extension, computed by the **chase**. It adds **fresh generators** where T demands rows S never had, and merges rows T's equations make equal | move data across: the receiving side gains it |
| **Π_F** | push forward by the right Kan extension: products and joins | combine data that must agree |
| **query** | *for / where / return* (an "uber-flower"): a collection of conjunctive queries, more general than a mapping, which composes | a selection or a rewrite, such as `type:` → `kind:` |
| **transform** | a morphism between two instances of one schema | how one state of a database maps into another |
| **integration** | the **pushout** (colimit) of schemas along their overlap; each source is Σ'd in; rows are merged by equations | merge two vocabularies, then the data |

The three migrations are adjoint: **Σ_F ⊣ Δ_F ⊣ Π_F**. Everything in §5 follows from that one
fact.

The generators of an instance are *labelled nulls*. They have no meaning of their own and are
considered up to isomorphism. When the chase needs a row nobody supplied, it adds a fresh
symbol, such as `g(x)` for "the thing x's g points at" (Wisnesky and Spivak, *Fast Left Kan
Extensions Using the Chase*, 2022).

**CQL itself cannot be vendored.** The Java IDE's licence permits non-commercial and evaluation
use only, and the Haskell implementation is AGPL-3.0. That does not matter much: the mathematics
is not licensed, and §4 shows that the part the family needs is small enough to write from the
papers.

# 4. A Void Core document as a schema

## 4.1 The schema of a database

A database's **schema**, in the algebra's sense, is read off its declarations. It is not the
store's SQL tables.

| entity | foreign keys | attributes | from |
|---|---|---|---|
| `Mantle` | | `id`, `name`, `domain` | SPEC §3.4 |
| `Rune` | `mantle: Rune → Mantle` | `spirit.id`, `name`, the six facets, `placement`, `quantity` | §3.1–3.2 |
| one entity per declared glyph `G` | `is: G → Rune` | the fields the glyph's declaration gives its content | §3.3 |
| `Link` | `from, to: Link → Rune` | `relation`, `weight`, `directed` | §3.7 |
| `Tag` | `on: Tag → Rune` | `axis`, `value` | §5 |
| `Binding` | `from, to: Binding → Rune` | `on`, `do`, `note` | §3.6 |

**Equations** come from Core's own rules:
- **a link stays inside its mantle:** `from.mantle = to.mantle`. That is SPEC §3.7 as a path
  equation, so every mapping must preserve it, and the check is mechanical.
- every glyph entity's `is` is one-to-one, and a name is unique in its mantle. These are **side
  conditions**, not equations: a key and an injectivity condition. Rancho checks them beside the
  algebra, as Void Palabra's *anomalies* do after a merge.

**Tags are data**, rows of `Tag`. So renaming an axis (`type:` → `kind:`) is not a schema mapping.
It is a **query**: *for t in Tag where t.axis = "type" return axis = "kind"*. A database that
declares its axes may have them promoted into its schema, and then a rename becomes an arrow.

## 4.2 Why Core is an easy case

Two facts make the hard parts of CQL disappear for the family:

1. **The schema is a finite acyclic category.** Every foreign key points *up*: glyph → Rune →
   Mantle, Link → Rune, Tag → Rune, Binding → Rune. There are finitely many paths, so every
   question about equations, whether two paths are equal or a mapping preserves them, is
   **decided by enumeration**. CQL needs Knuth–Bendix completion and a theorem prover because
   general schemas have cycles. Ours do not. (Should a glyph ever carry a reference to another
   rune as a declared field, that is a foreign key `G → Rune`, still acyclic, because `Rune`
   does not point back down.)
2. **Core's random ids are already labelled nulls.** SPEC §3.1 forbids deriving `spirit.id` from
   content because *"the randomness is load-bearing"*. Meaningless, unique, compared only for
   equality: that is the definition of a generator. So the copy and move rules of [beyond one
   database](/design/beyond-one-database.md) §2.1 are the algebra's, not ours:
   - **Σ carries generators.** A rune that moves keeps its `spirit.id`, because Σ_F sends a
     generator of S to the same generator in T.
   - **Σ mints where it must.** A row the target requires and the source never had (a mantle for
     a rune whose mantle did not cross, say) is a fresh generator, which is a freshly minted id.
   - **A copy is a move followed by an isomorphism** that renames every generator, which is what
     "a copy mints new ids" means.

# 5. The operations, and their laws

## 5.1 The category of schemas

| operation | gives | law (each one a test) |
|---|---|---|
| **identity** `id_S` | S → S | `F ∘ id = F = id ∘ F` |
| **compose** `G ∘ F` | A → C from A → B and B → C | `H ∘ (G ∘ F) = (H ∘ G) ∘ F` |
| **check** `F` | whether F is a mapping at all: every foreign key to a path with the right ends, every attribute to a term of the right type, every equation preserved | decidable for finite acyclic schemas (§4.2) |

Composing two schema mappings is always a schema mapping. The warning from model management, that
composition can need more power than its parts (Fagin, Kolaitis, Popa and Tan, 2005), applies to
mappings written as general rules. It does not apply to schema morphisms, which compose as
functors. That is one reason to keep mappings in this form wherever possible (§27).

## 5.2 Moving data: Δ, Σ and Π

| | direction | does | in the family |
|---|---|---|---|
| **Δ_F** | T's data → S | reindex: read T in S's terms | a view of another database, or of an adopted file |
| **Σ_F** | S's data → T | union, creating what T needs, merging what T's equations make equal | a move, or a copy |
| **Π_F** | S's data → T | join, keeping only what agrees | combining participants that must match |

Laws (from Σ ⊣ Δ ⊣ Π):
- **Δ composes backwards:** `Δ_{G∘F} = Δ_F ∘ Δ_G`.
- **Σ and Π compose forwards:** `Σ_{G∘F} = Σ_G ∘ Σ_F`, and the same for Π.
- **Δ_id, Σ_id and Π_id do nothing.**

## 5.3 The round trip measures loss

Every adjunction has a **unit**. For Σ ⊣ Δ it is the map `η: I → Δ_F(Σ_F(I))`: move the data
across, read it back, and compare it with what you started with.
- **If η is an isomorphism, the move is lossless:** moving back returns exactly the original.
- **That holds whenever F is fully faithful**, meaning it sends distinct entities to distinct
  entities and the paths between them one-to-one. This is a property of the mapping alone, decided
  before any data moves.
- **Where η fails, the failure is the loss**, row by row: what merged, what was created, what had
  no image. That is model management's **Diff**, computed rather than guessed.

So *"a move back is checked, not assumed"* ([after the dataspace](/design/after-the-dataspace.md)
§3.1) has an exact form: **check that F is fully faithful on the part being moved, or report the
unit's failure.**

**Inverse.** An exact inverse exists only when F is an isomorphism of schemas. Otherwise Δ_F is
the best recovery of Σ_F that the algebra offers (it is Σ_F's right adjoint), which is the
categorical form of a *maximum recovery* (Arenas and others, 2008–2009).

## 5.4 Merging schemas: the pushout

Two databases enlisted in a [dataspace](/design/beyond-one-database.md#4-the-dataspace-and-its-federation)
that share part of their vocabulary have an overlap schema `O` with mappings `O → A` and `O → B`.
- **Their pushout** `A +_O B` is the smallest schema containing both and identifying the overlap.
  This is model management's **Merge**, and CQL's integration pattern.
- **Each participant's data is Σ'd into it.** Rows the overlap's equations identify are merged.
  Which runes are the same rune is either declared, or proposed and accepted (§10).
- **For finite acyclic schemas the pushout is computed directly**, by gluing the two graphs and
  taking the equations of both.

A dataspace with a merged schema can answer a question across its participants in one vocabulary,
which is what the dataspace literature means by integration, with the mappings as its proof.

# 6. Commands are deltas

Every change in the family is a dispatcher command, and the record slice of a command is a
**signed set of rows**, a Z-set in DBSP's terms (Budiu and others, 2023):

| command | its delta |
|---|---|
| `rune new` | `+` one `Rune` row and one glyph row |
| `rm` | `−` those rows, and the `Link` and `Tag` rows on the rune |
| `set` | `−` the old row, `+` the new one |
| `tag` | `+` one `Tag` row |
| `link` | `+` one `Link` row |
| `mantle new` | `+` one `Mantle` row |

(Only the record slice. Undo frames, the active mantle and the view slice are not data, and do not
cross.)

**A mapping translates commands exactly where it is linear**:
- **Δ_F is always linear**, `Δ_F(I + d) = Δ_F(I) + Δ_F(d)`, because it only reindexes. So a
  command crosses Δ **by itself**, with no need to read the target.
- **Σ_F is linear when it merges nothing**: when T's equations identify no two generators that S
  kept apart. Then a command crosses Σ by itself too. When it does merge, the translated command
  depends on the target's state, exactly as an incremental join does in DBSP.
- **Π_F is bilinear** (it joins), so it needs both sides' state, kept incrementally.

That turns the first round's informal rule (*"a mantle moves as the commands that would make
it"*) into a decision:
- **linear:** translate each command by itself;
- **not linear:** translate against the target's current state;
- **the algebra says which**, from the mapping alone.

It is also how views across participants stay current from the command stream instead of being
recomputed ([after the dataspace](/design/after-the-dataspace.md) §6).

# 7. The line with Void Palabra, as mathematics

Rancho and Palabra are close relatives: both are about databases at rest and over time, and both
read Core's state and journal as data. The algebra separates them cleanly:

| | **Void Palabra** | **Void Rancho** |
|---|---|---|
| works in | **one schema**: the states of *one* database, across its replicas and over time | **many schemas**: *different* databases, and the shape and size of each |
| its objects | instances of one schema: states, versions (cuts), utterances (deltas in a partial order) | schemas, mappings between them, and the placement of one database's bytes |
| its central operation | **join**: merging two states of the same database | **migration** (Δ, Σ, Π): moving data between databases with different schemas |
| identity it owns | the content name of a state or a change (hash) | the id of a database and of its parts |
| across devices | transport, sync, reconciliation, capabilities | none: local to one device or one shared medium |
| in ORCHESTRA's terms | the transport of updates, their history and merge, conflicts, who may write | the mappings, the translation of updates through them, provenance through mappings, trust rules over mapping paths |

**The interface law.** Σ_F is a left adjoint, so it preserves colimits. Δ_F has adjoints on both
sides, so it preserves limits and colimits. So, wherever Palabra's join is a colimit of
instances (a union keyed by id, conflicts aside):

> **merging and then mapping gives the same result as mapping and then merging**, for Σ and Δ.

That is the property ORCHESTRA's update exchange needs. Each participant's changes can be
translated independently, and Palabra can merge the results, in either order. Where Palabra's
join is *not* a colimit (a conflict, a last-writer-wins rule), the law can fail, and the place it
fails is a Palabra conflict. Whether the join is a colimit in this sense is Palabra's to confirm
([open questions](/design/open-questions.md) §31).

**Words, kept apart:**
- *reconciliation*, *join*, *merge*, *utterance*, *cut*, *canonical form* and *anomaly* are
  Palabra's;
- *schema*, *mapping*, *migration*, *dataspace*, *federation* and *deed* are Rancho's.

ORCHESTRA's *publication* and *reconciliation* are therefore named here **consign** (a
participant offers commands through a mapping) and **take delivery** (another runs them, by its
own commands, and may refuse or override any). Both words are free in every project.

# 8. Provenance is a semiring

ORCHESTRA's trust rests on **provenance semirings** (Green, Karvounarakis and Tannen, PODS 2007):
- every source row carries a token: a participant and an utterance, by id;
- a derived row carries a **polynomial** over those tokens, in N[X]:
  - `+` means *derived in two ways* (a union, or two mappings giving the same row);
  - `·` means *derived jointly* (a join, as Π makes);
- Δ, Σ and Π act on the polynomials as they act on rows, so provenance composes through any
  chain of mappings for free;
- **a trust policy is a semiring homomorphism** to `{keep, drop}`. Evaluate the polynomial, and
  the row is kept or not. *"Accept from B, but not what B got from C"* is one such evaluation.

This is the algebra behind §26's "trust policies as data". Void Allomone owns annotations and
their lattice merge. A semiring is a different structure from a lattice, but close enough that
Allomone should know before Rancho builds it ([open questions](/design/open-questions.md) §32).

# 9. A holiday carries a schema

Void Core's holiday is the door to a system the application does not own, and its `describe()`
returns `{ capabilities, kind, status }` (SPEC §10.1). The author: *"our holiday structure should
be built to handle that"*. It can, without changing Core:
- **A schema is a data shape**, defined by Rancho (`rancho-schema/1`): entities, foreign keys,
  attributes, equations.
- **A holiday that can be mapped returns its schema** inside `describe()`'s capabilities. Core's
  shape is unchanged, and Core never reads it.
- **A database's own schema** is derived by Rancho from its declarations (§4.1). A holiday is how
  a schema reaches Rancho from something that is not a database the family holds.
- So the holiday is the **door**, the schema it describes is the **object**, a mapping is an
  **arrow**, and Rancho is the **algebra** over all three.

That follows the boundary test (`boundaries.md` §5): a shared schema is a data shape, not a
function call, so nothing links anything new ([open questions](/design/open-questions.md) §30).

# 10. Where AI fits: proposals, outside the algebra

**Match**, finding which elements of two schemas correspond, is the one operation the algebra
cannot do. It needs judgement about meaning. Everything after it is algebra. So the structure is
a **propose, check, accept** loop, and Rancho owns all of it except the proposer:

1. **The input:** two schemas, in Rancho's data shape, with sample rows if the host allows.
2. **A proposal:** a list of **correspondences**, each with a source element, a target element, a
   score, and who or what proposed it (a model's id and its provenance stamp, as Void Bicho
   records it). A proposal is data, a rune.
3. **The check:** Rancho assembles the accepted correspondences into a mapping and checks it
   (§5.1). A proposal that is not a mapping is refused with the reason.
4. **Acceptance is a person's command.** A model never applies a mapping
   ([open questions](/design/open-questions.md) §29).

This is the shape Void Squid's forge already has (a proposer and a verifier in a match loop), and
the `propose` contract it asked Void Bicho for. Rancho asks for the same contract: schemas in,
ordered correspondences with confidence out.

**What a proposer can be, lightest first** (none of them is Rancho's):

| proposer | size | what it is good at |
|---|---|---|
| a **deterministic baseline**: name similarity plus **Similarity Flooding** (Melnik, Garcia-Molina and Rahm, ICDE 2002), a fixpoint over the two schemas' graphs | no model | structure. Valentine's benchmark (ICDE 2021) found the schema-based matchers the fastest, and noted they suffer when names are noisy. It is mathematics, so **Rancho may ship it**, and the loop then works with no model at all |
| a **small sentence embedder** (all-MiniLM-L6-v2, about 22M parameters; Bicho already runs MiniLM) | tiny | names and descriptions that mean the same thing in other words |
| **Magneto's retriever** (MPNet, about 110M, fine-tuned on LLM-generated examples; PVLDB 2025), optionally reranked by a large model | small, plus an optional large one | the current best cost and accuracy trade-off: a small model retrieves, a large one only reranks the few candidates |
| **Unicorn** (one small encoder and a mixture of experts for seven matching tasks; SIGMOD 2023) | small | one model for schema, entity, column-type and ontology matching |
| **Ditto** (entity matching with small pretrained models; PVLDB 2020) | small | which runes in two participants are the same rune (§5.4) |
| a **JEPA-style joint embedding** (Bicho's `world` and `embed` kinds) | small | embeds both schemas' elements into one latent space, so matching is prediction in that space |

Rancho names none of these and links none. A host or Void Bicho registers a proposer, and Rancho
sees only correspondences with scores. That keeps Rancho abstract, as the author asked: *"Rancho
is giving structure to do it."*

# 11. What to build, in order

Each step is pure (it reads data and writes data, and changes nothing), and each lands with its
laws as tests:
1. **The schema of a database** (§4.1), from its declarations, with the side conditions checked.
2. **The mapping's data shape**, with identity, composition and the check (§5.1). Tested by the
   category laws on random schemas.
3. **Δ, on instances and on commands** (§5.2, §6). Tested by `Δ_{G∘F} = Δ_F ∘ Δ_G` and by
   linearity.
4. **Σ**, by the chase, which is direct for acyclic schemas: carrying ids, minting where required,
   and **the round-trip unit** (§5.3). Tested by "fully faithful ⇒ lossless" and by reporting the
   loss otherwise.
5. **The pushout of two schemas** (§5.4).
6. **Provenance polynomials** through Δ and Σ, and trust as their evaluation (§8).
7. **The proposal loop and the deterministic baseline** (§10).
8. Only then, **consign and take delivery** in a dataspace, on top of all of the above.

Π, general queries (uber-flowers), and promoting tag axes into a schema come after, when a client
needs them.

# 12. Is Void Allomone already this algebra?

The author, 2026-11-06: *"Void Allomone might actually be our system of algebra already? Perhaps
we should strengthen Void Allomone more such that it can truly be this algebraic step? Or is it
worth distinguishing them?"*

**Read from its own bundle, Allomone is a derive-only annotation language.** Independent sources
(scripts) state what should be true of the subjects (runes) of **one** document. A
join-semilattice merge, with a law per property (`Unique`, `Max`, `Min`, `Sum`, `Tally`, `Any`,
`All`, `Custom`), composes their opinions into one derived annotation set. A real tie across
sources is ⊤, a conflict a person settles with a logged command. Its foundations
(`VoidAllomone:/concepts/foundations.md`) are join semilattices, closure operators, Datalog's
EDB/IDB split with **one stratum**, CALM monotonicity and specificity as a subsumption order. It
writes nothing: *"deriving writes nothing"*.

## 12.1 They are different algebras

| | **Void Allomone** | **Rancho's algebra** |
|---|---|---|
| the mathematics | **order theory**: lattices, joins, closure operators | **category theory**: schemas as categories, mappings as functors, Kan extensions, adjunctions, pushouts |
| what it is about | **opinions about one database**: what should be true of its runes | **relations between different databases**: how one schema's words correspond to another's |
| inside or between | inside one document, one vocabulary. *"`kind` is YOUR word"*: it compares strings and never reads them for meaning | between schemas; translating the words is the whole point |
| its conditions | about one subject at a time (its tags, kind, name, mantle, the user graph). No variable ranges over other subjects, so no joins | paths through foreign keys and conjunctive queries, joins included (Π) |
| writes | **never**: derive-only, with writing deliberately deferred and human-gated (`materialization.md`) | **must**: Σ moves data, by commands, carrying ids and minting new ones |
| fixpoints | refused for now. One stratum is *"the hinge of the whole design"*, and a fixpoint *"should not be made casually"* | needed: the chase that computes Σ is a fixpoint |
| conflict | ⊤, a first-class object | a mapping that fails its check is refused with the reason; a lossy round trip is reported row by row |

**So Allomone should not become this algebra.** Making it so would break the two properties its
whole design rests on: it would have to **write** (Σ), and it would have to **iterate to a
fixpoint** (the chase). Both are exactly what its own foundations call the change that should not
be made casually. It would also stop being vocabulary-agnostic, which is what lets one script
serve any host.

## 12.2 They are one idea at three levels

There is a single thread under the family's mathematics: **composition governed by laws, with
disagreement surfaced rather than hidden**. It appears at three levels, and a lattice is a special
kind of category (a poset), so the three are also one language:

| level | library | what is composed | the law |
|---|---|---|---|
| opinions about one database | **Void Allomone** | independent sources' statements about each rune | a join in each property's lattice; ⊤ for a real tie |
| states of one database over time | **Void Palabra** | two states of the same database | the join of instances keyed by id; conflicts as objects |
| different databases | **Void Rancho** | schemas and their data | functors and their migrations; the pushout of schemas |

Σ and Δ preserve colimits, and a join is a colimit. That is why the three levels can be stacked
without one breaking another (§7).

## 12.3 Where they meet, as data shapes

Allomone can be *strengthened* in its own direction where the algebra hands it something, without
taking on writes or fixpoints. Each of these is a data shape, so nothing links anything (the
boundary test):

1. **Scripts that travel between databases.** An Allomone script names kinds and tags. Δ along a
   mapping presents database A's runes in database B's words, so **a script written for B
   annotates A unchanged**. Allomone does nothing new: the host gives it subjects read through
   Δ.
2. **Combining proposals.** A proposal is a rune in a proposals mantle (§10). Each proposer, the
   deterministic baseline or a model, is an Allomone **source**: a `Max` law for a
   correspondence's score, and a `Unique` law for *"the target of this source element"*. When two
   proposers disagree, the result is ⊤, which a person settles with a logged command. That command
   is the acceptance §10 requires. It is Allomone's own ruling, *"propose, do not generate… the
   engine's job is to find what needs deciding, and the person's job is to decide it"*, applied to
   mappings.
3. **Simple trust policies.** Where provenance is written as tags on crossed runes (`from:B`,
   `via:<mapping id>`), a monotone policy (*"distrust what came through C"*) is an Allomone script.
   The full provenance polynomial, which needs `·` for joint derivation, stays Rancho's (§8).

And Rancho takes three lessons from Allomone:
- a failure to compose is an **object to inspect** (Allomone's *explaining a cell*), so a refused
  mapping and a lossy round trip are explained element by element;
- **build the analyzer before the capability** (`materialization.md`), as Rancho builds the check
  before the move;
- **undeclared means conservative**: an undeclared property is `Unique` there, and an unmapped
  element is refused here, never guessed.

**Lean** ([open questions](/design/open-questions.md) §33): keep them distinct. Record the pair in
Void Mago's `family.json` as proposed, and offer Allomone the three meeting points by message.
Allomone is not one of the repositories this agent may edit, so the message is the way.

# References

- P. Schultz, D. I. Spivak, C. Vasilakopoulou, R. Wisnesky, *Algebraic Databases*, Theory and
  Applications of Categories 32, 2017 (arXiv 1602.03501).
- P. Schultz, R. Wisnesky, *Algebraic Data Integration*, Journal of Functional Programming 27,
  2017 (arXiv 1503.03571).
- R. Wisnesky, D. I. Spivak, *Fast Left Kan Extensions Using the Chase*, Journal of Automated
  Reasoning, 2022 (arXiv 2205.02425).
- D. I. Spivak, *Functorial Data Migration*, Information and Computation 217, 2012;
  D. I. Spivak, R. Wisnesky, *Relational Foundations for Functorial Data Migration*, DBPL 2015.
- The CQL project, categoricaldata.net and github.com/CategoricalData/CQL (licence: BSD-3 for
  non-commercial and evaluation use; the Haskell implementation, statebox/cql, is AGPL-3.0).
- P. A. Bernstein, S. Melnik, *Model Management 2.0*, SIGMOD 2007; R. Fagin, P. G. Kolaitis,
  L. Popa, W.-C. Tan, *Composing Schema Mappings*, ACM TODS 30(4), 2005; M. Arenas, J. Pérez,
  J. Reutter, C. Riveros, *Inverting Schema Mappings*, VLDB 2009.
- T. J. Green, G. Karvounarakis, V. Tannen, *Provenance Semirings*, PODS 2007; T. J. Green,
  G. Karvounarakis, Z. G. Ives, V. Tannen, *Update Exchange with Mappings and Provenance*, VLDB
  2007.
- M. Budiu, T. Chajed, F. McSherry, L. Ryzhyk, V. Tannen, *DBSP*, PVLDB 16(7), 2023.
- S. Melnik, H. Garcia-Molina, E. Rahm, *Similarity Flooding: A Versatile Graph Matching
  Algorithm*, ICDE 2002.
- C. Koutras and others, *Valentine: Evaluating Matching Techniques for Dataset Discovery*, ICDE
  2021.
- Y. Liu, E. H. M. Pena, A. Santos, E. Wu, J. Freire, *Magneto: Combining Small and Large
  Language Models for Schema Matching*, PVLDB 18(8), 2025.
- J. Tu and others, *Unicorn: A Unified Multi-tasking Model for Supporting Matching Tasks in Data
  Integration*, SIGMOD 2023.
- Y. Li, J. Li, Y. Suhara, A. Doan, W.-C. Tan, *Deep Entity Matching with Pre-Trained Language
  Models* (Ditto), PVLDB 14(1), 2020.
- Void Core `SPEC.md` §3.1–3.7, §5, §10.1; Void Palabra's join, anomaly and reconciliation
  concepts; Void Squid's forge and its message to Void Bicho (2026-10-28).
