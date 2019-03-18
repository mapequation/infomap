/*
 * MapEquation.h
 *
 *  Created on: 25 feb 2015
 *      Author: Daniel
 */

#ifndef SRC_CLUSTERING_MAPEQUATION_H_
#define SRC_CLUSTERING_MAPEQUATION_H_

#include "../utils/infomath.h"
#include "../io/convert.h"
#include "../io/Config.h"
#include "../utils/Log.h"
#include "../utils/VectorMap.h"
// #include "NodeBase.h"
#include "FlowData.h"
#include <vector>
#include <map>
#include <iostream>

namespace infomap {

class NodeBase;
// struct Config;
template<class FlowDataType>
class Node;

class MapEquation {
public:
	using FlowDataType = FlowData;
	// using DeltaFlowDataType = MemDeltaFlow;
	using DeltaFlowDataType = DeltaFlow;
	using NodeType = Node<FlowDataType>;

	MapEquation() {}

	MapEquation(const MapEquation& other)
	:	codelength(other.codelength),
		indexCodelength(other.indexCodelength),
		moduleCodelength(other.moduleCodelength),
		nodeFlow_log_nodeFlow(other.nodeFlow_log_nodeFlow),
		flow_log_flow(other.flow_log_flow),
		exit_log_exit(other.exit_log_exit),
		enter_log_enter(other.enter_log_enter),
		enterFlow(other.enterFlow),
		enterFlow_log_enterFlow(other.enterFlow_log_enterFlow),
		exitNetworkFlow(other.exitNetworkFlow),
		exitNetworkFlow_log_exitNetworkFlow(other.exitNetworkFlow_log_exitNetworkFlow)
	{}

	MapEquation& operator=(const MapEquation& other) {
		codelength = other.codelength;
		indexCodelength = other.indexCodelength;
		moduleCodelength = other.moduleCodelength;
		nodeFlow_log_nodeFlow = other.nodeFlow_log_nodeFlow;
		flow_log_flow = other.flow_log_flow;
		exit_log_exit = other.exit_log_exit;
		enter_log_enter = other.enter_log_enter;
		enterFlow = other.enterFlow;
		enterFlow_log_enterFlow = other.enterFlow_log_enterFlow;
		exitNetworkFlow = other.exitNetworkFlow;
		exitNetworkFlow_log_exitNetworkFlow = other.exitNetworkFlow_log_exitNetworkFlow;
		return *this;
	}

	virtual ~MapEquation() {}

	// ===================================================
	// Getters
	// ===================================================

	static bool haveMemory() { return false; }

	double getIndexCodelength() const { return indexCodelength; }

	double getModuleCodelength() const { return moduleCodelength; }

	double getCodelength() const { return codelength; }

	// ===================================================
	// IO
	// ===================================================

	std::ostream& print(std::ostream&) const;

	// friend std::ostream& operator<<(std::ostream&, const MapEquation&);


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

	void addMemoryContributions(NodeBase& current, DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta) {}

	void addMemoryContributions(NodeBase& current, DeltaFlowDataType& oldModuleDelta, VectorMap<DeltaFlowDataType>& moduleDeltaFlow) {}

	double getDeltaCodelengthOnMovingNode(NodeBase& current,
			DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData, std::vector<unsigned int>& moduleMembers);

	// ===================================================
	// Consolidation
	// ===================================================

	NodeBase* createNode() const;
	NodeBase* createNode(const NodeBase&) const;
	NodeBase* createNode(FlowDataType) const;
	const NodeType& getNode(const NodeBase&) const;

	void updateCodelengthOnMovingNode(NodeBase& current,
			DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData, std::vector<unsigned int>& moduleMembers);

	void consolidateModules(std::vector<NodeBase*>& modules) {}

	// ===================================================
	// Debug
	// ===================================================

	void printDebug();


protected:
	// ===================================================
	// Protected member functions
	// ===================================================

	double calcCodelengthOnModuleOfLeafNodes(const NodeBase& parent) const;

	double calcCodelengthOnModuleOfModules(const NodeBase& parent) const;

	void calculateCodelength(std::vector<NodeBase*>& nodes);

	void calculateCodelengthTerms(std::vector<NodeBase*>& nodes);

	void calculateCodelengthFromCodelengthTerms();


public:
	// ===================================================
	// Public member variables
	// ===================================================

	double codelength = 0.0;
	double indexCodelength = 0.0;
	double moduleCodelength = 0.0;

protected:
	// ===================================================
	// Protected member variables
	// ===================================================

	double nodeFlow_log_nodeFlow = 0.0; // constant while the leaf network is the same
	double flow_log_flow = 0.0; // node.(flow + exitFlow)
	double exit_log_exit = 0.0;
	double enter_log_enter = 0.0;
	double enterFlow = 0.0;
	double enterFlow_log_enterFlow = 0.0;

	// For hierarchical
	double exitNetworkFlow = 0.0;
	double exitNetworkFlow_log_exitNetworkFlow = 0.0;
};

}


#endif /* SRC_CLUSTERING_MAPEQUATION_H_ */
