/* rancho/segments.hpp — the shared form of a database: immutable segments and a
 * manifest per snapshot (okf/concepts/tiers.md, okf/design/storage-research.md §2).
 *
 * A live store is SQLite on one host. Anything SHARED or ARCHIVED — a database
 * on a NAS that several machines open, a backup, a copy for a laptop — is
 * written as a directory of files that are never changed once written:
 *
 *   <dir>/segments/<sha256>.json   one per cluster leaf (or per page of a
 *                                  mantle not clustered yet), named by the
 *                                  SHA-256 of its bytes — Void Palabra's hash,
 *                                  so identical content has one name across
 *                                  the family, and is stored once
 *   <dir>/manifests/<n>.json       snapshot n: the document's other keys, each
 *                                  mantle's skeleton and its segments in order,
 *                                  with per-segment statistics (runes, edges,
 *                                  bytes, the ρ range, tag counts)
 *   <dir>/ROOT                     the current snapshot's number: the one
 *                                  mutable thing, replaced by an atomic rename
 *
 * The Apache Iceberg pattern: a commit writes new segments, then a manifest
 * CREATED EXCLUSIVELY (a second committer racing for the same number fails
 * instead of overwriting), then swaps the ROOT pointer. Readers take no lock:
 * they read ROOT and then only files that never change, so they see the old
 * snapshot or the new one and never half. History is the chain of manifests;
 * an unchanged cluster is the same segment in both, so a snapshot after a
 * small edit writes what changed and a manifest.
 *
 * On object storage the same layout is a key space, and the two operations it
 * needs exist since 2024 (create-if-absent for the manifest, compare-and-swap
 * for ROOT); a bucket adapter is planned, not built.
 *
 * Every segment's name is checked against its bytes when it is read: a shared
 * drive that corrupts a file is reported, never loaded.
 *
 * NOT SYNC. Two people editing one database is Void Palabra's merge, arriving
 * as commands. Segments let a second machine OPEN a database fast; they do not
 * let two machines write one store. */
#pragma once

#include "rancho/store.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace rancho {

struct SegmentOptions {
    /* A mantle not clustered is cut into pages, in order, of about this many
     * bytes of rune JSON. A clustered mantle is cut by leaf. */
    std::uint64_t page_bytes = 4ull << 20;
};

struct SnapshotStats {
    bool ok = false;
    std::string error;
    long long snapshot = 0;                       // the number committed
    long long segments = 0, written = 0, reused = 0; // reused: already there, byte for byte
    long long runes = 0, edges = 0;
    std::uint64_t bytes_written = 0;
};

/* Commit the whole store as the next snapshot in `dir` (created if absent).
 * Soft-deleted rows are not written: a snapshot is what the database IS. */
SnapshotStats snapshot(Store& store, const std::filesystem::path& dir, const SegmentOptions& options = {});

struct RestoreStats {
    bool ok = false;
    std::string error;
    long long snapshot = 0, segments = 0, runes = 0, edges = 0;
};

/* Replace `into` with snapshot `n` of `dir` (0: the current one). One
 * transaction: on failure the store is as it was. */
RestoreStats restore(const std::filesystem::path& dir, Store& into, long long n = 0);

/* The snapshots `dir` holds, oldest first; and the current one (0 if none). */
std::vector<long long> snapshots(const std::filesystem::path& dir);
long long current_snapshot(const std::filesystem::path& dir);

/* SHA-256 (FIPS 180-4) as lowercase hex. Implemented in-tree, as Void Palabra's
 * is, and checked against the FIPS vectors in segments_test.cpp. */
std::string sha256_hex(std::string_view bytes);

} // namespace rancho
