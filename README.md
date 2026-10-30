# Void Rancho

**The database layer for Void Core applications.** Founded 2026-10-30. A **library**, not
an application — nothing is built yet; the OKF comes before the code, as always.

*Rancho* — a ranch: a big place where a lot of living things are kept, counted, moved
between pastures and looked after. The runes are the herd.

One sentence: **Void Rancho stores, indexes, queries and pages runes and mantles at any
size — on a laptop, a phone, a NAS or object storage — and can hand a mantle to a learned
model as the matrices it reads, without ever becoming a second way to change the data.**

## Why it exists

Every application built on Void Core has had to build its own database:

- **Void Hormiga** built a SQLite store with a cluster-paged working set (2026-10-27),
  after a client's 5×10⁵-rune database peaked at 22 GB of memory.
- **Void Squid**'s `assets.db` copies Hormiga's schema — runes, fields, tags, links.
- **Void Loops Studio**'s project is a mantle, and its audio will be data too.

The data *model* was always shared: it is Void Core. The *database* never was. Void Core
does no I/O by definition, so it cannot be the database — and it should stay pure. Rancho
is the layer above it, the way Void Palabra (sync and history) and Void Maiz (the view)
are.

## The idea worth having early

**A mantle is already the shape a database and a learned world model both want**:
entities with identity across time (`spirit.id`), typed relations between ordered pairs
(`layout.edges`), typed numbers (quantity annotations), and changes labelled by the action
that caused them (Core's journal). Rancho's job is to store that shape at scale and to
expose it two ways — as **rows** (for queries) and as **sparse matrices** (for clustering,
graph algorithms, and JEPA-class models). See
[okf/concepts/matrix-view.md](okf/concepts/matrix-view.md).

## The one rule

**Every row is explainable by a command.** Changes enter through Void Core's dispatcher —
a person, an agent, an importer and Hormiga's Antfarm alike — and Rancho keeps the store in
step with what Core says. Rancho never invents data, and never keeps a second schema: a
rune's typed columns come from its glyph's declaration.

## Status

**Nothing is built.** Every concept is `status:planned`, per the honesty convention
inherited from Void Core. Start reading at **[okf/index.md](okf/index.md)**.

## Siblings

- **[Void Core](../VoidCore/)** — the structure and the command language. Rancho owns none
  of its vocabulary.
- **[Void Palabra](../VoidPalabra/)** — canonical bytes, names, merge, history, sync. Rancho
  stores; Palabra names and reconciles.
- **[Void Bicho](../VoidBicho/)** — runs models. Bicho computes embeddings; Rancho keeps and
  indexes them.
- **[Void Hormiga](../VoidHormiga/)** — the first client, whose store is rung 1.
