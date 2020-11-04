/*
 * MetaMapEquation.h
 */

#ifndef _METAMAPEQUATION_H_
#define _METAMAPEQUATION_H_

#include <vector>
#include <set>
#include <map>
#include <utility>
#include "MapEquation.h"
#include "FlowData.h"
#include "../utils/Log.h"
#include "../utils/MetaCollection.h"

namespace infomap {

class InfoNode;

class MetaMapEquation : protected MapEquation {
  using Base = MapEquation;

public:
  using Base::DeltaFlowDataType;
  using Base::FlowDataType;

  // ===================================================
  // Getters
  // ===================================================

  static bool haveMemory() noexcept { return true; }

  using Base::getIndexCodelength;

  double getModuleCodelength() const noexcept override;

  double getCodelength() const noexcept override;

  double getMetaCodelength(bool unweighted = false) const noexcept
  {
    return unweighted ? metaCodelength : metaDataRate * metaCodelength;
  };

  // ===================================================
  // IO
  // ===================================================

  std::ostream& print(std::ostream& out) const noexcept override;

  friend std::ostream& operator<<(std::ostream&, const MetaMapEquation&);

  // ===================================================
  // Init
  // ===================================================

  void init(const Config& config) noexcept override;

  void initNetwork(InfoNode& root) noexcept override;

  using Base::initSuperNetwork;

  using Base::initSubNetwork;

  void initPartition(std::vector<InfoNode*>& nodes) noexcept override;

  // ===================================================
  // Codelength
  // ===================================================

  double calcCodelength(const InfoNode& parent) const override;

  using Base::addMemoryContributions;

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

  void consolidateModules(std::vector<InfoNode*>& modules) override;

  // ===================================================
  // Debug
  // ===================================================

  void printDebug() const override;

protected:
  // ===================================================
  // Protected member functions
  // ===================================================
  double calcCodelengthOnModuleOfLeafNodes(const InfoNode& parent) const;

  // ===================================================
  // Init
  // ===================================================

  void initMetaNodes(InfoNode& root);

  void initPartitionOfMetaNodes(std::vector<InfoNode*>& nodes);

  // ===================================================
  // Codelength
  // ===================================================

  void calculateCodelength(std::vector<InfoNode*>& nodes);

  using Base::calculateCodelengthTerms;

  using Base::calculateCodelengthFromCodelengthTerms;

  // ===================================================
  // Consolidation
  // ===================================================

  void updateMetaData(InfoNode& current, unsigned int oldModuleIndex, unsigned int bestModuleIndex);

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
  double getCurrentModuleMetaCodelength(unsigned int module, InfoNode& current, int addRemoveOrNothing) const;

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

  // For meta data
  using ModuleMetaMap = std::map<unsigned int, MetaCollection>; // moduleId -> (metaId -> count)

  // Mutable to make getDeltaCodelengthOnMovingNode const
  mutable ModuleMetaMap m_moduleToMetaCollection;

  unsigned int numMetaDataDimensions = 0;
  double metaDataRate = 1.0;
  bool weightByFlow = true;
  double metaCodelength = 0.0;
  double m_unweightedNodeFlow = 0.0;
};


} // namespace infomap

#endif /* _METAMAPEQUATION_H_ */
