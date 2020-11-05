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

class BiasedMapEquation : protected MapEquation {
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

  // ===================================================
  // IO
  // ===================================================

  std::ostream& print(std::ostream& out) const noexcept override;

  friend std::ostream& operator<<(std::ostream&, const BiasedMapEquation&);

  // ===================================================
  // Init
  // ===================================================

  void init(const Config& config) noexcept override;

  using Base::initNetwork;

  using Base::initSuperNetwork;

  using Base::initSubNetwork;

  using Base::initPartition;

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

  static int getDeltaNumModulesIfMoving(unsigned int oldModule, unsigned int newModule, std::vector<unsigned int>& moduleMembers);

  // ===================================================
  // Codelength
  // ===================================================

  void calculateCodelength(std::vector<InfoNode*>& nodes) override;

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
