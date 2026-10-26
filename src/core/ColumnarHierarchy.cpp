/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

// The stacked-hierarchy search of the columnar engine: the operators that change
// the stack and the drivers that sequence them. The move loop and the two-level
// search they call live in ColumnarMapEquation.cpp.

#include "ColumnarHierarchy.h"
#include "InfoNode.h"
#include "../utils/infomath.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <limits>
#include <map>
#include <numeric>
#include <unordered_map>
#include <utility>

#include "ColumnarObjective.h"
#include "ColumnarTuning.h"

namespace infomap {

using namespace columnar;

double ColumnarHierarchy::optimizeTwoLevelStack()
{
  m_search.optimizeTwoLevel();

  // Materialize the partition as a two-level stack so the stack-based helpers
  // (codelength, coarsening, toNodePaths) apply.
  m_stack.setFlat(m_search.leafTopModule(), static_cast<int>(m_search.numTopModules()));

  double L = hierarchicalCodelengthFromStack();
  // Module-merge coarsening within the root: a no-op for the base objective
  // (merging only lengthens the base codelength; the top regroup needs an
  // interior level), but the memory/meta/lossy corrections reward merges the
  // leaf-level move loop cannot reach — the same reason the hierarchical
  // searches run it.
  double beforeMerge = L;
  coarsenModules(L, 1000);

  // Interleave the merge with a seeded leaf fine-tune: a correction-driven
  // merge reshapes the modules far from where the leaf move loop last saw
  // them (air30k: K 1344 -> 328), so re-tuning the leaves inside the merged
  // structure recovers most of the remaining gap to the OO -2 optimum, and
  // the tuned partition can enable further merges. Alternate until the pair
  // stops improving. The base objective never enters (its merge is a no-op,
  // so the loop condition fails immediately and the already-converged
  // fine-tune is not repeated). The subdivision operator is NOT part of the
  // per-trial pipeline — it is the expensive discovery step, spent once on
  // the winning trial (deepRepairTwoLevelStack).
  for (int round = 0; round < 100 && L < beforeMerge - kMinImprovement; ++round) {
    const bool tuned = retuneLeavesWithinModules(L);
    beforeMerge = L;
    coarsenModules(L, 1000);
    if (!tuned && L >= beforeMerge - kMinImprovement)
      break;
  }
  return L;
}

double ColumnarHierarchy::completeFlatFromAggregation(std::vector<int> aggTop, int aggK)
{
  // Complete the flat two-level pipeline from the probe's converged
  // aggregation partition (m_search.optimizeTwoLevel(0, false)): materialize the
  // stack, run the deferred leaf fine-tune to convergence, then the same
  // merge <-> retune interleave as optimizeTwoLevelStack. This is the
  // expensive leaf-level half the probe skipped; a flat-first trial pays it
  // only when the probe says the flat basin is competitive.
  m_stack.setFlat(std::move(aggTop), aggK);

  m_bottomConverged = true;
  double L = hierarchicalCodelengthFromStack();
  while (retuneLeavesWithinModules(L)) {
    pollInterrupt();
  }
  double beforeMerge = L;
  coarsenModules(L, 1000);
  for (int round = 0; round < 100 && L < beforeMerge - kMinImprovement; ++round) {
    const bool tuned = retuneLeavesWithinModules(L);
    beforeMerge = L;
    coarsenModules(L, 1000);
    if (!tuned && L >= beforeMerge - kMinImprovement)
      break;
  }
  return L;
}

double ColumnarHierarchy::deepRepairTwoLevelStack(bool externalSeed)
{
  // Exploitation phase (#889), run ONCE on the winning trial's partition
  // (seeded via seedHierarchyFromLeafPaths) instead of inside every trial:
  // the from-singletons split is the expensive discovery step, and spending
  // it on partitions that lose the best-of-N anyway is wasted search. Its
  // cost is one repair per run, amortizing with -N. Interleave the split
  // with the seeded leaf fine-tune and the merge until the trio stops
  // improving; every step is gated on the true stack codelength, so the
  // result is never worse than the seed.
  m_subClusterCache.clear();
  m_lastSinglesPieces.clear();
  m_freshSinglesProductive = true;
  // Did the search converge to a module of (nearly) the whole network? Decided on
  // the seed, not per round: the one-level fallback (a single module) is no search
  // result -- every trial ended worse and was replaced by it -- and extracting
  // communities from it top-down, then from what remains, is the only operator
  // that leaves it (om3 E50000 `-d --regularized -N10`: 7.9697 -> 7.9312), F57.
  {
    const int K = m_stack.size(1);
    std::vector<int> size(K, 0);
    for (int i = 0; i < nLeaves(); ++i)
      ++size[m_stack.leafModule(i)];
    const int largest = K > 0 ? *std::max_element(size.begin(), size.end()) : 0;
    m_seedHasWholeNetworkModule = K > 1 && largest >= kWholeNetworkModuleShare * nLeaves();
    m_seedIsOneModule = K == 1;
  }
  double L = hierarchicalCodelengthFromStack();
  double before = L;
  bool allowSingletons = true;
  int split = splitTopModules(L, allowSingletons, externalSeed);
  allowSingletons = split != 0;
  for (int round = 0; round < 100; ++round) {
    const bool tuned = retuneLeavesWithinModules(L);
    before = L;
    coarsenModules(L, 1000, false, externalSeed);
    if (tuned || L < before - kMinImprovement)
      allowSingletons = true;
    split = splitTopModules(L, allowSingletons, externalSeed);
    allowSingletons = split != 0;
    if (!tuned && split == 0 && L >= before - kMinImprovement)
      break;
  }
  return L;
}

int ColumnarHierarchy::splitTopModules(double& L, bool allowSingletons, bool externalSeed)
{
  // Merge overshoot only exists where module-move-capable corrections drive
  // the aggregation (the base merge is a no-op), and the recombination loop
  // needs their module-level deltas — skip entirely otherwise, keeping base
  // networks bit-exact at zero cost.
  std::vector<ColumnarCorrection*> unitCorr;
  for (auto& cp : corrections())
    if (cp->participatesInMoveLoop() && cp->participatesInModuleMoves())
      unitCorr.push_back(cp.get());
  // ...except on an externally seeded partition, whose modules the engine did not
  // build and can need subdividing under any objective (externalSeed, #824).
  if (unitCorr.empty() && !externalSeed)
    return 0;

  const int K = m_stack.size(1);

  // Recombination: a seeded module-level move loop over the pieces — the
  // module-scale analog of the leaf fine-tune. Corrections act on aggregated
  // units through their per-unit aggregates, so a piece peels off its module
  // only when the true augmented objective says so. Gated on the true stack
  // codelength (revert if not improving).
  auto recombine = [&](const std::vector<int>& leafToPiece, const std::vector<int>& pieceParent) -> bool {
    const int nPieces = static_cast<int>(pieceParent.size());
    if (nPieces == K)
      return false; // every module is a single piece: nothing to split
    const std::vector<int>& module = m_search.moveUnits(m_search.flow().aggregate(leaf0(), leafToPiece, nPieces), pieceParent, &leafToPiece);

    // Fast reject: no piece ended up outside its seeded module — the stack is
    // untouched, so no rebuild or evaluation is needed. This is the common
    // case in the convergence tail, where each round pays only the piece
    // aggregation and one settled move-loop sweep.
    bool anyMoved = false;
    for (int p = 0; p < nPieces && !anyMoved; ++p)
      anyMoved = module[p] != pieceParent[p];
    if (!anyMoved)
      return false;

    std::vector<int> remap(m_search.activeLevel().n, -1);
    int k = 0;
    for (int p = 0; p < nPieces; ++p)
      if (remap[module[p]] == -1)
        remap[module[p]] = k++;
    std::vector<int> newTop(nLeaves());
    for (int i = 0; i < nLeaves(); ++i)
      newTop[i] = remap[module[leafToPiece[i]]];

    return gatedStep(L, [&] {
      m_stack.setModules(0, std::move(newTop), k);
      return true;
    });
  };

  // Intersect a global fine labeling with the current modules: pieces =
  // (label ∩ module), well-defined even after leaf tuning moved individual
  // leaves across module boundaries.
  auto intersectPieces = [&](const std::vector<int>& labels, std::vector<int>& leafToPiece, std::vector<int>& pieceParent) {
    std::unordered_map<long long, int> pieceId;
    pieceId.reserve(static_cast<std::size_t>(K) * 2);
    leafToPiece.resize(nLeaves());
    pieceParent.clear();
    for (int i = 0; i < nLeaves(); ++i) {
      const int mod = m_stack.leafModule(i);
      const long long key = static_cast<long long>(labels[i]) * (static_cast<long long>(K) + 1) + mod;
      auto res = pieceId.emplace(key, static_cast<int>(pieceParent.size()));
      if (res.second)
        pieceParent.push_back(mod);
      leafToPiece[i] = res.first->second;
    }
  };

  // Piece source 1 (cheap): the pass-1 building blocks — the finest quanta
  // the aggregation itself consumed. Re-sorting them undoes overshot merges
  // move by move where each step is downhill.
  std::vector<int> leafToPieceScratch;
  std::vector<int> pieceParentScratch;
  if (!m_search.leafBlocks().empty()) {
    intersectPieces(m_search.leafBlocks(), leafToPieceScratch, pieceParentScratch);
    if (recombine(leafToPieceScratch, pieceParentScratch))
      return 1;
  }

  // Piece source 2 (cheap): the last fresh from-singletons derivation,
  // projected onto the current partition. Community structure shifts little
  // between interleave rounds, so stale community pieces usually still
  // propose the right extractions — the expensive fresh derivation below
  // then only runs when the cheap sources are exhausted.
  if (!m_lastSinglesPieces.empty()) {
    intersectPieces(m_lastSinglesPieces, leafToPieceScratch, pieceParentScratch);
    if (recombine(leafToPieceScratch, pieceParentScratch))
      return 2;
  }
  // Fresh derivation is the expensive source: keep paying for it only while
  // it pays (malaria-like nets keep revealing new community splits round
  // after round; air30k-like nets get nothing beyond the first derivation).
  // "Pays" is measured against its cost below (kFreshYieldPerNetwork).
  if (!m_freshDiscovery || !allowSingletons || (!m_lastSinglesPieces.empty() && !m_freshSinglesProductive))
    return 0;

  // Piece source 2 (decisive): from-singletons sub-clustering within each
  // module — community-granularity pieces, so extracting a whole community
  // from an over-merged module is a single gated move (block-granularity
  // pieces face the same uphill barrier as single leaves when a split's
  // intermediate states are worse).
  std::vector<std::vector<int>> leavesPer(K);
  for (int i = 0; i < nLeaves(); ++i)
    leavesPer[m_stack.leafModule(i)].push_back(i);
  std::vector<int> leafToPiece(nLeaves(), -1);
  std::vector<int> pieceParent;
  std::vector<int> loc(nLeaves(), -1);
  std::vector<int> localAssign;
  // A module holding (nearly) the whole network stays one piece when the search
  // converged to such a module (m_seedHasWholeNetworkModule): re-deriving it
  // repeats that whole-network search. On the regularized om5-om8 E100000 winners
  // (one module of 99% of the states plus crumbs) each re-derivation cost 0.4 s --
  // as much as the trial -- for <= 0.02% in bits (F57).
  const double wholeNetworkModule = m_seedHasWholeNetworkModule ? kWholeNetworkModuleShare * nLeaves() : std::numeric_limits<double>::infinity();
  std::size_t rederivedLeaves = 0;
  for (int P = 0; P < K; ++P) {
    const std::vector<int>& S = leavesPer[P];
    if (S.empty())
      continue;
    if (S.size() == 1 || static_cast<double>(S.size()) >= wholeNetworkModule) {
      const int piece = static_cast<int>(pieceParent.size());
      for (int i : S)
        leafToPiece[i] = piece;
      pieceParent.push_back(P);
      continue;
    }
    // A module's sub-clustering depends only on its own leaf set (its exit is
    // the crossing flow of its members), so unchanged modules reuse the
    // result from earlier rounds — typically only the modules a split/merge/
    // re-tune touched are re-clustered.
    int Ksub;
    auto it = m_subClusterCache.find(S);
    if (it != m_subClusterCache.end()) {
      Ksub = it->second.first;
      localAssign = it->second.second;
    } else {
      // Proposal granularity: skip the sub-optimize's fine-tune — the pieces
      // only need to separate communities, and the gated recombination plus
      // the interleaved leaf re-tune do the polishing. On a one-module seed
      // stop at its pass-1 building blocks: what the extraction peels off the
      // one-level fallback are tight groups of a few states (om3 E50000
      // `--regularized`: 48 groups of 3-23 states around one module of 99% of
      // them), finer than the aggregated pieces (~95 states there), and the
      // aggregation passes are about two thirds of a derivation's cost. The
      // aggregated pieces exposed a few such groups per derivation, by chance, so
      // the repair re-aggregated the whole remainder round after round (F59).
      Ksub = subClusterLeaves(S, m_stack.level(1).exit[P], loc, localAssign, false, nullptr, m_seedIsOneModule ? 1u : 0u);
      m_subClusterCache.emplace(S, std::make_pair(Ksub, localAssign));
      rederivedLeaves += S.size();
    }
    const int base = static_cast<int>(pieceParent.size());
    for (std::size_t j = 0; j < S.size(); ++j)
      leafToPiece[S[j]] = base + localAssign[j];
    for (int s = 0; s < Ksub; ++s)
      pieceParent.push_back(P);
  }
  m_lastSinglesPieces = leafToPiece;
  const double beforeSingles = L;
  const bool singlesImproved = recombine(leafToPiece, pieceParent);
  // Another fresh derivation only while this one bought its cost: a share of the
  // codelength proportional to the share of the network it re-clustered. After
  // the leaf re-tune nearly every module differs by a few leaves, so each round
  // re-clusters ~all of it; on om5 `-d --regularized` the rounds after the first
  // bought 0.0003-0.005% each at 0.4 s apiece (F57).
  // Block-granularity extraction from a one-module seed buys more per round at a
  // fraction of the cost, so the 1e-4 bar lets it run many rounds that each buy
  // ~0.01%; there it has to buy kFreshYieldOneModule instead (F59: om3-om8 E50000
  // `-2d --regularized` faster than before on every row, 0.02-0.07% in bits short
  // of running on).
  const double yield = m_seedIsOneModule ? kFreshYieldOneModule : kFreshYieldPerNetwork;
  const double cost = yield * beforeSingles * static_cast<double>(rederivedLeaves) / nLeaves();
  m_freshSinglesProductive = singlesImproved && beforeSingles - L >= cost;
  return singlesImproved ? 2 : 0;
}

bool ColumnarHierarchy::hierarchicalWinnerRepairEnabled()
{
  return hSplitWinnerMode() != kHSplitOff;
}

double ColumnarHierarchy::deepRepairHierarchicalStack()
{
  // Once-per-run repair of a DEEP winner (COL_HSPLIT_WINNER): the hierarchical
  // split operator interleaved with the module coarsening on the best-of-N
  // hierarchy, mirroring deepRepairTwoLevelStack's role for flat winners.
  // Every step is gated on the true stack codelength, so the result is never
  // worse than the seed.
  // One pass is convergence in practice: re-running the interleave with the
  // piece-source state re-armed was measured to change nothing on malaria,
  // web-NotreDame or air30k (identical codelengths, ~10-40% more attempts), so
  // the round loop was dropped rather than shipped as a dead dial.
  m_subClusterCache.clear();
  m_lastSinglesPieces.clear();
  m_lastLevelPieces.clear();
  m_levelFreshProductive.clear();
  m_freshSinglesProductive = true;
  double L = hierarchicalCodelengthFromStack();
  coarsenModules(L, 1000, true);
  return L;
}

int ColumnarHierarchy::splitLevelModules(int k, double& L, bool allowSingletons)
{
  const int top = m_stack.topLevel();
  if (k < 0 || k + 1 > top)
    return 0; // level k+1 must be a module level of the stack
  const int nU = m_stack.size(k);
  const int K = m_stack.size(k + 1);
  if (nU <= 1 || K >= nU)
    return 0; // every module already holds a single child: nothing to subdivide

  const std::clock_t tStart = std::clock();
  struct ClockGuard {
    std::clock_t t0;
    int level;
    ~ClockGuard() { g_hSplitClocks[level].fetch_add(static_cast<long long>(std::clock() - t0), std::memory_order_relaxed); }
  } clockGuard { tStart, std::min(k, kHSplitMaxLevels - 1) };

  const bool interior = k > 0;

  // The level-k network in the space the level-(k+1) codebook actually uses:
  // leaves are coded by their flow, interior modules by their index rate q
  // (setIndexRateAsFlow, the up-build's transform). Only the move loop sees the
  // transform; the stack levels keep the true flows.
  const bool phaseTiming = hSplitPhaseTiming();
  Level moveBaseCopy;
  {
    PhaseTimer pt(0, phaseTiming);
    moveBaseCopy = m_stack.level(k);
    if (interior)
      m_search.flow().setIndexRateAsFlow(moveBaseCopy);
  }
  const Level& moveBase = moveBaseCopy;

  if (static_cast<int>(m_lastLevelPieces.size()) <= k) {
    m_lastLevelPieces.resize(k + 1);
    m_levelFreshProductive.resize(k + 1, 1);
  }

  std::vector<int> seed; // pieceParent, compacted when it overruns the piece level
  // Recombination: a seeded move loop over the pieces of the level-k network,
  // seeded at the current level-(k+1) modules. Pieces may move to any module,
  // including an empty one — group split and cross-parent relocation in one
  // operator. Gated on the true stack codelength (revert if not improving).
  auto recombine = [&](const std::vector<int>& unitToPiece, const std::vector<int>& pieceParent) -> bool {
    const int nPieces = static_cast<int>(pieceParent.size());
    if (nPieces == K)
      return false; // every module is a single piece: nothing to split
    g_hSplitAttempts[std::min(k, kHSplitMaxLevels - 1)].fetch_add(1, std::memory_order_relaxed);
    // The move level has one module slot per piece, but pieceParent holds
    // level-(k+1) module ids, where a module emptied by an earlier split
    // survives as an empty id: with enough of them a parent id reaches
    // nPieces. Seed with compacted ids then; remap below compacts the result
    // anyway, so only the grouping matters.
    seed = pieceParent;
    if (*std::max_element(seed.begin(), seed.end()) >= nPieces) {
      g_hSplitSeedCompactions.fetch_add(1, std::memory_order_relaxed);
      std::vector<int> id(K, -1);
      int next = 0;
      for (int& s : seed) {
        if (id[s] == -1)
          id[s] = next++;
        s = id[s];
      }
    }
    Level pieces;
    {
      PhaseTimer pt(1, phaseTiming);
      pieces = m_search.flow().aggregate(moveBase, unitToPiece, nPieces);
    }
    // Module-move-capable corrections score the LEAF module partition, so they
    // only participate when the level being re-sorted is the leaf level: a move
    // at an interior level leaves every leaf's module unchanged, hence the
    // correction unchanged. (Unlike splitTopModules, the presence of such a
    // correction is NOT a precondition — the hierarchical over-merge comes from
    // the enter-flow up-build, which runs on base networks too.)
    const std::vector<int>* module = nullptr;
    {
      PhaseTimer pt(2, phaseTiming);
      module = &m_search.moveUnits(std::move(pieces), seed, interior ? nullptr : &unitToPiece);
    }

    // Fast reject: no piece ended up outside its seeded module — the stack is
    // untouched, so no rebuild or evaluation is needed.
    bool anyMoved = false;
    for (int p = 0; p < nPieces && !anyMoved; ++p)
      anyMoved = (*module)[p] != seed[p];
    if (!anyMoved)
      return false;

    // Tentative from here: the checkpoint saves only the slots the rebuild
    // writes (the level-k assignment, the module->grandparent map and the levels
    // above k), by moving them aside rather than copying them.
    const ColumnarStack::Checkpoint checkpoint = m_stack.checkpoint();
    {
      PhaseTimer ptRebuild(3, phaseTiming);
      std::vector<int> remap(m_search.activeLevel().n, -1);
      int nk = 0;
      for (int p = 0; p < nPieces; ++p)
        if (remap[(*module)[p]] == -1)
          remap[(*module)[p]] = nk++;
      std::vector<int> newAK(nU);
      for (int u = 0; u < nU; ++u)
        newAK[u] = remap[(*module)[unitToPiece[u]]];

      if (k + 1 < top) {
        // A new module inherits the grandparent of the module its pieces
        // predominantly came from (by piece flow) — a group split stays in its
        // old grandparent, a relocation follows the module it joined. Ties break
        // on the lower grandparent id, so the map is order-independent.
        std::vector<std::unordered_map<int, double>> votes(nk);
        for (int p = 0; p < nPieces; ++p)
          votes[remap[(*module)[p]]][m_stack.parent(k + 1, pieceParent[p])] += m_search.activeLevel().flow[p];
        std::vector<int> newAK1(nk, 0);
        for (int m = 0; m < nk; ++m) {
          int bestG = -1;
          double bestW = -1.0;
          for (const auto& kv : votes[m])
            if (kv.second > bestW || (kv.second == bestW && kv.first < bestG)) {
              bestW = kv.second;
              bestG = kv.first;
            }
          newAK1[m] = bestG < 0 ? 0 : bestG;
        }
        m_stack.setModules(k, std::move(newAK), nk, std::move(newAK1));
        // Re-aggregate every level above: a cross-grandparent relocation changes
        // their aggregates. The unit counts stay put (a grandparent that lost all
        // its children survives as an empty module, which costs nothing in the
        // codelength and keeps the higher assignments valid).
        m_stack.reaggregateFrom(k + 1);
      } else {
        m_stack.setModules(k, std::move(newAK), nk);
      }
    }

    double splitL;
    {
      PhaseTimer pt(4, phaseTiming);
      splitL = hierarchicalCodelengthFromStack();
    }
    if (splitL < L - kMinImprovement) {
      m_stack.commit(checkpoint);
      const double relGain = L > 0.0 ? (L - splitL) / L : 0.0;
      g_hSplitGainNano[std::min(k, kHSplitMaxLevels - 1)].fetch_add(static_cast<long long>(relGain * 1e9), std::memory_order_relaxed);
      L = splitL;
      g_hSplitAccepts[std::min(k, kHSplitMaxLevels - 1)].fetch_add(1, std::memory_order_relaxed);
      return true;
    }
    {
      PhaseTimer pt(5, phaseTiming);
      m_stack.rollback(checkpoint);
    }
    return false;
  };

  // Intersect a finer labeling of the level-k units with the current modules:
  // pieces = (label ∩ module), well-defined even when the labeling has drifted.
  auto intersectPieces = [&](const std::vector<int>& labels, std::vector<int>& unitToPiece, std::vector<int>& pieceParent) {
    std::unordered_map<long long, int> pieceId;
    pieceId.reserve(static_cast<std::size_t>(K) * 2);
    unitToPiece.resize(nU);
    pieceParent.clear();
    for (int u = 0; u < nU; ++u) {
      const int mod = m_stack.parent(k, u);
      const long long key = static_cast<long long>(labels[u]) * (static_cast<long long>(K) + 1) + mod;
      auto res = pieceId.emplace(key, static_cast<int>(pieceParent.size()));
      if (res.second)
        pieceParent.push_back(mod);
      unitToPiece[u] = res.first->second;
    }
  };

  std::vector<int> scratchPieces;
  std::vector<int> scratchParent;

  // Piece source 1 (free, leaf level only): the pass-1 building blocks the
  // bottom aggregation consumed — the finest quanta already computed.
  if (!interior && static_cast<int>(m_search.leafBlocks().size()) == nU) {
    intersectPieces(m_search.leafBlocks(), scratchPieces, scratchParent);
    if (recombine(scratchPieces, scratchParent))
      return 1;
  }

  // Piece source 2 (free): the last derivation at this level, projected onto
  // the current partition. Structure shifts little between rounds, so stale
  // pieces usually still propose the right extractions.
  const std::vector<int>& lastPieces = interior ? m_lastLevelPieces[k] : m_lastSinglesPieces;
  if (static_cast<int>(lastPieces.size()) == nU) {
    intersectPieces(lastPieces, scratchPieces, scratchParent);
    if (recombine(scratchPieces, scratchParent))
      return 2;
  }

  // Piece source 3 (the expensive one at leaf level, module-scale and cheap
  // above it): a fresh from-singletons sub-clustering of each module's
  // children — community granularity, so extracting a whole community from an
  // over-merged module is a single gated move.
  const bool productive = interior ? m_levelFreshProductive[k] != 0 : m_freshSinglesProductive;
  if (!m_freshDiscovery || !allowSingletons || (static_cast<int>(lastPieces.size()) == nU && !productive))
    return 0;

  std::vector<int> unitToPiece(nU, -1);
  std::vector<int> pieceParent;
  {
    PhaseTimer ptSub(6, phaseTiming);
    std::vector<std::vector<int>> childrenPer(K);
    for (int u = 0; u < nU; ++u)
      childrenPer[m_stack.parent(k, u)].push_back(u);
    std::vector<int> loc(nU, -1);
    std::vector<int> localAssign;
    for (int P = 0; P < K; ++P) {
      const std::vector<int>& S = childrenPer[P];
      if (S.empty())
        continue;
      if (S.size() == 1) {
        unitToPiece[S[0]] = static_cast<int>(pieceParent.size());
        pieceParent.push_back(P);
        continue;
      }
      int Ksub = 0;
      if (interior) {
        Ksub = subClusterUnits(m_stack.level(k), true, false, S, m_stack.level(k + 1).exit[P], loc, localAssign, false, nullptr);
      } else {
        // A module's sub-clustering depends only on its own leaf set, so
        // unchanged modules reuse earlier rounds' result (memo shared with
        // splitTopModules; only the touched modules are re-clustered).
        auto it = m_subClusterCache.find(S);
        if (it != m_subClusterCache.end()) {
          Ksub = it->second.first;
          localAssign = it->second.second;
        } else {
          Ksub = subClusterLeaves(S, m_stack.level(1).exit[P], loc, localAssign, false);
          m_subClusterCache.emplace(S, std::make_pair(Ksub, localAssign));
        }
      }
      const int firstPiece = static_cast<int>(pieceParent.size());
      for (std::size_t j = 0; j < S.size(); ++j)
        unitToPiece[S[j]] = firstPiece + localAssign[j];
      for (int s = 0; s < Ksub; ++s)
        pieceParent.push_back(P);
    }
  }
  const bool freshImproved = recombine(unitToPiece, pieceParent);
  if (interior) {
    m_lastLevelPieces[k] = std::move(unitToPiece);
    m_levelFreshProductive[k] = freshImproved ? 1 : 0;
  } else {
    m_lastSinglesPieces = std::move(unitToPiece);
    m_freshSinglesProductive = freshImproved;
  }
  return freshImproved ? 3 : 0;
}

bool ColumnarHierarchy::retuneLeavesWithinModules(double& L)
{
  // Seeded leaf fine-tune across the current module level: re-run the leaf
  // move loop (corrections active, co-physical proposals enabled by the
  // seeded phase) from the current assignment, rebuild the module level, and
  // keep the result only if it lowers the true stack codelength.
  const std::vector<int>& module = m_search.moveLeaves(m_stack.assign(0));

  std::vector<int> remap(m_search.activeLevel().n, -1);
  int k = 0;
  for (int i = 0; i < m_search.activeLevel().n; ++i)
    if (remap[module[i]] == -1)
      remap[module[i]] = k++;
  std::vector<int> newTop(nLeaves());
  for (int i = 0; i < nLeaves(); ++i)
    newTop[i] = remap[module[i]];

  return gatedStep(L, [&] {
    m_stack.setModules(0, std::move(newTop), k);
    return true;
  });
}

bool ColumnarHierarchy::seedHierarchyFromLeafPaths(const std::vector<std::vector<int>>& leafPaths)
{
  if (static_cast<int>(leafPaths.size()) != nLeaves() || !m_stack.setFromLeafPaths(leafPaths))
    return false;
  return true;
}

double ColumnarHierarchy::optimizeFromSeed(bool flat, unsigned int sweepLimit)
{
  if (!m_stack.hasModules())
    return std::numeric_limits<double>::infinity(); // nothing seeded

  // The split and merge operators earn their keep on a seed the engine did not
  // build, so every step below runs them as on an external seed (#824).
  const bool flatSeed = m_stack.numLevels() == 2;

  // (1) POLISH the seed at its own granularity.
  //
  // A flat seed (a --two-level run, or a one-level-deep cluster file) gets the
  // two-level interleave: the from-singletons split, the seeded leaf re-tune and the
  // module merge, until the trio stops improving. The split is the operator that
  // matters for an EXTERNAL seed -- it subdivides a supplied module the engine would
  // never have built, the job the object-oriented coarseTune does with a sub-Infomap
  // per module.
  //
  // A deep seed gets the interior-layer refinement, whose k == 0 pass re-derives each
  // grandparent's leaf set from singletons -- the same subdivision, one level down --
  // followed by the module coarsening.
  double L = flatSeed
      ? deepRepairTwoLevelStack(true)
      : refineHierarchy(hierarchicalCodelengthFromStack(), sweepLimit, std::numeric_limits<double>::infinity(), true);
  if (flatSeed)
    // The interleave leaves the leaves at the two-level fixpoint, exactly as
    // completeFlatFromAggregation does.
    m_bottomConverged = true;
  if (flat)
    return L; // --two-level: there is no hierarchy to rebuild

  // (2) REBUILD the hierarchy above the polished bottom, and keep whichever of the
  // two is better.
  //
  // Polishing alone cannot move a level boundary: neither operator in (1) adds or
  // removes a level, so a seed whose SHAPE is wrong for the objective traps the search
  // at its own depth. A FLAT seed is the extreme case -- its bottom is its top, so the
  // whole hierarchy is missing. So take the polished leaf-module partition as the
  // bottom and grow the hierarchy over it with the ordinary enter-flow up-build, then
  // refine that. Measured on powergrid (`--seed 123 -N1 -c`), step (1) alone against
  // step (1) + this:
  //
  // | seed                          | polish only |   + rebuild |
  // |-------------------------------|------------:|------------:|
  // | flat, 339 modules (perturbed) | 5.587611395 | 4.738295187 |
  // | flat, 2 random modules        | 5.620992378 | 4.738600187 |
  // | its own tree cut to 3 levels  |  4.97282865 | 4.777177752 |
  // | its own tree (4-7 levels)     |  4.74974076 |  4.74974076 |
  //
  // -- so up to -15% on a flat seed, -3.9% on a mis-shaped deep one, and nothing lost
  // where the seed's own shape was already right (the last row: the rebuild loses the
  // gate and the polished stack is kept).
  //
  // This is the reading of "continue from this partition" that matches the
  // object-oriented warm start: there, coarseTune re-derives sub-modules inside each
  // module and the recursion rebuilds the structure above them, so the seed decides
  // where the search STARTS and at what granularity, not how many levels the answer
  // has. Keeping the polished stack as a gated candidate is the same
  // best-of-two-shapes the flat-first trial already does (see optimizeFlexible).
  ColumnarStack best = m_stack;
  double bestL = L;

  if (flatSeed) {
    // A flat seed's bottom IS its top, so there is nothing to build over yet. Stack a
    // pass-through copy of it, making the seed modules the leaves' GRANDPARENTS, and
    // re-derive each grandparent's leaf set from singletons: building blocks that
    // respect the seed but are finer than it, which is what the up-build consumes.
    const int K = m_stack.size(1);
    std::vector<int> ident(K);
    for (int i = 0; i < K; ++i)
      ident[i] = i;
    m_stack.pushLevel(std::move(ident), K);
    if (!refineBottomWithinParents()) {
      // No module could be subdivided: the seed is already at leaf granularity, so
      // the up-build would consume the same bottom the polish converged on.
      m_stack = std::move(best);
      return bestL;
    }
  }

  // The rebuilt bottom is a fresh from-singletons derivation, not the converged flat
  // partition, so the leaf-layer refine below is not a re-solve (see m_bottomConverged).
  m_bottomConverged = false;
  const double rebuilt = refineHierarchy(buildHierarchyFromBottom(m_stack.assign(0), m_stack.size(1), 0), sweepLimit, std::numeric_limits<double>::infinity(), true);
  if (rebuilt < bestL - kMinImprovement)
    bestL = rebuilt;
  else
    m_stack = std::move(best);
  return bestL;
}

double ColumnarHierarchy::optimizeHierarchical(unsigned int bottomBlockLimit, unsigned int superAggLimit)
{
  // Bottom: leaves -> building blocks. With bottomBlockLimit > 0 we stop the
  // aggregation early (finer bottom) and skip fine-tune to preserve fineness;
  // with 0 we take the full two-level optimum with fine-tune.
  m_search.optimizeTwoLevel(bottomBlockLimit, bottomBlockLimit == 0);
  return buildHierarchyFromBottom(m_search.leafTopModule(), static_cast<int>(m_search.numTopModules()), superAggLimit);
}

// Grow the multi-level hierarchy from an ALREADY-BUILT bottom two-level (a
// leaf -> module assignment with bottomK modules) using the enter-flow
// super-search. Independent of how the bottom was found, so optimizeColumnar
// builds the bottom once and calls this per up-merge strategy (superAggLimit) —
// the bottom is identical across strategies (superAgg only shapes the up-build),
// so recomputing it was waste. Repeated calls from the same bottom are
// deterministic and produce the same result as a full rebuild.
double ColumnarHierarchy::buildHierarchyFromBottom(std::vector<int> bottom, int bottomK, unsigned int superAggLimit)
{
  using infomath::plogp;

  m_stack.setFlat(std::move(bottom), bottomK);

  // Grow up with the enter-flow super-search while it shortens the index code.
  while (m_stack.numTopModules() > 1) {
    pollInterrupt();
    // Enter-flow transform: the super-network's node flow is the module's index
    // rate q; enter/exit, the teleport aggregates and the edges are the module's
    // inter-module quantities, carried through unchanged.
    Level superNet = m_stack.level(m_stack.topLevel());
    m_search.flow().setIndexRateAsFlow(superNet);

    // Current top-level index codebook (flat): plogp(sum q) - sum plogp(q), over
    // the same index rates the super-search below optimizes (superNet.flow), so
    // the two sides of this comparison measure the same codebook.
    double sumEnter = 0.0, sumPlogpEnter = 0.0;
    for (double q : superNet.flow) {
      sumEnter += q;
      sumPlogpEnter += plogp(q);
    }
    const double curIndexCodelength = plogp(sumEnter) - sumPlogpEnter;

    ColumnarTwoLevel superOpt;
    superOpt.setInterruptCallback(m_search.interruptCallback());
    superOpt.buildFromLevel(std::move(superNet), m_search.flow(), m_search.seed(), 0.0);
    // Conservative up-build (superAggLimit > 0): fewer aggregation passes per
    // super-level so we don't collapse the whole level in one greedy jump —
    // more, finer super-levels for the down-sweep to tune and (later) collapse.
    const double superCodelength = superAggLimit > 0
        ? superOpt.optimizeTwoLevel(superAggLimit, false)
        : superOpt.optimizeTwoLevel();
    const unsigned int superK = superOpt.numTopModules();

    const bool trivial = superK <= 1 || static_cast<int>(superK) == m_stack.numTopModules();
    if (trivial || superCodelength >= curIndexCodelength - kMinImprovement)
      break;

    m_stack.pushLevel(superOpt.leafTopModule(), static_cast<int>(superK));
  }

  return hierarchicalCodelengthFromStack();
}

void ColumnarHierarchy::addSlicedLeafCorrections(ColumnarTwoLevel& subOpt, const std::vector<int>& globalLeafIds) const
{
  for (const auto& c : corrections()) {
    if (!c->participatesInMoveLoop())
      continue; // structural (e.g. Bias): gating-only, not part of the refine
    auto sliced = c->sliceForLeaves(globalLeafIds);
    if (sliced)
      subOpt.addCorrection(std::move(sliced));
  }
}

bool ColumnarHierarchy::buildPartialSeed(const Level& sub, const std::vector<int>& S, const std::vector<int>& assign, int layer, int sweep, std::vector<int>& seed) const
{
  const int nP = sub.n;
  if (!partSeedActive(layer) || (partSeedResweepOnly() && sweep == 0))
    return false; // from singletons, the default
  if (nP <= 1)
    return false; // a single unit has nothing to re-place
  const double q = partSeedRelease();
  const PartSeedMetric metric = partSeedMetric();

  std::vector<char> released(nP, 0);
  if (metric == PartSeedMetric::Random) {
    for (int j = 0; j < nP; ++j) {
      const unsigned long long h = partSeedMix(static_cast<unsigned long long>(m_search.seed())
                                               ^ (0x9e3779b97f4a7c15ULL * static_cast<unsigned long long>(layer + 1))
                                               ^ (0xff51afd7ed558ccdULL * static_cast<unsigned long long>(S[j] + 1)));
      const double u = static_cast<double>(h >> 11) * (1.0 / 9007199254740992.0);
      released[j] = u < q ? 1 : 0;
    }
  } else {
    // loose[j]: how weakly j's current module holds it. Units with no signal at
    // all (no internal link / no flow) count as fully loose, so they are the
    // first to be released rather than silently locked.
    const bool byExit = metric == PartSeedMetric::Exit || metric == PartSeedMetric::InvExit;
    std::vector<double> loose(nP, 1.0);
    for (int j = 0; j < nP; ++j) {
      if (byExit) {
        const double tot = sub.flow[j] + sub.exit[j];
        if (tot > 0.0)
          loose[j] = sub.exit[j] / tot;
      } else {
        const int mj = assign[S[j]];
        double tot = 0.0, out = 0.0;
        for (int e = sub.outStart[j]; e < sub.outStart[j + 1]; ++e) {
          tot += sub.outFlow[e];
          if (assign[S[sub.outTarget[e]]] != mj)
            out += sub.outFlow[e];
        }
        for (int e = sub.inStart[j]; e < sub.inStart[j + 1]; ++e) {
          tot += sub.inFlow[e];
          if (assign[S[sub.inTarget[e]]] != mj)
            out += sub.inFlow[e];
        }
        if (tot > 0.0)
          loose[j] = out / tot;
      }
    }
    int R = static_cast<int>(std::llround(q * nP));
    R = std::max(0, std::min(nP, R));
    std::vector<int> order(nP);
    std::iota(order.begin(), order.end(), 0);
    // Loosest first (tightest first for the inv/iex controls); the local index
    // breaks ties, so the selection is deterministic.
    const bool inverse = metric == PartSeedMetric::InvBoundary || metric == PartSeedMetric::InvExit;
    if (inverse)
      std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return loose[a] < loose[b]; });
    else
      std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return loose[a] > loose[b]; });
    for (int r = 0; r < R; ++r)
      released[order[r]] = 1;
  }

  // Compact the LOCKED units' modules to 0..K-1, then hand each released unit a
  // fresh id K, K+1, ... . locked + released <= nP, so every id stays inside the
  // sub-network's module id space and seedAssignment can rebuild m_emptyModules
  // from the resulting membership counts.
  seed.assign(nP, -1);
  std::unordered_map<int, int> toLocal;
  for (int j = 0; j < nP; ++j) {
    if (released[j])
      continue;
    const int mod = assign[S[j]];
    auto it = toLocal.find(mod);
    if (it == toLocal.end()) {
      const int id = static_cast<int>(toLocal.size());
      toLocal.emplace(mod, id);
      seed[j] = id;
    } else {
      seed[j] = it->second;
    }
  }
  int next = static_cast<int>(toLocal.size());
  for (int j = 0; j < nP; ++j)
    if (released[j])
      seed[j] = next++;
  return true;
}

