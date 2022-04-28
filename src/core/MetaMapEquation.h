/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Eriksson, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_AGPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef _METAMAPEQUATION_H_
#define _METAMAPEQUATION_H_

#include "MapEquation.h"
#include "FlowData.h"
#include "../utils/Log.h"
#include "../utils/MetaCollection.h"
#include <vector>
#include <set>
#include <map>
#include <utility>

namespace infomap {

class InfoNode;

class MetaMapEquation : protected MapEquation {
  using Base = MapEquation;

public:
  using FlowDataType = FlowData;
  using DeltaFlowDataType = DeltaFlow;

  MetaMapEquation() = default;

  MetaMapEquation(const MetaMapEquation& other)
      : MapEquation(other),
        m_moduleToMetaCollection(other.m_moduleToMetaCollection),
        numMetaDataDimensions(other.numMetaDataDimensions),
        metaDataRate(other.metaDataRate),
        weightByFlow(other.weightByFlow),
        metaCodelength(other.metaCodelength) { }

  MetaMapEquation& operator=(const MetaMapEquation& other)
  {
    Base::operator=(other);
    m_moduleToMetaCollection = other.m_moduleToMetaCollection;
    numMetaDataDimensions = other.numMetaDataDimensions;
    metaDataRate = other.metaDataRate;
    weightByFlow = other.weightByFlow;
    metaCodelength = other.metaCodelength;
    return *this;
  }

  ~MetaMapEquation() override = default;

  // ===================================================
  // Getters
  // ===================================================

  using Base::getIndexCodelength;

  double getModuleCodelength() const override;

  double getCodelength() const override;

  double getMetaCodelength(bool unweighted = false) const
  {
    return unweighted ? metaCodelength : metaDataRate * metaCodelength;
  };

  // ===================================================
  // IO
  // ===================================================

  std::ostream& print(std::ostream& out) const override;
  friend std::ostream& operator<<(std::ostream&, const MetaMapEquation&);

  // ===================================================
  // Init
  // ===================================================

  void init(const Config& config) override;

  void initTree(InfoNode& root) override;

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

  // ===================================================
  // Init
  // ===================================================

  void initMetaNodes(InfoNode& root);

  void initPartitionOfMetaNodes(std::vector<InfoNode*>& nodes);

  // ===================================================
  // Codelength
  // ===================================================

  void calculateCodelength(std::vector<InfoNode*>& nodes) override;

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
  double getCurrentModuleMetaCodelength(unsigned int module, InfoNode& current, int addRemoveOrNothing);

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

  ModuleMetaMap m_moduleToMetaCollection;

  unsigned int numMetaDataDimensions = 0;
  double metaDataRate = 1.0;
  bool weightByFlow = true;
  double metaCodelength = 0.0;
  double m_unweightedNodeFlow = 0.0;
};

} // namespace infomap

#endif /* _METAMAPEQUATION_H_ */
