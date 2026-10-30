---
type: Concept
title: The matrix view — what a database looks like to a JEPA model
description: "A mantle read as sparse matrices: a stable row index over runes, one adjacency matrix per relation (float weights), a rune×tag incidence, a feature matrix derived from quantity annotations and attribute-assertion edges, embeddings beside them, and transitions from the journal. Graph algorithms as sparse linear algebra. Answers the author's question: a database looks to a JEPA very much like runes and mantles."
tags: [status:planned, audience:dev, confidence:exploratory, foundation]
timestamp: 2026-10-30T00:00:00Z
---

> The author: *"what would a database look like to a jepa model? i think it would look very
> similar to our runes and mantle structure!"*

**Yes — and the reason is specific.** A JEPA world model needs an object-structured state
(entities with identity across time), relations between them, numbers it can embed by type,
and transitions labelled by actions (`VoidCore:/design/world-models.md` §4). A mantle has all
four by construction. What it lacks is the **array form** a model, a clustering algorithm or a
GPU reads. The matrix view is that form, derived — never stored as a second truth.

# The arrays

For one mantle with `N` runes:

| array | shape | built from | notes |
|---|---|---|---|
| **row index** `ρ` | `spirit.id → 0..N-1` | rune ids | **persistent and Rancho's own.** Core's rune order is preserved but not semantic (`VoidCore:SPEC.md §4`), so a row number may not be read off document order. Stable across loads so matrices from two times line up — that is what makes a transition a difference of two arrays |
| **adjacency** `A_r` | `N × N`, sparse | `layout.edges` with relation `r` | **one matrix per relation**, weights as `float64`. Never summed across relations: Core's own warning that "supports, 0.8" and "speed, 900" are not commensurable (`VoidCore:/concepts/quantity.md`) becomes a type rule here |
| **ports** | per edge | `i:j` relation labels | stored as columns on the edge table (from-port, to-port), so `A_r` can be split per port pair when a model wants interaction-net structure |
| **values** `V` | `N × M`, sparse | edges whose target is a **measure** rune (attribute assertions, `VoidCore:SPEC.md §3.7.1`) | entity × measure → value. *An attribute-assertion edge already is a sparse matrix entry;* the unit comes from the measure rune's `quantity` |
| **features** `X` | `N × F` | glyph fields annotated in `kinds` | typed by measurement level: **ratio** → float (log optional), **interval** → float centred (no true zero), **ordinal** → rank, **nominal** → category id / one-hot. Fields without a level are not features |
| **tags** `T` | `N × K`, sparse boolean | rune tags (+ name-as-tag, `glyph:`) | with per-tag **provenance** (who wrote it — the Squid lesson) |
| **tag graph** `S` | `K × K` | `mantle.tags[…].near` | similarity; a different graph from `A_r` and never mixed with it |
| **embeddings** `Z_e` | `N × d` per encoder `e` | produced by Void Bicho | stored with the provenance stamp (model, params, seed); never in Core's state document |
| **transitions** | a sequence | Core's journal (`command`, `minted`, and — once Core adds it — `touched`) | `(a_t, Δ_t)`: the action and the rows it changed. A snapshot carries configuration, not rate (`VoidCore:/sources/frozen-in-a-frame.md`); rate comes from here |

# Operations as sparse linear algebra

Most graph algorithms a database needs are sparse matrix–vector or matrix–matrix products
over a chosen **semiring** — the GraphBLAS formulation (Davis, SuiteSparse:GraphBLAS, ACM TOMS
Algorithm 1037; see [storage research](/design/storage-research.md) §5):

| operation | semiring | uses |
|---|---|---|
| reachability, BFS | boolean (∨, ∧) | "everything connected to this contact" |
| shortest path | (min, +) over weights | routing, distances between runes |
| PageRank, centrality | (+, ×) | importance; FaultSack's ranking |
| label propagation | (max/argmax, ·) | fast clustering seeds |
| neighbour aggregation `A_r · X` | (+, ×) | **message passing** — one GNN layer; the encoder of a graph JEPA (Graph-JEPA) is a stack of these |
| clustering (Leiden) | — | combinatorial, not a single product; Void Maiz's C++ implementation is the reference (`VoidMaiz:voidmaiz/graph.hpp`) |

# Why this is the JEPA bridge

- **Slots with identity** — rows of `ρ`, stable across time. Slot-JEPA and Causal-JEPA spend
  their effort recovering exactly that from pixels.
- **Relations per ordered pair** — `A_r`, typed. Inside-JEPA grounds such channels through
  inverse dynamics; the journal supplies the action for every change, so inverse dynamics is
  free.
- **Typed numbers** — `X` and `V`, scaled by measurement level, units known.
- **Transitions** — the journal, as deltas on stable rows.
- **A cluster is a natural training example**: a connected subgraph of bounded size
  ([cluster](/concepts/cluster.md)) is the unit both a weak device and a batch of a model can
  hold.

# What it is not

- **Not a second source of truth.** Every array is derived from rows, rows from commands. A
  model writes nothing here except embeddings, and those are outputs, not data.
- **Not computing the application.** Rancho is not Core, so the railguard does not forbid
  computing; but an analytic result that should change data (a cluster tag, a suggested link)
  goes back as a **dispatcher command**, like every other change.

# Status

`planned`. Order of building: `ρ` and `A_r` (needed by clustering), then `T`, `V`, `X`, then
embeddings and transitions. Export to a columnar file for training is
[storage research](/design/storage-research.md) §3.