int ColumnarHierarchy::subClusterLeaves(const std::vector<int>& S, double parentExit, std::vector<int>& loc, std::vector<int>& localAssign, bool fineTune, const std::vector<int>* leafModule, unsigned int maxAggPasses)
{
  return subClusterUnits(leaf0(), false, true, S, parentExit, loc, localAssign, fineTune, leafModule, maxAggPasses);
}

int ColumnarHierarchy::subClusterUnits(const Level& base, bool interior, bool sliceCorrections, const std::vector<int>& S, double parentExit, std::vector<int>& loc, std::vector<int>& localAssign, bool fineTune, const std::vector<int>* unitModule, unsigned int maxAggPasses)
{
  // Build the parent's internal sub-network over its children (global flow --
  // or enter flow for interior units, whose codeword usage is the enter flow --
  // and the edges internal to the parent).
  Level sub = m_search.flow().slice(base, S, loc, interior);

  // Optimal in-context two-level of the parent's children (its exit is the
  // sub-network exit), objective-aware at leaf level: slice Meta/Mem to the
  // leaves so the re-partition optimizes base + correction, not just gates on
  // it. Interior levels stay first-order (as refineLayerWithinGrandparent).
  ColumnarTwoLevel subOpt;
  subOpt.setInterruptCallback(m_search.interruptCallback());
  // Partial seeding: keep the cores of the partition we were handed, release its
  // loose boundary as singletons (leaf layer, so layer == 0). Interior callers
  // pass no partition, which is the from-singletons default. Read `sub` here,
  // before buildFromLevel takes ownership of its storage below.
  std::vector<int> seed;
  const std::vector<int>* seedPtr = nullptr;
  if (unitModule != nullptr && buildPartialSeed(sub, S, *unitModule, 0, 0, seed))
    seedPtr = &seed;

  subOpt.buildFromLevel(std::move(sub), m_search.flow(), m_search.seed(), parentExit);
  subOpt.setRootLeaves(m_search.rootLeaves());
  if (sliceCorrections)
    addSlicedLeafCorrections(subOpt, S);
  subOpt.optimizeTwoLevel(maxAggPasses, fineTune, seedPtr);
  localAssign.assign(subOpt.leafTopModule().begin(), subOpt.leafTopModule().end());
  const int Ksub = static_cast<int>(subOpt.numTopModules());
  return Ksub;
}

