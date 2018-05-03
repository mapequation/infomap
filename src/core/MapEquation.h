/*
 * MapEquation.h
 *
 *  Created on: 25 feb 2015
 *      Author: Daniel
 */

#ifndef SRC_CLUSTERING_MAPEQUATION_H_
#define SRC_CLUSTERING_MAPEQUATION_H_

#include "IMapEquation.h"
#include "../utils/infomath.h"
#include "../io/convert.h"
#include "../utils/Log.h"
#include "../utils/VectorMap.h"
// #include "InfoNode.h"
#include "FlowData.h"
#include <vector>
#include <map>
#include <iostream>

namespace infomap {

class InfoNode;

// class MapEquation : public IMapEquation {
class MapEquation {
public:
	using FlowDataType = FlowData;
	using DeltaFlowDataType = MemDeltaFlow;
	// using DeltaFlowDataType = DeltaFlow;

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

	virtual MapEquation* clone() {
		return new MapEquation(*this);
	}

	// ===================================================
	// Getters
	// ===================================================

	virtual bool haveMemory() { return false; }

	// ===================================================
	// IO
	// ===================================================

	virtual std::ostream& print(std::ostream&) const;

	// friend std::ostream& operator<<(std::ostream&, const MapEquation&);


	// ===================================================
	// Init
	// ===================================================

	virtual void initNetwork(InfoNode& root);

	virtual void initSuperNetwork(InfoNode& root);

	virtual void initSubNetwork(InfoNode& root);

	virtual void initPartition(std::vector<InfoNode*>& nodes);

	// ===================================================
	// Codelength
	// ===================================================

	virtual double calcCodelength(const InfoNode& parent) const;
	virtual void addMemoryContributions(InfoNode& current, DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta) {}

	virtual void addMemoryContributions(InfoNode& current, DeltaFlowDataType& oldModuleDelta, VectorMap<DeltaFlowDataType>& moduleDeltaFlow) {}

	virtual double getDeltaCodelengthOnMovingNode(InfoNode& current,
			DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData);

	// ===================================================
	// Consolidation
	// ===================================================

	virtual void updateCodelengthOnMovingNode(InfoNode& current,
			DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData);

	virtual void consolidateModules(std::vector<InfoNode*>& modules) {}

	// ===================================================
	// Debug
	// ===================================================

	void printDebug();


protected:
	// ===================================================
	// Protected member functions
	// ===================================================

	double calcCodelengthOnModuleOfLeafNodes(const InfoNode& parent) const;

	double calcCodelengthOnModuleOfModules(const InfoNode& parent) const;

	void calculateCodelength(std::vector<InfoNode*>& nodes);

	void calculateCodelengthTerms(std::vector<InfoNode*>& nodes);

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
