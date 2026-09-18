/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "ColumnarTuning.h"

#include <cstdio>
#include <ctime>

namespace infomap {
namespace columnar {

  // Definitions for the counters declared extern in the header. Atomic so parallel
  // trials stay countable.
  std::atomic<long long> g_hSplitAttempts[kHSplitMaxLevels];
  std::atomic<long long> g_hSplitAccepts[kHSplitMaxLevels];
  std::atomic<long long> g_hSplitClocks[kHSplitMaxLevels];
  std::atomic<long long> g_hSplitGainNano[kHSplitMaxLevels];
  std::atomic<long long> g_hSplitPhase[kHSplitPhases];

  namespace {
    static const char* const kHSplitPhaseName[kHSplitPhases] = { "moveBaseCopy", "pieceAgg", "moveLoop", "rebuild", "evalStack", "saveRestore", "subCluster" };

    struct HSplitStatsDump {
      ~HSplitStatsDump()
      {
        if (std::getenv("COL_HSPLIT_STATS") == nullptr)
          return;
        long long ta = 0, tc = 0;
        double tt = 0.0, tg = 0.0;
        for (int k = 0; k < kHSplitMaxLevels; ++k) {
          const long long a = g_hSplitAttempts[k].load(std::memory_order_relaxed);
          const long long c = g_hSplitAccepts[k].load(std::memory_order_relaxed);
          const double t = static_cast<double>(g_hSplitClocks[k].load(std::memory_order_relaxed)) / CLOCKS_PER_SEC;
          const double g = static_cast<double>(g_hSplitGainNano[k].load(std::memory_order_relaxed)) * 1e-9;
          ta += a;
          tc += c;
          tt += t;
          tg += g;
          if (a != 0) {
            const double perAtt = 100.0 * g / static_cast<double>(a);
            std::fprintf(stderr, "[hsplit] level k=%d: %lld accepted / %lld attempted, %.2fs, gain %.4f%% (%.5f%%/att)\n", k, c, a, t, 100.0 * g, perAtt);
          }
        }
        std::fprintf(stderr, "[hsplit] total: %lld accepted / %lld attempted, %.2fs, gain %.4f%%\n", tc, ta, tt, 100.0 * tg);
        if (hSplitPhaseTiming())
          for (int p = 0; p < kHSplitPhases; ++p) {
            const double secs = static_cast<double>(g_hSplitPhase[p].load(std::memory_order_relaxed)) / CLOCKS_PER_SEC;
            std::fprintf(stderr, "[hsplit] phase %-12s %.3fs\n", kHSplitPhaseName[p], secs);
          }
      }
    };
    HSplitStatsDump g_hSplitStatsDump;
  } // namespace

} // namespace columnar
} // namespace infomap
