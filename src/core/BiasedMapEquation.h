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
// #include "InfoNode.h"
#include "../utils/Log.h"

namespace infomap {

class InfoNode;

class BiasedMapEquation : protected MapEquation {
  using Base = MapEquation;

public:
  using FlowDataType = FlowData;
  using DeltaFlowDataType = DeltaFlow;

  BiasedMapEquation() : MapEquation() {}

  BiasedMapEquation(const BiasedMapEquation& other)
      : MapEquation(other),
        preferredNumModules(other.preferredNumModules),
        currentNumModules(other.currentNumModules),
        biasedCost(other.biasedCost)
  {
  }

  BiasedMapEquation& operator=(const BiasedMapEquation& other)
  {
    Base::operator=(other);
    preferredNumModules = other.preferredNumModules;
    currentNumModules = other.currentNumModules;
    biasedCost = other.biasedCost;
    return *this;
  }

  virtual ~BiasedMapEquation() = default;

  // ===================================================
  // Getters
  // ===================================================

  using Base::getIndexCodelength;

  // double getModuleCodelength() const { return moduleCodelength + metaCodelength; };
  double getModuleCodelength() const;

  // double getCodelength() const { return codelength + metaCodelength; };
  double getCodelength() const;

  // ===================================================
  // IO
  // ===================================================

  // using Base::print;
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

  void printDebug();

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
  // Init
  // ===================================================

  // void initMetaNodes(InfoNode& root);

  // void initPartitionOfMetaNodes(std::vector<InfoNode*>& nodes);

  // ===================================================
  // Codelength
  // ===================================================

  void calculateCodelength(std::vector<InfoNode*>& nodes);

  using Base::calculateCodelengthTerms;

  using Base::calculateCodelengthFromCodelengthTerms;

  double calcNumModuleCost(unsigned int numModules) const;

  // ===================================================
  // Consolidation
  // ===================================================

  // void updateMetaData(InfoNode& current, unsigned int oldModuleIndex, unsigned int bestModuleIndex);

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

  /**
   *  Get meta codelength of module of current node
   * @param addRemoveOrNothing +1, -1 or 0 to calculate codelength
   * as if current node was added, removed or untouched in current module
   */
  // double getCurrentModuleMetaCodelength(unsigned int module, InfoNode& current, int addRemoveOrNothing);

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
