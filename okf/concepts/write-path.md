---
type: Concept
title: Write path
description: Every row explainable by a command — how the store is kept in step with Void Core after each dispatch, why the journal needs to say what a command touched, and the model-based test that proves it.
tags: [status:planned, audience:dev, audience:library, confidence:asserted, foundation]
timestamp: 2026-10-30T00:00:00Z
---

**Rancho never changes data.** Changes are dispatcher commands on a Void Core manager — from a
person, an agent, an importer, a sync, or Hormiga's Antfarm — and Rancho brings the store into
step with what Core then says. This is Rancho's railguard made mechanical
([boundaries](/design/boundaries.md)).

# The loop

1. A command is dispatched on the [working set](/concepts/working-set.md)'s manager.
2. Core's journal records it (`command`, `pure`, `minted`).
3. Rancho writes back the rows that changed, inside one store transaction.
4. A command that names runes not loaded (a rename, a deletion, a `@tag` target over the whole
   mantle) is applied to the unloaded rows **in the store**, with Core's semantics.

Hormiga does step 3 today by re-asking Core (`cat <rune>`, `links <rune>`) for the runes a
command names, because **the journal records the action and not its effect**. Void Core's
planned `touched` record (`VoidCore:/design/world-models.md` phase 1) closes that: the ids a
command changed, beside the ids it minted. It is the same change a world model needs for its
transitions — one addition serving both pillars.

# The test

**Model-based, inherited from Hormiga:** run the same random command sequence against (a) a
working set over a store and (b) a Core manager holding everything; after every command the
two must export the same document, including edges to unloaded runes. The random walk includes
the commands that rename, remove and replace (`rune rm|rename|dup`, `undo`, `redo`, `revert`,
a failing `batch`) — the same set Core 0.2.15's ref-index test exercises.

# Status

`planned`.
