/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef MAPEQUATION_H_
#define MAPEQUATION_H_

#include "../utils/infomath.h"
#include "../utils/convert.h"
#include "../io/Config.h"
#include "../utils/Log.h"
#include "../utils/VectorMap.h"
#include "InfoNode.h"
#include "FlowData.h"
#include <vector>
#include <map>
#include <array>
#include <ostream>
#include <algorithm>

namespace infomap {

class InfoNode;

// One module entry of a physical node's flat module map, kept sorted by
// module id. The sort order is load-bearing: iteration order determines
// floating-point summation order in the codelength terms.
struct ModuleMemNodes {
  ModuleMemNodes(unsigned int module, unsigned int numMemNodes, double sumFlow)
      : module(module), numMemNodes(numMemNodes), sumFlow(sumFlow) {}
  unsigned int module;
  unsigned int numMemNodes; // use counter to check for zero to avoid round-off errors in sumFlow
  double sumFlow;
};

/**
 * Base implementation of the map equation, shared by the concrete objectives
 * (BiasedMapEquation, MemMapEquation, MetaMapEquation, RegularizedMultilayerMapEquation).
 *
 * Dispatch is static, not dynamic: each objective is used as the concrete
 * `Objective` type parameter of InfomapOptimizer<Objective> and stored by value,
 * so no call here goes through a base pointer/reference. Subclasses inherit
 * privately and re-expose or redefine members by name; the methods below are
 * therefore deliberately non-virtual (a redefinition in a subclass hides the
 * base version). Internal cross-calls are qualified with `ME::` so they always
 * bind to this base implementation regardless of the most-derived type.
 */

inline std::vector<ModuleMemNodes>::iterator findModuleMemNodes(std::vector<ModuleMemNodes>& moduleToMemNodes, unsigned int module)
{
  return std::lower_bound(moduleToMemNodes.begin(), moduleToMemNodes.end(), module, [](const ModuleMemNodes& memNodes, unsigned int target) { return memNodes.module < target; });
}

template <typename FlowDataType = FlowData, typename DeltaFlowDataType = DeltaFlow>
class MapEquation {
  using ME = MapEquation<FlowDataType, DeltaFlowDataType>;

public:
  MapEquation() = default;

  MapEquation(const MapEquation& other) = default;

  MapEquation& operator=(const MapEquation& other) = default;

  MapEquation(MapEquation&& other) noexcept = default;

  MapEquation& operator=(MapEquation&& other) noexcept = default;

  ~MapEquation() = default;

  // ===================================================
  // Getters
  // ===================================================

  double getIndexCodelength() const { return indexCodelength; }

  double getModuleCodelength() const { return moduleCodelength; }

  double getCodelength() const { return codelength; }

  // ===================================================
  // IO
  // ===================================================

  std::ostream& print(std::ostream& out) const
  {
    return out << indexCodelength << " + " << moduleCodelength << " = " << io::toPrecision(codelength);
  }

  // ===================================================
  // Init
  // ===================================================

  void init(const Config& config)
  {
    Log(3) << "MapEquation::init()...\n";
    m_config = config;
    m_nonRedundant = config.nonRedundant;
    m_nrExact = config.nonRedundantExact;
  }

  void initNetwork(InfoNode& root)
  {
    Log(3) << "MapEquation::initNetwork()...\n";

    nodeFlow_log_nodeFlow = 0.0;
    for (InfoNode& node : root) {
      double plogpFlow = infomath::plogp(node.data.flow);
      nodeFlow_log_nodeFlow += plogpFlow;
      if (m_nonRedundant) // leaf-level seed of the additive F aggregate (carried on consolidation)
        node.nrFlowLogFlow = plogpFlow;
    }
    ME::initSubNetwork(root);
  }

  void initSuperNetwork(InfoNode& root)
  {
    Log(3) << "MapEquation::initSuperNetwork()...\n";

    nodeFlow_log_nodeFlow = 0.0;
    for (InfoNode& node : root) {
      nodeFlow_log_nodeFlow += infomath::plogp(node.data.enterFlow);
    }
  }

  void initSubNetwork(InfoNode& root)
  {
    exitNetworkFlow = root.data.exitFlow;
    exitNetworkFlow_log_exitNetworkFlow = infomath::plogp(exitNetworkFlow);
  }

  void initPartition(std::vector<InfoNode*>& nodes) { ME::calculateCodelength(nodes); }

  // ===================================================
  // Codelength
  // ===================================================

  double calcCodelength(const InfoNode& parent) const
  {
    return parent.isLeafModule() ? ME::calcCodelengthOnModuleOfLeafNodes(parent) : ME::calcCodelengthOnModuleOfModules(parent);
  }

  void addMemoryContributions(InfoNode& /*current*/, DeltaFlowDataType& /*oldModuleDelta*/, DeltaFlowDataType& /*newModuleDelta*/) {}

  void addMemoryContributions(InfoNode& /*current*/, DeltaFlowDataType& /*oldModuleDelta*/, VectorMap<DeltaFlowDataType>& /*moduleDeltaFlow*/) {}

  void addTeleportationFlow(InfoNode& current, const std::vector<FlowDataType>& moduleFlowData, DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta);

