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
#include "../utils/Random.h"

#include <set>
#include <utility>
#include <algorithm>
#include <vector>

namespace infomap {

template <typename Objective>
class InfomapOptimizer : public InfomapOptimizerBase {
  using FlowDataType = typename Objective::FlowDataType;
  using DeltaFlowDataType = typename Objective::DeltaFlowDataType;

public:
  void init(InfomapBase* infomap) override
  {
    m_infomap = infomap;
    m_objective.init(infomap->getConfig());
    m_innerParallelMoveSweep = 0;
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

  void setNetworkProperties(const StateNetwork& network) override;

  void inheritNetworkPropertiesFrom(const InfomapOptimizerBase& parent) override;

protected:
  unsigned int numActiveModules() const override { return m_infomap->activeNetwork().size() - m_emptyModules.size(); }

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

  bool shouldUseInnerParallelization() const;

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
  unsigned int m_innerParallelMoveSweep = 0;
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

// Only BiasedMapEquation uses per-network properties; every other objective keeps the no-op.
template <typename Objective>
inline void InfomapOptimizer<Objective>::setNetworkProperties(const StateNetwork& /*network*/)
{
}

template <>
inline void InfomapOptimizer<BiasedMapEquation>::setNetworkProperties(const StateNetwork& network)
{
  m_objective.setNetworkProperties(network);
}

template <typename Objective>
inline void InfomapOptimizer<Objective>::inheritNetworkPropertiesFrom(const InfomapOptimizerBase& /*parent*/)
{
}

template <>
inline void InfomapOptimizer<BiasedMapEquation>::inheritNetworkPropertiesFrom(const InfomapOptimizerBase& parent)
{
  // The objective type is identical across a whole run, so the parent optimizer is also a
  // BiasedMapEquation optimizer. Copy the full-network properties so sub/super instances keep
  // the same entropy bias correction normalization the shared static used to provide.
  if (const auto* p = dynamic_cast<const InfomapOptimizer<BiasedMapEquation>*>(&parent))
    m_objective.setNetworkPropertiesFrom(p->m_objective);
}

template <>
inline bool InfomapOptimizer<MetaMapEquation>::shouldUseInnerParallelization() const
{
  // MetaMapEquation's delta evaluation mutates shared metadata state while
  // testing moves, so the lock-free inner parallel loop is not thread-safe.
  return false;
}

template <>
inline bool InfomapOptimizer<MemMapEquation>::shouldUseInnerParallelization() const
{
  // MemMapEquation keeps extra physical-node state that is updated together
  // with accepted moves. Keep it on the stable serial path until that state can
  // be batched with the module-flow updates.
  return false;
}

#if INFOMAP_FEATURE_REGULARIZED_HIGHER_ORDER
template <>
inline bool InfomapOptimizer<RegularizedMultilayerMapEquation>::shouldUseInnerParallelization() const
{
  // RegularizedMultilayerMapEquation extends the memory objective with layer
  // teleportation state. Keep it on the stable serial path until those updates
  // can be batched with module-flow and physical-node updates.
  return false;
}
#endif

template <typename Objective>
inline bool InfomapOptimizer<Objective>::shouldUseInnerParallelization() const
{
  return m_infomap->innerParallelization;
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
void InfomapOptimizer<Objective>::initPartition()
{
  auto& network = m_infomap->activeNetwork();
  Log(4) << "InfomapOptimizer::initPartition() with " << network.size() << " nodes...\n";

  // Init one module for each node
  auto numNodes = network.size();
  m_moduleFlowData.resize(numNodes);
  m_moduleMembers.assign(numNodes, 1);
  m_emptyModules.clear();
  m_emptyModules.reserve(numNodes);

  unsigned int i = 0;
  for (auto& nodePtr : network) {
    InfoNode& node = *nodePtr;
    node.index = i; // Unique module index for each node
    m_moduleFlowData[i] = node.data;
    node.dirty = true;
    ++i;
  }

  m_objective.initPartition(network);
}

template <typename Objective>
void InfomapOptimizer<Objective>::moveActiveNodesToPredefinedModules(std::vector<unsigned int>& modules)
{
  auto& network = m_infomap->activeNetwork();
  auto numNodes = network.size();
  if (modules.size() != numNodes)
    throw std::length_error("Size of predefined modules differ from size of active network.");

  for (unsigned int i = 0; i < numNodes; ++i) {
    moveNodeToPredefinedModule(*network[i], modules[i]);
  }
}

template <typename Objective>
bool InfomapOptimizer<Objective>::moveNodeToPredefinedModule(InfoNode& current, unsigned int newModule)
{
  unsigned int oldM = current.index;
  unsigned int newM = newModule;

  if (newM == oldM) {
    return false;
  }

  DeltaFlowDataType oldModuleDelta(oldM, 0.0, 0.0);
  DeltaFlowDataType newModuleDelta(newM, 0.0, 0.0);

  // For all outlinks
  for (auto& e : current.outEdges()) {
    auto& edge = *e;
    unsigned int otherModule = edge.target->index;
    if (otherModule == oldM) {
      oldModuleDelta.deltaExit += edge.data.flow;
    } else if (otherModule == newM) {
      newModuleDelta.deltaExit += edge.data.flow;
    }
  }
  // For all inlinks
  for (auto& e : current.inEdges()) {
    auto& edge = *e;
    unsigned int otherModule = edge.source->index;
    if (otherModule == oldM) {
      oldModuleDelta.deltaEnter += edge.data.flow;
    } else if (otherModule == newM) {
      newModuleDelta.deltaEnter += edge.data.flow;
    }
  }

  // For recorded teleportation
  m_objective.addTeleportationFlow(current, m_moduleFlowData, oldModuleDelta, newModuleDelta);

  // Update empty module vector
  if (m_moduleMembers[newM] == 0) {
    m_emptyModules.pop_back();
  }
  if (m_moduleMembers[current.index] == 1) {
    m_emptyModules.push_back(oldM);
  }

  m_objective.updateCodelengthOnMovingNode(current, oldModuleDelta, newModuleDelta, m_moduleFlowData, m_moduleMembers);

  m_moduleMembers[oldM] -= 1;
  m_moduleMembers[newM] += 1;

  current.index = newM;
  return true;
}

template <typename Objective>
INFOMAP_HOT inline unsigned int InfomapOptimizer<Objective>::optimizeActiveNetwork()
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
    unsigned int numNodesMoved = shouldUseInnerParallelization()
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
INFOMAP_HOT unsigned int InfomapOptimizer<Objective>::tryMoveEachNodeIntoBestModule()
{
  // Get random enumeration of nodes
  auto& network = m_infomap->activeNetwork();
  std::vector<unsigned int> nodeEnumeration(network.size());
  m_infomap->m_rand.getRandomizedIndexVector(nodeEnumeration);

  unsigned int numNodes = nodeEnumeration.size();
  unsigned int numMoved = 0;
  unsigned int numRandomMoves = std::min(m_infomap->numRandomMoves, numNodes);

  // Create map with module links
  VectorMap<DeltaFlowDataType> deltaFlow(numNodes);

  for (unsigned int i = 0; i < numNodes; ++i) {
    InfoNode& current = *network[nodeEnumeration[i]];

    if (!current.dirty)
      continue;

    // If other nodes have moved here, don't move away on first loop
    if (m_moduleMembers[current.index] > 1 && m_infomap->isFirstLoop() && m_infomap->tuneIterationLimit != 1)
      continue;

    deltaFlow.startRound();

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

    // For random moves
    if (current.degree() <= m_infomap->maxDegreeForRandomMoves) {
      for (unsigned int j = 0; j < numRandomMoves; ++j) {
        unsigned int randIndex = m_infomap->m_rand.randInt(0, numNodes - 1);
        InfoNode& neighbour = *network[nodeEnumeration[randIndex]];
        deltaFlow.add(neighbour.index, DeltaFlowDataType(neighbour.index, 0, 0));
      }
    }

    // For not moving
    deltaFlow.add(current.index, DeltaFlowDataType(current.index, 0.0, 0.0));
    DeltaFlowDataType& oldModuleDelta = deltaFlow[current.index];
    oldModuleDelta.module = current.index; // Make sure index is correct if created new

    // Option to move to empty module (if node not already alone)
    if (m_moduleMembers[current.index] > 1 && !m_emptyModules.empty()) {
      deltaFlow.add(m_emptyModules.back(), DeltaFlowDataType(m_emptyModules.back(), 0.0, 0.0));
    }

    // For memory networks
    m_objective.addMemoryContributions(current, oldModuleDelta, deltaFlow);

    // For recorded teleportation
    m_objective.addTeleportationFlow(current, m_moduleFlowData, deltaFlow);

    auto& moduleDeltaEnterExit = deltaFlow.values();
    unsigned int numModuleLinks = deltaFlow.size();

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
    if (bestDeltaModule.module != current.index) {
      unsigned int bestModuleIndex = bestDeltaModule.module;
      // Update empty module vector
      if (m_moduleMembers[bestModuleIndex] == 0) {
        m_emptyModules.pop_back();
      }
      if (m_moduleMembers[current.index] == 1) {
        m_emptyModules.push_back(current.index);
      }

      m_objective.updateCodelengthOnMovingNode(current, oldModuleDelta, bestDeltaModule, m_moduleFlowData, m_moduleMembers);

      m_moduleMembers[current.index] -= 1;
      m_moduleMembers[bestModuleIndex] += 1;

      unsigned int oldModuleIndex = current.index;
      current.index = bestModuleIndex;

      ++numMoved;

      InfoNode* nodeInOldModule = &current;
      unsigned int numLinkedNodesInOldModule = 0;
      // Mark neighbours as dirty
      for (auto& e : current.outEdges()) {
        e->target->dirty = true;
        if (e->target->index == oldModuleIndex) {
          nodeInOldModule = e->target;
          ++numLinkedNodesInOldModule;
        }
      }
      for (auto& e : current.inEdges()) {
        e->source->dirty = true;
        if (e->source->index == oldModuleIndex) {
          nodeInOldModule = e->source;
          ++numLinkedNodesInOldModule;
        }
      }

      // Move single connected nodes to same module
      if (numLinkedNodesInOldModule == 1 && m_moduleMembers[oldModuleIndex] == 1) {
        moveNodeToPredefinedModule(*nodeInOldModule, bestModuleIndex);
        ++numMoved;
        // Mark neighbours as dirty
        if (nodeInOldModule->degree() > 1) {
          for (auto& e : nodeInOldModule->outEdges())
            e->target->dirty = true;
          for (auto& e : nodeInOldModule->inEdges())
            e->source->dirty = true;
        }
      }
    } else {
      current.dirty = false;
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
INFOMAP_HOT unsigned int InfomapOptimizer<Objective>::tryMoveEachNodeIntoBestModuleInParallel()
{
  // Get random enumeration of nodes
  auto& network = m_infomap->activeNetwork();
  std::vector<unsigned int> nodeEnumeration(network.size());
  m_infomap->m_rand.getRandomizedIndexVector(nodeEnumeration);

  unsigned int numNodes = nodeEnumeration.size();
  unsigned int numRandomMoves = std::min(m_infomap->numRandomMoves, numNodes);
  const unsigned int parallelMoveSweep = ++m_innerParallelMoveSweep;

  std::vector<std::vector<unsigned int>> randomMoveTargets(numNodes);
  if (numRandomMoves > 0) {
    for (unsigned int i = 0; i < numNodes; ++i) {
      InfoNode& current = *network[nodeEnumeration[i]];
      if (!current.dirty)
        continue;
      if (m_moduleMembers[current.index] > 1 && m_infomap->isFirstLoop() && m_infomap->tuneIterationLimit != 1)
        continue;
      if (current.degree() > m_infomap->maxDegreeForRandomMoves)
        continue;
      auto& targets = randomMoveTargets[nodeEnumeration[i]];
      targets.reserve(numRandomMoves);
      for (unsigned int j = 0; j < numRandomMoves; ++j) {
        unsigned int randIndex = m_infomap->m_rand.randInt(0, numNodes - 1);
        targets.push_back(nodeEnumeration[randIndex]);
      }
    }
  }

  struct MoveProposal {
    bool valid = false;
    bool clearDirty = false;
    unsigned int nodeIndex = 0;
    unsigned int oldModule = 0;
    unsigned int newModule = 0;
    bool targetWasEmpty = false;
  };

  std::vector<MoveProposal> proposals(numNodes);

#pragma omp parallel
  {
    VectorMap<DeltaFlowDataType> deltaFlow(numNodes);
    std::vector<unsigned int> moduleEnumeration;

#pragma omp for schedule(dynamic) // Use dynamic scheduling as some threads could end early
    for (unsigned int i = 0; i < numNodes; ++i) {
      deltaFlow.startRound();

      // Pick nodes in random order
      unsigned int nodeIndex = nodeEnumeration[i];
      InfoNode& current = *network[nodeIndex];

      if (!current.dirty)
        continue;

      // If other nodes have moved here, don't move away on first loop
      if (m_moduleMembers[current.index] > 1 && m_infomap->isFirstLoop() && m_infomap->tuneIterationLimit != 1)
        continue;

      // If no links connecting this node with other nodes, it won't move into others,
      // and others won't move into this. TODO: Always best leave it alone?
      // For memory networks, don't skip try move to same physical node!

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

      // For random moves
      for (unsigned int randNodeIndex : randomMoveTargets[nodeIndex]) {
        InfoNode& neighbour = *network[randNodeIndex];
        deltaFlow.add(neighbour.index, DeltaFlowDataType(neighbour.index, 0.0, 0.0));
      }

      // For not moving
      deltaFlow.add(current.index, DeltaFlowDataType(current.index, 0.0, 0.0));
      DeltaFlowDataType& oldModuleDelta = deltaFlow[current.index];
      oldModuleDelta.module = current.index; // Make sure index is correct if created new

      // Option to move to empty module (if node not already alone)
      if (m_moduleMembers[current.index] > 1 && !m_emptyModules.empty()) {
        deltaFlow.add(m_emptyModules.back(), DeltaFlowDataType(m_emptyModules.back(), 0.0, 0.0));
      }

      // For memory networks
      m_objective.addMemoryContributions(current, oldModuleDelta, deltaFlow);

      // For recorded teleportation
      m_objective.addTeleportationFlow(current, m_moduleFlowData, deltaFlow);

      auto& moduleDeltaEnterExit = deltaFlow.values();
      unsigned int numModuleLinks = deltaFlow.size();

      // Randomize link order for optimized search without sharing m_rand across threads.
      moduleEnumeration.resize(numModuleLinks);
      Random moduleRand(static_cast<unsigned int>(m_infomap->seedToRandomNumberGenerator)
                        + 0x9e3779b9u * (nodeIndex + 1u)
                        + 0x85ebca6bu * parallelMoveSweep);
      moduleRand.getRandomizedIndexVector(moduleEnumeration);

      DeltaFlowDataType bestDeltaModule(oldModuleDelta);
      double bestDeltaCodelength = 0.0;
      DeltaFlowDataType strongestConnectedModule(oldModuleDelta);
      double deltaCodelengthOnStrongestConnectedModule = 0.0;

      // Find the move that minimizes the description length
      for (unsigned int k = 0; k < numModuleLinks; ++k) {
        auto j = moduleEnumeration[k];
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
        auto& proposal = proposals[nodeIndex];
        proposal.clearDirty = true;
        proposal.nodeIndex = nodeIndex;
        continue;
      }

      auto& proposal = proposals[nodeIndex];
      proposal.valid = true;
      proposal.nodeIndex = nodeIndex;
      proposal.oldModule = current.index;
      proposal.newModule = bestDeltaModule.module;
      proposal.targetWasEmpty = m_moduleMembers[bestDeltaModule.module] == 0;
    }
  }

  std::vector<unsigned int> selectedProposalIndices;
  selectedProposalIndices.reserve(numNodes);
  unsigned int numProposals = 0;
  unsigned int numSkippedChangedModule = 0;
  unsigned int numSkippedInvalidTarget = 0;
  unsigned int numSkippedRejectedRecheck = 0;

  for (unsigned int nodeIndex : nodeEnumeration) {
    auto& proposal = proposals[nodeIndex];
    if (!proposal.valid)
      continue;
    ++numProposals;
    InfoNode& current = *network[proposal.nodeIndex];
    if (current.index != proposal.oldModule) {
      ++numSkippedChangedModule;
      continue;
    }

    bool validMove = proposal.targetWasEmpty
        ? m_moduleMembers[proposal.oldModule] > 1 && m_moduleMembers[proposal.newModule] == 0
        : m_moduleMembers[proposal.newModule] > 0;
    if (!validMove) {
      ++numSkippedInvalidTarget;
      continue;
    }
    unsigned int oldModule = proposal.oldModule;
    unsigned int newModule = proposal.newModule;
    DeltaFlowDataType oldModuleDelta(oldModule, 0.0, 0.0);
    DeltaFlowDataType newModuleDelta(newModule, 0.0, 0.0);

    for (auto& e : current.outEdges()) {
      auto& edge = *e;
      unsigned int otherModule = edge.target->index;
      if (otherModule == oldModule) {
        oldModuleDelta.deltaExit += edge.data.flow;
      } else if (otherModule == newModule) {
        newModuleDelta.deltaExit += edge.data.flow;
      }
    }
    for (auto& e : current.inEdges()) {
      auto& edge = *e;
      unsigned int otherModule = edge.source->index;
      if (otherModule == oldModule) {
        oldModuleDelta.deltaEnter += edge.data.flow;
      } else if (otherModule == newModule) {
        newModuleDelta.deltaEnter += edge.data.flow;
      }
    }

    // For recorded teleportation
    m_objective.addTeleportationFlow(current, m_moduleFlowData, oldModuleDelta, newModuleDelta);

    double deltaCodelength = m_objective.getDeltaCodelengthOnMovingNode(current,
                                                                        oldModuleDelta,
                                                                        newModuleDelta,
                                                                        m_moduleFlowData,
                                                                        m_moduleMembers);
    if (deltaCodelength >= -m_infomap->minimumSingleNodeCodelengthImprovement) {
      ++numSkippedRejectedRecheck;
      continue;
    }

    m_objective.updateCodelengthOnMovingNode(current, oldModuleDelta, newModuleDelta, m_moduleFlowData, m_moduleMembers);

    m_moduleMembers[oldModule] -= 1;
    m_moduleMembers[newModule] += 1;
    current.index = newModule;

    selectedProposalIndices.push_back(nodeIndex);
  }

  unsigned int numMoved = selectedProposalIndices.size();
  Log(3) << "Inner-parallelization proposals: " << numProposals
         << ", accepted: " << numMoved
         << ", skipped after changed module: " << numSkippedChangedModule
         << ", invalid target: " << numSkippedInvalidTarget
         << ", rejected by recheck: " << numSkippedRejectedRecheck << "\n";

  for (auto& proposal : proposals) {
    if (proposal.clearDirty)
      network[proposal.nodeIndex]->dirty = false;
  }

  for (unsigned int nodeIndex : selectedProposalIndices) {
    InfoNode& current = *network[nodeIndex];
    for (auto& e : current.outEdges())
      e->target->dirty = true;
    for (auto& e : current.inEdges())
      e->source->dirty = true;
  }

  m_emptyModules.clear();
  for (unsigned int moduleIndex = 0; moduleIndex < m_moduleMembers.size(); ++moduleIndex) {
    if (m_moduleMembers[moduleIndex] == 0)
      m_emptyModules.push_back(moduleIndex);
  }

  return numMoved;
}

template <typename Objective>
inline void InfomapOptimizer<Objective>::consolidateModules(bool replaceExistingModules)
{
  auto& network = m_infomap->activeNetwork();
  auto numNodes = network.size();
  std::vector<InfoNode*> modules(numNodes, nullptr);

  InfoNode& firstActiveNode = *network[0];
  auto level = firstActiveNode.depth();
  auto leafLevel = m_infomap->numLevels();

  if (leafLevel == 1)
    replaceExistingModules = false;

  // Release children pointers on current parent(s) to put new modules between
  for (auto& n : network) {
    n->parent->releaseChildren(); // Safe to call multiple times
  }

  // Create the new module nodes and re-parent the active network from its common parent to the new module level
  for (unsigned int i = 0; i < numNodes; ++i) {
    InfoNode* node = network[i];
    unsigned int moduleIndex = node->index;
    if (modules[moduleIndex] == nullptr) {
      modules[moduleIndex] = new InfoNode(m_moduleFlowData[moduleIndex]);
      modules[moduleIndex]->index = moduleIndex;
      node->parent->addChild(modules[moduleIndex]);
    }
    modules[moduleIndex]->addChild(node);
  }

  using NodePair = std::pair<unsigned int, unsigned int>;
  using EdgeMap = std::map<NodePair, double>;
  EdgeMap moduleLinks;

  for (auto& node : network) {
    unsigned int module1 = node->index;
    for (auto& e : node->outEdges()) {
      InfoEdge& edge = *e;
      unsigned int module2 = edge.target->index;
      if (module1 != module2) {
        // Use new variables to not swap module1
        unsigned int m1 = module1, m2 = module2;
        // If undirected, the order may be swapped to aggregate the edge on an opposite one
        if (m_infomap->isUndirectedClustering() && m1 > m2)
          std::swap(m1, m2);
        auto ret = moduleLinks.insert(std::make_pair(NodePair(m1, m2), edge.data.flow));
        if (!ret.second) {
          ret.first->second += edge.data.flow;
        }
      }
    }
  }

  // Add the aggregated edge flow structure to the new modules
  for (auto& e : moduleLinks) {
    const auto& nodePair = e.first;
    modules[nodePair.first]->addOutEdge(*modules[nodePair.second], 0.0, e.second);
  }

  if (replaceExistingModules) {
    if (level == 1) {
      Log(4) << "Consolidated super modules, removing old modules...\n";
      for (auto& node : network)
        node->replaceWithChildren();
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
