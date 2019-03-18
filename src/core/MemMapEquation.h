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
// #include "NodeBase.h"
#include "../utils/Log.h"
#include <vector>
#include <set>
#include <map>
#include <utility>

namespace infomap {

class NodeBase;
template<class FlowDataType>
class Node;
struct MemNodeSet;

class MemMapEquation : protected MapEquation {
	using Base = MapEquation;
public:
	using FlowDataType = FlowData;
	using DeltaFlowDataType = MemDeltaFlow;
	using NodeType = Node<FlowDataType>;

	MemMapEquation() : MapEquation() {}

	MemMapEquation(const MemMapEquation& other)
	:	MapEquation(other),
		m_physToModuleToMemNodes(other.m_physToModuleToMemNodes),
		m_numPhysicalNodes(other.m_numPhysicalNodes),
		m_memoryContributionsAdded(other.m_memoryContributionsAdded)
	{}

	MemMapEquation& operator=(const MemMapEquation& other) {
		Base::operator =(other);
		m_physToModuleToMemNodes = other.m_physToModuleToMemNodes;
		m_numPhysicalNodes = other.m_numPhysicalNodes;
		m_memoryContributionsAdded = other.m_memoryContributionsAdded;
		return *this;
	}

	virtual ~MemMapEquation() {}

	// ===================================================
	// Getters
	// ===================================================

	static bool haveMemory() { return true; }

	using Base::getIndexCodelength;

	using Base::getModuleCodelength;

	using Base::getCodelength;

	// ===================================================
	// IO
	// ===================================================

	// using Base::print;
	std::ostream& print(std::ostream& out) const;
	// friend std::ostream& operator<<(std::ostream&, const MemMapEquation&);

	// ===================================================
	// Init
	// ===================================================

	void init(const Config& config);

	void initNetwork(NodeBase& root);

	void initSuperNetwork(NodeBase& root);

	void initSubNetwork(NodeBase& root);

	void initPartition(std::vector<NodeBase*>& nodes);

	// ===================================================
	// Codelength
	// ===================================================

	double calcCodelength(const NodeBase& parent) const;

	void addMemoryContributions(NodeBase& current, DeltaFlowDataType& oldModuleDelta, VectorMap<DeltaFlowDataType>& moduleDeltaFlow);

	double getDeltaCodelengthOnMovingNode(NodeBase& current,
			DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData, std::vector<unsigned int>& moduleMembers);

	// ===================================================
	// Consolidation
	// ===================================================

	void updateCodelengthOnMovingNode(NodeBase& current,
			DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData, std::vector<unsigned int>& moduleMembers);

	void consolidateModules(std::vector<NodeBase*>& modules);

	// ===================================================
	// Debug
	// ===================================================

	void printDebug();

protected:
	// ===================================================
	// Protected member functions
	// ===================================================
	double calcCodelengthOnModuleOfLeafNodes(const NodeBase& parent) const;

	// ===================================================
	// Init
	// ===================================================

	void initPhysicalNodes(NodeBase& root);

	void initPartitionOfPhysicalNodes(std::vector<NodeBase*>& nodes);

	// ===================================================
	// Codelength
	// ===================================================

	void calculateCodelength(std::vector<NodeBase*>& nodes);

	using Base::calculateCodelengthTerms;

	using Base::calculateCodelengthFromCodelengthTerms;

	void calculateNodeFlow_log_nodeFlow();

	// ===================================================
	// Consolidation
	// ===================================================

	void updatePhysicalNodes(NodeBase& current, unsigned int oldModuleIndex, unsigned int bestModuleIndex);

	void addMemoryContributionsAndUpdatePhysicalNodes(NodeBase& current, DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta);

	const NodeType& getNode(const NodeBase&) const;

public:
	NodeBase* createNode() const;
	NodeBase* createNode(const NodeBase&) const;
	NodeBase* createNode(FlowDataType) const;

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


}

#endif /* MODULES_CLUSTERING_CLUSTERING_MEMMAPEQUATION_H_ */
