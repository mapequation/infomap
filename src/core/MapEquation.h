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
#include "../utils/Log.h"
#include "../utils/ReusableVector.h"
#include "InfoNodeBase.h"
#include "FlowData.h"
#include <vector>
#include <map>

namespace infomap {

template<typename Node>
class MapEquation {
public:
	using FlowDataType = FlowData;
	using DeltaFlowDataType = DeltaFlow;

	MapEquation() {}

	MapEquation(const MapEquation<Node>& other)
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

	MapEquation<Node>& operator=(const MapEquation<Node>& other) {
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

	Node& get(InfoNodeBase& node);

	const Node& get(const InfoNodeBase& node) const;

	// ===================================================
	// IO
	// ===================================================

	std::ostream& toString(std::ostream& o) const;

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

	void addMemoryContributions(InfoNodeBase& current, DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta) {}

	void addMemoryContributions(InfoNodeBase& current, DeltaFlowDataType& oldModuleDelta, ReusableVector<DeltaFlowDataType>& moduleDeltaFlow) {}

	double getDeltaCodelengthOnMovingNode(InfoNodeBase& current,
			DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData);

	// ===================================================
	// Consolidation
	// ===================================================

	void updateCodelengthOnMovingNode(InfoNodeBase& current,
			DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData);

	template<typename container>
	void consolidateModules(container& modules) {}

	// ===================================================
	// Debug
	// ===================================================

	void printDebug();


protected:
	// ===================================================
	// Protected member functions
	// ===================================================

	double calcCodelengthOnModuleOfLeafNodes(const InfoNodeBase& parent) const;

	double calcCodelengthOnModuleOfModules(const InfoNodeBase& parent) const;

	template<typename container>
	void calculateCodelength(container& nodes);

	template<typename container>
	void calculateCodelengthTerms(container& nodes);

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


// ===================================================
// Getters
// ===================================================

template<typename Node>
inline
Node& MapEquation<Node>::get(InfoNodeBase& node)
{
	return static_cast<Node&>(node);
}

template<typename Node>
inline
const Node& MapEquation<Node>::get(const InfoNodeBase& node) const
{
	return static_cast<const Node&>(node);
}


// ===================================================
// IO
// ===================================================

template<typename Node>
inline
std::ostream& MapEquation<Node>::toString(std::ostream& out) const {
	return out << indexCodelength << " + " << moduleCodelength << " = " <<	io::toPrecision(codelength);
}

template<typename Node>
inline
std::ostream& operator<<(std::ostream& out, const MapEquation<Node>& mapEq) {
	return mapEq.toString(out);
}


// ===================================================
// Init
// ===================================================

template<typename Node>
void MapEquation<Node>::initNetwork(InfoNodeBase& root)
{
	Log(3) << "MapEquation::initNetwork()...\n";

	nodeFlow_log_nodeFlow = 0.0;
	for (InfoNodeBase& node : root)
	{
		nodeFlow_log_nodeFlow += infomath::plogp(node.data.flow);
	}
	initSubNetwork(root);
}

template<typename Node>
void MapEquation<Node>::initSuperNetwork(InfoNodeBase& root)
{
	Log(3) << "MapEquation::initSuperNetwork()...\n";

	nodeFlow_log_nodeFlow = 0.0;
	for (InfoNodeBase& node : root)
	{
		nodeFlow_log_nodeFlow += infomath::plogp(node.data.enterFlow);
	}
}

template<typename Node>
void MapEquation<Node>::initSubNetwork(InfoNodeBase& root)
{
	exitNetworkFlow = root.data.exitFlow;
	exitNetworkFlow_log_exitNetworkFlow = infomath::plogp(exitNetworkFlow);
}

template<typename Node>
template<typename container>
inline
void MapEquation<Node>::initPartition(container& nodes)
{
	calculateCodelength(nodes);
}


// ===================================================
// Codelength
// ===================================================

template<typename Node>
template<typename container>
inline
void MapEquation<Node>::calculateCodelength(container& nodes)
{
	calculateCodelengthTerms(nodes);

	calculateCodelengthFromCodelengthTerms();
}

template<typename Node>
template<typename container>
inline
void MapEquation<Node>::calculateCodelengthTerms(container& nodes)
{
	enter_log_enter = 0.0;
	flow_log_flow = 0.0;
	exit_log_exit = 0.0;
	enterFlow = 0.0;

	// For each module
	for (InfoNodeBase* n : nodes)
	{
		InfoNodeBase& node = *n;
		// own node/module codebook
		flow_log_flow += infomath::plogp(node.data.flow + node.data.exitFlow);

		// use of index codebook
		enter_log_enter += infomath::plogp(node.data.enterFlow);
		exit_log_exit += infomath::plogp(node.data.exitFlow);
		enterFlow += node.data.enterFlow;
	}
	enterFlow += exitNetworkFlow;
	enterFlow_log_enterFlow = infomath::plogp(enterFlow);
}

template<typename Node>
inline
void MapEquation<Node>::calculateCodelengthFromCodelengthTerms()
{
	indexCodelength = enterFlow_log_enterFlow - enter_log_enter - exitNetworkFlow_log_exitNetworkFlow;
	moduleCodelength = -exit_log_exit + flow_log_flow - nodeFlow_log_nodeFlow;
	codelength = indexCodelength + moduleCodelength;
}

template<typename Node>
inline
double MapEquation<Node>::calcCodelength(const InfoNodeBase& parent) const
{
	return parent.isLeafModule()? calcCodelengthOnModuleOfLeafNodes(parent) : calcCodelengthOnModuleOfModules(parent);
}

template<typename Node>
inline
double MapEquation<Node>::calcCodelengthOnModuleOfLeafNodes(const InfoNodeBase& parent) const
{
	double parentFlow = parent.data.flow;
	double parentExit = parent.data.exitFlow;
	double totalParentFlow = parentFlow + parentExit;
	if (totalParentFlow < 1e-16)
		return 0.0;

	double indexLength = 0.0;
	for (const auto& node : parent)
	{
		indexLength -= infomath::plogp(node.data.flow / totalParentFlow);
	}
	indexLength -= infomath::plogp(parentExit / totalParentFlow);

	indexLength *= totalParentFlow;

	return indexLength;
}

template<typename Node>
inline
double MapEquation<Node>::calcCodelengthOnModuleOfModules(const InfoNodeBase& parent) const
{
	double parentFlow = parent.data.flow;
	double parentExit = parent.data.exitFlow;
	if (parentFlow < 1e-16)
		return 0.0;

	// H(x) = -xlog(x), T = q + SUM(p), q = exitFlow, p = enterFlow
	// Normal format
	// L = q * -log(q/T) + p * SUM(-log(p/T))
	// Compact format
	// L = T * ( H(q/T) + SUM( H(p/T) ) )
	// Expanded format
	// L = q * -log(q) - q * -log(T) + SUM( p * -log(p) - p * -log(T) ) = T * log(T) - q*log(q) - SUM( p*log(p) )
	// As T is not known, use expanded format to avoid two loops
	double sumEnter = 0.0;
	double sumEnterLogEnter = 0.0;
	for (const auto& node : parent)
	{
		sumEnter += node.data.enterFlow; // rate of enter to finer level
		sumEnterLogEnter += infomath::plogp(node.data.enterFlow);
	}
	// The possibilities from this module: Either exit to coarser level or enter one of its children
	double totalCodewordUse = parentExit + sumEnter;

	return infomath::plogp(totalCodewordUse) - sumEnterLogEnter - infomath::plogp(parentExit);
}


template<typename Node>
inline
double MapEquation<Node>::getDeltaCodelengthOnMovingNode(InfoNodeBase& current,
		DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData)
{
	using infomath::plogp;
	unsigned int oldModule = oldModuleDelta.module;
	unsigned int newModule = newModuleDelta.module;
	double deltaEnterExitOldModule = oldModuleDelta.deltaEnter + oldModuleDelta.deltaExit;
	double deltaEnterExitNewModule = newModuleDelta.deltaEnter + newModuleDelta.deltaExit;

	double delta_enter = plogp(enterFlow + deltaEnterExitOldModule - deltaEnterExitNewModule) - enterFlow_log_enterFlow;

	double delta_enter_log_enter = \
			- plogp(moduleFlowData[oldModule].enterFlow) \
			- plogp(moduleFlowData[newModule].enterFlow) \
			+ plogp(moduleFlowData[oldModule].enterFlow - current.data.enterFlow + deltaEnterExitOldModule) \
			+ plogp(moduleFlowData[newModule].enterFlow + current.data.enterFlow - deltaEnterExitNewModule);

	double delta_exit_log_exit = \
			- plogp(moduleFlowData[oldModule].exitFlow) \
			- plogp(moduleFlowData[newModule].exitFlow) \
			+ plogp(moduleFlowData[oldModule].exitFlow - current.data.exitFlow + deltaEnterExitOldModule) \
			+ plogp(moduleFlowData[newModule].exitFlow + current.data.exitFlow - deltaEnterExitNewModule);

	double delta_flow_log_flow = \
			- plogp(moduleFlowData[oldModule].exitFlow + moduleFlowData[oldModule].flow) \
			- plogp(moduleFlowData[newModule].exitFlow + moduleFlowData[newModule].flow) \
			+ plogp(moduleFlowData[oldModule].exitFlow + moduleFlowData[oldModule].flow \
					- current.data.exitFlow - current.data.flow + deltaEnterExitOldModule) \
			+ plogp(moduleFlowData[newModule].exitFlow + moduleFlowData[newModule].flow \
					+ current.data.exitFlow + current.data.flow - deltaEnterExitNewModule);

	double deltaL = delta_enter - delta_enter_log_enter - delta_exit_log_exit + delta_flow_log_flow;
	return deltaL;
}

template<typename Node>
inline
void MapEquation<Node>::updateCodelengthOnMovingNode(InfoNodeBase& current,
		DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData)
{
	using infomath::plogp;
	unsigned int oldModule = oldModuleDelta.module;
	unsigned int newModule = newModuleDelta.module;
	double deltaEnterExitOldModule = oldModuleDelta.deltaEnter + oldModuleDelta.deltaExit;
	double deltaEnterExitNewModule = newModuleDelta.deltaEnter + newModuleDelta.deltaExit;

	enterFlow -= \
			moduleFlowData[oldModule].enterFlow + \
			moduleFlowData[newModule].enterFlow;
	enter_log_enter -= \
			plogp(moduleFlowData[oldModule].enterFlow) + \
			plogp(moduleFlowData[newModule].enterFlow);
	exit_log_exit -= \
			plogp(moduleFlowData[oldModule].exitFlow) + \
			plogp(moduleFlowData[newModule].exitFlow);
	flow_log_flow -= \
			plogp(moduleFlowData[oldModule].exitFlow + moduleFlowData[oldModule].flow) + \
			plogp(moduleFlowData[newModule].exitFlow + moduleFlowData[newModule].flow);


	moduleFlowData[oldModule] -= current.data;
	moduleFlowData[newModule] += current.data;

	moduleFlowData[oldModule].enterFlow += deltaEnterExitOldModule;
	moduleFlowData[oldModule].exitFlow += deltaEnterExitOldModule;
	moduleFlowData[newModule].enterFlow -= deltaEnterExitNewModule;
	moduleFlowData[newModule].exitFlow -= deltaEnterExitNewModule;

	enterFlow += \
			moduleFlowData[oldModule].enterFlow + \
			moduleFlowData[newModule].enterFlow;
	enter_log_enter += \
			plogp(moduleFlowData[oldModule].enterFlow) + \
			plogp(moduleFlowData[newModule].enterFlow);
	exit_log_exit += \
			plogp(moduleFlowData[oldModule].exitFlow) + \
			plogp(moduleFlowData[newModule].exitFlow);
	flow_log_flow += \
			plogp(moduleFlowData[oldModule].exitFlow + moduleFlowData[oldModule].flow) + \
			plogp(moduleFlowData[newModule].exitFlow + moduleFlowData[newModule].flow);

	enterFlow_log_enterFlow = plogp(enterFlow);

	indexCodelength = enterFlow_log_enterFlow - enter_log_enter - exitNetworkFlow_log_exitNetworkFlow;
	moduleCodelength = -exit_log_exit + flow_log_flow - nodeFlow_log_nodeFlow;
	codelength = indexCodelength + moduleCodelength;
}


// ===================================================
// Debug
// ===================================================

template<typename Node>
void MapEquation<Node>::printDebug()
{
	std::cout << "(enterFlow_log_enterFlow: " << enterFlow_log_enterFlow << ", " <<
			"enter_log_enter: " << enter_log_enter << ", " <<
			"exitNetworkFlow_log_exitNetworkFlow: " << exitNetworkFlow_log_exitNetworkFlow << ") ";
//	std::cout << "enterFlow_log_enterFlow: " << enterFlow_log_enterFlow << "\n" <<
//			"enter_log_enter: " << enter_log_enter << "\n" <<
//			"exitNetworkFlow_log_exitNetworkFlow: " << exitNetworkFlow_log_exitNetworkFlow << "\n";
//	std::cout << "exit_log_exit: " << exit_log_exit << "\n" <<
//			"flow_log_flow: " << flow_log_flow << "\n" <<
//			"nodeFlow_log_nodeFlow: " << nodeFlow_log_nodeFlow << "\n";
}

}


#endif /* SRC_CLUSTERING_MAPEQUATION_H_ */
