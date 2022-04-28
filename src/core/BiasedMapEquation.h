/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Eriksson, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_AGPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef _BIASEDMAPEQUATION_H_
#define _BIASEDMAPEQUATION_H_

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

class BiasedMapEquation : protected MapEquation {
  using Base = MapEquation;

public:
  using FlowDataType = FlowData;
  using DeltaFlowDataType = DeltaFlow;

  BiasedMapEquation() : MapEquation() { }

  BiasedMapEquation(const BiasedMapEquation& other)
      : MapEquation(other),
        preferredNumModules(other.preferredNumModules),
        currentNumModules(other.currentNumModules),
        biasedCost(other.biasedCost),
        useEntropyBiasCorrection(other.useEntropyBiasCorrection),
        indexEntropyBiasCorrection(other.indexEntropyBiasCorrection),
        moduleEntropyBiasCorrection(other.moduleEntropyBiasCorrection),
        gamma(other.gamma)
  {
  }

  BiasedMapEquation& operator=(const BiasedMapEquation& other)
  {
    Base::operator=(other);
    preferredNumModules = other.preferredNumModules;
    currentNumModules = other.currentNumModules;
    biasedCost = other.biasedCost;
    useEntropyBiasCorrection = other.useEntropyBiasCorrection;
    indexEntropyBiasCorrection = other.indexEntropyBiasCorrection;
    moduleEntropyBiasCorrection = other.moduleEntropyBiasCorrection;
    gamma = other.gamma;
    return *this;
  }

  ~BiasedMapEquation() override = default;

  // ===================================================
  // Getters
  // ===================================================

  double getIndexCodelength() const override;

  double getModuleCodelength() const override;

  double getCodelength() const override;

  double getEntropyBiasCorrection() const;

  // ===================================================
  // IO
  // ===================================================

  std::ostream& print(std::ostream& out) const override;
  friend std::ostream& operator<<(std::ostream&, const BiasedMapEquation&);

  // ===================================================
  // Init
  // ===================================================

  void init(const Config& config) override;

  void initTree(InfoNode& /*root*/) override { }

  void initNetwork(InfoNode& root) override;

  void initSuperNetwork(InfoNode& root) override;

  void initSubNetwork(InfoNode& root) override;

  void initPartition(std::vector<InfoNode*>& nodes) override;

  // ===================================================
  // Codelength
  // ===================================================

  double calcCodelength(const InfoNode& parent) const override;

  void addMemoryContributions(InfoNode& /*current*/, DeltaFlowDataType& /*oldModuleDelta*/, VectorMap<DeltaFlowDataType>& /*moduleDeltaFlow*/) { }

  double getDeltaCodelengthOnMovingNode(InfoNode& current,
                                        DeltaFlowDataType& oldModuleDelta,
                                        DeltaFlowDataType& newModuleDelta,
                                        std::vector<FlowDataType>& moduleFlowData,
                                        std::vector<unsigned int>& moduleMembers);

  // ===================================================
  // Consolidation
  // ===================================================

  void updateCodelengthOnMovingNode(InfoNode& current,
                                    DeltaFlowDataType& oldModuleDelta,
                                    DeltaFlowDataType& newModuleDelta,
                                    std::vector<FlowDataType>& moduleFlowData,
                                    std::vector<unsigned int>& moduleMembers);

  void consolidateModules(std::vector<InfoNode*>& modules) override;

  // ===================================================
  // Debug
  // ===================================================

  void printDebug() override;

protected:
  // ===================================================
  // Protected member functions
  // ===================================================
  double calcCodelengthOnModuleOfLeafNodes(const InfoNode& parent) const override;
  double calcCodelengthOnModuleOfModules(const InfoNode& parent) const override;

  int getDeltaNumModulesIfMoving(InfoNode& current,
                                 unsigned int oldModule,
                                 unsigned int newModule,
                                 std::vector<unsigned int>& moduleMembers) const;

  // ===================================================
  // Init
  // ===================================================

  // ===================================================
  // Codelength
  // ===================================================

  void calculateCodelength(std::vector<InfoNode*>& nodes) override;

  using Base::calculateCodelengthTerms;

  using Base::calculateCodelengthFromCodelengthTerms;

  double calcNumModuleCost(unsigned int numModules) const;

  double calcIndexEntropyBiasCorrection(unsigned int numModules) const;
  double calcModuleEntropyBiasCorrection(unsigned int numModules) const;
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

protected:
  // ===================================================
  // Protected member functions
  // ===================================================

  double correctionCoefficient() const { return gamma * entropyBiasCorrectionMultiplier; }

  // ===================================================
  // Protected member variables
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
  double gamma = 0.7;
  static double s_totalDegree;
  static unsigned int s_numNodes;

public:
  static void setNetworkProperties(const StateNetwork& network);
};

} // namespace infomap

#endif /* _BIASEDMAPEQUATION_H_ */
