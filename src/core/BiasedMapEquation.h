/*
 * BiasedMapEquation.h
 */

#ifndef _BIASEDMAPEQUATION_H_
#define _BIASEDMAPEQUATION_H_

#include "MapEquation.h"
#include <vector>
#include <map>

namespace infomap {

struct FlowData;
struct DeltaFlow;

class BiasedMapEquation : public MapEquation<FlowData, DeltaFlow> {
  using Base = MapEquation<FlowData, DeltaFlow>;

public:
  // ===================================================
  // Getters
  // ===================================================

  static bool haveMemory() noexcept { return true; }

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
