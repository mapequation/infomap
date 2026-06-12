/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef BIASED_MAPEQUATION_H_
#define BIASED_MAPEQUATION_H_

#include "MapEquation.h"
#include "FlowData.h"
#include "../utils/Log.h"
#include <vector>
#include <set>
#include <map>
#include <utility>

namespace infomap {

class InfoNode;
class StateNetwork;

class BiasedMapEquation final : private MapEquation<> {
  using Base = MapEquation<>;

public:
  using FlowDataType = FlowData;
  using DeltaFlowDataType = DeltaFlow;

  // ===================================================
  // Getters
  // ===================================================

  double getIndexCodelength() const;

  double getModuleCodelength() const;

  double getCodelength() const;

  double getEntropyBiasCorrection() const;

  // ===================================================
  // IO
  // ===================================================

  std::ostream& print(std::ostream& out) const;

  friend std::ostream& operator<<(std::ostream&, const BiasedMapEquation&);

  // ===================================================
  // Init
  // ===================================================

  void init(const Config& config);

  void initTree(InfoNode& /*root*/) {}

  void initNetwork(InfoNode& root);

  using Base::initSuperNetwork;

  using Base::initSubNetwork;

  void initPartition(std::vector<InfoNode*>& nodes);

  // ===================================================
  // Codelength
  // ===================================================

  double calcCodelength(const InfoNode& parent) const;

  using Base::addMemoryContributions;

  using Base::addTeleportationFlow;

  double getDeltaCodelengthOnMovingNode(InfoNode& current,
                                        DeltaFlow& oldModuleDelta,
                                        DeltaFlow& newModuleDelta,
                                        std::vector<FlowData>& moduleFlowData,
                                        std::vector<unsigned int>& moduleMembers);

  // ===================================================
  // Consolidation
  // ===================================================

  void updateCodelengthOnMovingNode(InfoNode& current,
                                    DeltaFlow& oldModuleDelta,
                                    DeltaFlow& newModuleDelta,
                                    std::vector<FlowData>& moduleFlowData,
                                    std::vector<unsigned int>& moduleMembers);

  void consolidateModules(std::vector<InfoNode*>& modules);

  // ===================================================
  // Debug
  // ===================================================

  void printDebug() const;

private:
  // ===================================================
  // Private member functions
  // ===================================================
  double calcCodelengthOnModuleOfLeafNodes(const InfoNode& parent) const;
  double calcCodelengthOnModuleOfModules(const InfoNode& parent) const;

  static int getDeltaNumModulesIfMoving(unsigned int oldModule, unsigned int newModule, std::vector<unsigned int>& moduleMembers);

  // ===================================================
  // Init
  // ===================================================

  // ===================================================
  // Codelength
  // ===================================================

  void calculateCodelength(std::vector<InfoNode*>& nodes);

  using Base::calculateCodelengthTerms;

  using Base::calculateCodelengthFromCodelengthTerms;

  double calcNumModuleCost(unsigned int numModules) const;

  double calcIndexEntropyBiasCorrection(unsigned int numModules) const;
  double calcModuleEntropyBiasCorrection() const;
  double calcEntropyBiasCorrection(unsigned int numModules) const;

  // ===================================================
  // Consolidation
  // ===================================================

public:
  // ===================================================
  // Public member variables
  // ===================================================

  using Base::codelength;
  using Base::indexCodelength;
  using Base::moduleCodelength;

private:
  // ===================================================
  // Private member variables
  // ===================================================

  using Base::enter_log_enter;
  using Base::enterFlow;
  using Base::enterFlow_log_enterFlow;
  using Base::exit_log_exit;
  using Base::flow_log_flow; // node.(flow + exitFlow)
  using Base::nodeFlow_log_nodeFlow; // constant while the leaf network is the same

  // For hierarchical
  using Base::exitNetworkFlow;
  using Base::exitNetworkFlow_log_exitNetworkFlow;

  // For biased
  unsigned int preferredNumModules = 0;
  unsigned int currentNumModules = 0;
  double biasedCost = 0.0;

  // For entropy bias correction
  bool useEntropyBiasCorrection = false;
  double entropyBiasCorrectionMultiplier = 1;
  double indexEntropyBiasCorrection = 0;
  double moduleEntropyBiasCorrection = 0;
  double m_totalDegree = 1;
  unsigned int m_numNodes = 0;

public:
  void setNetworkProperties(const StateNetwork& network);

  // Copy the whole-network properties from a parent objective. Sub/super Infomap instances
  // partition InfoNodes and have no StateNetwork of their own, but the entropy bias correction
  // must keep using the full network's total degree and node count (previously a shared static).
  void setNetworkPropertiesFrom(const BiasedMapEquation& other)
  {
    m_totalDegree = other.m_totalDegree;
    m_numNodes = other.m_numNodes;
  }
};

} // namespace infomap

#endif // BIASED_MAPEQUATION_H_
