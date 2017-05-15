/*
 * Infomap.h
 *
 *  Created on: 4 mar 2015
 *      Author: Daniel
 */

#ifndef INFOMAP_OPTIMIZER_H_
#define INFOMAP_OPTIMIZER_H_

#include "InfomapBase.h"
#include <set>
#include <unordered_map>
// #include "../utils/ReusableVector.h"

namespace infomap {

template<typename Node, typename Objective>
class InfomapOptimizer : public InfomapBase<Node> {
	using Base = InfomapBase<Node>;
	template<typename InfoNode>
	using ThisInfomapType = InfomapOptimizer<InfoNode, Objective>;
	using FlowData = typename Objective::FlowDataType;
	using DeltaFlowData = typename Objective::DeltaFlowDataType;

protected:
//	using Base::EdgeType;
//	using EdgeType = Base::EdgeType;
	using EdgeType = Edge<InfoNodeBase>;
public:
	InfomapOptimizer() : InfomapBase<Node>() {}
	virtual ~InfomapOptimizer() {}

	// ===================================================
	// IO
	// ===================================================

	virtual std::ostream& toString(std::ostream& out) const;

	// ===================================================
	// Getters
	// ===================================================

	using Base::root;
	using Base::numLeafNodes;
	using Base::numTopModules;
	using Base::m_numNonTrivialTopModules;
	using Base::numLevels;

	virtual double getCodelength() const;

	virtual double getIndexCodelength() const;

	virtual double getModuleCodelength() const;

protected:
	using Base::get;

	virtual InfomapBase<Node>& getInfomap(InfoNodeBase& node);

	using Base::isMainInfomap;

	using Base::isFirstLoop;

	virtual unsigned int numActiveModules() const;

	using Base::activeNetwork;

	auto getModuleFlowData() -> std::vector<FlowData>& { return m_moduleFlowData; }

	// ===================================================
	// Run: Init: *
	// ===================================================

	// Init terms that is constant for the whole network
	virtual void initNetwork();

	virtual void initSuperNetwork();

	virtual double calcCodelength(const InfoNodeBase& parent) const;

	// ===================================================
	// Run: Partition: *
	// ===================================================

	virtual void initPartition();

	virtual void moveActiveNodesToPredifinedModules(std::vector<unsigned int>& modules);

	virtual unsigned int optimizeActiveNetwork();
	
	unsigned int tryMoveEachNodeIntoBestModule();

	virtual void consolidateModules(bool replaceExistingModules = true);

	virtual bool restoreConsolidatedOptimizationPointIfNoImprovement(bool forceRestore = false);

	// ===================================================
	// Debug: *
	// ===================================================

	virtual void printDebug();

	// ===================================================
	// Protected members
	// ===================================================

	using Base::m_oneLevelCodelength;
	using Base::m_rand;
	using Base::m_aggregationLevel;
	using Base::m_isCoarseTune;

