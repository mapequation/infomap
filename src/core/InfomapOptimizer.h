/*
 */

#ifndef INFOMAP_OPTIMIZER_H_
#define INFOMAP_OPTIMIZER_H_

#include "InfomapOptimizerBase.h"
#include "InfomapBase.h"
#include <set>
#include "../utils/VectorMap.h"
#include "../utils/infomath.h"
#include "InfoNode.h"
#include "FlowData.h"
#include <utility>
// #include <tuple>

namespace infomap {

template <typename Objective>
class InfomapOptimizer : public InfomapOptimizerBase {
  using FlowDataType = FlowData;
  // using DeltaFlowDataType = MemDeltaFlow;
  // using FlowDataType = typename Objective::FlowDataType;
  using DeltaFlowDataType = typename Objective::DeltaFlowDataType;

protected:
  using EdgeType = Edge<InfoNode>;

public:
  InfomapOptimizer() = default;

  virtual ~InfomapOptimizer() = default;

  virtual void init(InfomapBase* infomap);

  // ===================================================
  // IO
  // ===================================================

  virtual std::ostream& toString(std::ostream& out) const;

  // ===================================================
  // Getters
  // ===================================================

  virtual double getCodelength() const;

  virtual double getIndexCodelength() const;

  virtual double getModuleCodelength() const;

  virtual double getMetaCodelength(bool unweighted = false) const;

protected:
  // virtual InfomapBase& getInfomap(InfoNode& node);
  // virtual InfomapBase* getNewInfomapInstance() const;
  // virtual InfomapBase* getNewInfomapInstanceWithoutMemory() const;

  // using Base::isTopLevel;
  // using Base::isMainInfomap;

  // using Base::isFirstLoop;

  virtual unsigned int numActiveModules() const;

  // ===================================================
  // Run: Init: *
  // ===================================================

  // Init terms that is constant for the whole network
  virtual void initTree();

  virtual void initNetwork();

  virtual void initSuperNetwork();

  virtual double calcCodelength(const InfoNode& parent) const;

  // ===================================================
  // Run: Partition: *
  // ===================================================

  virtual void initPartition();

  virtual void moveActiveNodesToPredefinedModules(std::vector<unsigned int>& modules);

  virtual unsigned int optimizeActiveNetwork();

  virtual unsigned int tryMoveEachNodeIntoBestModule();

  // virtual unsigned int tryMoveEachNodeIntoBestModuleLocal();

  virtual unsigned int tryMoveEachNodeIntoBestModuleInParallel();

  virtual void consolidateModules(bool replaceExistingModules = true);

  virtual bool restoreConsolidatedOptimizationPointIfNoImprovement(bool forceRestore = false);

  // ===================================================
  // Debug: *
  // ===================================================

  virtual void printDebug();

  // ===================================================
  // Protected members
  // ===================================================

  InfomapBase* m_infomap;

