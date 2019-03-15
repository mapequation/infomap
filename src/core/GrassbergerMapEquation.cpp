/*
 * GrassbergerMapEquation.h
 */

#include "GrassbergerMapEquation.h"
#include "FlowData.h"
#include "InfoNode.h"
#include "../utils/Log.h"
#include "../io/Config.h"
#include <vector>
#include <set>
#include <map>
#include <utility>
#include <cstdlib>

namespace infomap {


// ===================================================
// IO
// ===================================================

std::ostream& GrassbergerMapEquation::print(std::ostream& out) const {
	return out << indexCodelength << " + " << moduleCodelength <<
	" = " <<	io::toPrecision(getCodelength());
}

// std::ostream& operator<<(std::ostream& out, const GrassbergerMapEquation& mapEq) {
// 	return out << indexCodelength << " + " << moduleCodelength << " = " <<	io::toPrecision(codelength);
// }


// ===================================================
// Init
// ===================================================

void GrassbergerMapEquation::init(const Config& config)
{
	Log(3) << "GrassbergerMapEquation::init()...\n";
}


void GrassbergerMapEquation::initNetwork(InfoNode& root)
{
	Log(3) << "GrassbergerMapEquation::initNetwork()...\n";
	
	nodeFlow_log_nodeFlow = 0.0;
	m_totalDegree = 0;
	for (InfoNode& node : root)
	{
		m_totalDegree += node.dataInt.flow;
	}
	for (InfoNode& node : root)
	{
		nodeFlow_log_nodeFlow += plogp(node.dataInt.flow);
	}
	initSubNetwork(root);
}

void GrassbergerMapEquation::initSuperNetwork(InfoNode& root)
{
	nodeFlow_log_nodeFlow = 0.0;
	for (InfoNode& node : root)
	{
		nodeFlow_log_nodeFlow += plogp(node.dataInt.enterExitFlow);
	}
}

void GrassbergerMapEquation::initSubNetwork(InfoNode& root)
{
	exitNetworkFlow = root.dataInt.enterExitFlow;
	exitNetworkFlow_log_exitNetworkFlow = plogp(m_exitNetworkFlow);
}

void GrassbergerMapEquation::initPartition(std::vector<InfoNode*>& nodes)
{
	calculateCodelength(nodes);
}


// ===================================================
// Codelength
// ===================================================

double GrassbergerMapEquation::plogp(unsigned int d) const
{
	double p = d * 1.0 / m_totalDegree;
	return infomath::plogp(p);
}

void GrassbergerMapEquation::calculateCodelength(std::vector<InfoNode*>& nodes)
{
	calculateCodelengthTerms(nodes);

	calculateCodelengthFromCodelengthTerms();
}

void GrassbergerMapEquation::calculateCodelengthTerms(std::vector<InfoNode*>& nodes)
{
	enter_log_enter = 0.0;
	flow_log_flow = 0.0;
	exit_log_exit = 0.0;
	enterFlow = 0.0;

	// For each module
	for (InfoNode* n : nodes)
	{
		InfoNode& node = *n;
		// own node/module codebook
		flow_log_flow += plogp(node.dataInt.flow + node.dataInt.enterExitFlow);

		// use of index codebook
		enter_log_enter += plogp(node.dataInt.enterExitFlow);
		exit_log_exit += plogp(node.dataInt.enterExitFlow);
		enterFlow += node.dataInt.enterExitFlow;
	}
	enterFlow += exitNetworkFlow;
	enterFlow_log_enterFlow = plogp(enterFlow);
}

void GrassbergerMapEquation::calculateCodelengthFromCodelengthTerms()
{
	indexCodelength = enterFlow_log_enterFlow - enter_log_enter - exitNetworkFlow_log_exitNetworkFlow;
	moduleCodelength = -exit_log_exit + flow_log_flow - nodeFlow_log_nodeFlow;
	// indexCodelength = 1.0 / m_totalDegree * (m_enterFlow_log_enterFlow - m_enter_log_enter);
	// moduleCodelength = 1.0 / m_totalDegree * (-m_exit_log_exit + m_flow_log_flow - m_nodeFlow_log_nodeFlow);
	codelength = indexCodelength + moduleCodelength;
}

double GrassbergerMapEquation::calcCodelength(const InfoNode& parent) const
{
	return parent.isLeafModule() ?
		calcCodelengthOnModuleOfLeafNodes(parent) :
		calcCodelengthOnModuleOfModules(parent);
}

double GrassbergerMapEquation::calcCodelengthOnModuleOfLeafNodes(const InfoNode& parent) const
{
	unsigned int parentFlow = parent.dataInt.flow;
	unsigned int parentExit = parent.dataInt.enterExitFlow;
	unsigned int totalParentFlow = parentFlow + parentExit;
	if (totalParentFlow == 0)
		return 0.0;

	double indexLength = 0.0;
	for (const auto& node : parent)
	{
		indexLength -= infomath::plogpN(node.dataInt.flow, totalParentFlow);
	}
	indexLength -= infomath::plogpN(parentExit, totalParentFlow);

	indexLength *= totalParentFlow * 1.0 / m_totalDegree;

	return indexLength;
}

double GrassbergerMapEquation::calcCodelengthOnModuleOfModules(const InfoNode& parent) const
{
	unsigned int parentFlow = parent.dataInt.flow;
	unsigned int parentExit = parent.dataInt.enterExitFlow;
	// unsigned int totalParentFlow = parentFlow + parentExit;
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
	for (const auto& node : parent)
	{
		sumEnter += node.dataInt.enterExitFlow; // rate of enter to finer level
		sumEnterLogEnter += plogp(node.dataInt.enterExitFlow);
	}
	// The possibilities from this module: Either exit to coarser level or enter one of its children
	unsigned int totalCodewordUse = parentExit + sumEnter;

	// return plogp(totalCodewordUse) - sumEnterLogEnter - plogp(parentExit);
	double L = plogp(totalCodewordUse) - sumEnterLogEnter - plogp(parentExit);

	// Log() << "\nL: " << plogp(totalCodewordUse) << " - " << sumEnterLogEnter << " - " << plogp(parentExit) << " = " << L;
	return L;
}

double GrassbergerMapEquation::getDeltaCodelengthOnMovingNode(InfoNode& current,
		DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData, std::vector<unsigned int>& moduleMembers)
{
	// double deltaL = Base::getDeltaCodelengthOnMovingNode(current, oldModuleDelta, newModuleDelta, moduleFlowData, moduleMembers);
	// using infomath::plogp;
	unsigned int oldModule = oldModuleDelta.module;
	unsigned int newModule = newModuleDelta.module;
	double deltaEnterExitOldModule = oldModuleDelta.deltaEnter + oldModuleDelta.deltaExit;
	double deltaEnterExitNewModule = newModuleDelta.deltaEnter + newModuleDelta.deltaExit;

	double delta_enter = plogp(enterFlow + deltaEnterExitOldModule - deltaEnterExitNewModule) - enterFlow_log_enterFlow;

	double delta_enter_log_enter = \
			- plogp(moduleFlowData[oldModule].enterExitFlow) \
			- plogp(moduleFlowData[newModule].enterExitFlow) \
			+ plogp(moduleFlowData[oldModule].enterExitFlow - current.dataInt.enterExitFlow + deltaEnterExitOldModule) \
			+ plogp(moduleFlowData[newModule].enterExitFlow + current.dataInt.enterExitFlow - deltaEnterExitNewModule);

	double delta_exit_log_exit = \
			- plogp(moduleFlowData[oldModule].enterExitFlow) \
			- plogp(moduleFlowData[newModule].enterExitFlow) \
			+ plogp(moduleFlowData[oldModule].enterExitFlow - current.dataInt.enterExitFlow + deltaEnterExitOldModule) \
			+ plogp(moduleFlowData[newModule].enterExitFlow + current.dataInt.enterExitFlow - deltaEnterExitNewModule);

	double delta_flow_log_flow = \
			- plogp(moduleFlowData[oldModule].enterExitFlow + moduleFlowData[oldModule].flow) \
			- plogp(moduleFlowData[newModule].enterExitFlow + moduleFlowData[newModule].flow) \
			+ plogp(moduleFlowData[oldModule].enterExitFlow + moduleFlowData[oldModule].flow \
					- current.dataInt.enterExitFlow - current.data.flow + deltaEnterExitOldModule) \
			+ plogp(moduleFlowData[newModule].enterExitFlow + moduleFlowData[newModule].flow \
					+ current.dataInt.enterExitFlow + current.data.flow - deltaEnterExitNewModule);

	double deltaL = delta_enter - delta_enter_log_enter - delta_exit_log_exit + delta_flow_log_flow;
	return deltaL;
}


// ===================================================
// Consolidation
// ===================================================

void GrassbergerMapEquation::updateCodelengthOnMovingNode(InfoNode& current,
		DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData, std::vector<unsigned int>& moduleMembers)
{
	// using infomath::plogp;
	unsigned int oldModule = oldModuleDelta.module;
	unsigned int newModule = newModuleDelta.module;
	double deltaEnterExitOldModule = oldModuleDelta.deltaEnter + oldModuleDelta.deltaExit;
	double deltaEnterExitNewModule = newModuleDelta.deltaEnter + newModuleDelta.deltaExit;

	enterFlow -= \
			moduleFlowData[oldModule].enterExitFlow + \
			moduleFlowData[newModule].enterExitFlow;
	enter_log_enter -= \
			plogp(moduleFlowData[oldModule].enterExitFlow) + \
			plogp(moduleFlowData[newModule].enterExitFlow);
	exit_log_exit -= \
			plogp(moduleFlowData[oldModule].enterExitFlow) + \
			plogp(moduleFlowData[newModule].enterExitFlow);
	flow_log_flow -= \
			plogp(moduleFlowData[oldModule].enterExitFlow + moduleFlowData[oldModule].flow) + \
			plogp(moduleFlowData[newModule].enterExitFlow + moduleFlowData[newModule].flow);


	moduleFlowData[oldModule] -= current.dataInt;
	moduleFlowData[newModule] += current.dataInt;

	moduleFlowData[oldModule].enterExitFlow += deltaEnterExitOldModule;
	moduleFlowData[oldModule].enterExitFlow += deltaEnterExitOldModule;
	moduleFlowData[newModule].enterExitFlow -= deltaEnterExitNewModule;
	moduleFlowData[newModule].enterExitFlow -= deltaEnterExitNewModule;

	enterFlow += \
			moduleFlowData[oldModule].enterExitFlow + \
			moduleFlowData[newModule].enterExitFlow;
	enter_log_enter += \
			plogp(moduleFlowData[oldModule].enterExitFlow) + \
			plogp(moduleFlowData[newModule].enterExitFlow);
	exit_log_exit += \
			plogp(moduleFlowData[oldModule].enterExitFlow) + \
			plogp(moduleFlowData[newModule].enterExitFlow);
	flow_log_flow += \
			plogp(moduleFlowData[oldModule].enterExitFlow + moduleFlowData[oldModule].flow) + \
			plogp(moduleFlowData[newModule].enterExitFlow + moduleFlowData[newModule].flow);

	enterFlow_log_enterFlow = plogp(enterFlow);

	indexCodelength = enterFlow_log_enterFlow - enter_log_enter - exitNetworkFlow_log_exitNetworkFlow;
	moduleCodelength = -exit_log_exit + flow_log_flow - nodeFlow_log_nodeFlow;
	codelength = indexCodelength + moduleCodelength;
	// Log() << "\nL_index = " << enterFlow_log_enterFlow << " - " << enter_log_enter << " = " << indexCodelength;
	// Log() << "\nL_mod = " << exit_log_exit << " + " << flow_log_flow << " - " << nodeFlow_log_nodeFlow << " = " << moduleCodelength;
	// Log() << "\n=> L = " << codelength << "\n============= ";
	return;	
}


void GrassbergerMapEquation::consolidateModules(std::vector<InfoNode*>& modules)
{
}



// ===================================================
// Debug
// ===================================================

void GrassbergerMapEquation::printDebug()
{
	std::cout << "GrassbergerMapEquation\n";
	Base::printDebug();
}


}
