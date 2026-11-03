/* rancho/memory.hpp — how much memory this process holds, and the machine has.
 *
 * The bytes the system has committed to the process (Windows' private
 * bytes, the resident set elsewhere): what a weak device runs out of. Used by
 * the tests that hold memory to a budget, the memory budget of the working set
 * (okf/concepts/working-set.md). Ported from Void Hormiga's
 * platform/process_memory.hpp. 0 where it cannot
 * be read. */
#pragma once

#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <sys/sysctl.h>
#else
#include <cstdio>
#include <unistd.h>
#endif

namespace rancho::memory {

inline std::uint64_t process_bytes() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof pmc))
        return pmc.PrivateUsage;
    return 0;
#elif defined(__APPLE__)
    mach_task_basic_info info{};
    mach_msg_type_number_t n = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &n) == KERN_SUCCESS)
        return info.resident_size;
    return 0;
#else
    std::FILE* f = std::fopen("/proc/self/statm", "r");
    if (!f) return 0;
    unsigned long long pages = 0, resident = 0;
    const int got = std::fscanf(f, "%llu %llu", &pages, &resident);
    std::fclose(f);
    return got == 2 ? resident * (std::uint64_t)sysconf(_SC_PAGESIZE) : 0;
#endif
}

/* The machine's physical memory, 0 when unknown. */
inline std::uint64_t physical_bytes() {
#if defined(_WIN32)
    MEMORYSTATUSEX m{};
    m.dwLength = sizeof m;
    return GlobalMemoryStatusEx(&m) ? m.ullTotalPhys : 0;
#elif defined(__APPLE__)
    std::uint64_t v = 0;
    std::size_t len = sizeof v;
    return sysctlbyname("hw.memsize", &v, &len, nullptr, 0) == 0 ? v : 0;
#else
    const long pages = sysconf(_SC_PHYS_PAGES), size = sysconf(_SC_PAGESIZE);
    return pages > 0 && size > 0 ? (std::uint64_t)pages * (std::uint64_t)size : 0;
#endif
}

} // namespace rancho::memory
