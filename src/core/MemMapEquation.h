/*
 * MemMapEquation.h
 *
 *  Created on: 4 mar 2015
 *      Author: Daniel
 */

#ifndef MODULES_CLUSTERING_CLUSTERING_MEMMAPEQUATION_H_
#define MODULES_CLUSTERING_CLUSTERING_MEMMAPEQUATION_H_

#include "MapEquation.h"
#include "FlowData.h"
#include "InfoNode.h"
#include "../utils/Log.h"
#include <vector>
#include <set>
#include <map>
#include <utility>

namespace infomap {

struct MemNodeSet;

template<typename Node>
class MemMapEquation: protected MapEquation<Node> {
	using Base = MapEquation<Node>;
public:
	using FlowDataType = FlowData;
	using DeltaFlowDataType = MemDeltaFlow;

	MemMapEquation() : MapEquation<Node>() {}

	MemMapEquation(const MemMapEquation<Node>& other)
	:	MapEquation<Node>(other),
		m_physToModuleToMemNodes(other.m_physToModuleToMemNodes),
		m_numPhysicalNodes(other.m_numPhysicalNodes)
	{}

	MemMapEquation<Node>& operator=(const MemMapEquation<Node>& other) {
		Base::operator =(other);
		return *this;
	}

	virtual ~MemMapEquation() {}

	// ===================================================
	// Getters
	// ===================================================

	using Base::get;

	// ===================================================
	// IO
	// ===================================================

	using Base::toString;

	// ===================================================
	// Init
	// ===================================================

	void initNetwork(InfoNodeBase& root);

	void initSuperNetwork(InfoNodeBase& root);

	void initSubNetwork(InfoNodeBase& root);

	template<typename container>
	void initPartition(container& nodes);

	// ===================================================
	// Codelength
	// ===================================================

	double calcCodelength(const InfoNodeBase& parent) const;

	void addMemoryContributions(InfoNodeBase& current, DeltaFlowDataType& oldModuleDelta, ReusableVector<DeltaFlowDataType>& moduleDeltaFlow);

	double getDeltaCodelengthOnMovingNode(InfoNodeBase& current,
			DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData);

	// ===================================================
	// Consolidation
	// ===================================================

	void updateCodelengthOnMovingNode(InfoNodeBase& current,
			DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData);

	template<typename container>
	void consolidateModules(container& modules);

	// ===================================================
	// Debug
	// ===================================================

	void printDebug();

protected:
	// ===================================================
	// Protected member functions
	// ===================================================

	// ===================================================
	// Init
	// ===================================================

	void initPhysicalNodes(InfoNodeBase& root);

	template<typename container>
	void initPartitionOfPhysicalNodes(container& nodes);

	// ===================================================
	// Codelength
	// ===================================================

	template<typename container>
	void calculateCodelength(container& nodes);

	using Base::calculateCodelengthTerms;

	using Base::calculateCodelengthFromCodelengthTerms;

	void calculateNodeFlow_log_nodeFlow();

	// ===================================================
	// Consolidation
	// ===================================================

	void updatePhysicalNodes(Node& current, unsigned int oldModuleIndex, unsigned int bestModuleIndex);

	void addMemoryContributionsAndUpdatePhysicalNodes(Node& current, DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta);

public:
	// ===================================================
	// Public member variables
	// ===================================================

	using Base::codelength;
	using Base::indexCodelength;
	using Base::moduleCodelength;

protected:
	// ===================================================
	// Protected member variables
	// ===================================================

	using Base::nodeFlow_log_nodeFlow; // constant while the leaf network is the same
	using Base::flow_log_flow; // node.(flow + exitFlow)
	using Base::exit_log_exit;
	using Base::enter_log_enter;
	using Base::enterFlow;
	using Base::enterFlow_log_enterFlow;

	// For hierarchical
	using Base::exitNetworkFlow;
	using Base::exitNetworkFlow_log_exitNetworkFlow;

	using ModuleToMemNodes = std::map<unsigned int, MemNodeSet>;

