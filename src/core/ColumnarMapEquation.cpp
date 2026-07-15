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
  inline double deltaCodelengthMovingNode(double enterFlow, double enterFlow_log_enterFlow,
                                          double curEnter, double curExit, double curFlow,
                                          double oldEnter, double oldExit, double oldFlow,
                                          double newEnter, double newExit, double newFlow,
                                          double deltaOld, double deltaNew)
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
  m_leafFlow.resize(m_nLeaves);
  m_leafNodeFlow_log_nodeFlow = 0.0;
  for (int i = 0; i < m_nLeaves; ++i) {
    L.flow[i] = leafNodes[i]->data.flow;
    L.enter[i] = leafNodes[i]->data.enterFlow;
    L.exit[i] = leafNodes[i]->data.exitFlow;
    m_leafFlow[i] = L.flow[i];
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

void ColumnarTwoLevel::buildFromLevel(const Level& level, bool undirected, unsigned long seed, double exitNetworkFlow)
{
  using infomath::plogp;
  m_undirected = undirected;
  m_seed = seed;
  m_exitNetworkFlow = exitNetworkFlow;
  m_nLeaves = level.n;
  m_leaf0 = level;
  m_leafFlow = level.flow;
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

  m_enterFlow = 0.0;
  m_enter_log_enter = 0.0;
  m_exit_log_exit = 0.0;
  m_flow_log_flow = 0.0;
  for (int i = 0; i < n; ++i) {
    m_module[i] = i;
    m_mFlow[i] = m_lvl.flow[i];
    m_mEnter[i] = m_lvl.enter[i];
    m_mExit[i] = m_lvl.exit[i];
    m_flow_log_flow += plogp(m_lvl.flow[i] + m_lvl.exit[i]);
    m_enter_log_enter += plogp(m_lvl.enter[i]);
    m_exit_log_exit += plogp(m_lvl.exit[i]);
    m_enterFlow += m_lvl.enter[i];
  }
  // Flow leaving the (sub-)network is coded in the index too (0 if closed).
  m_enterFlow += m_exitNetworkFlow;
  m_enterFlow_log_enterFlow = plogp(m_enterFlow);
  m_nodeFlow_log_nodeFlow = m_leafNodeFlow_log_nodeFlow; // constant over leaves
  m_codelength = (m_enterFlow_log_enterFlow - m_enter_log_enter)
      + (-m_exit_log_exit + m_flow_log_flow - m_nodeFlow_log_nodeFlow);
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

  m_enterFlow -= m_mEnter[cMod] + m_mEnter[newMod];
  m_enter_log_enter -= plogp(m_mEnter[cMod]) + plogp(m_mEnter[newMod]);
  m_exit_log_exit -= plogp(m_mExit[cMod]) + plogp(m_mExit[newMod]);
  m_flow_log_flow -= plogp(m_mExit[cMod] + m_mFlow[cMod]) + plogp(m_mExit[newMod] + m_mFlow[newMod]);

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

  m_enterFlow += m_mEnter[cMod] + m_mEnter[newMod];
  m_enter_log_enter += plogp(m_mEnter[cMod]) + plogp(m_mEnter[newMod]);
  m_exit_log_exit += plogp(m_mExit[cMod]) + plogp(m_mExit[newMod]);
  m_flow_log_flow += plogp(m_mExit[cMod] + m_mFlow[cMod]) + plogp(m_mExit[newMod] + m_mFlow[newMod]);
  m_enterFlow_log_enterFlow = plogp(m_enterFlow);
  m_codelength = (m_enterFlow_log_enterFlow - m_enter_log_enter)
      + (-m_exit_log_exit + m_flow_log_flow - m_nodeFlow_log_nodeFlow);

  m_mMembers[cMod] -= 1;
  m_mMembers[newMod] += 1;
  m_module[u] = newMod;
}

void ColumnarTwoLevel::seedAssignment(const std::vector<int>& assign)
{
  // Start from singletons, then force each unit into its assigned module so the
  // module aggregates + running terms end up exact (order-independent sums).
  initPartition();
  for (int u = 0; u < m_lvl.n; ++u)
    moveUnit(u, assign[u]);
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

      for (int m : touched) {
        if (m == cMod)
          continue;
        const double deltaNew = dEnter[m] + dExit[m];
        double dl = deltaCodelengthMovingNode(
            m_enterFlow, m_enterFlow_log_enterFlow,
            curEnter, curExit, curFlow,
            m_mEnter[cMod], m_mExit[cMod], m_mFlow[cMod],
            m_mEnter[m], m_mExit[m], m_mFlow[m],
            deltaOld, deltaNew);
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
        m_enterFlow -= m_mEnter[cMod] + m_mEnter[bestMod];
        m_enter_log_enter -= plogp(m_mEnter[cMod]) + plogp(m_mEnter[bestMod]);
        m_exit_log_exit -= plogp(m_mExit[cMod]) + plogp(m_mExit[bestMod]);
        m_flow_log_flow -= plogp(m_mExit[cMod] + m_mFlow[cMod]) + plogp(m_mExit[bestMod] + m_mFlow[bestMod]);

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

        m_enterFlow += m_mEnter[cMod] + m_mEnter[bestMod];
        m_enter_log_enter += plogp(m_mEnter[cMod]) + plogp(m_mEnter[bestMod]);
        m_exit_log_exit += plogp(m_mExit[cMod]) + plogp(m_mExit[bestMod]);
        m_flow_log_flow += plogp(m_mExit[cMod] + m_mFlow[cMod]) + plogp(m_mExit[bestMod] + m_mFlow[bestMod]);
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
  // Module aggregates already hold the consolidated flow data.
  for (int oldM = 0; oldM < n; ++oldM) {
    if (m_mMembers[oldM] == 0)
      continue;
    const int m = remap[oldM];
    next.flow[m] = m_mFlow[oldM];
    next.enter[m] = m_mEnter[oldM];
    next.exit[m] = m_mExit[oldM];
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

ColumnarTwoLevel::Level ColumnarTwoLevel::aggregateLevel(const Level& base, const std::vector<int>& assign, int K, bool undirected)
{
  Level out;
  out.n = K;
  out.flow.assign(K, 0.0);
  out.enter.assign(K, 0.0);
  out.exit.assign(K, 0.0);
  for (int i = 0; i < base.n; ++i)
    out.flow[assign[i]] += base.flow[i];

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
  using infomath::plogp;

  // Bottom: leaves -> building blocks. With bottomBlockLimit > 0 we stop the
  // aggregation early (finer bottom) and skip fine-tune to preserve fineness;
  // with 0 we take the full two-level optimum with fine-tune.
  optimizeTwoLevel(bottomBlockLimit, bottomBlockLimit == 0);

  m_hierLevels.clear();
  m_hierAssign.clear();
  m_hierLevels.push_back(m_leaf0);
  m_hierAssign.push_back(m_leafTop);
  Level cur = aggregateLevel(m_leaf0, m_leafTop, static_cast<int>(m_numTopModules), m_undirected);
  m_hierLevels.push_back(cur);

  // Grow up with the enter-flow super-search while it shortens the index code.
  while (cur.n > 1) {
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
    superOpt.buildFromLevel(superNet, m_undirected, m_seed);
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

  // Level-1 modules code their leaf children (module-of-leaf-nodes term).
  {
    const Level& L1 = m_hierLevels[1];
    const std::vector<int>& leafToL1 = m_hierAssign[0];
    std::vector<double> T(L1.n);
    for (int m = 0; m < L1.n; ++m)
      T[m] = L1.flow[m] + L1.exit[m];
    std::vector<double> acc(L1.n, 0.0);
    for (int i = 0; i < m_leaf0.n; ++i) {
      const int m = leafToL1[i];
      if (T[m] >= 1e-16)
        acc[m] -= plogp(m_leaf0.flow[i] / T[m]);
    }
    for (int m = 0; m < L1.n; ++m) {
      if (T[m] < 1e-16)
        continue;
      acc[m] -= plogp(L1.exit[m] / T[m]);
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
      sumEnter[p] += Lkm1.enter[c];
      sumPlogpEnter[p] += plogp(Lkm1.enter[c]);
    }
    for (int m = 0; m < Lk.n; ++m) {
      if (Lk.flow[m] < 1e-16)
        continue;
      const double totalUse = Lk.exit[m] + sumEnter[m];
      total += plogp(totalUse) - sumPlogpEnter[m] - plogp(Lk.exit[m]);
    }
  }

  // Root codes the topmost modules (exit of the whole network is 0).
  {
    const Level& Ltop = m_hierLevels[topLevel];
    double sumEnter = 0.0, sumPlogpEnter = 0.0;
    for (int m = 0; m < Ltop.n; ++m) {
      sumEnter += Ltop.enter[m];
      sumPlogpEnter += plogp(Ltop.enter[m]);
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
  const double Fo = m_moduleFlow[oldMod], Fn = m_moduleFlow[newMod];
  const double fo = moduleCategoryFlow(oldMod, q), fn = moduleCategoryFlow(newMod, q);
  // term(m) = plogp(F_m) - sum_c plogp(f_c). Only F and category q change.
  const double dOld = (plogp(Fo - w) - plogp(Fo)) - (plogp(fo - w) - plogp(fo));
  const double dNew = (plogp(Fn + w) - plogp(Fn)) - (plogp(fn + w) - plogp(fn));
  return m_metaDataRate * (dOld + dNew);
}

void MetaCorrection::applyMove(int leaf, int oldMod, int newMod)
{
  if (oldMod == newMod)
    return;
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
    const double bv = (it == cb.end()) ? 0.0 : it->second;
    d -= plogp(kv.second + bv) - plogp(kv.second) - plogp(bv); // -sum_c plogp(f_c)
  }
  return m_metaDataRate * d;
}

void MetaCorrection::applyMerge(int a, int b)
{
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
}

double MemCorrection::physFlow(int module, int physical) const
{
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
  const int nLeaves = static_cast<int>(leafModule.size());
  for (int i = 0; i < nLeaves; ++i) {
    const int m = leafModule[i], p = m_leafPhysical[i];
    m_modulePhysFlow[m][p] += m_leafFlow[i];
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
  const int p = m_leafPhysical[leaf];
  const double f = m_leafFlow[leaf];
  const double oc = physFlow(oldMod, p), nc = physFlow(newMod, p);
  // term = C_state - sum plogp(physFlow); only phys p in the two modules changes.
  return (plogp(oc) - plogp(oc - f)) + (plogp(nc) - plogp(nc + f));
}

void MemCorrection::applyMove(int leaf, int oldMod, int newMod)
{
  if (oldMod == newMod)
    return;
  const int p = m_leafPhysical[leaf];
  const double f = m_leafFlow[leaf];
  auto& oldPhys = m_modulePhysFlow[oldMod];
  auto it = oldPhys.find(p);
  if (it != oldPhys.end()) {
    it->second -= f;
    if (it->second <= 1e-16) {
      oldPhys.erase(it);
      m_physModules[p].erase(oldMod); // oldMod no longer holds physical p
    }
  }
  m_modulePhysFlow[newMod][p] += f;
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
  const auto& pb = m_modulePhysFlow[b];
  double dSum = 0.0; // change in sum_p plogp(physFlow)
  for (const auto& kv : pa) {
    const auto it = pb.find(kv.first);
    const double bv = (it == pb.end()) ? 0.0 : it->second;
    dSum += plogp(kv.second + bv) - plogp(kv.second) - plogp(bv);
  }
  return -dSum; // correction = C_state - sum_p plogp; delta correction = -dSum
}

void MemCorrection::applyMerge(int a, int b)
{
  auto& pa = m_modulePhysFlow[a];
  auto& pb = m_modulePhysFlow[b];
  for (const auto& kv : pa) {
    pb[kv.first] += kv.second;
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
  std::vector<int> newA1;                // new L1' -> L2
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
    std::vector<int> outDeg(nP, 0), inDeg(nP, 0);
    for (int j = 0; j < nP; ++j) {
      const int g = S[j];
      sub.flow[j] = m_leaf0.flow[g];
      sub.enter[j] = m_leaf0.enter[g];
      sub.exit[j] = m_leaf0.exit[g];
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
    subOpt.buildFromLevel(sub, m_undirected, m_seed, m_hierLevels[2].exit[P]);
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

double ColumnarTwoLevel::optimizeFlexible(unsigned int bottomBlockLimit)
{
  double L = optimizeHierarchical(bottomBlockLimit);
  for (int iter = 0; iter < 8; ++iter) {
    if (!refineBottomWithinParents())
      break;
    const double newL = hierarchicalCodelengthFromStack();
    if (newL >= L - kMinImprovement) {
      L = std::min(L, newL);
      break;
    }
    L = newL;
  }
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
    std::vector<int> outDeg(nP, 0), inDeg(nP, 0);
    for (int j = 0; j < nP; ++j) {
      const int g = S[j];
      sub.flow[j] = interior ? base.enter[g] : base.flow[g];
      sub.enter[j] = base.enter[g];
      sub.exit[j] = base.exit[g];
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
    subOpt.buildFromLevel(sub, m_undirected, m_seed, grand.exit[G]);
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
  std::vector<int> outDeg(nU, 0), inDeg(nU, 0);
  for (int j = 0; j < nU; ++j) {
    sub.flow[j] = base.enter[j];
    sub.enter[j] = base.enter[j];
    sub.exit[j] = base.exit[j];
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
  subOpt.buildFromLevel(sub, m_undirected, m_seed, 0.0);
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
  if (m_hierLevels.size() < 3)
    return false; // need a level-2 parent term to price the merge against

  // Corrections that reward coarsening (Mem/Meta). No participants => the base
  // objective, where any merge only raises the codelength: nothing to do.
  std::vector<ColumnarCorrection*> corr;
  for (auto& c : m_corrections)
    if (c->participatesInMoveLoop())
      corr.push_back(c.get());
  if (corr.empty())
    return false;

  const int K = m_hierLevels[1].n; // leaf modules (level-0 -> 1)
  const int numParents = m_hierLevels[2].n;

  // Seed the corrections' per-module state at the current leaf-module partition.
  for (auto* c : corr)
    c->initMoveLoop(m_hierAssign[0], K);

  // Mutable leaf-module aggregates + parent map + adjacency (crossing flow).
  std::vector<double> flow = m_hierLevels[1].flow;
  std::vector<double> enter = m_hierLevels[1].enter;
  std::vector<double> exit = m_hierLevels[1].exit;
  std::vector<int> parent = m_hierAssign[1];
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
  std::vector<double> totalUse(numParents, 0.0);
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
      for (const auto& kv : adjOut[a]) consider(kv.first);
      for (const auto& kv : adjIn[a]) consider(kv.first);
      if (coPhysicalMerge) {
        mergeCand.clear();
        for (auto* c : corr)
          c->proposeMergePartners(a, mergeCand);
        for (int b : mergeCand) consider(b);
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
  std::vector<int> newA1(Knew);
  for (int a = 0; a < K; ++a)
    if (alive[a])
      newA1[newId[a]] = parent[a];

  m_hierAssign[0] = std::move(newA0);
  m_hierAssign[1] = std::move(newA1);
  m_hierLevels[1] = aggregateLevel(m_leaf0, m_hierAssign[0], Knew, m_undirected);
  m_numTopModules = static_cast<unsigned int>(m_hierLevels.back().n);
  return true;
}

double ColumnarTwoLevel::optimizeConverge(unsigned int bottomBlockLimit, unsigned int superAggLimit, unsigned int sweepLimit)
{
  m_superAggLimit = superAggLimit;
  double L = optimizeHierarchical(bottomBlockLimit);

  // sweepLimit caps the up/down tuning sweeps; 0 means run until convergence
  // (the loop breaks early on a no-improvement sweep regardless of the cap).
  const int maxSweeps = sweepLimit > 0 ? static_cast<int>(sweepLimit) : 1000;

  // Up/down sweep: refine each interior layer within its grandparent, bottom-up,
  // accepting a refine only if it lowers the true hierarchical codelength (the
  // interior refine optimizes a units-as-leaves proxy, so we gate on the real
  // objective and revert otherwise). Iterate whole sweeps until none improves.
#ifdef COLUMNAR_DEBUG
  std::fprintf(stderr, "[converge] start L=%.9f levels=%d superAgg=%u\n", L, (int)m_hierLevels.size(), superAggLimit);
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
      std::fprintf(stderr, "[converge] %s %.9f -> %.9f (levels %d, top %d)\n",
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
  // Phase 1 — interior refine (up/down sweep): re-partition each interior layer
  // within its grandparent, to convergence. This is the expensive pass (the
  // k==0 refine re-partitions every leaf), so it runs ONCE here as a build
  // refinement. The Phase-2 coarsening below only ever re-fragments a finished
  // interior partition (measured: such a refine always reverts), so it is not
  // worth re-running per coarsening step.
  // With a single interior level the refine is idempotent (deterministic given
  // fixed grandparents, which it does not change), so one pass is convergence —
  // skip the second no-op pass. Multiple interior levels can interact, so loop.
  const int numInterior = std::max(0, static_cast<int>(m_hierLevels.size()) - 2);
  const int phase1Sweeps = (numInterior <= 1) ? std::min(1, maxSweeps) : maxSweeps;
  for (int sweep = 0; sweep < phase1Sweeps; ++sweep) {
    bool improved = false;
    const int top = static_cast<int>(m_hierLevels.size()) - 1;
    for (int k = 0; k + 2 <= top; ++k)
      improved |= gatedStep([&] { return refineLayerWithinGrandparent(k); }, "REFINE");
    if (!improved)
      break;
  }

  // Phase 2 — mem-aware coarsening: merge leaf modules (mergeLeafModulesWithinParents)
  // and regroup the top level (refineTopLayer), interleaved to convergence. Both
  // operate on the few modules (not the leaves), so this loop is cheap; iterating
  // lets a merge cross parents that a prior top-refine has regrouped. No-op for
  // the base objective (merge needs a correction; top-refine is gated).
#ifdef COLUMNAR_DEBUG
  const std::clock_t t_p2 = std::clock();
#endif
  for (int sweep = 0; sweep < maxSweeps; ++sweep) {
    bool improved = false;
    improved |= gatedStep([&] { return mergeLeafModulesWithinParents(); }, "MERGE");
    improved |= gatedStep([&] { return refineTopLayer(); }, "TOP-REFINE");
    if (!improved)
      break;
  }
#ifdef COLUMNAR_DEBUG
  const std::clock_t t_end = std::clock();
  std::fprintf(stderr, "[converge] phase1=%.3fs phase2=%.3fs (superAgg=%u)\n",
               double(t_p2 - t_p1) / CLOCKS_PER_SEC, double(t_end - t_p2) / CLOCKS_PER_SEC, superAggLimit);
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
  // best setting varies by network (like Infomap's multi-trial search). Run the
  // up/down sweep at a small set of super-merge settings and keep the best
  // partition, leaving its stack in the members for materialization.
  static const unsigned int kSuperAggSettings[] = { 0u, 1u };

  double bestL = std::numeric_limits<double>::infinity();
  std::vector<Level> bestLevels;
  std::vector<std::vector<int>> bestAssign;
  unsigned int bestTop = 0;

  for (unsigned int superAgg : kSuperAggSettings) {
    const double L = optimizeConverge(bottomBlockLimit, superAgg, sweepLimit);
    if (L < bestL - kMinImprovement) {
      bestL = L;
      bestLevels = m_hierLevels;
      bestAssign = m_hierAssign;
      bestTop = m_numTopModules;
    }
  }

  m_hierLevels = std::move(bestLevels);
  m_hierAssign = std::move(bestAssign);
  m_numTopModules = bestTop;
#ifdef COLUMNAR_DEBUG
  {
    const double corr = objectiveCorrection();
    std::fprintf(stderr, "[optColumnar] best total=%.6f base=%.6f corr=%.6f top=%u\n",
                 bestL, bestL - corr, corr, bestTop);
  }
#endif
  return bestL;
}

} // namespace infomap
