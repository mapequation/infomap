/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef COLUMNAR_TUNING_H_
#define COLUMNAR_TUNING_H_

// Environment-variable tuning knobs and diagnostic counters for the columnar
// engine. These are A/B handles, not user-facing configuration: every default
// here reproduces the shipped search bit-exactly, and nothing in this file is
// reachable from Config or the CLI. Split out of ColumnarMapEquation.cpp so the
// algorithm files start with algorithm.
//
// The helpers are `inline` rather than `static` because more than one columnar
// translation unit reads them; the function-local `static const` caches mean a
// getenv happens once per knob per process either way.

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <string>

namespace infomap {
namespace columnar {

  // Co-physical (correction-proposed) candidate mode, from env COL_COMERGE:
  //   off (default) | all | seeded.
  // Proposing co-physical merge candidates is redundant with the edge-based
  // candidate set on UNDIRECTED clustering (co-physical state nodes are 2-hop
  // connected there) but may matter on DIRECTED clustering, where co-physical
  // modules need not be edge-adjacent. Kept as a tuning knob for that case:
  //   0 = off, 1 = all move-loop phases + merge, 2 = seeded phases only.
  inline int coMergeMode()
  {
    static const int mode = [] {
      const char* e = std::getenv("COL_COMERGE");
      if (!e) return 0;
      if (std::string(e) == "all") return 1;
      if (std::string(e) == "seeded") return 2;
      return 0;
    }();
    return mode;
  }

  // Flow-community regroup probe (env COL_REGROUP): cluster the finest retained
  // aggregation level as its OWN first-order network (base objective, blind to
  // the corrections), then gate the found grouping as a seeded module-level
  // candidate under the true objective. This is the group-merge operator the
  // module-move corrections need: their reward is strongly superadditive in
  // module size, so the augmented objective can hold a deep optimum (merge ~100
  // building blocks into one community) that no chain of pairwise-downhill
  // moves or merges reaches — greedy either stalls fully fragmented or
  // snowballs past every community boundary into one module, and both end
  // states are fixpoints of all pairwise operators. The probe finds the group
  // proposal in the one place it is visible: the flow structure of the block
  // graph itself.
  //
  // ON BY DEFAULT: this is the shipped behaviour. Only reachable with a
  // module-move-capable correction attached (Mem/Meta), so every base-objective
  // network is bit-identical at zero cost. Set COL_REGROUP=off for the
  // pre-operator A/B baseline.
  inline bool regroupProbeEnabled()
  {
    static const bool on = [] {
      const char* e = std::getenv("COL_REGROUP");
      return e == nullptr || std::string(e) != "off";
    }();
    return on;
  }

  // Hierarchical split operator (experimental, see splitLevelModules), from env
  // COL_HSPLIT:
  //   off/unset (default) | 1 | all  = every stack level
  //   leaf      = only k == 0 (split a leaf module)
  //   interior  = only k >= 1 (split a module of modules)
  //   cheap     = every level, but no fresh from-singletons leaf re-derivation
  //               (the one expensive piece source; F20 measured it at 3.17 s on
  //               malaria in the two-level operator)
  //   auto      = the measured policy: interior levels always (that is where the
  //               enter-flow up-build over-merges, on base networks too), the
  //               leaf level only when a module-move-capable correction is
  //               attached (where splitTopModules' gate does apply) and then
  //               only from the free piece sources
  //   winner    = the once-per-run policy: interior levels always, the leaf level
  //               only with a module-move-capable correction attached (as `auto`),
  //               but WITH the expensive from-singletons piece source, which is
  //               the discovery the once-per-run pass exists to pay for
  // Default off keeps the engine bit-identical to the pre-operator baseline.
  enum HSplitMode : std::uint8_t {
    kHSplitOff = 0,
    kHSplitAll = 1,
    kHSplitLeaf = 2,
    kHSplitInterior = 3,
    kHSplitCheap = 4,
    kHSplitAuto = 5,
    kHSplitWinner = 6
  };
  inline int hSplitParseMode(const char* envName, int whenUnset)
  {
    const char* e = std::getenv(envName);
    if (e == nullptr)
      return whenUnset;
    if (std::string(e) == "off")
      return static_cast<int>(kHSplitOff);
    const std::string v(e);
    if (v == "1" || v == "all")
      return static_cast<int>(kHSplitAll);
    if (v == "leaf")
      return static_cast<int>(kHSplitLeaf);
    if (v == "interior")
      return static_cast<int>(kHSplitInterior);
    if (v == "cheap")
      return static_cast<int>(kHSplitCheap);
    if (v == "auto")
      return static_cast<int>(kHSplitAuto);
    if (v == "winner")
      return static_cast<int>(kHSplitWinner);
    return static_cast<int>(kHSplitOff);
  }
  // Per-trial variant: run the split interleave inside every trial's coarsening.
  inline int hSplitMode()
  {
    static const int mode = hSplitParseMode("COL_HSPLIT", static_cast<int>(kHSplitOff));
    return mode;
  }
  // Once-per-run variant: run the split interleave ONCE on the winning trial's
  // hierarchy instead of inside every trial — the #889 maybeDeepRepairBest
  // pattern, which previously only fired for two-level-shaped winners.
  // Independent of COL_HSPLIT.
  //
  // ON BY DEFAULT: this is the shipped behaviour (malaria -0.371% mean over
  // seeds 123/234/345 for +5..11% CPU on a ~3s run; air30k and regularized
  // microscopic gains at +1.5..3%; every base network bit-identical at zero
  // cost, because the repair is gated on module-move corrections). Set
  // COL_HSPLIT_WINNER=off to disable it, or to any other mode name to override
  // the level policy.
  inline int hSplitWinnerMode()
  {
    static const int mode = hSplitParseMode("COL_HSPLIT_WINNER", static_cast<int>(kHSplitWinner));
    return mode;
  }