bool ColumnarHierarchy::refineBottomWithinParents()
{
  if (m_stack.numLevels() < 3)
    return false; // no super-structure to refine within

  const std::vector<int>& a0 = m_stack.assign(0); // leaf -> L1
  const std::vector<int>& a1 = m_stack.assign(1); // L1 -> L2
  const int numL2 = m_stack.size(2);

  // Group leaves by their level-2 module (the parent to refine within).
  std::vector<std::vector<int>> leavesPer(numL2);
  for (int i = 0; i < nLeaves(); ++i)
    leavesPer[a1[a0[i]]].push_back(i);

  std::vector<int> newA0(nLeaves(), -1); // leaf -> new L1'
  std::vector<int> newA1; // new L1' -> L2
  int nextL1 = 0;
  std::vector<int> loc(nLeaves(), -1); // global leaf -> local id (reused per P)

  std::vector<int> subAssign;
  for (int P = 0; P < numL2; ++P) {
    const std::vector<int>& S = leavesPer[P];
    const int nP = static_cast<int>(S.size());
    if (nP == 0)
      continue;
    // The leaf layer's within-parent re-derivation: the `-F` counterpart of
    // refineLayerWithinGrandparent(0), so it is offered the same partial seed.
    // Inert under the shipped policy — `-F` runs this pass exactly once, and a
    // first refine is never partial-seeded (see partSeedResweepOnly). It is
    // reachable via COL_PARTSEED_ALWAYS, which is how the `-F` half was
    // measured: partial-seeding this single pass costs web-NotreDame +0.08% on
    // 3/3 seeds, because here it replaces the only discovery the search has.
    const int Ksub = subClusterLeaves(S, m_stack.level(2).exit[P], loc, subAssign, true, &a0);

    for (int j = 0; j < nP; ++j)
      newA0[S[j]] = nextL1 + subAssign[j];
    for (int s = 0; s < Ksub; ++s)
      newA1.push_back(P);
    nextL1 += Ksub;
  }

  // Replace the bottom level; levels 2+ are unchanged (same leaf sets per L2).
  m_stack.setModules(0, std::move(newA0), nextL1, std::move(newA1));
  return true;
}

