/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

// THROWAWAY instrumentation for the parse/build perf investigation.
// Header-only so it needs no build-system change. All cost is gated behind
// environment variables and is a no-op (one getenv) when they are unset.
//
//   INFOMAP_PHASE_PROFILE=1   print "[phaseprofile] <name> +<dt>s rss=<cur> peak=<peak>"
//                             to stderr at every mark() boundary.
//   INFOMAP_STOP_AFTER=<name> _Exit(0) right after marking <name>, so an external
//                             `/usr/bin/time -l` attributes PEAK RSS to the work up
//                             to and including that phase (e.g. read_build).

#ifndef PHASEPROFILE_H_
#define PHASEPROFILE_H_

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(__APPLE__)
#include <mach/mach.h>
#include <sys/resource.h>
#elif defined(__unix__) || defined(__unix)
#include <cstdio>
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace infomap {
namespace phaseprofile {

  inline unsigned long long currentRssBytes()
  {
#if defined(__APPLE__)
    mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS)
      return 0;
    return info.resident_size;
#elif defined(__unix__) || defined(__unix)
    FILE* f = std::fopen("/proc/self/statm", "r");
    if (!f)
      return 0;
    long pages = 0, resident = 0;
    unsigned long long rss = 0;
    if (std::fscanf(f, "%ld %ld", &pages, &resident) == 2)
      rss = static_cast<unsigned long long>(resident) * static_cast<unsigned long long>(sysconf(_SC_PAGESIZE));
    std::fclose(f);
    return rss;
#else
    return 0;
#endif
  }

  inline unsigned long long peakRssBytes()
  {
#if defined(__APPLE__)
    struct rusage u;
    if (getrusage(RUSAGE_SELF, &u) != 0)
      return 0;
    return static_cast<unsigned long long>(u.ru_maxrss); // bytes on macOS
#elif defined(__unix__) || defined(__unix)
    struct rusage u;
    if (getrusage(RUSAGE_SELF, &u) != 0)
      return 0;
    return static_cast<unsigned long long>(u.ru_maxrss) * 1024ULL; // KiB on Linux
#else
    return 0;
#endif
  }

  inline std::chrono::high_resolution_clock::time_point& lastMark()
  {
    static auto t = std::chrono::high_resolution_clock::now();
    return t;
  }

  inline void mark(const char* name)
  {
    static const bool enabled = std::getenv("INFOMAP_PHASE_PROFILE") != nullptr;
    if (enabled) {
      auto now = std::chrono::high_resolution_clock::now();
      double dt = std::chrono::duration<double>(now - lastMark()).count();
      lastMark() = now;
      const double mb = 1024.0 * 1024.0;
      std::fprintf(stderr, "[phaseprofile] %-14s +%9.3f s   rss=%9.1f MB   peak=%9.1f MB\n",
                   name, dt, currentRssBytes() / mb, peakRssBytes() / mb);
      std::fflush(stderr);
    }
    const char* stop = std::getenv("INFOMAP_STOP_AFTER");
    if (stop != nullptr && std::strcmp(stop, name) == 0) {
      std::fprintf(stderr, "[phaseprofile] stop-after %s  peak=%.1f MB\n",
                   name, peakRssBytes() / (1024.0 * 1024.0));
      std::fflush(stderr);
      std::_Exit(0);
    }
  }

} // namespace phaseprofile
} // namespace infomap

#endif // PHASEPROFILE_H_
