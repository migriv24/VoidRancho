---
type: Concept
title: Working set
description: What Void Core holds in memory — a budgeted subset of the store, loaded by cluster, so a database larger than the device still opens and edits fast.
resource: src/plan.cpp
tags: [status:current, audience:dev, audience:library, confidence:asserted]
timestamp: 2026-10-30T00:00:00Z
---

A Void Core manager holds one whole state document, and every edit pays costs proportional to
it (the undo memento, the export). The **working set** is the part of the store Core is given:
small mantles whole, large mantles **by [cluster](/concepts/cluster.md)**, and the edges whose
both ends are loaded (`Store::state(Selection)`).

# The budget (`rancho::Budget`, `rancho::plan`)

A share of the device's memory — an eighth of physical RAM, clamped to 128 MB–4 GB
(`rancho/memory.hpp` reads it) — that counts **everything** the working set costs. Measured
by Hormiga on its 5×10⁵-rune stress database, per byte of state text: Core's tree ×6.1, the
projected scene ×5.8, each undo frame ×5.5, the export ×1, a `save`'s baseline ×5.5, transient
parses ×6 — about (25 + 5.5 × undo depth) times the text. The undo depth is part of the
budget: 4 on a small machine, 16 on a large one.

**The plan.** A database that fits loads whole, exactly as before the store. One that does not
keeps its small mantles whole and loads the large ones by cluster: the leaf holding the rune in
focus, then its neighbours in the hierarchy while they fit. A large mantle not clustered yet
loads its first runes in order. (`plan_test`: 151 KB of state for a 174 KB budget, 181 of 482
runes, the measure with them; the rune in focus always in the plan.)

# Changing it

Loading a different selection rebuilds Core from the store (`CoreLink::replace_state`): a
change of working set is a **reload, never an edit**, and does not enter the journal. Undo
history does not survive a reload while Core's undo is memento-based — another reason for
Core's reified commands. Rows the last session soft-deleted are purged first, since no undo
can reach them any more.

# Status

`current`. Ported unchanged from Hormiga (its `plan.hpp`), with the budget's figures still
Hormiga's measurements; a Rancho host whose projection costs differ should measure its own.
