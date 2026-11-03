# Void Rancho — agent instructions

You are working on **Void Rancho**, the database layer for Void Core applications: a C++20
library with a C ABI, one layer above **Void Core** (`../VoidCore`, linked at its C ABI) and
never linked by it. Read `okf/index.md` first, then `okf/design/boundaries.md`.

## Ground rules

1. **OKF first.** `okf/` is the source of truth for design; code follows concepts. Every
   session that changes design or code adds an entry to `okf/log.md` and trues up the concept
   pages it touched. A concept may claim `status:current` only with a `resource:` link to the
   code that backs it, and says what of it is still planned.
2. **The railguard: could every row be rebuilt by replaying commands?** Rancho never changes
   data. Every change is a Void Core dispatcher command; the store is brought into step with
   what Core then says. Nothing here may become a second door — not an API, not
   `Store::handle()` (read-only by contract), not the C ABI.
3. **Rancho owns none of Core's vocabulary, and no schema of its own.** Typed columns come
   from glyph `kinds`; tag expressions are decided by `vc_tag_match` (SPEC §5); the §6.1 codec
   is Core's (`rancho::arg`, `split_argv` forward to it). Never reimplement either.
4. **Hormiga's API is a promise.** `Store`, `Selection`, `JsonStream`, `Plan`, `Budget` and
   `WorkingSet` keep the members and meanings of Void Hormiga's `src/store`, so Hormiga moves
   by a namespace change. Add; do not rename. A store Hormiga wrote (format 1) must keep
   opening and migrating in place.
5. **Vendor, don't depend.** SQLite and nlohmann/json are vendored with their notes
   (`vendor/*/LICENSE.md`, `THIRD-PARTY-NOTICES.md`), the same builds as Hormiga's. Siblings
   are built from their own repositories (`VOIDCORE_ROOT`, `VOIDMAIZ_ROOT`), never copied —
   Maiz's `graph.cpp` is compiled in place until the graph-analytics home is settled.
6. **Upstream messages, not upstream edits.** Rancho uses Core and Maiz and never patches
   them. Needs are drafted at the repo root as
   `MESSAGE_FOR_<RECIPIENT>_rancho-<topic>-<YYYY-MM-DD>.md` (gitignored: correspondence, not
   project); replies arrive as `MESSAGE_FOR_VOIDRANCHO_<sender>-<topic>-<date>.md`. A consumed
   message is folded into `okf/log.md` and deleted.
7. **Nothing from a deployment is committed.** No database, export, client name, slug, count
   or path from anyone's use of a Void application — `.gitignore` refuses `*.db`, `*.store`
   is created only in the temp folder by tests. Write a finding as a fact about Rancho.
8. **The author decides.** Open questions go to `okf/design/open-questions.md` with a lean.
   A lean taken in order to build is marked as taken and stays overrulable.

## Build and test

```
cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build
```

One test executable per area (`tests/*_test.cpp`), one function per promise, named for what
it promises. The model-based write-path test and the `ls --tag` parity fuzz are the two that
guard the railguard; never weaken either to make a change pass.
