---
type: Concept
title: Working set
description: What Void Core holds in memory — a budgeted subset of the store, loaded by cluster, so a database larger than the device still opens and edits fast.
tags: [status:planned, audience:dev, audience:library, confidence:asserted]
timestamp: 2026-10-30T00:00:00Z
---

A Void Core manager holds one whole state document, and every edit currently pays costs
proportional to it (the undo memento, the export). The **working set** is the part of the
store Core is given: small mantles whole, large mantles **by [cluster](/concepts/cluster.md)**,
and the edges whose both ends are loaded.

# The budget

A share of the device's memory — Hormiga's rule is an eighth of physical RAM, clamped to
128 MB–4 GB — that counts **everything** the working set costs: Core's tree, its undo frames,
the projected scene, any exported text, Rancho's own indexes. Better hardware loads more
clusters; weak hardware loads fewer and waits more. The smallest machine Hormiga promises has
2 GB of RAM.

# Changing it

Loading or dropping a cluster rebuilds the Core manager from the store (Void Maiz's
`replace_state`), so a change of working set is a **reload, never an edit**, and it does not
enter the journal. Undo history does not survive a reload while Core's undo is memento-based
— another reason for Core's reified commands, held on Core's roadmap.

# Reading without loading

Anything that **shows or counts** the whole database — a table by page, a calendar by month, a
map by viewport, a search, a publish — reads the store directly. What is loaded is only what
can be edited without a wait.

# Status

`planned`.
