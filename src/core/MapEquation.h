/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Eriksson, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_AGPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef MAPEQUATION_H_
#define MAPEQUATION_H_

#include "../utils/infomath.h"
#include "../utils/convert.h"
#include "../io/Config.h"
#include "../utils/Log.h"
#include "../utils/VectorMap.h"
#include "FlowData.h"
#include <vector>
#include <map>
#include <iostream>

namespace infomap {

class InfoNode;

class MapEquation {
public:
  using FlowDataType = FlowData;
  using DeltaFlowDataType = DeltaFlow;

  MapEquation() = default;

  MapEquation(const MapEquation& other)
      : codelength(other.codelength),
        indexCodelength(other.indexCodelength),
        moduleCodelength(other.moduleCodelength),
        nodeFlow_log_nodeFlow(other.nodeFlow_log_nodeFlow),
        flow_log_flow(other.flow_log_flow),
        exit_log_exit(other.exit_log_exit),
        enter_log_enter(other.enter_log_enter),
        enterFlow(other.enterFlow),
        enterFlow_log_enterFlow(other.enterFlow_log_enterFlow),
        exitNetworkFlow(other.exitNetworkFlow),
        exitNetworkFlow_log_exitNetworkFlow(other.exitNetworkFlow_log_exitNetworkFlow) { }

  MapEquation& operator=(const MapEquation& other)
  {
    codelength = other.codelength;
    indexCodelength = other.indexCodelength;
    moduleCodelength = other.moduleCodelength;
    nodeFlow_log_nodeFlow = other.nodeFlow_log_nodeFlow;
    flow_log_flow = other.flow_log_flow;
    exit_log_exit = other.exit_log_exit;
    enter_log_enter = other.enter_log_enter;
    enterFlow = other.enterFlow;
    enterFlow_log_enterFlow = other.enterFlow_log_enterFlow;
    exitNetworkFlow = other.exitNetworkFlow;
    exitNetworkFlow_log_exitNetworkFlow = other.exitNetworkFlow_log_exitNetworkFlow;
    return *this;
  }

  virtual ~MapEquation() = default;

  // ===================================================
  // Getters
  // ===================================================

  double getIndexCodelength() const { return indexCodelength; }

  double getModuleCodelength() const { return moduleCodelength; }

  double getCodelength() const { return codelength; }

  // ===================================================
  // IO
  // ===================================================

  std::ostream& print(std::ostream&) const;

  friend std::ostream& operator<<(std::ostream&, const MapEquation&);

  // ===================================================
  // Init
  // ===================================================

  void init(const Config& config);

  void initTree(InfoNode& /*root*/) { }

  void initNetwork(InfoNode& root);

  void initSuperNetwork(InfoNode& root);

  void initSubNetwork(InfoNode& root);

  void initPartition(std::vector<InfoNode*>& nodes);

  // ===================================================
  // Codelength
  // ===================================================

  double calcCodelength(const InfoNode& parent) const;

  void addMemoryContributions(InfoNode& /*current*/, DeltaFlowDataType& /*oldModuleDelta*/, DeltaFlowDataType& /*newModuleDelta*/) { }

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

  void consolidateModules(std::vector<InfoNode*>& /*modules*/) { }

  // ===================================================
  // Debug
  // ===================================================

  void printDebug();

protected:
  // ===================================================
  // Protected member functions
  // ===================================================

  double calcCodelengthOnModuleOfLeafNodes(const InfoNode& parent) const;

  double calcCodelengthOnModuleOfModules(const InfoNode& parent) const;

  void calculateCodelength(std::vector<InfoNode*>& nodes);

  void calculateCodelengthTerms(std::vector<InfoNode*>& nodes);

  void calculateCodelengthFromCodelengthTerms();

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

  double nodeFlow_log_nodeFlow = 0.0; // constant while the leaf network is the same
  double flow_log_flow = 0.0; // node.(flow + exitFlow)
  double exit_log_exit = 0.0;
  double enter_log_enter = 0.0;
  double enterFlow = 0.0;
  double enterFlow_log_enterFlow = 0.0;

  // For hierarchical
  double exitNetworkFlow = 0.0;
  double exitNetworkFlow_log_exitNetworkFlow = 0.0;
};

} // namespace infomap

#endif // MAPEQUATION_H_
