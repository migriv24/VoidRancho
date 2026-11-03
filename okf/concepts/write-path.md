---
type: Concept
title: Write path
description: Every row explainable by a command — how the store is kept in step with Void Core after each dispatch, the actor whose name the new tags carry, why the journal needs to say what a command touched, and the model-based test that proves it.
resource: src/working_set.cpp
tags: [status:current, audience:dev, audience:library, confidence:asserted, foundation]
timestamp: 2026-10-30T00:00:00Z
---

**Rancho never changes data.** Changes are dispatcher commands on a Void Core manager — from a
person, an agent, an importer, a sync, or Hormiga's Antfarm — and Rancho brings the store into
step with what Core then says. This is Rancho's railguard made mechanical
([boundaries](/design/boundaries.md)): every row a sync writes is one Core already holds.

# The loop (`rancho::WorkingSet`)

1. A command is dispatched on the [working set](/concepts/working-set.md)'s Core — through
   `WorkingSet::dispatch`, which forwards it unchanged and notes what it named.
2. `WorkingSet::sync` writes back what changed, in one store transaction:
   - **targeted** when every command since the last sync was one it can read (`set`,
     `setjson`, `facet`, `tag`, `measure`, `rune new`, `link`, `unlink`, `use`, `config set`,
     batches of them): the runes and pairs they named are re-read from Core (`cat`, `links`,
     `config` — reads) and written;
   - **full** otherwise: Core's whole export is compared with the last snapshot, rune by rune
     (followed by spirit id, so a rename is a rename) and edge by edge.
3. What a command does to rows Core never loaded — a rename's edges, a removal's edges, an
   undo bringing them back — is applied **in the store** with Core's semantics (soft deletion
   marked with what removed it; restored when that rune or mantle reappears).
4. A new rune may not take the name of a rune that is not loaded; `sync` refuses rather than
   overwrite a rune nobody looked at, and `name_taken` is what a caller minting names asks.

The Core is any **`CoreLink`** (`include/rancho/core.hpp`): dispatch, export, replace,
revision. A host keeps its own Core — Hormiga's is a `maiz::Core` with its glyphs, log sink
and undo depth — and adapts it; `VcCore` is the default over Core's C ABI. Rancho does not
depend on Void Maiz to have one. For a `maiz::Core` the adapter is Maiz's:
`maiz::RanchoLink` (`VoidMaiz:include/voidmaiz/rancho.hpp`, header-only, 2026-10-31). Its
export is the host's per-revision cached one, and its revision does not move on the reads a
sync sends (`cat`, `links`, `config`), so syncing costs the host neither its export nor its
scene.

**The actor.** `WorkingSet::set_actor("agent:needle")`: every tag a sync writes for the first
time carries that name as its [source](/concepts/store.md). A tag that was there keeps its own.

# Why the journal needs `touched`

Step 2's targeted path reads the **command** to learn what it touched, because Core's journal
records the action and not its effect. Void Core's planned `touched` record
(`VoidCore:/design/world-models.md` phase 1) — the ids a command changed, beside the ids it
minted — makes this exact for every verb, not only the ones listed, and it is the same `Δ` a
world model needs for its transitions. One addition serving both pillars; Rancho's standing
ask of Core.

# The test

**Model-based, inherited from Hormiga** (`tests/working_set_test.cpp`): the same random
commands against (a) a working set over one cluster of a store and (b) a Core holding
everything; after every command the store must export what (b) exports, including edges to
runes never loaded. 400 commands per walk — set, tag, rune new (in a batch), rune rm, rune
rename, link (with weights), unlink, undo, redo, cross-mantle batches, config — once through
the full comparison only and once through the fast path. Measured 2026-10-30: 399 commands
each; the fast path took 268 of them targeted; 47 renames, 4 restores, 27 removals; equal
after every one. Focused tests cover what a walk reaches rarely: a mantle removed and undone
keeps its 200 unloaded runes; a new rune cannot overwrite an unloaded one; the actor's name
on new tags.

# Status

`current`. Waiting on Core: `touched`.
