/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef INFOMAP_OPTIMIZER_H_
#define INFOMAP_OPTIMIZER_H_

#include "InfomapOptimizerBase.h"
#include "InfomapBase.h"
#include "../utils/VectorMap.h"
#include "../utils/infomath.h"
#include "InfoNode.h"
#include "FlowData.h"

#include <set>
#include <utility>

namespace infomap {

namespace detail {

inline double edgeFlow(const InfoEdge& edge)
{
  return edge.data.flow;
}

template <typename EdgeLike>
inline double edgeFlow(const EdgeLike& edge)
{
  return edge.flow;
}

} // namespace detail

template <typename Objective>
class InfomapOptimizer : public InfomapOptimizerBase {
  using FlowDataType = FlowData;
  using DeltaFlowDataType = typename Objective::DeltaFlowDataType;

public:
  void init(InfomapBase* infomap) override
  {
    m_infomap = infomap;
    m_objective.init(infomap->getConfig());
  }

  // ===================================================
  // IO
  // ===================================================

  std::ostream& toString(std::ostream& out) const override { return out << m_objective; }

  // ===================================================
  // Getters
  // ===================================================

  double getCodelength() const override { return m_objective.getCodelength(); }

  double getIndexCodelength() const override { return m_objective.getIndexCodelength(); }

  double getModuleCodelength() const override { return m_objective.getModuleCodelength(); }

  double getMetaCodelength(bool unweighted = false) const override;

protected:
  unsigned int numActiveModules() const override { return m_infomap->activeNetwork().size() - m_emptyModules.size(); }

  template <typename Graph>
  void addNeighbourModuleLinks(Graph& graph, typename Graph::ActiveNodeId currentId, VectorMap<DeltaFlowDataType>& deltaFlow);

  template <typename Graph>
  void accumulateMoveDelta(Graph& graph,
                           typename Graph::ActiveNodeId currentId,
                           unsigned int oldModule,
                           unsigned int newModule,
                           DeltaFlowDataType& oldModuleDelta,
                           DeltaFlowDataType& newModuleDelta);

  template <typename Graph>
  void markNeighboursDirty(Graph& graph,
                           typename Graph::ActiveNodeId currentId,
                           unsigned int oldModuleIndex,
                           InfoNode*& nodeInOldModule,
                           unsigned int& numLinkedNodesInOldModule);

  template <typename Graph>
  void markNodeNeighboursDirty(Graph& graph, typename Graph::ActiveNodeId nodeId);

  template <typename Graph>
  void moveActiveNodesToPredefinedModulesImpl(Graph& graph, std::vector<unsigned int>& modules);

  template <typename Graph>
  bool moveNodeToPredefinedModuleImpl(Graph& graph, typename Graph::ActiveNodeId currentId, unsigned int newModule);

  template <typename Graph>
  unsigned int tryMoveEachNodeIntoBestModuleImpl(Graph& graph);

  // ===================================================
  // Run: Init: *
  // ===================================================

  // Init terms that is constant for the whole network
  void initTree() override;

  void initNetwork() override;

  void initSuperNetwork() override;

  double calcCodelength(const InfoNode& parent) const override { return m_objective.calcCodelength(parent); }

  // ===================================================
  // Run: Partition: *
  // ===================================================

  void initPartition() override;

  void moveActiveNodesToPredefinedModules(std::vector<unsigned int>& modules) override;

  bool moveNodeToPredefinedModule(InfoNode& current, unsigned int module);

  unsigned int optimizeActiveNetwork() override;

  unsigned int tryMoveEachNodeIntoBestModule() override;

  unsigned int tryMoveEachNodeIntoBestModuleInParallel() override;

  void consolidateModules(bool replaceExistingModules = true) override;

  bool restoreConsolidatedOptimizationPointIfNoImprovement(bool forceRestore = false) override;

  // ===================================================
  // Debug: *
  // ===================================================

  void printDebug() override { m_objective.printDebug(); }

  // ===================================================
  // Protected members
  // ===================================================