void ColumnarHierarchy::coarsenModules(double& L, int maxSweeps, bool winnerSplit, bool externalSeed)
{
  // Each step is gated (gatedStep): kept only if it lowers the true
  // hierarchical codelength, else reverted. The checkpoint saves just the slots
  // a step writes, so a step that changes nothing costs nothing to gate.
  // Hierarchical split operator (COL_HSPLIT, default off): the merge's dual.
  // Only on stacks with a real hierarchy — a two-level stack is the two-level
  // search's own territory (splitTopModules), which must stay untouched.
  const int splitMode = winnerSplit ? hSplitWinnerMode() : hSplitMode();
  const bool doSplit = splitMode != kHSplitOff && m_stack.numLevels() >= 3;
  // Per-level dirty flag: splitLevelModules(k) is deterministic given the
  // stack, so once it has rejected every source at level k it will keep
  // rejecting until some other operator changes the partition. Skipping the
  // repeat calls is free (identical trajectory) and it is most of the cost —
  // each attempt pays a piece aggregation over the whole level plus a settled
  // move loop, and the convergence tail is nothing but repeats.
  std::vector<char> splitDirty(doSplit ? static_cast<std::size_t>(m_stack.numLevels()) : 0, 1);
  for (int sweep = 0; sweep < maxSweeps; ++sweep) {
    pollInterrupt();
    const bool merged = gatedStep(L, [&] { return mergeLeafModulesWithinParents(externalSeed); });
    const bool regrouped = gatedStep(L, [&] { return refineTopLayer(); });
    bool split = false;
    if (doSplit) {
      const int top = m_stack.topLevel();
      if (merged || regrouped)
        std::fill(splitDirty.begin(), splitDirty.end(), 1);
      // auto/winner: the leaf level only where a module-move-capable correction
      // makes it pay (measured — on base networks k = 0 accepts nothing and is
      // the most expensive level, because refineLayerWithinGrandparent(0) has
      // just re-derived that partition from singletons).
      const bool leafNeedsCorrection = splitMode == kHSplitAuto || splitMode == kHSplitWinner;
      const bool leafAllowed = !leafNeedsCorrection || m_search.hasModuleMoveCorrections();
      for (int k = 0; k + 1 <= top; ++k) {
        if (!hSplitLevelEnabled(splitMode, k) || splitDirty[k] == 0)
          continue;
        if (k == 0 && !leafAllowed)
          continue;
        // cheap/auto ration the leaf level down to the free piece sources; the
        // once-per-run `winner` pass keeps the expensive from-singletons source,
        // which is the discovery it exists to pay for (F20).
        const bool fresh = !((splitMode == kHSplitCheap || splitMode == kHSplitAuto) && k == 0);
        if (splitLevelModules(k, L, fresh) != 0) {
          split = true;
          std::fill(splitDirty.begin(), splitDirty.end(), 1);
        } else {
          splitDirty[k] = 0;
        }
      }
    }
    if (!merged && !regrouped && !split)
      break;
  }
}

