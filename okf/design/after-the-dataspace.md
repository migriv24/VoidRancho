---
type: Design
title: After the dataspace — what came next, and what is stronger
description: "The author asked whether anything newer or stronger has followed dataspaces (2005). It has, along six lines: collaborative data sharing with update exchange, provenance and trust (ORCHESTRA); an algebra of mappings (model management) and mappings with proofs (categorical data integration, CQL); one commit across many tables (lakehouse catalogs, Nessie); sovereignty that travels with the data (the European data spaces, local-first access control); incremental maintenance of every view (differential dataflow, DBSP); and models that propose mappings. What each adds, what fits the family, and the leans taken from them."
tags: [status:current, audience:dev, confidence:exploratory]
timestamp: 2026-11-06T00:00:00Z
---

# 1. The question

The author, 2026-11-06, after [beyond one database](/design/beyond-one-database.md) named the
session a **dataspace**: *"Data space systems have existed for a long time, can you do some
research to find if anything has come after it? Any newer novel idea? Have people suggested
anything stronger?"*

**Short answer: yes.** A dataspace (Franklin, Halevy and Maier, 2005) was weak on purpose. It
promised best-effort answers to queries over participants it did not control, with mappings
added pay-as-you-go. It said almost nothing about **edits**, **trust**, **history**, **proof**,
**who may do what**, or **cost as the data grows**. Each of those has since been taken up, and
the strongest successors are below, ordered by how much they matter to Rancho.

| line | what it adds to the dataspace | fit for the family |
|---|---|---|
| collaborative data sharing (ORCHESTRA, 2005–2013) | **edits** flow through mappings; each participant curates what arrives and trusts by provenance | **closest of all**: §2 |
| model management (2000–2009) | an **algebra of mappings**: match, compose, invert, merge, diff | high: §3 |
| categorical data integration (CQL, 2012–) | mappings that **provably** preserve structure and constraints | high, for the check: §3 |
| lakehouse catalogs (Iceberg, Nessie, 2017–) | **one commit across many tables**, branches, time travel, without copying data | high: confirms the cut, §4 |
| sovereign data spaces (IDS, Gaia-X, 2015–) and local-first access control (Keyhive, 2024–) | **policies and capabilities that travel with the data** | medium now, high when databases are shared: §5 |
| incremental computation (differential dataflow, DBSP, 2013–2023) | every view **maintained from changes**, never recomputed | high, for the infinite-dataset premise: §6 |
| foundation models for integration (2022–) | **match** done by a model: schema and entity matching | medium, through Void Bicho: §7 |
| polystores (BigDAWG, 2015) | queries across engines with **different data models** | low: §8 |
| data mesh (2019) | an **organizational** pattern: domain ownership, data as a product, governance as code | low as technology: §8 |

# 2. Collaborative data sharing: edits, not answers

**ORCHESTRA** (University of Pennsylvania: Ives, Green, Karvounarakis, Tannen and others; CIDR
2005, VLDB 2007, SIGMOD Record 2008, TODS 2013) is the strongest successor for what the family
does. A *collaborative data sharing system* (CDSS) connects autonomous peers, each with its own
schema, through a network of schema mappings, as a dataspace does. Then it adds what a
dataspace lacked:
- **Update exchange.** What flows between peers is **updates**, translated through the mappings
  (*"a generalization of data exchange"*). There are two operations:
  - **publication**: a peer makes its local edits available;
  - **reconciliation**: a peer takes in what others published, translated into its own schema.
- **Local curation.** A peer may override or reject anything that arrived. Its own edits win
  in its own database, and what it imported is never forced on it.
- **Provenance.** Every imported tuple carries *how* it got there: which peers, which mappings.
  This is **provenance semirings** (Green, Karvounarakis and Tannen, PODS 2007), an annotation
  that composes through every query and mapping.
- **Trust policies over provenance.** A peer filters incoming updates by conditions on that
  provenance: *"accept from B, but not what came to B from C"*.

