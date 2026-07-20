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
#include <cmath>
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
#include <cstdio>
#endif

namespace infomap {

// Co-physical (correction-proposed) candidate mode, from env COL_COMERGE:
//   off (default) | all | seeded.
// Proposing co-physical merge candidates is redundant with the edge-based
// candidate set on UNDIRECTED clustering (co-physical state nodes are 2-hop
// connected there) but may matter on DIRECTED clustering, where co-physical
// modules need not be edge-adjacent. Kept as a tuning knob for that case:
//   0 = off, 1 = all move-loop phases + merge, 2 = seeded phases only.
static int coMergeMode()
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

void ColumnarMapEquation::buildFromTree(const InfoNode& root, const std::vector<InfoNode*>& leafNodes, bool undirected)
{
  m_undirected = undirected;
  const std::size_t nLeaves = leafNodes.size();

  m_leafFlow.assign(nLeaves, 0.0);
  std::unordered_map<const InfoNode*, int> leafId;
  leafId.reserve(nLeaves * 2);
  for (std::size_t i = 0; i < nLeaves; ++i) {
    m_leafFlow[i] = leafNodes[i]->data.flow;
    leafId[leafNodes[i]] = static_cast<int>(i);
  }

  // DFS over internal nodes (pre-order): assign module ids, depth, parent.
  // A node is always popped after its parent, so the parent id is known.
  m_modFlow.clear();
  m_modEnter.clear();
  m_modExit.clear();
  m_modParent.clear();
  m_modDepth.clear();
  m_modLeafModule.clear();
  std::vector<const InfoNode*> modNode;
  std::unordered_map<const InfoNode*, int> modId;

  std::vector<std::pair<const InfoNode*, int>> stack;
  stack.emplace_back(&root, 0);
  while (!stack.empty()) {
    const InfoNode* node = stack.back().first;
    const int depth = stack.back().second;
    stack.pop_back();

    const int id = static_cast<int>(m_modFlow.size());
    modId[node] = id;
    m_modFlow.push_back(0.0);
    m_modEnter.push_back(0.0);
    m_modExit.push_back(0.0);
    m_modDepth.push_back(depth);

    int par = -1;
    if (node->parent != nullptr) {
      auto it = modId.find(node->parent);
      if (it != modId.end())
        par = it->second;
    }
    m_modParent.push_back(par);
    m_modLeafModule.push_back(node->firstChild != nullptr && node->firstChild->isLeaf() ? 1 : 0);
    modNode.push_back(node);

    for (const auto& child : *node) {
      if (!child.isLeaf())
        stack.emplace_back(&child, depth + 1);
    }
  }
  const int nMod = static_cast<int>(m_modFlow.size());

  // Children as CSR (second pass: all ids now assigned).
  m_childStart.assign(nMod + 1, 0);
  for (int m = 0; m < nMod; ++m) {
    int cnt = 0;
    for (const auto& child : *modNode[m]) {
      (void)child;
      ++cnt;
    }
    m_childStart[m + 1] = m_childStart[m] + cnt;
  }
  m_childList.assign(static_cast<std::size_t>(m_childStart[nMod]), 0);
  for (int m = 0; m < nMod; ++m) {
    int pos = m_childStart[m];
    const bool leafMod = m_modLeafModule[m] != 0;
    for (const auto& child : *modNode[m]) {
      m_childList[pos++] = leafMod ? leafId.at(&child) : modId.at(&child);
    }
  }

  // Leaf -> parent module id.
  m_leafParentMod.assign(nLeaves, -1);
  for (std::size_t i = 0; i < nLeaves; ++i) {
    m_leafParentMod[i] = modId.at(leafNodes[i]->parent);
  }

  aggregate(leafNodes);
}

void ColumnarMapEquation::aggregate(const std::vector<InfoNode*>& leafNodes)
{
  const int nMod = static_cast<int>(m_modFlow.size());
  std::fill(m_modFlow.begin(), m_modFlow.end(), 0.0);
  std::fill(m_modEnter.begin(), m_modEnter.end(), 0.0);
  std::fill(m_modExit.begin(), m_modExit.end(), 0.0);

  // Module flow: deepest first so children are summed before their parents.
  std::vector<int> order(nMod);
  std::iota(order.begin(), order.end(), 0);
  std::sort(order.begin(), order.end(), [this](int a, int b) { return m_modDepth[a] > m_modDepth[b]; });
  for (int m : order) {
    const bool leafMod = m_modLeafModule[m] != 0;
    double f = 0.0;
    for (int k = m_childStart[m]; k < m_childStart[m + 1]; ++k) {
      const int c = m_childList[k];
      f += leafMod ? m_leafFlow[c] : m_modFlow[c];
    }
    m_modFlow[m] = f;
  }

  // Enter/exit flow: walk each leaf out-edge from the two endpoints' modules up
  // to their lowest common ancestor, matching aggregateFlowValuesFromLeafToRoot.
  const std::size_t nLeaves = leafNodes.size();
  std::unordered_map<const InfoNode*, int> leafId;
  leafId.reserve(nLeaves * 2);
  for (std::size_t i = 0; i < nLeaves; ++i)
    leafId[leafNodes[i]] = static_cast<int>(i);

  for (std::size_t i = 0; i < nLeaves; ++i) {
    InfoNode* s = leafNodes[i];
    for (InfoEdge* e : s->outEdges()) {
      auto tIt = leafId.find(e->target);
      if (tIt == leafId.end())
        continue; // target not a leaf in this set (shouldn't happen)
      int a = m_leafParentMod[i];
      int b = m_leafParentMod[tIt->second];
      if (a == b)
        continue; // intra-module edge: no contribution to module enter/exit

      const double f = e->data.flow;
      const double half = f / 2.0;
      while (m_modDepth[a] > m_modDepth[b]) {
        if (m_undirected) {
          m_modExit[a] += half;
          m_modEnter[a] += half;
        } else {
          m_modExit[a] += f;
        }
        a = m_modParent[a];
      }
      while (m_modDepth[b] > m_modDepth[a]) {
        if (m_undirected) {
          m_modEnter[b] += half;
          m_modExit[b] += half;
        } else {
          m_modEnter[b] += f;
        }
        b = m_modParent[b];
      }
      while (a != b) {
        if (m_undirected) {
          m_modExit[a] += half;
          m_modEnter[a] += half;
          m_modEnter[b] += half;
          m_modExit[b] += half;
        } else {
          m_modExit[a] += f;
          m_modEnter[b] += f;
        }
        a = m_modParent[a];
        b = m_modParent[b];
      }
    }
  }
}

double ColumnarMapEquation::moduleTerm(int m) const
{
  using infomath::plogp;
  if (m_modLeafModule[m] != 0) {
    // Module of leaf nodes: index codebook over child flow + module exit.
    const double T = m_modFlow[m] + m_modExit[m];
    if (T < 1e-16)
      return 0.0;
    double indexLength = 0.0;
    for (int k = m_childStart[m]; k < m_childStart[m + 1]; ++k)
      indexLength -= plogp(m_leafFlow[m_childList[k]] / T);
    indexLength -= plogp(m_modExit[m] / T);
    return indexLength * T;
  }
  // Module of modules: exit to coarser level or enter one of the children.
  if (m_modFlow[m] < 1e-16)
    return 0.0;
  double sumEnter = 0.0;
  double sumEnterLogEnter = 0.0;
  for (int k = m_childStart[m]; k < m_childStart[m + 1]; ++k) {
    const double en = m_modEnter[m_childList[k]];
    sumEnter += en;
    sumEnterLogEnter += plogp(en);
  }
  const double totalCodewordUse = m_modExit[m] + sumEnter;
  return plogp(totalCodewordUse) - sumEnterLogEnter - plogp(m_modExit[m]);
}

double ColumnarMapEquation::hierarchicalCodelength() const
{
  double total = 0.0;
  const int nMod = static_cast<int>(m_modFlow.size());
  for (int m = 0; m < nMod; ++m)
    total += moduleTerm(m);
  return total;
}

unsigned int ColumnarMapEquation::numLevels() const
{
  int maxDepth = 0;
  for (int d : m_modDepth)
    maxDepth = std::max(maxDepth, d);
  // Module depths run 0 (root) .. maxDepth (deepest leaf module); the leaves sit
  // one level below, so the tree depth (matching maxTreeDepth) is maxDepth + 1.
  return static_cast<unsigned int>(maxDepth + 1);
}

// ===================================================
// ColumnarTwoLevel (M1b): columnar two-level optimizer
// ===================================================

namespace {
  // Codelength calibration constants, mirroring the Config defaults the OO core
  // uses (the columnar path is a measurement scaffold, not user-tunable).
  constexpr double kMinImprovement = 1e-10;
  constexpr double kMinSingleImprovement = 1e-16;
  constexpr unsigned int kCoreLoopLimit = 10;