  void addTeleportationFlow(InfoNode& current, const std::vector<FlowDataType>& moduleFlowData, VectorMap<DeltaFlowDataType>& moduleDeltaFlow);

  double getDeltaCodelengthOnMovingNode(InfoNode& current,
                                        DeltaFlowDataType& oldModuleDelta,
                                        DeltaFlowDataType& newModuleDelta,
                                        std::vector<FlowDataType>& moduleFlowData,
                                        std::vector<unsigned int>& /*moduleMembers*/);

  // ===================================================
  // Consolidation
  // ===================================================

  void updateCodelengthOnMovingNode(InfoNode& current,
                                    DeltaFlowDataType& oldModuleDelta,
                                    DeltaFlowDataType& newModuleDelta,
                                    std::vector<FlowDataType>& moduleFlowData,
                                    std::vector<unsigned int>& /*moduleMembers*/);

  // Carry the per-module leaf aggregate F_i onto the freshly created module nodes so
  // the next (coarser) optimization level reads the correct sum for each module's
  // leaves. No-op unless the non-redundant map equation is active.
  void consolidateModules(std::vector<InfoNode*>& modules)
  {
    if (!m_nonRedundant)
      return;
    for (unsigned int i = 0; i < modules.size(); ++i) {
      if (modules[i] != nullptr && i < m_moduleFlowLogFlow.size())
        modules[i]->nrFlowLogFlow = m_moduleFlowLogFlow[i];
    }
  }

  // ===================================================
  // Debug
  // ===================================================

  void printDebug() const
  {
    Log().print("(enterFlow_log_enterFlow: {:g}, enter_log_enter: {:g}, exitNetworkFlow_log_exitNetworkFlow: {:g}) ",
                enterFlow_log_enterFlow,
                enter_log_enter,
                exitNetworkFlow_log_exitNetworkFlow);
  }

protected:
  // ===================================================
  // Protected member functions
  // ===================================================

  double calcCodelengthOnModuleOfLeafNodes(const InfoNode& parent) const;

  double calcCodelengthOnModuleOfModules(const InfoNode& parent) const;

  void calculateCodelength(std::vector<InfoNode*>& nodes)
  {
    ME::calculateCodelengthTerms(nodes);
    ME::calculateCodelengthFromCodelengthTerms();
  }

  void calculateCodelengthTerms(std::vector<InfoNode*>& nodes);

  void calculateCodelengthFromCodelengthTerms()
  {
    if (m_nonRedundant)
      return; // indexCodelength / moduleCodelength / codelength already set in calculateCodelengthTerms

    indexCodelength = enterFlow_log_enterFlow - enter_log_enter - exitNetworkFlow_log_exitNetworkFlow;
    moduleCodelength = -exit_log_exit + flow_log_flow - nodeFlow_log_nodeFlow;
    codelength = indexCodelength + moduleCodelength;
  }

public:
  // ===================================================
  // Public member variables
  // ===================================================

  double codelength = 0.0;
  double indexCodelength = 0.0;
  double moduleCodelength = 0.0;

protected:
  // ===================================================
  // Protected member variables
  // ===================================================

  Config m_config;

  double nodeFlow_log_nodeFlow = 0.0; // constant while the leaf network is the same
  double flow_log_flow = 0.0; // node.(flow + exitFlow)
  double exit_log_exit = 0.0;
  double enter_log_enter = 0.0;
  double enterFlow = 0.0;
  double enterFlow_log_enterFlow = 0.0;

  // For hierarchical
  double exitNetworkFlow = 0.0;
  double exitNetworkFlow_log_exitNetworkFlow = 0.0;

  // ===================================================
  // Non-redundant map equation (L*)
  // ===================================================
  // The non-redundant map equation removes two events a random walker on a real
  // network can never do: (1) exit module i and immediately re-enter i (the exit
  // codebook that picks the next module excludes i itself), and (2) enter module i
  // and immediately exit before visiting a node (the first visit uses a separate
  // enter codebook with no exit codeword). See
  // "Non-redundant map equation - expanded form.ipynb" for the derivation and the
  // numerical verification of the expanded plogp form used here.
  //
  // Per module i (undirected, two-level; e = exitNetworkFlow, the exit to a parent):
  //   enter  T^< = q_i^enter H(P_i)                       = q_i^enter (plogp(p_i) - F_i)/p_i
  //   within T^o = (p_i+q_i^exit-q_i^enter) H(P_i u {q_i^exit})
  //   exit   T^> = q_i^exit H(Q \ {q_i})                  leave-one-out over sibling enter rates
  // with F_i = sum_{a in i} plogp(p_a). The enter+within terms are additive per
  // module (O(1) delta); the exit term couples all modules through the leave-one-out
  // normalizer Z_i = (sum_b q_b^enter) - q_i^enter + e, so its delta sweeps all
  // modules (O(m)). indexCodelength holds sum_i T^>, moduleCodelength holds
  // sum_i (T^< + T^o).
  bool m_nonRedundant = false;
  std::vector<double> m_moduleFlowLogFlow; // per-module F_i, aligned with moduleFlowData

