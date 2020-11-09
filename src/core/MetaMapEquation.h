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

struct FlowData;
struct DeltaFlow;

class MetaMapEquation : public MapEquation<FlowData, DeltaFlow> {
  using Base = MapEquation<FlowData, DeltaFlow>;

public:

  // ===================================================
  // Getters
  // ===================================================

  static bool haveMemory() noexcept { return true; }

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

  void initPartition(std::vector<InfoNode*>& nodes) noexcept override;

  // ===================================================
  // Codelength
  // ===================================================

  double calcCodelength(const InfoNode& parent) const override;

  double getDeltaCodelengthOnMovingNode(InfoNode& current,
                                        DeltaFlow& oldModuleDelta,
                                        DeltaFlow& newModuleDelta,
                                        std::vector<FlowData>& moduleFlowData,
                                        std::vector<unsigned int>& moduleMembers) const override;

  // ===================================================
  // Consolidation
  // ===================================================

  void updateCodelengthOnMovingNode(InfoNode& current,
                                    DeltaFlow& oldModuleDelta,
                                    DeltaFlow& newModuleDelta,
                                    std::vector<FlowData>& moduleFlowData,
                                    std::vector<unsigned int>& moduleMembers) override;

  void consolidateModules(std::vector<InfoNode*>& modules) override;

  // ===================================================
  // Debug
  // ===================================================

  void printDebug() const override;

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
