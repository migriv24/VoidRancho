---
type: Concept
title: Store
description: The database at rest — one row per rune, edge and tag, the whole JSON kept beside typed columns derived from glyph declarations, and the round-trip law that makes it safe.
tags: [status:planned, audience:dev, audience:library, confidence:asserted, foundation]
timestamp: 2026-10-30T00:00:00Z
---

The **store** is a database at rest: everything a Void Core state document holds, laid out so
that one rune can be found among millions without reading the rest. Rung 1 is Void Hormiga's
store (`VoidHormiga:/concepts/platform/store.md`), generalized.

# Shape

| table | one row per | indexed by | keeps |
|---|---|---|---|
| runes | rune | mantle, `spirit.id`, name, glyph, cluster path | its **whole JSON as Core writes it**, plus typed columns |
| edges | edge | mantle, from, to, relation | from-port, to-port, `weight` as a **float64 column**, directed, whole JSON |
| tags | (rune, tag) | tag, rune | the tag's **source** (author, importer, model, rule) |
| mantles | mantle | name | id, domain, rules, other layout keys |
| documents | remaining top-level key | key | `config`, `glyphs`, `active`, `scripts`, `domains`, `bindings` |
| blobs | asset | content hash | the bytes, by Palabra's content-addressed name — stored once |

**Typed columns come from the schema Core already has.** A glyph's `fields` and `kinds`
(quantity annotations: level, unit, min, max) decide which content fields become numeric or
categorical columns. Rancho never declares a schema of its own — a second copy of the schema
is the failure Core's glyph declarations exist to end (`VoidCore:/concepts/glyph.md`).

# Laws

- **Round trip.** Writing the store back out as a state document, whole or per mantle, gives
  back what went in. Property-tested, as Core's Scry round-trip law is.
- **Query parity.** A tag expression evaluated over the store returns exactly the runes Core's
  `ls --tag` returns — the test Core's MeshDB holiday passed 7/7. One grammar
  (`vc_tag_match`), compiled to SQL.
- **Order is not meaning.** Core preserves rune order but does not give it meaning
  (`VoidCore:SPEC.md §4`); the store keeps an order column for round trip and never sorts by it
  for anything else.

# Status

`planned`.
