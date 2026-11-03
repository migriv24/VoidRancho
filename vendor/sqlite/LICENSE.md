# SQLite — vendored amalgamation

- **What**: `sqlite3.c` / `sqlite3.h`, the SQLite 3.53.3 amalgamation, copied
  2026-10-30 from Void Hormiga's `vendor/sqlite/` (itself downloaded 2026-07-19
  from https://www.sqlite.org/2026/sqlite-amalgamation-3530300.zip). The same
  build as Hormiga's on purpose: a store either one writes, the other opens.
- **License**: SQLite is in the **public domain** (https://www.sqlite.org/copyright.html):
  "Anyone is free to copy, modify, publish, use, compile, sell, or distribute
  the original SQLite code, either in source code form or as a compiled
  binary, for any purpose, commercial or non-commercial, and by any means."
- **Why vendored**: one C file, in-process, no server — rung 1's engine
  (okf/design/storage-research.md §1). Vendor, don't depend.
- **Local changes**: none. Update by replacing both files with a newer
  amalgamation and bumping this note (and Hormiga's, together).
