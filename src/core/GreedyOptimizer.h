/*
 * GreedyOptimizer.h
 *
 *  Created on: 4 mar 2015
 *      Author: Daniel
 */

#ifndef MODULES_CLUSTERING_CLUSTERING_GREEDYOPTIMIZER_H_
#define MODULES_CLUSTERING_CLUSTERING_GREEDYOPTIMIZER_H_

#include <vector>
#include "../utils/Log.h"
#include "../utils/infomath.h"
#include "InfoNodeBase.h"
#include <stdexcept>

namespace infomap {

template<typename Node, typename Objective>
class GreedyOptimizer {
	using FlowData = typename Objective::FlowDataType;
	using DeltaFlowData = typename Objective::DeltaFlowDataType;
public:
	GreedyOptimizer() {}
	virtual ~GreedyOptimizer() {}

	// ===================================================
	// IO
	// ===================================================

	virtual std::ostream& toString(std::ostream& o) const;

	// ===================================================
	// Getters
	// ===================================================

	double codelength() const;

	double indexCodelength() const;

	double moduleCodelength() const;

	unsigned int numActiveModules() const;

protected:
	std::vector<InfoNodeBase*>& activeNetwork() { return *m_activeNetwork; }
	const std::vector<InfoNodeBase*>& activeNetwork() const { return *m_activeNetwork; }

public:
	auto getModuleFlowData() -> std::vector<FlowData>& { return m_moduleFlowData; }

	// ===================================================
	// Init
	// ===================================================

	void initNetwork(InfoNodeBase& root);

	void initSuperNetwork(InfoNodeBase& root);

	void initSubNetwork(InfoNodeBase& root);

	void initPartition(std::vector<InfoNodeBase*>& network);

	double calcCodelength(const InfoNodeBase& parent) const;

	// ===================================================
	// Optimize
	// ===================================================

	void getNodeEnumeration(std::vector<unsigned int>& enumeration);

	/**
	 * Return the number of effective loops, i.e. not the last if not at coreLoopLimit
	 */
	unsigned int optimizeModules(unsigned int coreLoopLimit, double minimumCodelengthImprovement);

	unsigned int tryMoveEachNodeIntoBestModule();

	void moveActiveNodesToPredifinedModules(std::vector<unsigned int>& modules);

	/**
	 * Return true if restored to consolidated state
	 */
	bool restoreObjectiveToConsolidatedStateIfNoImprovement(bool forceRestore = false);

	// ===================================================
	// Consolidation
	// ===================================================

	template<typename container>
	void consolidateModules(container& modules);

	// ===================================================
	// Debug
	// ===================================================

	void printDebug();

protected:
	Objective m_objective;
	Objective m_consolidatedObjective;
	std::vector<InfoNodeBase*>* m_activeNetwork = nullptr;
	std::vector<FlowData> m_moduleFlowData;
	std::vector<unsigned int> m_moduleMembers;
	std::vector<unsigned int> m_emptyModules;

	infomath::RandGen m_rand = infomath::RandGen(123);