**Why it fits.** It is the design the first round arrived at by itself, with ten years of
results behind it:
- *"a mapping translates commands"* ([beyond one database](/design/beyond-one-database.md) §3.2) is
  update exchange;
- *"every edit lands through the participant's own dispatcher"* (law 2) is reconciliation into a
  peer that keeps control;
- *"a copy records where each rune came from"* (§2.1) is a weak form of provenance.

What ORCHESTRA adds that we did not have:
- **Publication and reconciliation are separate, deliberate steps.** A participant is never
  written to because another participant edited. Edits cross only when a participant reconciles.
  That is a stronger form of autonomy than our laws state.
- **A participant may override what arrived**, and the override is its own edit, kept against
  later updates.
- **Provenance, not just origin.** A rune that crossed through two mappings carries both, so a
  trust rule and an undo can follow the path.

**Lean** ([open questions](/design/open-questions.md) §26): a dataspace's crossings are **update
exchange**:
- a participant **consigns** commands from its journal, through a mapping (ORCHESTRA's
  publication);
- another participant **takes delivery** of them, by its own commands, and may refuse or
  override any of them (ORCHESTRA's reconciliation);
- each crossed rune carries its **provenance** (participants and mappings, by id), written as
  data by the commands that take delivery;
- a participant's **trust policy** is data in the federation, and is checked on delivery.

A move between participants (§4.5 there) is then a consignment, a delivery, and the source's own
`mantle rm`, which is still a saga.

*Renamed the same evening:* ORCHESTRA's *publication* and *reconciliation* are **consign** and
**take delivery** in the family, because *reconciliation* is Void Palabra's (its have/want
protocol between peers), and *publish* belongs to Hormiga and Mago ([the
algebra](/design/the-algebra.md) §7).

# 3. Mappings as things with an algebra, and with proofs

## 3.1 Model management

Bernstein, Halevy and Pottinger (*A Vision for Management of Complex Models*, SIGMOD Record
2000), then Bernstein and Melnik (*Model Management 2.0*, SIGMOD 2007), treat **mappings as
first-class values** with operators:

| operator | does | in a dataspace |
|---|---|---|
| **Match** | proposes a mapping between two schemas | where a mapping comes from (§7) |
| **Compose** | A→B and B→C give A→C | a mapping no one wrote, derived |
| **Invert** | A→B gives B→A | a move back |
| **Merge** | two schemas and a mapping give one schema | enlisting into a shared vocabulary |
| **Diff / Extract** | what one mapping covers that another does not | what a crossing would lose |
| **ModelGen, TransGen** | a schema in another model; code from a mapping | out of scope |

The theory that followed is a warning, and it is the stronger idea here:
- **Composition is not always expressible** in the language of its parts. Fagin, Kolaitis, Popa
  and Tan (*Composing Schema Mappings: Second-Order Dependencies to the Rescue*, TODS 2005) showed
  that composing two simple mappings can need a more powerful kind of rule.
- **Inverses usually do not exist exactly.** Fagin's inverse, then quasi-inverses and maximum
  recoveries (Arenas, Pérez, Riveros and others, 2008–2009), measure how much of the original a
  mapping lets you get back. Most real mappings lose something.

**Consequences for Rancho:**
- A **move back is not free.** If the mapping has no exact inverse, moving a mantle from A to B
  and back does not return the original. So either the move records what it lost (a Diff), or it
  is refused when the loss is not acceptable. The round trip has to be checked, not assumed:
  the same discipline as Void Core's `scry` lens laws.
- **A derived mapping is a proposal.** Composing A→B and B→C gives a candidate for A→C. It is
  checked and authored like any other mapping before it is used.

## 3.2 Categorical data integration: mappings with proofs

