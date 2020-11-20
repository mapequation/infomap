/*
 * MapEquation.h
 *
 *  Created on: 25 feb 2015
 *      Author: Daniel
 */

#ifndef SRC_CLUSTERING_MAPEQUATION_H_
#define SRC_CLUSTERING_MAPEQUATION_H_

#include "../utils/infomath.h"
#include "../io/convert.h"
#include "../io/Config.h"
#include "../utils/Log.h"
#include "../utils/VectorMap.h"
// #include "InfoNode.h"
#include "FlowData.h"
#include <vector>
#include <map>
#include <iostream>

namespace infomap {

class InfoNode;
// struct Config;

class MapEquation {
public:
  using FlowDataType = FlowData;
  using FlowType = FlowData::FlowType;
  // using DeltaFlowDataType = MemDeltaFlow;
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
        exitNetworkFlow_log_exitNetworkFlow(other.exitNetworkFlow_log_exitNetworkFlow) {}

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

  static bool haveMemory() { return false; }

  FlowType getIndexCodelength() const { return indexCodelength; }

  FlowType getModuleCodelength() const { return moduleCodelength; }

  FlowType getCodelength() const { return codelength; }

  // ===================================================
  // IO
  // ===================================================

  std::ostream& print(std::ostream&) const;

  friend std::ostream& operator<<(std::ostream&, const MapEquation&);


  // ===================================================
  // Init
  // ===================================================

  void init(const Config& config);

  void initNetwork(InfoNode& root);

  void initSuperNetwork(InfoNode& root);

  void initSubNetwork(InfoNode& root);

  void initPartition(std::vector<InfoNode*>& nodes);

  // ===================================================
  // Codelength
  // ===================================================

  FlowType calcCodelength(const InfoNode& parent) const;

  void addMemoryContributions(InfoNode& current, DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta) {}

  void addMemoryContributions(InfoNode& current, DeltaFlowDataType& oldModuleDelta, VectorMap<DeltaFlowDataType>& moduleDeltaFlow) {}

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

  void consolidateModules(std::vector<InfoNode*>& modules) {}

  // ===================================================
  // Debug
  // ===================================================

  void printDebug();


protected:
  // ===================================================
  // Protected member functions
  // ===================================================

  FlowType calcCodelengthOnModuleOfLeafNodes(const InfoNode& parent) const;

  FlowType calcCodelengthOnModuleOfModules(const InfoNode& parent) const;

  void calculateCodelength(std::vector<InfoNode*>& nodes);

  void calculateCodelengthTerms(std::vector<InfoNode*>& nodes);

  void calculateCodelengthFromCodelengthTerms();


public:
  // ===================================================
  // Public member variables
  // ===================================================

  FlowType codelength{};
  FlowType indexCodelength{};
  FlowType moduleCodelength{};

protected:
  // ===================================================
  // Protected member variables
  // ===================================================

  FlowType nodeFlow_log_nodeFlow{}; // constant while the leaf network is the same
  FlowType flow_log_flow{}; // node.(flow + exitFlow)
  FlowType exit_log_exit{};
  FlowType enter_log_enter{};
  FlowType enterFlow{};
  FlowType enterFlow_log_enterFlow{};

  // For hierarchical
  FlowType exitNetworkFlow{};
  FlowType exitNetworkFlow_log_exitNetworkFlow{};
};

} // namespace infomap


#endif /* SRC_CLUSTERING_MAPEQUATION_H_ */
