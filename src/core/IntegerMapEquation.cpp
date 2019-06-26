/*
 * IntegerMapEquation.h
 */

#include "IntegerMapEquation.h"
#include "FlowData.h"
#include "NodeBase.h"
#include "Node.h"
#include "../utils/Log.h"
#include "../io/Config.h"
#include <vector>
#include <set>
#include <map>
#include <utility>
#include <cstdlib>
#include <cmath>

namespace infomap {

	using NodeType = Node<FlowDataInt>;

// ===================================================
// IO
// ===================================================

std::ostream& IntegerMapEquation::print(std::ostream& out) const {
	return out << indexCodelength << " + " << moduleCodelength <<
	" = " <<	io::toPrecision(getCodelength());
}

// std::ostream& operator<<(std::ostream& out, const IntegerMapEquation& mapEq) {
// 	return out << indexCodelength << " + " << moduleCodelength << " = " <<	io::toPrecision(codelength);
// }


// ===================================================
// Init
// ===================================================

void IntegerMapEquation::init(const Config& config)
{
	Log(3) << "IntegerMapEquation::init()...\n";
}


void IntegerMapEquation::initNetwork(NodeBase& root)
{
	Log(3) << "IntegerMapEquation::initNetwork()...\n";

	nodeFlow_log_nodeFlow = 0.0;
	m_totalDegree = 0.0;
        m_totalDegreePrior = 0.0;
	m_totalNodes = 0;
	for (NodeBase& node : root)
	{
		m_totalDegree += getNode(node).data.flow;
		m_totalNodes++;
	}
	m_prior = log(m_totalNodes);
        m_totalDegreePrior = m_totalDegree + m_totalNodes * m_prior;

	for (NodeBase& node : root)
	{
		nodeFlow_log_nodeFlow += plogp(getNode(node).data.flow + m_prior);
	}
	initSubNetwork(root);
}

void IntegerMapEquation::initSuperNetwork(NodeBase& root)
{
	nodeFlow_log_nodeFlow = 0.0;
	for (NodeBase& node : root)
	{
		nodeFlow_log_nodeFlow += plogp(getNode(node).data.enterExitFlow);
	}
}

void IntegerMapEquation::initSubNetwork(NodeBase& root)
{
	exitNetworkFlow = getNode(root).data.enterExitFlow;
	exitNetworkFlow_log_exitNetworkFlow = plogp(m_exitNetworkFlow);
}

void IntegerMapEquation::initPartition(std::vector<NodeBase*>& nodes)
{
	calculateCodelength(nodes);
}


// ===================================================
// Codelength
// ===================================================

double IntegerMapEquation::plogp(double d) const
{
	double p = d * 1.0 / m_totalDegree;
	return infomath::plogp(p);
}

double IntegerMapEquation::plogpN(double d, double N) const
{
	return infomath::plogpN(d, N);
}

void IntegerMapEquation::calculateCodelength(std::vector<NodeBase*>& nodes)
{
	calculateCodelengthTerms(nodes);

	calculateCodelengthFromCodelengthTerms();
}

void IntegerMapEquation::calculateCodelengthTerms(std::vector<NodeBase*>& nodes)
{
	enter_log_enter = 0.0;
	flow_log_flow = 0.0;
	exit_log_exit = 0.0;
	enterFlow = 0.0;

	// For each module
	for (NodeBase* n : nodes)
	{
		auto& node = getNode(*n);
		unsigned int moduleSize = node.childDegree();
		double eta = moduleSize * (m_totalNodes - moduleSize) / (m_totalNodes - 1.0);
		// own node/module codebook
		flow_log_flow += plogp(node.data.flow + node.data.enterExitFlow + m_prior * (moduleSize + eta));

		// use of index codebook
		enter_log_enter += plogp(node.data.enterExitFlow + m_prior * eta);
		exit_log_exit += plogp(node.data.enterExitFlow + m_prior * eta);
		enterFlow += node.data.enterExitFlow + m_prior * eta;
	}
	//enterFlow += exitNetworkFlow; ????
	enterFlow_log_enterFlow = plogp(enterFlow);
}

void IntegerMapEquation::calculateCodelengthFromCodelengthTerms()
{
	indexCodelength = enterFlow_log_enterFlow - enter_log_enter - exitNetworkFlow_log_exitNetworkFlow;
	moduleCodelength = -exit_log_exit + flow_log_flow - nodeFlow_log_nodeFlow;
	// indexCodelength = 1.0 / m_totalDegree * (m_enterFlow_log_enterFlow - m_enter_log_enter);
	// moduleCodelength = 1.0 / m_totalDegree * (-m_exit_log_exit + m_flow_log_flow - m_nodeFlow_log_nodeFlow);
	codelength = indexCodelength + moduleCodelength;
}

double IntegerMapEquation::calcCodelength(const NodeBase& parent) const
{
	return parent.isLeafModule() ?
		calcCodelengthOnModuleOfLeafNodes(parent) :
		calcCodelengthOnModuleOfModules(parent);
}

double IntegerMapEquation::calcCodelengthOnModuleOfLeafNodes(const NodeBase& p) const
{
	auto& parent = getNode(p);

	unsigned int moduleSize = parent.childDegree();
	double eta = moduleSize * (m_totalNodes - moduleSize) / (m_totalNodes - 1.0);

	double parentFlow = parent.data.flow + m_prior * moduleSize;
	double parentExit = parent.data.enterExitFlow + m_prior * eta;
	double totalParentFlow = parentFlow + parentExit;
	if (totalParentFlow == 0)
		return 0.0;

	double indexLength = 0.0;
	for (const auto& node : parent)
	{
		indexLength -= plogpN(getNode(node).data.flow + m_prior, totalParentFlow);
	}
	indexLength -= plogpN(parentExit, totalParentFlow);

	indexLength *= totalParentFlow * 1.0 / (m_totalDegreePrior);

	return indexLength;
}

double IntegerMapEquation::calcCodelengthOnModuleOfModules(const NodeBase& p) const
{
	auto& parent = getNode(p);
	unsigned int parentFlow = parent.data.flow;
	double parentExit = parent.data.enterExitFlow;
	//unsigned int parentModuleSize = parent.childDegree();
	if (parentFlow == 0)
		return 0.0;

	// H(x) = -xlog(x), T = q + SUM(p), q = exitFlow, p = enterFlow
	// Normal format
	// L = q * -log(q/T) + p * SUM(-log(p/T))
	// Compact format
	// L = T * ( H(q/T) + SUM( H(p/T) ) )
	// Expanded format
	// L = q * -log(q) - q * -log(T) + SUM( p * -log(p) - p * -log(T) )
	//   = T * log(T) - q*log(q) - SUM( p*log(p) )
	// As T is not known, use expanded format to avoid two loops
	double sumEnter = 0.0;
	double sumEnterLogEnter = 0.0;

	for (const auto& n : parent)
	{
		auto& node = getNode(n);
		unsigned int moduleSize = node.childDegree();
		double eta = moduleSize * (m_totalNodes - moduleSize) / (m_totalNodes - 1.0);
		sumEnter += node.data.enterExitFlow + m_prior * eta; // rate of enter to finer level
		sumEnterLogEnter += plogp(node.data.enterExitFlow + m_prior * eta);
	}
	//double parentEta = parentModuleSize * (m_totalNodes - parentModuleSize) / (m_totalNodes - 1.0);
	//parentExit += m_prior * parentEta;
	// The possibilities from this module: Either exit to coarser level or enter one of its children
	double totalCodewordUse = parentExit + sumEnter;

	// return plogp(totalCodewordUse) - sumEnterLogEnter - plogp(parentExit);
	double L = plogp(totalCodewordUse) - sumEnterLogEnter - plogp(parentExit);

	// Log() << "\nL: " << plogp(totalCodewordUse) << " - " << sumEnterLogEnter << " - " << plogp(parentExit) << " = " << L;
	return L;
}

double IntegerMapEquation::getDeltaCodelengthOnMovingNode(NodeBase& curr,
		DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData, std::vector<unsigned int>& moduleMembers)
{
	auto& current = getNode(curr);
	// double deltaL = Base::getDeltaCodelengthOnMovingNode(current, oldModuleDelta, newModuleDelta, moduleFlowData, moduleMembers);
	// using infomath::plogp;
	unsigned int oldModule = oldModuleDelta.module;
	unsigned int sizeOldModule = moduleMembers[oldModule];
	double etaOldModule = sizeOldModule * (m_totalNodes - sizeOldModule) / (m_totalNodes - 1.0);
	double deltaEtaOldModule = (2.0 * sizeOldModule - m_totalNodes - 1.0) / (m_totalNodes - 1.0);
	double deltaEnterExitOldModule = oldModuleDelta.deltaEnter + oldModuleDelta.deltaExit;

	unsigned int newModule = newModuleDelta.module;
	unsigned int sizeNewModule = moduleMembers[newModule];
	double etaNewModule = sizeNewModule * (m_totalNodes - sizeNewModule) / (m_totalNodes - 1.0);
	double deltaEtaNewModule = (m_totalNodes - 2.0 * sizeNewModule - 1.0) / (m_totalNodes - 1.0);
	double deltaEnterExitNewModule = newModuleDelta.deltaEnter + newModuleDelta.deltaExit;

	// add to both enter and exit flow
	deltaEnterExitOldModule *= 2;
	deltaEnterExitNewModule *= 2;

	double delta_enter = plogp(enterFlow + deltaEnterExitOldModule - deltaEnterExitNewModule + m_prior * (deltaEtaOldModule + deltaEtaNewModule)) - enterFlow_log_enterFlow; //??????

	double delta_enter_log_enter = \
			- plogp(moduleFlowData[oldModule].enterExitFlow + m_prior * etaOldModule) \
			- plogp(moduleFlowData[newModule].enterExitFlow + m_prior * etaNewModule) \
			+ plogp(moduleFlowData[oldModule].enterExitFlow - current.data.enterExitFlow + deltaEnterExitOldModule + m_prior * (etaOldModule + deltaEtaOldModule)) \
			+ plogp(moduleFlowData[newModule].enterExitFlow + current.data.enterExitFlow - deltaEnterExitNewModule + m_prior * (etaNewModule + deltaEtaNewModule));

	double delta_exit_log_exit = \
			- plogp(moduleFlowData[oldModule].enterExitFlow + m_prior * etaOldModule) \
			- plogp(moduleFlowData[newModule].enterExitFlow + m_prior * etaNewModule) \
			+ plogp(moduleFlowData[oldModule].enterExitFlow - current.data.enterExitFlow + deltaEnterExitOldModule + m_prior * (etaOldModule + deltaEtaOldModule)) \
			+ plogp(moduleFlowData[newModule].enterExitFlow + current.data.enterExitFlow - deltaEnterExitNewModule + m_prior * (etaNewModule + deltaEtaNewModule));

	double delta_flow_log_flow = \
			- plogp(moduleFlowData[oldModule].enterExitFlow + moduleFlowData[oldModule].flow + m_prior * (sizeOldModule + etaOldModule)) \
			- plogp(moduleFlowData[newModule].enterExitFlow + moduleFlowData[newModule].flow + m_prior * (sizeNewModule + etaNewModule)) \
			+ plogp(moduleFlowData[oldModule].enterExitFlow + moduleFlowData[oldModule].flow \
					- current.data.enterExitFlow - current.data.flow + deltaEnterExitOldModule + m_prior * (sizeOldModule - 1.0 + etaOldModule + deltaEtaOldModule)) \
			+ plogp(moduleFlowData[newModule].enterExitFlow + moduleFlowData[newModule].flow \
					+ current.data.enterExitFlow + current.data.flow - deltaEnterExitNewModule + m_prior * (sizeNewModule + 1.0 + etaNewModule + deltaEtaNewModule));

	double deltaL = delta_enter - delta_enter_log_enter - delta_exit_log_exit + delta_flow_log_flow;

	// Log() << "\ndeltaL = " << delta_enter << " - " << delta_enter_log_enter << " - " <<
	// delta_exit_log_exit << " + " << delta_flow_log_flow << " = " << deltaL;
	return deltaL;
}


// ===================================================
// Consolidation
// ===================================================

void IntegerMapEquation::updateCodelengthOnMovingNode(NodeBase& curr,
		DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData, std::vector<unsigned int>& moduleMembers)
{
	auto& current = getNode(curr);
	// using infomath::plogp;
	unsigned int oldModule = oldModuleDelta.module;
	unsigned int sizeOldModule = moduleMembers[oldModule];
	double etaOldModule = sizeOldModule * (m_totalNodes - sizeOldModule) / (m_totalNodes - 1.0);
	double deltaEtaOldModule = (2.0 * sizeOldModule - m_totalNodes - 1.0) / (m_totalNodes - 1.0);
	double deltaEnterExitOldModule = oldModuleDelta.deltaEnter + oldModuleDelta.deltaExit;

	unsigned int newModule = newModuleDelta.module;
	unsigned int sizeNewModule = moduleMembers[newModule];
	double etaNewModule = sizeNewModule * (m_totalNodes - sizeNewModule) / (m_totalNodes - 1.0);
	double deltaEtaNewModule = (m_totalNodes - 2.0 * sizeNewModule - 1.0) / (m_totalNodes - 1.0);
	double deltaEnterExitNewModule = newModuleDelta.deltaEnter + newModuleDelta.deltaExit;

	enterFlow -= \
			moduleFlowData[oldModule].enterExitFlow + m_prior * etaOldModule + \
			moduleFlowData[newModule].enterExitFlow + m_prior * etaNewModule;
	enter_log_enter -= \
			plogp(moduleFlowData[oldModule].enterExitFlow + m_prior * etaOldModule) + \
			plogp(moduleFlowData[newModule].enterExitFlow + m_prior * etaNewModule);
	exit_log_exit -= \
			plogp(moduleFlowData[oldModule].enterExitFlow + m_prior * etaOldModule) + \
			plogp(moduleFlowData[newModule].enterExitFlow + m_prior * etaNewModule);
	flow_log_flow -= \
			plogp(moduleFlowData[oldModule].enterExitFlow + moduleFlowData[oldModule].flow + m_prior * (sizeOldModule + etaOldModule)) + \
			plogp(moduleFlowData[newModule].enterExitFlow + moduleFlowData[newModule].flow + m_prior * (sizeNewModule + etaNewModule));


	moduleFlowData[oldModule] -= current.data;
	moduleFlowData[newModule] += current.data;

	moduleFlowData[oldModule].enterExitFlow += deltaEnterExitOldModule;
	moduleFlowData[oldModule].enterExitFlow += deltaEnterExitOldModule;
	moduleFlowData[newModule].enterExitFlow -= deltaEnterExitNewModule;
	moduleFlowData[newModule].enterExitFlow -= deltaEnterExitNewModule;

	moduleFlowData[oldModule].moduleSize -= 1;
	moduleFlowData[newModule].moduleSize += 1;

	enterFlow += \
			moduleFlowData[oldModule].enterExitFlow + m_prior * (etaOldModule + deltaEtaOldModule) + \
			moduleFlowData[newModule].enterExitFlow + m_prior * (etaNewModule + deltaEtaNewModule);
	enter_log_enter += \
			plogp(moduleFlowData[oldModule].enterExitFlow + m_prior * (etaOldModule + deltaEtaOldModule)) + \
			plogp(moduleFlowData[newModule].enterExitFlow + m_prior * (etaNewModule + deltaEtaNewModule));
	exit_log_exit += \
			plogp(moduleFlowData[oldModule].enterExitFlow + m_prior * (etaOldModule + deltaEtaOldModule)) + \
			plogp(moduleFlowData[newModule].enterExitFlow + m_prior * (etaNewModule + deltaEtaNewModule));
	flow_log_flow += \
			plogp(moduleFlowData[oldModule].enterExitFlow + moduleFlowData[oldModule].flow + m_prior * (sizeOldModule - 1.0 + etaOldModule + deltaEtaOldModule)) + \
			plogp(moduleFlowData[newModule].enterExitFlow + moduleFlowData[newModule].flow + m_prior * (sizeNewModule + 1.0 + etaNewModule + deltaEtaNewModule));

	enterFlow_log_enterFlow = plogp(enterFlow);

	indexCodelength = enterFlow_log_enterFlow - enter_log_enter - exitNetworkFlow_log_exitNetworkFlow;
	moduleCodelength = -exit_log_exit + flow_log_flow - nodeFlow_log_nodeFlow;
	codelength = indexCodelength + moduleCodelength;
	// Log() << "\nL_index = " << enterFlow_log_enterFlow << " - " << enter_log_enter << " = " << indexCodelength;
	// Log() << "\nL_mod = " << exit_log_exit << " + " << flow_log_flow << " - " << nodeFlow_log_nodeFlow << " = " << moduleCodelength;
	// Log() << "\n=> L = " << codelength << "\n============= ";
	return;
}


void IntegerMapEquation::consolidateModules(std::vector<NodeBase*>& modules)
{
}

NodeBase* IntegerMapEquation::createNode() const
{
	return new NodeType();
}

NodeBase* IntegerMapEquation::createNode(const NodeBase& other) const
{
	return new NodeType(static_cast<const NodeType&>(other));
}

NodeBase* IntegerMapEquation::createNode(FlowDataType flowData) const
{
	return new NodeType(flowData);
}

const NodeType& IntegerMapEquation::getNode(const NodeBase& other) const
{
	return static_cast<const NodeType&>(other);
}

// ===================================================
// Debug
// ===================================================

void IntegerMapEquation::printDebug()
{
	std::cout << "IntegerMapEquation\n";
	Base::printDebug();
}


}
