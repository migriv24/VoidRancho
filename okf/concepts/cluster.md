---
type: Concept
title: Cluster
description: A connected group of runes of bounded size, found from the graph itself (Leiden, recursively) — the one unit Rancho pages, stores as a segment, analyses and exports. Weights are first-class input, one relation at a time, and the settings are recorded beside the result.
resource: src/store.cpp
tags: [status:current, audience:dev, audience:library, confidence:asserted]
timestamp: 2026-11-02T00:00:00Z
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

# Status

`current`. Not yet: writing the leaves back as `c1:`/`c2:` tags — which, being data, would go
through the dispatcher as commands; a choice per database.
