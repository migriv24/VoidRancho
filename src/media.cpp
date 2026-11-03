/* media.cpp — see rancho/media.hpp. */
#include "rancho/media.hpp"

#if defined(_WIN32)
#include <windows.h>
#include <vector>
#elif defined(__APPLE__)
#include <sys/mount.h>
#include <sys/param.h>
#include <cstring>
#else
#include <sys/vfs.h>
#endif

namespace fs = std::filesystem;

namespace rancho {

bool is_unc(const std::string& path) {
    if (path.size() < 3) return false;
    const auto sep = [](char c) { return c == '\\' || c == '/'; };
    if (!sep(path[0]) || !sep(path[1])) return false;
    // \\?\C:\... is a local path spelled long; \\?\UNC\server\... is not
    if (path[2] == '?' || path[2] == '.') {
        const std::string rest = path.substr(path.size() >= 4 ? 4 : path.size());
        return rest.rfind("UNC\\", 0) == 0 || rest.rfind("UNC/", 0) == 0;
    }
    return !sep(path[2]);
}

const char* medium_name(Medium m) {
    switch (m) {
    case Medium::Local: return "local";
    case Medium::Network: return "network";
    default: return "unknown";
    }
}

Medium detect_medium(const fs::path& path) {
    /* The spelling first, and never through std::filesystem::absolute: the
     * MinGW libstdc++ makes `\\nas\share\x` into `C:\nas\share\x`, which would
     * have judged every NAS path a local one (found by media_test, 2026-10-30). */
    if (is_unc(path.string())) return Medium::Network;
#if defined(_WIN32)
    const std::wstring in = path.wstring();
    std::vector<wchar_t> full(32768);
    const DWORD n = GetFullPathNameW(in.c_str(), (DWORD)full.size(), full.data(), nullptr);
    if (n == 0 || n >= full.size()) return Medium::Unknown;
    const std::wstring abs(full.data(), n);
    if (abs.size() >= 2 && abs[0] == L'\\' && abs[1] == L'\\') {
        // \\server\share (or \\?\UNC\...) after resolving: a network path; \\?\C:\ is not
        if (abs.size() >= 4 && abs[2] == L'?' && abs.compare(4, 4, L"UNC\\") != 0) {
            if (abs.size() < 7) return Medium::Unknown;
            const std::wstring root = abs.substr(4, 3);
            return GetDriveTypeW(root.c_str()) == DRIVE_REMOTE ? Medium::Network : Medium::Local;
        }
        return Medium::Network;
    }
    if (abs.size() < 3 || abs[1] != L':') return Medium::Unknown;
    const std::wstring root = abs.substr(0, 3); // "C:\"
    switch (GetDriveTypeW(root.c_str())) {
    case DRIVE_REMOTE: return Medium::Network; // a mapped network drive
    case DRIVE_FIXED:
    case DRIVE_REMOVABLE:
    case DRIVE_CDROM:
    case DRIVE_RAMDISK: return Medium::Local;
    default: return Medium::Unknown;
    }
#else
    // the file may not exist yet: ask about the nearest folder that does
    std::error_code ec;
    fs::path dir = fs::absolute(path, ec);
    if (ec) dir = path;
    if (!fs::is_directory(dir, ec)) dir = dir.parent_path();
    while (!dir.empty() && !fs::exists(dir, ec) && dir.has_parent_path() && dir.parent_path() != dir)
        dir = dir.parent_path();
    struct statfs s;
    if (statfs(dir.c_str(), &s) != 0) return Medium::Unknown;
#if defined(__APPLE__)
    for (const char* n : {"nfs", "smbfs", "afpfs", "webdav", "cifs"})
        if (std::strcmp(s.f_fstypename, n) == 0) return Medium::Network;
    return (s.f_flags & MNT_LOCAL) ? Medium::Local : Medium::Network;
#else
    switch ((unsigned long)s.f_type) {
    case 0x6969UL:     // NFS
    case 0x517BUL:     // SMB
    case 0xFF534D42UL: // CIFS
    case 0xFE534D42UL: // SMB2
    case 0x5346414FUL: // AFS
    case 0x47504653UL: // GPFS
    case 0x00C36400UL: // Ceph
    case 0x6B414653UL: // kAFS
    case 0x013111A8UL: // IBRIX
    case 0x0BD00BD0UL: // Lustre
        return Medium::Network;
    default: return Medium::Local;
    }
#endif
#endif
}

} // namespace rancho
