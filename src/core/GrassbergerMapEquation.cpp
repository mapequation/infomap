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
	Base::initNetwork(root);
}

void GrassbergerMapEquation::initSuperNetwork(InfoNode& root)
{
	Base::initSuperNetwork(root);
}

void GrassbergerMapEquation::initSubNetwork(InfoNode& root)
{
	Base::initSubNetwork(root);
}

void GrassbergerMapEquation::initPartition(std::vector<InfoNode*>& nodes)
{
	calculateCodelength(nodes);
}


// ===================================================
// Codelength
// ===================================================

void GrassbergerMapEquation::calculateCodelength(std::vector<InfoNode*>& nodes)
{
	calculateCodelengthTerms(nodes);

	calculateCodelengthFromCodelengthTerms();
}

double GrassbergerMapEquation::calcCodelength(const InfoNode& parent) const
{
	return parent.isLeafModule() ?
		calcCodelengthOnModuleOfLeafNodes(parent) :
		// Use first-order model on index codebook
		MapEquation::calcCodelengthOnModuleOfModules(parent);
}

double GrassbergerMapEquation::calcCodelengthOnModuleOfLeafNodes(const InfoNode& parent) const
{
	double indexLength = MapEquation::calcCodelength(parent);

	return indexLength;
}

double GrassbergerMapEquation::getDeltaCodelengthOnMovingNode(InfoNode& current,
		DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData, std::vector<unsigned int>& moduleMembers)
{
	double deltaL = Base::getDeltaCodelengthOnMovingNode(current, oldModuleDelta, newModuleDelta, moduleFlowData, moduleMembers);

	return deltaL;
}


// ===================================================
// Consolidation
// ===================================================

void GrassbergerMapEquation::updateCodelengthOnMovingNode(InfoNode& current,
		DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData, std::vector<unsigned int>& moduleMembers)
{
	Base::updateCodelengthOnMovingNode(current, oldModuleDelta, newModuleDelta, moduleFlowData, moduleMembers);

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
