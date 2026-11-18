---
type: Concept
title: Rodeo and embarque
description: Reading the whole potrero in bounded memory, resumably (el rodeo), and writing what it makes as one thing in many content-named files (el embarque, in partidas). What an export needs when the database is larger than memory and the output larger than a file.
resource: src/rodeo.cpp
tags: [status:current, audience:dev, audience:library, confidence:asserted]
timestamp: 2026-11-09T00:00:00Z
---

**Why.** The author (2026-11-03, relayed by Void Hormiga's "el lindero"): an export that turns
a database into another modality, such as a newsletter, a site or a compiled game, *"has to be
able to completely traverse the database and encapsulate it fully"*, and a petabyte output
should not be a petabyte file. An export reads **the potrero, never the corral**: the working
set holds a part, and an answer made from a part is not a finished export.

# El rodeo: a pass over everything

`rancho::Rodeo` visits every rune and every edge of a store **a herd at a time**, in the
store's own order. For each mantle, in order:

1. its always-loaded runes (measures), as one herd `"!"`;
2. each cluster leaf, in path order (one index step per leaf);
3. its runes in no leaf (made after the clustering, or a mantle never clustered), in pages of
   `RodeoOptions::page_bytes`;
4. the edges whose source is no live rune (SPEC §3.7 allows them), in pages.

Each edge comes **once**, with the herd that holds its source. Memory is one herd's: a leaf
holds at most the clustering's `target` runes. `cursor()`, taken after any herd, is a short
text (`rodeo/1 <mantle> <stage> <key>`) that a new `Rodeo`, in this process or another, resumes
from, so a stopped pass is not lost. Until `done()`, what a rodeo has made is **suelto**. After
it, it is **cabal**, because nothing was left unread. A rodeo only reads. For an answer as of
one moment while the original takes edits, run it on a `Store::vacuum_into` copy.

# El embarque: one thing, many files

The output of a rodeo is an **embarque**: one thing in identity, many in storage, which is the
[segment](/concepts/segment.md) design applied to outputs.

```
<dir>/partidas/<sha256>         one partida's bytes, never changed once written
<dir>/embarques/<sha256>.json   a manifest: format voidrancho.embarque/1, the host's meta, the partidas in order
```

- **A partida** is cut by the **output's** structure (a site's pages, a game's levels, an asset
  pack), not the database's. The writer does not care what it holds.
- It is **content-named**: identical bytes are stored once, across partidas and across
  embarques, and a resumed export rewrites nothing that is already there.
- **The manifest's hash names the embarque.** So a petabyte embarque is one name, one version
  and one signature, and never one file. It is written, copied, verified and served a partida
  at a time. The same partidas with the same meta give the same name.
- Reading checks every file against its name: a damaged partida or manifest is reported, never
  returned (`read_partida`, `read_embarque`).

**El molino**, an output computed when asked (a page rendered on request, a world streamed as a
player moves), needs no new mechanism. It is a host reading the store, and stays possible beside
the embarque.

# Tests (`tests/rodeo_test.cpp`)

1. A rodeo reads every rune and every edge **exactly once**: a measure, clustered leaves, runes
   made after the clustering, an edge from a name that is no rune, and a mantle never clustered.
   No herd is larger than a leaf or a page.
2. A rodeo stopped after 1, 2, 5, 9 or 14 herds and resumed from its cursor by a new `Rodeo`
   reaches the same end. A text that is not a cursor is refused.
3. An embarque stores identical partidas once, refuses a repeated name, rewrites nothing when
   resumed, gets the same name for the same content, and refuses a damaged partida.

# Planned

- An export that **writes** partidas while a rodeo reads, with a resumable pair of cursors (the
  rodeo's and the embarque's), as one helper. Today a host holds both.
- A bucket adapter for an embarque, the same as for segments (tier 3).
- **Passes for recursive questions** (reachability, components, PageRank, clustering itself)
  as rodeos with state kept between passes, with dynamic-frontier Leiden (Sahu 2024) to keep the
  clusters current. Void Hormiga's ask 6 in "the frontier".
- A rodeo of a whole **database** (every part of a deed), not only a store.