  // ---- O(1) power-series approximation of the leave-one-out exit term ----
  // The exact exit delta sweeps all modules (O(m)) because the leave-one-out normalizer
  // Z_i = D - q_i^enter (D = sum_b q_b^enter + e) couples every module through the global
  // D. Expanding 1/(D - q_i^enter) and log(D - q_i^enter) in powers of q_i^enter/D turns
  // that per-module nonlinearity into a handful of power-sum aggregates that ARE
  // incrementally maintainable, so the exit codelength and its move delta become O(K),
  // independent of m:
  //   L*_exit = Qx*log2(D)
  //             - (1/ln2) * sum_{k=1..K} A_k / (k * D^k)
  //             - (S + plogp(e)) * sum_{k=0..K} A_k / D^(k+1)
  //             + sum_{k=0..K} B_k / D^(k+1)
  //   A_k = sum_i q_i^exit (q_i^enter)^k        (A_0 = Qx = sum_i q_i^exit)
  //   B_k = sum_i q_i^exit (q_i^enter)^k plogp(q_i^enter)
  //   S   = sum_i plogp(q_i^enter),   D = sum_i q_i^enter + e
  // The truncation error is ~ (max_i q_i^enter / D)^(K+1): negligible when modules are
  // many (small ratio -- the expensive regime the exact sweep chokes on) and only
  // appreciable when they are few (small ratio's complement), where m is tiny and the
  // codelength is re-scored exactly at every level end anyway. Empty modules contribute
  // 0 to every aggregate automatically (q^exit = 0), so no active-set bookkeeping is
  // needed. Set --non-redundant-exact (m_nrExact) to drive the search with the exact
  // O(m) sweep instead (validation / small networks).
  static constexpr int nrSeriesK = 12; // truncation order of the exit-normalizer power series
  bool m_nrExact = false; // exact O(m) exit sweep instead of the O(K) series
  double m_nrSumEnter = 0.0; // sum_i q_i^enter over active modules (excludes e)
  double m_nrSumEnterLogEnter = 0.0; // S = sum_i plogp(q_i^enter)
  std::array<double, nrSeriesK + 1> m_nrA {}; // A_k = sum_i q_i^exit (q_i^enter)^k
  std::array<double, nrSeriesK + 1> m_nrB {}; // B_k = sum_i q_i^exit (q_i^enter)^k plogp(q_i^enter)

  // Contribution of one module's enter + within codebooks, from its own statistics
  // (moduleFlow = p_i, qEnter = q_i^enter, qExit = q_i^exit, F = sum_{a in i} plogp(p_a)).
  double nrEnterWithin(double moduleFlow, double qEnter, double qExit, double F) const
  {
    if (moduleFlow < 1e-16)
      return 0.0;
    double tEnter = qEnter * (infomath::plogp(moduleFlow) - F) / moduleFlow;
    double tWithin = 0.0;
    double T = moduleFlow + qExit;
    if (T > 1e-16) {
      double usage = moduleFlow + qExit - qEnter; // = moduleFlow when undirected
      tWithin = (usage / T) * (infomath::plogp(T) - F - infomath::plogp(qExit));
    }
    return tEnter + tWithin;
  }

  // One module's exit codebook contribution given the global enter-rate total
  // sumEnterPlusExitNetwork (= sum_b q_b^enter + e) and sumEnterLogEnter (= sum_b
  // plogp(q_b^enter)); e is passed separately for the exit-to-parent codeword.
  double nrExitTerm(double qEnter, double qExit, double sumEnterPlusExitNetwork, double sumEnterLogEnter, double e) const
  {
    double Z = sumEnterPlusExitNetwork - qEnter;
    if (Z < 1e-16)
      return 0.0;
    return qExit * (infomath::plogp(Z) - (sumEnterLogEnter - infomath::plogp(qEnter)) - infomath::plogp(e)) / Z;
  }

  // Add (sign = +1) or remove (sign = -1) one module's contribution to the power-series
  // aggregates A_k, B_k (k = 0..nrSeriesK). O(K).
  void nrSeriesApply(double* A, double* B, double qEnter, double qExit, double sign) const
  {
    double pe = infomath::plogp(qEnter);
    double pk = 1.0; // (qEnter)^k
    for (int k = 0; k <= nrSeriesK; ++k) {
      double t = sign * qExit * pk;
      A[k] += t;
      B[k] += t * pe;
      pk *= qEnter;
    }
  }

