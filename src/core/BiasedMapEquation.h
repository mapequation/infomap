/*
 * BiasedMapEquation.h
 */

#ifndef _BIASEDMAPEQUATION_H_
#define _BIASEDMAPEQUATION_H_

#include "MapEquation.h"
#include <vector>
#include <map>

namespace infomap {

class InfoNode;

class BiasedMapEquation final : public MapEquation {
  using Base = MapEquation;

public:
  // ===================================================
  // Getters
  // ===================================================

  static bool haveMemory() noexcept { return true; }

  double getModuleCodelength() const noexcept final;

  double getCodelength() const noexcept final;

  // ===================================================
  // IO
  // ===================================================

  std::ostream& print(std::ostream& out) const noexcept final;

  friend std::ostream& operator<<(std::ostream&, const BiasedMapEquation&);

  // ===================================================
  // Init
  // ===================================================

  void init(const Config& config) noexcept final;

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

  static int getDeltaNumModulesIfMoving(unsigned int oldModule, unsigned int newModule, std::vector<unsigned int>& moduleMembers);

  // ===================================================
  // Codelength
  // ===================================================

  void calculateCodelength(std::vector<InfoNode*>& nodes) final;

  double calcNumModuleCost(unsigned int numModules) const;

  // ===================================================
  // Protected member variables
  // ===================================================

  // For biased
  unsigned int preferredNumModules = 0;
  unsigned int currentNumModules = 0;
  double biasedCost = 0.0;
};


} // namespace infomap

#endif /* _BIASEDMAPEQUATION_H_ */