The functorial data migration we already cite (Spivak, 2012) grew into **algebraic databases**
(Schultz, Spivak, Vasilakopoulou and Wisnesky, 2016) and the **Categorical Query Language**, CQL
(categoricaldata.net). There, a schema carries equations (constraints), an instance is an
algebra, and the migrations Σ, Δ and Π are guaranteed to **preserve the constraints**. A mapping
that would break them is rejected, by a decision procedure, before any data moves. The slogan is
the title of one of their papers: *Informal Data Transformation Considered Harmful* (2020).

**Consequence:** our *"a mapping is checked before it runs"* can be a **guarantee**, not a
lint, if a mapping is limited to forms that can be checked. A mapping built only from renames,
field correspondences and selections (Δ and Σ along a glyph-to-glyph map) can be verified
completely against both vocabularies. Free-form transformation code cannot be. Lean (§27): a
mapping is data in a small checkable language. A mapping that needs arbitrary code is not a
mapping. It is a host's program, run by its own commands.

# 4. One commit across many tables: the lakehouse catalogs

Industry took the same road from the other end:
- the **data lake** (2010) left files where they were;
- open **table formats** (Apache Iceberg, Delta Lake, Hudi) gave each table snapshots, a manifest
  list and atomic commits over immutable files. That is Rancho's segments, a manifest named by
  hash and a pinned part, nearly exactly;
- the **lakehouse** (Armbrust, Ghodsi, Xin and Zaharia, CIDR 2021) put warehouse guarantees on top;
- **Project Nessie** adds git-like **catalog-level** versioning. Each commit records the state of
  *every* table, by snapshot, without copying data, and branches cover the whole catalog. Apache
  Polaris instead offers atomic multi-table commits through the Iceberg REST catalog.

**What it confirms:**
- **A version of a federation is a cut across its participants** ([beyond one
  database](/design/beyond-one-database.md) §4.4). A Nessie commit is exactly that: one snapshot
  per table. It works at scale.
- **The saga lean is right.** Iceberg's own documentation says of Nessie's branch-and-merge:
  *"this isn't a true multi-table transaction on the database"*, and that is between tables in
  one object store. Between participants on different machines there is less still.
- **Branches over the whole set** are cheap when every part is content-named. A dataspace could
  branch all its participants at once by pinning, without copying a row.

Nessie's commits have a single parent, and merging is left to the catalog. The family's
equivalent is Void Palabra's, which already merges.

# 5. Sovereignty: policies and capabilities that travel with the data

## 5.1 The European data spaces

Since 2015 the word **data space** has been taken up for something else, which matters for our
naming. The International Data Spaces Association (IDS), Gaia-X and the EU's common European data
spaces use it for **sovereign data sharing between organizations**:
- each participant runs a **connector**, and data stays with its owner until a **contract** is
  agreed;