  Objective m_objective;
  Objective m_consolidatedObjective;
  std::vector<FlowDataType> m_moduleFlowData;
  std::vector<unsigned int> m_moduleMembers;
  std::vector<unsigned int> m_emptyModules;
};


template <typename Objective>
inline void InfomapOptimizer<Objective>::init(InfomapBase* infomap)
{
  m_infomap = infomap;
  // m_objective.init(infomap->getConfig());
  Config& conf = infomap->getConfig();
  m_objective.init(conf);
}

// ===================================================
// IO
// ===================================================

template <typename Objective>
inline std::ostream& InfomapOptimizer<Objective>::toString(std::ostream& out) const
{
  return out << m_objective;
}

// ===================================================
// Getters
// ===================================================

template <typename Objective>
inline double InfomapOptimizer<Objective>::getCodelength() const
{
  return m_objective.getCodelength();
}

template <typename Objective>
inline double InfomapOptimizer<Objective>::getIndexCodelength() const
{
  return m_objective.getIndexCodelength();
}

template <typename Objective>
inline double InfomapOptimizer<Objective>::getModuleCodelength() const
{
  return m_objective.getModuleCodelength();
}

template <>
inline double InfomapOptimizer<MetaMapEquation>::getMetaCodelength(bool unweighted) const
{
  return m_objective.getMetaCodelength(unweighted);
}

template <typename Objective>
inline double InfomapOptimizer<Objective>::getMetaCodelength(/* [[maybe_unused]] */ bool unweighted) const
{
  return 0.0;
}

template <typename Objective>
inline unsigned int InfomapOptimizer<Objective>::numActiveModules() const
{
  return m_infomap->activeNetwork().size() - m_emptyModules.size();
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
    m_objective.initSubNetwork(m_infomap->root());

  m_consolidatedObjective = m_objective;
}

template <typename Objective>
inline void InfomapOptimizer<Objective>::initSuperNetwork()
{
  Log(4) << "InfomapOptimizer::initSuperNetwork()...\n";
  m_objective.initSuperNetwork(m_infomap->root());
}

template <typename Objective>
inline double InfomapOptimizer<Objective>::calcCodelength(const InfoNode& parent) const
{
  return m_objective.calcCodelength(parent);
}


// ===================================================
// Run: Partition: *
// ===================================================

template <typename Objective>
void InfomapOptimizer<Objective>::initPartition()
{
  auto& network = m_infomap->activeNetwork();
  Log(4) << "InfomapOptimizer::initPartition() with " << network.size() << " nodes..." << std::endl;

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

  // Log(3) << "Initiated to codelength " << m_objective << " in " << numActiveModules() << " modules." << std::endl;
}


template <typename Objective>
void InfomapOptimizer<Objective>::moveActiveNodesToPredefinedModules(std::vector<unsigned int>& modules)
{
  auto& network = m_infomap->activeNetwork();
  auto numNodes = network.size();
  if (modules.size() != numNodes)
    throw std::length_error("Size of predefined modules differ from size of active network.");
  unsigned int numMoved = 0;

  for (unsigned int i = 0; i < numNodes; ++i) {
    InfoNode& current = *network[i];
    unsigned int oldM = current.index;
    unsigned int newM = modules[i];

    if (newM != oldM) {
      DeltaFlowDataType oldModuleDelta(oldM, 0.0, 0.0);
      DeltaFlowDataType newModuleDelta(newM, 0.0, 0.0);


      // For all outlinks
      for (auto& e : current.outEdges()) {
        auto& edge = *e;
        unsigned int otherModule = edge.target.index;
        if (otherModule == oldM)
          oldModuleDelta.deltaExit += edge.data.flow;
        else if (otherModule == newM)
          newModuleDelta.deltaExit += edge.data.flow;
      }
      // For all inlinks
      for (auto& e : current.inEdges()) {
        auto& edge = *e;
        unsigned int otherModule = edge.source.index;
        if (otherModule == oldM)
          oldModuleDelta.deltaEnter += edge.data.flow;
        else if (otherModule == newM)
          newModuleDelta.deltaEnter += edge.data.flow;
      }


      //Update empty module vector
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
      ++numMoved;
    }
  }
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

  // Log() << "\nOptimize, initial codelength: " << m_objective.getCodelength();

  do {
    ++coreLoopCount;
    unsigned int numNodesMoved = m_infomap->innerParallelization ? tryMoveEachNodeIntoBestModuleInParallel() :
                                                                 // tryMoveEachNodeIntoBestModuleLocal();
        tryMoveEachNodeIntoBestModule();
    // Break if not enough improvement
    // Log() << "\nLoop " << coreLoopCount << "/" << loopLimit << ", numNodesMoved: " <<
    // 	numNodesMoved << ", -> codelength: " << m_objective.getCodelength() << "\n";
    if (numNodesMoved == 0 || m_objective.getCodelength() >= oldCodelength - m_infomap->minimumCodelengthImprovement)
      break;
    ++numEffectiveLoops;
    oldCodelength = m_objective.getCodelength();
  } while (coreLoopCount != loopLimit);

  return numEffectiveLoops;
}

// template<typename Objective>
// unsigned int InfomapOptimizer<Objective>::tryMoveEachNodeIntoBestModuleLocal()
// {
// 	// Get random enumeration of nodes
// 	auto& network = m_infomap->activeNetwork();
// 	std::vector<unsigned int> nodeEnumeration(network.size());
// 	m_infomap->m_rand.getRandomizedIndexVector(nodeEnumeration);

// 	unsigned int numNodes = nodeEnumeration.size();
// 	unsigned int numMoved = 0;

// 	// Create map with module links
// 	// std::vector<DeltaFlowData> deltaFlow(numNodes);
// 	// VectorMap<DeltaFlowDataType> deltaFlow(numNodes);
// 	// Stopwatch timer(false);
// 	// double t = 0.0;
// 	// double tCount = 0;

// 	for (unsigned int i = 0; i < numNodes; ++i)
// 	{
// 		InfoNode& current = *network[nodeEnumeration[i]];

// //		Log(5) << "Trying to move node " << current << " from module " << current.index << "...\n";

// 		if (!current.dirty)
// 			continue;

// 		// If other nodes have moved here, don't move away on first loop
// 		if (m_moduleMembers[current.index] > 1 && m_infomap->isFirstLoop() && m_infomap->tuneIterationLimit != 1)
// 			continue;

// 		// If no links connecting this node with other nodes, it won't move into others,
// 		// and others won't move into this. TODO: Always best leave it alone?
// 		// For memory networks, don't skip try move to same physical node!
// 		// if (current.degree() == 0)
// 		// {
// 		// 	current.dirty = false;
// 		// 	continue;
// 		// }

// 		// Create map with module links
// 		std::map<unsigned int, DeltaFlowDataType> deltaFlow;
// 		// std::unordered_map<unsigned int, DeltaFlowDataType> deltaFlow;
// 		// deltaFlow.rehash(numNodes);
// 		// for (auto& d : deltaFlow) {
// 		// 	d.reset();
// 		// }
// 		// deltaFlow.startRound();

// 		// For all outlinks
// 		for (auto& e : current.outEdges())
// 		{
// 			auto& edge = *e;
// 			InfoNode& neighbour = edge.target;
// 			deltaFlow[neighbour.index] += DeltaFlowDataType(neighbour.index, edge.data.flow, 0.0);
// 			// deltaFlow.add(neighbour.index, DeltaFlowDataType(neighbour.index, edge.data.flow, 0.0));
// 		}
// 		// For all inlinks
// 		for (auto& e : current.inEdges())
// 		{
// 			auto& edge = *e;
// 			InfoNode& neighbour = edge.source;
// 			// timer.start();
// 			deltaFlow[neighbour.index] += DeltaFlowDataType(neighbour.index, 0.0, edge.data.flow);
// 			// deltaFlow.add(neighbour.index, DeltaFlowDataType(neighbour.index, 0.0, edge.data.flow));
// 			// t += timer.getElapsedTimeInMilliSec();
// 			// ++tCount;
// 		}

// 		// For not moving
// 		// deltaFlow.add(current.index, DeltaFlowDataType(current.index, 0.0, 0.0));
// 		DeltaFlowDataType& oldModuleDelta = deltaFlow[current.index];
// 		oldModuleDelta.module = current.index; // Make sure index is correct if created new
// 		// ++oldModuleDelta.count;
// 		oldModuleDelta += DeltaFlowDataType(current.index, 0.0, 0.0);

// 		// Option to move to empty module (if node not already alone)
// 		if (m_moduleMembers[current.index] > 1 && m_emptyModules.size() > 0) {
// 			deltaFlow[m_emptyModules.back()] += DeltaFlowDataType(m_emptyModules.back(), 0.0, 0.0);
// 			// deltaFlow.add(m_emptyModules.back(), DeltaFlowDataType(m_emptyModules.back(), 0.0, 0.0));
// 		}

// 		// For memory networks
// 		m_objective.addMemoryContributions(current, oldModuleDelta, deltaFlow);

// 		// Collect the move options to a vector
// 		std::vector<DeltaFlowDataType> moduleDeltaEnterExit(deltaFlow.size());
// 		unsigned int numModuleLinks = 0;
// 		for (auto& it : deltaFlow)
// 		{
// 			moduleDeltaEnterExit[numModuleLinks] = it.second;
// 			++numModuleLinks;
// 		}
// 		// auto& moduleDeltaEnterExit = deltaFlow.values();

// 		// Randomize link order for optimized search
// 		if (numModuleLinks > 2) {
// 			for (unsigned int j = 0; j < numModuleLinks - 2; ++j)
// 			{
// 				unsigned int randPos = m_infomap->m_rand.randInt(j+1, numModuleLinks - 1);
// 				swap(moduleDeltaEnterExit[j], moduleDeltaEnterExit[randPos]);
// 			}
// 		}
// 		// t += timer.getElapsedTimeInMilliSec();
// 		// ++tCount;

// 		DeltaFlowDataType bestDeltaModule(oldModuleDelta);
// 		double bestDeltaCodelength = 0.0;
// 		DeltaFlowDataType strongestConnectedModule(oldModuleDelta);
// 		double deltaCodelengthOnStrongestConnectedModule = 0.0;

// 		// Log(5) << "Move node " << current << " in module " << current.index << "...\n";
// 		// Find the move that minimizes the description length
// 		for (unsigned int j = 0; j < numModuleLinks; ++j)
// 		{
// 			// if (moduleDeltaEnterExit[j].count == 0) {
// 			// 	continue;
// 			// }
// 			unsigned int otherModule = moduleDeltaEnterExit[j].module;
// 			if(otherModule != current.index)
// 			{
// 				double deltaCodelength = m_objective.getDeltaCodelengthOnMovingNode(current,
// 						oldModuleDelta, moduleDeltaEnterExit[j], m_moduleFlowData, m_moduleMembers);

// 				// Log(5) << " Move to module " << otherModule << " -> deltaCodelength: " << deltaCodelength <<
// 				// 		", deltaEnter: " << moduleDeltaEnterExit[j].deltaEnter << ", deltaExit: " << moduleDeltaEnterExit[j].deltaExit <<
// 				// 		", module enter/exit: " << m_moduleFlowData[otherModule].enterFlow << " / " << m_moduleFlowData[otherModule].exitFlow <<
// 				// 		", OLD delta enter/exit: " << oldModuleDelta.deltaEnter << " / " << oldModuleDelta.deltaExit << "\n";
// 				if (deltaCodelength < bestDeltaCodelength - m_infomap->minimumSingleNodeCodelengthImprovement)
// 				{
// 					bestDeltaModule = moduleDeltaEnterExit[j];
// 					bestDeltaCodelength = deltaCodelength;
// 				}

// 				// Save strongest connected module to prefer if codelength improvement equal
// 				if (moduleDeltaEnterExit[j].deltaExit > strongestConnectedModule.deltaExit)
// 				{
// 					strongestConnectedModule = moduleDeltaEnterExit[j];
// 					deltaCodelengthOnStrongestConnectedModule = deltaCodelength;
// 				}
// 			}
// 		}

// 		// Prefer strongest connected module if equal delta codelength
// 		if (strongestConnectedModule.module != bestDeltaModule.module &&
// 				deltaCodelengthOnStrongestConnectedModule <= bestDeltaCodelength + m_infomap->minimumSingleNodeCodelengthImprovement)
// 		{
// 			bestDeltaModule = strongestConnectedModule;
// 		}

// 		// Make best possible move
// 		if(bestDeltaModule.module != current.index)
// 		{
// 			unsigned int bestModuleIndex = bestDeltaModule.module;
// 			//Update empty module vector
// 			if(m_moduleMembers[bestModuleIndex] == 0)
// 			{
// 				m_emptyModules.pop_back();
// 			}
// 			if(m_moduleMembers[current.index] == 1)
// 			{
// 				m_emptyModules.push_back(current.index);
// 			}

// 			// timer.start();
// 			m_objective.updateCodelengthOnMovingNode(current, oldModuleDelta, bestDeltaModule, m_moduleFlowData, m_moduleMembers);
// 			// t += timer.getElapsedTimeInMilliSec();
// 			// ++tCount;

// 			m_moduleMembers[current.index] -= 1;
// 			m_moduleMembers[bestModuleIndex] += 1;

// 			// double oldCodelength = m_objective.getCodelength();
// 			// Log(5) << " --> Moved to module " << bestModuleIndex << " -> codelength: " << oldCodelength << " + " <<
// 			// 		bestDeltaCodelength << " (" << (m_objective.getCodelength() - oldCodelength) << ") = " << m_objective << "\n";

// //			unsigned int oldModuleIndex = current.index;
// 			current.index = bestModuleIndex;

// 			++numMoved;

// 			// Mark neighbours as dirty
// 			for (auto& e : current.outEdges())
// 				e->target.dirty = true;
// 			for (auto& e : current.inEdges())
// 				e->source.dirty = true;
// 		}
// 		else
// 			current.dirty = false;

// //		if (!current.dirty)
// //			Log(5) << " --> Didn't move!\n";

// 	}

// 	// Log() << " !!! " << t << "/" << tCount << " = " << t / tCount << " !!! ";

// 	return numMoved;
// }

template <typename Objective>
unsigned int InfomapOptimizer<Objective>::tryMoveEachNodeIntoBestModule()
{
  // m_rand.seed(123);
  // Get random enumeration of nodes
  auto& network = m_infomap->activeNetwork();
  std::vector<unsigned int> nodeEnumeration(network.size());
  m_infomap->m_rand.getRandomizedIndexVector(nodeEnumeration);

  auto numNodes = nodeEnumeration.size();
  unsigned int numMoved = 0;

  // Create map with module links
  // std::vector<DeltaFlowData> deltaFlow(numNodes);
  VectorMap<DeltaFlowDataType> deltaFlow(numNodes);
  // SimpleMap<DeltaFlowDataType> deltaFlow(numNodes);
  // SimpleMap<DeltaFlowDataType> deltaFlow2(numNodes);
  // Stopwatch timer(false);
  // double t = 0.0;
  // double tCount = 0;

  for (unsigned int i = 0; i < numNodes; ++i) {
    InfoNode& current = *network[nodeEnumeration[i]];

    //		Log(5) << "Trying to move node " << current << " from module " << current.index << "...\n";

    if (!current.dirty)
      continue;

    // If other nodes have moved here, don't move away on first loop
    if (m_moduleMembers[current.index] > 1 && m_infomap->isFirstLoop() && m_infomap->tuneIterationLimit != 1)
      continue;

    // If no links connecting this node with other nodes, it won't move into others,
    // and others won't move into this. TODO: Always best leave it alone?
    // For memory networks, don't skip try move to same physical node!
    // if (current.degree() == 0)
    // {
    // 	current.dirty = false;
    // 	continue;
    // }

    // Create map with module links
    // std::unordered_map<unsigned int, DeltaFlowDataType> deltaFlow;
    // deltaFlow.rehash(numNodes);
    // for (auto& d : deltaFlow) {
    // 	d.reset();
    // }
    deltaFlow.startRound();

    // For all outlinks
    for (auto& e : current.outEdges()) {
      auto& edge = *e;
      InfoNode& neighbour = edge.target;
      // deltaFlow[neighbour.index] += DeltaFlowDataType(neighbour.index, edge.data.flow, 0.0);
      deltaFlow.add(neighbour.index, DeltaFlowDataType(neighbour.index, edge.data.flow, 0.0));
    }
    // For all inlinks
    for (auto& e : current.inEdges()) {
      auto& edge = *e;
      InfoNode& neighbour = edge.source;
      // timer.start();
      // deltaFlow[neighbour.index] += DeltaFlowDataType(neighbour.index, 0.0, edge.data.flow);
      deltaFlow.add(neighbour.index, DeltaFlowDataType(neighbour.index, 0.0, edge.data.flow));
      // t += timer.getElapsedTimeInMilliSec();
      // ++tCount;
    }

    // For not moving
    deltaFlow.add(current.index, DeltaFlowDataType(current.index, 0.0, 0.0));
    DeltaFlowDataType oldModuleDelta = deltaFlow[current.index];
    oldModuleDelta.module = current.index; // Make sure index is correct if created new
    // ++oldModuleDelta.count;
    // oldModuleDelta += DeltaFlowDataType(current.index, 0.0, 0.0);

    // Option to move to empty module (if node not already alone)
    if (m_moduleMembers[current.index] > 1 && m_emptyModules.size() > 0) {
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

    // Log(5) << "Move node " << current << " in module " << current.index << "...\n";
    // Find the move that minimizes the description length
    for (unsigned int j = 0; j < numModuleLinks; ++j) {
      // if (moduleDeltaEnterExit[j].count == 0) {
      // 	continue;
      // }
      unsigned int otherModule = moduleDeltaEnterExit[j].module;
      if (otherModule != current.index) {
        double deltaCodelength = m_objective.getDeltaCodelengthOnMovingNode(current,
                                                                            oldModuleDelta,
                                                                            moduleDeltaEnterExit[j],
                                                                            m_moduleFlowData,
                                                                            m_moduleMembers);

        // Log(5) << " Move to module " << otherModule << " -> deltaCodelength: " << deltaCodelength <<
        // 		", deltaEnter: " << moduleDeltaEnterExit[j].deltaEnter << ", deltaExit: " << moduleDeltaEnterExit[j].deltaExit <<
        // 		", module enter/exit: " << m_moduleFlowData[otherModule].enterFlow << " / " << m_moduleFlowData[otherModule].exitFlow <<
        // 		", OLD delta enter/exit: " << oldModuleDelta.deltaEnter << " / " << oldModuleDelta.deltaExit << "\n";
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
      //Update empty module vector
      if (m_moduleMembers[bestModuleIndex] == 0) {
        m_emptyModules.pop_back();
      }
      if (m_moduleMembers[current.index] == 1) {
        m_emptyModules.push_back(current.index);
      }

      // timer.start();
      m_objective.updateCodelengthOnMovingNode(current, oldModuleDelta, bestDeltaModule, m_moduleFlowData, m_moduleMembers);
      // t += timer.getElapsedTimeInMilliSec();
      // ++tCount;

      m_moduleMembers[current.index] -= 1;
      m_moduleMembers[bestModuleIndex] += 1;

      // double oldCodelength = m_objective.getCodelength();
      // Log(5) << " --> Moved to module " << bestModuleIndex << " -> codelength: " << oldCodelength << " + " <<
      // 		bestDeltaCodelength << " (" << (m_objective.getCodelength() - oldCodelength) << ") = " << m_objective << "\n";

      //			unsigned int oldModuleIndex = current.index;
      current.index = bestModuleIndex;

      ++numMoved;

      // Mark neighbours as dirty
      for (auto& e : current.outEdges())
        e->target.dirty = true;
      for (auto& e : current.inEdges())
        e->source.dirty = true;
    } else
      current.dirty = false;

    //		if (!current.dirty)
    //			Log(5) << " --> Didn't move!\n";
  }

  // Log() << " !!! " << t << "/" << tCount << " = " << t / tCount << " !!! ";

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
  double diffSerialParallelCodelength = 0.0;

  // Create map with module links
  // std::vector<DeltaFlowData> deltaFlow(numNodes);
  // VectorMap<DeltaFlowData> deltaFlow(numNodes);
  // Stopwatch timer(false);
  // double t = 0.0;
  // double tCount = 0;

#pragma omp parallel for schedule(dynamic) // Use dynamic scheduling as some threads could end early
  for (unsigned int i = 0; i < numNodes; ++i) {
    //		printf("Node %d processed by thread %d\n", i, omp_get_thread_num());
    // Pick nodes in random order
    InfoNode& current = *network[nodeEnumeration[i]];

    //		Log(5) << "Trying to move node " << current << " from module " << current.index << "...\n";

    if (!current.dirty)
      continue;

    // If other nodes have moved here, don't move away on first loop
    if (m_moduleMembers[current.index] > 1 && m_infomap->isFirstLoop() && m_infomap->tuneIterationLimit != 1)
      continue;

    // If no links connecting this node with other nodes, it won't move into others,
    // and others won't move into this. TODO: Always best leave it alone?
    // For memory networks, don't skip try move to same physical node!
    // if (current.degree() == 0)
    // {
    // 	current.dirty = false;
    // 	continue;
    // }

    // Create map with module links
    // std::unordered_map<unsigned int, DeltaFlowDataType> deltaFlow;
    // deltaFlow.rehash(numNodes);
    VectorMap<DeltaFlowDataType> deltaFlow(numNodes);

    // For all outlinks
    for (auto& e : current.outEdges()) {
      auto& edge = *e;
      InfoNode& neighbour = edge.target;
      // deltaFlow[neighbour.index] += DeltaFlowDataType(neighbour.index, edge.data.flow, 0.0);
      deltaFlow.add(neighbour.index, DeltaFlowDataType(neighbour.index, edge.data.flow, 0.0));
    }
    // For all inlinks
    for (auto& e : current.inEdges()) {
      auto& edge = *e;
      InfoNode& neighbour = edge.source;
      // timer.start();
      // deltaFlow[neighbour.index] += DeltaFlowDataType(neighbour.index, 0.0, edge.data.flow);
      deltaFlow.add(neighbour.index, DeltaFlowDataType(neighbour.index, 0.0, edge.data.flow));
      // t += timer.getElapsedTimeInMilliSec();
      // ++tCount;
    }

    // For not moving
    deltaFlow.add(current.index, DeltaFlowDataType(current.index, 0.0, 0.0));
    DeltaFlowDataType& oldModuleDelta = deltaFlow[current.index];
    oldModuleDelta.module = current.index; // Make sure index is correct if created new
    // ++oldModuleDelta.count;
    // oldModuleDelta += DeltaFlowDataType(current.index, 0.0, 0.0);

    // Option to move to empty module (if node not already alone)
    if (m_moduleMembers[current.index] > 1 && m_emptyModules.size() > 0) {
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

    // Log(5) << "Move node " << current << " in module " << current.index << "...\n";
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
    // 		if(bestDeltaModule.module != current.index)
    // 		{
    // 			unsigned int bestModuleIndex = bestDeltaModule.module;
    // 			//Update empty module vector
    // 			if(m_moduleMembers[bestModuleIndex] == 0)
    // 			{
    // 				m_emptyModules.pop_back();
    // 			}
    // 			if(m_moduleMembers[current.index] == 1)
    // 			{
    // 				m_emptyModules.push_back(current.index);
    // 			}

    // 			m_objective.updateCodelengthOnMovingNode(current, oldModuleDelta, bestDeltaModule, m_moduleFlowData, m_moduleMembers);

    // 			m_moduleMembers[current.index] -= 1;
    // 			m_moduleMembers[bestModuleIndex] += 1;

    // 			// double oldCodelength = m_objective.getCodelength();
    // 			// Log(5) << " --> Moved to module " << bestModuleIndex << " -> codelength: " << oldCodelength << " + " <<
    // 			// 		bestDeltaCodelength << " (" << (m_objective.getCodelength() - oldCodelength) << ") = " << m_objective << "\n";

    // //			unsigned int oldModuleIndex = current.index;
    // 			current.index = bestModuleIndex;

    // 			++numMoved;

    // 			// Mark neighbours as dirty
    // 			for (auto& e : current.outEdges())
    // 				e->target.dirty = true;
    // 			for (auto& e : current.inEdges())
    // 				e->source.dirty = true;
    // 		}
    // 		else
    // 			current.dirty = false;

    // Make best possible move
    if (bestDeltaModule.module == current.index) {
      current.dirty = false;
      continue;
    } else {
#pragma omp critical(moveUpdate)
      {
        unsigned int bestModuleIndex = bestDeltaModule.module;
        unsigned int oldModuleIndex = current.index;

        bool validMove = true;
        if (bestModuleIndex == m_emptyModules.back()) {
          // Check validity of move to empty target
          validMove = m_moduleMembers[oldModuleIndex] > 1 && m_emptyModules.size() > 0;
        } else {
          // Not valid if the best module is empty now but not when decided
          validMove = m_moduleMembers[bestModuleIndex] > 0;
        }

        if (validMove) {
          // Recalculate delta codelength for proposed move to see if still an improvement

          oldModuleDelta = DeltaFlowDataType(oldModuleIndex, 0.0, 0.0);
          DeltaFlowDataType newModuleDelta(bestModuleIndex, 0.0, 0.0);


          // For all outlinks
          for (auto& e : current.outEdges()) {
            auto& edge = *e;
            unsigned int otherModule = edge.target.index;
            if (otherModule == oldModuleIndex)
              oldModuleDelta.deltaExit += edge.data.flow;
            else if (otherModule == bestModuleIndex)
              newModuleDelta.deltaExit += edge.data.flow;
          }
          // For all inlinks
          for (auto& e : current.inEdges()) {
            auto& edge = *e;
            unsigned int otherModule = edge.source.index;
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
            //Update empty module vector
            if (m_moduleMembers[bestModuleIndex] == 0) {
              m_emptyModules.pop_back();
            }
            if (m_moduleMembers[oldModuleIndex] == 1) {
              m_emptyModules.push_back(oldModuleIndex);
            }

            m_objective.updateCodelengthOnMovingNode(current, oldModuleDelta, bestDeltaModule, m_moduleFlowData, m_moduleMembers);

            m_moduleMembers[oldModuleIndex] -= 1;
            m_moduleMembers[bestModuleIndex] += 1;

            // double oldCodelength = m_objective.getCodelength();
            // Log(5) << " --> Moved to module " << bestModuleIndex << " -> codelength: " << oldCodelength << " + " <<
            // 		bestDeltaCodelength << " (" << (m_objective.getCodelength() - oldCodelength) << ") = " << m_objective << "\n";

            //			unsigned int oldModuleIndex = current.index;
            current.index = bestModuleIndex;
            diffSerialParallelCodelength += bestDeltaCodelength - deltaCodelength;

            ++numMoved;

            // Mark neighbours as dirty
            for (auto& e : current.outEdges())
              e->target.dirty = true;
            for (auto& e : current.inEdges())
              e->source.dirty = true;
          } else {
            ++numInvalidMoves;
          }
        } else {
          ++numInvalidMoves;
        }
      }
    }

    //		if (!current.dirty)
    //			Log(5) << " --> Didn't move!\n";
  }

  // Log() << " !!! " << t << "/" << tCount << " = " << t / tCount << " !!! ";

  // Log() << "\n(#invalidMoves: " << numInvalidMoves <<
  // 		", diffSerialParallelCodelength: " << diffSerialParallelCodelength << ") ";

  //	return numMoved;
  return numMoved + numInvalidMoves;
}


template <typename Objective>
inline void InfomapOptimizer<Objective>::consolidateModules(bool replaceExistingModules)
{
  //	Log(1) << "Consolidate modules to codelength " << m_optimizer << "..." << std::endl;

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


  // Aggregate links from lower level to the new modular level
  // struct CompareNodePairDeterministically {
  // 	bool operator() (const std::pair<InfoNode*, InfoNode*>& x, const std::pair<InfoNode*, InfoNode*>& y) const {
  // 		return x.first->index < y.first->index || (!(y.first->index < x.first->index) && x.second->index < y.second->index);
  // 	}
  // };
  // using NodePair = std::pair<InfoNode*, InfoNode*>;
  // using EdgeMap = std::map<NodePair, double, CompareNodePairDeterministically>;
  // EdgeMap moduleLinks;

  // for (auto& node : network)
  // {
  // 	InfoNode* module1 = node->parent;
  // 	for (auto& e : node->outEdges())
  // 	{
  // 		EdgeType& edge = *e;
  // 		InfoNode* module2 = edge.target.parent;
  // 		if (module1 != module2)
  // 		{
  // 			// Use new pointers to not swap module1
  // 			InfoNode* m1 = module1;
  // 			InfoNode* m2 = module2;
  // 			if (!m_infomap->directedEdges && m1->index > m2->index)
  // 				std::swap(m1, m2);
  // 			// Insert the node pair in the edge map. If not inserted, add the flow value to existing node pair.
  // 			auto ret = moduleLinks.emplace(
  // 				std::piecewise_construct,
  // 				std::forward_as_tuple(m1, m2),
  // 				std::forward_as_tuple(edge.data.flow)
  // 			);
  // 			if (!ret.second) {
  // 				ret.first->second += edge.data.flow;
  // 			}
  // 		}
  // 	}
  // }

  using NodePair = std::pair<unsigned int, unsigned int>;
  using EdgeMap = std::map<NodePair, double>;
  EdgeMap moduleLinks;

  for (auto& node : network) {
    unsigned int module1 = node->index;
    for (auto& e : node->outEdges()) {
      EdgeType& edge = *e;
      unsigned int module2 = edge.target.index;
      if (module1 != module2) {
        // Use new variables to not swap module1
        unsigned int m1 = module1, m2 = module2;
        // If undirected, the order may be swapped to aggregate the edge on an opposite one
        if (m_infomap->isUndirectedClustering() && m1 > m2)
          std::swap(m1, m2);
        // Insert the node pair in the edge map. If not inserted, add the flow value to existing node pair.
        // auto ret = moduleLinks.emplace(
        // 	std::piecewise_construct,
        // 	std::forward_as_tuple(m1, m2),
        // 	std::forward_as_tuple(edge.data.flow)
        // );
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
    // nodePair.first->addOutEdge(*nodePair.second, 0.0, e.second);
    modules[nodePair.first]->addOutEdge(*modules[nodePair.second], 0.0, e.second);
  }

  if (replaceExistingModules) {
    if (level == 1) {
      Log(4) << "Consolidated super modules, removing old modules..." << std::endl;
      for (auto& node : network)
        node->replaceWithChildren();
    } else if (level == 2) {
      Log(4) << "Consolidated sub-modules, removing modules..." << std::endl;
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


// ===================================================
// Debug: *
// ===================================================

template <typename Objective>
inline void InfomapOptimizer<Objective>::printDebug()
{
  m_objective.printDebug();
}


} /* namespace infomap */

#endif /* INFOMAP_OPTIMIZER_H_ */