  InfomapBase* m_infomap = nullptr;
  Objective m_objective;
  Objective m_consolidatedObjective;
  std::vector<FlowDataType> m_moduleFlowData;
  std::vector<unsigned int> m_moduleMembers;
  std::vector<unsigned int> m_emptyModules;
};

// ===================================================
// Getters
// ===================================================

template <>
inline double InfomapOptimizer<MetaMapEquation>::getMetaCodelength(bool unweighted) const
{
  return m_objective.getMetaCodelength(unweighted);
}

template <typename Objective>
inline double InfomapOptimizer<Objective>::getMetaCodelength(bool /*unweighted*/) const
{
  return 0.0;
}

// ===================================================
// Run: Init: *
// ===================================================

template <typename Objective>
inline void InfomapOptimizer<Objective>::initTree()
{
  Log(4) << "InfomapOptimizer::initTree()...\n";
  m_objective.initTree(m_infomap->root());
}

template <typename Objective>
inline void InfomapOptimizer<Objective>::initNetwork()
{
  Log(4) << "InfomapOptimizer::initNetwork()...\n";
  m_objective.initNetwork(m_infomap->root());

  if (!m_infomap->isMainInfomap())
    m_objective.initSubNetwork(m_infomap->root()); // TODO: Already called in initNetwork?
}

template <typename Objective>
inline void InfomapOptimizer<Objective>::initSuperNetwork()
{
  Log(4) << "InfomapOptimizer::initSuperNetwork()...\n";
  m_objective.initSuperNetwork(m_infomap->root());
}

// ===================================================
// Run: Partition: *
// ===================================================

template <typename Objective>
template <typename Graph>
void InfomapOptimizer<Objective>::addNeighbourModuleLinks(Graph& graph, typename Graph::ActiveNodeId currentId, VectorMap<DeltaFlowDataType>& deltaFlow)
{
  graph.forEachOutEdge(currentId, [&](auto neighbourId, const InfoNode&, const auto& edge) {
    auto otherModule = graph.moduleIndex(neighbourId);
    deltaFlow.add(otherModule, DeltaFlowDataType(otherModule, detail::edgeFlow(edge), 0.0));
  });

  graph.forEachInEdge(currentId, [&](auto neighbourId, const InfoNode&, const auto& edge) {
    auto otherModule = graph.moduleIndex(neighbourId);
    deltaFlow.add(otherModule, DeltaFlowDataType(otherModule, 0.0, detail::edgeFlow(edge)));
  });
}

template <typename Objective>
template <typename Graph>
void InfomapOptimizer<Objective>::accumulateMoveDelta(Graph& graph,
                                                      typename Graph::ActiveNodeId currentId,
                                                      unsigned int oldModule,
                                                      unsigned int newModule,
                                                      DeltaFlowDataType& oldModuleDelta,
                                                      DeltaFlowDataType& newModuleDelta)
{
  graph.forEachOutEdge(currentId, [&](auto neighbourId, const InfoNode&, const auto& edge) {
    unsigned int otherModule = graph.moduleIndex(neighbourId);
    if (otherModule == oldModule) {
      oldModuleDelta.deltaExit += detail::edgeFlow(edge);
    } else if (otherModule == newModule) {
      newModuleDelta.deltaExit += detail::edgeFlow(edge);
    }
  });

  graph.forEachInEdge(currentId, [&](auto neighbourId, const InfoNode&, const auto& edge) {
    unsigned int otherModule = graph.moduleIndex(neighbourId);
    if (otherModule == oldModule) {
      oldModuleDelta.deltaEnter += detail::edgeFlow(edge);
    } else if (otherModule == newModule) {
      newModuleDelta.deltaEnter += detail::edgeFlow(edge);
    }
  });
}

template <typename Objective>
template <typename Graph>
void InfomapOptimizer<Objective>::markNeighboursDirty(Graph& graph,
                                                      typename Graph::ActiveNodeId currentId,
                                                      unsigned int oldModuleIndex,
                                                      InfoNode*& nodeInOldModule,
                                                      unsigned int& numLinkedNodesInOldModule)
{
  graph.forEachOutEdge(currentId, [&](auto neighbourId, InfoNode& neighbour, const auto&) {
    graph.dirty(neighbourId) = true;
    if (graph.moduleIndex(neighbourId) == oldModuleIndex) {
      nodeInOldModule = &neighbour;
      ++numLinkedNodesInOldModule;
    }
  });

  graph.forEachInEdge(currentId, [&](auto neighbourId, InfoNode& neighbour, const auto&) {
    graph.dirty(neighbourId) = true;
    if (graph.moduleIndex(neighbourId) == oldModuleIndex) {
      nodeInOldModule = &neighbour;
      ++numLinkedNodesInOldModule;
    }
  });
}

template <typename Objective>
template <typename Graph>
void InfomapOptimizer<Objective>::markNodeNeighboursDirty(Graph& graph, typename Graph::ActiveNodeId nodeId)
{
  graph.forEachOutEdge(nodeId, [&](auto neighbourId, InfoNode&, const auto&) {
    graph.dirty(neighbourId) = true;
  });

  graph.forEachInEdge(nodeId, [&](auto neighbourId, InfoNode&, const auto&) {
    graph.dirty(neighbourId) = true;
  });
}

template <typename Objective>
void InfomapOptimizer<Objective>::initPartition()
{
  auto initPartitionImpl = [&](auto graph) {
    auto& network = m_infomap->activeNetwork();
    Log(4) << "InfomapOptimizer::initPartition() with " << graph.size() << " nodes...\n";

    // Init one module for each node
    auto numNodes = graph.size();
    m_moduleFlowData.resize(numNodes);
    m_moduleMembers.assign(numNodes, 1);
    m_emptyModules.clear();
    m_emptyModules.reserve(numNodes);

    for (unsigned int i = 0; i < numNodes; ++i) {
      graph.moduleIndex(i) = i; // Unique module index for each node
      m_moduleFlowData[i] = graph.payloadFor(i).data;
      graph.dirty(i) = true;
    }

    m_objective.initPartition(network);
  };

  auto csrGraph = m_infomap->csrBackend();
  if (csrGraph.available()) {
    return initPartitionImpl(csrGraph);
  }

  auto graph = m_infomap->pointerBackend();
  initPartitionImpl(graph);
}

template <typename Objective>
void InfomapOptimizer<Objective>::moveActiveNodesToPredefinedModules(std::vector<unsigned int>& modules)
{
  auto csrGraph = m_infomap->csrBackend();
  if (csrGraph.available()) {
    return moveActiveNodesToPredefinedModulesImpl(csrGraph, modules);
  }
  auto graph = m_infomap->pointerBackend();
  moveActiveNodesToPredefinedModulesImpl(graph, modules);
}

template <typename Objective>
template <typename Graph>
void InfomapOptimizer<Objective>::moveActiveNodesToPredefinedModulesImpl(Graph& graph, std::vector<unsigned int>& modules)
{
  auto numNodes = graph.size();
  if (modules.size() != numNodes)
    throw std::length_error("Size of predefined modules differ from size of active network.");

  for (unsigned int i = 0; i < numNodes; ++i) {
    moveNodeToPredefinedModuleImpl(graph, i, modules[i]);
  }
}

template <typename Objective>
bool InfomapOptimizer<Objective>::moveNodeToPredefinedModule(InfoNode& current, unsigned int newModule)
{
  auto csrGraph = m_infomap->csrBackend();
  if (csrGraph.available()) {
    return moveNodeToPredefinedModuleImpl(csrGraph, csrGraph.idFor(current), newModule);
  }
  auto graph = m_infomap->pointerBackend();
  return moveNodeToPredefinedModuleImpl(graph, graph.idFor(current), newModule);
}

template <typename Objective>
template <typename Graph>
bool InfomapOptimizer<Objective>::moveNodeToPredefinedModuleImpl(Graph& graph, typename Graph::ActiveNodeId currentId, unsigned int newModule)
{
  InfoNode& current = graph.nodeFor(currentId);
  unsigned int oldM = graph.moduleIndex(currentId);
  unsigned int newM = newModule;

  if (newM == oldM) {
    return false;
  }

  DeltaFlowDataType oldModuleDelta(oldM, 0.0, 0.0);
  DeltaFlowDataType newModuleDelta(newM, 0.0, 0.0);

  accumulateMoveDelta(graph, currentId, oldM, newM, oldModuleDelta, newModuleDelta);

  // For recorded teleportation
  if (m_infomap->recordedTeleportation) {
    auto& oldModuleFlowData = m_moduleFlowData[oldM];
    double deltaEnterOld = (oldModuleFlowData.teleportFlow - current.data.teleportFlow) * current.data.teleportWeight;
    double deltaExitOld = current.data.teleportFlow * (oldModuleFlowData.teleportWeight - current.data.teleportWeight);
    oldModuleDelta.deltaEnter += deltaEnterOld;
    oldModuleDelta.deltaExit += deltaExitOld;

    auto& newModuleFlowData = m_moduleFlowData[newM];
    double deltaEnterNew = current.data.teleportFlow * newModuleFlowData.teleportWeight;
    double deltaExitNew = newModuleFlowData.teleportFlow * current.data.teleportWeight;
    newModuleDelta.deltaEnter += deltaEnterNew;
    newModuleDelta.deltaExit += deltaExitNew;
  }
  // Update empty module vector
  if (m_moduleMembers[newM] == 0) {
    m_emptyModules.pop_back();
  }
  if (m_moduleMembers[oldM] == 1) {
    m_emptyModules.push_back(oldM);
  }

  m_objective.updateCodelengthOnMovingNode(current, oldModuleDelta, newModuleDelta, m_moduleFlowData, m_moduleMembers);

  m_moduleMembers[oldM] -= 1;
  m_moduleMembers[newM] += 1;

  graph.moduleIndex(currentId) = newM;
  return true;
}

template <typename Objective>
inline unsigned int InfomapOptimizer<Objective>::optimizeActiveNetwork()
{
  unsigned int coreLoopCount = 0;
  unsigned int numEffectiveLoops = 0;
  double oldCodelength = m_objective.getCodelength();
  unsigned int loopLimit = m_infomap->coreLoopLimit;
  unsigned int minRandLoop = 2;
  if (loopLimit >= minRandLoop && m_infomap->randomizeCoreLoopLimit)
    loopLimit = m_infomap->m_rand.randInt(minRandLoop, loopLimit);
  if (m_infomap->m_aggregationLevel > 0 || m_infomap->m_isCoarseTune) {
    loopLimit = 20;
  }

  do {
    ++coreLoopCount;
    unsigned int numNodesMoved = m_infomap->innerParallelization
        ? tryMoveEachNodeIntoBestModuleInParallel()
        : tryMoveEachNodeIntoBestModule();
    // Break if not enough improvement
    if (numNodesMoved == 0 || m_objective.getCodelength() >= oldCodelength - m_infomap->minimumCodelengthImprovement)
      break;
    ++numEffectiveLoops;
    oldCodelength = m_objective.getCodelength();
  } while (coreLoopCount != loopLimit);

  return numEffectiveLoops;
}

template <typename Objective>
unsigned int InfomapOptimizer<Objective>::tryMoveEachNodeIntoBestModule()
{
  auto csrGraph = m_infomap->csrBackend();
  if (csrGraph.available()) {
    return tryMoveEachNodeIntoBestModuleImpl(csrGraph);
  }
  auto graph = m_infomap->pointerBackend();
  return tryMoveEachNodeIntoBestModuleImpl(graph);
}

template <typename Objective>
template <typename Graph>
unsigned int InfomapOptimizer<Objective>::tryMoveEachNodeIntoBestModuleImpl(Graph& graph)
{
  // Get random enumeration of nodes
  std::vector<unsigned int> nodeEnumeration(graph.size());
  m_infomap->m_rand.getRandomizedIndexVector(nodeEnumeration);

  auto numNodes = nodeEnumeration.size();
  unsigned int numMoved = 0;

  // Create map with module links
  VectorMap<DeltaFlowDataType> deltaFlow(numNodes);

  for (unsigned int i = 0; i < numNodes; ++i) {
    auto currentId = nodeEnumeration[i];
    InfoNode& current = graph.nodeFor(currentId);
    unsigned int currentModuleIndex = graph.moduleIndex(currentId);

    if (!graph.dirty(currentId))
      continue;

    // If other nodes have moved here, don't move away on first loop
    if (m_moduleMembers[currentModuleIndex] > 1 && m_infomap->isFirstLoop() && m_infomap->tuneIterationLimit != 1)
      continue;

    // If no links connecting this node with other nodes, it won't move into others,
    // and others won't move into this. TODO: Always best leave it alone?
    // For memory networks, don't skip try move to same physical node!

    deltaFlow.startRound();

    // For all outlinks
    addNeighbourModuleLinks(graph, currentId, deltaFlow);

    // For not moving
    deltaFlow.add(currentModuleIndex, DeltaFlowDataType(currentModuleIndex, 0.0, 0.0));
    DeltaFlowDataType& oldModuleDelta = deltaFlow[currentModuleIndex];
    oldModuleDelta.module = currentModuleIndex; // Make sure index is correct if created new

    // Option to move to empty module (if node not already alone)
    if (m_moduleMembers[currentModuleIndex] > 1 && !m_emptyModules.empty()) {
      deltaFlow.add(m_emptyModules.back(), DeltaFlowDataType(m_emptyModules.back(), 0.0, 0.0));
    }

    // For memory networks
    m_objective.addMemoryContributions(current, oldModuleDelta, deltaFlow);

    auto& moduleDeltaEnterExit = deltaFlow.values();
    unsigned int numModuleLinks = deltaFlow.size();

    // For recorded teleportation
    if (m_infomap->recordedTeleportation) {
      for (unsigned int j = 0; j < numModuleLinks; ++j) {
        auto& deltaEnterExit = moduleDeltaEnterExit[j];
        auto moduleIndex = deltaEnterExit.module;
        if (moduleIndex == currentModuleIndex) {
          auto& oldModuleFlowData = m_moduleFlowData[moduleIndex];
          double deltaEnterOld = (oldModuleFlowData.teleportFlow - current.data.teleportFlow) * current.data.teleportWeight;
          double deltaExitOld = current.data.teleportFlow * (oldModuleFlowData.teleportWeight - current.data.teleportWeight);
          deltaFlow.add(moduleIndex, DeltaFlowDataType(moduleIndex, deltaExitOld, deltaEnterOld));
        } else {
          auto& newModuleFlowData = m_moduleFlowData[moduleIndex];
          double deltaEnterNew = newModuleFlowData.teleportFlow * current.data.teleportWeight;
          double deltaExitNew = current.data.teleportFlow * newModuleFlowData.teleportWeight;
          deltaFlow.add(moduleIndex, DeltaFlowDataType(moduleIndex, deltaExitNew, deltaEnterNew));
        }
      }
    }

    // Randomize link order for optimized search
    std::vector<unsigned int> moduleEnumeration(numModuleLinks);
    m_infomap->m_rand.getRandomizedIndexVector(moduleEnumeration);

    DeltaFlowDataType bestDeltaModule(oldModuleDelta);
    double bestDeltaCodelength = 0.0;
    DeltaFlowDataType strongestConnectedModule(oldModuleDelta);
    double deltaCodelengthOnStrongestConnectedModule = 0.0;

    // Find the move that minimizes the description length
    for (unsigned int k = 0; k < numModuleLinks; ++k) {
      auto j = moduleEnumeration[k];
      unsigned int otherModule = moduleDeltaEnterExit[j].module;
      if (otherModule != currentModuleIndex) {
        double deltaCodelength = m_objective.getDeltaCodelengthOnMovingNode(current,
                                                                            oldModuleDelta,
                                                                            moduleDeltaEnterExit[j],
                                                                            m_moduleFlowData,
                                                                            m_moduleMembers);

        if (deltaCodelength < bestDeltaCodelength - m_infomap->minimumSingleNodeCodelengthImprovement) {
          bestDeltaModule = moduleDeltaEnterExit[j];
          bestDeltaCodelength = deltaCodelength;
        }

        // Save strongest connected module to prefer if codelength improvement equal
        if (moduleDeltaEnterExit[j].deltaExit > strongestConnectedModule.deltaExit) {
          strongestConnectedModule = moduleDeltaEnterExit[j];
          deltaCodelengthOnStrongestConnectedModule = deltaCodelength;
        }
      }
    }

    // Prefer strongest connected module if equal delta codelength
    if (strongestConnectedModule.module != bestDeltaModule.module && deltaCodelengthOnStrongestConnectedModule <= bestDeltaCodelength + m_infomap->minimumSingleNodeCodelengthImprovement) {
      bestDeltaModule = strongestConnectedModule;
    }

    // Make best possible move
    if (bestDeltaModule.module != currentModuleIndex) {
      unsigned int bestModuleIndex = bestDeltaModule.module;
      // Update empty module vector
      if (m_moduleMembers[bestModuleIndex] == 0) {
        m_emptyModules.pop_back();
      }
      if (m_moduleMembers[currentModuleIndex] == 1) {
        m_emptyModules.push_back(currentModuleIndex);
      }

      m_objective.updateCodelengthOnMovingNode(current, oldModuleDelta, bestDeltaModule, m_moduleFlowData, m_moduleMembers);

      m_moduleMembers[currentModuleIndex] -= 1;
      m_moduleMembers[bestModuleIndex] += 1;

      unsigned int oldModuleIndex = currentModuleIndex;
      graph.moduleIndex(currentId) = bestModuleIndex;

      ++numMoved;

      InfoNode* nodeInOldModule = &current;
      unsigned int numLinkedNodesInOldModule = 0;
      // Mark neighbours as dirty
      markNeighboursDirty(graph, currentId, oldModuleIndex, nodeInOldModule, numLinkedNodesInOldModule);

      // Move single connected nodes to same module
      if (numLinkedNodesInOldModule == 1 && m_moduleMembers[oldModuleIndex] == 1) {
        moveNodeToPredefinedModuleImpl(graph, graph.idFor(*nodeInOldModule), bestModuleIndex);
        ++numMoved;
        // Mark neighbours as dirty
        if (nodeInOldModule->degree() > 1) {
          auto linkedNodeId = graph.idFor(*nodeInOldModule);
          markNodeNeighboursDirty(graph, linkedNodeId);
        }
      }
    } else {
      graph.dirty(currentId) = false;
    }
  }

  return numMoved;
}

/**
 * Minimize the codelength by trying to move each node into best module, in parallel.
 *
 * For each node:
 * 1. Calculate the change in codelength for a move to each of its neighbouring modules or to an empty module
 * 2. Move to the one that reduces the codelength the most, if any.
 *
 * @return The number of nodes moved.
 */
template <typename Objective>
unsigned int InfomapOptimizer<Objective>::tryMoveEachNodeIntoBestModuleInParallel()
{
  // Get random enumeration of nodes
  auto& network = m_infomap->activeNetwork();
  std::vector<unsigned int> nodeEnumeration(network.size());
  m_infomap->m_rand.getRandomizedIndexVector(nodeEnumeration);

  auto numNodes = nodeEnumeration.size();
  unsigned int numMoved = 0;
  unsigned int numInvalidMoves = 0;

#pragma omp parallel for schedule(dynamic) // Use dynamic scheduling as some threads could end early
  for (unsigned int i = 0; i < numNodes; ++i) {
    // Pick nodes in random order
    InfoNode& current = *network[nodeEnumeration[i]];

    if (!current.dirty)
      continue;

    // If other nodes have moved here, don't move away on first loop
    if (m_moduleMembers[current.index] > 1 && m_infomap->isFirstLoop() && m_infomap->tuneIterationLimit != 1)
      continue;

    // If no links connecting this node with other nodes, it won't move into others,
    // and others won't move into this. TODO: Always best leave it alone?
    // For memory networks, don't skip try move to same physical node!

    // Create map with module links
    VectorMap<DeltaFlowDataType> deltaFlow(numNodes);

    // For all outlinks
    for (auto& e : current.outEdges()) {
      auto& edge = *e;
      InfoNode* neighbour = edge.target;
      deltaFlow.add(neighbour->index, DeltaFlowDataType(neighbour->index, edge.data.flow, 0.0));
    }
    // For all inlinks
    for (auto& e : current.inEdges()) {
      auto& edge = *e;
      InfoNode* neighbour = edge.source;
      deltaFlow.add(neighbour->index, DeltaFlowDataType(neighbour->index, 0.0, edge.data.flow));
    }

    // For not moving
    deltaFlow.add(current.index, DeltaFlowDataType(current.index, 0.0, 0.0));
    DeltaFlowDataType& oldModuleDelta = deltaFlow[current.index];
    oldModuleDelta.module = current.index; // Make sure index is correct if created new

    // Option to move to empty module (if node not already alone)
    if (m_moduleMembers[current.index] > 1 && !m_emptyModules.empty()) {
      // deltaFlow[m_emptyModules.back()] += DeltaFlowDataType(m_emptyModules.back(), 0.0, 0.0);
      deltaFlow.add(m_emptyModules.back(), DeltaFlowDataType(m_emptyModules.back(), 0.0, 0.0));
    }

    // For memory networks
    m_objective.addMemoryContributions(current, oldModuleDelta, deltaFlow);

    auto& moduleDeltaEnterExit = deltaFlow.values();
    unsigned int numModuleLinks = deltaFlow.size();

    // Randomize link order for optimized search
    if (numModuleLinks > 2) {
      for (unsigned int j = 0; j < numModuleLinks - 2; ++j) {
        unsigned int randPos = m_infomap->m_rand.randInt(j + 1, numModuleLinks - 1);
        swap(moduleDeltaEnterExit[j], moduleDeltaEnterExit[randPos]);
      }
    }

    DeltaFlowDataType bestDeltaModule(oldModuleDelta);
    double bestDeltaCodelength = 0.0;
    DeltaFlowDataType strongestConnectedModule(oldModuleDelta);
    double deltaCodelengthOnStrongestConnectedModule = 0.0;

    // Find the move that minimizes the description length
    for (unsigned int j = 0; j < deltaFlow.size(); ++j) {
      unsigned int otherModule = moduleDeltaEnterExit[j].module;
      if (otherModule != current.index) {
        double deltaCodelength = m_objective.getDeltaCodelengthOnMovingNode(current,
                                                                            oldModuleDelta,
                                                                            moduleDeltaEnterExit[j],
                                                                            m_moduleFlowData,
                                                                            m_moduleMembers);

        if (deltaCodelength < bestDeltaCodelength - m_infomap->minimumSingleNodeCodelengthImprovement) {
          bestDeltaModule = moduleDeltaEnterExit[j];
          bestDeltaCodelength = deltaCodelength;
        }

        // Save strongest connected module to prefer if codelength improvement equal
        if (moduleDeltaEnterExit[j].deltaExit > strongestConnectedModule.deltaExit) {
          strongestConnectedModule = moduleDeltaEnterExit[j];
          deltaCodelengthOnStrongestConnectedModule = deltaCodelength;
        }
      }
    }

    // Prefer strongest connected module if equal delta codelength
    if (strongestConnectedModule.module != bestDeltaModule.module && deltaCodelengthOnStrongestConnectedModule <= bestDeltaCodelength + m_infomap->minimumSingleNodeCodelengthImprovement) {
      bestDeltaModule = strongestConnectedModule;
    }

    // Make best possible move
    if (bestDeltaModule.module == current.index) {
      current.dirty = false;
      continue;
    } else {
#pragma omp critical(moveUpdate)
      {
        unsigned int bestModuleIndex = bestDeltaModule.module;
        unsigned int oldModuleIndex = current.index;

        bool validMove = bestModuleIndex == m_emptyModules.back()
            // Check validity of move to empty target
            ? m_moduleMembers[oldModuleIndex] > 1 && !m_emptyModules.empty()
            // Not valid if the best module is empty now but not when decided
            : m_moduleMembers[bestModuleIndex] > 0;

        if (validMove) {
          // Recalculate delta codelength for proposed move to see if still an improvement
          oldModuleDelta = DeltaFlowDataType(oldModuleIndex, 0.0, 0.0);
          DeltaFlowDataType newModuleDelta(bestModuleIndex, 0.0, 0.0);

          // For all outlinks
          for (auto& e : current.outEdges()) {
            auto& edge = *e;
            unsigned int otherModule = edge.target->index;
            if (otherModule == oldModuleIndex)
              oldModuleDelta.deltaExit += edge.data.flow;
            else if (otherModule == bestModuleIndex)
              newModuleDelta.deltaExit += edge.data.flow;
          }
          // For all inlinks
          for (auto& e : current.inEdges()) {
            auto& edge = *e;
            unsigned int otherModule = edge.source->index;
            if (otherModule == oldModuleIndex)
              oldModuleDelta.deltaEnter += edge.data.flow;
            else if (otherModule == bestModuleIndex)
              newModuleDelta.deltaEnter += edge.data.flow;
          }

          // For memory networks
          m_objective.addMemoryContributions(current, oldModuleDelta, deltaFlow);

          double deltaCodelength = m_objective.getDeltaCodelengthOnMovingNode(current,
                                                                              oldModuleDelta,
                                                                              newModuleDelta,
                                                                              m_moduleFlowData,
                                                                              m_moduleMembers);

          if (deltaCodelength < 0.0 - m_infomap->minimumSingleNodeCodelengthImprovement) {
            // Update empty module vector
            if (m_moduleMembers[bestModuleIndex] == 0) {
              m_emptyModules.pop_back();
            }
            if (m_moduleMembers[oldModuleIndex] == 1) {
              m_emptyModules.push_back(oldModuleIndex);
            }

            m_objective.updateCodelengthOnMovingNode(current, oldModuleDelta, bestDeltaModule, m_moduleFlowData, m_moduleMembers);

            m_moduleMembers[oldModuleIndex] -= 1;
            m_moduleMembers[bestModuleIndex] += 1;

            current.index = bestModuleIndex;

            ++numMoved;

            // Mark neighbours as dirty
            for (auto& e : current.outEdges())
              e->target->dirty = true;
            for (auto& e : current.inEdges())
              e->source->dirty = true;
          } else {
            ++numInvalidMoves;
          }
        } else {
          ++numInvalidMoves;
        }
      }
    }
  }

  return numMoved + numInvalidMoves;
}

template <typename Objective>
inline void InfomapOptimizer<Objective>::consolidateModules(bool replaceExistingModules)
{
  auto graph = m_infomap->pointerBackend();
  auto numNodes = graph.size();
  std::vector<InfoNode*> modules(numNodes, nullptr);

  InfoNode& firstActiveNode = graph.nodeFor(0);
  auto level = firstActiveNode.depth();
  auto leafLevel = m_infomap->numLevels();

  if (leafLevel == 1)
    replaceExistingModules = false;

  // Release children pointers on current parent(s) to put new modules between
  for (unsigned int i = 0; i < numNodes; ++i) {
    graph.nodeFor(i).parent->releaseChildren(); // Safe to call multiple times
  }

  // Create the new module nodes and re-parent the active network from its common parent to the new module level
  for (unsigned int i = 0; i < numNodes; ++i) {
    InfoNode& node = graph.nodeFor(i);
    unsigned int moduleIndex = graph.moduleIndex(i);
    if (modules[moduleIndex] == nullptr) {
      modules[moduleIndex] = new InfoNode(m_moduleFlowData[moduleIndex]);
      modules[moduleIndex]->index = moduleIndex;
      node.parent->addChild(modules[moduleIndex]);
    }
    modules[moduleIndex]->addChild(&node);
  }

  using NodePair = std::pair<unsigned int, unsigned int>;
  using EdgeMap = std::map<NodePair, double>;
  EdgeMap moduleLinks;

  for (unsigned int i = 0; i < numNodes; ++i) {
    unsigned int module1 = graph.moduleIndex(i);
    for (const auto& edge : graph.outEdges(i)) {
      unsigned int module2 = graph.moduleIndex(edge.neighbourId);
      if (module1 != module2) {
        // Use new variables to not swap module1
        unsigned int m1 = module1, m2 = module2;
        // If undirected, the order may be swapped to aggregate the edge on an opposite one
        if (m_infomap->isUndirectedClustering() && m1 > m2)
          std::swap(m1, m2);
        auto ret = moduleLinks.insert(std::make_pair(NodePair(m1, m2), edge.flow));
        if (!ret.second) {
          ret.first->second += edge.flow;
        }
      }
    }
  }

  // Add the aggregated edge flow structure to the new modules
  for (auto& e : moduleLinks) {
    const auto& nodePair = e.first;
    modules[nodePair.first]->addOutEdge(*modules[nodePair.second], 0.0, e.second);
  }

  unsigned int moduleCount = 0;
  for (const auto* module : modules) {
    if (module != nullptr) {
      ++moduleCount;
    }
  }
  m_infomap->recordConsolidationSnapshot(level, numNodes, moduleCount, moduleLinks.size(), replaceExistingModules);

  if (replaceExistingModules) {
    if (level == 1) {
      Log(4) << "Consolidated super modules, removing old modules...\n";
      for (unsigned int i = 0; i < numNodes; ++i)
        graph.nodeFor(i).replaceWithChildren();
    } else if (level == 2) {
      Log(4) << "Consolidated sub-modules, removing modules...\n";
      unsigned int moduleIndex = 0;
      for (InfoNode& module : m_infomap->root()) {
        // Store current modular structure on the sub-modules
        for (auto& subModule : module)
          subModule.index = moduleIndex;
        ++moduleIndex;
      }
      m_infomap->root().replaceChildrenWithGrandChildren();
    }
  }

  // Calculate the number of non-trivial modules
  m_infomap->m_numNonTrivialTopModules = 0;
  for (auto& module : m_infomap->root()) {
    if (module.childDegree() != 1)
      ++m_infomap->m_numNonTrivialTopModules;
  }

  m_objective.consolidateModules(modules);
  m_consolidatedObjective = m_objective;
}

template <typename Objective>
inline bool InfomapOptimizer<Objective>::restoreConsolidatedOptimizationPointIfNoImprovement(bool forceRestore)
{
  if (forceRestore || m_objective.getCodelength() >= m_consolidatedObjective.getCodelength() - m_infomap->minimumSingleNodeCodelengthImprovement) {
    m_objective = m_consolidatedObjective;
    return true;
  }
  return false;
}

} /* namespace infomap */

#endif // INFOMAP_OPTIMIZER_H_