double ColumnarHierarchy::optimizeFlexible(unsigned int bottomBlockLimit, unsigned int sweepLimit)
{
  m_bottomConverged = false;
  double flatL = std::numeric_limits<double>::infinity();
  ColumnarStack flatStack;
  double L;
  if (m_flatFirstBottom) {
    // Flat-first trial (see setFlatFirstBottom): probe the flat basin with the
    // full aggregation only, build from the fine-blocks bottom of the same
    // pass-1 as usual, and complete the expensive leaf-level flat pipeline
    // only when the probe is competitive — then keep the better build and the
    // flat stack as a gated candidate (below).
    const double flatEst = m_search.optimizeTwoLevel(0, false);
    const bool probeEscalated = m_search.regroupEscalated(); // same verdict as in optimizeColumnar
    std::vector<int> flatAggTop = m_search.leafTopModule();
    const int flatAggK = static_cast<int>(m_search.numTopModules());
    int fineK = 0;
    for (int b : m_search.leafBlocks())
      if (b >= fineK)
        fineK = b + 1;
    L = buildHierarchyFromBottom(m_search.leafBlocks(), fineK, 0);
    const bool complete = probeEscalated || flatEst < L * (1.0 + kFlatProbeMargin);
#ifdef COLUMNAR_DEBUG
    std::fprintf(stderr, "[flat-first -F] est=%.6f build=%.6f ratio=%.4f %s\n", flatEst, L, flatEst / L, complete ? "complete" : "skip");
#endif
    if (complete) {
      ColumnarStack fine = m_stack;
      flatL = completeFlatFromAggregation(std::move(flatAggTop), flatAggK);
      flatStack = m_stack;
      const double buildFlat = buildHierarchyFromBottom(flatStack.assign(0), flatStack.size(1), 0);
      if (buildFlat < L - kMinImprovement) {
        L = buildFlat;
      } else {
        m_stack = std::move(fine);
        m_bottomConverged = false; // back on the fine-blocks bottom
      }
    }
  } else {
    L = optimizeHierarchical(bottomBlockLimit);
  }
  // Same verdict as optimizeColumnar's (see setAbandonDoomedBuild): a build
  // already worse than one module, with no completed flat candidate to beat it,
  // is handed back unrefined for the fallback and the run-level rescue.
  if (m_abandonDoomedBuild && !(flatL < L) && L > oneLevelCodelength())
    return L;
  // A single bottom re-partition within grandparents. refineBottomWithinParents
  // keeps every leaf inside its level-2 grandparent, so the leaf-set per
  // grandparent is invariant; re-running it re-partitions the same leaf-sets
  // within the same grandparents from singletons with the same seed, which is
  // idempotent (same reasoning as the single-interior-level case in
  // refineHierarchy). A second pass therefore only ever re-derives the same
  // partition to detect convergence — a full O(n_leaves) re-partition of pure
  // overhead — so we stop after one (measured: bit-identical, ~25-35% faster on
  // the deep/memory nets).
  // Skipped on a converged flat bottom: refineBottomWithinParents IS the
  // leaf-layer re-derivation the flat pipeline already converged (see
  // m_bottomConverged) — unless partial seeding is asked to run there anyway
  // (COL_PARTSEED_FLAT), which is a different pass than the one that converged.
  if ((!m_bottomConverged || (partSeedFlatBottom() && partSeedActive(0))) && refineBottomWithinParents())
    L = std::min(L, hierarchicalCodelengthFromStack());
  if (m_abandonUnrecoveredBuild && !(flatL < L) && L > oneLevelCodelength())
    return L;
  // Coarsen (merge leaf modules + regroup the top), exactly as the converge
  // search does. Cheap (module-level, not leaf-level) and a no-op for the base
  // objective, but it is what the memory / metadata / lossy objectives need — a
  // fast search that skipped it landed far from the optimum on those (air30k
  // +14%). With it, -F matches converge on those objectives at a fraction of the
  // cost, and stays unchanged on base networks.
  coarsenModules(L, sweepLimit > 0 ? static_cast<int>(sweepLimit) : 1000);
  // Flat-first trial: the super-build may not pay for itself — keep the flat
  // two-level stack when it beats the refined hierarchy.
  if (flatL < L - kMinImprovement) {
    m_stack = std::move(flatStack);
    L = flatL;
  }
  return L;
}