  // Total exit codelength sum_i T^> from the power-series aggregates. O(K), independent of
  // the number of modules m. See the aggregate declarations above for the formula.
  double nrExitTotalSeries(const double* A, const double* B, double sumEnter, double S, double e) const
  {
    constexpr double INV_LN2 = 1.4426950408889634; // 1/ln(2) = log2(e)
    double D = sumEnter + e;
    if (D < 1e-16)
      return 0.0;
    double L = A[0] * infomath::log2(D); // Qx * log2(D)
    double Dpow = D; // D^k
    for (int k = 1; k <= nrSeriesK; ++k) {
      L -= A[k] * INV_LN2 / (k * Dpow); // A_k / (k D^k), 1/ln2 converts ln -> log2
      Dpow *= D;
    }
    double coef = S + infomath::plogp(e);
    Dpow = D; // D^(k+1)
    for (int k = 0; k <= nrSeriesK; ++k) {
      L += (B[k] - coef * A[k]) / Dpow; // (B_k - (S + plogp(e)) A_k) / D^(k+1)
      Dpow *= D;
    }
    return L;
  }
};

/**
 * Add teleportation flow for predefined move
 */
template <typename FlowDataType, typename DeltaFlowDataType>
void MapEquation<FlowDataType, DeltaFlowDataType>::addTeleportationFlow(InfoNode& current, const std::vector<FlowDataType>& moduleFlowData, DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta)
{
  if (!m_config.recordedTeleportation)
    return;

  auto& oldModuleFlowData = moduleFlowData[oldModuleDelta.module];
  double deltaEnterOld = (oldModuleFlowData.teleportFlow - current.data.teleportFlow) * current.data.teleportWeight;
  double deltaExitOld = current.data.teleportFlow * (oldModuleFlowData.teleportWeight - current.data.teleportWeight);
  oldModuleDelta.deltaEnter += deltaEnterOld;
  oldModuleDelta.deltaExit += deltaExitOld;

  auto& newModuleFlowData = moduleFlowData[newModuleDelta.module];
  double deltaEnterNew = current.data.teleportFlow * newModuleFlowData.teleportWeight;
  double deltaExitNew = newModuleFlowData.teleportFlow * current.data.teleportWeight;
  newModuleDelta.deltaEnter += deltaEnterNew;
  newModuleDelta.deltaExit += deltaExitNew;
}

template <typename FlowDataType, typename DeltaFlowDataType>
void MapEquation<FlowDataType, DeltaFlowDataType>::addTeleportationFlow(InfoNode& current, const std::vector<FlowDataType>& moduleFlowData, VectorMap<DeltaFlowDataType>& moduleDeltaFlow)
{
  if (!m_config.recordedTeleportation)
    return;

  auto& moduleDeltaEnterExit = moduleDeltaFlow.values();

  for (unsigned int j = 0; j < moduleDeltaFlow.size(); ++j) {
    auto& deltaEnterExit = moduleDeltaEnterExit[j];
    auto moduleIndex = deltaEnterExit.module;

    if (moduleIndex == current.index) {
      auto& oldModuleFlowData = moduleFlowData[moduleIndex];
      double deltaEnterOld = (oldModuleFlowData.teleportFlow - current.data.teleportFlow) * current.data.teleportWeight;
      double deltaExitOld = current.data.teleportFlow * (oldModuleFlowData.teleportWeight - current.data.teleportWeight);
      moduleDeltaFlow.add(moduleIndex, DeltaFlowDataType(moduleIndex, deltaExitOld, deltaEnterOld));
    } else {
      auto& newModuleFlowData = moduleFlowData[moduleIndex];
      double deltaEnterNew = newModuleFlowData.teleportFlow * current.data.teleportWeight;
      double deltaExitNew = current.data.teleportFlow * newModuleFlowData.teleportWeight;
      moduleDeltaFlow.add(moduleIndex, DeltaFlowDataType(moduleIndex, deltaExitNew, deltaEnterNew));
    }
  }
}

template <typename FlowDataType, typename DeltaFlowDataType>
INFOMAP_HOT double MapEquation<FlowDataType, DeltaFlowDataType>::getDeltaCodelengthOnMovingNode(InfoNode& current, DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData, std::vector<unsigned int>& moduleMembers)
{
  if (m_nonRedundant) {
    using infomath::plogp;
    unsigned int oldM = oldModuleDelta.module;
    unsigned int newM = newModuleDelta.module;
    double dOld = oldModuleDelta.deltaEnter + oldModuleDelta.deltaExit;
    double dNew = newModuleDelta.deltaEnter + newModuleDelta.deltaExit;

    const FlowDataType& oldMfd = moduleFlowData[oldM];
    const FlowDataType& newMfd = moduleFlowData[newM];
    const FlowDataType& cur = current.data;

    // Module statistics after the move (mirrors updateCodelengthOnMovingNode).
    double pOldA = oldMfd.flow - cur.flow;
    double qeOldA = oldMfd.enterFlow - cur.enterFlow + dOld;
    double qxOldA = oldMfd.exitFlow - cur.exitFlow + dOld;
    double pNewA = newMfd.flow + cur.flow;
    double qeNewA = newMfd.enterFlow + cur.enterFlow - dNew;
    double qxNewA = newMfd.exitFlow + cur.exitFlow - dNew;
    double curFlowLogFlow = current.nrFlowLogFlow; // sum of leaf plogp carried on the moving node
    double FOldA = m_moduleFlowLogFlow[oldM] - curFlowLogFlow;
    double FNewA = m_moduleFlowLogFlow[newM] + curFlowLogFlow;

    // (1) enter + within: only the two participating modules change  -> O(1)
    double dEnterWithin = (ME::nrEnterWithin(pOldA, qeOldA, qxOldA, FOldA) + ME::nrEnterWithin(pNewA, qeNewA, qxNewA, FNewA))
        - (ME::nrEnterWithin(oldMfd.flow, oldMfd.enterFlow, oldMfd.exitFlow, m_moduleFlowLogFlow[oldM])
           + ME::nrEnterWithin(newMfd.flow, newMfd.enterFlow, newMfd.exitFlow, m_moduleFlowLogFlow[newM]));

    // (2) exit codebook: the leave-one-out normalizer Z_i = D - q_i^enter couples all
    // modules through the global enter-rate total D.
    double e = exitNetworkFlow;
    double exitBefore = 0.0, exitAfter = 0.0;
    if (m_nrExact) {
      // Exact: recompute the full sum before and after the move  -> O(m).
      double sumEnterBefore = e, sumEnterAfter = e;
      double sumEnterLogBefore = 0.0, sumEnterLogAfter = 0.0;
      for (unsigned int i = 0; i < moduleFlowData.size(); ++i) {
        if (moduleMembers[i] == 0 && i != newM)
          continue;
        double qeBefore = moduleFlowData[i].enterFlow;
        double qeAfter = i == oldM ? qeOldA : (i == newM ? qeNewA : qeBefore);
        sumEnterBefore += qeBefore;
        sumEnterAfter += qeAfter;
        sumEnterLogBefore += plogp(qeBefore);
        sumEnterLogAfter += plogp(qeAfter);
      }
      for (unsigned int i = 0; i < moduleFlowData.size(); ++i) {
        if (moduleMembers[i] == 0 && i != newM)
          continue;
        double qeBefore = moduleFlowData[i].enterFlow;
        double qxBefore = moduleFlowData[i].exitFlow;
        double qeAfter = qeBefore, qxAfter = qxBefore;
        if (i == oldM) {
          qeAfter = qeOldA;
          qxAfter = qxOldA;
        } else if (i == newM) {
          qeAfter = qeNewA;
          qxAfter = qxNewA;
        }
        exitBefore += ME::nrExitTerm(qeBefore, qxBefore, sumEnterBefore, sumEnterLogBefore, e);
        exitAfter += ME::nrExitTerm(qeAfter, qxAfter, sumEnterAfter, sumEnterLogAfter, e);
      }
    } else {
      // Approximate: only the two participating modules change their contribution to the
      // running aggregates, so the exit total before/after is O(K) from the power series.
      double At[nrSeriesK + 1], Bt[nrSeriesK + 1];
      for (int k = 0; k <= nrSeriesK; ++k) {
        At[k] = m_nrA[k];
        Bt[k] = m_nrB[k];
      }
      ME::nrSeriesApply(At, Bt, oldMfd.enterFlow, oldMfd.exitFlow, -1.0);
      ME::nrSeriesApply(At, Bt, newMfd.enterFlow, newMfd.exitFlow, -1.0);
      ME::nrSeriesApply(At, Bt, qeOldA, qxOldA, +1.0);
      ME::nrSeriesApply(At, Bt, qeNewA, qxNewA, +1.0);
      double sumEnterT = m_nrSumEnter - oldMfd.enterFlow - newMfd.enterFlow + qeOldA + qeNewA;
      double sumEnterLogT = m_nrSumEnterLogEnter - plogp(oldMfd.enterFlow) - plogp(newMfd.enterFlow) + plogp(qeOldA) + plogp(qeNewA);
      exitBefore = ME::nrExitTotalSeries(m_nrA.data(), m_nrB.data(), m_nrSumEnter, m_nrSumEnterLogEnter, e);
      exitAfter = ME::nrExitTotalSeries(At, Bt, sumEnterT, sumEnterLogT, e);
    }

    return dEnterWithin + (exitAfter - exitBefore);
  }

  using infomath::plogp_batch;
  unsigned int oldModule = oldModuleDelta.module;
  unsigned int newModule = newModuleDelta.module;
  double deltaEnterExitOldModule = oldModuleDelta.deltaEnter + oldModuleDelta.deltaExit;
  double deltaEnterExitNewModule = newModuleDelta.deltaEnter + newModuleDelta.deltaExit;

  FlowDataType& oldMfd = moduleFlowData[oldModule];
  FlowDataType& newMfd = moduleFlowData[newModule];
  double oldEnter = oldMfd.enterFlow;
  double newEnter = newMfd.enterFlow;
  double oldExit = oldMfd.exitFlow;
  double newExit = newMfd.exitFlow;
  double oldExitPlusFlow = oldMfd.exitFlow + oldMfd.flow;
  double newExitPlusFlow = newMfd.exitFlow + newMfd.flow;
  double curEnter = current.data.enterFlow;
  double curExit = current.data.exitFlow;
  double curFlow = current.data.flow;

  constexpr int kPlogpBatchN = 13;
  double args[kPlogpBatchN] = {
    enterFlow + deltaEnterExitOldModule - deltaEnterExitNewModule,
    oldEnter,
    newEnter,
    oldEnter - curEnter + deltaEnterExitOldModule,
    newEnter + curEnter - deltaEnterExitNewModule,
    oldExit,
    newExit,
    oldExit - curExit + deltaEnterExitOldModule,
    newExit + curExit - deltaEnterExitNewModule,
    oldExitPlusFlow,
    newExitPlusFlow,
    oldExitPlusFlow - curExit - curFlow + deltaEnterExitOldModule,
    newExitPlusFlow + curExit + curFlow - deltaEnterExitNewModule,
  };
  double pl[kPlogpBatchN];
  plogp_batch(args, pl, kPlogpBatchN);

  double delta_enter = pl[0] - enterFlow_log_enterFlow;
  double delta_enter_log_enter = -pl[1] - pl[2] + pl[3] + pl[4];
  double delta_exit_log_exit = -pl[5] - pl[6] + pl[7] + pl[8];
  double delta_flow_log_flow = -pl[9] - pl[10] + pl[11] + pl[12];

  double deltaL = delta_enter - delta_enter_log_enter - delta_exit_log_exit + delta_flow_log_flow;
  return deltaL;
}

template <typename FlowDataType, typename DeltaFlowDataType>
void MapEquation<FlowDataType, DeltaFlowDataType>::updateCodelengthOnMovingNode(InfoNode& current, DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData, std::vector<unsigned int>& moduleMembers)
{
  using infomath::plogp;
  unsigned int oldModule = oldModuleDelta.module;
  unsigned int newModule = newModuleDelta.module;
  double deltaEnterExitOldModule = oldModuleDelta.deltaEnter + oldModuleDelta.deltaExit;
  double deltaEnterExitNewModule = newModuleDelta.deltaEnter + newModuleDelta.deltaExit;

  if (m_nonRedundant) {
    double curFlowLogFlow = current.nrFlowLogFlow; // sum of leaf plogp carried on the moving node

    if (m_nrExact) {
      // Exact: update the two participating modules' flow data and per-module F_i, then
      // recompute L* from the current module statistics (the exit term needs an O(m) sweep).
      moduleFlowData[oldModule] -= current.data;
      moduleFlowData[newModule] += current.data;
      moduleFlowData[oldModule].enterFlow += deltaEnterExitOldModule;
      moduleFlowData[oldModule].exitFlow += deltaEnterExitOldModule;
      moduleFlowData[newModule].enterFlow -= deltaEnterExitNewModule;
      moduleFlowData[newModule].exitFlow -= deltaEnterExitNewModule;
      m_moduleFlowLogFlow[oldModule] -= curFlowLogFlow;
      m_moduleFlowLogFlow[newModule] += curFlowLogFlow;

      double e = exitNetworkFlow;
      double sumEnter = e, sumEnterLogEnter = 0.0, moduleL = 0.0;
      for (unsigned int i = 0; i < moduleFlowData.size(); ++i) {
        if (moduleMembers[i] == 0 && i != newModule)
          continue;
        const FlowDataType& d = moduleFlowData[i];
        sumEnter += d.enterFlow;
        sumEnterLogEnter += plogp(d.enterFlow);
        moduleL += ME::nrEnterWithin(d.flow, d.enterFlow, d.exitFlow, m_moduleFlowLogFlow[i]);
      }
      double indexL = 0.0;
      for (unsigned int i = 0; i < moduleFlowData.size(); ++i) {
        if (moduleMembers[i] == 0 && i != newModule)
          continue;
        const FlowDataType& d = moduleFlowData[i];
        indexL += ME::nrExitTerm(d.enterFlow, d.exitFlow, sumEnter, sumEnterLogEnter, e);
      }
      moduleCodelength = moduleL;
      indexCodelength = indexL;
      codelength = indexL + moduleL;
      return;
    }

    // Approximate: enter+within is additive per module (O(1)); the exit term is the O(K)
    // power series over aggregates that only the two participating modules perturb. Both
    // moduleCodelength and the aggregates are maintained incrementally.
    FlowDataType& oldMfd = moduleFlowData[oldModule];
    FlowDataType& newMfd = moduleFlowData[newModule];
    double ewBefore = ME::nrEnterWithin(oldMfd.flow, oldMfd.enterFlow, oldMfd.exitFlow, m_moduleFlowLogFlow[oldModule])
        + ME::nrEnterWithin(newMfd.flow, newMfd.enterFlow, newMfd.exitFlow, m_moduleFlowLogFlow[newModule]);
    ME::nrSeriesApply(m_nrA.data(), m_nrB.data(), oldMfd.enterFlow, oldMfd.exitFlow, -1.0);
    ME::nrSeriesApply(m_nrA.data(), m_nrB.data(), newMfd.enterFlow, newMfd.exitFlow, -1.0);
    m_nrSumEnter -= oldMfd.enterFlow + newMfd.enterFlow;
    m_nrSumEnterLogEnter -= plogp(oldMfd.enterFlow) + plogp(newMfd.enterFlow);

    oldMfd -= current.data;
    newMfd += current.data;
    oldMfd.enterFlow += deltaEnterExitOldModule;
    oldMfd.exitFlow += deltaEnterExitOldModule;
    newMfd.enterFlow -= deltaEnterExitNewModule;
    newMfd.exitFlow -= deltaEnterExitNewModule;
    m_moduleFlowLogFlow[oldModule] -= curFlowLogFlow;
    m_moduleFlowLogFlow[newModule] += curFlowLogFlow;

    double ewAfter = ME::nrEnterWithin(oldMfd.flow, oldMfd.enterFlow, oldMfd.exitFlow, m_moduleFlowLogFlow[oldModule])
        + ME::nrEnterWithin(newMfd.flow, newMfd.enterFlow, newMfd.exitFlow, m_moduleFlowLogFlow[newModule]);
    ME::nrSeriesApply(m_nrA.data(), m_nrB.data(), oldMfd.enterFlow, oldMfd.exitFlow, +1.0);
    ME::nrSeriesApply(m_nrA.data(), m_nrB.data(), newMfd.enterFlow, newMfd.exitFlow, +1.0);
    m_nrSumEnter += oldMfd.enterFlow + newMfd.enterFlow;
    m_nrSumEnterLogEnter += plogp(oldMfd.enterFlow) + plogp(newMfd.enterFlow);

    moduleCodelength += ewAfter - ewBefore;
    indexCodelength = ME::nrExitTotalSeries(m_nrA.data(), m_nrB.data(), m_nrSumEnter, m_nrSumEnterLogEnter, exitNetworkFlow);
    codelength = indexCodelength + moduleCodelength;
    return;
  }

  enterFlow -= moduleFlowData[oldModule].enterFlow + moduleFlowData[newModule].enterFlow;
  enter_log_enter -= plogp(moduleFlowData[oldModule].enterFlow) + plogp(moduleFlowData[newModule].enterFlow);
  exit_log_exit -= plogp(moduleFlowData[oldModule].exitFlow) + plogp(moduleFlowData[newModule].exitFlow);
  flow_log_flow -= plogp(moduleFlowData[oldModule].exitFlow + moduleFlowData[oldModule].flow) + plogp(moduleFlowData[newModule].exitFlow + moduleFlowData[newModule].flow);

  moduleFlowData[oldModule] -= current.data;
  moduleFlowData[newModule] += current.data;

  moduleFlowData[oldModule].enterFlow += deltaEnterExitOldModule;
  moduleFlowData[oldModule].exitFlow += deltaEnterExitOldModule;
  moduleFlowData[newModule].enterFlow -= deltaEnterExitNewModule;
  moduleFlowData[newModule].exitFlow -= deltaEnterExitNewModule;

  enterFlow += moduleFlowData[oldModule].enterFlow + moduleFlowData[newModule].enterFlow;
  enter_log_enter += plogp(moduleFlowData[oldModule].enterFlow) + plogp(moduleFlowData[newModule].enterFlow);
  exit_log_exit += plogp(moduleFlowData[oldModule].exitFlow) + plogp(moduleFlowData[newModule].exitFlow);
  flow_log_flow += plogp(moduleFlowData[oldModule].exitFlow + moduleFlowData[oldModule].flow) + plogp(moduleFlowData[newModule].exitFlow + moduleFlowData[newModule].flow);

  enterFlow_log_enterFlow = plogp(enterFlow);

  indexCodelength = enterFlow_log_enterFlow - enter_log_enter - exitNetworkFlow_log_exitNetworkFlow;
  moduleCodelength = -exit_log_exit + flow_log_flow - nodeFlow_log_nodeFlow;
  codelength = indexCodelength + moduleCodelength;
}

template <typename FlowDataType, typename DeltaFlowDataType>
double MapEquation<FlowDataType, DeltaFlowDataType>::calcCodelengthOnModuleOfLeafNodes(const InfoNode& parent) const
{
  double parentFlow = parent.data.flow;
  double parentExit = parent.data.exitFlow;
  double totalParentFlow = parentFlow + parentExit;
  if (totalParentFlow < 1e-16)
    return 0.0;

  if (m_nonRedundant) {
    // The module's own enter + within codebooks. Its exit codebook (leave-one-out over
    // siblings) is charged at the parent level, in calcCodelengthOnModuleOfModules.
    double F = 0.0;
    for (const auto& node : parent)
      F += infomath::plogp(node.data.flow);
    return ME::nrEnterWithin(parentFlow, parent.data.enterFlow, parentExit, F);
  }

  double indexLength = 0.0;
  for (const auto& node : parent) {
    indexLength -= infomath::plogp(node.data.flow / totalParentFlow);
  }
  indexLength -= infomath::plogp(parentExit / totalParentFlow);

  indexLength *= totalParentFlow;

  return indexLength;
}

template <typename FlowDataType, typename DeltaFlowDataType>
double MapEquation<FlowDataType, DeltaFlowDataType>::calcCodelengthOnModuleOfModules(const InfoNode& parent) const
{
  double parentFlow = parent.data.flow;
  double parentExit = parent.data.exitFlow;
  if (parentFlow < 1e-16)
    return 0.0;

  if (m_nonRedundant) {
    // This level's codebooks over the parent's children: the parent's own enter codebook
    // (which child to enter, by enter rates) plus each child's exit codebook (leave-one-out
    // over its siblings, with exit-to-parent rate parentExit as the extra codeword).
    double sumEnter = 0.0, sumEnterLogEnter = 0.0;
    for (const auto& node : parent) {
      sumEnter += node.data.enterFlow;
      sumEnterLogEnter += infomath::plogp(node.data.enterFlow);
    }
    double L = 0.0;
    if (sumEnter > 1e-16) // enter codebook: pick a child submodule by enter rate
      L += parent.data.enterFlow * (infomath::plogp(sumEnter) - sumEnterLogEnter) / sumEnter;
    double sumEnterPlusE = sumEnter + parentExit;
    for (const auto& node : parent) // each child's exit codebook (leave-one-out over siblings)
      L += ME::nrExitTerm(node.data.enterFlow, node.data.exitFlow, sumEnterPlusE, sumEnterLogEnter, parentExit);
    return L;
  }

  // H(x) = -xlog(x), T = q + SUM(p), q = exitFlow, p = enterFlow
  // Normal format
  // L = q * -log(q/T) + SUM(p * -log(p/T))
  // Compact format
  // L = T * ( H(q/T) + SUM( H(p/T) ) )
  // Expanded format
  // L = q * -log(q) - q * -log(T) + SUM( p * -log(p) - p * -log(T) )
  // = T * log(T) - q*log(q) - SUM( p*log(p) )
  // = -H(T) + H(q) + SUM(H(p))
  // As T is not known, use expanded format to avoid two loops
  double sumEnter = 0.0;
  double sumEnterLogEnter = 0.0;
  for (const auto& node : parent) {
    sumEnter += node.data.enterFlow; // rate of enter to finer level
    sumEnterLogEnter += infomath::plogp(node.data.enterFlow);
  }
  // The possibilities from this module: Either exit to coarser level or enter one of its children
  double totalCodewordUse = parentExit + sumEnter;

  return infomath::plogp(totalCodewordUse) - sumEnterLogEnter - infomath::plogp(parentExit);
}

template <typename FlowDataType, typename DeltaFlowDataType>
void MapEquation<FlowDataType, DeltaFlowDataType>::calculateCodelengthTerms(std::vector<InfoNode*>& nodes)
{
  if (m_nonRedundant) {
    // (Re)seed all L* state for the current partition (called at every level's initPartition,
    // where every node is its own module -- node.index set by the optimizer). This is the one
    // point where the running exit aggregates are rebuilt; updateCodelengthOnMovingNode keeps
    // them in sync incrementally afterwards.
    m_moduleFlowLogFlow.assign(nodes.size(), 0.0);
    double sumEnter = 0.0; // sum_i q_i^enter (WITHOUT e; nrExitTotalSeries adds e internally)
    double sumEnterLogEnter = 0.0; // S = sum_i plogp(q_i^enter)
    double moduleL = 0.0;
    m_nrA.fill(0.0);
    m_nrB.fill(0.0);
    for (InfoNode* n : nodes) {
      const FlowDataType& d = n->data;
      // F_i is the sum of plogp(leaf visit flow) over this node's leaves: for a leaf
      // node it equals plogp(flow); for a consolidated module node it is the value
      // carried by consolidateModules. Never plogp(module flow), which would be wrong.
      double F = n->nrFlowLogFlow;
      m_moduleFlowLogFlow[n->index] = F;
      sumEnter += d.enterFlow;
      sumEnterLogEnter += infomath::plogp(d.enterFlow);
      moduleL += ME::nrEnterWithin(d.flow, d.enterFlow, d.exitFlow, F);
      if (!m_nrExact)
        ME::nrSeriesApply(m_nrA.data(), m_nrB.data(), d.enterFlow, d.exitFlow, +1.0);
    }
    m_nrSumEnter = sumEnter;
    m_nrSumEnterLogEnter = sumEnterLogEnter;
    double indexL;
    if (m_nrExact) {
      indexL = 0.0;
      double sumEnterPlusE = sumEnter + exitNetworkFlow; // D
      for (InfoNode* n : nodes) {
        const FlowDataType& d = n->data;
        indexL += ME::nrExitTerm(d.enterFlow, d.exitFlow, sumEnterPlusE, sumEnterLogEnter, exitNetworkFlow);
      }
    } else {
      indexL = ME::nrExitTotalSeries(m_nrA.data(), m_nrB.data(), sumEnter, sumEnterLogEnter, exitNetworkFlow);
    }
    moduleCodelength = moduleL;
    indexCodelength = indexL;
    codelength = indexL + moduleL;
    return;
  }

  enter_log_enter = 0.0;
  flow_log_flow = 0.0;
  exit_log_exit = 0.0;
  enterFlow = 0.0;

  // For each module
  for (InfoNode* n : nodes) {
    InfoNode& node = *n;
    // own node/module codebook
    flow_log_flow += infomath::plogp(node.data.flow + node.data.exitFlow);

    // use of index codebook
    enter_log_enter += infomath::plogp(node.data.enterFlow);
    exit_log_exit += infomath::plogp(node.data.exitFlow);
    enterFlow += node.data.enterFlow;
  }
  enterFlow += exitNetworkFlow;
  enterFlow_log_enterFlow = infomath::plogp(enterFlow);
}

} // namespace infomap

#endif // MAPEQUATION_H_
