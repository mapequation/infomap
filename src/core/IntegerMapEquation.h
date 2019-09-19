/*
 * IntegerMapEquation.h
 */

#ifndef _INTEGER_MAP_EQUATION_H_
#define _INTEGER_MAP_EQUATION_H_

#include <vector>
#include <set>
#include <map>
#include <utility>
#include "MapEquation.h"
#include "FlowData.h"
// #include "NodeBase.h"
#include "../utils/Log.h"

namespace infomap {

class NodeBase;
template<class FlowDataType>
class Node;

class IntegerMapEquation : protected MapEquation {
	using Base = MapEquation;
public:
	using FlowDataType = FlowDataInt;
	using DeltaFlowDataType = DeltaFlowInt;
	using NodeType = Node<FlowDataType>;

	IntegerMapEquation() : MapEquation() {}

	IntegerMapEquation(const IntegerMapEquation& other)
	:	MapEquation(other)
	{}

	IntegerMapEquation& operator=(const IntegerMapEquation& other) {
		Base::operator =(other);
		return *this;
	}

	virtual ~IntegerMapEquation() {}

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
	// friend std::ostream& operator<<(std::ostream&, const IntegerMapEquation&);

	// ===================================================
	// Init
	// ===================================================

	void init(const Config& config);

	virtual void initNetwork(NodeBase& root);

	void initSuperNetwork(NodeBase& root);

	void initSubNetwork(NodeBase& root);

	void initPartition(std::vector<NodeBase*>& nodes);

	// ===================================================
	// Codelength
	// ===================================================

	double calcCodelength(const NodeBase& parent) const;
	
	void addMemoryContributions(NodeBase& current, DeltaFlowDataType& oldModuleDelta, VectorMap<DeltaFlowDataType>& moduleDeltaFlow) {}

	double getDeltaCodelengthOnMovingNode(NodeBase& current,
			DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData, std::vector<unsigned int>& moduleMembers);

	// ===================================================
	// Consolidation
	// ===================================================

	void updateCodelengthOnMovingNode(NodeBase& current,
			DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData, std::vector<unsigned int>& moduleMembers);

	void consolidateModules(std::vector<NodeBase*>& modules);

	NodeBase* createNode() const;
	NodeBase* createNode(const NodeBase&) const;
	NodeBase* createNode(FlowDataType) const;
	const NodeType& getNode(const NodeBase&) const;

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

	int getDeltaNumModulesIfMoving(NodeBase& current,
			unsigned int oldModule, unsigned int newModule, std::vector<unsigned int>& moduleMembers) const;

	// ===================================================
	// Init
	// ===================================================

	// void initGrassbergerNodes(NodeBase& root);

	// void initPartitionOfGrassbergerNodes(std::vector<NodeBase*>& nodes);

	// ===================================================
	// Codelength
	// ===================================================

	// Integer version, normalized using p = d/m_totalDegree
	virtual double plogp(double d) const;
	// p = d/N
	virtual double plogpN(double d, double N) const;

	void calculateCodelength(std::vector<NodeBase*>& nodes);

	void calculateCodelengthTerms(std::vector<NodeBase*>& nodes);

	void calculateCodelengthFromCodelengthTerms();

	// ===================================================
	// Consolidation
	// ===================================================

	// void updateGrassbergerData(NodeBase& current, unsigned int oldModuleIndex, unsigned int bestModuleIndex);

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

	// For Grassberger and Bayes
	double m_totalDegree = 0.0;
	double m_totalDegreePrior = 0.0;
	double m_totalDegreeNorm = 0.0;
	unsigned int m_totalNodes = 0;
	double m_prior = 0.0;
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

#endif /* _INTEGER_MAP_EQUATION_H_ */
