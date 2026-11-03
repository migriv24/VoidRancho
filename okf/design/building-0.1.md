---
type: Design
title: Building 0.1 — the leans taken, and what the building found
description: "The first build of Void Rancho (2026-10-30, rungs 1–5 and the first form of rung 6): which open-question leans were taken to build at all and are provisional until the author rules; the seams chosen (CoreLink, Leiden in place, the tag grammar decided by Core); and four findings — a MinGW path bug that would have opened every NAS store in WAL, a query-planner trap, link updating in place, and Hormiga's format-1 stores opening unchanged."
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-10-30T00:00:00Z
---

# Building 0.1

The author's direction on 2026-10-30 was to begin full development, with Void Hormiga's
database work as the thing to copy and the other family repositories in mind. Building needed
answers to questions this bundle had left open; each was answered with the lean already
written in [open questions](/design/open-questions.md), and **each stays the author's to
overrule** — none is hard to reverse yet, because nothing links Rancho.

## 1. Leans taken

| question | lean taken | where it shows |
|---|---|---|
| §5 language | C++20 inside, a **C ABI** at the edge (`rancho/rancho.h`), SQLite vendored; never linked by Core | `CMakeLists.txt`, `capi_test` |
| §4 system of record | **the store is the baseline**: an import replaces it; a full document is an export | `Store::import`, `state` |
| §6 shared media | detect a network path; open it single-host in rollback mode with an exclusive lock; steer sharing to segments | `media.hpp`, `media_test` |
| §7 training export | **NumPy `.npy` first** (readable with nothing but numpy), Lance second | `export_npy` |
| §3 graph analytics | **not moved.** Rancho compiled Void Maiz's `graph.cpp` in place for Leiden (since 2026-10-31 it links Maiz's `voidmaiz_graph` target instead); its own algebra is new code, not Maiz's | `RANCHO_HAVE_LEIDEN` |
| §2 the line with Palabra | not settled; segments use **Palabra's hash** (SHA-256) so the two can meet later | `sha256_hex` |

## 2. Seams chosen

- **`CoreLink`, not `maiz::Core`.** Hormiga's store took a `maiz::Core`. Rancho takes an
  interface of four calls (dispatch, export, replace, revision), because the host already owns
  its Core — with its glyphs, sinks and undo depth — and because a database library depending
  on a *view* library would be a dependency in the wrong direction. `VcCore` is the default,
  straight over Core's C ABI. Hormiga's adapter turned out to be Maiz's to write, and Maiz
  wrote it: `maiz::RanchoLink` (2026-10-31).
- **Core decides every query.** SPEC §5 says hosts MUST evaluate tag expressions through
  `vc_tag_match`. Compiling the grammar to SQL — this bundle's original plan — would have been
  a second implementation of it. Instead SQL only *narrows* (a superset, from the expression's
  AND/OR shape) and Core decides each candidate ([query](/concepts/query.md)).
- **Everything added is derived.** Weights, ports, typed fields, the row index: read off what
  Core wrote. No Rancho schema exists to drift from the glyphs.
- **The SPEC §6.1 codec is Core's.** `rancho::arg` and `split_argv` forward to Core, for the
  reason Void Maiz gives at length: five codebases implemented §6.1 by hand and all five got
  it wrong.

## 3. Findings

**(a) MinGW's `std::filesystem::absolute` drops the UNC prefix.** `\\nas\share\org.store`
becomes `C:\nas\share\org.store`. A first draft of `detect_medium` normalized the path before
asking where it lived, and so judged every NAS path local — which would have opened Hormiga's
clients' shared stores in WAL mode, the one thing the research says must not happen. Caught by
`media_test` on its first run; Rancho now reads the spelling first and resolves with
`GetFullPathNameW`. **Any family host normalizing paths this way has the same bug** — told to
Hormiga.

**(b) The planner trap, again.** `WHERE r.id IN (<candidates>) ORDER BY r.pos` made SQLite walk
the whole mantle in position order to avoid a sort: 39 ms at 8×10⁴ runes for a three-rune
answer, 13× its time at 2×10⁴. Driving the query from the candidates (a CROSS JOIN fixes the
order) made it 0.19 ms at both sizes. Hormiga's selections fell into the same trap twice
(its comments in `write_state`); the general rule: **when the answer is small, say which table
leads.**

**(c) `link` on an existing pair updates it.** Several tests assumed `n` link commands make
`n` edges. They do not when a pair and relation repeat — Core updates the edge in place. The
tests now count edges from the store.

**(d) A Hormiga store opens unchanged.** Format 1 (Hormiga 0.2.3's tables) is migrated in place
on open, every column filled from the JSON its rows already keep; the document it writes is the
same. Adoption need not touch a single client file.

## 4. Measured (windows-x64, GCC 15, RelWithDebInfo)

| what | 2×10⁴ runes | 8×10⁴ runes |
|---|---|---|
| import, extra memory | +10.0 MB | +10.4 MB |
| import time | 0.6 s | 2.6 s |
| 200 runes as a selection | 1.1 ms | 1.2 ms |
| a query by names | 0.18 ms | 0.19 ms |

6×10⁴ runes snapshot in 3.4 s and restore in 3.4 s with flat memory. The write path: 399 random
commands per walk, the store equal to a whole-database Core after each.

## 5. Not built, and why

- **Transitions** wait on Core's `touched`; nothing is gained by parsing commands harder.
- **The bucket adapter** (tier 3) needs an HTTP client and credentials, which is a host's
  choice of stack (Hormiga has one in its networking via Maiz/Hammer); the layout is ready.
- **Embeddings** are Bicho's to compute; storing them waits on Bicho's provenance stamp.
- **A Python binding** waits on a Python host asking; the C ABI is its foundation.