bool ColumnarHierarchy::refineLayerWithinGrandparent(int k, int sweep)
{
  const int top = m_stack.topLevel();
  if (k < 0 || k + 2 > top)
    return false; // need a layer-(k+2) grandparent to refine within

  const Level& base = m_stack.level(k); // layer-k units to re-partition
  const std::vector<int>& aC = m_stack.assign(k); // unit_k -> module_{k+1}
  const std::vector<int>& aP = m_stack.assign(k + 1); // module_{k+1} -> module_{k+2}
  const Level& grand = m_stack.level(k + 2);
  const int numGP = grand.n;
  const int nU = base.n;

  // Group units by their grandparent (layer k+2), the boundary they may not cross.
  std::vector<std::vector<int>> unitsPer(numGP);
  for (int u = 0; u < nU; ++u)
    unitsPer[aP[aC[u]]].push_back(u);

  std::vector<int> newAC(nU, -1); // unit_k -> new module_{k+1}
  std::vector<int> newAP; // new module_{k+1} -> grandparent (module_{k+2})
  int nextMod = 0;
  std::vector<int> loc(nU, -1); // global unit -> local id (reused per grandparent)

  // Layer-0 units are leaves (two-level leaf objective: node flow = leaf flow).
  // Interior units are modules coded by the module-of-modules term, whose
  // codeword usage is the module's index rate q (its enter rate) — mirror the
  // up-build's enter-flow transform so grouping optimizes the right cost
  // instead of over-merging on total flow.
  const bool interior = (k > 0);

  for (int G = 0; G < numGP; ++G) {
    const std::vector<int>& S = unitsPer[G];
    const int nP = static_cast<int>(S.size());
    if (nP == 0)
      continue;
    // Build G's internal sub-network over layer-k units (flow/enter/exit carried
    // from the units; only edges internal to G are kept).
    Level sub = m_search.flow().slice(base, S, loc, interior);

    // Optimal in-context two-level of G's units (G's exit = sub-network exit).
    // At k == 0 the units are leaves, so the leaf-shaping corrections apply and
    // are sliced to G's leaves; interior levels (k > 0) stay first-order (base).
    // Partial seeding: lock the cores of the layer-k partition inside G, release
    // its loose boundary as singletons for the sub-optimize to re-place. Read
    // `sub` here, before buildFromLevel takes ownership of its storage below.
    std::vector<int> seed;
    const std::vector<int>* seedPtr = nullptr;
    if (buildPartialSeed(sub, S, aC, k, sweep, seed))
      seedPtr = &seed;

    ColumnarTwoLevel subOpt;
    subOpt.setInterruptCallback(m_search.interruptCallback());
    subOpt.buildFromLevel(std::move(sub), m_search.flow(), m_search.seed(), grand.exit[G]);
    subOpt.setRootLeaves(m_search.rootLeaves());
    if (k == 0)
      addSlicedLeafCorrections(subOpt, S);
    subOpt.optimizeTwoLevel(0, true, seedPtr);
    const std::vector<int>& subAssign = subOpt.leafTopModule();
    const int Ksub = static_cast<int>(subOpt.numTopModules());

    for (int j = 0; j < nP; ++j)
      newAC[S[j]] = nextMod + subAssign[j];
    for (int s = 0; s < Ksub; ++s)
      newAP.push_back(G);
    nextMod += Ksub;
  }

  m_stack.setModules(k, std::move(newAC), nextMod, std::move(newAP));
  return true;
}

bool ColumnarHierarchy::refineTopLayer()
{
  const int top = m_stack.topLevel();
  if (top < 2)
    return false; // need a layer below the top (layer top-1) to regroup
  const int k = top - 1; // re-partition layer-k units into new top modules

  const Level& base = m_stack.level(k);
  const int nU = base.n;

  // One root group over all layer-k units; enter-flow transform (grouping
  // modules, whose codeword usage is the index rate q), root exit = 0.
  std::vector<int> all(nU);
  std::iota(all.begin(), all.end(), 0);
  std::vector<int> loc(nU, -1);
  Level sub = m_search.flow().slice(base, all, loc, true);

  ColumnarTwoLevel subOpt;
  subOpt.setInterruptCallback(m_search.interruptCallback());
  subOpt.buildFromLevel(std::move(sub), m_search.flow(), m_search.seed(), 0.0);
  subOpt.optimizeTwoLevel();
  const int Ksub = static_cast<int>(subOpt.numTopModules());
  if (Ksub <= 1 || Ksub == nU)
    return false; // no useful regrouping (trivial partition)

  m_stack.setModules(k, subOpt.leafTopModule(), Ksub);
  return true;
}

