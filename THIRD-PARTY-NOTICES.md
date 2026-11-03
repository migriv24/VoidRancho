# Third-party notices

Void Rancho is MIT licensed (see [LICENSE](LICENSE)). It **vendors** its
dependencies rather than fetching them — no package manager, no CDN — so every
third-party component ships in this repository with its own license beside the
code it covers. This file is an index; those license texts are authoritative.

| component | where | license |
|---|---|---|
| SQLite (amalgamation) | `vendor/sqlite/` | public domain — `LICENSE.md` |
| nlohmann/json | `vendor/json/` | MIT — `LICENSE.md` |

Void Core and Void Maiz are sibling projects, not vendored copies — they are
built from their own repositories beside this one (`../VoidCore`,
`../VoidMaiz`). From Void Maiz only `src/graph/graph.cpp` (the Leiden
implementation) is compiled, in place. See [`void.json`](void.json).