  inline bool hSplitLevelEnabled(int mode, int k)
  {
    switch (mode) {
    case kHSplitOff:
      return false;
    case kHSplitLeaf:
      return k == 0;
    case kHSplitInterior:
      return k > 0;
    default:
      return true;
    }
  }

  // Accepted/attempted split counters per stack level (COL_HSPLIT_STATS=1 prints
  // them at exit). Atomic so parallel trials stay countable.
  constexpr int kHSplitMaxLevels = 8;
  extern std::atomic<long long> g_hSplitAttempts[kHSplitMaxLevels];
  extern std::atomic<long long> g_hSplitAccepts[kHSplitMaxLevels];
  extern std::atomic<long long> g_hSplitClocks[kHSplitMaxLevels]; // CPU clocks inside the operator
  // Cumulative RELATIVE codelength gain per level, in units of 1e-9, so the
  // accumulator can stay an integer atomic (gains are ~1e-6..1e-3 each).
  extern std::atomic<long long> g_hSplitGainNano[kHSplitMaxLevels];
  // Sub-phase clocks inside the operator (diagnostic, COL_HSPLIT_STATS):
  // 0 = moveBase copy, 1 = piece aggregateLevel, 2 = seeded moveLoop,
  // 3 = stack rebuild (aggregations), 4 = hierarchicalCodelengthFromStack,
  // 5 = save/restore of levels+assignments, 6 = fresh sub-clustering.
  constexpr int kHSplitPhases = 7;
  extern std::atomic<long long> g_hSplitPhase[kHSplitPhases];
  inline bool hSplitPhaseTiming()
  {
    static const bool on = std::getenv("COL_HSPLIT_PHASES") != nullptr;
    return on;
  }
  struct PhaseTimer {
    std::clock_t t0;
    int idx;
    bool on;
    explicit PhaseTimer(int i, bool enabled)
        : t0(enabled ? std::clock() : 0), idx(i), on(enabled) {}
    ~PhaseTimer()
    {
      if (on)
        g_hSplitPhase[idx].fetch_add(static_cast<long long>(std::clock() - t0), std::memory_order_relaxed);
    }
  };