  // Exact port of MapEquation::getDeltaCodelengthOnMovingNode for the base
  // objective: change in codelength from moving a unit out of its old module
  // (aggregates old*) into a candidate module (aggregates new*). deltaOld/deltaNew
  // are the summed enter+exit flow between the unit and the old/new module.
  inline double deltaCodelengthMovingNode(double enterFlow, double enterFlow_log_enterFlow, double curEnter, double curExit, double curFlow, double oldEnter, double oldExit, double oldFlow, double newEnter, double newExit, double newFlow, double deltaOld, double deltaNew)
  {
    using infomath::plogp;
    const double oldExitPlusFlow = oldExit + oldFlow;
    const double newExitPlusFlow = newExit + newFlow;

    const double delta_enter = plogp(enterFlow + deltaOld - deltaNew) - enterFlow_log_enterFlow;
    const double delta_enter_log_enter = -plogp(oldEnter) - plogp(newEnter)
        + plogp(oldEnter - curEnter + deltaOld) + plogp(newEnter + curEnter - deltaNew);
    const double delta_exit_log_exit = -plogp(oldExit) - plogp(newExit)
        + plogp(oldExit - curExit + deltaOld) + plogp(newExit + curExit - deltaNew);
    const double delta_flow_log_flow = -plogp(oldExitPlusFlow) - plogp(newExitPlusFlow)
        + plogp(oldExitPlusFlow - curExit - curFlow + deltaOld) + plogp(newExitPlusFlow + curExit + curFlow - deltaNew);

    return delta_enter - delta_enter_log_enter - delta_exit_log_exit + delta_flow_log_flow;
  }

  // Old-module plogp terms of deltaCodelengthMovingNode: constant for every
  // candidate the move loop probes for the same unit, so the caller computes
  // them once per unit (6 of the 13 plogp calls per candidate hoist out; the
  // per-candidate math below keeps the exact expression structure, so results
  // stay bit-identical to the unhoisted form).
  struct OldSideTerms {
    double plogpOldEnter, plogpOldExit, plogpOldEF;
    double plogpOldEnterAfter, plogpOldExitAfter, plogpOldEFAfter;
  };

  inline OldSideTerms hoistOldSide(double curEnter, double curExit, double curFlow, double oldEnter, double oldExit, double oldFlow, double deltaOld)
  {
    using infomath::plogp;
    const double oldExitPlusFlow = oldExit + oldFlow;
    return { plogp(oldEnter), plogp(oldExit), plogp(oldExitPlusFlow), plogp(oldEnter - curEnter + deltaOld), plogp(oldExit - curExit + deltaOld), plogp(oldExitPlusFlow - curExit - curFlow + deltaOld) };
  }

  inline double deltaCodelengthMovingNodeHoisted(double enterFlow, double enterFlow_log_enterFlow, double curEnter, double curExit, double curFlow, const OldSideTerms& o, double newEnter, double newExit, double newFlow, double deltaOld, double deltaNew)
  {
    using infomath::plogp;
    const double newExitPlusFlow = newExit + newFlow;

    const double delta_enter = plogp(enterFlow + deltaOld - deltaNew) - enterFlow_log_enterFlow;
    const double delta_enter_log_enter = -o.plogpOldEnter - plogp(newEnter)
        + o.plogpOldEnterAfter + plogp(newEnter + curEnter - deltaNew);
    const double delta_exit_log_exit = -o.plogpOldExit - plogp(newExit)
        + o.plogpOldExitAfter + plogp(newExit + curExit - deltaNew);
    const double delta_flow_log_flow = -o.plogpOldEF - plogp(newExitPlusFlow)
        + o.plogpOldEFAfter + plogp(newExitPlusFlow + curExit + curFlow - deltaNew);

    return delta_enter - delta_enter_log_enter - delta_exit_log_exit + delta_flow_log_flow;
  }

  // Recorded-teleportation (tele-path) analogue of OldSideTerms/hoistOldSide:
  // the six old-module (A-side) plogp terms plus the A-side shift of enterFlow
  // are constant across every candidate the move loop probes for the same unit,
  // so the caller hoists them once per unit visit. moduleTeleEnter/moduleTeleExit
  // are inlined here as free lambdas (these helpers live in the class); the
  // per-candidate expression structure below matches deltaCodelengthMovingNodeTele
  // exactly, so results stay bit-identical to the unhoisted form.
  struct TeleOldSideTerms {
    double plogpEA0, plogpXA0, plogpFA0; // before the move
    double plogpEA1, plogpXA1, plogpFA1; // after
    double deltaEnterA; // eA1 - eA0, the A-side contribution to enterFlow1
  };

  inline TeleOldSideTerms hoistOldSideTele(double curEnter, double curExit, double curFlow, double tfu, double twu, double oldEnter, double oldExit, double oldFlow, double oldTeleFlow, double oldTeleWeight, double totalTeleFlow, double deltaOld)
  {
    using infomath::plogp;
    const auto teleEnter = [&](double tf, double tw) { return (totalTeleFlow - tf) * tw; };
    const auto teleExit = [&](double tf, double tw) { return tf * (1.0 - tw); };
    const double eA0 = oldEnter + teleEnter(oldTeleFlow, oldTeleWeight);
    const double xA0 = oldExit + teleExit(oldTeleFlow, oldTeleWeight);
    const double fA0 = xA0 + oldFlow;
    const double eA1 = (oldEnter - curEnter + deltaOld) + teleEnter(oldTeleFlow - tfu, oldTeleWeight - twu);
    const double xA1 = (oldExit - curExit + deltaOld) + teleExit(oldTeleFlow - tfu, oldTeleWeight - twu);
    const double fA1 = xA1 + (oldFlow - curFlow);
    return { plogp(eA0), plogp(xA0), plogp(fA0), plogp(eA1), plogp(xA1), plogp(fA1), eA1 - eA0 };
  }

  inline double deltaCodelengthMovingNodeTeleHoisted(double enterFlow, double enterFlow_log_enterFlow, double curEnter, double curExit, double curFlow, double tfu, double twu, const TeleOldSideTerms& o, double newEnter, double newExit, double newFlow, double newTeleFlow, double newTeleWeight, double totalTeleFlow, double deltaNew)
  {
    using infomath::plogp;
    const auto teleEnter = [&](double tf, double tw) { return (totalTeleFlow - tf) * tw; };
    const auto teleExit = [&](double tf, double tw) { return tf * (1.0 - tw); };
    const double eB0 = newEnter + teleEnter(newTeleFlow, newTeleWeight);
    const double xB0 = newExit + teleExit(newTeleFlow, newTeleWeight);
    const double fB0 = xB0 + newFlow;
    const double eB1 = (newEnter + curEnter - deltaNew) + teleEnter(newTeleFlow + tfu, newTeleWeight + twu);
    const double xB1 = (newExit + curExit - deltaNew) + teleExit(newTeleFlow + tfu, newTeleWeight + twu);
    const double fB1 = xB1 + (newFlow + curFlow);

    const double enterFlow1 = enterFlow + o.deltaEnterA + (eB1 - eB0);
    const double d_enter = plogp(enterFlow1) - enterFlow_log_enterFlow;
    const double d_enter_log = (o.plogpEA1 + plogp(eB1)) - (o.plogpEA0 + plogp(eB0));
    const double d_exit_log = (o.plogpXA1 + plogp(xB1)) - (o.plogpXA0 + plogp(xB0));
    const double d_flow_log = (o.plogpFA1 + plogp(fB1)) - (o.plogpFA0 + plogp(fB0));
    return d_enter - d_enter_log - d_exit_log + d_flow_log;
  }
} // namespace

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

