# Void Rancho

**The database layer for Void Core applications.** A **library**, not an application.
Founded 2026-10-30; **0.1.0** the same day.

*Rancho* — a ranch: a big place where a lot of living things are kept, counted, moved
between pastures and looked after. The runes are the herd.

One sentence: **Void Rancho stores, indexes, queries and pages runes and mantles at any
size — on a laptop, a phone, a NAS or object storage — and can hand a mantle to a learned
model as the matrices it reads, without ever becoming a second way to change the data.**

## Why it exists

Every application built on Void Core had to build its own database:

- **Void Hormiga** built a SQLite store with a cluster-paged working set (2026-10-27),
  after a client's 5×10⁵-rune database peaked at 22 GB of memory.
- **Void Squid**'s `assets.db` copies Hormiga's schema — runes, fields, tags, links.
- **Void Loops Studio**'s project is a mantle, and its audio will be data too.

The data *model* was always shared: it is Void Core. The *database* never was. Void Core
does no I/O by definition, so it cannot be the database — and it should stay pure. Rancho
is the layer above it, the way Void Palabra (sync and history) and Void Maiz (the view) are.
Rung 1 is Hormiga's store, extracted — its API kept, so Hormiga moves by changing a
namespace, and its existing store files open and migrate in place.

## The one rule

**Every row is explainable by a command.** Changes enter through Void Core's dispatcher —
a person, an agent, an importer and Hormiga's Antfarm alike — and Rancho keeps the store in
step with what Core says. Rancho never invents data and never keeps a second schema: a
rune's typed columns come from its glyph's declaration, and every query is decided by
Core's own tag evaluator.

## What 0.1.0 does

| | | |
|---|---|---|
| **store** | a whole state document as SQLite rows; the round trip exact; import memory flat as the file grows | [store](okf/concepts/store.md) |
| **queries** | tag expressions answered exactly as `ls --tag`; typed-field ranges; pages | [query](okf/concepts/query.md) |
| **write path** | write-back after each dispatch, through any Core; the actor's name on new tags | [write path](okf/concepts/write-path.md) |
| **working set** | a database larger than the device opens and edits by cluster, within a memory budget | [working set](okf/concepts/working-set.md) |
| **clusters** | Leiden, recursively; edge weights first-class, one relation at a time; settings recorded | [cluster](okf/concepts/cluster.md) |
| **matrix view** | ρ, one sparse matrix per relation, values, tags, typed features; BFS, shortest paths, PageRank, components, A·X; a `.npy` export | [matrix view](okf/concepts/matrix-view.md) |
| **shared media** | a store on a NAS path is single-host by lock; snapshots as content-named segments | [tiers](okf/concepts/tiers.md), [segment](okf/concepts/segment.md) |

Not yet: transitions (waits on Core recording what a command `touched`), object-storage
buckets, embeddings, Lance, a Python binding. See [the roadmap](okf/roadmap.md).

## Build

Beside `../VoidCore` (linked). Graph analysis (Leiden, its hierarchy, PageRank, components,
NMI) is Rancho's own leaf target, `voidrancho_graph` (`rancho/graph.hpp`), which links nothing:

```
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
```

Built and tested on windows-x64 (MSYS2 ucrt64, GCC 15).

As a subdirectory of a host that also adds Void Maiz, in either order: `VOIDCORE_ROOT` may
name Core's repository or its `core/` directory, a parent's `voidcore` target is used rather
than a second Core, `voidrancho_graph` is defined once (`cmake/voidrancho_graph.cmake`, which
Void Maiz's bridge also includes), and Rancho's tests are registered only with
`-DRANCHO_BUILD_TESTS=ON` (on by default when Rancho is built on its own). A host with a
`maiz::Core` passes it as `maiz::RanchoLink` (`voidmaiz/rancho.hpp`).

## Use

```cpp
#include "rancho/store.hpp"
#include "rancho/working_set.hpp"
#include "rancho/plan.hpp"
#include "rancho/memory.hpp"

rancho::Store store("org.store");                       // opens or creates; single-host on a NAS
rancho::JsonStream in("org.json");                      // a state document of any size
store.import(in);

auto names = store.select("contacts", "group:science AND NOT status:archived");

rancho::VcCore core;                                    // or the host's own Core, as a CoreLink
rancho::WorkingSet ws(store, core);
ws.load(rancho::plan(store, rancho::Budget::for_machine(rancho::memory::physical_bytes())).sel);
ws.set_actor("person:ada");
ws.dispatch("tag maria +status:called");                // the dispatcher is still the only door
ws.sync();                                              // the store says what Core says
```

A C host uses `include/rancho/rancho.h`.

## Status and documentation

Start at **[okf/index.md](okf/index.md)**; what was decided to build, and what building
found, is [okf/design/building-0.1.md](okf/design/building-0.1.md).

## Siblings

- **[Void Core](../VoidCore/)** — the structure and the command language. Rancho owns none
  of its vocabulary, and is never linked by it.
- **[Void Palabra](../VoidPalabra/)** — canonical bytes, names, merge, history, sync. Rancho
  stores; Palabra names and reconciles. Segments use Palabra's hash.
- **[Void Bicho](../VoidBicho/)** — runs models. Bicho computes embeddings; Rancho keeps
  them and hands Bicho the matrices.
- **[Void Maiz](../VoidMaiz/)** — the view. Its graph analysis moved here (2026-11-02);
  `voidmaiz/graph.hpp` forwards to ours. Maiz ships the `CoreLink` adapter for its Core
  (`maiz::RanchoLink`).
- **[Void Hormiga](../VoidHormiga/)** — the first client, whose store is rung 1.