- **usage policies** (written in W3C's ODRL) say what the receiver may do with it, and some
  connectors enforce them, in places inside trusted execution environments;
- the **Dataspace Protocol** is the interoperability standard. It went to release 1.0 under the
  Eclipse Dataspace Working Group in 2024, and IDSA is taking it toward ISO.

What it adds: **rights and obligations that travel with the data**, decided before anything
crosses. The weakness is the same as any usage control: once bytes are on another machine,
enforcement depends on trusting that machine.

**Naming:** within the family, *dataspace* is free (checked in [beyond one
database](/design/beyond-one-database.md) §4.7). Outside it, a reader may take *data space* to mean
the IDS kind. The two share participants and autonomy, and differ in that ours is one person's
session over their own databases, not a market between organizations. Lean: keep the word, write
it as one word as Franklin and colleagues did, and say so where the OKF defines it.

## 5.2 Local-first access control

**Local-first software** (Kleppmann, Wiggins, van Hardenberg and McGranaghan, Onward! 2019) keeps
the data on the devices of the people who use it and syncs without a server. That is Void
Palabra's line. Ink & Switch's **Keyhive** (2024–) solves what that leaves open:
- *"access control must travel with the data itself and work without a central guard"*;
- **convergent capabilities**, which converge under concurrent edits like a CRDT;
- **BeeKEM**, a concurrency-friendly group key agreement, for end-to-end encryption.

**Consequence:** a dataspace whose participants belong to different people needs exactly this:
who may consign into a participant, and who may take delivery from it. It belongs with Palabra, which
already owns signing and peers. Rancho's part is only to record, in the federation, which
capability a crossing was made under (§28).

# 6. Every view maintained from changes

A dataspace answers queries across its participants. Recomputing such an answer is O(database),
and the [infinite-dataset premise](/concepts/language.md) forbids that where a bound is possible
(law 6).
- **Differential dataflow** (McSherry, Murray, Isaacs and Isard, CIDR 2013) maintains iterative
  computations under changes.
- **DBSP** (Budiu, Chajed, McSherry, Ryzhyk and Tannen, VLDB 2023 best paper) gives a general
  answer with no heuristics: any query written from its primitive operators over streams of
  changes is turned *automatically* into its incremental form.

**Fit:** the family is made of changes. Every edit is a command in a journal. So a
query-backed mantle across participants, a mapping's view of an adopted file, and derived data
such as clusters and estimates can be maintained **from the command stream**, in proportion to
what changed. That is the strongest available answer to *"nothing O(database)"*, and it is the
same idea as update exchange (§2) seen from the reading side. Lean: Rancho's derived data and
views across participants are specified as change-in, change-out wherever their operators allow
it. Clusters already have a write-back path, and an estimate already moves monotonically. This is
a direction, not a dependency: nothing is vendored.

# 7. A model proposes the mapping

**Match** was always the weakest operator, done by heuristics. *Can Foundation Models Wrangle
Your Data?* (Narayan, Chami, Orr and Ré, PVLDB 2022) showed large models doing **schema
matching, entity matching, error detection, transformation and imputation**, untrained for them,
at or above the specialized systems of the time, while noting their sensitivity to small changes
in a prompt.

**Fit:** this is pay-as-you-go done well, and Void Bicho is the family's model line. The danger
is the one the author named for pointers: a guessed mapping applied silently. Lean (§29): a
model **proposes** a mapping, a person accepts it by a command, and the checker of §3.2 verifies
it. A model never applies one. A proposal is a rune, so who proposed it, and which encoder, is
recorded (Bicho's provenance stamp).

# 8. Weaker fits, for the record

- **Polystores** (BigDAWG: Duggan, Elmore, Stonebraker and others, SIGMOD Record 2015) query
  across engines with different data models, through *islands of information* and a CAST between
  them. Every participant here is already in Void Core's one model, so the problem a polystore
  solves does not arise. An adopted file, read through a mapping, is the closest case.
- **Data mesh** (Dehghani, 2019) is about organizations: domain ownership, data as a product, a
  self-serve platform, and **federated computational governance**, meaning policies as code. The
  technical idea is the last one. Our expectations (§5.2 there) and the railguard are already
  policy as code.
- **Linked Data** (Bizer, Heath and Berners-Lee, 2009) was described as the dataspace realized
  on the scale of the web: everything named by a URI, which is law 9 at global scale. It shows
  identity working without a centre, and links that may dangle, which is the same tolerance as
  Core's.

# 9. What changes, and what does not

**What stays:** the dataspace as the session and its federation as the record; mappings that
translate commands; the cut; sagas; disbanding.

**What becomes stronger**, each as a lean in [open questions](/design/open-questions.md):
- **§26 — Crossings are update exchange** (§2): consignment and delivery as separate
  steps, local curation, provenance on every crossed rune, and trust policies as data.
- **§27 — A mapping is written in a small, checkable language** (§3): renames, field
  correspondences and selections, verified before use. Composition gives proposals, an inverse
  is checked rather than assumed, and a move that would lose something records the loss or is
  refused.
- **§28 — Who may consign and take delivery** (§5) is Void Palabra's, with capabilities that travel
  with the data, as Keyhive's do. The federation records which capability each crossing used.
- **§29 — A model may propose a mapping**, never apply one (§7).

**Direction, not a question:** views and derived data across participants are maintained from
the command stream (§6).

# References

- M. J. Franklin, A. Y. Halevy, D. Maier, *From Databases to Dataspaces*, SIGMOD Record 34(4),
  2005; A. Y. Halevy, M. J. Franklin, D. Maier, *Principles of Dataspace Systems*, PODS 2006.
- Z. G. Ives, T. J. Green, G. Karvounarakis, N. E. Taylor, V. Tannen, P. P. Talukdar, M. Jacob,
  F. Pereira, *The ORCHESTRA Collaborative Data Sharing System*, SIGMOD Record 37(3), 2008;
  T. J. Green, G. Karvounarakis, Z. G. Ives, V. Tannen, *Update Exchange with Mappings and
  Provenance*, VLDB 2007; G. Karvounarakis, T. J. Green, Z. G. Ives, V. Tannen, *Collaborative
  Data Sharing via Update Exchange and Provenance*, ACM TODS 2013.
- T. J. Green, G. Karvounarakis, V. Tannen, *Provenance Semirings*, PODS 2007.
- P. A. Bernstein, A. Y. Halevy, R. A. Pottinger, *A Vision for Management of Complex Models*,
  SIGMOD Record 29(4), 2000; P. A. Bernstein, S. Melnik, *Model Management 2.0: Manipulating
  Richer Mappings*, SIGMOD 2007.
- R. Fagin, P. G. Kolaitis, L. Popa, W.-C. Tan, *Composing Schema Mappings: Second-Order
  Dependencies to the Rescue*, ACM TODS 30(4), 2005; M. Arenas, J. Pérez, J. Reutter,
  C. Riveros, *Inverting Schema Mappings: Bridging the Gap between Theory and Practice*, VLDB
  2009.
- P. Schultz, D. I. Spivak, C. Vasilakopoulou, R. Wisnesky, *Algebraic Databases*, Theory and
  Applications of Categories 32, 2017 (arXiv 1602.03501); P. Schultz, R. Wisnesky, *Algebraic
  Data Integration*, Journal of Functional Programming 27, 2017; the CQL project,
  categoricaldata.net.
- M. Armbrust, A. Ghodsi, R. Xin, M. Zaharia, *Lakehouse: A New Generation of Open Platforms that
  Unify Data Warehousing and Advanced Analytics*, CIDR 2021; Apache Iceberg; Project Nessie,
  *Transactions* (projectnessie.org); Iceberg's Nessie documentation.
- International Data Spaces Association, *Data Space Connector Report* (2024) and the
  announcement of the Dataspace Protocol's path to 1.0 under the Eclipse Dataspace Working Group
  (January 2024).
- M. Kleppmann, A. Wiggins, P. van Hardenberg, M. McGranaghan, *Local-First Software: You Own
  Your Data, in Spite of the Cloud*, Onward! 2019; Ink & Switch, *Keyhive: Local-first access
  control* (lab notebook, 2024–).
- F. McSherry, D. G. Murray, R. Isaacs, M. Isard, *Differential Dataflow*, CIDR 2013; M. Budiu,
  T. Chajed, F. McSherry, L. Ryzhyk, V. Tannen, *DBSP: Automatic Incremental View Maintenance
  for Rich Query Languages*, PVLDB 16(7), 2023.
- A. Narayan, I. Chami, L. Orr, C. Ré, *Can Foundation Models Wrangle Your Data?*, PVLDB 16(4),
  2022.
- J. Duggan, A. J. Elmore, M. Stonebraker, et al., *The BigDAWG Polystore System*, SIGMOD Record
  44(2), 2015.
- Z. Dehghani, *Data Mesh*, O'Reilly 2022 (the 2019 and 2020 articles).
- C. Bizer, T. Heath, T. Berners-Lee, *Linked Data — The Story So Far*, IJSWIS 5(3), 2009.
