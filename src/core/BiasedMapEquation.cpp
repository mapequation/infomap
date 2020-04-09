/*
 * BiasedMapEquation.h
 */

#include "BiasedMapEquation.h"
#include "FlowData.h"
#include "InfoNode.h"
#include "../utils/Log.h"
#include "../io/Config.h"
#include <vector>
#include <set>
#include <map>
#include <utility>
#include <cstdlib>

namespace infomap {

double BiasedMapEquation::getModuleCodelength() const
{
  // std::cout << "\n$$$$$ getModuleCodelength: " << moduleCodelength << " + " << biasedCost << " = " << moduleCodelength + biasedCost << "\n";
  return moduleCodelength + biasedCost;
}

double BiasedMapEquation::getCodelength() const
{
  // std::cout << "\n$$$$$ getCodelength: " << codelength << " + " << biasedCost << " = " << codelength + biasedCost << "\n";
  return codelength + biasedCost;
}

// ===================================================
// IO
// ===================================================

std::ostream& BiasedMapEquation::print(std::ostream& out) const
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

void BiasedMapEquation::init(const Config& config)
{
  Log(3) << "BiasedMapEquation::init()...\n";
  preferredNumModules = config.preferredNumberOfModules;
}


void BiasedMapEquation::initNetwork(InfoNode& root)
{
  Log(3) << "BiasedMapEquation::initNetwork()...\n";
  Base::initNetwork(root);
}

void BiasedMapEquation::initSuperNetwork(InfoNode& root)
{
  Base::initSuperNetwork(root);
}

void BiasedMapEquation::initSubNetwork(InfoNode& root)
{
  Base::initSubNetwork(root);
}

void BiasedMapEquation::initPartition(std::vector<InfoNode*>& nodes)
{
  calculateCodelength(nodes);
}


// ===================================================
// Codelength
// ===================================================

double BiasedMapEquation::calcNumModuleCost(unsigned int numModules) const
{
  if (preferredNumModules == 0) return 0;
  int deltaNumModules = numModules - preferredNumModules;
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

double BiasedMapEquation::calcCodelengthOnModuleOfLeafNodes(const InfoNode& parent) const
{
  double indexLength = MapEquation::calcCodelength(parent);

  // double biasedCost	= calcNumModuleCost(parent.childDegree())
  // std::cout << "\n!!!!! calcCodelengthOnModuleOfLeafNodes(parent) -> biasedCost: " << biasedCost << "\n";

  // return indexLength + biasedCost;
  return indexLength;
}

int BiasedMapEquation::getDeltaNumModulesIfMoving(InfoNode& current,
                                                  unsigned int oldModule,
                                                  unsigned int newModule,
                                                  std::vector<unsigned int>& moduleMembers) const
{
  bool removeOld = moduleMembers[oldModule] == 1;
  bool createNew = moduleMembers[newModule] == 0;
  int deltaNumModules = removeOld && !createNew ? -1 : (!removeOld && createNew ? 1 : 0);
  return deltaNumModules;
}

double BiasedMapEquation::getDeltaCodelengthOnMovingNode(InfoNode& current,
                                                         DeltaFlowDataType& oldModuleDelta,
                                                         DeltaFlowDataType& newModuleDelta,
                                                         std::vector<FlowDataType>& moduleFlowData,
                                                         std::vector<unsigned int>& moduleMembers)
{
  double deltaL = Base::getDeltaCodelengthOnMovingNode(current, oldModuleDelta, newModuleDelta, moduleFlowData, moduleMembers);

  if (preferredNumModules == 0)
    return deltaL;

  int deltaNumModules = getDeltaNumModulesIfMoving(current, oldModuleDelta.module, newModuleDelta.module, moduleMembers);

  double deltaBiasedCost = calcNumModuleCost(currentNumModules + deltaNumModules) - biasedCost;

  // std::cout << "\n!!!!! getDeltaCodelengthOnMovingNode(" << current.stateId << ") from " <<
  // 	oldModule << " (" << moduleMembers[oldModule] << ") to " <<
  // 	newModule << " (" << moduleMembers[newModule] << ") -> currentNumModules = " <<
  // 	currentNumModules << " + " << deltaNumModules << " => cost: " <<
  // 	biasedCost << " + " << deltaBiasedCost << " = " << (biasedCost + deltaBiasedCost) << "\n";

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

  int deltaNumModules = getDeltaNumModulesIfMoving(current, oldModuleDelta.module, newModuleDelta.module, moduleMembers);

  // double deltaBiasedCost = calcNumModuleCost(currentNumModules + deltaNumModules) - biasedCost;

  // std::cout << "\n!!!!! updateCodelengthOnMovingNode(" << current.stateId << ") from " <<
  // 	oldModule << " (" << moduleMembers[oldModule] << ") to " <<
  // 	newModule << " (" << moduleMembers[newModule] << ") -> currentNumModules = " <<
  // 	currentNumModules << " + " << deltaNumModules << " => cost: " <<
  // 	biasedCost << " + " << deltaBiasedCost << " = " << (biasedCost + deltaBiasedCost) << "\n";

  // biasedCost += deltaBiasedCost;

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

void BiasedMapEquation::printDebug()
{
  std::cout << "BiasedMapEquation\n";
  Base::printDebug();
}


} // namespace infomap