	std::vector<ModuleToMemNodes> m_physToModuleToMemNodes; // vector[physicalNodeID] map<moduleID, {#memNodes, sumFlow}>
	unsigned int m_numPhysicalNodes = 0;
	bool m_memoryContributionsAdded = false;
};

struct MemNodeSet
{
	MemNodeSet(unsigned int numMemNodes, double sumFlow) : numMemNodes(numMemNodes), sumFlow(sumFlow) {}
	MemNodeSet(const MemNodeSet& other) : numMemNodes(other.numMemNodes), sumFlow(other.sumFlow) {}
	MemNodeSet& operator=(const MemNodeSet& other) { numMemNodes = other.numMemNodes; sumFlow = other.sumFlow; return *this; }
	unsigned int numMemNodes; // use counter to check for zero to avoid round-off errors in sumFlow
	double sumFlow;
};



// ===================================================
// IO
// ===================================================

template<typename Node>
inline
std::ostream& operator<<(std::ostream& out, const MemMapEquation<Node>& mapEq) {
	return mapEq.toString(out);
}


// ===================================================
// Init
// ===================================================


template<typename Node>
void MemMapEquation<Node>::initNetwork(InfoNodeBase& root)
{
	initPhysicalNodes(root);
}

template<typename Node>
void MemMapEquation<Node>::initSuperNetwork(InfoNodeBase& root)
{
	//TODO: How use enterFlow instead of flow
}

template<typename Node>
void MemMapEquation<Node>::initSubNetwork(InfoNodeBase& root)
{
//	Base::initSubNetwork(root);
}

template<typename Node>
template<typename container>
inline
void MemMapEquation<Node>::initPartition(container& nodes)
{
	initPartitionOfPhysicalNodes(nodes);

	calculateCodelength(nodes);
}

template<typename Node>
inline
void MemMapEquation<Node>::initPhysicalNodes(InfoNodeBase& root)
{
	bool notInitiated = get(*root.firstChild).physicalNodes.empty();
	if (notInitiated)
	{
		Log(3) << "MemMapEquation::initPhysicalNodesOnOriginalNetwork()...\n";
		std::set<unsigned int> setOfPhysicalNodes;
		// Collect all physical nodes in this network
		for (InfoNodeBase& node : root)
		{
			setOfPhysicalNodes.insert(get(node).physicalId);
		}

		m_numPhysicalNodes = setOfPhysicalNodes.size();

		// Re-index physical nodes
		std::map<unsigned int, unsigned int> toZeroBasedIndex;
		unsigned int zeroBasedPhysicalId = 0;
		for (unsigned int physIndex : setOfPhysicalNodes)
		{
			toZeroBasedIndex.insert(std::make_pair(physIndex, zeroBasedPhysicalId++));
		}

		for (InfoNodeBase& node : root)
		{
			Node& n = get(node);
			unsigned int zeroBasedIndex = toZeroBasedIndex[n.physicalId];
			n.physicalNodes.push_back(PhysData(zeroBasedIndex, node.data.flow));
		}
	}
	else
	{
		Log(3) << "MemMapEquation::initPhysicalNodesOnSubNetwork()...\n";
		std::set<unsigned int> setOfPhysicalNodes;

		// Collect all physical nodes in this sub network
		for (InfoNodeBase& node : root)
		{
			for (PhysData& physData : get(node).physicalNodes)
			{
				setOfPhysicalNodes.insert(physData.physNodeIndex);
			}
		}

		m_numPhysicalNodes = setOfPhysicalNodes.size();

		// Re-index physical nodes
		std::map<unsigned int, unsigned int> toZeroBasedIndex;
		unsigned int zeroBasedPhysicalId = 0;
		for (unsigned int physIndex : setOfPhysicalNodes)
		{
			toZeroBasedIndex.insert(std::make_pair(physIndex, zeroBasedPhysicalId++));
		}

		for (InfoNodeBase& node : root)
		{
			for (PhysData& physData : get(node).physicalNodes)
			{
				physData.physNodeIndex = toZeroBasedIndex[physData.physNodeIndex];
			}
		}
	}
}

template<typename Node>
template<typename container>
inline
void MemMapEquation<Node>::initPartitionOfPhysicalNodes(container& nodes)
{
	Log(4) << "MemMapEquation::initPartitionOfPhysicalNodes()...\n";
	m_physToModuleToMemNodes.clear();
	m_physToModuleToMemNodes.resize(m_numPhysicalNodes);

	for (auto& n : nodes)
	{
		Node& node = get(*n);
		unsigned int moduleIndex = node.index; // Assume unique module index for all nodes in this initiation phase

		for(PhysData& physData : node.physicalNodes)
		{
			m_physToModuleToMemNodes[physData.physNodeIndex].insert(m_physToModuleToMemNodes[physData.physNodeIndex].end(),
					std::make_pair(moduleIndex, MemNodeSet(1, physData.sumFlowFromM2Node)));
		}
	}

	m_memoryContributionsAdded = false;
}



// ===================================================
// Codelength
// ===================================================

template<typename Node>
template<typename container>
inline
void MemMapEquation<Node>::calculateCodelength(container& nodes)
{
	calculateCodelengthTerms(nodes);

	calculateNodeFlow_log_nodeFlow();

	calculateCodelengthFromCodelengthTerms();
}

template<typename Node>
inline
void MemMapEquation<Node>::calculateNodeFlow_log_nodeFlow()
{
	nodeFlow_log_nodeFlow = 0.0;
	for (unsigned int i = 0; i < m_numPhysicalNodes; ++i)
	{
		const ModuleToMemNodes& moduleToMemNodes = m_physToModuleToMemNodes[i];
		for (ModuleToMemNodes::const_iterator modToMemIt(moduleToMemNodes.begin()); modToMemIt != moduleToMemNodes.end(); ++modToMemIt)
			nodeFlow_log_nodeFlow += infomath::plogp(modToMemIt->second.sumFlow);
	}
}

template<typename Node>
inline
double MemMapEquation<Node>::calcCodelength(const InfoNodeBase& parent) const
{
	if (get(parent).numPhysicalNodes() == 0)
		return Base::calcCodelength(parent); // Infomap root node

	//TODO: For ordinary networks, flow should be used instead of enter flow
	// for leaf nodes, what about memory networks? sumFlowFromM2Node vs sumEnterFlowFromM2Node?
	double parentFlow = parent.data.flow;
	double parentExit = parent.data.exitFlow;
	double totalParentFlow = parentFlow + parentExit;
	if (totalParentFlow < 1e-16)
		return 0.0;

	double indexLength = 0.0;

	for (const PhysData& physData : get(parent).physicalNodes)
	{
		indexLength -= infomath::plogp(physData.sumFlowFromM2Node / totalParentFlow);
	}
	indexLength -= infomath::plogp(parentExit / totalParentFlow);

	indexLength *= totalParentFlow;

	return indexLength;
}

template<typename Node>
inline
void MemMapEquation<Node>::addMemoryContributions(InfoNodeBase& current,
	DeltaFlowDataType& oldModuleDelta, ReusableVector<DeltaFlowDataType>& moduleDeltaFlow)
{
	// Overlapping modules
	/**
	 * delta = old.first + new.first + old.second - new.second.
	 * Two cases: (p(x) = plogp(x))
	 * Moving to a module that already have that physical node: (old: p1, p2, new p3, moving p2 -> old:p1, new p2,p3)
	 * Then old.second = new.second = plogp(physicalNodeSize) -> cancelation -> delta = p(p1) - p(p1+p2) + p(p2+p3) - p(p3)
	 * Moving to a module that not have that physical node: (old: p1, p2, new -, moving p2 -> old: p1, new: p2)
	 * Then new.first = new.second = 0 -> delta = p(p1) - p(p1+p2) + p(p2).
	 */
	auto& physicalNodes = get(current).physicalNodes;
	unsigned int numPhysicalNodes = physicalNodes.size();
	for (unsigned int i = 0; i < numPhysicalNodes; ++i)
	{
		PhysData& physData = physicalNodes[i];
		ModuleToMemNodes& moduleToMemNodes = m_physToModuleToMemNodes[physData.physNodeIndex];
		for (ModuleToMemNodes::iterator overlapIt(moduleToMemNodes.begin()); overlapIt != moduleToMemNodes.end(); ++overlapIt)
		{
			unsigned int moduleIndex = overlapIt->first;
			MemNodeSet& memNodeSet = overlapIt->second;
			if (moduleIndex == current.index) // From where the multiple assigned node is moved
			{
				double oldPhysFlow = memNodeSet.sumFlow;
				double newPhysFlow = memNodeSet.sumFlow - physData.sumFlowFromM2Node;
				oldModuleDelta.sumDeltaPlogpPhysFlow += infomath::plogp(newPhysFlow) - infomath::plogp(oldPhysFlow);
				oldModuleDelta.sumPlogpPhysFlow += infomath::plogp(physData.sumFlowFromM2Node);
			}
			else // To where the multiple assigned node is moved
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


template<typename Node>
inline
double MemMapEquation<Node>::getDeltaCodelengthOnMovingNode(InfoNodeBase& current,
		DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData)
{
	double deltaL = Base::getDeltaCodelengthOnMovingNode(current, oldModuleDelta, newModuleDelta, moduleFlowData);

	double delta_nodeFlow_log_nodeFlow = oldModuleDelta.sumDeltaPlogpPhysFlow + newModuleDelta.sumDeltaPlogpPhysFlow + oldModuleDelta.sumPlogpPhysFlow - newModuleDelta.sumPlogpPhysFlow;

	return deltaL - delta_nodeFlow_log_nodeFlow;
}




// ===================================================
// Consolidation
// ===================================================

template<typename Node>
inline
void MemMapEquation<Node>::updateCodelengthOnMovingNode(InfoNodeBase& current,
		DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData)
{
	Base::updateCodelengthOnMovingNode(current, oldModuleDelta, newModuleDelta, moduleFlowData);
	if (m_memoryContributionsAdded)
		updatePhysicalNodes(get(current), oldModuleDelta.module, newModuleDelta.module);
	else
		addMemoryContributionsAndUpdatePhysicalNodes(get(current), oldModuleDelta, newModuleDelta);

	double delta_nodeFlow_log_nodeFlow = oldModuleDelta.sumDeltaPlogpPhysFlow + newModuleDelta.sumDeltaPlogpPhysFlow + oldModuleDelta.sumPlogpPhysFlow - newModuleDelta.sumPlogpPhysFlow;

	nodeFlow_log_nodeFlow += delta_nodeFlow_log_nodeFlow;
	moduleCodelength -= delta_nodeFlow_log_nodeFlow;
	codelength -= delta_nodeFlow_log_nodeFlow;
}


template<typename Node>
inline
void MemMapEquation<Node>::updatePhysicalNodes(Node& current, unsigned int oldModuleIndex, unsigned int bestModuleIndex)
{
	// For all multiple assigned nodes
	for (unsigned int i = 0; i < current.physicalNodes.size(); ++i)
	{
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

template<typename Node>
inline
void MemMapEquation<Node>::addMemoryContributionsAndUpdatePhysicalNodes(Node& current, DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta)
{
	unsigned int oldModuleIndex = oldModuleDelta.module;
	unsigned int bestModuleIndex = newModuleDelta.module;

	// For all multiple assigned nodes
	for (unsigned int i = 0; i < current.physicalNodes.size(); ++i)
	{
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
		if (overlapIt == moduleToMemNodes.end())
		{
			moduleToMemNodes.insert(std::make_pair(bestModuleIndex, MemNodeSet(1, physData.sumFlowFromM2Node)));
			oldPhysFlow = 0.0;
			newPhysFlow = physData.sumFlowFromM2Node;
			newModuleDelta.sumDeltaPlogpPhysFlow += infomath::plogp(newPhysFlow) - infomath::plogp(oldPhysFlow);
			newModuleDelta.sumPlogpPhysFlow += infomath::plogp(physData.sumFlowFromM2Node);
		}
		else
		{
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


template<typename Node>
template<typename container>
inline
void MemMapEquation<Node>::consolidateModules(container& modules)
{
	std::map<unsigned int, std::map<unsigned int, unsigned int> > validate;

	for(unsigned int i = 0; i < m_numPhysicalNodes; ++i)
	{
		ModuleToMemNodes& modToMemNodes = m_physToModuleToMemNodes[i];
		for(ModuleToMemNodes::iterator overlapIt = modToMemNodes.begin(); overlapIt != modToMemNodes.end(); ++overlapIt)
		{
			if(++validate[overlapIt->first][i] > 1)
				throw std::domain_error("[InfomapGreedy::consolidateModules] Error updating physical nodes: duplication error");

			get(*modules[overlapIt->first]).physicalNodes.push_back(PhysData(i, overlapIt->second.sumFlow));
		}
	}
}



// ===================================================
// Debug
// ===================================================

template<typename Node>
void MemMapEquation<Node>::printDebug()
{
	std::cout << "MemMapEquation::m_numPhysicalNodes: " << m_numPhysicalNodes << "\n";
	Base::printDebug();
}


} /* namespace ClusteringModule */

#endif /* MODULES_CLUSTERING_CLUSTERING_MEMMAPEQUATION_H_ */
