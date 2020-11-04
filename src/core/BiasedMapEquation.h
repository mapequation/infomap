/*
 * BiasedMapEquation.h
 */

#ifndef _BIASEDMAPEQUATION_H_
#define _BIASEDMAPEQUATION_H_

#include <vector>
#include <set>
#include <map>
#include <utility>
#include "MapEquation.h"
#include "FlowData.h"
#include "../utils/Log.h"

namespace infomap {

class InfoNode;

class BiasedMapEquation : protected MapEquation {
  using Base = MapEquation;

public:
  using Base::FlowDataType;
  using Base::DeltaFlowDataType;

  // ===================================================
  // Getters
  // ===================================================

  static bool haveMemory() noexcept{ return true; }

  using Base::getIndexCodelength;

  double getModuleCodelength() const noexcept override;

  double getCodelength() const noexcept override;

  // ===================================================
  // IO
  // ===================================================

  std::ostream& print(std::ostream& out) const;

  friend std::ostream& operator<<(std::ostream&, const BiasedMapEquation&);

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

  double calcCodelength(const InfoNode& parent) const;

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

  void consolidateModules(std::vector<InfoNode*>& modules);

  // ===================================================
  // Debug
  // ===================================================

  void printDebug() const override;

protected:
  // ===================================================
  // Protected member functions
  // ===================================================

  double calcCodelengthOnModuleOfLeafNodes(const InfoNode& parent) const;

  int getDeltaNumModulesIfMoving(InfoNode& current,
                                 unsigned int oldModule,
                                 unsigned int newModule,
                                 std::vector<unsigned int>& moduleMembers) const;

  // ===================================================
  // Codelength
  // ===================================================

  void calculateCodelength(std::vector<InfoNode*>& nodes);

  using Base::calculateCodelengthTerms;

  using Base::calculateCodelengthFromCodelengthTerms;

  double calcNumModuleCost(unsigned int numModules) const;

public:
  // ===================================================
  // Public member variables
  // ===================================================

  using Base::codelength;
  using Base::indexCodelength;
  using Base::moduleCodelength;

protected:
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
};


} // namespace infomap

#endif /* _BIASEDMAPEQUATION_H_ */
