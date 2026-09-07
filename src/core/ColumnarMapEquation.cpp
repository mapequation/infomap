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

void ColumnarTwoLevel::buildFromLeaves(const std::vector<InfoNode*>& leafNodes, bool undirected, unsigned long seed)
{
  using infomath::plogp;
  m_undirected = undirected;
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
  L.linkEnter.resize(m_nLeaves);
  L.linkExit.resize(m_nLeaves);
  L.teleFlow.resize(m_nLeaves);
  L.teleWeight.resize(m_nLeaves);
  m_leafFlow.resize(m_nLeaves);
  m_leafTeleFlow.resize(m_nLeaves);
  m_leafTeleWeight.resize(m_nLeaves);
  m_totalTeleFlow = 0.0;
  m_leafNodeFlow_log_nodeFlow = 0.0;
  for (int i = 0; i < m_nLeaves; ++i) {
    L.flow[i] = leafNodes[i]->data.flow;
    L.linkEnter[i] = leafNodes[i]->data.enterFlow;
    L.linkExit[i] = leafNodes[i]->data.exitFlow;
    m_leafFlow[i] = L.flow[i];
    // Recorded-teleportation bookkeeping (used only when setRecordedTeleportation
    // is on); harmless to populate otherwise.
    L.teleFlow[i] = leafNodes[i]->data.teleportFlow;
    L.teleWeight[i] = leafNodes[i]->data.teleportWeight;
    m_leafTeleFlow[i] = L.teleFlow[i];
    m_leafTeleWeight[i] = L.teleWeight[i];
    m_totalTeleFlow += m_leafTeleFlow[i];
    m_leafNodeFlow_log_nodeFlow += plogp(L.flow[i]);
  }

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

void ColumnarTwoLevel::buildFromLevel(Level level, bool undirected, unsigned long seed, double exitNetworkFlow, bool recordedTeleport, double globalTotalTeleFlow)
{
  m_leaf0Owned = std::move(level);
  initLeafContext(&m_leaf0Owned, undirected, seed, exitNetworkFlow, recordedTeleport, globalTotalTeleFlow);
}

void ColumnarTwoLevel::buildFromBorrowedLevel(const Level& level, bool undirected, unsigned long seed, double exitNetworkFlow, bool recordedTeleport, double globalTotalTeleFlow)
{
  // No copy: the caller owns `level` and keeps it alive and unchanged (see the
  // header contract). Everything below reads the leaf network through leaf0().
  initLeafContext(&level, undirected, seed, exitNetworkFlow, recordedTeleport, globalTotalTeleFlow);
}

void ColumnarTwoLevel::initLeafContext(const Level* leaf, bool undirected, unsigned long seed, double exitNetworkFlow, bool recordedTeleport, double globalTotalTeleFlow)
{
  using infomath::plogp;
  m_undirected = undirected;
  m_seed = seed;
  m_exitNetworkFlow = exitNetworkFlow;
  m_leaf0Ptr = leaf;
  // Until a search activates a level explicitly, the active level is the leaf
  // network — never a null pointer.
  m_lvlPtr = leaf;
  m_nLeaves = leaf->n;
  m_rootLeaves = m_nLeaves; // sub-optimizers get the true root's count after build
  m_leafFlow = leaf->flow;
  // Inherit the GLOBAL teleport context (the level already carries per-unit
  // teleFlow/teleWeight; the total stays the whole network's, not this level's).
  m_recordedTeleport = recordedTeleport;
  m_totalTeleFlow = globalTotalTeleFlow;
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

  // m_mEnter/m_mExit stay link-based; the codelength terms below use the
  // effective (teleport-inclusive) enter/exit when recorded teleportation is on.
  if (m_recordedTeleport) {
    m_mTeleFlow.assign(n, 0.0);
    m_mTeleWeight.assign(n, 0.0);
  }

  m_enterFlow = 0.0;
  m_enter_log_enter = 0.0;
  m_exit_log_exit = 0.0;
  m_flow_log_flow = 0.0;
  for (int i = 0; i < n; ++i) {
    m_module[i] = i;
    m_mFlow[i] = lvl().flow[i];
    m_mEnter[i] = lvl().linkEnter[i];
    m_mExit[i] = lvl().linkExit[i];
    double enter = lvl().linkEnter[i];
    double exit = lvl().linkExit[i];
    if (m_recordedTeleport) {
      m_mTeleFlow[i] = lvl().teleFlow[i];
      m_mTeleWeight[i] = lvl().teleWeight[i];
      enter += moduleTeleEnter(lvl().teleFlow[i], lvl().teleWeight[i]);
      exit += moduleTeleExit(lvl().teleFlow[i], lvl().teleWeight[i]);
    }
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
  const double enter = m_mEnter[m] + (m_recordedTeleport ? moduleTeleEnter(m_mTeleFlow[m], m_mTeleWeight[m]) : 0.0);
  const double exit = m_mExit[m] + (m_recordedTeleport ? moduleTeleExit(m_mTeleFlow[m], m_mTeleWeight[m]) : 0.0);
  m_enterFlow -= enter;
  m_enter_log_enter -= plogp(enter);
  m_exit_log_exit -= plogp(exit);
  m_flow_log_flow -= plogp(exit + m_mFlow[m]);
}

void ColumnarTwoLevel::addModuleTerms(int m)
{
  using infomath::plogp;
  const double enter = m_mEnter[m] + (m_recordedTeleport ? moduleTeleEnter(m_mTeleFlow[m], m_mTeleWeight[m]) : 0.0);
  const double exit = m_mExit[m] + (m_recordedTeleport ? moduleTeleExit(m_mTeleFlow[m], m_mTeleWeight[m]) : 0.0);
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
  const double deltaOld = dEnterOld + dExitOld;
  const double deltaNew = dEnterNew + dExitNew;
  const double curEnter = lvl().linkEnter[u];
  const double curExit = lvl().linkExit[u];
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

  if (m_recordedTeleport) {
    const double tfu = lvl().teleFlow[u], twu = lvl().teleWeight[u];
    m_mTeleFlow[cMod] -= tfu;
    m_mTeleWeight[cMod] -= twu;
    m_mTeleFlow[newMod] += tfu;
    m_mTeleWeight[newMod] += twu;
  }

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
    const double enter = m_mEnter[m] + (m_recordedTeleport ? moduleTeleEnter(m_mTeleFlow[m], m_mTeleWeight[m]) : 0.0);
    const double exit = m_mExit[m] + (m_recordedTeleport ? moduleTeleExit(m_mTeleFlow[m], m_mTeleWeight[m]) : 0.0);
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

double ColumnarTwoLevel::deltaCodelengthMovingNodeTele(double curEnter, double curExit, double curFlow, double tfu, double twu, int A, int B, double deltaOld, double deltaNew) const
{
  using infomath::plogp;
  // Effective (link + teleport) enter/exit of A and B before the move.
  const double eA0 = m_mEnter[A] + moduleTeleEnter(m_mTeleFlow[A], m_mTeleWeight[A]);
  const double xA0 = m_mExit[A] + moduleTeleExit(m_mTeleFlow[A], m_mTeleWeight[A]);
  const double eB0 = m_mEnter[B] + moduleTeleEnter(m_mTeleFlow[B], m_mTeleWeight[B]);
  const double xB0 = m_mExit[B] + moduleTeleExit(m_mTeleFlow[B], m_mTeleWeight[B]);
  const double fA0 = xA0 + m_mFlow[A];
  const double fB0 = xB0 + m_mFlow[B];

  // After: u leaves A (link enter/exit lose curEnter/curExit, gain the crossing
  // deltaOld now on the boundary) and joins B; teleport flow/weight move with it.
  const double eA1 = (m_mEnter[A] - curEnter + deltaOld) + moduleTeleEnter(m_mTeleFlow[A] - tfu, m_mTeleWeight[A] - twu);
  const double xA1 = (m_mExit[A] - curExit + deltaOld) + moduleTeleExit(m_mTeleFlow[A] - tfu, m_mTeleWeight[A] - twu);
  const double eB1 = (m_mEnter[B] + curEnter - deltaNew) + moduleTeleEnter(m_mTeleFlow[B] + tfu, m_mTeleWeight[B] + twu);
  const double xB1 = (m_mExit[B] + curExit - deltaNew) + moduleTeleExit(m_mTeleFlow[B] + tfu, m_mTeleWeight[B] + twu);
  const double fA1 = xA1 + (m_mFlow[A] - curFlow);
  const double fB1 = xB1 + (m_mFlow[B] + curFlow);

  const double enterFlow1 = m_enterFlow + (eA1 - eA0) + (eB1 - eB0);

  const double d_enter = plogp(enterFlow1) - m_enterFlow_log_enterFlow;
  const double d_enter_log = (plogp(eA1) + plogp(eB1)) - (plogp(eA0) + plogp(eB0));
  const double d_exit_log = (plogp(xA1) + plogp(xB1)) - (plogp(xA0) + plogp(xB0));
  const double d_flow_log = (plogp(fA1) + plogp(fB1)) - (plogp(fA0) + plogp(fB0));
  return d_enter - d_enter_log - d_exit_log + d_flow_log;
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

unsigned int ColumnarTwoLevel::moveLoop(unsigned int sweepCap)
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

      const double deltaOld = dEnter[cMod] + dExit[cMod];
      const double curEnter = lvl().linkEnter[u];
      const double curExit = lvl().linkExit[u];
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

      // Hoist the old-module (A-side) terms once per unit visit — 6 of the 13
      // plogp per candidate on the base path, 6 of 12 on the recorded-teleport
      // path. Per-candidate math below is unchanged, so results stay bit-exact.
      const bool tele = m_recordedTeleport;
      const double tfu = tele ? lvl().teleFlow[u] : 0.0;
      const double twu = tele ? lvl().teleWeight[u] : 0.0;
      const OldSideTerms oldSide = tele
          ? OldSideTerms {}
          : hoistOldSide(curEnter, curExit, curFlow, m_mEnter[cMod], m_mExit[cMod], m_mFlow[cMod], deltaOld);
      const TeleOldSideTerms teleOldSide = tele
          ? hoistOldSideTele(curEnter, curExit, curFlow, tfu, twu, m_mEnter[cMod], m_mExit[cMod], m_mFlow[cMod], m_mTeleFlow[cMod], m_mTeleWeight[cMod], m_totalTeleFlow, deltaOld)
          : TeleOldSideTerms {};
      for (int m : touched) {
        if (m == cMod)
          continue;
        const double deltaNew = dEnter[m] + dExit[m];
        double dl = tele
            ? deltaCodelengthMovingNodeTeleHoisted(m_enterFlow, m_enterFlow_log_enterFlow, curEnter, curExit, curFlow, tfu, twu, teleOldSide, m_mEnter[m], m_mExit[m], m_mFlow[m], m_mTeleFlow[m], m_mTeleWeight[m], m_totalTeleFlow, deltaNew)
            : deltaCodelengthMovingNodeHoisted(
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
        const double deltaNew = dEnter[bestMod] + dExit[bestMod];
        // Update empty-module bookkeeping.
        if (m_mMembers[bestMod] == 0 && !m_emptyModules.empty())
          m_emptyModules.pop_back();
        if (m_mMembers[cMod] == 1)
          m_emptyModules.push_back(cMod);

        // Update running terms + module aggregates (port of updateCodelength...).
        // removeModuleTerms/addModuleTerms use the effective (teleport-inclusive)
        // enter/exit, so the running codelength stays exact under recorded
        // teleportation; with it off they reduce to the link enter/exit.
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

        if (m_recordedTeleport) {
          // tfu/twu were hoisted at the top of this unit visit.
          m_mTeleFlow[cMod] -= tfu;
          m_mTeleWeight[cMod] -= twu;
          m_mTeleFlow[bestMod] += tfu;
          m_mTeleWeight[bestMod] += twu;
        }

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
  next.linkEnter.assign(K, 0.0);
  next.linkExit.assign(K, 0.0);
  next.teleFlow.assign(K, 0.0);
  next.teleWeight.assign(K, 0.0);
  // Module aggregates already hold the consolidated flow data. enter/exit stay
  // link-based (the teleport term is rebuilt from teleFlow/teleWeight by the
  // next level's initPartition), matching how m_leaf0 stores link enter/exit.
  // m_mTeleFlow/m_mTeleWeight are only populated under recorded teleportation.
  for (int oldM = 0; oldM < n; ++oldM) {
    if (m_mMembers[oldM] == 0)
      continue;
    const int m = remap[oldM];
    next.flow[m] = m_mFlow[oldM];
    next.linkEnter[m] = m_mEnter[oldM];
    next.linkExit[m] = m_mExit[oldM];
    if (m_recordedTeleport) {
      next.teleFlow[m] = m_mTeleFlow[oldM];
      next.teleWeight[m] = m_mTeleWeight[oldM];
    }
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
    bestCodelength = runPass(newTop, c, pass1Seed);
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
      bestTop = std::move(newTop);
      bestCodelength = L;
      bestK = static_cast<unsigned int>(c);
      if (c <= 1 || c == lvl().n)
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
          setIndexRateAsFlow(probeNet);
          ColumnarTwoLevel probe;
          probe.setInterruptCallback(m_interruptCallback);
          probe.buildFromLevel(std::move(probeNet), m_undirected, m_seed, 0.0, m_recordedTeleport, m_totalTeleFlow);
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
          Level next = aggregateLevel(rungLvl, group, probeK, m_undirected);
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
        Level convergedBase = aggregateLevel(leaf0(), bestTop, static_cast<int>(bestK), m_undirected);
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

double ColumnarTwoLevel::optimizeTwoLevelStack()
{
  optimizeTwoLevel();

  // Materialize the partition as a two-level stack so the stack-based helpers
  // (codelength, coarsening, toNodePaths) apply.
  m_hierLevels.clear();
  m_hierAssign.clear();
  m_hierLevels.emplace_back(); // level 0 is the leaf network; see hierLevel()
  m_hierAssign.push_back(m_leafTop);
  m_hierLevels.push_back(aggregateLevel(leaf0(), m_leafTop, static_cast<int>(m_numTopModules), m_undirected));

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

double ColumnarTwoLevel::completeFlatFromAggregation(std::vector<int> aggTop, int aggK)
{
  // Complete the flat two-level pipeline from the probe's converged
  // aggregation partition (optimizeTwoLevel(0, false)): materialize the
  // stack, run the deferred leaf fine-tune to convergence, then the same
  // merge <-> retune interleave as optimizeTwoLevelStack. This is the
  // expensive leaf-level half the probe skipped; a flat-first trial pays it
  // only when the probe says the flat basin is competitive.
  m_leafTop = std::move(aggTop);
  m_numTopModules = static_cast<unsigned int>(aggK);
  m_hierLevels.clear();
  m_hierAssign.clear();
  m_hierLevels.emplace_back(); // level 0 is the leaf network; see hierLevel()
  m_hierAssign.push_back(m_leafTop);
  m_hierLevels.push_back(aggregateLevel(leaf0(), m_leafTop, aggK, m_undirected));

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

double ColumnarTwoLevel::deepRepairTwoLevelStack()
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
  double L = hierarchicalCodelengthFromStack();
  double before = L;
  bool allowSingletons = true;
  int split = splitTopModules(L, allowSingletons);
  allowSingletons = split != 0;
  for (int round = 0; round < 100; ++round) {
    const bool tuned = retuneLeavesWithinModules(L);
    before = L;
    coarsenModules(L, 1000);
    if (tuned || L < before - kMinImprovement)
      allowSingletons = true;
    split = splitTopModules(L, allowSingletons);
    allowSingletons = split != 0;
    if (!tuned && split == 0 && L >= before - kMinImprovement)
      break;
  }
  return L;
}

int ColumnarTwoLevel::splitTopModules(double& L, bool allowSingletons)
{
  // Merge overshoot only exists where module-move-capable corrections drive
  // the aggregation (the base merge is a no-op), and the recombination loop
  // needs their module-level deltas — skip entirely otherwise, keeping base
  // networks bit-exact at zero cost.
  std::vector<ColumnarCorrection*> unitCorr;
  for (auto& cp : m_corrections)
    if (cp->participatesInMoveLoop() && cp->participatesInModuleMoves())
      unitCorr.push_back(cp.get());
  // ...except on an externally seeded partition, whose modules the engine did not
  // build and can need subdividing under any objective (m_externalSeed, #824).
  if (unitCorr.empty() && !m_externalSeed)
    return 0;

  const int K = hierLevel(1).n;

  // Recombination: a seeded module-level move loop over the pieces — the
  // module-scale analog of the leaf fine-tune. Corrections act on aggregated
  // units through their per-unit aggregates, so a piece peels off its module
  // only when the true augmented objective says so. Gated on the true stack
  // codelength (revert if not improving).
  auto recombine = [&](const std::vector<int>& leafToPiece, const std::vector<int>& pieceParent) -> bool {
    const int nPieces = static_cast<int>(pieceParent.size());
    if (nPieces == K)
      return false; // every module is a single piece: nothing to split
    activateOwnedLevel(aggregateLevel(leaf0(), leafToPiece, nPieces, m_undirected));
    m_leafMoveLoop = false;
    m_moduleCorrActive = true;
    m_seededPhase = true;
    for (auto* cp : unitCorr)
      cp->setUnits(leafToPiece, nPieces);
    seedAssignment(pieceParent);
    moveLoop();
    m_moduleCorrActive = false;
    for (auto* cp : unitCorr)
      cp->resetUnitsToLeaves();

    // Fast reject: no piece ended up outside its seeded module — the stack is
    // untouched, so no rebuild or evaluation is needed. This is the common
    // case in the convergence tail, where each round pays only the piece
    // aggregation and one settled move-loop sweep.
    bool anyMoved = false;
    for (int p = 0; p < nPieces && !anyMoved; ++p)
      anyMoved = m_module[p] != pieceParent[p];
    if (!anyMoved)
      return false;

    std::vector<int> savedTop = m_hierAssign[0];
    const int savedK = K;
    std::vector<int> remap(lvl().n, -1);
    int k = 0;
    for (int p = 0; p < nPieces; ++p)
      if (remap[m_module[p]] == -1)
        remap[m_module[p]] = k++;
    std::vector<int> newTop(m_nLeaves);
    for (int i = 0; i < m_nLeaves; ++i)
      newTop[i] = remap[m_module[leafToPiece[i]]];

    m_hierAssign[0] = std::move(newTop);
    m_hierLevels[1] = aggregateLevel(leaf0(), m_hierAssign[0], k, m_undirected);
    m_numTopModules = static_cast<unsigned int>(k);
    const double splitL = hierarchicalCodelengthFromStack();
    if (splitL < L - kMinImprovement) {
      L = splitL;
      return true;
    }
    m_hierAssign[0] = std::move(savedTop);
    m_hierLevels[1] = aggregateLevel(leaf0(), m_hierAssign[0], savedK, m_undirected);
    m_numTopModules = static_cast<unsigned int>(savedK);
    return false;
  };

  // Intersect a global fine labeling with the current modules: pieces =
  // (label ∩ module), well-defined even after leaf tuning moved individual
  // leaves across module boundaries.
  auto intersectPieces = [&](const std::vector<int>& labels, std::vector<int>& leafToPiece, std::vector<int>& pieceParent) {
    std::unordered_map<long long, int> pieceId;
    pieceId.reserve(static_cast<std::size_t>(K) * 2);
    leafToPiece.resize(m_nLeaves);
    pieceParent.clear();
    for (int i = 0; i < m_nLeaves; ++i) {
      const int mod = m_hierAssign[0][i];
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
  if (!m_leafBlocks.empty()) {
    intersectPieces(m_leafBlocks, leafToPieceScratch, pieceParentScratch);
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
  if (!m_freshDiscovery || !allowSingletons || (!m_lastSinglesPieces.empty() && !m_freshSinglesProductive))
    return 0;

  // Piece source 2 (decisive): from-singletons sub-clustering within each
  // module — community-granularity pieces, so extracting a whole community
  // from an over-merged module is a single gated move (block-granularity
  // pieces face the same uphill barrier as single leaves when a split's
  // intermediate states are worse).
  std::vector<std::vector<int>> leavesPer(K);
  for (int i = 0; i < m_nLeaves; ++i)
    leavesPer[m_hierAssign[0][i]].push_back(i);
  std::vector<int> leafToPiece(m_nLeaves, -1);
  std::vector<int> pieceParent;
  std::vector<int> loc(m_nLeaves, -1);
  std::vector<int> localAssign;
  for (int P = 0; P < K; ++P) {
    const std::vector<int>& S = leavesPer[P];
    if (S.empty())
      continue;
    if (S.size() == 1) {
      leafToPiece[S[0]] = static_cast<int>(pieceParent.size());
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
      // the interleaved leaf re-tune do the polishing.
      Ksub = subClusterLeaves(S, hierLevel(1).linkExit[P], loc, localAssign, false);
      m_subClusterCache.emplace(S, std::make_pair(Ksub, localAssign));
    }
    const int base = static_cast<int>(pieceParent.size());
    for (std::size_t j = 0; j < S.size(); ++j)
      leafToPiece[S[j]] = base + localAssign[j];
    for (int s = 0; s < Ksub; ++s)
      pieceParent.push_back(P);
  }
  m_lastSinglesPieces = leafToPiece;
  const bool singlesImproved = recombine(leafToPiece, pieceParent);
  m_freshSinglesProductive = singlesImproved;
  return singlesImproved ? 2 : 0;
}

bool ColumnarTwoLevel::hierarchicalWinnerRepairEnabled()
{
  return hSplitWinnerMode() != kHSplitOff;
}

double ColumnarTwoLevel::deepRepairHierarchicalStack()
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
  m_forceHSplit = true;
  double L = hierarchicalCodelengthFromStack();
  coarsenModules(L, 1000);
  m_forceHSplit = false;
  return L;
}

int ColumnarTwoLevel::splitLevelModules(int k, double& L, bool allowSingletons)
{
  const int top = static_cast<int>(m_hierLevels.size()) - 1;
  if (k < 0 || k + 1 > top)
    return 0; // level k+1 must be a module level of the stack
  const int nU = hierLevel(k).n;
  const int K = hierLevel(k + 1).n;
  if (nU <= 1 || K >= nU)
    return 0; // every module already holds a single child: nothing to subdivide

  const std::clock_t tStart = std::clock();
  struct ClockGuard {
    std::clock_t t0;
    int level;
    ~ClockGuard() { g_hSplitClocks[level].fetch_add(static_cast<long long>(std::clock() - t0), std::memory_order_relaxed); }
  } clockGuard { tStart, std::min(k, kHSplitMaxLevels - 1) };

  const bool interior = k > 0;

  // Module-move-capable corrections score the LEAF module partition, so they
  // only participate when the level being re-sorted is the leaf level: a move
  // at an interior level leaves every leaf's module unchanged, hence the
  // correction unchanged. (Unlike splitTopModules, the presence of such a
  // correction is NOT a precondition — the hierarchical over-merge comes from
  // the enter-flow up-build, which runs on base networks too.)
  std::vector<ColumnarCorrection*> unitCorr;
  if (!interior)
    for (auto& cp : m_corrections)
      if (cp->participatesInMoveLoop() && cp->participatesInModuleMoves())
        unitCorr.push_back(cp.get());

  // The level-k network in the space the level-(k+1) codebook actually uses:
  // leaves are coded by their flow, interior modules by their index rate q
  // (setIndexRateAsFlow, the up-build's transform). Only the move loop sees the
  // transform; the stack levels keep the true flows.
  const bool phaseTiming = hSplitPhaseTiming();
  Level moveBaseCopy;
  {
    PhaseTimer pt(0, phaseTiming);
    moveBaseCopy = hierLevel(k);
    if (interior)
      setIndexRateAsFlow(moveBaseCopy);
  }
  const Level& moveBase = moveBaseCopy;

  if (static_cast<int>(m_lastLevelPieces.size()) <= k) {
    m_lastLevelPieces.resize(k + 1);
    m_levelFreshProductive.resize(k + 1, 1);
  }

  // Recombination: a seeded move loop over the pieces of the level-k network,
  // seeded at the current level-(k+1) modules. Pieces may move to any module,
  // including an empty one — group split and cross-parent relocation in one
  // operator. Gated on the true stack codelength (revert if not improving).
  auto recombine = [&](const std::vector<int>& unitToPiece, const std::vector<int>& pieceParent) -> bool {
    const int nPieces = static_cast<int>(pieceParent.size());
    if (nPieces == K)
      return false; // every module is a single piece: nothing to split
    g_hSplitAttempts[std::min(k, kHSplitMaxLevels - 1)].fetch_add(1, std::memory_order_relaxed);
    {
      PhaseTimer pt(1, phaseTiming);
      activateOwnedLevel(aggregateLevel(moveBase, unitToPiece, nPieces, m_undirected));
    }
    m_leafMoveLoop = false;
    m_moduleCorrActive = !unitCorr.empty();
    m_seededPhase = true;
    {
      PhaseTimer pt(2, phaseTiming);
      for (auto* cp : unitCorr)
        cp->setUnits(unitToPiece, nPieces);
      seedAssignment(pieceParent);
      moveLoop();
    }
    m_moduleCorrActive = false;
    for (auto* cp : unitCorr)
      cp->resetUnitsToLeaves();

    // Fast reject: no piece ended up outside its seeded module — the stack is
    // untouched, so no rebuild or evaluation is needed.
    bool anyMoved = false;
    for (int p = 0; p < nPieces && !anyMoved; ++p)
      anyMoved = m_module[p] != pieceParent[p];
    if (!anyMoved)
      return false;

    // Save only what the rebuild touches: the level-k assignment, the
    // module->grandparent map, and every level above k (all module levels, so
    // cheap — the leaf level is never copied).
    std::vector<int> savedAK;
    std::vector<int> savedAK1;
    std::vector<Level> savedUpper;
    {
      PhaseTimer pt(5, phaseTiming);
      savedAK = m_hierAssign[k];
      if (k + 1 < top)
        savedAK1 = m_hierAssign[k + 1];
      savedUpper.assign(m_hierLevels.begin() + (k + 1), m_hierLevels.end());
    }
    const unsigned int savedNumTop = m_numTopModules;

    {
      PhaseTimer ptRebuild(3, phaseTiming);
      std::vector<int> remap(lvl().n, -1);
      int nk = 0;
      for (int p = 0; p < nPieces; ++p)
        if (remap[m_module[p]] == -1)
          remap[m_module[p]] = nk++;
      std::vector<int> newAK(nU);
      for (int u = 0; u < nU; ++u)
        newAK[u] = remap[m_module[unitToPiece[u]]];
      m_hierAssign[k] = std::move(newAK);
      m_hierLevels[k + 1] = aggregateLevel(hierLevel(k), m_hierAssign[k], nk, m_undirected);

      if (k + 1 < top) {
        // A new module inherits the grandparent of the module its pieces
        // predominantly came from (by piece flow) — a group split stays in its
        // old grandparent, a relocation follows the module it joined. Ties break
        // on the lower grandparent id, so the map is order-independent.
        std::vector<std::unordered_map<int, double>> votes(nk);
        for (int p = 0; p < nPieces; ++p)
          votes[remap[m_module[p]]][savedAK1[pieceParent[p]]] += lvl().flow[p];
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
        m_hierAssign[k + 1] = std::move(newAK1);
        // Re-aggregate every level above: a cross-grandparent relocation changes
        // their aggregates. The unit counts stay put (a grandparent that lost all
        // its children survives as an empty module, which costs nothing in the
        // codelength and keeps the higher assignments valid).
        for (int j = k + 1; j < top; ++j)
          m_hierLevels[j + 1] = aggregateLevel(hierLevel(j), m_hierAssign[j], hierLevel(j + 1).n, m_undirected);
      }
      m_numTopModules = static_cast<unsigned int>(m_hierLevels.back().n);
    }

    double splitL;
    {
      PhaseTimer pt(4, phaseTiming);
      splitL = hierarchicalCodelengthFromStack();
    }
    if (splitL < L - kMinImprovement) {
      const double relGain = L > 0.0 ? (L - splitL) / L : 0.0;
      g_hSplitGainNano[std::min(k, kHSplitMaxLevels - 1)].fetch_add(static_cast<long long>(relGain * 1e9), std::memory_order_relaxed);
      L = splitL;
      g_hSplitAccepts[std::min(k, kHSplitMaxLevels - 1)].fetch_add(1, std::memory_order_relaxed);
      return true;
    }
    {
      PhaseTimer pt(5, phaseTiming);
      m_hierAssign[k] = std::move(savedAK);
      if (k + 1 < top)
        m_hierAssign[k + 1] = std::move(savedAK1);
      for (int j = k + 1; j <= top; ++j)
        m_hierLevels[j] = std::move(savedUpper[j - (k + 1)]);
    }
    m_numTopModules = savedNumTop;
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
      const int mod = m_hierAssign[k][u];
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
  if (!interior && static_cast<int>(m_leafBlocks.size()) == nU) {
    intersectPieces(m_leafBlocks, scratchPieces, scratchParent);
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
      childrenPer[m_hierAssign[k][u]].push_back(u);
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
        Ksub = subClusterUnits(hierLevel(k), true, false, S, hierLevel(k + 1).linkExit[P], loc, localAssign, false, nullptr);
      } else {
        // A module's sub-clustering depends only on its own leaf set, so
        // unchanged modules reuse earlier rounds' result (memo shared with
        // splitTopModules; only the touched modules are re-clustered).
        auto it = m_subClusterCache.find(S);
        if (it != m_subClusterCache.end()) {
          Ksub = it->second.first;
          localAssign = it->second.second;
        } else {
          Ksub = subClusterLeaves(S, hierLevel(1).linkExit[P], loc, localAssign, false);
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

bool ColumnarTwoLevel::retuneLeavesWithinModules(double& L)
{
  // Seeded leaf fine-tune across the current module level: re-run the leaf
  // move loop (corrections active, co-physical proposals enabled by the
  // seeded phase) from the current assignment, rebuild the module level, and
  // keep the result only if it lowers the true stack codelength.
  std::vector<int> savedTop = m_hierAssign[0];
  const int savedK = hierLevel(1).n;

  activateLeafLevel();
  m_leafMoveLoop = true;
  m_seededPhase = true;
  seedAssignment(m_hierAssign[0]);
  moveLoop();

  std::vector<int> remap(lvl().n, -1);
  int k = 0;
  for (int i = 0; i < lvl().n; ++i)
    if (remap[m_module[i]] == -1)
      remap[m_module[i]] = k++;
  std::vector<int> newTop(m_nLeaves);
  for (int i = 0; i < m_nLeaves; ++i)
    newTop[i] = remap[m_module[i]];

  m_hierAssign[0] = std::move(newTop);
  m_hierLevels[1] = aggregateLevel(leaf0(), m_hierAssign[0], k, m_undirected);
  m_numTopModules = static_cast<unsigned int>(k);
  const double tunedL = hierarchicalCodelengthFromStack();
  if (tunedL < L - kMinImprovement) {
    L = tunedL;
    return true;
  }
  m_hierAssign[0] = std::move(savedTop);
  m_hierLevels[1] = aggregateLevel(leaf0(), m_hierAssign[0], savedK, m_undirected);
  m_numTopModules = static_cast<unsigned int>(savedK);
  return false;
}

bool ColumnarTwoLevel::seedHierarchyFromLeafPaths(const std::vector<std::vector<int>>& leafPaths)
{
  if (static_cast<int>(leafPaths.size()) != m_nLeaves)
    return false;

  // Require a rectangular tree (every leaf the same number of module levels).
  // Ragged trees don't map onto the strict-level stack; the caller falls back.
  int depth = -1;
  for (const auto& p : leafPaths) {
    const int d = static_cast<int>(p.size());
    if (d < 1)
      return false;
    if (depth == -1)
      depth = d;
    else if (d != depth)
      return false;
  }

  // Compact a module id per leaf at each stack level. Stack level j (1 = finest
  // .. depth = top) groups leaves that share the path prefix path[0 .. depth-j]:
  // the finest module needs the whole path to match, the top module only path[0].
  std::vector<std::vector<int>> levelId(depth + 1); // levelId[j][leaf], j = 1..depth
  std::vector<int> levelK(depth + 1, 0);
  for (int j = 1; j <= depth; ++j) {
    const int prefixLen = depth - j + 1;
    std::map<std::vector<int>, int> ids;
    levelId[j].resize(m_nLeaves);
    for (int i = 0; i < m_nLeaves; ++i) {
      std::vector<int> key(leafPaths[i].begin(), leafPaths[i].begin() + prefixLen);
      auto res = ids.emplace(std::move(key), static_cast<int>(ids.size()));
      levelId[j][i] = res.first->second;
    }
    levelK[j] = static_cast<int>(ids.size());
  }

  // Build the stack bottom-up via aggregateLevel: level 0 = leaves, level 1 =
  // finest modules (leaf assignment levelId[1]), each coarser level from the
  // level-k -> level-(k+1) module map derived from the per-leaf level ids.
  m_hierLevels.clear();
  m_hierAssign.clear();
  m_hierLevels.emplace_back(); // level 0 is the leaf network; see hierLevel()
  m_hierAssign.push_back(levelId[1]);
  Level cur = aggregateLevel(leaf0(), levelId[1], levelK[1], m_undirected);
  m_hierLevels.push_back(cur);
  for (int k = 1; k < depth; ++k) {
    // level-k module (columnar level k, my j=k) -> level-(k+1) module (j=k+1).
    std::vector<int> assign(levelK[k], -1);
    for (int i = 0; i < m_nLeaves; ++i)
      assign[levelId[k][i]] = levelId[k + 1][i];
    m_hierAssign.push_back(assign);
    cur = aggregateLevel(cur, assign, levelK[k + 1], m_undirected);
    m_hierLevels.push_back(cur);
  }
  m_numTopModules = static_cast<unsigned int>(m_hierLevels.back().n);
  m_leafTop = levelId[1]; // leaf -> finest module, for a subsequent optimize
  return true;
}

double ColumnarTwoLevel::optimizeFromSeed(bool flat, unsigned int sweepLimit)
{
  if (m_hierLevels.size() < 2)
    return std::numeric_limits<double>::infinity(); // nothing seeded

  // The split and merge operators earn their keep on a seed the engine did not
  // build; see m_externalSeed.
  m_externalSeed = true;
  const bool flatSeed = m_hierLevels.size() == 2;

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
      ? deepRepairTwoLevelStack()
      : refineHierarchy(hierarchicalCodelengthFromStack(), sweepLimit);
  if (flatSeed)
    // The interleave leaves the leaves at the two-level fixpoint, exactly as
    // completeFlatFromAggregation does.
    m_bottomConverged = true;
  if (flat) {
    m_externalSeed = false;
    return L; // --two-level: there is no hierarchy to rebuild
  }

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
  std::vector<Level> bestLevels = m_hierLevels;
  std::vector<std::vector<int>> bestAssign = m_hierAssign;
  double bestL = L;

  if (flatSeed) {
    // A flat seed's bottom IS its top, so there is nothing to build over yet. Stack a
    // pass-through copy of it, making the seed modules the leaves' GRANDPARENTS, and
    // re-derive each grandparent's leaf set from singletons: building blocks that
    // respect the seed but are finer than it, which is what the up-build consumes.
    const int K = m_hierLevels[1].n;
    std::vector<int> ident(K);
    for (int i = 0; i < K; ++i)
      ident[i] = i;
    m_hierAssign.push_back(ident);
    m_hierLevels.push_back(aggregateLevel(m_hierLevels[1], ident, K, m_undirected));
    m_numTopModules = static_cast<unsigned int>(K);
    if (!refineBottomWithinParents()) {
      // No module could be subdivided: the seed is already at leaf granularity, so
      // the up-build would consume the same bottom the polish converged on.
      m_hierLevels = std::move(bestLevels);
      m_hierAssign = std::move(bestAssign);
      m_numTopModules = static_cast<unsigned int>(m_hierLevels.back().n);
      m_externalSeed = false;
      return bestL;
    }
  }

  m_leafTop = m_hierAssign[0];
  const int bottomK = m_hierLevels[1].n;
  m_numTopModules = static_cast<unsigned int>(bottomK);
  // The rebuilt bottom is a fresh from-singletons derivation, not the converged flat
  // partition, so the leaf-layer refine below is not a re-solve (see m_bottomConverged).
  m_bottomConverged = false;
  const double rebuilt = refineHierarchy(buildHierarchyFromBottom(bottomK), sweepLimit);
  if (rebuilt < bestL - kMinImprovement) {
    bestL = rebuilt;
  } else {
    m_hierLevels = std::move(bestLevels);
    m_hierAssign = std::move(bestAssign);
  }
  m_numTopModules = static_cast<unsigned int>(m_hierLevels.back().n);
  // Keep m_leafTop consistent with whichever stack won: nothing in the engine reads it
  // after this today (the materialization goes through m_hierAssign), but leaving it
  // pointing at the losing bottom is a trap for whatever calls this next.
  m_leafTop = m_hierAssign[0];
  m_externalSeed = false;
  return bestL;
}

ColumnarTwoLevel::Level ColumnarTwoLevel::aggregateLevel(const Level& base, const std::vector<int>& assign, int K, bool undirected)
{
  Level out;
  out.n = K;
  out.flow.assign(K, 0.0);
  out.linkEnter.assign(K, 0.0);
  out.linkExit.assign(K, 0.0);
  out.teleFlow.assign(K, 0.0);
  out.teleWeight.assign(K, 0.0);
  const bool haveTele = !base.teleFlow.empty();
  for (int i = 0; i < base.n; ++i) {
    out.flow[assign[i]] += base.flow[i];
    if (haveTele) {
      out.teleFlow[assign[i]] += base.teleFlow[i];
      out.teleWeight[assign[i]] += base.teleWeight[i];
    }
  }

  // Group-group edge flow + crossing enter/exit, from base out-edges.
  std::unordered_map<long long, double> edgeMap;
  edgeMap.reserve(base.outTarget.size());
  for (int a = 0; a < base.n; ++a) {
    const int ga = assign[a];
    for (int k = base.outStart[a]; k < base.outStart[a + 1]; ++k) {
      const int gb = assign[base.outTarget[k]];
      if (ga == gb)
        continue;
      const double f = base.outFlow[k];
      edgeMap[static_cast<long long>(ga) * K + gb] += f;
      if (undirected) {
        const double half = f / 2.0;
        out.linkExit[ga] += half;
        out.linkEnter[ga] += half;
        out.linkEnter[gb] += half;
        out.linkExit[gb] += half;
      } else {
        out.linkExit[ga] += f;
        out.linkEnter[gb] += f;
      }
    }
  }

  std::vector<int> outDeg(K, 0), inDeg(K, 0);
  for (const auto& kv : edgeMap) {
    ++outDeg[static_cast<int>(kv.first / K)];
    ++inDeg[static_cast<int>(kv.first % K)];
  }
  out.outStart.assign(K + 1, 0);
  out.inStart.assign(K + 1, 0);
  for (int i = 0; i < K; ++i) {
    out.outStart[i + 1] = out.outStart[i] + outDeg[i];
    out.inStart[i + 1] = out.inStart[i] + inDeg[i];
  }
  out.outTarget.assign(out.outStart[K], 0);
  out.outFlow.assign(out.outStart[K], 0.0);
  out.inTarget.assign(out.inStart[K], 0);
  out.inFlow.assign(out.inStart[K], 0.0);
  std::vector<int> outPos(out.outStart.begin(), out.outStart.end() - 1);
  std::vector<int> inPos(out.inStart.begin(), out.inStart.end() - 1);
  for (const auto& kv : edgeMap) {
    const int ga = static_cast<int>(kv.first / K);
    const int gb = static_cast<int>(kv.first % K);
    const double f = kv.second;
    out.outTarget[outPos[ga]] = gb;
    out.outFlow[outPos[ga]] = f;
    ++outPos[ga];
    out.inTarget[inPos[gb]] = ga;
    out.inFlow[inPos[gb]] = f;
    ++inPos[gb];
  }
  return out;
}

double ColumnarTwoLevel::optimizeHierarchical(unsigned int bottomBlockLimit)
{
  // Bottom: leaves -> building blocks. With bottomBlockLimit > 0 we stop the
  // aggregation early (finer bottom) and skip fine-tune to preserve fineness;
  // with 0 we take the full two-level optimum with fine-tune.
  optimizeTwoLevel(bottomBlockLimit, bottomBlockLimit == 0);
  return buildHierarchyFromBottom(static_cast<int>(m_numTopModules));
}

// Grow the multi-level hierarchy from the ALREADY-BUILT bottom two-level
// (m_leaf0 / m_leafTop, with bottomK modules) using the enter-flow super-search.
// Independent of the bottom, so optimizeColumnar builds the bottom once and calls
// this per up-merge strategy (m_superAggLimit) — the bottom is identical across
// strategies (superAgg only shapes the up-build), so recomputing it was waste.
// Reads m_leaf0/m_leafTop without mutating them, so repeated calls from the same
// bottom are deterministic and produce the same result as a full rebuild.
double ColumnarTwoLevel::buildHierarchyFromBottom(int bottomK)
{
  using infomath::plogp;

  m_hierLevels.clear();
  m_hierAssign.clear();
  m_hierLevels.emplace_back(); // level 0 is the leaf network; see hierLevel()
  m_hierAssign.push_back(m_leafTop);
  Level cur = aggregateLevel(leaf0(), m_leafTop, bottomK, m_undirected);
  m_hierLevels.push_back(cur);

  // Grow up with the enter-flow super-search while it shortens the index code.
  while (cur.n > 1) {
    pollInterrupt();
    // Enter-flow transform: the super-network's node flow is the module's index
    // rate q; enter/exit, the teleport aggregates and the edges are the module's
    // inter-module quantities, carried through unchanged.
    Level superNet = cur;
    setIndexRateAsFlow(superNet);

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
    superOpt.setInterruptCallback(m_interruptCallback);
    superOpt.buildFromLevel(std::move(superNet), m_undirected, m_seed, 0.0, m_recordedTeleport, m_totalTeleFlow);
    // Conservative up-build (m_superAggLimit > 0): fewer aggregation passes per
    // super-level so we don't collapse the whole level in one greedy jump —
    // more, finer super-levels for the down-sweep to tune and (later) collapse.
    const double superCodelength = m_superAggLimit > 0
        ? superOpt.optimizeTwoLevel(m_superAggLimit, false)
        : superOpt.optimizeTwoLevel();
    const unsigned int superK = superOpt.numTopModules();

    const bool trivial = superK <= 1 || static_cast<int>(superK) == cur.n;
    if (trivial || superCodelength >= curIndexCodelength - kMinImprovement)
      break;

    m_hierAssign.push_back(superOpt.leafTopModule());
    cur = aggregateLevel(cur, superOpt.leafTopModule(), static_cast<int>(superK), m_undirected);
    m_hierLevels.push_back(cur);
  }

  m_numTopModules = static_cast<unsigned int>(m_hierLevels.back().n);
  return hierarchicalCodelengthFromStack();
}

void ColumnarTwoLevel::addSlicedLeafCorrections(ColumnarTwoLevel& subOpt, const std::vector<int>& globalLeafIds) const
{
  for (const auto& c : m_corrections) {
    if (!c->participatesInMoveLoop())
      continue; // structural (e.g. Bias): gating-only, not part of the refine
    auto sliced = c->sliceForLeaves(globalLeafIds);
    if (sliced)
      subOpt.addCorrection(std::move(sliced));
  }
}

bool ColumnarTwoLevel::buildPartialSeed(const Level& sub, const std::vector<int>& S, const std::vector<int>& assign, int layer, std::vector<int>& seed) const
{
  const int nP = sub.n;
  if (!partSeedActive(layer) || (partSeedResweepOnly() && m_refineSweep == 0))
    return false; // from singletons, the default
  if (nP <= 1)
    return false; // a single unit has nothing to re-place
  const double q = partSeedRelease();
  const PartSeedMetric metric = partSeedMetric();

  std::vector<char> released(nP, 0);
  if (metric == PartSeedMetric::Random) {
    for (int j = 0; j < nP; ++j) {
      const unsigned long long h = partSeedMix(static_cast<unsigned long long>(m_seed)
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
        const double tot = sub.flow[j] + sub.linkExit[j];
        if (tot > 0.0)
          loose[j] = sub.linkExit[j] / tot;
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

int ColumnarTwoLevel::subClusterLeaves(const std::vector<int>& S, double parentExit, std::vector<int>& loc, std::vector<int>& localAssign, bool fineTune, const std::vector<int>* leafModule)
{
  return subClusterUnits(leaf0(), false, true, S, parentExit, loc, localAssign, fineTune, leafModule);
}

int ColumnarTwoLevel::subClusterUnits(const Level& base, bool interior, bool sliceCorrections, const std::vector<int>& S, double parentExit, std::vector<int>& loc, std::vector<int>& localAssign, bool fineTune, const std::vector<int>* unitModule)
{
  const int nP = static_cast<int>(S.size());
  for (int j = 0; j < nP; ++j)
    loc[S[j]] = j;

  // Build the parent's internal sub-network over its children (global flow --
  // or enter flow for interior units, whose codeword usage is the enter flow --
  // and the edges internal to the parent).
  Level sub;
  sub.n = nP;
  sub.flow.resize(nP);
  sub.linkEnter.resize(nP);
  sub.linkExit.resize(nP);
  sub.teleFlow.resize(nP);
  sub.teleWeight.resize(nP);
  std::vector<int> outDeg(nP, 0), inDeg(nP, 0);
  for (int j = 0; j < nP; ++j) {
    const int g = S[j];
    sub.flow[j] = interior ? unitIndexRate(base, g) : base.flow[g];
    sub.linkEnter[j] = base.linkEnter[g];
    sub.linkExit[j] = base.linkExit[g];
    sub.teleFlow[j] = base.teleFlow.empty() ? 0.0 : base.teleFlow[g];
    sub.teleWeight[j] = base.teleWeight.empty() ? 0.0 : base.teleWeight[g];
    for (int k = base.outStart[g]; k < base.outStart[g + 1]; ++k) {
      const int lt = loc[base.outTarget[k]];
      if (lt != -1) {
        ++outDeg[j];
        ++inDeg[lt];
      }
    }
  }
  sub.outStart.assign(nP + 1, 0);
  sub.inStart.assign(nP + 1, 0);
  for (int j = 0; j < nP; ++j) {
    sub.outStart[j + 1] = sub.outStart[j] + outDeg[j];
    sub.inStart[j + 1] = sub.inStart[j] + inDeg[j];
  }
  sub.outTarget.assign(sub.outStart[nP], 0);
  sub.outFlow.assign(sub.outStart[nP], 0.0);
  sub.inTarget.assign(sub.inStart[nP], 0);
  sub.inFlow.assign(sub.inStart[nP], 0.0);
  std::vector<int> op(sub.outStart.begin(), sub.outStart.end() - 1);
  std::vector<int> ip(sub.inStart.begin(), sub.inStart.end() - 1);
  for (int j = 0; j < nP; ++j) {
    const int g = S[j];
    for (int k = base.outStart[g]; k < base.outStart[g + 1]; ++k) {
      const int lt = loc[base.outTarget[k]];
      if (lt != -1) {
        const double f = base.outFlow[k];
        sub.outTarget[op[j]] = lt;
        sub.outFlow[op[j]] = f;
        ++op[j];
        sub.inTarget[ip[lt]] = j;
        sub.inFlow[ip[lt]] = f;
        ++ip[lt];
      }
    }
  }

  // Optimal in-context two-level of the parent's children (its exit is the
  // sub-network exit), objective-aware at leaf level: slice Meta/Mem to the
  // leaves so the re-partition optimizes base + correction, not just gates on
  // it. Interior levels stay first-order (as refineLayerWithinGrandparent).
  ColumnarTwoLevel subOpt;
  subOpt.setInterruptCallback(m_interruptCallback);
  // Partial seeding: keep the cores of the partition we were handed, release its
  // loose boundary as singletons (leaf layer, so layer == 0). Interior callers
  // pass no partition, which is the from-singletons default. Read `sub` here,
  // before buildFromLevel takes ownership of its storage below.
  std::vector<int> seed;
  const std::vector<int>* seedPtr = nullptr;
  if (unitModule != nullptr && buildPartialSeed(sub, S, *unitModule, 0, seed))
    seedPtr = &seed;

  subOpt.buildFromLevel(std::move(sub), m_undirected, m_seed, parentExit, m_recordedTeleport, m_totalTeleFlow);
  subOpt.m_rootLeaves = m_rootLeaves;
  if (sliceCorrections)
    addSlicedLeafCorrections(subOpt, S);
  subOpt.optimizeTwoLevel(0, fineTune, seedPtr);
  localAssign.assign(subOpt.leafTopModule().begin(), subOpt.leafTopModule().end());
  const int Ksub = static_cast<int>(subOpt.numTopModules());

  for (int j = 0; j < nP; ++j)
    loc[S[j]] = -1; // reset scratch
  return Ksub;
}

bool ColumnarTwoLevel::refineBottomWithinParents()
{
  if (m_hierLevels.size() < 3)
    return false; // no super-structure to refine within

  const std::vector<int>& a0 = m_hierAssign[0]; // leaf -> L1
  const std::vector<int>& a1 = m_hierAssign[1]; // L1 -> L2
  const int numL2 = hierLevel(2).n;

  // Group leaves by their level-2 module (the parent to refine within).
  std::vector<std::vector<int>> leavesPer(numL2);
  for (int i = 0; i < m_nLeaves; ++i)
    leavesPer[a1[a0[i]]].push_back(i);

  std::vector<int> newA0(m_nLeaves, -1); // leaf -> new L1'
  std::vector<int> newA1; // new L1' -> L2
  int nextL1 = 0;
  std::vector<int> loc(m_nLeaves, -1); // global leaf -> local id (reused per P)

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
    const int Ksub = subClusterLeaves(S, hierLevel(2).linkExit[P], loc, subAssign, true, &a0);

    for (int j = 0; j < nP; ++j)
      newA0[S[j]] = nextL1 + subAssign[j];
    for (int s = 0; s < Ksub; ++s)
      newA1.push_back(P);
    nextL1 += Ksub;
  }

  // Replace the bottom level; levels 2+ are unchanged (same leaf sets per L2).
  m_hierAssign[0] = std::move(newA0);
  m_hierAssign[1] = std::move(newA1);
  m_hierLevels[1] = aggregateLevel(leaf0(), m_hierAssign[0], nextL1, m_undirected);
  m_numTopModules = static_cast<unsigned int>(m_hierLevels.back().n);
  return true;
}

void ColumnarTwoLevel::coarsenModules(double& L, int maxSweeps)
{
  // Gated apply: run `step`, keep it only if it lowers the true hierarchical
  // codelength, else revert. Same accept/revert policy the converge refinement
  // uses for its tuning steps.
  // The leaf network m_hierLevels[0] is invariant under every step gated here
  // (mergeLeafModulesWithinParents and refineTopLayer rewrite assignments and
  // module levels only — see the writes to m_hierLevels[k+1]/[top]), so it is
  // excluded from the snapshot. Copying it dominated the gate on large networks:
  // it carries the full leaf CSR (~50 MB on web-NotreDame) and was duplicated
  // twice per sweep. Bit-exact — the restored state is identical.
  auto gated = [&](auto&& step) {
    std::vector<Level> savedUpper(m_hierLevels.begin() + 1, m_hierLevels.end());
    std::vector<std::vector<int>> savedAssign = m_hierAssign;
    if (!step())
      return false;
    const double after = hierarchicalCodelengthFromStack();
    if (after < L - kMinImprovement) {
      L = after;
      return true;
    }
    m_hierLevels.resize(savedUpper.size() + 1);
    for (std::size_t j = 0; j < savedUpper.size(); ++j)
      m_hierLevels[j + 1] = std::move(savedUpper[j]);
    m_hierAssign = std::move(savedAssign);
    m_numTopModules = static_cast<unsigned int>(m_hierLevels.back().n);
    return false;
  };
  // Hierarchical split operator (COL_HSPLIT, default off): the merge's dual.
  // Only on stacks with a real hierarchy — a two-level stack is the two-level
  // search's own territory (splitTopModules), which must stay untouched.
  const int splitMode = m_forceHSplit ? hSplitWinnerMode() : hSplitMode();
  const bool doSplit = splitMode != kHSplitOff && static_cast<int>(m_hierLevels.size()) >= 3;
  // Per-level dirty flag: splitLevelModules(k) is deterministic given the
  // stack, so once it has rejected every source at level k it will keep
  // rejecting until some other operator changes the partition. Skipping the
  // repeat calls is free (identical trajectory) and it is most of the cost —
  // each attempt pays a piece aggregation over the whole level plus a settled
  // move loop, and the convergence tail is nothing but repeats.
  std::vector<char> splitDirty(doSplit ? m_hierLevels.size() : 0, 1);
  for (int sweep = 0; sweep < maxSweeps; ++sweep) {
    pollInterrupt();
    const bool merged = gated([&] { return mergeLeafModulesWithinParents(); });
    const bool regrouped = gated([&] { return refineTopLayer(); });
    bool split = false;
    if (doSplit) {
      const int top = static_cast<int>(m_hierLevels.size()) - 1;
      if (merged || regrouped)
        std::fill(splitDirty.begin(), splitDirty.end(), 1);
      // auto/winner: the leaf level only where a module-move-capable correction
      // makes it pay (measured — on base networks k = 0 accepts nothing and is
      // the most expensive level, because refineLayerWithinGrandparent(0) has
      // just re-derived that partition from singletons).
      const bool leafNeedsCorrection = splitMode == kHSplitAuto || splitMode == kHSplitWinner;
      const bool leafAllowed = !leafNeedsCorrection || hasModuleMoveCorrections();
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

double ColumnarTwoLevel::optimizeFlexible(unsigned int bottomBlockLimit, unsigned int sweepLimit)
{
  m_bottomConverged = false;
  double flatL = std::numeric_limits<double>::infinity();
  std::vector<Level> flatLevels;
  std::vector<std::vector<int>> flatAssign;
  double L;
  if (m_flatFirstBottom) {
    // Flat-first trial (see setFlatFirstBottom): probe the flat basin with the
    // full aggregation only, build from the fine-blocks bottom of the same
    // pass-1 as usual, and complete the expensive leaf-level flat pipeline
    // only when the probe is competitive — then keep the better build and the
    // flat stack as a gated candidate (below).
    const double flatEst = optimizeTwoLevel(0, false);
    std::vector<int> flatAggTop = m_leafTop;
    const int flatAggK = static_cast<int>(m_numTopModules);
    m_leafTop = m_leafBlocks;
    int fineK = 0;
    for (int b : m_leafBlocks)
      if (b >= fineK)
        fineK = b + 1;
    m_numTopModules = static_cast<unsigned int>(fineK);
    L = buildHierarchyFromBottom(fineK);
    const bool complete = flatEst < L * (1.0 + kFlatProbeMargin);
#ifdef COLUMNAR_DEBUG
    std::fprintf(stderr, "[flat-first -F] est=%.6f build=%.6f ratio=%.4f %s\n", flatEst, L, flatEst / L, complete ? "complete" : "skip");
#endif
    if (complete) {
      std::vector<Level> fineLevels = m_hierLevels;
      std::vector<std::vector<int>> fineAssign = m_hierAssign;
      const unsigned int fineTop = m_numTopModules;
      flatL = completeFlatFromAggregation(std::move(flatAggTop), flatAggK);
      flatLevels = m_hierLevels;
      flatAssign = m_hierAssign;
      m_leafTop = m_hierAssign[0];
      const double buildFlat = buildHierarchyFromBottom(flatLevels[1].n);
      if (buildFlat < L - kMinImprovement) {
        L = buildFlat;
      } else {
        m_hierLevels = std::move(fineLevels);
        m_hierAssign = std::move(fineAssign);
        m_numTopModules = fineTop;
        m_bottomConverged = false; // back on the fine-blocks bottom
      }
    }
  } else {
    L = optimizeHierarchical(bottomBlockLimit);
  }
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
    m_hierLevels = std::move(flatLevels);
    m_hierAssign = std::move(flatAssign);
    m_numTopModules = static_cast<unsigned int>(m_hierLevels.back().n);
    L = flatL;
  }
  return L;
}

bool ColumnarTwoLevel::refineLayerWithinGrandparent(int k)
{
  const int top = static_cast<int>(m_hierLevels.size()) - 1;
  if (k < 0 || k + 2 > top)
    return false; // need a layer-(k+2) grandparent to refine within

  const Level& base = hierLevel(k); // layer-k units to re-partition
  const std::vector<int>& aC = m_hierAssign[k]; // unit_k -> module_{k+1}
  const std::vector<int>& aP = m_hierAssign[k + 1]; // module_{k+1} -> module_{k+2}
  const Level& grand = hierLevel(k + 2);
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
  // codeword usage is the module's index rate q (unitIndexRate) — mirror the
  // up-build's enter-flow transform so grouping optimizes the right cost
  // instead of over-merging on total flow.
  const bool interior = (k > 0);

  for (int G = 0; G < numGP; ++G) {
    const std::vector<int>& S = unitsPer[G];
    const int nP = static_cast<int>(S.size());
    if (nP == 0)
      continue;
    for (int j = 0; j < nP; ++j)
      loc[S[j]] = j;

    // Build G's internal sub-network over layer-k units (flow/enter/exit carried
    // from the units; only edges internal to G are kept).
    Level sub;
    sub.n = nP;
    sub.flow.resize(nP);
    sub.linkEnter.resize(nP);
    sub.linkExit.resize(nP);
    sub.teleFlow.resize(nP);
    sub.teleWeight.resize(nP);
    std::vector<int> outDeg(nP, 0), inDeg(nP, 0);
    for (int j = 0; j < nP; ++j) {
      const int g = S[j];
      sub.flow[j] = interior ? unitIndexRate(base, g) : base.flow[g];
      sub.linkEnter[j] = base.linkEnter[g];
      sub.linkExit[j] = base.linkExit[g];
      sub.teleFlow[j] = base.teleFlow.empty() ? 0.0 : base.teleFlow[g];
      sub.teleWeight[j] = base.teleWeight.empty() ? 0.0 : base.teleWeight[g];
      for (int e = base.outStart[g]; e < base.outStart[g + 1]; ++e) {
        const int lt = loc[base.outTarget[e]];
        if (lt != -1) {
          ++outDeg[j];
          ++inDeg[lt];
        }
      }
    }
    sub.outStart.assign(nP + 1, 0);
    sub.inStart.assign(nP + 1, 0);
    for (int j = 0; j < nP; ++j) {
      sub.outStart[j + 1] = sub.outStart[j] + outDeg[j];
      sub.inStart[j + 1] = sub.inStart[j] + inDeg[j];
    }
    sub.outTarget.assign(sub.outStart[nP], 0);
    sub.outFlow.assign(sub.outStart[nP], 0.0);
    sub.inTarget.assign(sub.inStart[nP], 0);
    sub.inFlow.assign(sub.inStart[nP], 0.0);
    std::vector<int> op(sub.outStart.begin(), sub.outStart.end() - 1);
    std::vector<int> ip(sub.inStart.begin(), sub.inStart.end() - 1);
    for (int j = 0; j < nP; ++j) {
      const int g = S[j];
      for (int e = base.outStart[g]; e < base.outStart[g + 1]; ++e) {
        const int lt = loc[base.outTarget[e]];
        if (lt != -1) {
          const double f = base.outFlow[e];
          sub.outTarget[op[j]] = lt;
          sub.outFlow[op[j]] = f;
          ++op[j];
          sub.inTarget[ip[lt]] = j;
          sub.inFlow[ip[lt]] = f;
          ++ip[lt];
        }
      }
    }

    // Optimal in-context two-level of G's units (G's exit = sub-network exit).
    // At k == 0 the units are leaves, so the leaf-shaping corrections apply and
    // are sliced to G's leaves; interior levels (k > 0) stay first-order (base).
    // Partial seeding: lock the cores of the layer-k partition inside G, release
    // its loose boundary as singletons for the sub-optimize to re-place. Read
    // `sub` here, before buildFromLevel takes ownership of its storage below.
    std::vector<int> seed;
    const std::vector<int>* seedPtr = nullptr;
    if (buildPartialSeed(sub, S, aC, k, seed))
      seedPtr = &seed;

    ColumnarTwoLevel subOpt;
    subOpt.setInterruptCallback(m_interruptCallback);
    subOpt.buildFromLevel(std::move(sub), m_undirected, m_seed, grand.linkExit[G], m_recordedTeleport, m_totalTeleFlow);
    subOpt.m_rootLeaves = m_rootLeaves;
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

    for (int j = 0; j < nP; ++j)
      loc[S[j]] = -1; // reset scratch
  }

  m_hierAssign[k] = std::move(newAC);
  m_hierAssign[k + 1] = std::move(newAP);
  m_hierLevels[k + 1] = aggregateLevel(base, m_hierAssign[k], nextMod, m_undirected);
  m_numTopModules = static_cast<unsigned int>(m_hierLevels.back().n);
  return true;
}

bool ColumnarTwoLevel::refineTopLayer()
{
  const int top = static_cast<int>(m_hierLevels.size()) - 1;
  if (top < 2)
    return false; // need a layer below the top (layer top-1) to regroup
  const int k = top - 1; // re-partition layer-k units into new top modules

  const Level& base = hierLevel(k);
  const int nU = base.n;

  // One root group over all layer-k units; enter-flow transform (grouping
  // modules, whose codeword usage is the index rate q), root exit = 0.
  Level sub;
  sub.n = nU;
  sub.flow.resize(nU);
  sub.linkEnter.resize(nU);
  sub.linkExit.resize(nU);
  sub.teleFlow.resize(nU);
  sub.teleWeight.resize(nU);
  std::vector<int> outDeg(nU, 0), inDeg(nU, 0);
  for (int j = 0; j < nU; ++j) {
    sub.flow[j] = unitIndexRate(base, j);
    sub.linkEnter[j] = base.linkEnter[j];
    sub.linkExit[j] = base.linkExit[j];
    sub.teleFlow[j] = base.teleFlow.empty() ? 0.0 : base.teleFlow[j];
    sub.teleWeight[j] = base.teleWeight.empty() ? 0.0 : base.teleWeight[j];
    for (int e = base.outStart[j]; e < base.outStart[j + 1]; ++e) {
      ++outDeg[j];
      ++inDeg[base.outTarget[e]];
    }
  }
  sub.outStart.assign(nU + 1, 0);
  sub.inStart.assign(nU + 1, 0);
  for (int j = 0; j < nU; ++j) {
    sub.outStart[j + 1] = sub.outStart[j] + outDeg[j];
    sub.inStart[j + 1] = sub.inStart[j] + inDeg[j];
  }
  sub.outTarget.assign(sub.outStart[nU], 0);
  sub.outFlow.assign(sub.outStart[nU], 0.0);
  sub.inTarget.assign(sub.inStart[nU], 0);
  sub.inFlow.assign(sub.inStart[nU], 0.0);
  std::vector<int> op(sub.outStart.begin(), sub.outStart.end() - 1);
  std::vector<int> ip(sub.inStart.begin(), sub.inStart.end() - 1);
  for (int j = 0; j < nU; ++j)
    for (int e = base.outStart[j]; e < base.outStart[j + 1]; ++e) {
      const int t = base.outTarget[e];
      const double f = base.outFlow[e];
      sub.outTarget[op[j]] = t;
      sub.outFlow[op[j]] = f;
      ++op[j];
      sub.inTarget[ip[t]] = j;
      sub.inFlow[ip[t]] = f;
      ++ip[t];
    }

  ColumnarTwoLevel subOpt;
  subOpt.setInterruptCallback(m_interruptCallback);
  subOpt.buildFromLevel(std::move(sub), m_undirected, m_seed, 0.0, m_recordedTeleport, m_totalTeleFlow);
  subOpt.optimizeTwoLevel();
  const int Ksub = static_cast<int>(subOpt.numTopModules());
  if (Ksub <= 1 || Ksub == nU)
    return false; // no useful regrouping (trivial partition)

  m_hierAssign[k] = subOpt.leafTopModule();
  m_hierLevels[top] = aggregateLevel(base, m_hierAssign[k], Ksub, m_undirected);
  m_numTopModules = static_cast<unsigned int>(Ksub);
  return true;
}

bool ColumnarTwoLevel::mergeLeafModulesWithinParents()
{
  using infomath::plogp;
  if (m_hierLevels.size() < 2)
    return false; // no module level to merge

  // Corrections that reward coarsening (Mem/Meta/Lossy). No participants => the
  // base objective, where any merge only raises the codelength: nothing to do.
  std::vector<ColumnarCorrection*> corr;
  for (auto& c : m_corrections)
    if (c->participatesInMoveLoop())
      corr.push_back(c.get());
  // ...unless the partition is an external seed (m_externalSeed, #824): "any merge only
  // raises the codelength" holds for a partition this engine's aggregation produced,
  // whose module-level move loop already settled every merge, not for an over-fine
  // cluster file. mergeCost below is the full base-objective delta, and the caller
  // gates each accepted sweep on the true stack codelength. Measured: a 32-clique ring
  // seeded with its own planted partition returned that partition unchanged at
  // 3.652410119 under --two-level -- 2.3% above what the same run finds from scratch --
  // because merging adjacent cliques was the only move that helped and it was skipped.
  if (corr.empty() && !m_externalSeed)
    return false;

  const int K = hierLevel(1).n; // leaf modules (level-0 -> 1)
  // Leaf modules are merged within their parent. With a level-2 grandparent each
  // leaf module has an explicit parent; in a two-level tree (e.g. lossy) the
  // parent is the root, i.e. one group whose codebook uses the top enter flows.
  const bool hasGrandparent = m_hierLevels.size() >= 3;
  const int numParents = hasGrandparent ? hierLevel(2).n : 1;

  // Seed the corrections' per-module state at the current leaf-module partition.
  for (auto* c : corr)
    c->initMoveLoop(m_hierAssign[0], K);

  // Mutable leaf-module aggregates + parent map + adjacency (crossing flow).
  std::vector<double> flow = hierLevel(1).flow;
  std::vector<double> enter = hierLevel(1).linkEnter;
  std::vector<double> exit = hierLevel(1).linkExit;
  std::vector<int> parent = hasGrandparent ? m_hierAssign[1] : std::vector<int>(K, 0);
  std::vector<std::unordered_map<int, double>> adjOut(K), adjIn(K);
  {
    const Level& L1 = hierLevel(1);
    for (int a = 0; a < K; ++a)
      for (int e = L1.outStart[a]; e < L1.outStart[a + 1]; ++e) {
        const int t = L1.outTarget[e];
        const double f = L1.outFlow[e];
        adjOut[a][t] += f;
        adjIn[t][a] += f;
      }
  }
  // Parent module-of-modules "total use" = parent exit + sum of child enter.
  // The root's exit is 0 (whole-network codebook).
  std::vector<double> totalUse(numParents, 0.0);
  if (hasGrandparent)
    for (int p = 0; p < numParents; ++p)
      totalUse[p] = hierLevel(2).linkExit[p];
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
    const double w = crossing(a, b);
    const double flowB2 = flow[a] + flow[b];
    const double exitB2 = exit[a] + exit[b] - w;
    const double enterB2 = enter[a] + enter[b] - w;
    const double Ta = flow[a] + exit[a], Tb = flow[b] + exit[b], Tb2 = flowB2 + exitB2;
    // Level-1 leaf-module term: plogp(T) - plogp(exit) - sum_leaf plogp(flow);
    // the per-leaf sums cancel in the merge, leaving flows/exits only.
    const double dL1 = (plogp(Tb2) - plogp(Ta) - plogp(Tb))
        - (plogp(exitB2) - plogp(exit[a]) - plogp(exit[b]));
    // Parent module-of-modules term for the shared parent.
    const int p = parent[b];
    const double tu = totalUse[p];
    const double dPar = (plogp(tu - w) - plogp(tu))
        - (plogp(enterB2) - plogp(enter[a]) - plogp(enter[b]));
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
      const double w = crossing(a, b);
      // Commit aggregates.
      flow[b] += flow[a];
      const double newExit = exit[a] + exit[b] - w;
      const double newEnter = enter[a] + enter[b] - w;
      exit[b] = newExit;
      enter[b] = newEnter;
      totalUse[parent[b]] -= w;
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
  std::vector<int> newA0(m_nLeaves);
  for (int i = 0; i < m_nLeaves; ++i)
    newA0[i] = newId[root(m_hierAssign[0][i])];

  m_hierAssign[0] = std::move(newA0);
  m_hierLevels[1] = aggregateLevel(leaf0(), m_hierAssign[0], Knew, m_undirected);
  // In a 3+-level tree the merged leaf modules keep their (unchanged) parent; in
  // a two-level tree there is no parent-assignment level to rewrite.
  if (hasGrandparent) {
    std::vector<int> newA1(Knew);
    for (int a = 0; a < K; ++a)
      if (alive[a])
        newA1[newId[a]] = parent[a];
    m_hierAssign[1] = std::move(newA1);
  }
  m_numTopModules = static_cast<unsigned int>(m_hierLevels.back().n);
  return true;
}

double ColumnarTwoLevel::optimizeConverge(unsigned int bottomBlockLimit, unsigned int superAggLimit, unsigned int sweepLimit)
{
  m_superAggLimit = superAggLimit;
  const double L = optimizeHierarchical(bottomBlockLimit);
  return refineHierarchy(L, sweepLimit);
}

double ColumnarTwoLevel::refineHierarchy(double startL, unsigned int sweepLimit)
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
  std::fprintf(stderr, "[refine] start L=%.9f levels=%d superAgg=%u\n", L, (int)m_hierLevels.size(), m_superAggLimit);
#endif
  // Helper: run one gated tuning step (rebuild -> accept if it lowers the true
  // codelength, else revert). Returns whether it improved.
  // As in coarsenModules: refineLayerWithinGrandparent(k) writes m_hierAssign[k],
  // m_hierAssign[k+1] and m_hierLevels[k+1] with k >= 0, so the leaf network
  // m_hierLevels[0] is never touched and stays out of the snapshot (bit-exact).
  auto gatedStep = [&](auto&& step, const char* tag) -> bool {
    std::vector<Level> savedUpper(m_hierLevels.begin() + 1, m_hierLevels.end());
    std::vector<std::vector<int>> savedAssign = m_hierAssign;
    if (!step())
      return false;
    const double after = hierarchicalCodelengthFromStack();
    if (after < L - kMinImprovement) {
#ifdef COLUMNAR_DEBUG
      std::fprintf(stderr, "[refine] %s %.9f -> %.9f (levels %d, top %d)\n", tag, L, after, (int)m_hierLevels.size(), m_hierLevels.back().n);
#else
      (void)tag;
#endif
      L = after;
      return true;
    }
    m_hierLevels.resize(savedUpper.size() + 1);
    for (std::size_t j = 0; j < savedUpper.size(); ++j)
      m_hierLevels[j + 1] = std::move(savedUpper[j]);
    m_hierAssign = std::move(savedAssign);
    m_numTopModules = static_cast<unsigned int>(m_hierLevels.back().n);
    return false;
  };

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
  const int numInterior = std::max(0, static_cast<int>(m_hierLevels.size()) - 2);
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
    m_refineSweep = sweep;
    const double beforeSweep = L;
    bool improved = false;
    const int top = static_cast<int>(m_hierLevels.size()) - 1;
    std::vector<char> nextDirty(dirty.size(), 0);
    for (int k = 0; k + 2 <= top; ++k) {
      if (!dirty[k])
        continue;
      if (gatedStep([&] { return refineLayerWithinGrandparent(k); }, "REFINE")) {
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
  m_refineSweep = 0; // out of the sweep loop: no layer is a re-refine

  // Module coarsening: merge leaf modules (mergeLeafModulesWithinParents) and
  // regroup the top level (refineTopLayer), interleaved to convergence. Both
  // operate on the few modules (not the leaves), so this loop is cheap; iterating
  // lets a merge cross parents that a prior top-refine has regrouped. No-op for
  // the base objective (merge needs a correction; top-refine is gated).
#ifdef COLUMNAR_DEBUG
  const std::clock_t t_p2 = std::clock();
#endif
  coarsenModules(L, maxSweeps);
#ifdef COLUMNAR_DEBUG
  const std::clock_t t_end = std::clock();
  std::fprintf(stderr, "[refine] interior=%.3fs coarsen=%.3fs (superAgg=%u)\n", double(t_p2 - t_p1) / CLOCKS_PER_SEC, double(t_end - t_p2) / CLOCKS_PER_SEC, m_superAggLimit);
#endif
  return L;
}

std::vector<std::pair<unsigned int, std::vector<unsigned int>>>
ColumnarTwoLevel::toNodePaths(const std::vector<InfoNode*>& leafNodes) const
{
  std::vector<std::pair<unsigned int, std::vector<unsigned int>>> paths;
  const int nLeaves = m_hierLevels.empty() ? 0 : hierLevel(0).n;
  const int top = static_cast<int>(m_hierLevels.size()) - 1; // number of module levels
  paths.reserve(nLeaves);
  if (top < 1)
    return paths; // no module structure to materialize

  for (int i = 0; i < nLeaves; ++i) {
    // Walk the assignment stack upward: chain[k] = level-(k+1) module id of leaf i.
    std::vector<int> chain;
    chain.reserve(top);
    int u = i;
    for (int k = 0; k < top; ++k) {
      u = m_hierAssign[k][u];
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

double ColumnarTwoLevel::optimizeColumnar(unsigned int bottomBlockLimit, unsigned int sweepLimit)
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
  // The bottom two-level is IDENTICAL across strategies (m_superAggLimit only
  // shapes the up-build), so build the bottom ONCE and grow each strategy from
  // the shared bottom via buildHierarchyFromBottom — half the build work,
  // bit-identical result.
  static const unsigned int kSuperAggSettings[] = { 0u, 1u };

  double flatEst = std::numeric_limits<double>::infinity();
  std::vector<int> flatAggTop;
  int flatAggK = 0;
  if (m_flatFirstBottom) {
    // Flat-first trial (see setFlatFirstBottom): probe the flat basin with the
    // full aggregation only (module-level cost, no leaf fine-tune), and keep
    // the fine-blocks bottom from the same pass-1 (m_leafBlocks) for the
    // regular screen below — no second leaf sweep. The expensive leaf-level
    // flat pipeline runs after the screen, only when the probe is competitive.
    flatEst = optimizeTwoLevel(0, false);
    flatAggTop = m_leafTop;
    flatAggK = static_cast<int>(m_numTopModules);
    m_leafTop = m_leafBlocks;
    int fineK = 0;
    for (int b : m_leafBlocks)
      if (b >= fineK)
        fineK = b + 1;
    m_numTopModules = static_cast<unsigned int>(fineK);
  } else {
    optimizeTwoLevel(bottomBlockLimit, bottomBlockLimit == 0);
  }
  const int bottomK = static_cast<int>(m_numTopModules);

  double bestBuildL = std::numeric_limits<double>::infinity();
  std::vector<Level> bestLevels;
  std::vector<std::vector<int>> bestAssign;
  unsigned int bestTop = 0;
  unsigned int bestSuperAgg = 0;
  bool bestBottomConverged = false;

  for (unsigned int superAgg : kSuperAggSettings) {
    pollInterrupt();
    m_superAggLimit = superAgg;
    const double buildL = buildHierarchyFromBottom(bottomK);
#ifdef COLUMNAR_DEBUG
    std::fprintf(stderr, "[screen] superAgg=%u buildL=%.6f top=%u\n", superAgg, buildL, m_numTopModules);
#endif
    if (buildL < bestBuildL - kMinImprovement) {
      bestBuildL = buildL;
      bestLevels = m_hierLevels;
      bestAssign = m_hierAssign;
      bestTop = m_numTopModules;
      bestSuperAgg = superAgg;
      bestBottomConverged = false;
    }
  }

  // Flat-first trial: complete the flat pipeline when the probe lands within
  // the margin, keep the flat stack as a candidate, and screen the up-builds
  // grown from the flat bottom as additional strategies.
  double flatL = std::numeric_limits<double>::infinity();
  std::vector<Level> flatLevels;
  std::vector<std::vector<int>> flatAssign;
  if (m_flatFirstBottom) {
    const bool complete = flatEst < bestBuildL * (1.0 + kFlatProbeMargin);
#ifdef COLUMNAR_DEBUG
    std::fprintf(stderr, "[flat-first] est=%.6f build=%.6f ratio=%.4f %s\n", flatEst, bestBuildL, flatEst / bestBuildL, complete ? "complete" : "skip");
#endif
    if (complete) {
      flatL = completeFlatFromAggregation(std::move(flatAggTop), flatAggK);
      flatLevels = m_hierLevels;
      flatAssign = m_hierAssign;
      m_leafTop = m_hierAssign[0];
      const int flatK = hierLevel(1).n;
      for (unsigned int superAgg : kSuperAggSettings) {
        pollInterrupt();
        m_superAggLimit = superAgg;
        const double buildL = buildHierarchyFromBottom(flatK);
        if (buildL < bestBuildL - kMinImprovement) {
          bestBuildL = buildL;
          bestLevels = m_hierLevels;
          bestAssign = m_hierAssign;
          bestTop = m_numTopModules;
          bestSuperAgg = superAgg;
          bestBottomConverged = true;
        }
      }
    }
  }

  // Restore the winning build and refine only it.
  m_hierLevels = std::move(bestLevels);
  m_hierAssign = std::move(bestAssign);
  m_numTopModules = bestTop;
  m_superAggLimit = bestSuperAgg;
  m_bottomConverged = bestBottomConverged;
  double bestL = refineHierarchy(bestBuildL, sweepLimit);
  // Flat-first trial: the super-build may not pay for itself — keep the flat
  // two-level stack when it beats the refined hierarchy.
  if (flatL < bestL - kMinImprovement) {
    m_hierLevels = std::move(flatLevels);
    m_hierAssign = std::move(flatAssign);
    m_numTopModules = static_cast<unsigned int>(m_hierLevels.back().n);
    bestL = flatL;
  }
#ifdef COLUMNAR_DEBUG
  {
    const double corr = objectiveCorrection();
    std::fprintf(stderr, "[optColumnar] winner superAgg=%u total=%.6f base=%.6f corr=%.6f top=%u\n", bestSuperAgg, bestL, bestL - corr, corr, m_numTopModules);
  }
#endif
  return bestL;
}

} // namespace infomap