	Objective m_objective;
	Objective m_consolidatedObjective;
	std::vector<FlowData> m_moduleFlowData;
	std::vector<unsigned int> m_moduleMembers;
	std::vector<unsigned int> m_emptyModules;
};


// ===================================================
// Getters
// ===================================================

template<typename Node, typename Objective>
inline
InfomapBase<Node>& InfomapOptimizer<Node, Objective>::getInfomap(InfoNodeBase& node) {
	// return get(node).template getInfomap<ThisInfomapType>();
	return get(node).getInfomap();
}


template<typename Node, typename Objective>
inline
double InfomapOptimizer<Node, Objective>::getCodelength() const
{
	return m_objective.codelength;
}

template<typename Node, typename Objective>
inline
double InfomapOptimizer<Node, Objective>::getIndexCodelength() const
{
	return m_objective.indexCodelength;
}

template<typename Node, typename Objective>
inline
double InfomapOptimizer<Node, Objective>::getModuleCodelength() const
{
	return m_objective.moduleCodelength;
}

template<typename Node, typename Objective>
inline
unsigned int InfomapOptimizer<Node, Objective>::numActiveModules() const
{
	return activeNetwork().size() - m_emptyModules.size();
}


// ===================================================
// Run: Init: *
// ===================================================

template<typename Node, typename Objective>
inline
void InfomapOptimizer<Node, Objective>::initNetwork()
{
	Log(4) << "InfomapOptimizer::initNetwork()...\n";
	m_objective.initNetwork(root());

	if (!isMainInfomap())
		m_objective.initSubNetwork(root());
}

template<typename Node, typename Objective>
inline
void InfomapOptimizer<Node, Objective>::initSuperNetwork()
{
	Log(4) << "InfomapOptimizer::initSuperNetwork()...\n";
	m_objective.initSuperNetwork(root());
}

template<typename Node, typename Objective>
inline
double InfomapOptimizer<Node, Objective>::calcCodelength(const InfoNodeBase& parent) const
{
	return m_objective.calcCodelength(parent);
}


// ===================================================
// Run: Partition: *
// ===================================================

template<typename Node, typename Objective>
void InfomapOptimizer<Node, Objective>::initPartition()
{
	auto& network = activeNetwork();
	Log(4) << "InfomapOptimizer::initPartition() with " << network.size() << " nodes..." << std::endl;

	// Init one module for each node
	unsigned int numNodes = network.size();
	m_moduleFlowData.resize(numNodes);
	m_moduleMembers.assign(numNodes, 1);
	m_emptyModules.clear();
	m_emptyModules.reserve(numNodes);

	unsigned int i = 0;
	for (auto& nodePtr : network)
	{
		InfoNodeBase& node = *nodePtr;
		node.index = i; // Unique module index for each node
		m_moduleFlowData[i] = node.data;
		node.dirty = true;
		++i;
	}

	m_objective.initPartition(network);

	Log(2) << "Initiated to codelength " << *this << " in " << numActiveModules() << " modules." << std::endl;
}


template<typename Node, typename Objective>
void InfomapOptimizer<Node, Objective>::moveActiveNodesToPredifinedModules(std::vector<unsigned int>& modules)
{
	auto& network = activeNetwork();
	unsigned int numNodes = network.size();
	if (modules.size() != numNodes)
		throw std::length_error("Size of predefined modules differ from size of active network.");
	unsigned int numMoved = 0;

	for (unsigned int i = 0; i < numNodes; ++i)
	{
		InfoNodeBase& current = *network[i];
		unsigned int oldM = current.index;
		unsigned int newM = modules[i];

		if (newM != oldM)
		{
			DeltaFlowData oldModuleDelta(oldM, 0.0, 0.0);
			DeltaFlowData newModuleDelta(newM, 0.0, 0.0);


			// For all outlinks
			for (auto& e : current.outEdges())
			{
				auto& edge = *e;
				unsigned int otherModule = edge.target.index;
				if (otherModule == oldM)
					oldModuleDelta.deltaExit += edge.data.flow;
				else if (otherModule == newM)
					newModuleDelta.deltaExit += edge.data.flow;
			}
			// For all inlinks
			for (auto& e : current.inEdges())
			{
				auto& edge = *e;
				unsigned int otherModule = edge.source.index;
				if (otherModule == oldM)
					oldModuleDelta.deltaEnter += edge.data.flow;
				else if (otherModule == newM)
					newModuleDelta.deltaEnter += edge.data.flow;
			}


			//Update empty module vector
			if(m_moduleMembers[newM] == 0)
			{
				m_emptyModules.pop_back();
			}
			if(m_moduleMembers[current.index] == 1)
			{
				m_emptyModules.push_back(oldM);
			}

			m_objective.updateCodelengthOnMovingNode(current, oldModuleDelta, newModuleDelta, m_moduleFlowData);

			m_moduleMembers[oldM] -= 1;
			m_moduleMembers[newM] += 1;

			current.index = newM;
			++numMoved;
		}
	}
}

template<typename Node, typename Objective>
inline
unsigned int InfomapOptimizer<Node, Objective>::optimizeActiveNetwork()
{
	unsigned int coreLoopCount = 0;
	unsigned int numEffectiveLoops = 0;
	double oldCodelength = m_objective.codelength;
	unsigned int loopLimit = this->coreLoopLimit;
	unsigned int minRandLoop = 2;
	if (loopLimit >= minRandLoop && this->randomizeCoreLoopLimit)
		loopLimit = infomath::randInt(m_rand, minRandLoop, loopLimit);
		// loopLimit = static_cast<unsigned int>(m_rand() * (loopLimit - minRandLoop)) + minRandLoop;
	if (m_aggregationLevel > 0 || m_isCoarseTune) {
		loopLimit = 20;
	}

	Stopwatch timer(true), timerAll(true);
	unsigned int numMovedTotal = 0;

	while (coreLoopCount != loopLimit)
	{
		timer.start();
		++coreLoopCount;
		unsigned int numNodesMoved = tryMoveEachNodeIntoBestModule();
		numMovedTotal += numNodesMoved;
		Log() << " [[" << numNodesMoved << ": " << timer.getElapsedTimeInMilliSec() << "]] ";
		// Break if not enough improvement
		if (numNodesMoved == 0 || m_objective.codelength >= oldCodelength - this->minimumCodelengthImprovement)
			break;
		++numEffectiveLoops;
		oldCodelength = m_objective.codelength;
	}
	double t = timerAll.getElapsedTimeInMilliSec();
	Log() << " >>> TIME_PER_MOVE: " << t * 1.0 / numMovedTotal <<
		", TIME_PER_LOOP: " << t / coreLoopCount << " <<< ";

	return numEffectiveLoops;
}

template<typename Node, typename Objective>
unsigned int InfomapOptimizer<Node, Objective>::tryMoveEachNodeIntoBestModule()
{
	// Get random enumeration of nodes
	std::vector<unsigned int> nodeEnumeration(activeNetwork().size());
	infomath::getRandomizedIndexVector(nodeEnumeration, m_rand);

	auto& network = activeNetwork();
	unsigned int numNodes = nodeEnumeration.size();
	unsigned int numMoved = 0;

	// Create map with module links
	std::vector<DeltaFlowData> deltaFlow(numNodes);
	Stopwatch timer(false);
	double t = 0.0;
	double tCount = 0;

	for (unsigned int i = 0; i < numNodes; ++i)
	{
		InfoNodeBase& current = *network[nodeEnumeration[i]];

//		Log(5) << "Trying to move node " << current << " from module " << current.index << "...\n";

		if (!current.dirty)
			continue;

		// If other nodes have moved here, don't move away on first loop
		if (m_moduleMembers[current.index] > 1 && isFirstLoop())
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
		// std::unordered_map<unsigned int, DeltaFlowData> deltaFlow;
		// deltaFlow.rehash(numNodes);
		for (auto& d : deltaFlow) {
			d.reset();
		}

		// For all outlinks
		for (auto& e : current.outEdges())
		{
			auto& edge = *e;
			InfoNodeBase& neighbour = edge.target;
			deltaFlow[neighbour.index] += DeltaFlowData(neighbour.index, edge.data.flow, 0.0);
		}
		// For all inlinks
		for (auto& e : current.inEdges())
		{
			auto& edge = *e;
			InfoNodeBase& neighbour = edge.source;
			timer.start();
			deltaFlow[neighbour.index] += DeltaFlowData(neighbour.index, 0.0, edge.data.flow);
			t += timer.getElapsedTimeInMilliSec();
			++tCount;
		}

		// For not moving
		DeltaFlowData& oldModuleDelta = deltaFlow[current.index];
		oldModuleDelta.module = current.index; // Make sure index is correct if created new
		// ++oldModuleDelta.count;
		oldModuleDelta += DeltaFlowData(current.index, 0.0, 0.0);

		// Option to move to empty module (if node not already alone)
		if (m_moduleMembers[current.index] > 1 && m_emptyModules.size() > 0) {
			deltaFlow[m_emptyModules.back()] += DeltaFlowData(m_emptyModules.back(), 0.0, 0.0);
		}

		// For memory networks
		m_objective.addMemoryContributions(current, oldModuleDelta, deltaFlow);

		// // Collect the move options to a vector
		// std::vector<DeltaFlowData> moduleDeltaEnterExit(deltaFlow.size());
		// unsigned int numModuleLinks = 0;
		// for (auto& it : deltaFlow)
		// {
		// 	moduleDeltaEnterExit[numModuleLinks] = it.second;
		// 	++numModuleLinks;
		// }
		// // timer.start();
		// // Randomize link order for optimized search
		// infomath::uniform_uint_dist uniform;
		// for (unsigned int j = 0; j < numModuleLinks - 1; ++j)
		// {
		// 	unsigned int randPos = j + uniform(m_rand, infomath::uniform_param_t(0, numModuleLinks - j - 1));
		// 	swap(moduleDeltaEnterExit[j], moduleDeltaEnterExit[randPos]);
		// }
		// t += timer.getElapsedTimeInMilliSec();
		// ++tCount;
		auto& moduleDeltaEnterExit = deltaFlow;
		
		// Randomize link order for optimized search
		// infomath::uniform_uint_dist uniform;
		// for (unsigned int j = 0; j < numNodes - 1; ++j)
		// {
		// 	unsigned int randPos = j + uniform(m_rand, infomath::uniform_param_t(0, numNodes - j - 1));
		// 	swap(moduleDeltaEnterExit[j], moduleDeltaEnterExit[randPos]);
		// }

		DeltaFlowData bestDeltaModule(oldModuleDelta);
		double bestDeltaCodelength = 0.0;
		DeltaFlowData strongestConnectedModule(oldModuleDelta);
		double deltaCodelengthOnStrongestConnectedModule = 0.0;

		// Log(5) << "Move node " << current << " in module " << current.index << "...\n";
		// Find the move that minimizes the description length
		for (unsigned int j = 0; j < numNodes; ++j)
		{
			if (moduleDeltaEnterExit[j].count == 0) {
				continue;
			}
			unsigned int otherModule = moduleDeltaEnterExit[j].module;
			if(otherModule != current.index)
			{
				double deltaCodelength = m_objective.getDeltaCodelengthOnMovingNode(current,
						oldModuleDelta, moduleDeltaEnterExit[j], m_moduleFlowData);

				// Log(5) << " Move to module " << otherModule << " -> deltaCodelength: " << deltaCodelength <<
				// 		", deltaEnter: " << moduleDeltaEnterExit[j].deltaEnter << ", deltaExit: " << moduleDeltaEnterExit[j].deltaExit <<
				// 		", module enter/exit: " << m_moduleFlowData[otherModule].enterFlow << " / " << m_moduleFlowData[otherModule].exitFlow <<
				// 		", OLD delta enter/exit: " << oldModuleDelta.deltaEnter << " / " << oldModuleDelta.deltaExit << "\n";
				if (deltaCodelength < bestDeltaCodelength - this->minimumSingleNodeCodelengthImprovement)
				{
					bestDeltaModule = moduleDeltaEnterExit[j];
					bestDeltaCodelength = deltaCodelength;
				}

				// Save strongest connected module to prefer if codelength improvement equal
				if (moduleDeltaEnterExit[j].deltaExit > strongestConnectedModule.deltaExit)
				{
					strongestConnectedModule = moduleDeltaEnterExit[j];
					deltaCodelengthOnStrongestConnectedModule = deltaCodelength;
				}
			}
		}

		// Prefer strongest connected module if equal delta codelength
		if (strongestConnectedModule.module != bestDeltaModule.module &&
				deltaCodelengthOnStrongestConnectedModule <= bestDeltaCodelength + this->minimumSingleNodeCodelengthImprovement)
		{
			bestDeltaModule = strongestConnectedModule;
		}

		// Make best possible move
		if(bestDeltaModule.module != current.index)
		{
			unsigned int bestModuleIndex = bestDeltaModule.module;
			//Update empty module vector
			if(m_moduleMembers[bestModuleIndex] == 0)
			{
				m_emptyModules.pop_back();
			}
			if(m_moduleMembers[current.index] == 1)
			{
				m_emptyModules.push_back(current.index);
			}

			// timer.start();
			m_objective.updateCodelengthOnMovingNode(current, oldModuleDelta, bestDeltaModule, m_moduleFlowData);
			// t += timer.getElapsedTimeInMilliSec();
			// ++tCount;

			m_moduleMembers[current.index] -= 1;
			m_moduleMembers[bestModuleIndex] += 1;

			// double oldCodelength = m_objective.codelength;
			// Log(5) << " --> Moved to module " << bestModuleIndex << " -> codelength: " << oldCodelength << " + " <<
			// 		bestDeltaCodelength << " (" << (m_objective.codelength - oldCodelength) << ") = " << m_objective << "\n";

//			unsigned int oldModuleIndex = current.index;
			current.index = bestModuleIndex;

			++numMoved;

			// Mark neighbours as dirty
			for (auto& e : current.outEdges())
				e->target.dirty = true;
			for (auto& e : current.inEdges())
				e->source.dirty = true;
		}
		else
			current.dirty = false;

//		if (!current.dirty)
//			Log(5) << " --> Didn't move!\n";

	}

	Log() << " !!! " << t << "/" << tCount << " = " << t / tCount << " !!! ";

	return numMoved;
}

template<typename Node, typename Objective>
inline
void InfomapOptimizer<Node, Objective>::consolidateModules(bool replaceExistingModules)
{
//	Log(1) << "Consolidate modules to codelength " << m_optimizer << "..." << std::endl;

	auto& network = activeNetwork();
	unsigned int numNodes = network.size();
	std::vector<InfoNodeBase*> modules(numNodes, 0);

	auto& moduleFlowData = getModuleFlowData();

	InfoNodeBase& firstActiveNode = *network[0];
	unsigned int level = firstActiveNode.depth();
	unsigned int leafLevel = numLevels();

	if (leafLevel == 1)
		replaceExistingModules = false;


	// Release children pointers on current parent(s) to put new modules between
	for (InfoNodeBase* n : network) {
		n->parent->releaseChildren(); // Safe to call multiple times
	}

	// Create the new module nodes and re-parent the active network from its common parent to the new module level
	for (unsigned int i = 0; i < numNodes; ++i)
	{
		InfoNodeBase* node = network[i];
		unsigned int moduleIndex = node->index;
		if (modules[moduleIndex] == 0)
		{
			modules[moduleIndex] = new Node(moduleFlowData[moduleIndex]);
			node->parent->addChild(modules[moduleIndex]);
			modules[moduleIndex]->index = moduleIndex;
		}
		modules[moduleIndex]->addChild(node);
	}


	// Aggregate links from lower level to the new modular level
	typedef std::pair<InfoNodeBase*, InfoNodeBase*> NodePair;
	typedef std::map<NodePair, double> EdgeMap;
	EdgeMap moduleLinks;

	for (auto& node : network)
	{
		InfoNodeBase* parent = node->parent;
		for (auto& e : node->outEdges())
		{
			EdgeType& edge = *e;
			InfoNodeBase* otherParent = edge.target.parent;
			if (otherParent != parent)
			{
				InfoNodeBase *m1 = parent, *m2 = otherParent;
				// If undirected, the order may be swapped to aggregate the edge on an opposite one
				if (!this->directedEdges && m1->index > m2->index)
					std::swap(m1, m2);
				// Insert the node pair in the edge map. If not inserted, add the flow value to existing node pair.
				std::pair<EdgeMap::iterator, bool> ret = \
						moduleLinks.insert(std::make_pair(NodePair(m1, m2), edge.data.flow));
				if (!ret.second)
					ret.first->second += edge.data.flow;
			}
		}
	}

	// Add the aggregated edge flow structure to the new modules
	for (auto& e : moduleLinks)
	{
		const NodePair& nodePair = e.first;
		nodePair.first->addOutEdge(*nodePair.second, 0.0, e.second);
	}

	if (replaceExistingModules)
	{
		if (level == 1) {
			Log(4) << "Consolidated super modules, removing old modules..." << std::endl;
			for (auto& node : network)
				node->replaceWithChildren();
		}
		else if (level == 2) {
			Log(4) << "Consolidated sub-modules, removing modules..." << std::endl;
			unsigned int moduleIndex = 0;
			for (InfoNodeBase& module : root()) {
				// Store current modular structure on the sub-modules
				for (auto& subModule : module)
					subModule.index = moduleIndex;
				++moduleIndex;
			}
			root().replaceChildrenWithGrandChildren();
		}
	}

	// Calculate the number of non-trivial modules
	m_numNonTrivialTopModules = 0;
	for (auto& module : root())
	{
		if (module.childDegree() != 1)
			++m_numNonTrivialTopModules;
	}

	m_objective.consolidateModules(modules);
	m_consolidatedObjective = m_objective;
}

template<typename Node, typename Objective>
inline
bool InfomapOptimizer<Node, Objective>::restoreConsolidatedOptimizationPointIfNoImprovement(bool forceRestore)
{
	if (forceRestore || m_objective.codelength >= m_consolidatedObjective.codelength - this->minimumSingleNodeCodelengthImprovement)
	{
		m_objective = m_consolidatedObjective;
		return true;
	}
	return false;
}


// ===================================================
// IO
// ===================================================

template<typename Node, typename Objective>
std::ostream& InfomapOptimizer<Node, Objective>::toString(std::ostream& out) const
{
	return out << m_objective;
}


// ===================================================
// Debug: *
// ===================================================

template<typename Node, typename Objective>
inline
void InfomapOptimizer<Node, Objective>::printDebug()
{
	m_objective.printDebug();
}


} /* namespace ClusteringModule */

#endif /* INFOMAP_OPTIMIZER_H_ */
