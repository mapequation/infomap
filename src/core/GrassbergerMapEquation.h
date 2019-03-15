/*
 * GrassbergerMapEquation.h
 */

#ifndef _GRASSBERGERMAPEQUATION_H_
#define _GRASSBERGERMAPEQUATION_H_

#include <vector>
#include <set>
#include <map>
#include <utility>
#include "MapEquation.h"
#include "FlowData.h"
// #include "InfoNode.h"
#include "../utils/Log.h"

namespace infomap {

class InfoNode;

class GrassbergerMapEquation : protected MapEquation {
	using Base = MapEquation;
public:
	using FlowDataType = FlowDataInt;
	using DeltaFlowDataType = DeltaFlowInt;

	GrassbergerMapEquation() : MapEquation() {}

	GrassbergerMapEquation(const GrassbergerMapEquation& other)
	:	MapEquation(other)
	{}

	GrassbergerMapEquation& operator=(const GrassbergerMapEquation& other) {
		Base::operator =(other);
		return *this;
	}

	virtual ~GrassbergerMapEquation() {}

	// ===================================================
	// Getters
	// ===================================================

	static bool haveMemory() { return true; }

	using Base::getIndexCodelength;
	using Base::getModuleCodelength;
	using Base::getCodelength;

	// double getModuleCodelength() const { return moduleCodelength; };
	// double getModuleCodelength() const;

	// double getCodelength() const { return codelength; };
	// double getCodelength() const;

	// ===================================================
	// IO
	// ===================================================

	// using Base::print;
	std::ostream& print(std::ostream& out) const;
	// friend std::ostream& operator<<(std::ostream&, const GrassbergerMapEquation&);

	// ===================================================
	// Init
	// ===================================================

	void init(const Config& config);

	void initNetwork(InfoNode& root);

	void initSuperNetwork(InfoNode& root);

	void initSubNetwork(InfoNode& root);

	void initPartition(std::vector<InfoNode*>& nodes);

	// ===================================================
	// Codelength
	// ===================================================

	double calcCodelength(const InfoNode& parent) const;
	
	void addMemoryContributions(InfoNode& current, DeltaFlowDataType& oldModuleDelta, VectorMap<DeltaFlowDataType>& moduleDeltaFlow) {}

	double getDeltaCodelengthOnMovingNode(InfoNode& current,
			DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData, std::vector<unsigned int>& moduleMembers);

	// ===================================================
	// Consolidation
	// ===================================================

	void updateCodelengthOnMovingNode(InfoNode& current,
			DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData, std::vector<unsigned int>& moduleMembers);

	void consolidateModules(std::vector<InfoNode*>& modules);

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

	int getDeltaNumModulesIfMoving(InfoNode& current,
			unsigned int oldModule, unsigned int newModule, std::vector<unsigned int>& moduleMembers) const;

	// ===================================================
	// Init
	// ===================================================

	// void initGrassbergerNodes(InfoNode& root);

	// void initPartitionOfGrassbergerNodes(std::vector<InfoNode*>& nodes);

	// ===================================================
	// Codelength
	// ===================================================

	// Integer version, normalized using m_totalDegree
	double plogp(unsigned int d) const;

	void calculateCodelength(std::vector<InfoNode*>& nodes);

	void calculateCodelengthTerms(std::vector<InfoNode*>& nodes);

	void calculateCodelengthFromCodelengthTerms();

	// ===================================================
	// Consolidation
	// ===================================================

	// void updateGrassbergerData(InfoNode& current, unsigned int oldModuleIndex, unsigned int bestModuleIndex);

public:
	// ===================================================
	// Public member variables
	// ===================================================

	using Base::codelength;
	using Base::indexCodelength;
	using Base::moduleCodelength;

protected:
	// ===================================================
	// Protected member functions
	// ===================================================


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

	// For Grassberger
	unsigned int m_totalDegree = 0;
	double m_nodeFlow_log_nodeFlow = 0.0; // constant while the leaf network is the same
	double m_flow_log_flow = 0.0; // node.(flow + exitFlow)
	double m_exit_log_exit = 0.0;
	double m_enter_log_enter = 0.0;
	double m_enterFlow = 0.0;
	double m_enterFlow_log_enterFlow = 0.0;
	double m_exitNetworkFlow = 0.0;
	double m_exitNetworkFlow_log_exitNetworkFlow = 0.0;
};



}

#endif /* _GRASSBERGERMAPEQUATION_H_ */
