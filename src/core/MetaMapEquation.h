/*
 * MetaMapEquation.h
 */

#ifndef _METAMAPEQUATION_H_
#define _METAMAPEQUATION_H_

#include <vector>
#include <set>
#include <map>
#include <utility>
#include "MapEquation.h"
#include "FlowData.h"
// #include "NodeBase.h"
#include "../utils/Log.h"
#include "../utils/MetaCollection.h"

namespace infomap {

class NodeBase;
template<class FlowDataType>
class Node;

class MetaMapEquation : protected MapEquation {
	using Base = MapEquation;
public:
	using FlowDataType = FlowData;
	using DeltaFlowDataType = DeltaFlow;
	using NodeBaseType = Node<FlowDataType>;

	MetaMapEquation() : MapEquation() {}

	MetaMapEquation(const MetaMapEquation& other)
	:	MapEquation(other),
		m_moduleToMetaCollection(other.m_moduleToMetaCollection),
		numMetaDataDimensions(other.numMetaDataDimensions),
		metaDataRate(other.metaDataRate),
		weightByFlow(other.weightByFlow),
		metaCodelength(other.metaCodelength)
	{}

	MetaMapEquation& operator=(const MetaMapEquation& other) {
		Base::operator =(other);
		m_moduleToMetaCollection = other.m_moduleToMetaCollection;
		numMetaDataDimensions = other.numMetaDataDimensions;
		metaDataRate = other.metaDataRate;
		weightByFlow = other.weightByFlow;
		metaCodelength = other.metaCodelength;
		return *this;
	}

	virtual ~MetaMapEquation() {}

	// ===================================================
	// Getters
	// ===================================================

	static bool haveMemory() { return true; }

	using Base::getIndexCodelength;

	// double getModuleCodelength() const { return moduleCodelength + metaCodelength; };
	double getModuleCodelength() const;

	// double getCodelength() const { return codelength + metaCodelength; };
	double getCodelength() const;

	// ===================================================
	// IO
	// ===================================================

	// using Base::print;
	std::ostream& print(std::ostream& out) const;
	// friend std::ostream& operator<<(std::ostream&, const MetaMapEquation&);

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
	
	void addMemoryContributions(NodeBase& current, DeltaFlowDataType& oldModuleDelta, VectorMap<DeltaFlowDataType>& moduleDeltaFlow) {}

	double getDeltaCodelengthOnMovingNode(NodeBase& current,
			DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData, std::vector<unsigned int>& moduleMembers);

	// ===================================================
	// Consolidation
	// ===================================================

	void updateCodelengthOnMovingNode(NodeBase& current,
			DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData, std::vector<unsigned int>& moduleMembers);

	void consolidateModules(std::vector<NodeBase*>& modules);

	using Base::createNode;
	using Base::getNode;
	// const NodeBaseType& getNode(const NodeBase&) const;

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

	void initMetaNodes(NodeBase& root);

	void initPartitionOfMetaNodes(std::vector<NodeBase*>& nodes);

	// ===================================================
	// Codelength
	// ===================================================

	void calculateCodelength(std::vector<NodeBase*>& nodes);

	using Base::calculateCodelengthTerms;

	using Base::calculateCodelengthFromCodelengthTerms;

	// ===================================================
	// Consolidation
	// ===================================================

	void updateMetaData(NodeBase& current, unsigned int oldModuleIndex, unsigned int bestModuleIndex);

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

	/**
	 *  Get meta codelength of module of current node
	 * @param addRemoveOrNothing +1, -1 or 0 to calculate codelength
	 * as ifcurrent node was added, removed or untouched in current module
	 */
	double getCurrentModuleMetaCodelength(unsigned int module, NodeBase& current, int addRemoveOrNothing);

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

	// For meta data
	using ModuleMetaMap = std::map<unsigned int, MetaCollection>; // moduleId -> (metaId -> count)

	ModuleMetaMap m_moduleToMetaCollection;

	unsigned int numMetaDataDimensions = 0;
	double metaDataRate = 1.0;
	bool weightByFlow = false;
	double metaCodelength = 0.0;
};



}

#endif /* _METAMAPEQUATION_H_ */
