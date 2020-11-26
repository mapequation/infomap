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
// #include "InfoNode.h"
#include "../utils/Log.h"
#include "../utils/MetaCollection.h"

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
        metaCodelength(other.metaCodelength) {}

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

  virtual ~MetaMapEquation() = default;

  // ===================================================
  // Getters
  // ===================================================

  using Base::getIndexCodelength;

  // double getModuleCodelength() const { return moduleCodelength + metaCodelength; };
  double getModuleCodelength() const;

  // double getCodelength() const { return codelength + metaCodelength; };
  double getCodelength() const;

  double getMetaCodelength(bool unweighted = false) const
  {
    return unweighted ? metaCodelength : metaDataRate * metaCodelength;
  };

  // ===================================================
  // IO
  // ===================================================

  // using Base::print;
  std::ostream& print(std::ostream& out) const;
  friend std::ostream& operator<<(std::ostream&, const MetaMapEquation&);

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