bool ColumnarHierarchy::mergeLeafModulesWithinParents(bool externalSeed)
{
  using infomath::plogp;
  if (!m_stack.hasModules())
    return false; // no module level to merge

  // Corrections that reward coarsening (Mem/Meta/Lossy). No participants => the
  // base objective, where any merge only raises the codelength: nothing to do.
  std::vector<ColumnarCorrection*> corr;
  for (auto& c : corrections())
    if (c->participatesInMoveLoop())
      corr.push_back(c.get());
  // ...unless the partition is an external seed (externalSeed, #824): "any merge only
  // raises the codelength" holds for a partition this engine's aggregation produced,
  // whose module-level move loop already settled every merge, not for an over-fine
  // cluster file. mergeCost below is the full base-objective delta, and the caller
  // gates each accepted sweep on the true stack codelength. Measured: a 32-clique ring
  // seeded with its own planted partition returned that partition unchanged at
  // 3.652410119 under --two-level -- 2.3% above what the same run finds from scratch --
  // because merging adjacent cliques was the only move that helped and it was skipped.
  if (corr.empty() && !externalSeed)
    return false;

  const int K = m_stack.size(1); // leaf modules (level-0 -> 1)
  // Leaf modules are merged within their parent. With a level-2 grandparent each
  // leaf module has an explicit parent; in a two-level tree (e.g. lossy) the
  // parent is the root, i.e. one group whose codebook uses the top enter flows.
  const bool hasGrandparent = m_stack.numLevels() >= 3;
  const int numParents = hasGrandparent ? m_stack.size(2) : 1;

  // Seed the corrections' per-module state at the current leaf-module partition.
  for (auto* c : corr)
    c->initMoveLoop(m_stack.assign(0), K);

  // Mutable leaf-module aggregates + parent map + adjacency (crossing flow).
  std::vector<double> flow = m_stack.level(1).flow;
  std::vector<double> enter = m_stack.level(1).enter;
  std::vector<double> exit = m_stack.level(1).exit;
  std::vector<int> parent = hasGrandparent ? m_stack.assign(1) : std::vector<int>(K, 0);
  std::vector<std::unordered_map<int, double>> adjOut(K), adjIn(K);
  {
    const Level& L1 = m_stack.level(1);
    for (int a = 0; a < K; ++a)
      for (int e = L1.outStart[a]; e < L1.outStart[a + 1]; ++e) {
        const int t = L1.outTarget[e];
        const double f = L1.outFlow[e];
        adjOut[a][t] += f;
        adjIn[t][a] += f;
      }
  }
  // enter/exit are total rates, so a merge subtracts the recorded-teleport flow
  // between the two modules exactly where it subtracts their link flow; without
  // it the pass priced a merge of modules with little link flow between them as
  // pure cost and rejected merges the exact score accepts (#1357).
  ColumnarBoundaryFlow::TeleportSums tele;
  tele.reset(m_search.flow(), m_stack.level(1));

  // Parent module-of-modules "total use" = parent exit + sum of child enter.
  // The root's exit is 0 (whole-network codebook).
  std::vector<double> totalUse(numParents, 0.0);
  if (hasGrandparent)
    for (int p = 0; p < numParents; ++p)
      totalUse[p] = m_stack.level(2).exit[p];
  for (int a = 0; a < K; ++a)
    totalUse[parent[a]] += enter[a];

  std::vector<char> alive(K, 1);
  std::vector<int> mergedInto(K);
  for (int a = 0; a < K; ++a)
    mergedInto[a] = a;

  auto crossing = [&](int a, int b) {
    double w = 0.0;
    const auto o = adjOut[a].find(b);
    if (o != adjOut[a].end()) w += o->second;
    const auto i = adjIn[a].find(b);
    if (i != adjIn[a].end()) w += i->second;
    return w;
  };
  // Delta of folding module a into module b (both alive, same parent).
  auto mergeCost = [&](int a, int b) {
    const double w = crossing(a, b) + tele.betweenModules(a, b);
    const double flowB2 = flow[a] + flow[b];
    const double exitA = exit[a], exitB = exit[b];
    const double enterA = enter[a], enterB = enter[b];
    const double exitB2 = exit[a] + exit[b] - w;
    const double enterB2 = enter[a] + enter[b] - w;
    const double Ta = flow[a] + exitA, Tb = flow[b] + exitB, Tb2 = flowB2 + exitB2;
    // Level-1 leaf-module term: plogp(T) - plogp(exit) - sum_leaf plogp(flow);
    // the per-leaf sums cancel in the merge, leaving flows/exits only.
    const double dL1 = (plogp(Tb2) - plogp(Ta) - plogp(Tb))
        - (plogp(exitB2) - plogp(exitA) - plogp(exitB));
    // Parent module-of-modules term for the shared parent: its total use loses
    // the flow between a and b.
    const int p = parent[b];
    const double tu = totalUse[p];
    const double useDrop = w;
    const double dPar = (plogp(tu - useDrop) - plogp(tu))
        - (plogp(enterB2) - plogp(enterA) - plogp(enterB));
    double d = dL1 + dPar;
    for (auto* c : corr)
      d += c->mergeDelta(a, b);
    return d;
  };

  // Co-physical merge candidates beyond the edge-connected set. Redundant on
  // undirected clustering (co-attribute modules are 2-hop connected, so already
  // edge-adjacent) and only a small, inconsistent effect on directed, so kept as
  // a tuning knob (COL_COMERGE) rather than a default. Off by default.
  const bool coPhysicalMerge = (coMergeMode() != 0);
  std::vector<int> mergeCand;

  int totalMerges = 0;
  bool anyPass = true;
  while (anyPass) {
    anyPass = false;
    for (int a = 0; a < K; ++a) {
      if (!alive[a])
        continue;
      // Candidates: alive same-parent modules that are either connected in the
      // L1 network or (with the tuning mode on) share an attribute with a.
      int bestB = -1;
      double bestD = -kMinImprovement;
      auto consider = [&](int b) {
        if (b == a || !alive[b] || parent[b] != parent[a])
          return;
        const double d = mergeCost(a, b);
        if (d < bestD) {
          bestD = d;
          bestB = b;
        }
      };
      for (const auto& kv : adjOut[a])
        consider(kv.first);
      for (const auto& kv : adjIn[a])
        consider(kv.first);
      if (coPhysicalMerge) {
        mergeCand.clear();
        for (auto* c : corr)
          c->proposeMergePartners(a, mergeCand);
        for (int b : mergeCand)
          consider(b);
      }
      if (bestB < 0)
        continue;

      const int b = bestB;
      const double w = crossing(a, b) + tele.betweenModules(a, b);
      // Commit aggregates.
      flow[b] += flow[a];
      const double newExit = exit[a] + exit[b] - w;
      const double newEnter = enter[a] + enter[b] - w;
      exit[b] = newExit;
      enter[b] = newEnter;
      totalUse[parent[b]] -= w;
      tele.merge(a, b);
      for (auto* c : corr)
        c->applyMerge(a, b);
      // Rewire adjacency: fold a's edges into b.
      for (const auto& kv : adjOut[a]) {
        const int x = kv.first;
        if (x == b) continue;
        adjOut[b][x] += kv.second;
        adjIn[x][b] += kv.second;
        adjIn[x].erase(a);
      }
      for (const auto& kv : adjIn[a]) {
        const int x = kv.first;
        if (x == b) continue;
        adjOut[x][b] += kv.second;
        adjIn[b][x] += kv.second;
        adjOut[x].erase(a);
      }
      adjOut[b].erase(a);
      adjIn[b].erase(a);
      adjOut[a].clear();
      adjIn[a].clear();
      alive[a] = 0;
      mergedInto[a] = b;
      ++totalMerges;
      anyPass = true;
    }
  }

  if (totalMerges == 0)
    return false;

  // Rebuild the leaf-module level from the surviving modules.
  std::function<int(int)> root = [&](int x) {
    while (mergedInto[x] != x)
      x = mergedInto[x];
    return x;
  };
  std::vector<int> newId(K, -1);
  int Knew = 0;
  for (int a = 0; a < K; ++a)
    if (alive[a])
      newId[a] = Knew++;
  std::vector<int> newA0(nLeaves());
  for (int i = 0; i < nLeaves(); ++i)
    newA0[i] = newId[root(m_stack.leafModule(i))];

  // In a 3+-level tree the merged leaf modules keep their (unchanged) parent; in
  // a two-level tree there is no parent-assignment level to rewrite.
  if (hasGrandparent) {
    std::vector<int> newA1(Knew);
    for (int a = 0; a < K; ++a)
      if (alive[a])
        newA1[newId[a]] = parent[a];
    m_stack.setModules(0, std::move(newA0), Knew, std::move(newA1));
  } else {
    m_stack.setModules(0, std::move(newA0), Knew);
  }
  return true;
}

double ColumnarHierarchy::optimizeConverge(unsigned int bottomBlockLimit, unsigned int superAggLimit, unsigned int sweepLimit)
{
  const double L = optimizeHierarchical(bottomBlockLimit, superAggLimit);
  return refineHierarchy(L, sweepLimit);
}

double ColumnarHierarchy::refineHierarchy(double startL, unsigned int sweepLimit, double abandonAboveL, bool externalSeed)
{
  double L = startL;

  // sweepLimit caps the up/down tuning sweeps; 0 means run until convergence
  // (the loop breaks early on a no-improvement sweep regardless of the cap).
  const int maxSweeps = sweepLimit > 0 ? static_cast<int>(sweepLimit) : 1000;

  // Up/down sweep: refine each interior layer within its grandparent, bottom-up,
  // accepting a refine only if it lowers the true hierarchical codelength (the
  // interior refine optimizes a units-as-leaves proxy, so we gate on the real
  // objective and revert otherwise). Iterate whole sweeps until none improves.
#ifdef COLUMNAR_DEBUG
  std::fprintf(stderr, "[refine] start L=%.9f levels=%d\n", L, m_stack.numLevels());
#endif
#ifdef COLUMNAR_DEBUG
  const std::clock_t t_p1 = std::clock();
#endif
  // Interior-layer refinement (up/down sweep): re-partition each interior layer
  // within its grandparent, to convergence. This is the expensive pass (the
  // k==0 refine re-partitions every leaf), so it runs ONCE here as a build
  // refinement. The module coarsening below only ever re-fragments a finished
  // interior partition (measured: such a refine always reverts), so it is not
  // worth re-running per coarsening step.
  // With a single interior level the refine is idempotent (deterministic given
  // fixed grandparents, which it does not change), so one pass is convergence —
  // skip the second no-op pass. Multiple interior levels can interact, so loop.
  const int numInterior = std::max(0, m_stack.numLevels() - 2);
  const int refineSweeps = (numInterior <= 1) ? std::min(1, maxSweeps) : maxSweeps;
  const double relStop = m_minRelTuneImprovement * startL;
  // Incremental sweeps. refineLayerWithinGrandparent(k) reads only layers k, k+1,
  // k+2 and is idempotent given fixed grandparents (units can't cross grandparent
  // boundaries), so an accepted refine(k) can only change the inputs of its
  // neighbours k-1 and k+1 — every other layer would deterministically re-derive
  // the same partition and revert. Track a dirty set and re-refine only layers a
  // neighbour touched last sweep: same fixpoint as re-sweeping all layers, but it
  // skips the costly no-op passes (notably the k==0 all-leaves re-partition) once
  // the hierarchy settles. (refine(k) never changes the level count, so the
  // interior-layer index set is stable across sweeps.)
  std::vector<char> dirty(static_cast<size_t>(numInterior), 1);
  // The leaf layer of a converged flat bottom is already at the two-level
  // fixpoint — re-deriving it from singletons within each grandparent only
  // re-finds it (see m_bottomConverged). Start it clean; an accepted refine of
  // the layer above marks it dirty again. A partially seeded refine is NOT that
  // same pass, so COL_PARTSEED_FLAT keeps the layer dirty and lets it run.
  if (m_bottomConverged && numInterior > 0 && !(partSeedFlatBottom() && partSeedActive(0)))
    dirty[0] = 0;
  for (int sweep = 0; sweep < refineSweeps; ++sweep) {
    pollInterrupt();
    const double beforeSweep = L;
    bool improved = false;
    const int top = m_stack.topLevel();
    std::vector<char> nextDirty(dirty.size(), 0);
    for (int k = 0; k + 2 <= top; ++k) {
      if (!dirty[k])
        continue;
#ifdef COLUMNAR_DEBUG
      const double beforeRefine = L;
#endif
      if (gatedStep(L, [&] { return refineLayerWithinGrandparent(k, sweep); })) {
#ifdef COLUMNAR_DEBUG
        std::fprintf(stderr, "[refine] REFINE %.9f -> %.9f (levels %d, top %d)\n", beforeRefine, L, m_stack.numLevels(), m_stack.numTopModules());
#endif
        improved = true;
        if (k - 1 >= 0)
          nextDirty[k - 1] = 1;
        if (k + 1 + 2 <= top) // k+1 is a valid interior layer
          nextDirty[k + 1] = 1;
      }
    }
    if (!improved)
      break;
    // Diminishing-returns knee: stop once a whole sweep's gain drops below the
    // relative threshold (avoids grinding the last fraction of a percent, and the
    // extra no-improvement sweep that only detects full convergence).
    if (relStop > 0.0 && (beforeSweep - L) < relStop)
      break;
    dirty = std::move(nextDirty);
  }
  if (L > abandonAboveL)
    return L;

  // Module coarsening: merge leaf modules (mergeLeafModulesWithinParents) and
  // regroup the top level (refineTopLayer), interleaved to convergence. Both
  // operate on the few modules (not the leaves), so this loop is cheap; iterating
  // lets a merge cross parents that a prior top-refine has regrouped. No-op for
  // the base objective (merge needs a correction; top-refine is gated).
#ifdef COLUMNAR_DEBUG
  const std::clock_t t_p2 = std::clock();
#endif
  coarsenModules(L, maxSweeps, false, externalSeed);
#ifdef COLUMNAR_DEBUG
  const std::clock_t t_end = std::clock();
  std::fprintf(stderr, "[refine] interior=%.3fs coarsen=%.3fs\n", double(t_p2 - t_p1) / CLOCKS_PER_SEC, double(t_end - t_p2) / CLOCKS_PER_SEC);
#endif
  return L;
}

