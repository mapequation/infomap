/*
 * MetaMapEquation.h
 */

#ifndef _METAMAPEQUATION_H_
#define _METAMAPEQUATION_H_

#include "MapEquation.h"
#include "../utils/MetaCollection.h"
#include <vector>
#include <map>

namespace infomap {

class InfoNode;

class MetaMapEquation final : public MapEquation {
  using Base = MapEquation;

public:
  // ===================================================
  // Getters
  // ===================================================

  static bool haveMemory() noexcept { return true; }

  double getModuleCodelength() const noexcept final;

  double getCodelength() const noexcept final;

  double getMetaCodelength(bool unweighted = false) const noexcept
  {
    return unweighted ? metaCodelength : metaDataRate * metaCodelength;
  };

  // ===================================================
  // IO
  // ===================================================

  std::ostream& print(std::ostream& out) const noexcept final;

  friend std::ostream& operator<<(std::ostream&, const MetaMapEquation&);

  // ===================================================
  // Init
  // ===================================================

  void init(const Config& config) noexcept final;

  void initNetwork(InfoNode& root) noexcept final;

  void initPartition(std::vector<InfoNode*>& nodes) noexcept final;

  // ===================================================
  // Codelength
  // ===================================================

  double calcCodelength(const InfoNode& parent) const final;

  double getDeltaCodelengthOnMovingNode(InfoNode& current,
                                        DeltaFlowDataType& oldModuleDelta,
                                        DeltaFlowDataType& newModuleDelta,
                                        std::vector<FlowDataType>& moduleFlowData,
                                        std::vector<unsigned int>& moduleMembers) const final;

  // ===================================================
  // Consolidation
  // ===================================================

  void updateCodelengthOnMovingNode(InfoNode& current,
                                    DeltaFlowDataType& oldModuleDelta,
                                    DeltaFlowDataType& newModuleDelta,
                                    std::vector<FlowDataType>& moduleFlowData,
                                    std::vector<unsigned int>& moduleMembers) final;

  void consolidateModules(std::vector<InfoNode*>& modules) final;

  // ===================================================
  // Debug
  // ===================================================

  void printDebug() const final;

protected:
  // ===================================================
  // Protected member functions
  // ===================================================
  double calcCodelengthOnModuleOfLeafNodes(const InfoNode& parent) const final;

  // ===================================================
  // Init
  // ===================================================

  void initMetaNodes(InfoNode& root);

  void initPartitionOfMetaNodes(std::vector<InfoNode*>& nodes);

  // ===================================================
  // Codelength
  // ===================================================

  void calculateCodelength(std::vector<InfoNode*>& nodes) final;

  // ===================================================
  // Consolidation
  // ===================================================

  void updateMetaData(InfoNode& current, unsigned int oldModuleIndex, unsigned int bestModuleIndex);

  /**
   *  Get meta codelength of module of current node
   * @param addRemoveOrNothing +1, -1 or 0 to calculate codelength
   * as if current node was added, removed or untouched in current module
   */
  double getCurrentModuleMetaCodelength(unsigned int module, InfoNode& current, int addRemoveOrNothing) const;

  // ===================================================
  // Protected member variables
  // ===================================================

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
