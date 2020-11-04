/*
 * BiasedMapEquation.h
 */

#include "BiasedMapEquation.h"
#include "FlowData.h"
#include "InfoNode.h"
#include <vector>
#include <utility>
#include <cstdlib>

namespace infomap {

double BiasedMapEquation::getModuleCodelength() const noexcept
{
  return moduleCodelength + biasedCost;
}

double BiasedMapEquation::getCodelength() const noexcept
{
  return codelength + biasedCost;
}

// ===================================================
// IO
// ===================================================

std::ostream& BiasedMapEquation::print(std::ostream& out) const noexcept
{
  return out << indexCodelength << " + " << moduleCodelength << " + " << biasedCost << " = " << io::toPrecision(getCodelength());
}

std::ostream& operator<<(std::ostream& out, const BiasedMapEquation& mapEq)
{
  return mapEq.print(out);
}


// ===================================================
// Init
// ===================================================

void BiasedMapEquation::init(const Config& config) noexcept
{
  Log(3) << "BiasedMapEquation::init()...\n";
  preferredNumModules = config.preferredNumberOfModules;
}


// ===================================================
// Codelength
// ===================================================

double BiasedMapEquation::calcNumModuleCost(unsigned int numModules) const
{
  if (preferredNumModules == 0) return 0;

  const int deltaNumModules = numModules - preferredNumModules;

  return 1 * std::abs(deltaNumModules);
}

void BiasedMapEquation::calculateCodelength(std::vector<InfoNode*>& nodes)
{
  calculateCodelengthTerms(nodes);

  calculateCodelengthFromCodelengthTerms();

  currentNumModules = nodes.size();

  biasedCost = calcNumModuleCost(currentNumModules);
}

double BiasedMapEquation::calcCodelength(const InfoNode& parent) const
{
  return parent.isLeafModule()
      ? calcCodelengthOnModuleOfLeafNodes(parent)
      : MapEquation::calcCodelengthOnModuleOfModules(parent); // Use first-order model on index codebook
}

int BiasedMapEquation::getDeltaNumModulesIfMoving(InfoNode& current,
                                                  unsigned int oldModule,
                                                  unsigned int newModule,
                                                  std::vector<unsigned int>& moduleMembers)
{
  const bool removeOld = moduleMembers[oldModule] == 1;
  const bool createNew = moduleMembers[newModule] == 0;
  return removeOld && !createNew ? -1 : (!removeOld && createNew ? 1 : 0);
}

double BiasedMapEquation::getDeltaCodelengthOnMovingNode(InfoNode& current,
                                                         DeltaFlowDataType& oldModuleDelta,
                                                         DeltaFlowDataType& newModuleDelta,
                                                         std::vector<FlowDataType>& moduleFlowData,
                                                         std::vector<unsigned int>& moduleMembers) const
{
  const double deltaL = Base::getDeltaCodelengthOnMovingNode(current, oldModuleDelta, newModuleDelta, moduleFlowData, moduleMembers);

  if (preferredNumModules == 0)
    return deltaL;

  const int deltaNumModules = getDeltaNumModulesIfMoving(current, oldModuleDelta.module, newModuleDelta.module, moduleMembers);

  const double deltaBiasedCost = calcNumModuleCost(currentNumModules + deltaNumModules) - biasedCost;

  return deltaL + deltaBiasedCost;
}


// ===================================================
// Consolidation
// ===================================================

void BiasedMapEquation::updateCodelengthOnMovingNode(InfoNode& current,
                                                     DeltaFlowDataType& oldModuleDelta,
                                                     DeltaFlowDataType& newModuleDelta,
                                                     std::vector<FlowDataType>& moduleFlowData,
                                                     std::vector<unsigned int>& moduleMembers)
{
  Base::updateCodelengthOnMovingNode(current, oldModuleDelta, newModuleDelta, moduleFlowData, moduleMembers);

  if (preferredNumModules == 0)
    return;

  const int deltaNumModules = getDeltaNumModulesIfMoving(current, oldModuleDelta.module, newModuleDelta.module, moduleMembers);

  currentNumModules += deltaNumModules;
  biasedCost = calcNumModuleCost(currentNumModules);
}

void BiasedMapEquation::consolidateModules(std::vector<InfoNode*>& modules)
{
  unsigned int numModules = 0;

  for (auto& module : modules) {
    if (module == nullptr)
      continue;
    ++numModules;
  }
  currentNumModules = numModules;
}


// ===================================================
// Debug
// ===================================================

void BiasedMapEquation::printDebug() const
{
  std::cout << "BiasedMapEquation\n";
  Base::printDebug();
}


} // namespace infomap