std::vector<std::pair<unsigned int, std::vector<unsigned int>>>
ColumnarHierarchy::toNodePaths(const std::vector<InfoNode*>& leafNodes) const
{
  std::vector<std::pair<unsigned int, std::vector<unsigned int>>> paths;
  const int nLeaves = m_stack.numLevels() == 0 ? 0 : m_stack.size(0);
  const int top = m_stack.topLevel(); // number of module levels
  paths.reserve(nLeaves);

  // A dissolved (ragged) result cannot be walked off the rectangular stack: emit
  // the per-leaf paths dissolveUnprofitableLevels produced (coarsest-first module
  // ids, already 1-based since the module-tree's root is 0), plus the leaf slot.
  if (!m_dissolvedPaths.empty()) {
    for (int i = 0; i < nLeaves; ++i) {
      std::vector<unsigned int> path;
      path.reserve(m_dissolvedPaths[i].size() + 1);
      for (int id : m_dissolvedPaths[i])
        path.push_back(static_cast<unsigned int>(id));
      path.push_back(1); // leaf-rank slot (unused by initTree)
      paths.emplace_back(leafNodes[i]->stateId, std::move(path));
    }
    return paths;
  }

  if (top < 1)
    return paths; // no module structure to materialize

  for (int i = 0; i < nLeaves; ++i) {
    // Walk the assignment stack upward: chain[k] = level-(k+1) module id of leaf i.
    std::vector<int> chain;
    chain.reserve(top);
    int u = i;
    for (int k = 0; k < top; ++k) {
      u = m_stack.parent(k, u);
      chain.push_back(u);
    }
    // Emit coarsest-first (top module .. finest module), 1-based, + leaf slot.
    std::vector<unsigned int> path;
    path.reserve(top + 1);
    for (int k = top - 1; k >= 0; --k)
      path.push_back(static_cast<unsigned int>(chain[k]) + 1);
    path.push_back(1); // leaf-rank slot (unused by initTree)
    paths.emplace_back(leafNodes[i]->stateId, std::move(path));
  }
  return paths;
}

double ColumnarHierarchy::optimizeColumnar(unsigned int bottomBlockLimit, unsigned int sweepLimit)
{
  m_bottomConverged = false;
  // The up-merge aggressiveness selects which basin the build lands in, and the
  // best setting varies by network (like Infomap's multi-trial search). The old
  // approach refined *every* setting to convergence and kept the best — but the
  // expensive interior-layer refinement then ran once per setting, and the
  // losing setting's refinement was always discarded (up to ~half the optimize
  // time on large, deep networks).
  //
  // Instead, SCREEN cheaply: build each setting (no refinement) and compare the
  // post-build codelength, which empirically predicts the converged winner (the
  // up-merge choice fixes the basin; refinement only tunes within it). Then run
  // the interior-layer refinement to convergence on the winning build ONLY.
  //
  // The bottom two-level is IDENTICAL across strategies (superAggLimit only
  // shapes the up-build), so build the bottom ONCE and grow each strategy from
  // the shared bottom via buildHierarchyFromBottom — half the build work,
  // bit-identical result.
  static const unsigned int kSuperAggSettings[] = { 0u, 1u };

  double flatEst = std::numeric_limits<double>::infinity();
  bool probeEscalated = false;
  std::vector<int> flatAggTop;
  int flatAggK = 0;
  if (m_flatFirstBottom) {
    // Flat-first trial (see setFlatFirstBottom): probe the flat basin with the
    // full aggregation only (module-level cost, no leaf fine-tune), and keep
    // the fine-blocks bottom from the same pass-1 (m_search.leafBlocks()) for the
    // regular screen below — no second leaf sweep. The expensive leaf-level
    // flat pipeline runs after the screen, only when the probe is competitive
    // or the probe's own regroup ladder escalated (below).
    flatEst = m_search.optimizeTwoLevel(0, false);
    probeEscalated = m_search.regroupEscalated();
    flatAggTop = m_search.leafTopModule();
    flatAggK = static_cast<int>(m_search.numTopModules());
  } else {
    m_search.optimizeTwoLevel(bottomBlockLimit, bottomBlockLimit == 0);
  }
  // The bottom every strategy grows from: the fine blocks of the probe's pass 1
  // on a flat-first trial, else the two-level partition itself.
  const std::vector<int>& bottom = m_flatFirstBottom ? m_search.leafBlocks() : m_search.leafTopModule();
  int bottomK = static_cast<int>(m_search.numTopModules());
  if (m_flatFirstBottom) {
    bottomK = 0;
    for (int b : m_search.leafBlocks())
      if (b >= bottomK)
        bottomK = b + 1;
  }

  double bestBuildL = std::numeric_limits<double>::infinity();
  ColumnarStack best;
  [[maybe_unused]] unsigned int bestSuperAgg = 0; // reported under COLUMNAR_DEBUG
  bool bestBottomConverged = false;

  for (unsigned int superAgg : kSuperAggSettings) {
    pollInterrupt();
    const double buildL = buildHierarchyFromBottom(bottom, bottomK, superAgg);
#ifdef COLUMNAR_DEBUG
    std::fprintf(stderr, "[screen] superAgg=%u buildL=%.6f top=%d\n", superAgg, buildL, m_stack.numTopModules());
#endif
    if (buildL < bestBuildL - kMinImprovement) {
      bestBuildL = buildL;
      best = m_stack;
      bestSuperAgg = superAgg;
      bestBottomConverged = false;
    }
  }

  // Flat-first trial: complete the flat pipeline when the probe lands within
  // the margin, keep the flat stack as a candidate, and screen the up-builds
  // grown from the flat bottom as additional strategies.
  double flatL = std::numeric_limits<double>::infinity();
  ColumnarStack flatStack;
  if (m_flatFirstBottom) {
    // The regroup ladder escalating inside the probe is the detector's own
    // verdict that this trial sits in the group-hysteresis basin (F42): the
    // greedy fixpoint is not the objective's optimum, and the fine-blocks
    // up-build is the same greedy machinery. There the probe is the wrong
    // instrument for the gate -- on om8 E100000 it undersells the completed
    // flat pipeline by 3.7% (7.257 against 7.000), because the correction-driven
    // merge/retune that completion runs is where this family's gain is, the
    // reverse of the asymmetry the 0.5% margin was calibrated on (F21) -- so
    // complete regardless of the margin. With a quiet detector the margin
    // decides as before, so healthy rows are bit-identical (#1041, F56).
    const bool complete = probeEscalated || flatEst < bestBuildL * (1.0 + kFlatProbeMargin);
#ifdef COLUMNAR_DEBUG
    std::fprintf(stderr, "[flat-first] est=%.6f build=%.6f ratio=%.4f %s\n", flatEst, bestBuildL, flatEst / bestBuildL, complete ? "complete" : "skip");
#endif
    if (complete) {
      flatL = completeFlatFromAggregation(std::move(flatAggTop), flatAggK);
      flatStack = m_stack;
      const int flatK = flatStack.size(1);
      for (unsigned int superAgg : kSuperAggSettings) {
        pollInterrupt();
        const double buildL = buildHierarchyFromBottom(flatStack.assign(0), flatK, superAgg);
        if (buildL < bestBuildL - kMinImprovement) {
          bestBuildL = buildL;
          best = m_stack;
          bestSuperAgg = superAgg;
          bestBottomConverged = true;
        }
      }
    }
  }

  // Restore the winning build and refine only it.
  m_stack = std::move(best);
  m_bottomConverged = bestBottomConverged;
  // A build that starts worse than one module is in the wrong basin; see
  // setAbandonDoomedBuild. A completed flat candidate (flatL finite) beats it
  // below anyway, so the abandoned build never reaches the caller in that case.
  const bool doomed = m_abandonDoomedBuild && bestBuildL > oneLevelCodelength();
#ifdef COLUMNAR_DEBUG
  std::fprintf(stderr, "[build] best=%.6f one-level=%.6f ratio=%.4f %s\n", bestBuildL, oneLevelCodelength(), bestBuildL / oneLevelCodelength(), doomed ? "abandoned" : "refine");
#endif
  const double abandonAboveL = m_abandonUnrecoveredBuild ? oneLevelCodelength() : std::numeric_limits<double>::infinity();
  double bestL = doomed ? bestBuildL : refineHierarchy(bestBuildL, sweepLimit, abandonAboveL);
  // Flat-first trial: the super-build may not pay for itself — keep the flat
  // two-level stack when it beats the refined hierarchy.
  if (flatL < bestL - kMinImprovement) {
    m_stack = std::move(flatStack);
    bestL = flatL;
  }
#ifdef COLUMNAR_DEBUG
  {
    const double corr = objectiveCorrection(m_stack);
    std::fprintf(stderr, "[optColumnar] winner superAgg=%u total=%.6f base=%.6f corr=%.6f top=%d\n", bestSuperAgg, bestL, bestL - corr, corr, m_stack.numTopModules());
  }
#endif
  return bestL;
}

} // namespace infomap