  Level& L = m_leaf0;
  L.n = m_nLeaves;
  L.flow.resize(m_nLeaves);
  L.enter.resize(m_nLeaves);
  L.exit.resize(m_nLeaves);
  L.teleFlow.resize(m_nLeaves);
  L.teleWeight.resize(m_nLeaves);
  m_leafFlow.resize(m_nLeaves);
  m_leafTeleFlow.resize(m_nLeaves);
  m_leafTeleWeight.resize(m_nLeaves);
  m_totalTeleFlow = 0.0;
  m_leafNodeFlow_log_nodeFlow = 0.0;
  for (int i = 0; i < m_nLeaves; ++i) {
    L.flow[i] = leafNodes[i]->data.flow;
    L.enter[i] = leafNodes[i]->data.enterFlow;
    L.exit[i] = leafNodes[i]->data.exitFlow;
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

void ColumnarTwoLevel::buildFromLevel(const Level& level, bool undirected, unsigned long seed, double exitNetworkFlow, bool recordedTeleport, double globalTotalTeleFlow)
{
  using infomath::plogp;
  m_undirected = undirected;
  m_seed = seed;
  m_exitNetworkFlow = exitNetworkFlow;
  m_nLeaves = level.n;
  m_leaf0 = level;
  m_leafFlow = level.flow;
  // Inherit the GLOBAL teleport context (the level already carries per-unit
  // teleFlow/teleWeight; the total stays the whole network's, not this level's).
  m_recordedTeleport = recordedTeleport;
  m_totalTeleFlow = globalTotalTeleFlow;
  m_leafTeleFlow = level.teleFlow;
  m_leafTeleWeight = level.teleWeight;
  m_leafNodeFlow_log_nodeFlow = 0.0;
  for (double f : level.flow)
    m_leafNodeFlow_log_nodeFlow += plogp(f);
}

void ColumnarTwoLevel::initPartition()
{
  using infomath::plogp;
  const int n = m_lvl.n;
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
    m_mFlow[i] = m_lvl.flow[i];
    m_mEnter[i] = m_lvl.enter[i];
    m_mExit[i] = m_lvl.exit[i];
    double enter = m_lvl.enter[i];
    double exit = m_lvl.exit[i];
    if (m_recordedTeleport) {
      m_mTeleFlow[i] = m_lvl.teleFlow[i];
      m_mTeleWeight[i] = m_lvl.teleWeight[i];
      enter += moduleTeleEnter(m_lvl.teleFlow[i], m_lvl.teleWeight[i]);
      exit += moduleTeleExit(m_lvl.teleFlow[i], m_lvl.teleWeight[i]);
    }
    m_flow_log_flow += plogp(m_lvl.flow[i] + exit);
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
  for (int k = m_lvl.outStart[u]; k < m_lvl.outStart[u + 1]; ++k) {
    const int m = m_module[m_lvl.outTarget[k]];
    if (m == cMod)
      dExitOld += m_lvl.outFlow[k];
    else if (m == newMod)
      dExitNew += m_lvl.outFlow[k];
  }
  for (int k = m_lvl.inStart[u]; k < m_lvl.inStart[u + 1]; ++k) {
    const int m = m_module[m_lvl.inTarget[k]];
    if (m == cMod)
      dEnterOld += m_lvl.inFlow[k];
    else if (m == newMod)
      dEnterNew += m_lvl.inFlow[k];
  }
  const double deltaOld = dEnterOld + dExitOld;
  const double deltaNew = dEnterNew + dExitNew;
  const double curEnter = m_lvl.enter[u];
  const double curExit = m_lvl.exit[u];
  const double curFlow = m_lvl.flow[u];

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
    const double tfu = m_lvl.teleFlow[u], twu = m_lvl.teleWeight[u];
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
  const int n = m_lvl.n;
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
  for (int u = 0; u < m_lvl.n; ++u)
    moveUnit(u, assign[u]);
  m_deferTerms = false;
  rebuildRunningTerms();
  // Rebuild the empty-module list for the subsequent optimizing move loop.
  m_emptyModules.clear();
  for (int m = 0; m < m_lvl.n; ++m)
    if (m_mMembers[m] == 0)
      m_emptyModules.push_back(m);
}

unsigned int ColumnarTwoLevel::moveLoop()
{
  using infomath::plogp;
  const int n = m_lvl.n;

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
  // loop (units == leaves here, so unit id == leaf id). Their contribution is
  // tracked alongside the base codelength so move selection and convergence use
  // the true augmented objective. O(1) per candidate per correction.
  std::vector<ColumnarCorrection*> corr;
  double correctionTotal = 0.0;
  if (m_leafMoveLoop) {
    for (auto& c : m_corrections)
      if (c->participatesInMoveLoop())
        corr.push_back(c.get());
    for (auto* c : corr)
      correctionTotal += c->initMoveLoop(m_module, n);
  }

  // Co-physical move-loop candidates (see coMergeMode): "seeded" restricts them
  // to seeded phases (fine-tune / refine), where base structure already exists.
  const int coMode = coMergeMode();
  const bool proposeExtra = !corr.empty()
      && (coMode == 1 || (coMode == 2 && m_seededPhase));

  double oldCodelength = m_codelength + correctionTotal;
  unsigned int numEffectiveLoops = 0;
  unsigned int coreLoopCount = 0;
  const unsigned int loopLimit = kCoreLoopLimit;

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
      for (int k = m_lvl.outStart[u]; k < m_lvl.outStart[u + 1]; ++k) {
        const int m = m_module[m_lvl.outTarget[k]];
        touch(m);
        dExit[m] += m_lvl.outFlow[k];
      }
      for (int k = m_lvl.inStart[u]; k < m_lvl.inStart[u + 1]; ++k) {
        const int m = m_module[m_lvl.inTarget[k]];
        touch(m);
        dEnter[m] += m_lvl.inFlow[k];
      }
      // Ensure own module is a candidate (the "don't move" option).
      if (dEnter[cMod] == 0.0 && dExit[cMod] == 0.0)
        touched.push_back(cMod);

      const double deltaOld = dEnter[cMod] + dExit[cMod];
      const double curEnter = m_lvl.enter[u];
      const double curExit = m_lvl.exit[u];
      const double curFlow = m_lvl.flow[u];

      int bestMod = cMod;
      double bestDelta = 0.0;
      int strongestMod = cMod;
      double strongestExit = 0.0;
      double strongestDelta = 0.0;

      // Option to move to an empty module (if not already alone).
      if (m_mMembers[cMod] > 1 && !m_emptyModules.empty()) {
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
      const double tfu = tele ? m_lvl.teleFlow[u] : 0.0;
      const double twu = tele ? m_lvl.teleWeight[u] : 0.0;
      const OldSideTerms oldSide = tele
          ? OldSideTerms{}
          : hoistOldSide(curEnter, curExit, curFlow, m_mEnter[cMod], m_mExit[cMod], m_mFlow[cMod], deltaOld);
      const TeleOldSideTerms teleOldSide = tele
          ? hoistOldSideTele(curEnter, curExit, curFlow, tfu, twu, m_mEnter[cMod], m_mExit[cMod], m_mFlow[cMod], m_mTeleFlow[cMod], m_mTeleWeight[cMod], m_totalTeleFlow, deltaOld)
          : TeleOldSideTerms{};
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
        for (int k = m_lvl.outStart[u]; k < m_lvl.outStart[u + 1]; ++k)
          dirty[m_lvl.outTarget[k]] = 1;
        for (int k = m_lvl.inStart[u]; k < m_lvl.inStart[u + 1]; ++k)
          dirty[m_lvl.inTarget[k]] = 1;
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
  const int n = m_lvl.n;
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
  // Module aggregates already hold the consolidated flow data. enter/exit stay
  // link-based (the teleport term is rebuilt from teleFlow/teleWeight by the
  // next level's initPartition), matching how m_leaf0 stores link enter/exit.
  // m_mTeleFlow/m_mTeleWeight are only populated under recorded teleportation.
  for (int oldM = 0; oldM < n; ++oldM) {
    if (m_mMembers[oldM] == 0)
      continue;
    const int m = remap[oldM];
    next.flow[m] = m_mFlow[oldM];
    next.enter[m] = m_mEnter[oldM];
    next.exit[m] = m_mExit[oldM];
    if (m_recordedTeleport) {
      next.teleFlow[m] = m_mTeleFlow[oldM];
      next.teleWeight[m] = m_mTeleWeight[oldM];
    }
  }

  // Aggregate current-level out-edges into module-module edges.
  std::unordered_map<long long, double> edgeMap;
  edgeMap.reserve(m_lvl.outTarget.size());
  for (int a = 0; a < n; ++a) {
    const int ma = remap[m_module[a]];
    for (int k = m_lvl.outStart[a]; k < m_lvl.outStart[a + 1]; ++k) {
      const int mb = remap[m_module[m_lvl.outTarget[k]]];
      if (ma == mb)
        continue;
      edgeMap[static_cast<long long>(ma) * K + mb] += m_lvl.outFlow[k];
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

  m_lvl = std::move(next);
  return K;
}

double ColumnarTwoLevel::optimizeTwoLevel(unsigned int maxAggPasses, bool doFineTune)
{
  // Start each optimize from the immutable leaf network. The first move loop
  // (and any fine-tune) operate on leaves, so objective corrections are active;
  // aggregation passes operate on modules, where they are not.
  m_lvl = m_leaf0;
  m_leafMoveLoop = true;
  m_seededPhase = false; // aggregation starts from singletons
  m_leafTop.resize(m_nLeaves);
  for (int i = 0; i < m_nLeaves; ++i)
    m_leafTop[i] = i;

  double bestCodelength = std::numeric_limits<double>::infinity();
  std::vector<int> bestTop = m_leafTop;
  unsigned int bestK = static_cast<unsigned int>(m_lvl.n);

  bool first = true;
  unsigned int pass = 0;
  while (true) {
    pollInterrupt();
    ++pass;
    initPartition();
    moveLoop();

    // Compose leaf -> module (compacted) for this pass.
    std::vector<int> remap(m_lvl.n, -1);
    int c = 0;
    for (int i = 0; i < m_lvl.n; ++i)
      if (remap[m_module[i]] == -1)
        remap[m_module[i]] = c++;
    std::vector<int> newTop(m_nLeaves);
    for (int i = 0; i < m_nLeaves; ++i)
      newTop[i] = remap[m_module[m_leafTop[i]]];

    // Select by the augmented objective (base + correction of leaves -> current
    // modules), not base alone: for Mem the correction is a large fraction of
    // the codelength, so base-only selection optimizes the wrong quantity. The
    // base two-level codelength is invariant under aggregation, so m_codelength
    // is the base term for leaves -> current top modules; add that partition's
    // correction (recomputed, since module passes don't track the leaf term).
    const double L = m_codelength + augmentedCorrectionFor(newTop, c);

    const bool improved = L < bestCodelength - kMinImprovement;
    if (first || improved) {
      first = false;
      bestTop = std::move(newTop);
      bestCodelength = L;
      bestK = static_cast<unsigned int>(c);

      if (c <= 1 || c == m_lvl.n)
        break;
      if (maxAggPasses != 0 && pass >= maxAggPasses)
        break; // stop early: keep this (finer) level as the building-block bottom
      consolidateToNextLevel();
      m_leafMoveLoop = false; // subsequent passes aggregate modules, not leaves
    } else {
      break;
    }
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
    m_lvl = m_leaf0;
    m_leafMoveLoop = true; // fine-tune re-optimizes leaves
    m_seededPhase = true; // seeded at the current partition
    seedAssignment(m_leafTop);
    moveLoop();
    // Fine-tune re-optimizes leaves, so m_lastCorrection is exactly this
    // partition's correction; select by the augmented objective.
    const double augL = m_codelength + m_lastCorrection;
    if (augL >= bestCodelength - kMinImprovement)
      break;
    std::vector<int> remap(m_lvl.n, -1);
    int c = 0;
    for (int i = 0; i < m_lvl.n; ++i)
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
  m_hierLevels.push_back(m_leaf0);
  m_hierAssign.push_back(m_leafTop);
  m_hierLevels.push_back(aggregateLevel(m_leaf0, m_leafTop, static_cast<int>(m_numTopModules), m_undirected));

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
  // structure recovers most of the remaining gap to the OO -2 optimum
  // (air30k best-of-10: 5.605 -> 5.460 vs OO 5.394), and the tuned partition
  // can enable further merges. Alternate until the pair stops improving. The
  // base objective never enters (its merge is a no-op, so the loop condition
  // fails immediately and the already-converged fine-tune is not repeated).
  for (int round = 0; round < 100 && L < beforeMerge - kMinImprovement; ++round) {
    const bool tuned = retuneLeavesWithinModules(L);
    beforeMerge = L;
    coarsenModules(L, 1000);
    if (!tuned && L >= beforeMerge - kMinImprovement)
      break;
  }
  return L;
}

bool ColumnarTwoLevel::retuneLeavesWithinModules(double& L)
{
  // Seeded leaf fine-tune across the current module level: re-run the leaf
  // move loop (corrections active, co-physical proposals enabled by the
  // seeded phase) from the current assignment, rebuild the module level, and
  // keep the result only if it lowers the true stack codelength.
  std::vector<int> savedTop = m_hierAssign[0];
  const int savedK = m_hierLevels[1].n;

  m_lvl = m_leaf0;
  m_leafMoveLoop = true;
  m_seededPhase = true;
  seedAssignment(m_hierAssign[0]);
  moveLoop();

  std::vector<int> remap(m_lvl.n, -1);
  int k = 0;
  for (int i = 0; i < m_lvl.n; ++i)
    if (remap[m_module[i]] == -1)
      remap[m_module[i]] = k++;
  std::vector<int> newTop(m_nLeaves);
  for (int i = 0; i < m_nLeaves; ++i)
    newTop[i] = remap[m_module[i]];

  m_hierAssign[0] = std::move(newTop);
  m_hierLevels[1] = aggregateLevel(m_leaf0, m_hierAssign[0], k, m_undirected);
  m_numTopModules = static_cast<unsigned int>(k);
  const double tunedL = hierarchicalCodelengthFromStack();
  if (tunedL < L - kMinImprovement) {
    L = tunedL;
    return true;
  }
  m_hierAssign[0] = std::move(savedTop);
  m_hierLevels[1] = aggregateLevel(m_leaf0, m_hierAssign[0], savedK, m_undirected);
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
  m_hierLevels.push_back(m_leaf0);
  m_hierAssign.push_back(levelId[1]);
  Level cur = aggregateLevel(m_leaf0, levelId[1], levelK[1], m_undirected);
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

ColumnarTwoLevel::Level ColumnarTwoLevel::aggregateLevel(const Level& base, const std::vector<int>& assign, int K, bool undirected)
{
  Level out;
  out.n = K;
  out.flow.assign(K, 0.0);
  out.enter.assign(K, 0.0);
  out.exit.assign(K, 0.0);
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
        out.exit[ga] += half;
        out.enter[ga] += half;
        out.enter[gb] += half;
        out.exit[gb] += half;
      } else {
        out.exit[ga] += f;
        out.enter[gb] += f;
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
  m_hierLevels.push_back(m_leaf0);
  m_hierAssign.push_back(m_leafTop);
  Level cur = aggregateLevel(m_leaf0, m_leafTop, bottomK, m_undirected);
  m_hierLevels.push_back(cur);

  // Grow up with the enter-flow super-search while it shortens the index code.
  while (cur.n > 1) {
    pollInterrupt();
    // Enter-flow transform: the super-network's node flow is the module's enter
    // flow; enter/exit and edges are the module's inter-module quantities.
    Level superNet = cur;
    superNet.flow = cur.enter;

    // Current top-level index codebook (flat): plogp(sum enter) - sum plogp(enter).
    double sumEnter = 0.0, sumPlogpEnter = 0.0;
    for (double e : cur.enter) {
      sumEnter += e;
      sumPlogpEnter += plogp(e);
    }
    const double curIndexCodelength = plogp(sumEnter) - sumPlogpEnter;

    ColumnarTwoLevel superOpt;
    superOpt.setInterruptCallback(m_interruptCallback);
    superOpt.buildFromLevel(superNet, m_undirected, m_seed, 0.0, m_recordedTeleport, m_totalTeleFlow);
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

double ColumnarTwoLevel::hierarchicalCodelengthFromStack() const
{
  using infomath::plogp;
  const int topLevel = static_cast<int>(m_hierLevels.size()) - 1; // >= 1
  double total = 0.0;

  // Recorded-teleportation enter/exit additions per module level (1..topLevel).
  // A module gains teleport exit = teleFlow_m * (1 - teleWeight_m) and teleport
  // enter = (totalTele - teleFlow_m) * teleWeight_m, from its members' aggregated
  // teleport flow/weight -- exactly InfomapBase::aggregateFlowValuesFromLeafToRoot's
  // recorded-teleportation pass. Empty (and skipped) for the base flow model.
  std::vector<std::vector<double>> teleEnter, teleExit;
  if (m_recordedTeleport) {
    teleEnter.resize(topLevel + 1);
    teleExit.resize(topLevel + 1);
    std::vector<int> leafToK = m_hierAssign[0]; // leaf -> level-1 module
    for (int k = 1; k <= topLevel; ++k) {
      const int n = m_hierLevels[k].n;
      std::vector<double> tf(n, 0.0), tw(n, 0.0);
      for (int i = 0; i < m_nLeaves; ++i) {
        tf[leafToK[i]] += m_leafTeleFlow[i];
        tw[leafToK[i]] += m_leafTeleWeight[i];
      }
      teleEnter[k].assign(n, 0.0);
      teleExit[k].assign(n, 0.0);
      for (int m = 0; m < n; ++m) {
        teleEnter[k][m] = (m_totalTeleFlow - tf[m]) * tw[m];
        teleExit[k][m] = tf[m] * (1.0 - tw[m]);
      }
      if (k < topLevel) {
        const std::vector<int>& a = m_hierAssign[k]; // level-k -> level-(k+1)
        for (int i = 0; i < m_nLeaves; ++i)
          leafToK[i] = a[leafToK[i]];
      }
    }
  }
  auto exitAug = [&](int k, int m) { return m_hierLevels[k].exit[m] + (m_recordedTeleport ? teleExit[k][m] : 0.0); };
  auto enterAug = [&](int k, int m) { return m_hierLevels[k].enter[m] + (m_recordedTeleport ? teleEnter[k][m] : 0.0); };

  // Level-1 modules code their leaf children (module-of-leaf-nodes term).
  {
    const Level& L1 = m_hierLevels[1];
    const std::vector<int>& leafToL1 = m_hierAssign[0];
    std::vector<double> T(L1.n);
    for (int m = 0; m < L1.n; ++m)
      T[m] = L1.flow[m] + exitAug(1, m);
    std::vector<double> acc(L1.n, 0.0);
    for (int i = 0; i < m_leaf0.n; ++i) {
      const int m = leafToL1[i];
      if (T[m] >= 1e-16)
        acc[m] -= plogp(m_leaf0.flow[i] / T[m]);
    }
    for (int m = 0; m < L1.n; ++m) {
      if (T[m] < 1e-16)
        continue;
      acc[m] -= plogp(exitAug(1, m) / T[m]);
      total += acc[m] * T[m];
    }
  }

  // Higher module levels code their module children (module-of-modules term).
  for (int lvl = 2; lvl <= topLevel; ++lvl) {
    const Level& Lk = m_hierLevels[lvl];
    const Level& Lkm1 = m_hierLevels[lvl - 1];
    const std::vector<int>& childToParent = m_hierAssign[lvl - 1];
    std::vector<double> sumEnter(Lk.n, 0.0), sumPlogpEnter(Lk.n, 0.0);
    for (int c = 0; c < Lkm1.n; ++c) {
      const int p = childToParent[c];
      const double ec = enterAug(lvl - 1, c);
      sumEnter[p] += ec;
      sumPlogpEnter[p] += plogp(ec);
    }
    for (int m = 0; m < Lk.n; ++m) {
      if (Lk.flow[m] < 1e-16)
        continue;
      const double ex = exitAug(lvl, m);
      const double totalUse = ex + sumEnter[m];
      total += plogp(totalUse) - sumPlogpEnter[m] - plogp(ex);
    }
  }

  // Root codes the topmost modules (exit of the whole network is 0).
  {
    const Level& Ltop = m_hierLevels[topLevel];
    double sumEnter = 0.0, sumPlogpEnter = 0.0;
    for (int m = 0; m < Ltop.n; ++m) {
      const double e = enterAug(topLevel, m);
      sumEnter += e;
      sumPlogpEnter += plogp(e);
    }
    total += plogp(sumEnter) - sumPlogpEnter;
  }

  return total + objectiveCorrection();
}

double ColumnarTwoLevel::augmentedCorrectionFor(const std::vector<int>& leafAssign, int numModules)
{
  double c = 0.0;
  for (auto& corr : m_corrections)
    if (corr->participatesInMoveLoop())
      c += corr->initMoveLoop(leafAssign, numModules);
  return c;
}

double ColumnarTwoLevel::objectiveCorrection() const
{
  if (m_corrections.empty() || m_hierLevels.empty())
    return 0.0;
  double sum = 0.0;
  for (const auto& correction : m_corrections)
    sum += correction->hierarchicalCorrection(*this);
  return sum;
}

double BiasedEntropyCorrection::hierarchicalCorrection(const ColumnarTwoLevel& core) const
{
  // Sum of childDegree over all internal nodes incl. root == count of non-root
  // nodes == sum of every level's size (leaves + modules at all levels).
  long long nonRootNodes = 0;
  const unsigned int levels = core.hierNumLevels();
  for (unsigned int k = 0; k < levels; ++k)
    nonRootNodes += core.hierLevelSize(static_cast<int>(k));
  return m_multiplier * static_cast<double>(nonRootNodes) / (2.0 * m_totalDegree);
}

// ===================================================
// PreferredModulesCorrection (--preferred-number-of-modules: |K - K_pref| bias)
// ===================================================

double PreferredModulesCorrection::hierarchicalCorrection(const ColumnarTwoLevel& core) const
{
  // Leaf-module level, matching where the move-loop bias acts (and the other
  // corrections apply). <2 levels means no modules; the base handles that and
  // the one-level fallback would collapse it anyway.
  if (core.hierNumLevels() < 2)
    return 0.0;
  return cost(core.hierLevelSize(1));
}

double PreferredModulesCorrection::initMoveLoop(const std::vector<int>& leafModule, int numModules)
{
  m_moduleMembers.assign(numModules, 0);
  for (int m : leafModule)
    ++m_moduleMembers[m];
  m_numNonEmpty = 0;
  for (int c : m_moduleMembers)
    if (c > 0)
      ++m_numNonEmpty;
  return cost(m_numNonEmpty);
}

double PreferredModulesCorrection::moveDelta(int /*leaf*/, int oldMod, int newMod) const
{
  if (oldMod == newMod)
    return 0.0;
  // K changes only when the move empties the old module or fills an empty new
  // one (exactly BiasedMapEquation::getDeltaNumModulesIfMoving).
  const int deltaK = (m_moduleMembers[oldMod] == 1 ? -1 : 0) + (m_moduleMembers[newMod] == 0 ? 1 : 0);
  if (deltaK == 0)
    return 0.0;
  return cost(m_numNonEmpty + deltaK) - cost(m_numNonEmpty);
}

void PreferredModulesCorrection::applyMove(int /*leaf*/, int oldMod, int newMod)
{
  if (oldMod == newMod)
    return;
  if (m_moduleMembers[newMod] == 0)
    ++m_numNonEmpty;
  ++m_moduleMembers[newMod];
  --m_moduleMembers[oldMod];
  if (m_moduleMembers[oldMod] == 0)
    --m_numNonEmpty;
}

double PreferredModulesCorrection::mergeDelta(int a, int b) const
{
  // Folding module A into B drops K by one only when both are non-empty.
  const int deltaK = (m_moduleMembers[a] > 0 && m_moduleMembers[b] > 0) ? -1 : 0;
  if (deltaK == 0)
    return 0.0;
  return cost(m_numNonEmpty + deltaK) - cost(m_numNonEmpty);
}

void PreferredModulesCorrection::applyMerge(int a, int b)
{
  if (m_moduleMembers[a] == 0)
    return;
  if (m_moduleMembers[b] == 0) {
    // Non-empty A into empty B: B gains A's members as A empties — K unchanged.
    m_moduleMembers[b] = m_moduleMembers[a];
  } else {
    m_moduleMembers[b] += m_moduleMembers[a];
    --m_numNonEmpty;
  }
  m_moduleMembers[a] = 0;
}

double MetaCorrection::hierarchicalCorrection(const ColumnarTwoLevel& core) const
{
  using infomath::plogp;
  // Meta term applies at the leaf-module level (level 1): for each bottom
  // module, metaDataRate * F_m * (-sum_c plogp(f_c / F_m)) == metaDataRate *
  // (plogp(F_m) - sum_c plogp(f_c)), matching MetaCollection::calculateEntropy
  // summed over leaf modules.
  if (core.hierNumLevels() < 2)
    return 0.0;
  const int nLeaves = core.numLeaves();
  const int numModules = core.hierLevelSize(1);

  std::vector<double> moduleFlow(numModules, 0.0);
  std::unordered_map<long long, double> catFlow; // key = (module<<32)|category
  for (int i = 0; i < nLeaves; ++i) {
    const int m = core.hierLeafModule(i);
    moduleFlow[m] += m_leafWeight[i];
    const long long key = (static_cast<long long>(m) << 32) | static_cast<unsigned int>(m_leafCategory[i]);
    catFlow[key] += m_leafWeight[i];
  }

  double total = 0.0;
  for (int m = 0; m < numModules; ++m)
    total += plogp(moduleFlow[m]);
  for (const auto& kv : catFlow)
    total -= plogp(kv.second);
  return m_metaDataRate * total;
}

double MetaCorrection::moduleCategoryFlow(int module, int category) const
{
  const auto& cats = m_moduleCatFlow[module];
  const auto it = cats.find(category);
  return it == cats.end() ? 0.0 : it->second;
}

double MetaCorrection::initMoveLoop(const std::vector<int>& leafModule, int numModules)
{
  using infomath::plogp;
  m_moduleFlow.assign(numModules, 0.0);
  m_moduleCatFlow.assign(numModules, {});
  const int nLeaves = static_cast<int>(leafModule.size());
  for (int i = 0; i < nLeaves; ++i) {
    const int m = leafModule[i];
    m_moduleFlow[m] += m_leafWeight[i];
    m_moduleCatFlow[m][m_leafCategory[i]] += m_leafWeight[i];
  }
  double total = 0.0;
  for (int m = 0; m < numModules; ++m)
    total += plogp(m_moduleFlow[m]);
  for (const auto& cats : m_moduleCatFlow)
    for (const auto& kv : cats)
      total -= plogp(kv.second);
  return m_metaDataRate * total;
}

double MetaCorrection::moveDelta(int leaf, int oldMod, int newMod) const
{
  using infomath::plogp;
  if (oldMod == newMod)
    return 0.0;
  const int q = m_leafCategory[leaf];
  const double w = m_leafWeight[leaf];
  // term(m) = plogp(F_m) - sum_c plogp(f_c); only F_m and category q change.
  // The old-module delta is identical for every candidate the move loop probes
  // for the same leaf, so it (and plogp(w)) is computed once and cached; a
  // candidate module lacking category q (the common case) needs no category
  // logs — plogp(0) == 0 and plogp(0 + w) == plogp(w), cached alongside.
  if (leaf != m_cacheUnit || oldMod != m_cacheOldMod) {
    const double Fo = m_moduleFlow[oldMod];
    const double fo = moduleCategoryFlow(oldMod, q);
    m_cacheUnit = leaf;
    m_cacheOldMod = oldMod;
    m_cacheDOld = (plogp(Fo - w) - plogp(Fo)) - (plogp(fo - w) - plogp(fo));
    m_cachePlogpW = plogp(w);
  }
  const double Fn = m_moduleFlow[newMod];
  const double fn = moduleCategoryFlow(newMod, q);
  const double dNew = (plogp(Fn + w) - plogp(Fn))
      - (fn == 0.0 ? m_cachePlogpW : (plogp(fn + w) - plogp(fn)));
  return m_metaDataRate * (m_cacheDOld + dNew);
}

void MetaCorrection::applyMove(int leaf, int oldMod, int newMod)
{
  if (oldMod == newMod)
    return;
  m_cacheUnit = -1; // module contents change: drop the per-leaf delta cache
  const int q = m_leafCategory[leaf];
  const double w = m_leafWeight[leaf];
  m_moduleFlow[oldMod] -= w;
  m_moduleFlow[newMod] += w;
  auto& oldCats = m_moduleCatFlow[oldMod];
  auto oit = oldCats.find(q);
  if (oit != oldCats.end()) {
    oit->second -= w;
    if (oit->second <= 1e-16)
      oldCats.erase(oit);
  }
  m_moduleCatFlow[newMod][q] += w;
}

double MetaCorrection::mergeDelta(int a, int b) const
{
  using infomath::plogp;
  // contribution(m) = metaDataRate * (plogp(F_m) - sum_c plogp(f_{m,c})).
  const double Fa = m_moduleFlow[a], Fb = m_moduleFlow[b];
  double d = plogp(Fa + Fb) - plogp(Fa) - plogp(Fb); // change in the F term
  const auto& ca = m_moduleCatFlow[a];
  const auto& cb = m_moduleCatFlow[b];
  for (const auto& kv : ca) {
    const auto it = cb.find(kv.first);
    if (it == cb.end())
      continue; // category only in A: plogp(a+0) - plogp(a) - plogp(0) == 0
    const double bv = it->second;
    d -= plogp(kv.second + bv) - plogp(kv.second) - plogp(bv); // -sum_c plogp(f_c)
  }
  return m_metaDataRate * d;
}

void MetaCorrection::applyMerge(int a, int b)
{
  m_cacheUnit = -1;
  m_moduleFlow[b] += m_moduleFlow[a];
  m_moduleFlow[a] = 0.0;
  auto& ca = m_moduleCatFlow[a];
  auto& cb = m_moduleCatFlow[b];
  for (const auto& kv : ca)
    cb[kv.first] += kv.second;
  ca.clear();
}

std::unique_ptr<ColumnarCorrection> MetaCorrection::sliceForLeaves(const std::vector<int>& globalLeafIds) const
{
  const int n = static_cast<int>(globalLeafIds.size());
  std::vector<int> category(n);
  std::vector<double> weight(n);
  for (int j = 0; j < n; ++j) {
    const int g = globalLeafIds[j];
    category[j] = m_leafCategory[g];
    weight[j] = m_leafWeight[g];
  }
  return std::make_unique<MetaCorrection>(std::move(category), std::move(weight), m_metaDataRate);
}

// ===================================================
// MemCorrection (memory / state networks: physical-node codebook)
// ===================================================

MemCorrection::MemCorrection(std::vector<int> leafPhysical, std::vector<double> leafFlow)
    : m_leafPhysical(std::move(leafPhysical)), m_leafFlow(std::move(leafFlow)), m_cState(0.0)
{
  using infomath::plogp;
  for (double f : m_leafFlow)
    m_cState += plogp(f);
  // Compact the physical ids to [0, P): every downstream structure (module
  // maps, reverse index) keys on the compact id, and the dense per-module
  // lookup (see initMoveLoop) indexes with it directly.
  std::unordered_map<int, int> compact;
  compact.reserve(m_leafPhysical.size());
  for (auto& p : m_leafPhysical) {
    const auto r = compact.emplace(p, static_cast<int>(compact.size()));
    p = r.first->second;
  }
  m_numPhys = static_cast<int>(compact.size());
}

double MemCorrection::physFlow(int module, int physical) const
{
  if (m_dense)
    return m_densePhysFlow[static_cast<std::size_t>(module) * m_numPhys + physical];
  const auto& pf = m_modulePhysFlow[module];
  const auto it = pf.find(physical);
  return it == pf.end() ? 0.0 : it->second;
}

double MemCorrection::hierarchicalCorrection(const ColumnarTwoLevel& core) const
{
  using infomath::plogp;
  if (core.hierNumLevels() < 2)
    return 0.0;
  const int nLeaves = core.numLeaves();
  std::unordered_map<long long, double> physFlowMap; // key = (module<<32)|physical
  for (int i = 0; i < nLeaves; ++i) {
    const int m = core.hierLeafModule(i);
    const long long key = (static_cast<long long>(m) << 32) | static_cast<unsigned int>(m_leafPhysical[i]);
    physFlowMap[key] += m_leafFlow[i];
  }
  double sum = 0.0;
  for (const auto& kv : physFlowMap)
    sum += plogp(kv.second);
  return m_cState - sum;
}

double MemCorrection::initMoveLoop(const std::vector<int>& leafModule, int numModules)
{
  using infomath::plogp;
  m_modulePhysFlow.assign(numModules, {});
  m_physModules.clear();
  m_cacheUnit = -1;
  // Dense per-module physical-flow lookup when it fits comfortably in memory
  // (state networks typically have far fewer physical nodes than state nodes):
  // physFlow becomes an O(1) array read in the move loop's hot path instead of
  // a hash find. The sparse maps are kept alongside for iteration (merge scan).
  m_dense = static_cast<double>(numModules) * static_cast<double>(m_numPhys) <= 8e6;
  if (m_dense)
    m_densePhysFlow.assign(static_cast<std::size_t>(numModules) * m_numPhys, 0.0);
  // The physical -> modules reverse index only feeds the co-physical proposals
  // (coMergeMode); maintaining it is pure overhead when that tuning mode is off.
  const bool maintainIndex = coMergeMode() != 0;
  const int nLeaves = static_cast<int>(leafModule.size());
  for (int i = 0; i < nLeaves; ++i) {
    const int m = leafModule[i], p = m_leafPhysical[i];
    m_modulePhysFlow[m][p] += m_leafFlow[i];
    if (m_dense)
      m_densePhysFlow[static_cast<std::size_t>(m) * m_numPhys + p] += m_leafFlow[i];
    if (maintainIndex)
      m_physModules[p].insert(m);
  }
  double sum = 0.0;
  for (const auto& pf : m_modulePhysFlow)
    for (const auto& kv : pf)
      sum += plogp(kv.second);
  return m_cState - sum;
}

double MemCorrection::moveDelta(int leaf, int oldMod, int newMod) const
{
  using infomath::plogp;
  if (oldMod == newMod)
    return 0.0;
  // term = C_state - sum plogp(physFlow); only phys p in the two modules
  // changes. The old-module part is identical for every candidate of the same
  // leaf, so it is computed once and cached (the move loop probes many
  // candidates per leaf); a candidate without the leaf's physical (the common
  // case) needs no logs at all: plogp(0) == 0 and plogp(0 + f) == plogp(f),
  // which is cached alongside.
  const int p = m_leafPhysical[leaf];
  const double f = m_leafFlow[leaf];
  if (leaf != m_cacheUnit || oldMod != m_cacheOldMod) {
    const double oc = physFlow(oldMod, p);
    m_cacheUnit = leaf;
    m_cacheOldMod = oldMod;
    m_cacheOldTerm = plogp(oc) - plogp(oc - f);
    m_cachePlogpF = plogp(f);
  }
  const double nc = physFlow(newMod, p);
  return m_cacheOldTerm + (nc == 0.0 ? -m_cachePlogpF : (plogp(nc) - plogp(nc + f)));
}

void MemCorrection::applyMove(int leaf, int oldMod, int newMod)
{
  if (oldMod == newMod)
    return;
  m_cacheUnit = -1; // module contents change: drop the per-leaf delta cache
  const bool maintainIndex = !m_physModules.empty() || coMergeMode() != 0;
  const int p = m_leafPhysical[leaf];
  const double f = m_leafFlow[leaf];
  auto& oldPhys = m_modulePhysFlow[oldMod];
  auto it = oldPhys.find(p);
  if (it != oldPhys.end()) {
    it->second -= f;
    if (it->second <= 1e-16) {
      oldPhys.erase(it);
      if (maintainIndex)
        m_physModules[p].erase(oldMod); // oldMod no longer holds physical p
    }
  }
  m_modulePhysFlow[newMod][p] += f;
  if (m_dense) {
    auto& ov = m_densePhysFlow[static_cast<std::size_t>(oldMod) * m_numPhys + p];
    ov -= f;
    if (ov <= 1e-16)
      ov = 0.0; // keep the zero fast path exact
    m_densePhysFlow[static_cast<std::size_t>(newMod) * m_numPhys + p] += f;
  }
  if (maintainIndex)
    m_physModules[p].insert(newMod);
}

void MemCorrection::proposeMoveTargets(int leaf, std::vector<int>& out) const
{
  // Modules already holding a state node of this leaf's physical node — the
  // co-physical merges that shrink the physical codebook but are (usually) not
  // reachable through the edge neighborhood.
  const auto it = m_physModules.find(m_leafPhysical[leaf]);
  if (it == m_physModules.end())
    return;
  for (int m : it->second)
    out.push_back(m);
}

double MemCorrection::mergeDelta(int a, int b) const
{
  using infomath::plogp;
  // contribution(m) = -sum_p plogp(physFlow_m[p]); merging folds A's physical
  // flows into B. Only physicals present in BOTH modules change the sum (a
  // physical in only one keeps its single plogp term). Iterate the (smaller)
  // A side; b-only physicals are handled when B is the A of another pair.
  const auto& pa = m_modulePhysFlow[a];
  double dSum = 0.0; // change in sum_p plogp(physFlow)
  if (m_dense) {
    const std::size_t bBase = static_cast<std::size_t>(b) * m_numPhys;
    for (const auto& kv : pa) {
      const double bv = m_densePhysFlow[bBase + kv.first];
      if (bv == 0.0)
        continue; // physical only in A: plogp(a+0) - plogp(a) - plogp(0) == 0
      dSum += plogp(kv.second + bv) - plogp(kv.second) - plogp(bv);
    }
  } else {
    const auto& pb = m_modulePhysFlow[b];
    for (const auto& kv : pa) {
      const auto it = pb.find(kv.first);
      if (it == pb.end())
        continue; // physical only in A: plogp(a+0) - plogp(a) - plogp(0) == 0
      dSum += plogp(kv.second + it->second) - plogp(kv.second) - plogp(it->second);
    }
  }
  return -dSum; // correction = C_state - sum_p plogp; delta correction = -dSum
}

void MemCorrection::applyMerge(int a, int b)
{
  m_cacheUnit = -1;
  auto& pa = m_modulePhysFlow[a];
  auto& pb = m_modulePhysFlow[b];
  for (const auto& kv : pa) {
    pb[kv.first] += kv.second;
    if (m_dense) {
      m_densePhysFlow[static_cast<std::size_t>(b) * m_numPhys + kv.first] += kv.second;
      m_densePhysFlow[static_cast<std::size_t>(a) * m_numPhys + kv.first] = 0.0;
    }
    if (!m_physModules.empty()) { // maintained only when co-physical mode is on
      auto& mods = m_physModules[kv.first];
      mods.erase(a);
      mods.insert(b);
    }
  }
  pa.clear();
}

void MemCorrection::proposeMergePartners(int module, std::vector<int>& out) const
{
  // Leaf modules sharing a physical node with `module` (co-physical merge
  // candidates the edge-based set can miss on directed clustering).
  if (module >= static_cast<int>(m_modulePhysFlow.size()))
    return;
  for (const auto& kv : m_modulePhysFlow[module]) {
    const auto it = m_physModules.find(kv.first);
    if (it == m_physModules.end())
      continue;
    for (int m : it->second)
      if (m != module)
        out.push_back(m);
  }
}

std::unique_ptr<ColumnarCorrection> MemCorrection::sliceForLeaves(const std::vector<int>& globalLeafIds) const
{
  const int n = static_cast<int>(globalLeafIds.size());
  std::vector<int> physical(n);
  std::vector<double> flow(n);
  for (int j = 0; j < n; ++j) {
    const int g = globalLeafIds[j];
    physical[j] = m_leafPhysical[g];
    flow[j] = m_leafFlow[g];
  }
  return std::make_unique<MemCorrection>(std::move(physical), std::move(flow));
}

// ===================================================
// LossyCorrection (rate-distortion map equation: noise modules)
// ===================================================

LossyCorrection::LossyCorrection(std::vector<double> leafFlow, std::vector<double> leafEntropy, double lambda)
    : m_leafFlow(std::move(leafFlow)), m_leafEntropy(std::move(leafEntropy)), m_lambda(lambda)
{
  using infomath::plogp;
  m_leafFlf.resize(m_leafFlow.size());
  for (std::size_t i = 0; i < m_leafFlow.size(); ++i)
    m_leafFlf[i] = plogp(m_leafFlow[i]);
}

double LossyCorrection::moduleCost(double flow, double flowLogFlow, double entropy) const
{
  using infomath::plogp;
  // Naming-overhead loss beyond the tolerated distortion; 0 when the module is
  // not noise (matches LossyMapEquation::calcCorrection).
  return std::max(0.0, (plogp(flow) - flowLogFlow) - m_lambda * entropy);
}

double LossyCorrection::hierarchicalCorrection(const ColumnarTwoLevel& core) const
{
  if (core.hierNumLevels() < 2)
    return 0.0;
  const int nLeaves = core.numLeaves();
  const int numModules = core.hierLevelSize(1);
  std::vector<double> F(numModules, 0.0), flf(numModules, 0.0), H(numModules, 0.0);
  for (int i = 0; i < nLeaves; ++i) {
    const int m = core.hierLeafModule(i);
    F[m] += m_leafFlow[i];
    flf[m] += m_leafFlf[i];
    H[m] += m_leafEntropy[i];
  }
  double sumC = 0.0;
  for (int m = 0; m < numModules; ++m)
    sumC += moduleCost(F[m], flf[m], H[m]);
  return -sumC; // objective J = base - sum_m c_m
}

double LossyCorrection::initMoveLoop(const std::vector<int>& leafModule, int numModules)
{
  m_moduleFlow.assign(numModules, 0.0);
  m_moduleFlf.assign(numModules, 0.0);
  m_moduleEntropy.assign(numModules, 0.0);
  const int nLeaves = static_cast<int>(leafModule.size());
  for (int i = 0; i < nLeaves; ++i) {
    const int m = leafModule[i];
    m_moduleFlow[m] += m_leafFlow[i];
    m_moduleFlf[m] += m_leafFlf[i];
    m_moduleEntropy[m] += m_leafEntropy[i];
  }
  double sumC = 0.0;
  for (int m = 0; m < numModules; ++m)
    sumC += moduleCost(m_moduleFlow[m], m_moduleFlf[m], m_moduleEntropy[m]);
  return -sumC;
}

double LossyCorrection::moveDelta(int leaf, int oldMod, int newMod) const
{
  if (oldMod == newMod)
    return 0.0;
  const double f = m_leafFlow[leaf], lf = m_leafFlf[leaf], h = m_leafEntropy[leaf];
  const double dOld = moduleCost(m_moduleFlow[oldMod] - f, m_moduleFlf[oldMod] - lf, m_moduleEntropy[oldMod] - h)
      - moduleCost(m_moduleFlow[oldMod], m_moduleFlf[oldMod], m_moduleEntropy[oldMod]);
  const double dNew = moduleCost(m_moduleFlow[newMod] + f, m_moduleFlf[newMod] + lf, m_moduleEntropy[newMod] + h)
      - moduleCost(m_moduleFlow[newMod], m_moduleFlf[newMod], m_moduleEntropy[newMod]);
  return -(dOld + dNew); // correction = -sum_m c_m
}

void LossyCorrection::applyMove(int leaf, int oldMod, int newMod)
{
  if (oldMod == newMod)
    return;
  const double f = m_leafFlow[leaf], lf = m_leafFlf[leaf], h = m_leafEntropy[leaf];
  m_moduleFlow[oldMod] -= f;
  m_moduleFlf[oldMod] -= lf;
  m_moduleEntropy[oldMod] -= h;
  m_moduleFlow[newMod] += f;
  m_moduleFlf[newMod] += lf;
  m_moduleEntropy[newMod] += h;
}

double LossyCorrection::mergeDelta(int a, int b) const
{
  const double ca = moduleCost(m_moduleFlow[a], m_moduleFlf[a], m_moduleEntropy[a]);
  const double cb = moduleCost(m_moduleFlow[b], m_moduleFlf[b], m_moduleEntropy[b]);
  const double cab = moduleCost(m_moduleFlow[a] + m_moduleFlow[b],
                                m_moduleFlf[a] + m_moduleFlf[b],
                                m_moduleEntropy[a] + m_moduleEntropy[b]);
  return -(cab - ca - cb); // change in -sum_m c_m
}

void LossyCorrection::applyMerge(int a, int b)
{
  m_moduleFlow[b] += m_moduleFlow[a];
  m_moduleFlf[b] += m_moduleFlf[a];
  m_moduleEntropy[b] += m_moduleEntropy[a];
  m_moduleFlow[a] = m_moduleFlf[a] = m_moduleEntropy[a] = 0.0;
}

std::unique_ptr<ColumnarCorrection> LossyCorrection::sliceForLeaves(const std::vector<int>& globalLeafIds) const
{
  const int n = static_cast<int>(globalLeafIds.size());
  std::vector<double> flow(n), entropy(n);
  for (int j = 0; j < n; ++j) {
    const int g = globalLeafIds[j];
    flow[j] = m_leafFlow[g];
    entropy[j] = m_leafEntropy[g];
  }
  return std::make_unique<LossyCorrection>(std::move(flow), std::move(entropy), m_lambda);
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

bool ColumnarTwoLevel::refineBottomWithinParents()
{
  if (m_hierLevels.size() < 3)
    return false; // no super-structure to refine within

  const std::vector<int>& a0 = m_hierAssign[0]; // leaf -> L1
  const std::vector<int>& a1 = m_hierAssign[1]; // L1 -> L2
  const int numL2 = m_hierLevels[2].n;

  // Group leaves by their level-2 module (the parent to refine within).
  std::vector<std::vector<int>> leavesPer(numL2);
  for (int i = 0; i < m_nLeaves; ++i)
    leavesPer[a1[a0[i]]].push_back(i);

  std::vector<int> newA0(m_nLeaves, -1); // leaf -> new L1'
  std::vector<int> newA1; // new L1' -> L2
  int nextL1 = 0;
  std::vector<int> loc(m_nLeaves, -1); // global leaf -> local id (reused per P)

  for (int P = 0; P < numL2; ++P) {
    const std::vector<int>& S = leavesPer[P];
    const int nP = static_cast<int>(S.size());
    if (nP == 0)
      continue;
    for (int j = 0; j < nP; ++j)
      loc[S[j]] = j;

    // Build P's internal leaf sub-network (global flow, internal edges).
    Level sub;
    sub.n = nP;
    sub.flow.resize(nP);
    sub.enter.resize(nP);
    sub.exit.resize(nP);
    sub.teleFlow.resize(nP);
    sub.teleWeight.resize(nP);
    std::vector<int> outDeg(nP, 0), inDeg(nP, 0);
    for (int j = 0; j < nP; ++j) {
      const int g = S[j];
      sub.flow[j] = m_leaf0.flow[g];
      sub.enter[j] = m_leaf0.enter[g];
      sub.exit[j] = m_leaf0.exit[g];
      sub.teleFlow[j] = m_leaf0.teleFlow[g];
      sub.teleWeight[j] = m_leaf0.teleWeight[g];
      for (int k = m_leaf0.outStart[g]; k < m_leaf0.outStart[g + 1]; ++k) {
        const int lt = loc[m_leaf0.outTarget[k]];
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
      for (int k = m_leaf0.outStart[g]; k < m_leaf0.outStart[g + 1]; ++k) {
        const int lt = loc[m_leaf0.outTarget[k]];
        if (lt != -1) {
          const double f = m_leaf0.outFlow[k];
          sub.outTarget[op[j]] = lt;
          sub.outFlow[op[j]] = f;
          ++op[j];
          sub.inTarget[ip[lt]] = j;
          sub.inFlow[ip[lt]] = f;
          ++ip[lt];
        }
      }
    }

    // Optimal in-context two-level of P's leaves (P's exit is the sub-network
    // exit), objective-aware: slice Meta/Mem to P's leaves so the re-partition
    // optimizes base + correction, not just gates on it.
    ColumnarTwoLevel subOpt;
    subOpt.setInterruptCallback(m_interruptCallback);
    subOpt.buildFromLevel(sub, m_undirected, m_seed, m_hierLevels[2].exit[P], m_recordedTeleport, m_totalTeleFlow);
    addSlicedLeafCorrections(subOpt, S);
    subOpt.optimizeTwoLevel();
    const std::vector<int>& subAssign = subOpt.leafTopModule();
    const int Ksub = static_cast<int>(subOpt.numTopModules());

    for (int j = 0; j < nP; ++j)
      newA0[S[j]] = nextL1 + subAssign[j];
    for (int s = 0; s < Ksub; ++s)
      newA1.push_back(P);
    nextL1 += Ksub;

    for (int j = 0; j < nP; ++j)
      loc[S[j]] = -1; // reset scratch
  }

  // Replace the bottom level; levels 2+ are unchanged (same leaf sets per L2).
  m_hierAssign[0] = std::move(newA0);
  m_hierAssign[1] = std::move(newA1);
  m_hierLevels[1] = aggregateLevel(m_leaf0, m_hierAssign[0], nextL1, m_undirected);
  m_numTopModules = static_cast<unsigned int>(m_hierLevels.back().n);
  return true;
}

void ColumnarTwoLevel::coarsenModules(double& L, int maxSweeps)
{
  // Gated apply: run `step`, keep it only if it lowers the true hierarchical
  // codelength, else revert. Same accept/revert policy the converge refinement
  // uses for its tuning steps.
  auto gated = [&](auto&& step) {
    std::vector<Level> savedLevels = m_hierLevels;
    std::vector<std::vector<int>> savedAssign = m_hierAssign;
    if (!step())
      return false;
    const double after = hierarchicalCodelengthFromStack();
    if (after < L - kMinImprovement) {
      L = after;
      return true;
    }
    m_hierLevels = std::move(savedLevels);
    m_hierAssign = std::move(savedAssign);
    m_numTopModules = static_cast<unsigned int>(m_hierLevels.back().n);
    return false;
  };
  for (int sweep = 0; sweep < maxSweeps; ++sweep) {
    pollInterrupt();
    const bool merged = gated([&] { return mergeLeafModulesWithinParents(); });
    const bool regrouped = gated([&] { return refineTopLayer(); });
    if (!merged && !regrouped)
      break;
  }
}

double ColumnarTwoLevel::optimizeFlexible(unsigned int bottomBlockLimit, unsigned int sweepLimit)
{
  double L = optimizeHierarchical(bottomBlockLimit);
  // A single bottom re-partition within grandparents. refineBottomWithinParents
  // keeps every leaf inside its level-2 grandparent, so the leaf-set per
  // grandparent is invariant; re-running it re-partitions the same leaf-sets
  // within the same grandparents from singletons with the same seed, which is
  // idempotent (same reasoning as the single-interior-level case in
  // refineHierarchy). A second pass therefore only ever re-derives the same
  // partition to detect convergence — a full O(n_leaves) re-partition of pure
  // overhead — so we stop after one (measured: bit-identical, ~25-35% faster on
  // the deep/memory nets).
  if (refineBottomWithinParents())
    L = std::min(L, hierarchicalCodelengthFromStack());
  // Coarsen (merge leaf modules + regroup the top), exactly as the converge
  // search does. Cheap (module-level, not leaf-level) and a no-op for the base
  // objective, but it is what the memory / metadata / lossy objectives need — a
  // fast search that skipped it landed far from the optimum on those (air30k
  // +14%). With it, -F matches converge on those objectives at a fraction of the
  // cost, and stays unchanged on base networks.
  coarsenModules(L, sweepLimit > 0 ? static_cast<int>(sweepLimit) : 1000);
  return L;
}

bool ColumnarTwoLevel::refineLayerWithinGrandparent(int k)
{
  const int top = static_cast<int>(m_hierLevels.size()) - 1;
  if (k < 0 || k + 2 > top)
    return false; // need a layer-(k+2) grandparent to refine within

  const Level& base = m_hierLevels[k]; // layer-k units to re-partition
  const std::vector<int>& aC = m_hierAssign[k]; // unit_k -> module_{k+1}
  const std::vector<int>& aP = m_hierAssign[k + 1]; // module_{k+1} -> module_{k+2}
  const Level& grand = m_hierLevels[k + 2];
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
  // codeword usage is the module's *enter* flow — mirror the up-build's
  // enter-flow transform (superNet.flow = enter) so grouping optimizes the
  // right cost instead of over-merging on total flow.
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
    sub.enter.resize(nP);
    sub.exit.resize(nP);
    sub.teleFlow.resize(nP);
    sub.teleWeight.resize(nP);
    std::vector<int> outDeg(nP, 0), inDeg(nP, 0);
    for (int j = 0; j < nP; ++j) {
      const int g = S[j];
      sub.flow[j] = interior ? base.enter[g] : base.flow[g];
      sub.enter[j] = base.enter[g];
      sub.exit[j] = base.exit[g];
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
    ColumnarTwoLevel subOpt;
    subOpt.setInterruptCallback(m_interruptCallback);
    subOpt.buildFromLevel(sub, m_undirected, m_seed, grand.exit[G], m_recordedTeleport, m_totalTeleFlow);
    if (k == 0)
      addSlicedLeafCorrections(subOpt, S);
    subOpt.optimizeTwoLevel();
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

  const Level& base = m_hierLevels[k];
  const int nU = base.n;

  // One root group over all layer-k units; enter-flow transform (grouping
  // modules, whose codeword usage is the enter flow), root exit = 0.
  Level sub;
  sub.n = nU;
  sub.flow.resize(nU);
  sub.enter.resize(nU);
  sub.exit.resize(nU);
  sub.teleFlow.resize(nU);
  sub.teleWeight.resize(nU);
  std::vector<int> outDeg(nU, 0), inDeg(nU, 0);
  for (int j = 0; j < nU; ++j) {
    sub.flow[j] = base.enter[j];
    sub.enter[j] = base.enter[j];
    sub.exit[j] = base.exit[j];
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
  subOpt.buildFromLevel(sub, m_undirected, m_seed, 0.0, m_recordedTeleport, m_totalTeleFlow);
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
  if (corr.empty())
    return false;

  const int K = m_hierLevels[1].n; // leaf modules (level-0 -> 1)
  // Leaf modules are merged within their parent. With a level-2 grandparent each
  // leaf module has an explicit parent; in a two-level tree (e.g. lossy) the
  // parent is the root, i.e. one group whose codebook uses the top enter flows.
  const bool hasGrandparent = m_hierLevels.size() >= 3;
  const int numParents = hasGrandparent ? m_hierLevels[2].n : 1;

  // Seed the corrections' per-module state at the current leaf-module partition.
  for (auto* c : corr)
    c->initMoveLoop(m_hierAssign[0], K);

  // Mutable leaf-module aggregates + parent map + adjacency (crossing flow).
  std::vector<double> flow = m_hierLevels[1].flow;
  std::vector<double> enter = m_hierLevels[1].enter;
  std::vector<double> exit = m_hierLevels[1].exit;
  std::vector<int> parent = hasGrandparent ? m_hierAssign[1] : std::vector<int>(K, 0);
  std::vector<std::unordered_map<int, double>> adjOut(K), adjIn(K);
  {
    const Level& L1 = m_hierLevels[1];
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
      totalUse[p] = m_hierLevels[2].exit[p];
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
  m_hierLevels[1] = aggregateLevel(m_leaf0, m_hierAssign[0], Knew, m_undirected);
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
  auto gatedStep = [&](auto&& step, const char* tag) -> bool {
    std::vector<Level> savedLevels = m_hierLevels;
    std::vector<std::vector<int>> savedAssign = m_hierAssign;
    if (!step())
      return false;
    const double after = hierarchicalCodelengthFromStack();
    if (after < L - kMinImprovement) {
#ifdef COLUMNAR_DEBUG
      std::fprintf(stderr, "[refine] %s %.9f -> %.9f (levels %d, top %d)\n",
                   tag, L, after, (int)m_hierLevels.size(), m_hierLevels.back().n);
#else
      (void)tag;
#endif
      L = after;
      return true;
    }
    m_hierLevels = std::move(savedLevels);
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
  for (int sweep = 0; sweep < refineSweeps; ++sweep) {
    pollInterrupt();
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
  std::fprintf(stderr, "[refine] interior=%.3fs coarsen=%.3fs (superAgg=%u)\n",
               double(t_p2 - t_p1) / CLOCKS_PER_SEC, double(t_end - t_p2) / CLOCKS_PER_SEC, m_superAggLimit);
#endif
  return L;
}

std::vector<std::pair<unsigned int, std::vector<unsigned int>>>
ColumnarTwoLevel::toNodePaths(const std::vector<InfoNode*>& leafNodes) const
{
  std::vector<std::pair<unsigned int, std::vector<unsigned int>>> paths;
  const int nLeaves = m_hierLevels.empty() ? 0 : m_hierLevels[0].n;
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

  optimizeTwoLevel(bottomBlockLimit, bottomBlockLimit == 0);
  const int bottomK = static_cast<int>(m_numTopModules);

  double bestBuildL = std::numeric_limits<double>::infinity();
  std::vector<Level> bestLevels;
  std::vector<std::vector<int>> bestAssign;
  unsigned int bestTop = 0;
  unsigned int bestSuperAgg = 0;

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
    }
  }

  // Restore the winning build and refine only it.
  m_hierLevels = std::move(bestLevels);
  m_hierAssign = std::move(bestAssign);
  m_numTopModules = bestTop;
  m_superAggLimit = bestSuperAgg;
  const double bestL = refineHierarchy(bestBuildL, sweepLimit);
#ifdef COLUMNAR_DEBUG
  {
    const double corr = objectiveCorrection();
    std::fprintf(stderr, "[optColumnar] winner superAgg=%u total=%.6f base=%.6f corr=%.6f top=%u\n",
                 bestSuperAgg, bestL, bestL - corr, corr, m_numTopModules);
  }
#endif
  return bestL;
}

} // namespace infomap