  // PARTIAL SEEDING of the within-grandparent layer refinements (the mechanism is
  // documented on buildPartialSeed).
  //
  // The shipped policy is (q = 0.40, bnd, re-refine only, every interior layer).
  // The env knobs are the A/B handles it was chosen with; COL_PARTSEED_Q=1
  // restores the from-singletons search bit-exactly.
  //
  //   COL_PARTSEED_Q=<0..1>  release fraction: the share of a grandparent's units
  //     the refine gets back as fresh singletons. q = 1 releases everything, i.e.
  //     the from-singletons default; q = 0 seeds everything, which is a fixpoint
  //     (the greedy loop reproduces its input and the gate reverts). The optimum
  //     sits on a broad plateau — web-NotreDame varies by < 0.03% over q in
  //     0.33-0.50 and powergrid by < 0.06% over 0.25-0.40 — and both degrade
  //     above 0.6, where too little is locked to be worth seeding at all.
  //   COL_PARTSEED_M=bnd|ex|inv|iex|rand   which units count as "loosest":
  //     bnd  share of the unit's link flow, inside the grandparent, that crosses
  //          its current module's boundary. The default: it is module-aware and
  //          well-defined on every flow model.
  //     ex   the unit's own leakiness, exit / (flow + exit). DEGENERATE on
  //          undirected first-order networks, where a leaf's exit flow equals its
  //          flow, so the ratio is 0.5 for every node and the "ranking" collapses
  //          to node order (measured: ex and iex are bit-identical on powergrid).
  //     inv / iex  controls for bnd / ex: same ranking, released from the tight
  //          end instead. Both must LOSE to the from-singletons default, or the
  //          effect is release volume rather than release targeting.
  //     rand release a uniformly random q (deterministic in m_seed): the other
  //          control, for "any perturbation would do".
  //   COL_PARTSEED_LEAF=1   restrict to the leaf layer (default: every interior
  //                         layer — the interior layers are where the sweeps that
  //                         re-refine actually live, and they are nearly free)
  //   COL_PARTSEED_ALWAYS=1 also partial-seed a layer's FIRST refine (default:
  //                         only re-refines, see partSeedResweepOnly)
  //   COL_PARTSEED_FLAT=1   apply it to a converged flat bottom instead of
  //                         skipping that layer (see m_bottomConverged)
  constexpr double kPartSeedRelease = 0.40;
  enum class PartSeedMetric : std::uint8_t {
    Boundary,
    Exit,
    InvBoundary,
    InvExit,
    Random
  };
  inline double partSeedRelease()
  {
    static const double q = [] {
      const char* e = std::getenv("COL_PARTSEED_Q");
      return e != nullptr ? std::atof(e) : kPartSeedRelease;
    }();
    return q;
  }
  inline PartSeedMetric partSeedMetric()
  {
    static const PartSeedMetric m = [] {
      const char* e = std::getenv("COL_PARTSEED_M");
      if (e == nullptr)
        return PartSeedMetric::Boundary;
      const std::string v(e);
      if (v == "ex")
        return PartSeedMetric::Exit;
      if (v == "inv")
        return PartSeedMetric::InvBoundary;
      if (v == "iex")
        return PartSeedMetric::InvExit;
      if (v == "rand")
        return PartSeedMetric::Random;
      return PartSeedMetric::Boundary;
    }();
    return m;
  }
  inline bool partSeedLeafOnly()
  {
    static const bool on = std::getenv("COL_PARTSEED_LEAF") != nullptr;
    return on;
  }
  inline bool partSeedFlatBottom()
  {
    static const bool on = std::getenv("COL_PARTSEED_FLAT") != nullptr;
    return on;
  }
  // Partial-seed only a RE-refine of a layer, never its first from-singletons
  // derivation (COL_PARTSEED_ALWAYS=1 lifts this). Seeding damage is localised to
  // the first refine of a grandparent (F24 C), and before that first refine the
  // layer holds no partition worth locking — the build's greedy enter-flow
  // super-search made it, not a two-level optimize. Refine sweep 0 refines every
  // dirty layer, so m_refineSweep > 0 is exactly "this layer has been re-derived".
  // This is also what keeps the whole feature inert on the single-sweep searches
  // (`-F`, and any stack with one interior layer): they only ever refine once, so
  // there is nothing to re-refine and nothing changes.
  inline bool partSeedResweepOnly()
  {
    static const bool on = std::getenv("COL_PARTSEED_ALWAYS") == nullptr;
    return on;
  }
  // Partial seeding applies to layer k? Off (q >= 1) is the from-singletons
  // default, which must stay bit-identical, so this is the single gate.
  inline bool partSeedActive(int layer)
  {
    const double q = partSeedRelease();
    return q >= 0.0 && q < 1.0 && (layer == 0 || !partSeedLeafOnly());
  }
  // Deterministic 64-bit mix (splitmix64 finalizer): the PRNG for the `rand`
  // control, keyed so a run is reproducible from (m_seed, layer, unit).
  inline unsigned long long partSeedMix(unsigned long long x)
  {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
  }

} // namespace columnar
} // namespace infomap

#endif // COLUMNAR_TUNING_H_
