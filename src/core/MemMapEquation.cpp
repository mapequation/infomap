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
#include "../io/convert.h"
#include "../io/Config.h"
#include <vector>
#include <set>
#include <map>
#include <utility>
#include <algorithm>

namespace infomap {

using detail::MemNodeSet;

// ===================================================
// IO
// ===================================================

std::ostream& MemMapEquation::print(std::ostream& out) const noexcept
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

void MemMapEquation::init(const Config&) noexcept
{
  Log(3) << "MemMapEquation::init()...\n";
}


void MemMapEquation::initNetwork(InfoNode& root) noexcept
{
  initPhysicalNodes(root);
}

void MemMapEquation::initPartition(std::vector<InfoNode*>& nodes) noexcept
{
  initPartitionOfPhysicalNodes(nodes);

  calculateCodelength(nodes);
}

void MemMapEquation::initPhysicalNodes(InfoNode& root)
{
  const auto firstLeafIt = root.begin_leaf_nodes();
  const auto depth = firstLeafIt.depth();
  const bool notInitiatedOnLeafNodes = firstLeafIt->physicalNodes.empty();

  if (notInitiatedOnLeafNodes) {
    Log(3) << "MemMapEquation::initPhysicalNodesOnOriginalNetwork()...\n";
    auto setOfPhysicalNodes = std::set<unsigned int>();

    unsigned int maxPhysicalId = 0;
    unsigned int minPhysicalId = std::numeric_limits<unsigned int>::max();

    for (auto it(root.begin_leaf_nodes()); !it.isEnd(); ++it) {
      auto& node = *it;
      setOfPhysicalNodes.insert(node.physicalId);
      maxPhysicalId = std::max(maxPhysicalId, node.physicalId);
      minPhysicalId = std::min(minPhysicalId, node.physicalId);
    }

    m_numPhysicalNodes = setOfPhysicalNodes.size();

    // Re-index physical nodes if necessary
    auto toZeroBasedIndex = std::map<unsigned int, unsigned int>();

    if (maxPhysicalId - minPhysicalId + 1 > m_numPhysicalNodes) {
      unsigned int zeroBasedPhysicalId = 0;
      for (unsigned int physIndex : setOfPhysicalNodes) {
        toZeroBasedIndex.insert(std::make_pair(physIndex, zeroBasedPhysicalId++));
      }
    }

    for (auto it(root.begin_leaf_nodes()); !it.isEnd(); ++it) {
      auto& node = *it;
      unsigned int zeroBasedIndex = !toZeroBasedIndex.empty() ? toZeroBasedIndex[node.physicalId] : (node.physicalId - minPhysicalId);
      node.physicalNodes.emplace_back(zeroBasedIndex, node.data.flow);
    }

    // If leaf nodes was not directly under root, make sure leaf modules have physical nodes defined also
    if (depth > 1) {
      for (auto it(root.begin_leaf_modules()); !it.isEnd(); ++it) {
        auto& module = *it;
        auto physToFlow = std::map<unsigned int, double>();

        for (auto& node : module) {
          for (const auto& physData : node.physicalNodes) {
            physToFlow[physData.physNodeIndex] += physData.sumFlowFromM2Node;
          }
        }

        for (const auto& physFlow : physToFlow) {
          module.physicalNodes.emplace_back(physFlow.first, physFlow.second);
        }
      }
    }
  } else {
    // Either a sub-network (without modules) or the whole network with reconstructed tree
    if (depth == 1) {
      // new sub-network
      Log(3) << "MemMapEquation::initPhysicalNodesOnSubNetwork()...\n";
      auto setOfPhysicalNodes = std::set<unsigned int>();
      unsigned int maxPhysNodeIndex = 0;
      unsigned int minPhysNodeIndex = std::numeric_limits<unsigned int>::max();

      // Collect all physical nodes in this sub network
      for (auto& node : root) {
        for (auto& physData : node.physicalNodes) {
          setOfPhysicalNodes.insert(physData.physNodeIndex);
          maxPhysNodeIndex = std::max(maxPhysNodeIndex, physData.physNodeIndex);
          minPhysNodeIndex = std::min(minPhysNodeIndex, physData.physNodeIndex);
        }
      }

      m_numPhysicalNodes = setOfPhysicalNodes.size();

      // Re-index physical nodes if needed (not required when reconstructing tree)
      if (maxPhysNodeIndex >= m_numPhysicalNodes) {
        auto toZeroBasedIndex = std::map<unsigned int, unsigned int>();

        if (maxPhysNodeIndex - minPhysNodeIndex + 1 > m_numPhysicalNodes) {
          unsigned int zeroBasedPhysicalId = 0;
          for (unsigned int physIndex : setOfPhysicalNodes) {
            toZeroBasedIndex.insert(std::make_pair(physIndex, zeroBasedPhysicalId++));
          }
        }

        for (auto& node : root) {
          for (auto& physData : node.physicalNodes) {
            const unsigned int zeroBasedIndex = !toZeroBasedIndex.empty() ? toZeroBasedIndex[physData.physNodeIndex] : (physData.physNodeIndex - minPhysNodeIndex);
            physData.physNodeIndex = zeroBasedIndex;
          }
        }
      }
    } else {
      // whole network with reconstructed tree
      for (auto it(root.begin_leaf_modules()); !it.isEnd(); ++it) {
        auto& module = *it;
        auto physToFlow = std::map<unsigned int, double>();

        for (auto& node : module) {
          for (auto& physData : node.physicalNodes) {
            physToFlow[physData.physNodeIndex] += physData.sumFlowFromM2Node;
          }
        }
        for (auto& physFlow : physToFlow) {
          module.physicalNodes.emplace_back(physFlow.first, physFlow.second);
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
    auto& node = *n;
    const auto moduleIndex = node.index; // Assume unique module index for all nodes in this initiation phase

    for (auto& physData : node.physicalNodes) {
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
    const auto& moduleToMemNodes = m_physToModuleToMemNodes[i];
    for (const auto & moduleToMemNode : moduleToMemNodes)
      nodeFlow_log_nodeFlow += infomath::plogp(moduleToMemNode.second.sumFlow);
  }
}

double MemMapEquation::calcCodelength(const InfoNode& parent) const
{
  return parent.isLeafModule() ? (parent.isRoot() ? MapEquation::calcCodelengthOnModuleOfLeafNodes(parent) : calcCodelengthOnModuleOfLeafNodes(parent))
                               : MapEquation::calcCodelengthOnModuleOfModules(parent);
}

double MemMapEquation::calcCodelengthOnModuleOfLeafNodes(const InfoNode& parent) const
{
  if (parent.numPhysicalNodes() == 0) {
    return Base::calcCodelength(parent); // Infomap root node
  }

  //TODO: For ordinary networks, flow should be used instead of enter flow
  // for leaf nodes, what about memory networks? sumFlowFromM2Node vs sumEnterFlowFromM2Node?
  const double parentFlow = parent.data.flow;
  const double parentExit = parent.data.exitFlow;
  const double totalParentFlow = parentFlow + parentExit;

  if (totalParentFlow < 1e-16)
    return 0.0;

  double indexLength = 0.0;

  for (const auto& physData : parent.physicalNodes) {
    indexLength -= infomath::plogp(physData.sumFlowFromM2Node / totalParentFlow);
  }
  indexLength -= infomath::plogp(parentExit / totalParentFlow);

  return indexLength * totalParentFlow;
}

void MemMapEquation::addMemoryContributions(InfoNode& current,
                                            MemDeltaFlow& oldModuleDelta,
                                            VectorMap<MemDeltaFlow>& moduleDeltaFlow)
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
  const unsigned int numPhysicalNodes = physicalNodes.size();
  for (unsigned int i = 0; i < numPhysicalNodes; ++i) {
    auto& physData = physicalNodes[i];
    auto& moduleToMemNodes = m_physToModuleToMemNodes[physData.physNodeIndex];

    for (auto & moduleToMemNode : moduleToMemNodes) {
      const unsigned int moduleIndex = moduleToMemNode.first;
      auto& memNodeSet = moduleToMemNode.second;

      if (moduleIndex == current.index) // From where the multiple assigned node is moved
      {
        const double oldPhysFlow = memNodeSet.sumFlow;
        const double newPhysFlow = memNodeSet.sumFlow - physData.sumFlowFromM2Node;
        oldModuleDelta.sumDeltaPlogpPhysFlow += infomath::plogp(newPhysFlow) - infomath::plogp(oldPhysFlow);
        oldModuleDelta.sumPlogpPhysFlow += infomath::plogp(physData.sumFlowFromM2Node);
      } else // To where the multiple assigned node is moved
      {
        const double oldPhysFlow = memNodeSet.sumFlow;
        const double newPhysFlow = memNodeSet.sumFlow + physData.sumFlowFromM2Node;

        const double sumDeltaPlogpPhysFlow = infomath::plogp(newPhysFlow) - infomath::plogp(oldPhysFlow);
        const double sumPlogpPhysFlow = infomath::plogp(physData.sumFlowFromM2Node);
        moduleDeltaFlow.add(moduleIndex, MemDeltaFlow(moduleIndex, 0.0, 0.0, sumDeltaPlogpPhysFlow, sumPlogpPhysFlow));
      }
    }
  }

  m_memoryContributionsAdded = true;
}


double MemMapEquation::getDeltaCodelengthOnMovingNode(InfoNode& current,
                                                      MemDeltaFlow& oldModuleDelta,
                                                      MemDeltaFlow& newModuleDelta,
                                                      std::vector<FlowData>& moduleFlowData,
                                                      std::vector<unsigned int>& moduleMembers) const
{
  const double deltaL = Base::getDeltaCodelengthOnMovingNode(current, oldModuleDelta, newModuleDelta, moduleFlowData, moduleMembers);

  const double delta_nodeFlow_log_nodeFlow = oldModuleDelta.sumDeltaPlogpPhysFlow + newModuleDelta.sumDeltaPlogpPhysFlow + oldModuleDelta.sumPlogpPhysFlow - newModuleDelta.sumPlogpPhysFlow;

  return deltaL - delta_nodeFlow_log_nodeFlow;
}


// ===================================================
// Consolidation
// ===================================================

void MemMapEquation::updateCodelengthOnMovingNode(InfoNode& current,
                                                  MemDeltaFlow& oldModuleDelta,
                                                  MemDeltaFlow& newModuleDelta,
                                                  std::vector<FlowData>& moduleFlowData,
                                                  std::vector<unsigned int>& moduleMembers)
{
  Base::updateCodelengthOnMovingNode(current, oldModuleDelta, newModuleDelta, moduleFlowData, moduleMembers);

  if (m_memoryContributionsAdded)
    updatePhysicalNodes(current, oldModuleDelta.module, newModuleDelta.module);
  else
    addMemoryContributionsAndUpdatePhysicalNodes(current, oldModuleDelta, newModuleDelta);

  const double delta_nodeFlow_log_nodeFlow = oldModuleDelta.sumDeltaPlogpPhysFlow + newModuleDelta.sumDeltaPlogpPhysFlow + oldModuleDelta.sumPlogpPhysFlow - newModuleDelta.sumPlogpPhysFlow;

  nodeFlow_log_nodeFlow += delta_nodeFlow_log_nodeFlow;
  moduleCodelength -= delta_nodeFlow_log_nodeFlow;
  codelength -= delta_nodeFlow_log_nodeFlow;
}


void MemMapEquation::updatePhysicalNodes(InfoNode& current, unsigned int oldModuleIndex, unsigned int bestModuleIndex)
{
  // For all multiple assigned nodes
  for (const auto & physData : current.physicalNodes) {
    auto& moduleToMemNodes = m_physToModuleToMemNodes[physData.physNodeIndex];

    // Remove contribution to old module
    auto overlapIt = moduleToMemNodes.find(oldModuleIndex);

    if (overlapIt == moduleToMemNodes.end())
      throw std::length_error(io::Str() << "Couldn't find old module " << oldModuleIndex << " in physical node " << physData.physNodeIndex);

    {
      auto& memNodeSet = overlapIt->second;
      memNodeSet.sumFlow -= physData.sumFlowFromM2Node;

      if (--memNodeSet.numMemNodes == 0)
        moduleToMemNodes.erase(overlapIt);
    }

    // Add contribution to new module
    overlapIt = moduleToMemNodes.find(bestModuleIndex);

    if (overlapIt == moduleToMemNodes.end())
      moduleToMemNodes.insert(std::make_pair(bestModuleIndex, MemNodeSet(1, physData.sumFlowFromM2Node)));
    else {
      auto& memNodeSet = overlapIt->second;
      ++memNodeSet.numMemNodes;
      memNodeSet.sumFlow += physData.sumFlowFromM2Node;
    }
  }
}

void MemMapEquation::addMemoryContributionsAndUpdatePhysicalNodes(InfoNode& current, MemDeltaFlow& oldModuleDelta, MemDeltaFlow& newModuleDelta)
{
  const unsigned int oldModuleIndex = oldModuleDelta.module;
  const unsigned int bestModuleIndex = newModuleDelta.module;

  // For all multiple assigned nodes
  for (auto & physData : current.physicalNodes) {
    auto& moduleToMemNodes = m_physToModuleToMemNodes[physData.physNodeIndex];

    // Remove contribution to old module
    auto overlapIt = moduleToMemNodes.find(oldModuleIndex);
    if (overlapIt == moduleToMemNodes.end())
      throw std::length_error("Couldn't find old module among physical node assignments.");

    auto& memNodeSet = overlapIt->second;

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
      memNodeSet = overlapIt->second;

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
  auto validate = std::map<unsigned int, std::map<unsigned int, unsigned int>>();

  for (unsigned int i = 0; i < m_numPhysicalNodes; ++i) {
    auto& modToMemNodes = m_physToModuleToMemNodes[i];
    for (auto & modToMemNode : modToMemNodes) {
      if (++validate[modToMemNode.first][i] > 1)
        throw std::domain_error("[InfomapGreedy::consolidateModules] Error updating physical nodes: duplication error");

      modules[modToMemNode.first]->physicalNodes.emplace_back(i, modToMemNode.second.sumFlow);
    }
  }
}


// ===================================================
// Debug
// ===================================================

void MemMapEquation::printDebug() const
{
  std::cout << "MemMapEquation::m_numPhysicalNodes: " << m_numPhysicalNodes << "\n";
  Base::printDebug();
}


} // namespace infomap
