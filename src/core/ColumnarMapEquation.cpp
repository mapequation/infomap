/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "ColumnarMapEquation.h"
#include "InfoNode.h"
#include "InfoEdge.h"
#include "../utils/infomath.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>

#ifdef COLUMNAR_DEBUG
#include <cstdint>
#include <cstdio>
#endif

#include "ColumnarObjective.h"
#include "ColumnarTuning.h"

namespace infomap {

using namespace columnar;

void ColumnarTwoLevel::buildFromLeaves(const std::vector<InfoNode*>& leafNodes, bool undirected, unsigned long seed, bool recordedTeleport)
{
  using infomath::plogp;
  m_seed = seed;
  m_exitNetworkFlow = 0.0; // whole network is closed
  m_nLeaves = static_cast<int>(leafNodes.size());

  std::unordered_map<const InfoNode*, int> leafId;
  leafId.reserve(static_cast<std::size_t>(m_nLeaves) * 2);
  for (int i = 0; i < m_nLeaves; ++i)
    leafId[leafNodes[i]] = i;

  Level& L = m_leaf0Owned;
  m_leaf0Ptr = &m_leaf0Owned;
  m_lvlPtr = &m_leaf0Owned;
  L.n = m_nLeaves;
  L.flow.resize(m_nLeaves);
  L.enter.resize(m_nLeaves);
  L.exit.resize(m_nLeaves);
  L.teleFlow.resize(m_nLeaves);
  L.teleWeight.resize(m_nLeaves);
  m_leafFlow.resize(m_nLeaves);
  m_leafTeleFlow.resize(m_nLeaves);
  m_leafTeleWeight.resize(m_nLeaves);
  m_leafNodeFlow_log_nodeFlow = 0.0;
  for (int i = 0; i < m_nLeaves; ++i) {
    L.flow[i] = leafNodes[i]->data.flow;
    L.enter[i] = leafNodes[i]->data.enterFlow; // total rates, teleport included
    L.exit[i] = leafNodes[i]->data.exitFlow;
    m_leafFlow[i] = L.flow[i];
    // Recorded-teleportation bookkeeping (used only when setRecordedTeleportation
    // is on); harmless to populate otherwise.
    L.teleFlow[i] = leafNodes[i]->data.teleportFlow;
    L.teleWeight[i] = leafNodes[i]->data.teleportWeight;
    m_leafTeleFlow[i] = L.teleFlow[i];
    m_leafTeleWeight[i] = L.teleWeight[i];
    m_leafNodeFlow_log_nodeFlow += plogp(L.flow[i]);
  }
#if INFOMAP_FEATURE_REGULARIZED_MULTILAYER
  // Per-layer teleport data (regularized multilayer prior), present on every leaf
  // or none (InfomapBase::initNetwork): one entry per leaf, compact layer ids.
  bool layered = false;
  for (const InfoNode* leaf : leafNodes)
    layered = layered || !leaf->layerTeleFlowData().empty();
  if (layered) {
    std::map<unsigned int, int> compactLayer;
    for (const InfoNode* leaf : leafNodes)
      for (const auto& d : leaf->layerTeleFlowData())
        compactLayer.emplace(d.layerId, 0);
    int numLayers = 0;
    for (auto& kv : compactLayer)
      kv.second = numLayers++;
    L.layerStart.assign(m_nLeaves + 1, 0);
    for (int i = 0; i < m_nLeaves; ++i) {
      for (const auto& d : leafNodes[i]->layerTeleFlowData()) {
        L.layerId.push_back(compactLayer[d.layerId]);
        L.layerTeleFlow.push_back(d.teleportFlow);
        L.layerTeleWeight.push_back(d.teleportWeight);
      }
      L.layerStart[i + 1] = static_cast<int>(L.layerId.size());
    }
  }
#endif
  m_flow = ColumnarBoundaryFlow::forLeaves(L, undirected, recordedTeleport);

  // Out + in CSR from leaf out-edges (in is the transpose), matching the OO
  // core's use of outEdges()/inEdges() in the move loop.
  std::vector<int> outDeg(m_nLeaves, 0), inDeg(m_nLeaves, 0);
  for (int i = 0; i < m_nLeaves; ++i) {
    for (InfoEdge* e : leafNodes[i]->outEdges()) {
      auto it = leafId.find(e->target);
      if (it == leafId.end())
        continue;
      ++outDeg[i];
      ++inDeg[it->second];
    }
  }
  L.outStart.assign(m_nLeaves + 1, 0);
  L.inStart.assign(m_nLeaves + 1, 0);
  for (int i = 0; i < m_nLeaves; ++i) {
    L.outStart[i + 1] = L.outStart[i] + outDeg[i];
    L.inStart[i + 1] = L.inStart[i] + inDeg[i];
  }
  L.outTarget.assign(L.outStart[m_nLeaves], 0);
  L.outFlow.assign(L.outStart[m_nLeaves], 0.0);
  L.inTarget.assign(L.inStart[m_nLeaves], 0);
  L.inFlow.assign(L.inStart[m_nLeaves], 0.0);
  std::vector<int> outPos(L.outStart.begin(), L.outStart.end() - 1);
  std::vector<int> inPos(L.inStart.begin(), L.inStart.end() - 1);
  for (int i = 0; i < m_nLeaves; ++i) {
    for (InfoEdge* e : leafNodes[i]->outEdges()) {
      auto it = leafId.find(e->target);
      if (it == leafId.end())
        continue;
      const int j = it->second;
      const double f = e->data.flow;
      L.outTarget[outPos[i]] = j;
      L.outFlow[outPos[i]] = f;
      ++outPos[i];
      L.inTarget[inPos[j]] = i;
      L.inFlow[inPos[j]] = f;
      ++inPos[j];
    }
  }
}

void ColumnarTwoLevel::buildFromLevel(Level level, const ColumnarBoundaryFlow& flow, unsigned long seed, double exitNetworkFlow)
{
  m_leaf0Owned = std::move(level);
  initLeafContext(&m_leaf0Owned, flow, seed, exitNetworkFlow);
}

void ColumnarTwoLevel::buildFromBorrowedLevel(const Level& level, const ColumnarBoundaryFlow& flow, unsigned long seed, double exitNetworkFlow)
{
  // No copy: the caller owns `level` and keeps it alive and unchanged (see the
  // header contract). Everything below reads the leaf network through leaf0().
  initLeafContext(&level, flow, seed, exitNetworkFlow);
}

void ColumnarTwoLevel::initLeafContext(const Level* leaf, const ColumnarBoundaryFlow& flow, unsigned long seed, double exitNetworkFlow)
{
  using infomath::plogp;
  // Inherit the whole flow model: the teleport totals stay the whole network's,
  // not this level's (the level already carries the per-unit teleport sums).
  m_flow = flow;
  m_seed = seed;
  m_exitNetworkFlow = exitNetworkFlow;
  m_leaf0Ptr = leaf;
  // Until a search activates a level explicitly, the active level is the leaf
  // network — never a null pointer.
  m_lvlPtr = leaf;
  m_nLeaves = leaf->n;
  m_rootLeaves = m_nLeaves; // sub-optimizers get the true root's count after build
  m_leafFlow = leaf->flow;
  m_leafTeleFlow = leaf->teleFlow;
  m_leafTeleWeight = leaf->teleWeight;
  m_leafNodeFlow_log_nodeFlow = 0.0;
  for (double f : leaf->flow)
    m_leafNodeFlow_log_nodeFlow += plogp(f);
}

void ColumnarTwoLevel::initPartition()
{
  using infomath::plogp;
  const int n = lvl().n;
  m_module.resize(n);
  m_mFlow.resize(n);
  m_mEnter.resize(n);
  m_mExit.resize(n);
  m_mMembers.assign(n, 1);
  m_emptyModules.clear();

  m_tele.reset(m_flow, lvl());

  m_enterFlow = 0.0;
  m_enter_log_enter = 0.0;
  m_exit_log_exit = 0.0;
  m_flow_log_flow = 0.0;
  for (int i = 0; i < n; ++i) {
    m_module[i] = i;
    m_mFlow[i] = lvl().flow[i];
    m_mEnter[i] = lvl().enter[i];
    m_mExit[i] = lvl().exit[i];
    const double enter = lvl().enter[i];
    const double exit = lvl().exit[i];
    m_flow_log_flow += plogp(lvl().flow[i] + exit);
    m_enter_log_enter += plogp(enter);
    m_exit_log_exit += plogp(exit);
    m_enterFlow += enter;
  }
  // Flow leaving the (sub-)network is coded in the index too (0 if closed).
  m_enterFlow += m_exitNetworkFlow;
  m_enterFlow_log_enterFlow = plogp(m_enterFlow);
  m_nodeFlow_log_nodeFlow = m_leafNodeFlow_log_nodeFlow; // constant over leaves
  m_codelength = (m_enterFlow_log_enterFlow - m_enter_log_enter)
      + (-m_exit_log_exit + m_flow_log_flow - m_nodeFlow_log_nodeFlow);
}

void ColumnarTwoLevel::removeModuleTerms(int m)
{
  using infomath::plogp;
  const double enter = m_mEnter[m];
  const double exit = m_mExit[m];
  m_enterFlow -= enter;
  m_enter_log_enter -= plogp(enter);
  m_exit_log_exit -= plogp(exit);
  m_flow_log_flow -= plogp(exit + m_mFlow[m]);
}

void ColumnarTwoLevel::addModuleTerms(int m)
{
  using infomath::plogp;
  const double enter = m_mEnter[m];
  const double exit = m_mExit[m];
  m_enterFlow += enter;
  m_enter_log_enter += plogp(enter);
  m_exit_log_exit += plogp(exit);
  m_flow_log_flow += plogp(exit + m_mFlow[m]);
}

void ColumnarTwoLevel::moveUnit(int u, int newMod)
{
  using infomath::plogp;
  const int cMod = m_module[u];
  if (newMod == cMod)
    return;

  double dEnterOld = 0.0, dExitOld = 0.0, dEnterNew = 0.0, dExitNew = 0.0;
  for (int k = lvl().outStart[u]; k < lvl().outStart[u + 1]; ++k) {
    const int m = m_module[lvl().outTarget[k]];
    if (m == cMod)
      dExitOld += lvl().outFlow[k];
    else if (m == newMod)
      dExitNew += lvl().outFlow[k];
  }
  for (int k = lvl().inStart[u]; k < lvl().inStart[u + 1]; ++k) {
    const int m = m_module[lvl().inTarget[k]];
    if (m == cMod)
      dEnterOld += lvl().inFlow[k];
    else if (m == newMod)
      dEnterNew += lvl().inFlow[k];
  }
  // Link plus recorded-teleport flow between u and the rest of cMod / newMod.
  const double deltaOld = dEnterOld + dExitOld + m_tele.betweenOwn(lvl(), u, cMod);
  const double deltaNew = dEnterNew + dExitNew + m_tele.between(lvl(), u, newMod);
  const double curEnter = lvl().enter[u];
  const double curExit = lvl().exit[u];
  const double curFlow = lvl().flow[u];

  if (!m_deferTerms) {
    removeModuleTerms(cMod);
    removeModuleTerms(newMod);
  }

  m_mFlow[cMod] -= curFlow;
  m_mEnter[cMod] -= curEnter;
  m_mExit[cMod] -= curExit;
  m_mFlow[newMod] += curFlow;
  m_mEnter[newMod] += curEnter;
  m_mExit[newMod] += curExit;

  m_mEnter[cMod] += deltaOld;
  m_mExit[cMod] += deltaOld;
  m_mEnter[newMod] -= deltaNew;
  m_mExit[newMod] -= deltaNew;

  m_tele.move(lvl(), u, cMod, newMod);

  if (!m_deferTerms) {
    addModuleTerms(cMod);
    addModuleTerms(newMod);
    m_enterFlow_log_enterFlow = plogp(m_enterFlow);
    m_codelength = (m_enterFlow_log_enterFlow - m_enter_log_enter)
        + (-m_exit_log_exit + m_flow_log_flow - m_nodeFlow_log_nodeFlow);
  }

  m_mMembers[cMod] -= 1;
  m_mMembers[newMod] += 1;
  m_module[u] = newMod;
}

void ColumnarTwoLevel::rebuildRunningTerms()
{
  using infomath::plogp;
  m_enterFlow = 0.0;
  m_enter_log_enter = 0.0;
  m_exit_log_exit = 0.0;
  m_flow_log_flow = 0.0;
  const int n = lvl().n;
  for (int m = 0; m < n; ++m) {
    if (m_mMembers[m] == 0)
      continue;
    const double enter = m_mEnter[m];
    const double exit = m_mExit[m];
    m_enterFlow += enter;
    m_enter_log_enter += plogp(enter);
    m_exit_log_exit += plogp(exit);
    m_flow_log_flow += plogp(exit + m_mFlow[m]);
  }
  m_enterFlow += m_exitNetworkFlow;
  m_enterFlow_log_enterFlow = plogp(m_enterFlow);
  m_codelength = (m_enterFlow_log_enterFlow - m_enter_log_enter)
      + (-m_exit_log_exit + m_flow_log_flow - m_nodeFlow_log_nodeFlow);
}

void ColumnarTwoLevel::seedAssignment(const std::vector<int>& assign)
{
  // Start from singletons, then force each unit into its assigned module. The
  // placement is fully deterministic (no move decisions are made), so the
  // running plogp terms are not maintained per move: only the module
  // aggregates evolve (m_deferTerms), and the terms are rebuilt once at the
  // end — one O(K) plogp pass instead of ~12 plogp per placed unit.
  m_deferTerms = true;
  initPartition();
  for (int u = 0; u < lvl().n; ++u)
    moveUnit(u, assign[u]);
  m_deferTerms = false;
  rebuildRunningTerms();
  // Rebuild the empty-module list for the subsequent optimizing move loop.
  m_emptyModules.clear();
  for (int m = 0; m < lvl().n; ++m)
    if (m_mMembers[m] == 0)
      m_emptyModules.push_back(m);
}

const std::vector<int>& ColumnarTwoLevel::moveUnits(Level&& level, const std::vector<int>& seed, const std::vector<int>* leafToUnit)
{
  const int nUnits = level.n;
  activateOwnedLevel(std::move(level));
  m_leafMoveLoop = false;
  m_moduleCorrActive = leafToUnit != nullptr;
  m_seededPhase = true;
  if (leafToUnit != nullptr)
    for (auto& c : m_corrections)
      if (c->participatesInMoveLoop() && c->participatesInModuleMoves())
        c->setUnits(*leafToUnit, nUnits);
  seedAssignment(seed);
  moveLoop();
  m_moduleCorrActive = false;
  if (leafToUnit != nullptr)
    for (auto& c : m_corrections)
      if (c->participatesInMoveLoop() && c->participatesInModuleMoves())
        c->resetUnitsToLeaves();
  return m_module;
}

const std::vector<int>& ColumnarTwoLevel::moveLeaves(const std::vector<int>& seed)
{
  activateLeafLevel();
  m_leafMoveLoop = true;
  m_seededPhase = true;
  seedAssignment(seed);
  moveLoop();
  return m_module;
}

unsigned int ColumnarTwoLevel::moveLoop(unsigned int sweepCap)
{
  using Kind = ColumnarBoundaryFlow::TeleportKind;
  switch (m_tele.kind()) {
  case Kind::global:
    return moveLoopWith<Kind::global>(sweepCap);
  case Kind::layered:
    return moveLoopWith<Kind::layered>(sweepCap);
  default:
    return moveLoopWith<Kind::none>(sweepCap);
  }
}

template <ColumnarBoundaryFlow::TeleportKind Tele>
unsigned int ColumnarTwoLevel::moveLoopWith(unsigned int sweepCap)
{
  using infomath::plogp;
  const int n = lvl().n;

  std::mt19937_64 rng(m_seed + 0x9e3779b97f4a7c15ULL * static_cast<unsigned long long>(n));
  std::vector<int> order(n);
  std::iota(order.begin(), order.end(), 0);
  std::vector<char> dirty(n, 1);

  // Dense scratch for per-unit candidate module deltas.
  std::vector<double> dEnter(n, 0.0), dExit(n, 0.0);
  std::vector<int> touched;
  touched.reserve(64);
  std::vector<int> corrCand; // correction-proposed move targets (e.g. co-physical)
  corrCand.reserve(32);

  // Objective corrections that shape the leaf partition participate in the move
  // loop. On a leaf-level loop (m_leafMoveLoop, unit id == leaf id) all of them
  // do; on a module-level aggregation pass only those that maintain per-unit
  // aggregates (participatesInModuleMoves, unit indexing set via setUnits) —
  // that participation is what makes the aggregation trajectory itself
  // objective-aware (#834). Their contribution is tracked alongside the base
  // codelength so move selection and convergence use the true augmented
  // objective. O(1) per candidate per correction on leaves; O(unit attributes)
  // for aggregated units.
  std::vector<ColumnarCorrection*> corr;
  double correctionTotal = 0.0;
  for (auto& c : m_corrections)
    if (c->participatesInMoveLoop() && (m_leafMoveLoop || (m_moduleCorrActive && c->participatesInModuleMoves())))
      corr.push_back(c.get());
  for (auto* c : corr)
    correctionTotal += c->initMoveLoop(m_module, n);

  // Co-physical move-loop candidates (see coMergeMode): "seeded" restricts them
  // to seeded phases (fine-tune / refine), where base structure already exists.
  const int coMode = coMergeMode();
  const bool proposeExtra = !corr.empty()
      && (coMode == 1 || (coMode == 2 && m_seededPhase));

  double oldCodelength = m_codelength + correctionTotal;
  unsigned int numEffectiveLoops = 0;
  unsigned int coreLoopCount = 0;
  const unsigned int loopLimit = sweepCap != 0 ? sweepCap : kCoreLoopLimit;

  do {
    pollInterrupt();
    ++coreLoopCount;
    std::shuffle(order.begin(), order.end(), rng);
    unsigned int numMoved = 0;

    for (int idx = 0; idx < n; ++idx) {
      const int u = order[idx];
      if (!dirty[u])
        continue;
      const int cMod = m_module[u];

      // Accumulate flow between u and each neighbouring module.
      touched.clear();
      auto touch = [&](int m) {
        if (dEnter[m] == 0.0 && dExit[m] == 0.0)
          touched.push_back(m);
      };
      for (int k = lvl().outStart[u]; k < lvl().outStart[u + 1]; ++k) {
        const int m = m_module[lvl().outTarget[k]];
        touch(m);
        dExit[m] += lvl().outFlow[k];
      }
      for (int k = lvl().inStart[u]; k < lvl().inStart[u + 1]; ++k) {
        const int m = m_module[lvl().inTarget[k]];
        touch(m);
        dEnter[m] += lvl().inFlow[k];
      }
      // Ensure own module is a candidate (the "don't move" option).
      if (dEnter[cMod] == 0.0 && dExit[cMod] == 0.0)
        touched.push_back(cMod);

      // Link plus recorded-teleport flow between u and the rest of its module.
      double deltaOld = dEnter[cMod] + dExit[cMod];
      if constexpr (Tele != ColumnarBoundaryFlow::TeleportKind::none)
        deltaOld += m_tele.betweenOwnAs<Tele>(lvl(), u, cMod);
      const double curEnter = lvl().enter[u];
      const double curExit = lvl().exit[u];
      const double curFlow = lvl().flow[u];

      int bestMod = cMod;
      double bestDelta = 0.0;
      int strongestMod = cMod;
      double strongestExit = 0.0;
      double strongestDelta = 0.0;

      // Option to move to an empty module (if not already alone). Suppressed
      // during a purify-only polish (regroup ladder): there the loop's job is
      // to re-sort units between the seeded groups, not to re-open the finer
      // basin the seed was chosen against — the ladder's earlier rungs already
      // offer every finer scale as its own gated candidate.
      if (!m_noEmptyTargets && m_mMembers[cMod] > 1 && !m_emptyModules.empty()) {
        const int em = m_emptyModules.back();
        if (dEnter[em] == 0.0 && dExit[em] == 0.0)
          touched.push_back(em);
      }

      // Objective-proposed candidates beyond the edge neighborhood (Mem: modules
      // already holding a co-physical state node). These carry no direct flow to
      // u (deltaNew = 0) unless they are also edge neighbours, in which case they
      // are already in `touched`; the zero-flow guard both dedups and admits the
      // flow-disconnected merges the base loop could never reach.
      if (proposeExtra) {
        for (auto* c : corr) {
          corrCand.clear();
          c->proposeMoveTargets(u, corrCand);
          for (int m : corrCand)
            if (m != cMod && dEnter[m] == 0.0 && dExit[m] == 0.0)
              touched.push_back(m);
        }
      }

      // Hoist the old-module (A-side) terms once per unit visit (6 of the 13 plogp
      // per candidate). Per-candidate math below is unchanged, so results stay
      // bit-exact.
      const OldSideTerms oldSide = hoistOldSide(curEnter, curExit, curFlow, m_mEnter[cMod], m_mExit[cMod], m_mFlow[cMod], deltaOld);
      for (int m : touched) {
        if (m == cMod)
          continue;
        // The "strongest" tie-break below stays on link exit flow only.
        double deltaNew = dEnter[m] + dExit[m];
        if constexpr (Tele != ColumnarBoundaryFlow::TeleportKind::none)
          deltaNew += m_tele.betweenAs<Tele>(lvl(), u, m);
        double dl = deltaCodelengthMovingNodeHoisted(
            m_enterFlow, m_enterFlow_log_enterFlow, curEnter, curExit, curFlow, oldSide, m_mEnter[m], m_mExit[m], m_mFlow[m], deltaOld, deltaNew);
        for (auto* c : corr)
          dl += c->moveDelta(u, cMod, m);
        if (dl < bestDelta - kMinSingleImprovement) {
          bestDelta = dl;
          bestMod = m;
        }
        if (dExit[m] > strongestExit) {
          strongestExit = dExit[m];
          strongestMod = m;
          strongestDelta = dl;
        }
      }
      // Prefer the strongest connected module on a codelength tie.
      if (strongestMod != bestMod && strongestDelta <= bestDelta + kMinSingleImprovement)
        bestMod = strongestMod;

      if (bestMod != cMod) {
        double deltaNew = dEnter[bestMod] + dExit[bestMod];
        if constexpr (Tele != ColumnarBoundaryFlow::TeleportKind::none)
          deltaNew += m_tele.betweenAs<Tele>(lvl(), u, bestMod);
        // Update empty-module bookkeeping.
        if (m_mMembers[bestMod] == 0 && !m_emptyModules.empty())
          m_emptyModules.pop_back();
        if (m_mMembers[cMod] == 1)
          m_emptyModules.push_back(cMod);

        // Update running terms + module aggregates (port of updateCodelength...).
        removeModuleTerms(cMod);
        removeModuleTerms(bestMod);

        m_mFlow[cMod] -= curFlow;
        m_mEnter[cMod] -= curEnter;
        m_mExit[cMod] -= curExit;
        m_mFlow[bestMod] += curFlow;
        m_mEnter[bestMod] += curEnter;
        m_mExit[bestMod] += curExit;

        m_mEnter[cMod] += deltaOld;
        m_mExit[cMod] += deltaOld;
        m_mEnter[bestMod] -= deltaNew;
        m_mExit[bestMod] -= deltaNew;

        if constexpr (Tele != ColumnarBoundaryFlow::TeleportKind::none)
          m_tele.moveAs<Tele>(lvl(), u, cMod, bestMod);

        addModuleTerms(cMod);
        addModuleTerms(bestMod);
        m_enterFlow_log_enterFlow = plogp(m_enterFlow);
        m_codelength = (m_enterFlow_log_enterFlow - m_enter_log_enter)
            + (-m_exit_log_exit + m_flow_log_flow - m_nodeFlow_log_nodeFlow);

        // Apply the move to each active correction and track its contribution.
        for (auto* c : corr) {
          correctionTotal += c->moveDelta(u, cMod, bestMod);
          c->applyMove(u, cMod, bestMod);
        }

        m_mMembers[cMod] -= 1;
        m_mMembers[bestMod] += 1;
        m_module[u] = bestMod;
        ++numMoved;

        // Mark neighbours dirty.
        for (int k = lvl().outStart[u]; k < lvl().outStart[u + 1]; ++k)
          dirty[lvl().outTarget[k]] = 1;
        for (int k = lvl().inStart[u]; k < lvl().inStart[u + 1]; ++k)
          dirty[lvl().inTarget[k]] = 1;
      } else {
        dirty[u] = 0;
      }

      // Reset scratch for the touched modules.
      for (int m : touched) {
        dEnter[m] = 0.0;
        dExit[m] = 0.0;
      }
    }

    if (numMoved == 0 || (m_codelength + correctionTotal) >= oldCodelength - kMinImprovement)
      break;
    ++numEffectiveLoops;
    oldCodelength = m_codelength + correctionTotal;
  } while (coreLoopCount != loopLimit);

  m_lastCorrection = correctionTotal; // 0 when no leaf-level corrections active
  return numEffectiveLoops;
}

int ColumnarTwoLevel::consolidateToNextLevel()
{
  // Compact module ids present in m_module -> [0, K).
  const int n = lvl().n;
  std::vector<int> remap(n, -1);
  int K = 0;
  for (int i = 0; i < n; ++i) {
    if (remap[m_module[i]] == -1)
      remap[m_module[i]] = K++;
  }

  Level next;
  next.n = K;
  next.flow.assign(K, 0.0);
  next.enter.assign(K, 0.0);
  next.exit.assign(K, 0.0);
  next.teleFlow.assign(K, 0.0);
  next.teleWeight.assign(K, 0.0);
  // Module aggregates already hold the consolidated flow data, enter/exit as
  // total rates; the teleport sums are 0 unless teleportation is recorded.
  for (int oldM = 0; oldM < n; ++oldM) {
    if (m_mMembers[oldM] == 0)
      continue;
    const int m = remap[oldM];
    next.flow[m] = m_mFlow[oldM];
    next.enter[m] = m_mEnter[oldM];
    next.exit[m] = m_mExit[oldM];
    next.teleFlow[m] = m_tele.flowOf(oldM);
    next.teleWeight[m] = m_tele.weightOf(oldM);
  }
  // Per-layer teleport sums (layered flow model only): each module's layers.
  if (!lvl().layerStart.empty()) {
    std::vector<int> assign(n);
    for (int i = 0; i < n; ++i)
      assign[i] = remap[m_module[i]];
    m_flow.aggregateLayers(lvl(), assign, K, next);
  }

  // Aggregate current-level out-edges into module-module edges.
  std::unordered_map<long long, double> edgeMap;
  edgeMap.reserve(lvl().outTarget.size());
  for (int a = 0; a < n; ++a) {
    const int ma = remap[m_module[a]];
    for (int k = lvl().outStart[a]; k < lvl().outStart[a + 1]; ++k) {
      const int mb = remap[m_module[lvl().outTarget[k]]];
      if (ma == mb)
        continue;
      edgeMap[static_cast<long long>(ma) * K + mb] += lvl().outFlow[k];
    }
  }
  std::vector<int> outDeg(K, 0), inDeg(K, 0);
  for (const auto& kv : edgeMap) {
    const int ma = static_cast<int>(kv.first / K);
    const int mb = static_cast<int>(kv.first % K);
    ++outDeg[ma];
    ++inDeg[mb];
  }
  next.outStart.assign(K + 1, 0);
  next.inStart.assign(K + 1, 0);
  for (int i = 0; i < K; ++i) {
    next.outStart[i + 1] = next.outStart[i] + outDeg[i];
    next.inStart[i + 1] = next.inStart[i] + inDeg[i];
  }
  next.outTarget.assign(next.outStart[K], 0);
  next.outFlow.assign(next.outStart[K], 0.0);
  next.inTarget.assign(next.inStart[K], 0);
  next.inFlow.assign(next.inStart[K], 0.0);
  std::vector<int> outPos(next.outStart.begin(), next.outStart.end() - 1);
  std::vector<int> inPos(next.inStart.begin(), next.inStart.end() - 1);
  for (const auto& kv : edgeMap) {
    const int ma = static_cast<int>(kv.first / K);
    const int mb = static_cast<int>(kv.first % K);
    const double f = kv.second;
    next.outTarget[outPos[ma]] = mb;
    next.outFlow[outPos[ma]] = f;
    ++outPos[ma];
    next.inTarget[inPos[mb]] = ma;
    next.inFlow[inPos[mb]] = f;
    ++inPos[mb];
  }

  // Remap each leaf's top unit through the module compaction.
  for (int i = 0; i < m_nLeaves; ++i)
    m_leafTop[i] = remap[m_module[m_leafTop[i]]];

  activateOwnedLevel(std::move(next));
  return K;
}

double ColumnarTwoLevel::optimizeTwoLevel(unsigned int maxAggPasses, bool doFineTune, const std::vector<int>* pass1Seed)
{
  // Start each optimize from the immutable leaf network. The first move loop
  // (and any fine-tune) operate on leaves; aggregation passes operate on
  // modules, where the module-move-capable corrections stay active through
  // their per-unit aggregates (setUnits below) and the rest drop out.
  activateLeafLevel();
  m_leafMoveLoop = true;
  m_seededPhase = false; // aggregation starts from singletons
  // Defensive: restore leaf indexing in case a prior optimize was interrupted
  // mid-aggregation (corrections would otherwise misread leaf-indexed input).
  for (auto& cp : m_corrections)
    cp->resetUnitsToLeaves();
  m_leafTop.resize(m_nLeaves);
  for (int i = 0; i < m_nLeaves; ++i)
    m_leafTop[i] = i;

  double bestCodelength = std::numeric_limits<double>::infinity();
  std::vector<int> bestTop = m_leafTop;
  unsigned int bestK = static_cast<unsigned int>(lvl().n);
  std::vector<int> runnerUpTop; // see the collapsed-winner comparison after the fine-tune
  unsigned int runnerUpK = 0;
  double runnerUpL = std::numeric_limits<double>::infinity();

  // One optimizing pass over the current m_lvl units: move loop, compose the
  // (compacted) leaf -> module map, and score by the augmented objective (base
  // + correction of leaves -> current modules), not base alone: for Mem the
  // correction is a large fraction of the codelength, so base-only selection
  // optimizes the wrong quantity. Corrections that ran inside the move loop
  // (all of them on the leaf pass; the module-move-capable ones on aggregation
  // passes when m_moduleCorrActive) tracked their contribution incrementally
  // (m_lastCorrection, exact for the composed leaf partition since the
  // per-module aggregates are identical either way); the rest are recomputed
  // from the composed leaf partition.
  auto runPass = [&](std::vector<int>& newTop, int& c, const std::vector<int>* seed = nullptr) -> double {
    if (seed != nullptr) {
      // Singletons, then deterministically place every unit into its seeded
      // module, then improve greedily from there (OO's fine-tune init).
      m_seededPhase = true;
      seedAssignment(*seed);
    } else {
      initPartition();
    }
    moveLoop();
    std::vector<int> remap(lvl().n, -1);
    c = 0;
    for (int i = 0; i < lvl().n; ++i)
      if (remap[m_module[i]] == -1)
        remap[m_module[i]] = c++;
    newTop.assign(m_nLeaves, 0);
    for (int i = 0; i < m_nLeaves; ++i)
      newTop[i] = remap[m_module[m_leafTop[i]]];
    double corrSum = m_lastCorrection;
    if (!m_leafMoveLoop)
      for (auto& cp : m_corrections)
        if (cp->participatesInMoveLoop() && !(m_moduleCorrActive && cp->participatesInModuleMoves()))
          corrSum += cp->initMoveLoop(newTop, c);
    return m_codelength + corrSum;
  };

  // Pass 1: leaves (objective corrections fully active in the move loop).
  unsigned int pass = 1;
  pollInterrupt();
  {
    std::vector<int> newTop;
    int c = 0;
    if (m_adoptedPass1 && pass1Seed == nullptr) {
      // This exact sweep already ran (adoptPass1State): restore what it left -- the
      // assignment and the module aggregates the consolidation below reads -- and
      // compact it the way runPass does.
      Pass1State& kept = *m_adoptedPass1;
      m_module = std::move(kept.module);
      m_mFlow = std::move(kept.mFlow);
      m_mEnter = std::move(kept.mEnter);
      m_mExit = std::move(kept.mExit);
      m_tele = std::move(kept.tele);
      m_mMembers = std::move(kept.mMembers);
      bestCodelength = kept.codelength;
      m_adoptedPass1.reset();
      std::vector<int> remap(lvl().n, -1);
      for (int i = 0; i < lvl().n; ++i)
        if (remap[m_module[i]] == -1)
          remap[m_module[i]] = c++;
      newTop.assign(m_nLeaves, 0);
      for (int i = 0; i < m_nLeaves; ++i)
        newTop[i] = remap[m_module[m_leafTop[i]]];
    } else {
      bestCodelength = runPass(newTop, c, pass1Seed);
      if (m_keepPass1State && pass1Seed == nullptr && !m_pass1State) {
        m_pass1State = std::make_unique<Pass1State>();
        Pass1State& kept = *m_pass1State;
        kept.module = m_module;
        kept.mFlow = m_mFlow;
        kept.mEnter = m_mEnter;
        kept.mExit = m_mExit;
        kept.tele = m_tele;
        kept.mMembers = m_mMembers;
        kept.codelength = bestCodelength;
      }
    }
    m_seededPhase = false;
    bestTop = std::move(newTop);
    bestK = static_cast<unsigned int>(c);
  }
  // Retain the pass-1 partition: the finest building blocks the aggregation
  // consumes. splitTopModules re-sorts them with hindsight of the final
  // coarse structure (the subdivision half of a coarse-tune, #889). Pass 1
  // is the plain leaf loop, so the blocks are identical for every
  // aggregation strategy.
  m_leafBlocks = bestTop;
  const bool aggregate = bestK > 1 && static_cast<int>(bestK) != lvl().n
      && !(maxAggPasses != 0 && pass >= maxAggPasses);

  if (aggregate) {
    consolidateToNextLevel();
    m_leafMoveLoop = false; // subsequent passes aggregate modules, not leaves

    // Module passes (Louvain-style aggregation to convergence). The
    // module-move-capable corrections (Mem/Meta) stay active — setUnits gives
    // them per-unit aggregates so their deltas apply to aggregated units — so
    // the aggregation trajectory itself descends the augmented objective
    // (#834). Known limitation until a split operator exists: the objective-
    // aware module-level gains can overshoot to a coarseness the downstream
    // fine-tune and gated merges cannot split apart again (malaria, +0.4%
    // across seeds) — the price of optimizing the true objective where it
    // matters most (base-only aggregation loses 1.2% on air30k and 7.8% on
    // regularized air30k, and is slower: the coarsening it skips falls on the
    // costlier merge scan + leaf re-tune).
    std::vector<ColumnarCorrection*> unitCorr;
    for (auto& cp : m_corrections)
      if (cp->participatesInMoveLoop() && cp->participatesInModuleMoves())
        unitCorr.push_back(cp.get());

    m_moduleCorrActive = !unitCorr.empty();
    for (auto* cp : unitCorr)
      cp->setUnits(m_leafTop, lvl().n);

    // Retain the aggregation trajectory (unit level + leaf composition per
    // pass) for the descending repair below. Only with module-move-capable
    // corrections: the repair is theirs, and base networks pay nothing.
    // Whichever pass runs on the leaf network goes in as an empty placeholder,
    // read back through trajLevel(), so the trajectory never holds a copy of the
    // leaf CSR. Which slot that is depends on when the module corrections became
    // active, so record it rather than assuming slot 0.
    std::vector<Level> trajLevels;
    std::vector<std::vector<int>> trajComp;
    int trajLeafSlot = -1;
    auto trajLevel = [&](int k) -> const Level& { return k == trajLeafSlot ? leaf0() : trajLevels[k]; };

    while (true) {
      pollInterrupt();
      ++pass;
      if (m_moduleCorrActive) {
        // The leaf network goes in as a placeholder (trajLevel reads it from
        // leaf0()); aggregated levels are copied as before.
        if (&lvl() == &leaf0()) {
          trajLeafSlot = static_cast<int>(trajLevels.size());
          trajLevels.emplace_back();
        } else {
          trajLevels.push_back(lvl());
        }
        trajComp.push_back(m_leafTop);
      }
      std::vector<int> newTop;
      int c = 0;
      const double L = runPass(newTop, c);
      if (L >= bestCodelength - kMinImprovement)
        break;
      // A stalled aggregation ends here: once a pass buys less than
      // kAggregationStallGain of the codelength, the next ones re-sort a few
      // units each (om6_N256_E100k `-2d --regularized`: ~0.001% per pass over
      // ~2,700 units, #1042). Keep the pass, stop there.
      const bool stalled = bestCodelength - L < kAggregationStallGain * bestCodelength;
      bestTop = std::move(newTop);
      bestCodelength = L;
      bestK = static_cast<unsigned int>(c);
      if (c <= 1 || c == lvl().n || stalled)
        break;
      if (maxAggPasses != 0 && pass >= maxAggPasses)
        break; // stop early: keep this (finer) level as the building-block bottom
      consolidateToNextLevel();
      // The next pass moves the new aggregated units (m_leafTop was just
      // remapped by the consolidation to leaf -> new unit).
      for (auto* cp : unitCorr)
        cp->setUnits(m_leafTop, lvl().n);
    }

    // Descending in-trajectory repair (#889): each consolidation makes the
    // previous pass's units atomic, so a merge the objective-aware
    // aggregation overshot cannot be undone by later passes — but at this
    // point the retained trajectory levels still nest EXACTLY inside the
    // converged partition (no fine-tune has moved leaves yet). Re-sort each
    // retained granularity (coarse to fine, skipping the last = the final
    // pass's own fixpoint) within the current best partition with a seeded
    // move loop: extracting a whole overshot sub-community is a single
    // downhill move at its own granularity. Everything here — levels,
    // compositions, per-unit correction aggregates — was already computed by
    // the trajectory; the repair costs one seeded sweep per level over
    // strictly shrinking unit counts. Keep-best on the exact augmented
    // objective, same scoring as runPass. Accepted sweeps regroup whole
    // units, so the nesting invariant survives down the ladder.
    for (int k = static_cast<int>(trajLevels.size()) - 2; k >= 0; --k) {
      pollInterrupt();
      const std::vector<int>& comp = trajComp[k];
      const int nU = trajLevel(k).n;
      std::vector<int> unitParent(nU, -1);
      for (int i = 0; i < m_nLeaves; ++i)
        unitParent[comp[i]] = bestTop[i];
      activateLevelCopy(trajLevel(k));
      for (auto* cp : unitCorr)
        cp->setUnits(comp, nU);
      m_seededPhase = true;
      seedAssignment(unitParent);
      moveLoop();
      std::vector<int> remap(nU, -1);
      int c = 0;
      for (int u = 0; u < nU; ++u)
        if (remap[m_module[u]] == -1)
          remap[m_module[u]] = c++;
      std::vector<int> newTop(m_nLeaves);
      for (int i = 0; i < m_nLeaves; ++i)
        newTop[i] = remap[m_module[comp[i]]];
      double corrSum = m_lastCorrection;
      for (auto& cp : m_corrections)
        if (cp->participatesInMoveLoop() && !(m_moduleCorrActive && cp->participatesInModuleMoves()))
          corrSum += cp->initMoveLoop(newTop, c);
      const double L = m_codelength + corrSum;
      if (L < bestCodelength - kMinImprovement) {
        bestTop = std::move(newTop);
        bestCodelength = L;
        bestK = static_cast<unsigned int>(c);
      }
    }
    m_seededPhase = false;

    // Flow-community regroup probe (see columnar::regroupProbeEnabled): the
    // repair above can only regroup within the trajectory's own module ids,
    // so it cannot cross the group hysteresis of a module-move correction —
    // the correction's reward is superadditive in module size, so the true
    // optimum can need ~100 building blocks merged into one module while
    // every pairwise step (and hence every greedy trajectory) is uphill.
    // Both greedy end states — stalled fully fragmented, and snowballed into
    // one module — are fixpoints of every pairwise operator downstream
    // (state benchmark N256 om5/om6: 7.989 collapsed / 7.810 fragmented
    // against 6.902 for the planted 20-module partition). The group proposal
    // is visible in exactly one place: the flow structure of the block graph
    // itself, clustered as its OWN closed first-order network — base
    // objective, blind to the corrections, so the correction's pull cannot
    // snowball the probe either. Its grouping then seeds a module-level move
    // loop under the TRUE objective (same polish + scoring as the repair
    // above), kept only if it beats the aggregation's best. Skipped for
    // building-block bottoms (maxAggPasses != 0): those callers want
    // fineness, not the two-level optimum, and the up-build owns the
    // coarsening there. Runs in sub-optimizers too (an in-context refine with
    // sliced corrections): subdividing an over-merged module under the memory
    // objective is the same hysteresis one level down — a super-group holding
    // several communities is only split by rediscovering them inside it, which
    // is exactly the job splitTopModules delegates to the sub-cluster.
    // Fraction-of-root gate: the arm always runs at the top level, but inside a
    // sub-optimizer only when the sub-problem holds at least a tenth of the
    // root's leaves. The group hysteresis needs a macroscopic chunk of the
    // network (a super-group holding several communities), and the arm's cost
    // on small problems is not one ladder but the COUNT of them — the winner
    // repair's fresh splits probe every module's sub-cluster, and ~140 tiny
    // sub-detectors cost malaria `-C -N10` +5.2% in time for exactly 0 change
    // in bits. The gate is scale-free: om5's 12224-of-50133-leaf super-group
    // (24% of the root) qualifies, malaria's ~55-leaf modules (<1%) never do.
    // The ladder's best non-collapsed candidate, kept for the collapsed-winner
    // comparison after the fine-tune (below).
    const bool trackRunnerUp = doFineTune;
    if (regroupProbeEnabled() && maxAggPasses == 0 && m_nLeaves * 10 >= m_rootLeaves && !trajComp.empty()) {
      // The ladder is multi-scale on purpose: one probe pass finds the base
      // objective's OWN resolution on the base graph, which on a large sparse
      // one is an intermediate scale (om5: 11049 pass-1 blocks probe to 4412
      // groups, nowhere near the 20 planted communities). Re-aggregating by
      // the found grouping and probing again walks the community hierarchy of
      // the base graph rung by rung; every rung's grouping is offered to the
      // true objective independently, keep-best. Rung cost shrinks with the
      // unit count, so the first probe dominates. Returns whether any rung
      // improved the best partition.
      // Did the detector's accepted rung come FROM the leaf-granularity test?
      // That is the signal that this trial is in the basin the test exists for,
      // and it is what decides whether the escalated ladder pays for the test too.
      bool tuneWonInDetector = false;
      auto runLadder = [&](const Level& baseLvl, const std::vector<int>& comp0, bool allowLeafTune, bool isDetector) -> bool {
        bool anyAccepted = false;
        const int nBase = baseLvl.n;
        // Every rung's candidate is polished at BASE granularity: the seeded
        // move loop re-sorts individual base units between the rung's coarse
        // groups under the true objective, repairing the impurities a
        // whole-rung-unit move could never reach. lvl() stays the base level
        // for the whole ladder; only the probe sees the rung levels.
        activateLevelCopy(baseLvl);
        for (auto* cp : unitCorr)
          cp->setUnits(comp0, nBase);
        Level ladder; // rung >= 1 lives here; rung 0 reads the base level directly
        std::vector<int> compBase(nBase); // base unit -> current rung unit
        for (int b = 0; b < nBase; ++b)
          compBase[b] = b;
        // The rung polish — a seeded move loop over every base unit — is the
        // ladder's dominant cost, and the winning rung is empirically the
        // first or second (later, coarser rungs re-offer roughly the same
        // grouping with less resolution). Two consecutive rejections end it.
        int rejected = 0;
        for (int rung = 0; rung < 64 && rejected < 2; ++rung) {
          pollInterrupt();
          const Level& rungLvl = rung == 0 ? baseLvl : ladder;
          const int nU = rungLvl.n;
          // Probe under the enter-flow transform (the up-build's super-network
          // semantics: a unit's codeword usage is its index rate q — the rate at
          // which the walker enters it, teleport included). With the units' TRUE
          // flows the probe would just be the module-level move loop continued —
          // the Louvain equivalence — and stall on the very fixpoint it is trying
          // to escape; the boundary-flow view is what exposes the base graph's
          // community structure at each scale.
          Level probeNet = rungLvl;
          m_flow.setIndexRateAsFlow(probeNet);
          ColumnarTwoLevel probe;
          probe.setInterruptCallback(m_interruptCallback);
          probe.buildFromLevel(std::move(probeNet), m_flow, m_seed, 0.0);
          probe.optimizeTwoLevel(0, false);
          const int probeK = static_cast<int>(probe.numTopModules());
          if (probeK <= 1 || probeK >= nU)
            break;
          const std::vector<int> group = probe.leafTopModule(); // rung unit -> group
          {
            std::vector<int> gBase(nBase);
            for (int b = 0; b < nBase; ++b)
              gBase[b] = group[compBase[b]];
            m_seededPhase = true;
            m_noEmptyTargets = true;
            seedAssignment(gBase);
            moveLoop();
            m_noEmptyTargets = false;
            std::vector<int> remap(nBase, -1);
            int c = 0;
            for (int u = 0; u < nBase; ++u)
              if (remap[m_module[u]] == -1)
                remap[m_module[u]] = c++;
            std::vector<int> newTop(m_nLeaves);
            for (int i = 0; i < m_nLeaves; ++i)
              newTop[i] = remap[m_module[comp0[i]]];
            double corrSum = m_lastCorrection;
            for (auto& cp : m_corrections)
              if (cp->participatesInMoveLoop() && !(m_moduleCorrActive && cp->participatesInModuleMoves()))
                corrSum += cp->initMoveLoop(newTop, c);
            double L = m_codelength + corrSum;
            // The candidate was polished purify-only at BLOCK granularity while
            // the incumbent has already been tuned at leaf granularity, so a
            // straight comparison charges the candidate for a constraint the
            // incumbent never carried. Re-tune the candidate at leaf granularity
            // (empty targets on, one sweep) and score it there instead.
            //
            // Scope: first rung only, only when the cheap score does not already
            // win, and — in the escalated ladder — only when the detector's own
            // accepted rung came from this test (tuneWonInDetector). This is an escalation question,
            // not a ladder question — the detector exists to decide whether the
            // trial is in the pathological basin, and a candidate the block score
            // rejects is exactly where that decision is being got wrong. A trial
            // that escalated on the block score alone is one where the ladder is
            // working normally, so carrying the test into its full ladder is pure
            // cost — that is every `--regularized` overlapping row. Rung 0
            // is the one candidate built from the same units the incumbent is
            // made of; later rungs are strictly coarser re-offerings, and on
            // every row this fix improves, the rescue happens at rung 0.
            const unsigned int leafSweeps = regroupLeafTuneSweeps();
            bool tunedThisRung = false;
            if (leafSweeps != 0 && allowLeafTune && rung == 0 && L >= bestCodelength - kMinImprovement) {
              for (auto* cp : unitCorr)
                cp->resetUnitsToLeaves();
              activateLeafLevel();
              m_leafMoveLoop = true;
              seedAssignment(newTop);
              moveLoop(leafSweeps);
              const double leafL = m_codelength + m_lastCorrection;
              if (leafL < L) {
                std::vector<int> remapLeaf(m_nLeaves, -1);
                int cLeaf = 0;
                std::vector<int> leafTop(m_nLeaves);
                for (int i = 0; i < m_nLeaves; ++i) {
                  if (remapLeaf[m_module[i]] == -1)
                    remapLeaf[m_module[i]] = cLeaf++;
                  leafTop[i] = remapLeaf[m_module[i]];
                }
                newTop = std::move(leafTop);
                c = cLeaf;
                L = leafL;
                tunedThisRung = true;
              }
              m_leafMoveLoop = false;
              reactivateLevelCopy(baseLvl);
              for (auto* cp : unitCorr)
                cp->setUnits(comp0, nBase);
            }
            if (trackRunnerUp && L < runnerUpL && largestModuleFlowShare(newTop, c) < kCollapsedModuleFlowShare) {
              runnerUpTop = newTop;
              runnerUpK = static_cast<unsigned int>(c);
              runnerUpL = L;
            }
            if (L < bestCodelength - kMinImprovement) {
              bestTop = std::move(newTop);
              bestCodelength = L;
              bestK = static_cast<unsigned int>(c);
              anyAccepted = true;
              rejected = 0;
              if (tunedThisRung && isDetector)
                tuneWonInDetector = true;
            } else {
              ++rejected;
            }
            m_seededPhase = false;
          }
          // Next rung: the probe's grouping aggregated as its own network. Built
          // from the PROBE's assignment, not the gated candidate — the ladder
          // follows the base graph's community hierarchy regardless of which
          // rungs the true objective takes.
          Level next = m_flow.aggregate(rungLvl, group, probeK);
          for (int b = 0; b < nBase; ++b)
            compBase[b] = group[compBase[b]];
          ladder = std::move(next);
        }
        return anyAccepted;
      };

      // Escalation policy: probe the CONVERGED partition's module level first
      // (a few hundred units on a healthy network, so the no-op case costs one
      // aggregation and a tiny ladder instead of re-clustering thousands of
      // pass-1 blocks per trial). A SUBSTANTIAL accepted rung — 0.1% of the
      // codelength; the pathology's accepts are 2-9%, a healthy network's
      // ~0.01% — confirms the greedy fixpoint was not the objective's optimum,
      // and only then is the finest-granularity ladder worth its cost (the
      // converged units are too coarse to purify: staging the escape from the
      // pass-1 blocks is worth ~0.1% more on the om5/om6 family). An
      // aggregation that collapsed to fewer than 64 units has no usable
      // converged base and escalates directly.
      bool escalate = static_cast<int>(bestK) < 64;
      if (!escalate) {
        // DETECTOR ONLY: the cheap ladder's accepts are always rolled back.
        // Keeping them would let its marginal (~0.01%) wins and losses steer
        // healthy trials off the shipped trajectory for nothing; discarding
        // them makes a run whose detector stays quiet BIT-IDENTICAL to the
        // pre-probe engine (the ladder consumes no shared RNG state, and
        // everything downstream is re-derived from bestTop). A substantial
        // win is re-found — better — by the full ladder below.
        const std::vector<int> savedTop = bestTop;
        const unsigned int savedK = bestK;
        const double savedL = bestCodelength;
        Level convergedBase = m_flow.aggregate(leaf0(), bestTop, static_cast<int>(bestK));
        runLadder(convergedBase, savedTop, true, true);
        escalate = savedL - bestCodelength > 1e-3 * savedL;
        bestTop = savedTop;
        bestK = savedK;
        bestCodelength = savedL;
      }
      m_regroupEscalated = m_regroupEscalated || escalate;
      if (escalate)
        // The escalated ladder gets the leaf-granularity test unconditionally.
        // #1037 scoped it to trials whose detector rung came FROM that test
        // (tuneWonInDetector), which saved ~4% of instructions on a handful of
        // rows and cost -4.92% / -5.23% in bits on om2 / om4 `-2d --regularized`:
        // there the detector accepts a rung on its block score and only the
        // escalated ladder's leaf test can see the 4.9% partition underneath.
        // See regroupLeafTuneDetectorOnly for the A/B handle and the numbers.
        runLadder(trajLevel(0), trajComp[0], regroupLeafTuneDetectorOnly() ? tuneWonInDetector : true, false);
    }

    m_moduleCorrActive = false;
    for (auto* cp : unitCorr)
      cp->resetUnitsToLeaves();
  }

  m_leafTop = bestTop;
  m_numTopModules = bestK;

  if (!doFineTune)
    return bestCodelength;

  bestCodelength = fineTuneLeafTop(bestCodelength);

  // A collapsed winner -- one module holding effectively the whole network --
  // is compared with the ladder's best non-collapsed candidate only after BOTH
  // are leaf-tuned (#1042). The rungs are judged at the granularity of the
  // pass-1 blocks, and those blocks can be impure: under recorded teleportation
  // the teleport term rewards merging states across communities from the very
  // first sweep. A structured candidate pays for every impure block until its
  // leaves are re-sorted; the collapsed one, holding them all in one module,
  // pays nothing. om5_N256_E100k `-2d --regularized -N1`: the 70-module rung
  // scores 8.113 against the collapse's 7.993 bits at block granularity, but
  // 7.842 against 7.993 once tuned, and the run then ends at 7.825 (planted
  // 7.826) instead of 7.974. The test runs only after a collapse, so a run
  // whose winner keeps its structure pays nothing; and only for a candidate
  // within kRunnerUpTuneGap of the tuned winner, the most a leaf tune was ever
  // seen to recover.
  if (!runnerUpTop.empty() && runnerUpL <= bestCodelength * (1.0 + kRunnerUpTuneGap)
      && largestModuleFlowShare(m_leafTop, static_cast<int>(m_numTopModules)) >= kCollapsedModuleFlowShare) {
    std::swap(m_leafTop, runnerUpTop);
    std::swap(m_numTopModules, runnerUpK);
    const double runnerUpTuned = fineTuneLeafTop(runnerUpL);
    if (runnerUpTuned < bestCodelength - kMinImprovement) {
      bestCodelength = runnerUpTuned;
    } else {
      std::swap(m_leafTop, runnerUpTop);
      std::swap(m_numTopModules, runnerUpK);
    }
  }
  return bestCodelength;
}

double ColumnarTwoLevel::fineTuneLeafTop(double bestCodelength)
{
  // Fine-tune to convergence (tune level 0): re-optimize leaves across the
  // current top modules, seeded at the current assignment. Leaves may move
  // between modules and modules may merge/empty. Same primitive that will tune
  // interior levels; here it closes the gap the OO fine/coarse tune closes.
  while (true) {
    pollInterrupt();
    activateLeafLevel();
    m_leafMoveLoop = true; // fine-tune re-optimizes leaves
    m_seededPhase = true; // seeded at the current partition
    seedAssignment(m_leafTop);
    moveLoop();
    // Fine-tune re-optimizes leaves, so m_lastCorrection is exactly this
    // partition's correction; select by the augmented objective.
    const double augL = m_codelength + m_lastCorrection;
    if (augL >= bestCodelength - kMinImprovement)
      break;
    std::vector<int> remap(lvl().n, -1);
    int c = 0;
    for (int i = 0; i < lvl().n; ++i)
      if (remap[m_module[i]] == -1)
        remap[m_module[i]] = c++;
    for (int i = 0; i < m_nLeaves; ++i)
      m_leafTop[i] = remap[m_module[i]];
    m_numTopModules = static_cast<unsigned int>(c);
    bestCodelength = augL;
  }
  return bestCodelength;
}

double ColumnarTwoLevel::largestModuleFlowShare(const std::vector<int>& leafModule, int numModules) const
{
  std::vector<double> flow(static_cast<std::size_t>(numModules), 0.0);
  double total = 0.0;
  for (int i = 0; i < m_nLeaves; ++i) {
    flow[static_cast<std::size_t>(leafModule[i])] += leaf0().flow[i];
    total += leaf0().flow[i];
  }
  double largest = 0.0;
  for (double f : flow)
    largest = std::max(largest, f);
  return total > 0.0 ? largest / total : 0.0;
}

bool ColumnarTwoLevel::adoptPass1State(ColumnarTwoLevel& from)
{
  if (!from.m_pass1State || from.m_seed != m_seed || from.m_nLeaves != m_nLeaves)
    return false;
  m_adoptedPass1 = std::move(from.m_pass1State);
  return true;
}

} // namespace infomap
