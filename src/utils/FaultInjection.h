/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef INFOMAP_FAULT_INJECTION_H_
#define INFOMAP_FAULT_INJECTION_H_

#ifdef INFOMAP_FAULT_INJECTION
#include <atomic>
#include <stdexcept>
#endif

namespace infomap {
namespace faultinjection {

  // Test-only: makes one OpenMP region throw, so a test can check that the
  // exception reaches the caller instead of ending in std::terminate (#1252).
  // Compiled in only where CMake defines INFOMAP_FAULT_INJECTION (debug,
  // sanitizer and TSan builds). Elsewhere inject() is an empty inline function.
  enum class Site : int {
    None = 0,
    ParallelTrialWorkerSetup,
    RecursiveSubNetwork,
    RecursiveMaterialize,
    InnerParallelMoveLoop,
  };

#ifdef INFOMAP_FAULT_INJECTION

  struct InjectedFault : std::runtime_error {
    InjectedFault() : std::runtime_error("injected fault") {}
  };

  inline std::atomic<int>& armedSite() noexcept
  {
    static std::atomic<int> site { 0 };
    return site;
  }

  inline void arm(Site site) noexcept { armedSite().store(static_cast<int>(site), std::memory_order_relaxed); }

  inline void disarm() noexcept { arm(Site::None); }

  inline void inject(Site site)
  {
    if (armedSite().load(std::memory_order_relaxed) == static_cast<int>(site))
      throw InjectedFault();
  }

#else

  inline void inject(Site) noexcept {}

#endif

} // namespace faultinjection
} // namespace infomap

#endif // INFOMAP_FAULT_INJECTION_H_
