---
type: Concept
title: Cluster
description: A connected group of runes of bounded size, found from the graph itself (Leiden, recursively) — the one unit Rancho pages, stores as a segment, analyses and exports. Weights are first-class input, one relation at a time, and the settings are recorded beside the result.
resource: src/store.cpp
tags: [status:current, audience:dev, audience:library, confidence:asserted]
timestamp: 2026-11-09T00:00:00Z
---

A **cluster** is a connected set of runes, small enough for a weak device, found from the
mantle's own edges: **Leiden** (Traag, Waltman & van Eck 2019 — unlike Louvain, every community
it returns is connected), run recursively inside each community until each leaf holds at most
`target` runes. Each rune's path is stored (`12.4.3`) in the `clusters` table — derived data,
like an index: rebuilt, never authored, not part of the document. A rune of a **measure**
glyph is filed `!` (always loaded) because every value edge points at one, and **value edges
are not ties**.

The implementation is Rancho's own, `include/rancho/graph.hpp` (namespace `rancho::graph`), since
the author ruled on 2026-11-02 that graph analysis lives here ([boundaries](/design/boundaries.md)
§3). Void Maiz wrote it, and it moved unchanged except for its namespace. It is the **leaf target**
`voidrancho_graph`, which links neither SQLite nor Core. `cmake/voidrancho_graph.cmake` defines it
once per build and can be included on its own. Void Maiz's `voidmaiz_graph` bridges to it, so a host
linking both libraries carries one copy. `tests/graph_test.cpp` links that target and nothing else.
The recorded settings say `"implementation": "rancho/graph.hpp"`. Settings recorded before the move
say `voidmaiz/graph.hpp`, which is the same code.

# One unit, five jobs

| job | built |
|---|---|
| paging the [working set](/concepts/working-set.md) — an edit costs O(cluster) | yes (`Selection::by_cluster`) |
| a [segment](/concepts/segment.md) on disk or a shared drive | yes: one segment per leaf |
| analysis — local algorithms on one block of the matrix | yes: a cluster [matrix view](/concepts/matrix-view.md) |
| a training example — a bounded connected subgraph | yes: the same view, exported |
| sync — a peer asks for the clusters it shows | to discuss with Palabra |

# Weights decide (`rancho::ClusterOptions`)

The author's *"clustering and edge weights as floats"* as a setting rather than a constant:

- `relations`: which edges count as ties (empty: every relation);
- `weighted`: read each edge's float `weight` as its strength. **Exactly one relation** may be
  read weighted — weights of different relations are not commensurable (Core's quantity page;
  SPEC §3.7.1: *"supports, 0.8"* and *"speed, 900"* have no meaningful sum) — and every weight
  must be positive. Both are refused, with the reason, rather than computed;
- `seed`, `resolution`, `target`: the same database clusters the same way on every device.

The settings are **recorded beside the result** (`Store::cluster_settings`, and in every
snapshot's manifest), so anyone reading the clusters knows what they mean.

`plan_test` plants two partitions at right angles — the true groups tied by a few strong
edges (5.0), a decoy by many weak ones (0.05). Read without weights, clustering finds the
decoy (NMI with the truth 0.001); read with them, the truth (NMI 1.0). On Hormiga's planted
database the unweighted clustering recovers the groups at NMI 1.0, the same leaves on every
run.

# A top anyone can browse (`ClusterOptions::fanout`, 2026-11-09)

Leiden at γ = 1 is right that a sparse graph has many small, nearly separate communities, and
modularity never merges what is not linked. On Void Hormiga's stress store the root had
**23,607 children**: no overview can start there (its "the overview", 2026-11-05). So `cluster`
adds levels **above** Leiden's communities, until no node of the tree (the mantle itself
included) has more than `fanout` children (default 250; 0 keeps Leiden's tree as it is). It
uses Void Maiz's `build_tree` steps, now `graph::bound_fanout` in the leaf target:

1. the tops are **merged along their strongest ties per member**, none past a size cap of
   max(largest, 4·n/fanout), until at most `fanout` remain;
2. what is still too many is **bundled**: the largest half stand alone, the rest share bundles;
3. any node with more than `fanout` children is **sliced**.

Bundles and slices are **packing**, and say so (`ClusterNode::kind`), so a view knows which
groups mean something. The new levels are prefixes and inserted digits; a leaf stays a leaf and
Leiden's communities are never cut. A `Selection` by an upper path loads every leaf under it.
Modularity is still reported for Leiden's own communities. (`plan_test`: 600 islands, fanout 16:
every node has at most 16 children, packing is marked, and pairs stay together; with no fanout,
over 100 tops.)

# The reseña (2026-11-09)

Per node of the tree (`""` is the mantle itself): the runes under it, its **internal** ties
(both ends under it), its **outgoing** ties (one end), with their weights; and per node, the
ties **between its children** (`cluster_ties`). A tie is an edge with both ends clustered;
value edges to measures are not ties. It is made when `cluster` runs, in one pass over the
edges in bounded memory, and **kept by write-back**: a link made or removed, a rune deleted,
restored or filed under a leaf adjusts exactly the nodes on its path. So an overview of the
whole potrero reads the bodies it draws and the ties between them, never the mantle's edges
(`cluster_node`, `cluster_children`, `cluster_ties`; `refresh_summaries` counts again).
`plan_test` checks the mantle's internal ties against the document, and after each of seven
edits through a working set (a link across the tree, a weighted second relation, an unlink, a
removal and its undo, a new rune, then filed) checks that what write-back kept equals a recount.

After a bulk load (a snapshot restored) the reseña is recounted; packing marks do not travel
with the rows, so a restored tree's bundles and slices read as communities until the mantle is
clustered again.

**Planned:** the rest of Hormiga's summary per node (counts by glyph and by tag, the date range,
the geographic bounding box) and sketches for what does not decompose.

# Status

`current`. Not yet: writing the leaves back as `c1:`/`c2:` tags — which, being data, would go
through the dispatcher as commands; a choice per database.
