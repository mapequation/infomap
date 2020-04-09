/*
 * MemMapEquation.h
 *
 *  Created on: 4 mar 2015
 *      Author: Daniel
 */

#include "MemMapEquation.h"
#include "FlowData.h"
#include "InfoNode.h"
#include "../utils/Log.h"
#include <vector>
#include <set>
#include <map>
#include <utility>
#include <algorithm>

namespace infomap {

// ===================================================
// IO
// ===================================================

std::ostream& MemMapEquation::print(std::ostream& out) const
{
  return out << indexCodelength << " + " << moduleCodelength << " = " << io::toPrecision(codelength);
}

std::ostream& operator<<(std::ostream& out, const MemMapEquation& mapEq)
{
  return mapEq.print(out);
}


// ===================================================
// Init
// ===================================================

void MemMapEquation::init(const Config& config)
{
  Log(3) << "MemMapEquation::init()...\n";
}


void MemMapEquation::initNetwork(InfoNode& root)
{
  initPhysicalNodes(root);
}

void MemMapEquation::initSuperNetwork(InfoNode& root)
{
  //TODO: How use enterFlow instead of flow
}

void MemMapEquation::initSubNetwork(InfoNode& root)
{
  //	Base::initSubNetwork(root);
}

void MemMapEquation::initPartition(std::vector<InfoNode*>& nodes)
{
  initPartitionOfPhysicalNodes(nodes);

  calculateCodelength(nodes);
}

void MemMapEquation::initPhysicalNodes(InfoNode& root)
{
  // bool notInitiated = root.firstChild->physicalNodes.empty();
  auto firstLeafIt = root.begin_leaf_nodes();
  auto depth = firstLeafIt.depth();
  bool notInitiatedOnLeafNodes = firstLeafIt->physicalNodes.empty();
  if (notInitiatedOnLeafNodes) {
    Log(3) << "MemMapEquation::initPhysicalNodesOnOriginalNetwork()...\n";
    std::set<unsigned int> setOfPhysicalNodes;
    unsigned int maxPhysicalId = 0;
    unsigned int minPhysicalId = std::numeric_limits<unsigned int>::max();
    for (auto it(root.begin_leaf_nodes()); !it.isEnd(); ++it) {
      InfoNode& node = *it;
      setOfPhysicalNodes.insert(node.physicalId);
      maxPhysicalId = std::max(maxPhysicalId, node.physicalId);
      minPhysicalId = std::min(minPhysicalId, node.physicalId);
    }

    m_numPhysicalNodes = setOfPhysicalNodes.size();

    // Re-index physical nodes if necessary
    std::map<unsigned int, unsigned int> toZeroBasedIndex;
    if (maxPhysicalId - minPhysicalId + 1 > m_numPhysicalNodes) {
      unsigned int zeroBasedPhysicalId = 0;
      for (unsigned int physIndex : setOfPhysicalNodes) {
        toZeroBasedIndex.insert(std::make_pair(physIndex, zeroBasedPhysicalId++));
      }
    }

    for (auto it(root.begin_leaf_nodes()); !it.isEnd(); ++it) {
      InfoNode& node = *it;
      // Log() << "Leaf node " << node.stateId << " (phys " << node.physicalId << ") physicalNodes: ";
      // unsigned int zeroBasedIndex = toZeroBasedIndex[node.physicalId];
      // unsigned int zeroBasedIndex = getPhysIndex[node.physicalId];
      unsigned int zeroBasedIndex = !toZeroBasedIndex.empty() ? toZeroBasedIndex[node.physicalId] : (node.physicalId - minPhysicalId);
      node.physicalNodes.push_back(PhysData(zeroBasedIndex, node.data.flow));
      // Log() << "(" << zeroBasedIndex << ", " << node.data.flow << "), length: " << node.physicalNodes.size() << "\n";
    }

    // If leaf nodes was not directly under root, make sure leaf modules have
    // physical nodes defined also
    if (depth > 1) {
      for (auto it(root.begin_leaf_modules()); !it.isEnd(); ++it) {
        InfoNode& module = *it;
        std::map<unsigned int, double> physToFlow;
        for (auto& node : module) {
          for (PhysData& physData : node.physicalNodes) {
            physToFlow[physData.physNodeIndex] += physData.sumFlowFromM2Node;
          }
        }
        for (auto& physFlow : physToFlow) {
          module.physicalNodes.push_back(PhysData(physFlow.first, physFlow.second));
        }
      }
    }
  } else {
    // Either a sub-network (without modules) or the whole network with reconstructed tree
    if (depth == 1) {
      // new sub-network
      Log(3) << "MemMapEquation::initPhysicalNodesOnSubNetwork()...\n";
      std::set<unsigned int> setOfPhysicalNodes;
      // std::cout << "_*!|!*_";
      unsigned int maxPhysNodeIndex = 0;
      unsigned int minPhysNodeIndex = std::numeric_limits<unsigned int>::max();

      // Collect all physical nodes in this sub network
      for (InfoNode& node : root) {
        for (PhysData& physData : node.physicalNodes) {
          setOfPhysicalNodes.insert(physData.physNodeIndex);
          maxPhysNodeIndex = std::max(maxPhysNodeIndex, physData.physNodeIndex);
          minPhysNodeIndex = std::min(minPhysNodeIndex, physData.physNodeIndex);
        }
      }

      m_numPhysicalNodes = setOfPhysicalNodes.size();

      // Re-index physical nodes if needed (not required when reconstructing tree)
      if (maxPhysNodeIndex >= m_numPhysicalNodes) {
        std::map<unsigned int, unsigned int> toZeroBasedIndex;
        if (maxPhysNodeIndex - minPhysNodeIndex + 1 > m_numPhysicalNodes) {
          unsigned int zeroBasedPhysicalId = 0;
          for (unsigned int physIndex : setOfPhysicalNodes) {
            toZeroBasedIndex.insert(std::make_pair(physIndex, zeroBasedPhysicalId++));
          }
        }

        for (InfoNode& node : root) {
          for (PhysData& physData : node.physicalNodes) {
            unsigned int zeroBasedIndex = !toZeroBasedIndex.empty() ? toZeroBasedIndex[physData.physNodeIndex] : (physData.physNodeIndex - minPhysNodeIndex);
            physData.physNodeIndex = zeroBasedIndex;
          }
        }
      }
    } else {
      // whole network with reconstructed tree
      for (auto it(root.begin_leaf_modules()); !it.isEnd(); ++it) {
        InfoNode& module = *it;
        std::map<unsigned int, double> physToFlow;
        for (auto& node : module) {
          for (PhysData& physData : node.physicalNodes) {
            physToFlow[physData.physNodeIndex] += physData.sumFlowFromM2Node;
          }
        }
        for (auto& physFlow : physToFlow) {
          module.physicalNodes.push_back(PhysData(physFlow.first, physFlow.second));
        }
      }
    }
  }
}

void MemMapEquation::initPartitionOfPhysicalNodes(std::vector<InfoNode*>& nodes)
{
  Log(4) << "MemMapEquation::initPartitionOfPhysicalNodes()...\n";
  m_physToModuleToMemNodes.clear();
  m_physToModuleToMemNodes.resize(m_numPhysicalNodes);

  for (auto& n : nodes) {
    InfoNode& node = *n;
    unsigned int moduleIndex = node.index; // Assume unique module index for all nodes in this initiation phase

    for (PhysData& physData : node.physicalNodes) {
      m_physToModuleToMemNodes[physData.physNodeIndex].insert(m_physToModuleToMemNodes[physData.physNodeIndex].end(),
                                                              std::make_pair(moduleIndex, MemNodeSet(1, physData.sumFlowFromM2Node)));
    }
  }

  m_memoryContributionsAdded = false;
}


// ===================================================
// Codelength
// ===================================================

void MemMapEquation::calculateCodelength(std::vector<InfoNode*>& nodes)
{
  calculateCodelengthTerms(nodes);

  calculateNodeFlow_log_nodeFlow();

  calculateCodelengthFromCodelengthTerms();
}

void MemMapEquation::calculateNodeFlow_log_nodeFlow()
{
  nodeFlow_log_nodeFlow = 0.0;
  for (unsigned int i = 0; i < m_numPhysicalNodes; ++i) {
    const ModuleToMemNodes& moduleToMemNodes = m_physToModuleToMemNodes[i];
    for (ModuleToMemNodes::const_iterator modToMemIt(moduleToMemNodes.begin()); modToMemIt != moduleToMemNodes.end(); ++modToMemIt)
      nodeFlow_log_nodeFlow += infomath::plogp(modToMemIt->second.sumFlow);
  }
}

double MemMapEquation::calcCodelength(const InfoNode& parent) const
{
  return parent.isLeafModule() ? (parent.isRoot() ?
                                                  // Use first-order model for one-level codebook
                                      MapEquation::calcCodelengthOnModuleOfLeafNodes(parent)
                                                  : calcCodelengthOnModuleOfLeafNodes(parent))
                               :
                               // Use first-order model on index codebook
      MapEquation::calcCodelengthOnModuleOfModules(parent);
}

double MemMapEquation::calcCodelengthOnModuleOfLeafNodes(const InfoNode& parent) const
{
  if (parent.numPhysicalNodes() == 0) {
    return MapEquation::calcCodelength(parent); // Infomap root node
  }

  //TODO: For ordinary networks, flow should be used instead of enter flow
  // for leaf nodes, what about memory networks? sumFlowFromM2Node vs sumEnterFlowFromM2Node?
  double parentFlow = parent.data.flow;
  double parentExit = parent.data.exitFlow;
  double totalParentFlow = parentFlow + parentExit;
  if (totalParentFlow < 1e-16)
    return 0.0;

  double indexLength = 0.0;

  for (const PhysData& physData : parent.physicalNodes) {
    indexLength -= infomath::plogp(physData.sumFlowFromM2Node / totalParentFlow);
  }
  indexLength -= infomath::plogp(parentExit / totalParentFlow);

  indexLength *= totalParentFlow;

  return indexLength;
}

void MemMapEquation::addMemoryContributions(InfoNode& current,
                                            DeltaFlowDataType& oldModuleDelta,
                                            VectorMap<DeltaFlowDataType>& moduleDeltaFlow)
{
  // Overlapping modules
  /*
   * delta = old.first + new.first + old.second - new.second.
   * Two cases: (p(x) = plogp(x))
   * Moving to a module that already have that physical node: (old: p1, p2, new p3, moving p2 -> old:p1, new p2,p3)
   * Then old.second = new.second = plogp(physicalNodeSize) -> cancelation -> delta = p(p1) - p(p1+p2) + p(p2+p3) - p(p3)
   * Moving to a module that not have that physical node: (old: p1, p2, new -, moving p2 -> old: p1, new: p2)
   * Then new.first = new.second = 0 -> delta = p(p1) - p(p1+p2) + p(p2).
   */
  auto& physicalNodes = current.physicalNodes;
  unsigned int numPhysicalNodes = physicalNodes.size();
  for (unsigned int i = 0; i < numPhysicalNodes; ++i) {
    PhysData& physData = physicalNodes[i];
    ModuleToMemNodes& moduleToMemNodes = m_physToModuleToMemNodes[physData.physNodeIndex];
    for (ModuleToMemNodes::iterator overlapIt(moduleToMemNodes.begin()); overlapIt != moduleToMemNodes.end(); ++overlapIt) {
      unsigned int moduleIndex = overlapIt->first;
      MemNodeSet& memNodeSet = overlapIt->second;
      if (moduleIndex == current.index) // From where the multiple assigned node is moved
      {
        double oldPhysFlow = memNodeSet.sumFlow;
        double newPhysFlow = memNodeSet.sumFlow - physData.sumFlowFromM2Node;
        oldModuleDelta.sumDeltaPlogpPhysFlow += infomath::plogp(newPhysFlow) - infomath::plogp(oldPhysFlow);
        oldModuleDelta.sumPlogpPhysFlow += infomath::plogp(physData.sumFlowFromM2Node);
      } else // To where the multiple assigned node is moved
      {
        double oldPhysFlow = memNodeSet.sumFlow;
        double newPhysFlow = memNodeSet.sumFlow + physData.sumFlowFromM2Node;

        // DeltaFlowDataType& otherDeltaFlow = moduleDeltaFlow[moduleIndex];
        // otherDeltaFlow.module = moduleIndex; // Make sure module index is correct if created new module link
        // otherDeltaFlow.sumDeltaPlogpPhysFlow = infomath::plogp(newPhysFlow) - infomath::plogp(oldPhysFlow);
        // otherDeltaFlow.sumPlogpPhysFlow = infomath::plogp(physData.sumFlowFromM2Node);
        // ++otherDeltaFlow.count;
        double sumDeltaPlogpPhysFlow = infomath::plogp(newPhysFlow) - infomath::plogp(oldPhysFlow);
        double sumPlogpPhysFlow = infomath::plogp(physData.sumFlowFromM2Node);
        moduleDeltaFlow.add(moduleIndex, DeltaFlowDataType(moduleIndex, 0.0, 0.0, sumDeltaPlogpPhysFlow, sumPlogpPhysFlow));
      }
    }
  }
  m_memoryContributionsAdded = true;
}


double MemMapEquation::getDeltaCodelengthOnMovingNode(InfoNode& current,
                                                      DeltaFlowDataType& oldModuleDelta,
                                                      DeltaFlowDataType& newModuleDelta,
                                                      std::vector<FlowDataType>& moduleFlowData,
                                                      std::vector<unsigned int>& moduleMembers)
{
  double deltaL = Base::getDeltaCodelengthOnMovingNode(current, oldModuleDelta, newModuleDelta, moduleFlowData, moduleMembers);

  double delta_nodeFlow_log_nodeFlow = oldModuleDelta.sumDeltaPlogpPhysFlow + newModuleDelta.sumDeltaPlogpPhysFlow + oldModuleDelta.sumPlogpPhysFlow - newModuleDelta.sumPlogpPhysFlow;

  return deltaL - delta_nodeFlow_log_nodeFlow;
}


// ===================================================
// Consolidation
// ===================================================

void MemMapEquation::updateCodelengthOnMovingNode(InfoNode& current,
                                                  DeltaFlowDataType& oldModuleDelta,
                                                  DeltaFlowDataType& newModuleDelta,
                                                  std::vector<FlowDataType>& moduleFlowData,
                                                  std::vector<unsigned int>& moduleMembers)
{
  Base::updateCodelengthOnMovingNode(current, oldModuleDelta, newModuleDelta, moduleFlowData, moduleMembers);
  if (m_memoryContributionsAdded)
    updatePhysicalNodes(current, oldModuleDelta.module, newModuleDelta.module);
  else
    addMemoryContributionsAndUpdatePhysicalNodes(current, oldModuleDelta, newModuleDelta);

  double delta_nodeFlow_log_nodeFlow = oldModuleDelta.sumDeltaPlogpPhysFlow + newModuleDelta.sumDeltaPlogpPhysFlow + oldModuleDelta.sumPlogpPhysFlow - newModuleDelta.sumPlogpPhysFlow;

  nodeFlow_log_nodeFlow += delta_nodeFlow_log_nodeFlow;
  moduleCodelength -= delta_nodeFlow_log_nodeFlow;
  codelength -= delta_nodeFlow_log_nodeFlow;
}


void MemMapEquation::updatePhysicalNodes(InfoNode& current, unsigned int oldModuleIndex, unsigned int bestModuleIndex)
{
  // For all multiple assigned nodes
  for (unsigned int i = 0; i < current.physicalNodes.size(); ++i) {
    PhysData& physData = current.physicalNodes[i];
    ModuleToMemNodes& moduleToMemNodes = m_physToModuleToMemNodes[physData.physNodeIndex];

    // Remove contribution to old module
    ModuleToMemNodes::iterator overlapIt = moduleToMemNodes.find(oldModuleIndex);
    if (overlapIt == moduleToMemNodes.end())
      throw std::length_error(io::Str() << "Couldn't find old module " << oldModuleIndex << " in physical node " << physData.physNodeIndex);
    MemNodeSet& memNodeSet = overlapIt->second;
    memNodeSet.sumFlow -= physData.sumFlowFromM2Node;
    if (--memNodeSet.numMemNodes == 0)
      moduleToMemNodes.erase(overlapIt);

    // Add contribution to new module
    overlapIt = moduleToMemNodes.find(bestModuleIndex);
    if (overlapIt == moduleToMemNodes.end())
      moduleToMemNodes.insert(std::make_pair(bestModuleIndex, MemNodeSet(1, physData.sumFlowFromM2Node)));
    else {
      MemNodeSet& memNodeSet = overlapIt->second;
      ++memNodeSet.numMemNodes;
      memNodeSet.sumFlow += physData.sumFlowFromM2Node;
    }
  }
}

void MemMapEquation::addMemoryContributionsAndUpdatePhysicalNodes(InfoNode& current, DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta)
{
  unsigned int oldModuleIndex = oldModuleDelta.module;
  unsigned int bestModuleIndex = newModuleDelta.module;

  // For all multiple assigned nodes
  for (unsigned int i = 0; i < current.physicalNodes.size(); ++i) {
    PhysData& physData = current.physicalNodes[i];
    ModuleToMemNodes& moduleToMemNodes = m_physToModuleToMemNodes[physData.physNodeIndex];

    // Remove contribution to old module
    ModuleToMemNodes::iterator overlapIt = moduleToMemNodes.find(oldModuleIndex);
    if (overlapIt == moduleToMemNodes.end())
      throw std::length_error("Couldn't find old module among physical node assignments.");
    MemNodeSet& memNodeSet = overlapIt->second;
    double oldPhysFlow = memNodeSet.sumFlow;
    double newPhysFlow = memNodeSet.sumFlow - physData.sumFlowFromM2Node;
    oldModuleDelta.sumDeltaPlogpPhysFlow += infomath::plogp(newPhysFlow) - infomath::plogp(oldPhysFlow);
    oldModuleDelta.sumPlogpPhysFlow += infomath::plogp(physData.sumFlowFromM2Node);
    memNodeSet.sumFlow -= physData.sumFlowFromM2Node;
    if (--memNodeSet.numMemNodes == 0)
      moduleToMemNodes.erase(overlapIt);


    // Add contribution to new module
    overlapIt = moduleToMemNodes.find(bestModuleIndex);
    if (overlapIt == moduleToMemNodes.end()) {
      moduleToMemNodes.insert(std::make_pair(bestModuleIndex, MemNodeSet(1, physData.sumFlowFromM2Node)));
      oldPhysFlow = 0.0;
      newPhysFlow = physData.sumFlowFromM2Node;
      newModuleDelta.sumDeltaPlogpPhysFlow += infomath::plogp(newPhysFlow) - infomath::plogp(oldPhysFlow);
      newModuleDelta.sumPlogpPhysFlow += infomath::plogp(physData.sumFlowFromM2Node);
    } else {
      MemNodeSet& memNodeSet = overlapIt->second;
      oldPhysFlow = memNodeSet.sumFlow;
      newPhysFlow = memNodeSet.sumFlow + physData.sumFlowFromM2Node;
      newModuleDelta.sumDeltaPlogpPhysFlow += infomath::plogp(newPhysFlow) - infomath::plogp(oldPhysFlow);
      newModuleDelta.sumPlogpPhysFlow += infomath::plogp(physData.sumFlowFromM2Node);
      ++memNodeSet.numMemNodes;
      memNodeSet.sumFlow += physData.sumFlowFromM2Node;
    }
  }
}


void MemMapEquation::consolidateModules(std::vector<InfoNode*>& modules)
{
  std::map<unsigned int, std::map<unsigned int, unsigned int>> validate;

  for (unsigned int i = 0; i < m_numPhysicalNodes; ++i) {
    ModuleToMemNodes& modToMemNodes = m_physToModuleToMemNodes[i];
    for (ModuleToMemNodes::iterator overlapIt = modToMemNodes.begin(); overlapIt != modToMemNodes.end(); ++overlapIt) {
      if (++validate[overlapIt->first][i] > 1)
        throw std::domain_error("[InfomapGreedy::consolidateModules] Error updating physical nodes: duplication error");

      modules[overlapIt->first]->physicalNodes.push_back(PhysData(i, overlapIt->second.sumFlow));
    }
  }
}


// ===================================================
// Debug
// ===================================================

void MemMapEquation::printDebug()
{
  std::cout << "MemMapEquation::m_numPhysicalNodes: " << m_numPhysicalNodes << "\n";
  Base::printDebug();
}


} // namespace infomap