	// Config
	double m_minimumSingleNodeCodelengthImprovement = 1e-10;
};



// ===================================================
// IO
// ===================================================

template<typename Node, typename Objective>
inline
std::ostream& GreedyOptimizer<Node, Objective>::toString(std::ostream& out) const {
	return out << m_objective;
}

template<typename Node, typename Objective>
inline
std::ostream& operator<<(std::ostream& out, const GreedyOptimizer<Node, Objective>& optimizer) {
	return optimizer.toString(out);
}


// ===================================================
// Getters
// ===================================================

template<typename Node, typename Objective>
inline
double GreedyOptimizer<Node, Objective>::codelength() const
{
	return m_objective.codelength;
}

template<typename Node, typename Objective>
inline
double GreedyOptimizer<Node, Objective>::indexCodelength() const
{
	return m_objective.indexCodelength;
}

template<typename Node, typename Objective>
inline
double GreedyOptimizer<Node, Objective>::moduleCodelength() const
{
	return m_objective.moduleCodelength;
}

template<typename Node, typename Objective>
inline
unsigned int GreedyOptimizer<Node, Objective>::numActiveModules() const
{
	return activeNetwork().size() - m_emptyModules.size();
}

// ===================================================
// Init
// ===================================================

template<typename Node, typename Objective>
inline
void GreedyOptimizer<Node, Objective>::initNetwork(InfoNodeBase& root)
{
	Log(4) << "GreedyOptimizer::initNetwork()...\n";
	m_objective.initNetwork(root);
}

template<typename Node, typename Objective>
inline
void GreedyOptimizer<Node, Objective>::initSuperNetwork(InfoNodeBase& root)
{
	Log(4) << "GreedyOptimizer::initSuperNetwork()...\n";
	m_objective.initSuperNetwork(root);
}

template<typename Node, typename Objective>
inline
void GreedyOptimizer<Node, Objective>::initSubNetwork(InfoNodeBase& root)
{
	Log(4) << "GreedyOptimizer::initSubNetwork()...\n";
	m_objective.initSubNetwork(root);
}

template<typename Node, typename Objective>
void GreedyOptimizer<Node, Objective>::initPartition(std::vector<InfoNodeBase*>& network)
{
	Log(4) << "GreedyOptimizer::initPartition() with " << network.size() << " nodes..." << std::endl;
	m_activeNetwork = &network;

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
}

template<typename Node, typename Objective>
inline
double GreedyOptimizer<Node, Objective>::calcCodelength(const InfoNodeBase& parent) const
{
	return m_objective.calcCodelength(parent);
}


// ===================================================
// Optimize
// ===================================================

template<typename Node, typename Objective>
inline
void GreedyOptimizer<Node, Objective>::getNodeEnumeration(std::vector<unsigned int>& enumeration)
{
	// Get random enumeration of nodes
	enumeration.resize(activeNetwork().size());
	infomath::getRandomizedIndexVector(enumeration, m_rand);
}

template<typename Node, typename Objective>
unsigned int GreedyOptimizer<Node, Objective>::optimizeModules(unsigned int coreLoopLimit, double minimumCodelengthImprovement)
{
	unsigned int coreLoopCount = 0;
	double oldCodelength = m_objective.codelength;
	unsigned int numEffectiveLoops = 0;

	while (coreLoopCount != coreLoopLimit)
	{
		++coreLoopCount;
		unsigned int numNodesMoved = tryMoveEachNodeIntoBestModule();
		// Break if not enough improvement
		if (numNodesMoved == 0 || m_objective.codelength > oldCodelength - minimumCodelengthImprovement)
			break;
		++numEffectiveLoops;
		oldCodelength = m_objective.codelength;
	}

	return numEffectiveLoops;
}

template<typename Node, typename Objective>
unsigned int GreedyOptimizer<Node, Objective>::tryMoveEachNodeIntoBestModule()
{
	// Get enumeration of nodes
	std::vector<unsigned int> nodeEnumeration;
	getNodeEnumeration(nodeEnumeration);

	auto& network = activeNetwork();
	unsigned int numNodes = nodeEnumeration.size();
	unsigned int numMoved = 0;

	for (unsigned int i = 0; i < numNodes; ++i)
	{
		InfoNodeBase& current = *network[nodeEnumeration[i]];

//		Log(5) << "Trying to move node " << current << " from module " << current.index << "...\n";

		if (!current.dirty)
			continue;

//		if (m_moduleMembers[current.index] > 1 && isFirstLoop())
//			continue;

		// If no links connecting this node with other nodes, it won't move into others,
		// and others won't move into this. TODO: Always best leave it alone?
		// For memory networks, don't skip try move to same physical node!
		// if (current.degree() == 0)
		// {
		// 	current.dirty = false;
		// 	continue;
		// }

		// Create map with module links
		std::map<unsigned int, DeltaFlowData> deltaFlow;

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
			deltaFlow[neighbour.index] += DeltaFlowData(neighbour.index, 0.0, edge.data.flow);
		}

		// For not moving
		DeltaFlowData& oldModuleDelta = deltaFlow[current.index];
		oldModuleDelta.module = current.index; // Make sure index is correct if created new

		// Option to move to empty module (if node not already alone)
		if (m_moduleMembers[current.index] > 1 && m_emptyModules.size() > 0) {
			deltaFlow[m_emptyModules.back()] += DeltaFlowData(m_emptyModules.back(), 0.0, 0.0);
		}


		// For memory networks
		m_objective.addMemoryContributions(current, oldModuleDelta, deltaFlow);

		// Collect the move options to a vector
		std::vector<DeltaFlowData> moduleDeltaEnterExit(deltaFlow.size());
		unsigned int numModuleLinks = 0;
		for (auto& it : deltaFlow)
		{
			moduleDeltaEnterExit[numModuleLinks] = it.second;
			++numModuleLinks;
		}

		// Randomize link order for optimized search
		infomath::uniform_uint_dist uniform;
		for (unsigned int j = 0; j < numModuleLinks - 1; ++j)
		{
			unsigned int randPos = j + uniform(m_rand, infomath::uniform_param_t(0, numModuleLinks - j - 1));
			swap(moduleDeltaEnterExit[j], moduleDeltaEnterExit[randPos]);
		}

		DeltaFlowData bestDeltaModule(oldModuleDelta);
		double bestDeltaCodelength = 0.0;
		DeltaFlowData strongestConnectedModule(oldModuleDelta);
		double deltaCodelengthOnStrongestConnectedModule = 0.0;

		Log(5) << "Move node " << current << " in module " << current.index << "...\n";
		// Find the move that minimizes the description length
		for (unsigned int j = 0; j < numModuleLinks; ++j)
		{
			unsigned int otherModule = moduleDeltaEnterExit[j].module;
			if(otherModule != current.index)
			{
				double deltaCodelength = m_objective.getDeltaCodelengthOnMovingNode(current,
						oldModuleDelta, moduleDeltaEnterExit[j], m_moduleFlowData);

				Log(5) << " Move to module " << otherModule << " -> deltaCodelength: " << deltaCodelength <<
						", deltaEnter: " << moduleDeltaEnterExit[j].deltaEnter << ", deltaExit: " << moduleDeltaEnterExit[j].deltaExit <<
						", module enter/exit: " << m_moduleFlowData[otherModule].enterFlow << " / " << m_moduleFlowData[otherModule].exitFlow <<
						", OLD delta enter/exit: " << oldModuleDelta.deltaEnter << " / " << oldModuleDelta.deltaExit << "\n";
				if (deltaCodelength < bestDeltaCodelength - m_minimumSingleNodeCodelengthImprovement)
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
				deltaCodelengthOnStrongestConnectedModule <= bestDeltaCodelength + m_minimumSingleNodeCodelengthImprovement)
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

			double oldCodelength = m_objective.codelength;

			m_objective.updateCodelengthOnMovingNode(current, oldModuleDelta, bestDeltaModule, m_moduleFlowData);

			m_moduleMembers[current.index] -= 1;
			m_moduleMembers[bestModuleIndex] += 1;

			Log(5) << " --> Moved to module " << bestModuleIndex << " -> codelength: " << oldCodelength << " + " <<
					bestDeltaCodelength << " (" << (m_objective.codelength - oldCodelength) << ") = " << m_objective << "\n";

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

	return numMoved;
}


template<typename Node, typename Objective>
void GreedyOptimizer<Node, Objective>::moveActiveNodesToPredifinedModules(std::vector<unsigned int>& modules)
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
bool GreedyOptimizer<Node, Objective>::restoreObjectiveToConsolidatedStateIfNoImprovement(bool forceRestore)
{
	if (forceRestore || m_objective.codelength >= m_consolidatedObjective.codelength - m_minimumSingleNodeCodelengthImprovement)
	{
		m_objective = m_consolidatedObjective;
		return true;
	}
	return false;
}

template<typename Node, typename Objective>
template<typename container>
void GreedyOptimizer<Node, Objective>::consolidateModules(container& modules)
{
	m_objective.consolidateModules(modules);
	m_consolidatedObjective = m_objective;
}



// ===================================================
// Debug
// ===================================================

template<typename Node, typename Objective>
void GreedyOptimizer<Node, Objective>::printDebug()
{
//	std::cout << "Module flow data:\n";
//	for (FlowData& data : m_moduleFlowData) {
//		std::cout << "flow: " << data.flow << ", enterFlow: " << data.enterFlow << ", exitFlow: " << data.exitFlow << "\n";
//	}
//	std::cout << "Objective: " << m_objective << "\n";
	m_objective.printDebug();
}


} /* namespace ClusteringModule */

#endif /* MODULES_CLUSTERING_CLUSTERING_GREEDYOPTIMIZER_H_ */
