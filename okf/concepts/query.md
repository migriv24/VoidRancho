---
type: Concept
title: Query
description: Reading without loading — runes found by SPEC §5 tag expression (exactly what `ls --tag` answers, decided by Core's own evaluator), by typed-field range, and by page; how the tag index narrows without reimplementing the grammar.
resource: src/store.cpp
tags: [status:current, audience:dev, audience:library, confidence:asserted]
timestamp: 2026-10-30T00:00:00Z
---

Anything that **shows or counts** the database — a table by page, a calendar by month, a
search, a publish — reads the store, not Core ([working set](/concepts/working-set.md)).
Three reads exist:

| read | answers | cost |
|---|---|---|
| `Store::select(mantle, expr, limit, offset)` / `count` | the runes a tag expression selects, in rune order | what its narrowest atoms select |
| `Store::range(mantle, field, lo, hi)` | runes whose [typed field](/concepts/store.md) is in a range, in value order | an index range |
| `Store::page(mantle, offset, bytes)` | runes in order up to a byte budget | the page |

# One grammar, never two

Void Core's SPEC §5: hosts that filter by tag expression **MUST evaluate through
`vc_tag_match`** rather than reimplement the grammar. Rancho obeys it literally: **every
candidate is decided by Core's evaluator**, given the rune as `ls --tag` sees it — its tags,
its name (name-as-tag), and `glyph:<g>`.

What Rancho reads of the expression is only its **AND/OR shape**, to ask the tag index for a
**superset** of the answer: an atom's candidates are the runes carrying that tag, named it,
or of that glyph; AND intersects; OR unites; NOT — or anything not followed — gives up
narrowing, which is always safe. The tokenizer and the descent copy `VoidCore:core/src/tags/tag.c`
(0.2.15) step for step, implicit AND and the stray-`)` rule included, so the shape read is
the shape Core evaluates. A rune Core would *reject* can never slip through (Core decides);
the one way to be wrong is a narrowing that *excludes* a true match, which is what
`query_test` hunts:
**3 000 random expressions** built to reach the grammar's corners (implicit AND, `NOT`/`!`,
parentheses left open, operators in any case, `&&`/`||` mid-word as ordinary tag characters,
a stray `)`, name-as-tag, `glyph:`, another mantle's tag, a soft-deleted rune), every answer
equal to `ls --tag`'s in content and order.

# A query costs what it finds

Narrowed, the query is driven *from* the candidates. Written the obvious way (`id IN (...)`)
SQLite's planner walked the whole mantle in position order to skip a sort — the trap
Hormiga's selections fell into, here 13× slower at 8×10⁴ runes. With the join order fixed, a
query by names or rare tags takes 0.19 ms at 2×10⁴ runes and at 8×10⁴. An AND is as narrow
as its narrowest side; a NOT alone reads the mantle.

# Status

`current`. Not yet: `--kind` as a filter (it lives on the glyph, not in tags), and a
host `where`-predicate seam for "near" over embeddings (`VoidCore:/design/host-extension-seams.md`).
