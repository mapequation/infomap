/*
 * IMapEquation.h
 *
 *  Created on: 25 feb 2015
 *      Author: Daniel
 */

#ifndef CORE_IMAPEQUATION_H_
#define CORE_IMAPEQUATION_H_

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

class IMapEquation {
public:
	using FlowDataType = FlowData;
	using DeltaFlowDataType = MemDeltaFlow;
	// using DeltaFlowDataType = DeltaFlow;

	virtual ~IMapEquation() {}

	// ===================================================
	// Getters
	// ===================================================

	virtual bool haveMemory() = 0;

	// ===================================================
	// IO
	// ===================================================

	virtual std::ostream& print(std::ostream&) const = 0;

	// friend std::ostream& operator<<(std::ostream&, const IMapEquation&);


	// ===================================================
	// Init
	// ===================================================

	virtual void initNetwork(InfoNode& root) = 0;

	virtual void initSuperNetwork(InfoNode& root) = 0;

	virtual void initSubNetwork(InfoNode& root) = 0;

	virtual void initPartition(std::vector<InfoNode*>& nodes) = 0;

	// ===================================================
	// Codelength
	// ===================================================

	virtual double calcCodelength(const InfoNode& parent) const = 0;

	virtual void addMemoryContributions(InfoNode& current, DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta) = 0;

	virtual void addMemoryContributions(InfoNode& current, DeltaFlowDataType& oldModuleDelta, VectorMap<DeltaFlowDataType>& moduleDeltaFlow) = 0;

	virtual double getDeltaCodelengthOnMovingNode(InfoNode& current,
			DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData) = 0;

	// ===================================================
	// Consolidation
	// ===================================================

	virtual void updateCodelengthOnMovingNode(InfoNode& current,
			DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData) = 0;

	virtual void consolidateModules(std::vector<InfoNode*>& modules) = 0;


// protected:
// 	// ===================================================
// 	// Protected member functions
// 	// ===================================================

// 	virtual double calcCodelengthOnModuleOfLeafNodes(const InfoNode& parent) const = 0;

// 	virtual double calcCodelengthOnModuleOfModules(const InfoNode& parent) const = 0;

// 	virtual void calculateCodelength(std::vector<InfoNode*>& nodes) = 0;

// 	virtual void calculateCodelengthTerms(std::vector<InfoNode*>& nodes) = 0;

// 	virtual void calculateCodelengthFromCodelengthTerms() = 0;

};

}


#endif /* SRC_CLUSTERING_MAPEQUATION_H_ */
