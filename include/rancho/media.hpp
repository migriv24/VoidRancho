/* rancho/media.hpp — where a store's file lives, and what that allows
 * (okf/concepts/tiers.md, okf/design/storage-research.md §1).
 *
 * SQLite is the right tool for a live database on one machine and the wrong one
 * for a file two machines write. Its own documentation: WAL "does not work over
 * a network filesystem", because the WAL index is shared memory; and in any
 * mode it trusts the filesystem's locks, which on network filesystems have bugs
 * that corrupt a database two processes write. `PRAGMA journal_mode=WAL` can
 * even report success on a filesystem that cannot support it — so Rancho asks
 * the operating system where the file is instead of trusting the PRAGMA.
 *
 * What a store on a network path gets (Sharing::Auto):
 *   - the rollback journal, never WAL;
 *   - an EXCLUSIVE lock held for as long as the store is open, so a second
 *     machine opening it is refused with "database is locked" rather than
 *     allowed to corrupt it. One host at a time, made mechanical.
 * Several machines SHARING a database is not a second SQLite connection to one
 * file: it is segments on the shared medium (tiers 2–3), or Void Palabra's
 * merge, arriving as commands. */
#pragma once

#include <filesystem>
#include <string>

namespace rancho {

enum class Medium {
    Local,   // a disk of this machine
    Network, // SMB/CIFS, NFS, a mapped drive, a UNC path, WebDAV, AFP
    Unknown, // could not be told; treated as local
};

enum class Sharing {
    Auto,      // decide from the medium
    SingleHost // exclusive rollback journal whatever the medium (a USB stick moved between machines)
};

/* Where `path` (a file that may not exist yet: its folder is asked) lives. */
Medium detect_medium(const std::filesystem::path& path);
/* The part of the answer that is only the path's spelling: a UNC path
 * (\\server\share, //server/share) is on a network whatever the OS says. */
bool is_unc(const std::string& path);
const char* medium_name(Medium m);

} // namespace rancho
