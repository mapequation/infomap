/*
 * MapEquation.h
 *
 *  Created on: 25 feb 2015
 *      Author: Daniel
 */

#ifndef _MAPEQUATION_H_
#define _MAPEQUATION_H_

#include "../utils/infomath.h"
#include "../io/convert.h"
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

  // ===================================================
  // Getters
  // ===================================================

  static bool haveMemory() noexcept { return false; }

  virtual double getIndexCodelength() const noexcept { return indexCodelength; }

  virtual double getModuleCodelength() const noexcept { return moduleCodelength; }

  virtual double getCodelength() const noexcept { return codelength; }

  // ===================================================
  // IO
  // ===================================================

  virtual std::ostream& print(std::ostream&) const noexcept;

  friend std::ostream& operator<<(std::ostream&, const MapEquation&);


  // ===================================================
  // Init
  // ===================================================

  virtual void init(const Config& config) noexcept;

  virtual void initNetwork(InfoNode& root) noexcept;

  virtual void initSuperNetwork(InfoNode& root) noexcept;

  virtual void initSubNetwork(InfoNode& root) noexcept;

  virtual void initPartition(std::vector<InfoNode*>& nodes) noexcept;

  // ===================================================
  // Codelength
  // ===================================================

  virtual double calcCodelength(const InfoNode& parent) const;

  void addMemoryContributions(InfoNode& current, DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta) { }

  void addMemoryContributions(InfoNode& current, DeltaFlowDataType& oldModuleDelta, VectorMap<DeltaFlowDataType>& moduleDeltaFlow) { }

  double getDeltaCodelengthOnMovingNode(InfoNode& current,
                                                DeltaFlowDataType& oldModuleDelta,
                                                DeltaFlowDataType& newModuleDelta,
                                                std::vector<FlowDataType>& moduleFlowData,
                                                std::vector<unsigned int>& moduleMembers) const;

  // ===================================================
  // Consolidation
  // ===================================================

  void updateCodelengthOnMovingNode(InfoNode& current,
                                    DeltaFlowDataType& oldModuleDelta,
                                    DeltaFlowDataType& newModuleDelta,
                                    std::vector<FlowDataType>& moduleFlowData,
                                    std::vector<unsigned int>& moduleMembers);

  virtual void consolidateModules(std::vector<InfoNode*>& modules) { }

  // ===================================================
  // Debug
  // ===================================================

  virtual void printDebug() const;


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


#endif /* _MAPEQUATION_H_ */
