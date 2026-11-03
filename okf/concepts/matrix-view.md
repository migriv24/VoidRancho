---
type: Concept
title: The matrix view — what a database looks like to a JEPA model
description: "A mantle read as sparse matrices: a stable row index over runes, one adjacency matrix per relation (float weights), entity × measure values with units, a rune × tag incidence with sources, a feature matrix typed by measurement level — derived on every call, never stored as a second truth. Graph algorithms as sparse linear algebra over semirings, checked against textbook implementations. Exported as NumPy arrays for training. Answers the author's question: a database looks to a JEPA very much like runes and mantles."
resource: src/matrix.cpp
tags: [status:current, audience:dev, confidence:asserted, foundation]
timestamp: 2026-10-30T00:00:00Z
---

> The author: *"what would a database look like to a jepa model? i think it would look very
> similar to our runes and mantle structure!"*

**Yes — and the reason is specific.** A JEPA world model needs an object-structured state
(entities with identity across time), relations between them, numbers it can embed by type,
and transitions labelled by actions (`VoidCore:/design/world-models.md` §4). A mantle has all
four by construction. What it lacks is the **array form** a model, a clustering algorithm or a
GPU reads. The matrix view is that form (`rancho::matrix_view`), **derived from the store's
rows on every call** — never stored as a second truth.

# The arrays

For one mantle — or some of its [clusters](/concepts/cluster.md), with the mantle's measures —
with `n` runes:

| array | shape | built from | status |
|---|---|---|---|
| **row index** `ρ` | `n` | the [store](/concepts/store.md)'s `rows`: spirit id → a stable integer, never reused | **built.** Local order is by ρ, so two views of one database at two times line up row for row (`matrix_test`: *rows line up across time* — a removed rune's row is not given to the new one) |
| **adjacency** `A_r` | `n × n` CSR, float64 | `layout.edges` with relation `r`; row = from | **built.** One matrix per relation, **never summed across relations** — Core's warning that "supports, 0.8" and "speed, 900" are not commensurable is a type rule here. An undirected edge is stored both ways. The last of two edges of one relation between one pair is kept, and counted |
| **ports** | per edge | `i:j` relation labels | **stored** as edge columns (`from_port`, `to_port`); a per-port-pair split of `A_r` is not built |
| **values** `V` | `n × m` CSR | every edge whose `to` is a **measure** rune (attribute assertion, SPEC §3.7.1) | **built**, with each measure's unit from its `quantity`. *An attribute-assertion edge already is a sparse matrix entry.* Value edges are in V, never in `A_r` |
| **tags** `T` | `n × k` CSR, boolean | rune tags | **built**, columns in sorted order, each with the **sources** that wrote it (the Squid lesson) |
| **features** `X` | `n × f` dense | glyph fields annotated in `kinds` ([typed columns](/concepts/store.md)) | **built**: **ratio** → the number; **interval** → centred on its mean (no true zero; the centre is reported); **ordinal** → the number when it is one; **nominal** → a category index, categories reported. NaN where absent. Fields without a level are not features |
| **tag graph** `S` | `k × k` | `mantle.tags[…].near` | planned; a different graph from `A_r` and never mixed with it |
| **embeddings** `Z_e` | `n × d` per encoder | produced by Void Bicho | planned: stored with Bicho's provenance stamp, never in Core's state document |
| **transitions** | a sequence | Core's journal + `touched` | **waits on Core's `touched`** (world models phase 1). A snapshot carries configuration, not rate (`VoidCore:/sources/frozen-in-a-frame.md`); rate comes from here |

A cluster view costs what it holds: its edges are found from its runes through the source
index, never by reading the mantle's; an edge leaving it is counted as `boundary`.

# Operations as sparse linear algebra (`rancho/linalg.hpp`)

The GraphBLAS formulation (Davis, SuiteSparse:GraphBLAS, ACM TOMS Algorithms 1000 and 1037;
[storage research](/design/storage-research.md) §5): a graph algorithm is a sparse
matrix–vector product over a chosen **semiring**. Built as Rancho's own minimal CSR — the
fallback the research named for small devices — written as semiring products on purpose, so
SuiteSparse can stand behind the same functions when a database outgrows it.

| operation | semiring | function | checked against (`matrix_test`, seeded random graphs) |
|---|---|---|---|
| reachability, BFS | boolean (∨, ∧), masked | `bfs_levels` | a queue, 200 graphs |
| shortest path | tropical (min, +) | `shortest_paths` | Dijkstra, 200 graphs; a negative cycle reported |
| PageRank | (+, ×), weighted or not | `pagerank` | a dense power iteration, 120 runs, to 10⁻⁹ |
| weakly connected components | (min, second) over A + Aᵀ | `components` | union–find, 200 graphs |
| neighbour aggregation `A · X` | (+, ×), sum or mean | `aggregate` | a dense product |
| clustering (Leiden) | combinatorial | `Store::cluster` | planted partitions ([cluster](/concepts/cluster.md)) |

`aggregate` is one message-passing step — one GNN layer; the encoder of a graph JEPA
(Graph-JEPA) is a stack of these.

# The training export (`rancho::export_npy`)

Every array as a NumPy `.npy` file — CSR matrices as their `indptr`/`indices`/`data` — and a
`manifest.json` naming them, the rows, the relations, the units, the feature levels, the
categories and the unplaced edges. Readable with numpy and scipy and nothing else (checked
2026-10-30 by loading an export with numpy 2.x). The first form of rung 6; **Lance**, which
the world-model tooling reads, is the lean for the second ([open questions](/design/open-questions.md) §7).

# Why this is the JEPA bridge

- **Slots with identity** — rows of `ρ`, stable across time. Slot-JEPA and Causal-JEPA spend
  their effort recovering exactly that from pixels.
- **Relations per ordered pair** — `A_r`, typed. Inside-JEPA grounds such channels through
  inverse dynamics; the journal supplies the action for every change, so inverse dynamics is
  free — once `touched` gives the `Δ`.
- **Typed numbers** — `X` and `V`, scaled by measurement level, units known.
- **A cluster is a natural training example**: a connected subgraph of bounded size is the
  unit both a weak device and a batch of a model can hold.

# What it is not

- **Not a second source of truth.** Every array is derived from rows, rows from commands. A
  model writes nothing here except, later, embeddings — outputs, not data.
- **Not a way to change data.** An analytic result that should change something (a cluster
  tag, a suggested link) goes back as a **dispatcher command**, like every other change.

# Status

`current` for ρ, `A_r`, V, T, X, the algebra and the `.npy` export. Planned: S, embeddings,
transitions (on `touched`), a port-pair split, Lance.
