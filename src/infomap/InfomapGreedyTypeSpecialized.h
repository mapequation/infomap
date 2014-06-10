/**********************************************************************************

 Infomap software package for multi-level network clustering

 Copyright (c) 2013, 2014 Daniel Edler, Martin Rosvall
 
 For more information, see <http://www.mapequation.org>
 

 This file is part of Infomap software package.

 Infomap software package is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Infomap software package is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with Infomap software package.  If not, see <http://www.gnu.org/licenses/>.

**********************************************************************************/


#ifndef INFOMAPGREEDYTYPESPECIALIZED_H_
#define INFOMAPGREEDYTYPESPECIALIZED_H_

#include "InfomapGreedyCommon.h"
#include "../io/version.h"
#include <ostream>

template<typename FlowType, typename NetworkType>
class InfomapGreedyTypeSpecialized : public InfomapGreedyCommon<InfomapGreedyTypeSpecialized<FlowType, NetworkType> >
{
	friend class InfomapGreedyCommon<InfomapGreedyTypeSpecialized<FlowType, NetworkType> >;
	typedef InfomapGreedyCommon<InfomapGreedyTypeSpecialized<FlowType, NetworkType> >		Super;
	typedef typename flowData_traits<FlowType>::detailed_balance_type 						DetailedBalanceType;
	typedef typename flowData_traits<FlowType>::directed_with_recorded_teleportation_type 	DirectedWithRecordedTeleportationType;
	typedef typename flowData_traits<FlowType>::teleportation_type 							TeleportationType;
	typedef MemNode<FlowType>																NodeType;
	typedef Edge<NodeBase>																	EdgeType;

public:
	typedef FlowType																		flow_type;
	InfomapGreedyTypeSpecialized(const Config& conf) : 
	InfomapGreedyCommon<InfomapGreedyTypeSpecialized<FlowType, NetworkType> >(conf) {}
	virtual ~InfomapGreedyTypeSpecialized() {}

protected:

	virtual void initModuleOptimization();

	void calculateNodeFlow_log_nodeFlowForMemoryNetwork() {}
	
	void addContributionOfMovingMemoryNodes(NodeType& current, 
		DeltaFlow& oldModuleDelta, std::vector<DeltaFlow>& moduleDeltaEnterExit, 
		std::vector<unsigned int>& redirect, unsigned int& offset, unsigned int& numModuleLinks) {}
	
	void performMoveOfMemoryNode(NodeType& current, unsigned int oldModuleIndex, unsigned int bestModuleIndex) {}
	
	void performPredefinedMoveOfMemoryNode(NodeType& current, unsigned int oldModuleIndex, unsigned int bestModuleIndex, DeltaFlow& oldModuleDelta, DeltaFlow& newModuleDelta) {}
	
	double getDeltaCodelengthOnMovingMemoryNode(DeltaFlow& oldModuleDelta, DeltaFlow& newModuleDelta) { return 0.0; } 
	
	void updateCodelengthOnMovingMemoryNode(DeltaFlow& oldModuleDelta, DeltaFlow& newModuleDelta) {}

	void consolidatePhysicalNodes(std::vector<NodeBase*>& modules) {}

	void generateNetworkFromChildren(NodeBase& parent);

};


struct MemNodeSet
{
	MemNodeSet(unsigned int numMemNodes, double sumFlow) : numMemNodes(numMemNodes), sumFlow(sumFlow) {}
	unsigned int numMemNodes; // use counter to check for zero to avoid round-off errors in sumFlow
	double sumFlow;
};

template<typename FlowType>
class InfomapGreedyTypeSpecialized<FlowType, WithMemory> : public InfomapGreedyCommon<InfomapGreedyTypeSpecialized<FlowType, WithMemory> >
{
	friend class InfomapGreedyCommon<InfomapGreedyTypeSpecialized<FlowType, WithMemory> >;
	typedef InfomapGreedyCommon<InfomapGreedyTypeSpecialized<FlowType, WithMemory> >		Super;
	typedef typename flowData_traits<FlowType>::detailed_balance_type 						DetailedBalanceType;
	typedef typename flowData_traits<FlowType>::directed_with_recorded_teleportation_type 	DirectedWithRecordedTeleportationType;
	typedef typename flowData_traits<FlowType>::teleportation_type 							TeleportationType;
	typedef MemNode<FlowType>																NodeType;
	typedef Edge<NodeBase>																	EdgeType;
	typedef std::map<unsigned int, MemNodeSet>												ModuleToMemNodes;

	struct IndexedFlow {
		IndexedFlow() : index(0) {}
		IndexedFlow(unsigned int index, FlowType flowData) :
			index(index), flowData(flowData) {}
		IndexedFlow(const IndexedFlow& other) :
			index(other.index), flowData(other.flowData) {}
		IndexedFlow& operator=(const IndexedFlow& other) {
			index = other.index;
			flowData = other.flowData;
			return *this;
		}
		unsigned int index;
		FlowType flowData;
	};

public:
	typedef FlowType																		flow_type;
	InfomapGreedyTypeSpecialized(const Config& conf) : 
	InfomapGreedyCommon<InfomapGreedyTypeSpecialized<FlowType, WithMemory> >(conf),
		m_numPhysicalNodes(0) {}
	virtual ~InfomapGreedyTypeSpecialized() {}

protected:
	virtual double calcCodelengthOnRootOfLeafNodes(const NodeBase& parent);
	virtual double calcCodelengthOnModuleOfModules(const NodeBase& parent);
	virtual double calcCodelengthOnModuleOfLeafNodes(const NodeBase& parent);

	virtual void initModuleOptimization();

	void calculateNodeFlow_log_nodeFlowForMemoryNetwork();
	
	void addContributionOfMovingMemoryNodes(NodeType& current, 
		DeltaFlow& oldModuleDelta, std::vector<DeltaFlow>& moduleDeltaEnterExit, 
		std::vector<unsigned int>& redirect, unsigned int& offset, unsigned int& numModuleLinks);
	
	void performMoveOfMemoryNode(NodeType& current, unsigned int oldModuleIndex, unsigned int bestModuleIndex);

	void performPredefinedMoveOfMemoryNode(NodeType& current, unsigned int oldModuleIndex, unsigned int bestModuleIndex, DeltaFlow& oldModuleDelta, DeltaFlow& newModuleDelta);
	
	double getDeltaCodelengthOnMovingMemoryNode(DeltaFlow& oldModuleDelta, DeltaFlow& newModuleDelta);
	
	void updateCodelengthOnMovingMemoryNode(DeltaFlow& oldModuleDelta, DeltaFlow& newModuleDelta);

	void consolidatePhysicalNodes(std::vector<NodeBase*>& modules);

	void generateNetworkFromChildren(NodeBase& parent);

	virtual void saveHierarchicalNetwork(std::string rootName, bool includeLinks);

	virtual void printClusterNumbers(std::ostream& out);

	virtual void printFlowNetwork(std::ostream& out);

private:
	NodeType& getNode(NodeBase& node) { return static_cast<NodeType&>(node); }
	const NodeType& getNode(const NodeBase& node) const { return static_cast<const NodeType&>(node); }

	std::vector<ModuleToMemNodes> m_physToModuleToMemNodes; // vector[physicalNodeID] map<moduleID, {#memNodes, sumFlow}>  
	unsigned int m_numPhysicalNodes;


};


template<typename FlowType>
inline double InfomapGreedyTypeSpecialized<FlowType, WithMemory>::calcCodelengthOnRootOfLeafNodes(const NodeBase& parent)
{
	return Super::calcCodelengthOnModuleOfLeafNodes(parent);
}

template<typename FlowType>
inline double InfomapGreedyTypeSpecialized<FlowType, WithMemory>::calcCodelengthOnModuleOfLeafNodes(const NodeBase& parent)
{
	const FlowType& parentData = Super::getNode(parent).data;
	double parentFlow = parentData.flow;
	double parentExit = parentData.exitFlow;
	double totalParentFlow = parentFlow + parentExit;
	if (totalParentFlow < 1e-16)
		return 0.0;

	double indexLength = 0.0;
	// For each physical node
	const std::vector<PhysData>& physNodes = Super::getNode(parent).physicalNodes;
	for (unsigned int i = 0; i < physNodes.size(); ++i)
	{
		indexLength -= infomath::plogp(physNodes[i].sumFlowFromM2Node / totalParentFlow);
	}
	indexLength -= infomath::plogp(parentExit / totalParentFlow);

	indexLength *= totalParentFlow;

	return indexLength;
}

template<typename FlowType>
inline double InfomapGreedyTypeSpecialized<FlowType, WithMemory>::calcCodelengthOnModuleOfModules(const NodeBase& parent)
{
	return calcCodelengthOnModuleOfLeafNodes(parent);
	//TODO: In above indexLength -= infomath::plogp(physNodes[i].sumFlowFromM2Node / totalParentFlow),
	// shouldn't sum of enterFlow be used instead of flow even for the memory nodes?
}

template<typename FlowType, typename NetworkType>
void InfomapGreedyTypeSpecialized<FlowType, NetworkType>::initModuleOptimization()
{
	unsigned int numNodes = Super::m_activeNetwork.size();
	Super::m_moduleFlowData.resize(numNodes);
	Super::m_moduleMembers.assign(numNodes, 1);
	Super::m_emptyModules.clear();
	Super::m_emptyModules.reserve(numNodes);

	unsigned int i = 0;
	for (typename Super::activeNetwork_iterator it(Super::m_activeNetwork.begin()), itEnd(Super::m_activeNetwork.end());
			it != itEnd; ++it, ++i)
	{
		NodeType& node = Super::getNode(**it);
		node.index = i; // Unique module index for each node
		Super::m_moduleFlowData[i] = node.data;
		node.dirty = true;
	}

	// Initiate codelength terms for the initial state of one module per node
	Super::calculateCodelengthFromActiveNetwork(DetailedBalanceType());
}

template<typename FlowType>
void InfomapGreedyTypeSpecialized<FlowType, WithMemory>::initModuleOptimization()
{
	DEBUG_OUT("\n::initModuleOptimization()" << std::flush);
	unsigned int numNodes = Super::m_activeNetwork.size();
	Super::m_moduleFlowData.resize(numNodes);
	Super::m_moduleMembers.assign(numNodes, 1);
	Super::m_emptyModules.clear();
	Super::m_emptyModules.reserve(numNodes);

	if (m_numPhysicalNodes == 0)
		m_numPhysicalNodes = numNodes;
	m_physToModuleToMemNodes.clear();
	m_physToModuleToMemNodes.resize(m_numPhysicalNodes);

	unsigned int i = 0;
	for (typename Super::activeNetwork_iterator it(Super::m_activeNetwork.begin()), itEnd(Super::m_activeNetwork.end());
			it != itEnd; ++it, ++i)
	{
		NodeType& node = Super::getNode(**it);
		node.index = i; // Unique module index for each node
		Super::m_moduleFlowData[i] = node.data;
		node.dirty = true;

		unsigned int numPhysicalMembers = node.physicalNodes.size();
		for(unsigned int j = 0; j < numPhysicalMembers; ++j)
		{
			PhysData& physData = node.physicalNodes[j];
			m_physToModuleToMemNodes[physData.physNodeIndex].insert(m_physToModuleToMemNodes[physData.physNodeIndex].end(),
					std::make_pair(i, MemNodeSet(1, physData.sumFlowFromM2Node)));
		}
	}


	// Initiate codelength terms for the initial state of one module per node
	Super::calculateCodelengthFromActiveNetwork(DetailedBalanceType());
}


template<typename FlowType>
void InfomapGreedyTypeSpecialized<FlowType, WithMemory>::calculateNodeFlow_log_nodeFlowForMemoryNetwork()
{
	double nodeFlow_log_nodeFlow = 0.0;
	for (unsigned int i = 0; i < m_numPhysicalNodes; ++i)
	{
		const ModuleToMemNodes& moduleToMemNodes = m_physToModuleToMemNodes[i];
		for (ModuleToMemNodes::const_iterator modToMemIt(moduleToMemNodes.begin()); modToMemIt != moduleToMemNodes.end(); ++modToMemIt)
			nodeFlow_log_nodeFlow += infomath::plogp(modToMemIt->second.sumFlow);
	}
	Super::nodeFlow_log_nodeFlow = nodeFlow_log_nodeFlow;
}



template<typename FlowType>
inline
void InfomapGreedyTypeSpecialized<FlowType, WithMemory>::addContributionOfMovingMemoryNodes(NodeType& current, 
	DeltaFlow& oldModuleDelta, std::vector<DeltaFlow>& moduleDeltaEnterExit, 
	std::vector<unsigned int>& redirect, unsigned int& offset, unsigned int& numModuleLinks)
{
	// Overlapping modules
	/**
	 * delta = old.first + new.first + old.second - new.second.
	 * Two cases: (p(x) = plogp(x))
	 * Moving to a module that already have that physical node: (old: p1, p2, new p3, moving p2 -> old:p1, new p2,p3)
	 * Then old.second = new.second = plogp(physicalNodeSize) -> cancelation -> delta = p(p1) - p(p1+p2) + p(p2+p3) - p(p3)
	 * Moving to a module that not have that physical node: (old: p1, p2, new -, moving p2 -> old: p1, new: p2)
	 * Then new.first = new.second = 0 -> delta = p(p1) - p(p1+p2) + p(p2).
	 */
	unsigned int numPhysicalNodes = current.physicalNodes.size();
	for (unsigned int i = 0; i < numPhysicalNodes; ++i)
	{
		PhysData& physData = current.physicalNodes[i];
		ModuleToMemNodes& moduleToMemNodes = m_physToModuleToMemNodes[physData.physNodeIndex];
		for (ModuleToMemNodes::iterator overlapIt(moduleToMemNodes.begin()); overlapIt != moduleToMemNodes.end(); ++overlapIt)
		{
			unsigned int moduleIndex = overlapIt->first;
			MemNodeSet& memNodeSet = overlapIt->second;
			if (moduleIndex == current.index) // From where the multiple assigned node is moved
			{
				double oldPhysFlow = memNodeSet.sumFlow;
				double newPhysFlow = memNodeSet.sumFlow - physData.sumFlowFromM2Node;
				oldModuleDelta.sumDeltaPlogpPhysFlow += infomath::plogp(newPhysFlow) - infomath::plogp(oldPhysFlow);
				oldModuleDelta.sumPlogpPhysFlow += infomath::plogp(physData.sumFlowFromM2Node);
			}
			else // To where the multiple assigned node is moved
			{
				double oldPhysFlow = memNodeSet.sumFlow;
				double newPhysFlow = memNodeSet.sumFlow + physData.sumFlowFromM2Node;

				if (redirect[moduleIndex] >= offset)
				{
					moduleDeltaEnterExit[redirect[moduleIndex] - offset].sumDeltaPlogpPhysFlow += infomath::plogp(newPhysFlow) - infomath::plogp(oldPhysFlow);
					moduleDeltaEnterExit[redirect[moduleIndex] - offset].sumPlogpPhysFlow += infomath::plogp(physData.sumFlowFromM2Node);
				}
				else
				{
					redirect[moduleIndex] = offset + numModuleLinks;
					moduleDeltaEnterExit[numModuleLinks].module = moduleIndex;
					moduleDeltaEnterExit[numModuleLinks].deltaExit = 0.0;
					moduleDeltaEnterExit[numModuleLinks].deltaEnter = 0.0;
					moduleDeltaEnterExit[numModuleLinks].sumDeltaPlogpPhysFlow = infomath::plogp(newPhysFlow) - infomath::plogp(oldPhysFlow);
					moduleDeltaEnterExit[numModuleLinks].sumPlogpPhysFlow = infomath::plogp(physData.sumFlowFromM2Node);
					++numModuleLinks;
				}
			}
		}
	}
}

template<typename FlowType>
inline
void InfomapGreedyTypeSpecialized<FlowType, WithMemory>::performMoveOfMemoryNode(NodeType& current, 
	unsigned int oldModuleIndex, unsigned int bestModuleIndex)
{
	// For all multiple assigned nodes
	for (unsigned int i = 0; i < current.physicalNodes.size(); ++i)
	{
		PhysData& physData = current.physicalNodes[i];
		ModuleToMemNodes& moduleToMemNodes = m_physToModuleToMemNodes[physData.physNodeIndex];

		// Remove contribution to old module
		ModuleToMemNodes::iterator overlapIt = moduleToMemNodes.find(oldModuleIndex);
		if (overlapIt == moduleToMemNodes.end())
			throw std::length_error("Couldn't find old module among physical node assignments.");
		MemNodeSet& memNodeSet = overlapIt->second;
		memNodeSet.sumFlow -= physData.sumFlowFromM2Node;
		if (--memNodeSet.numMemNodes == 0)
			moduleToMemNodes.erase(overlapIt);

		// Add contribution to new module
		overlapIt = moduleToMemNodes.find(bestModuleIndex);
		if (overlapIt == moduleToMemNodes.end())
			moduleToMemNodes.insert(std::make_pair(bestModuleIndex, MemNodeSet(1, physData.sumFlowFromM2Node)));
		else {
			MemNodeSet& memNodeSet = overlapIt->second;
			++memNodeSet.numMemNodes;
			memNodeSet.sumFlow += physData.sumFlowFromM2Node;
		}
	}
}


template<typename FlowType>
inline
void InfomapGreedyTypeSpecialized<FlowType, WithMemory>::performPredefinedMoveOfMemoryNode(NodeType& current, 
	unsigned int oldModuleIndex, unsigned int bestModuleIndex, DeltaFlow& oldModuleDelta, DeltaFlow& newModuleDelta)
{
	// For all multiple assigned nodes
	for (unsigned int i = 0; i < current.physicalNodes.size(); ++i)
	{
		PhysData& physData = current.physicalNodes[i];
		ModuleToMemNodes& moduleToMemNodes = m_physToModuleToMemNodes[physData.physNodeIndex];

		// Remove contribution to old module
		ModuleToMemNodes::iterator overlapIt = moduleToMemNodes.find(oldModuleIndex);
		if (overlapIt == moduleToMemNodes.end())
			throw std::length_error("Couldn't find old module among physical node assignments.");
		MemNodeSet& memNodeSet = overlapIt->second;
		double oldPhysFlow = memNodeSet.sumFlow;
		double newPhysFlow = memNodeSet.sumFlow - physData.sumFlowFromM2Node;
		oldModuleDelta.sumDeltaPlogpPhysFlow += infomath::plogp(newPhysFlow) - infomath::plogp(oldPhysFlow);
		oldModuleDelta.sumPlogpPhysFlow += infomath::plogp(physData.sumFlowFromM2Node);
		memNodeSet.sumFlow -= physData.sumFlowFromM2Node;
		if (--memNodeSet.numMemNodes == 0)
			moduleToMemNodes.erase(overlapIt);


		// Add contribution to new module
		overlapIt = moduleToMemNodes.find(bestModuleIndex);
		if (overlapIt == moduleToMemNodes.end())
		{
			moduleToMemNodes.insert(std::make_pair(bestModuleIndex, MemNodeSet(1, physData.sumFlowFromM2Node)));
			oldPhysFlow = 0.0;
			newPhysFlow = physData.sumFlowFromM2Node;
			newModuleDelta.sumDeltaPlogpPhysFlow += infomath::plogp(newPhysFlow) - infomath::plogp(oldPhysFlow);
			newModuleDelta.sumPlogpPhysFlow += infomath::plogp(physData.sumFlowFromM2Node);
		}
		else
		{
			MemNodeSet& memNodeSet = overlapIt->second;
			oldPhysFlow = memNodeSet.sumFlow;
			newPhysFlow = memNodeSet.sumFlow + physData.sumFlowFromM2Node;
			newModuleDelta.sumDeltaPlogpPhysFlow += infomath::plogp(newPhysFlow) - infomath::plogp(oldPhysFlow);
			newModuleDelta.sumPlogpPhysFlow += infomath::plogp(physData.sumFlowFromM2Node);
			++memNodeSet.numMemNodes;
			memNodeSet.sumFlow += physData.sumFlowFromM2Node;
		}

	}
}


template<typename FlowType>
double InfomapGreedyTypeSpecialized<FlowType, WithMemory>::getDeltaCodelengthOnMovingMemoryNode(DeltaFlow& oldModuleDelta, DeltaFlow& newModuleDelta)
{
	double delta_nodeFlow_log_nodeFlow = oldModuleDelta.sumDeltaPlogpPhysFlow + newModuleDelta.sumDeltaPlogpPhysFlow + oldModuleDelta.sumPlogpPhysFlow - newModuleDelta.sumPlogpPhysFlow;
	return -delta_nodeFlow_log_nodeFlow;
}

template<typename FlowType>
void InfomapGreedyTypeSpecialized<FlowType, WithMemory>::updateCodelengthOnMovingMemoryNode(DeltaFlow& oldModuleDelta, DeltaFlow& newModuleDelta)
{
	double delta_nodeFlow_log_nodeFlow = oldModuleDelta.sumDeltaPlogpPhysFlow + newModuleDelta.sumDeltaPlogpPhysFlow + oldModuleDelta.sumPlogpPhysFlow - newModuleDelta.sumPlogpPhysFlow;
	Super::nodeFlow_log_nodeFlow += delta_nodeFlow_log_nodeFlow;
	Super::moduleCodelength -= delta_nodeFlow_log_nodeFlow;
	Super::codelength -= delta_nodeFlow_log_nodeFlow;
}



template<typename FlowType, typename NetworkType>
void InfomapGreedyTypeSpecialized<FlowType, NetworkType>::generateNetworkFromChildren(NodeBase& parent)
{
	// Clone all nodes
	unsigned int numNodes = parent.childDegree();
	Super::m_treeData.reserveNodeCount(numNodes);
	unsigned int i = 0;
	for (NodeBase::sibling_iterator childIt(parent.begin_child()), endIt(parent.end_child());
			childIt != endIt; ++childIt, ++i)
	{
		NodeType& otherNode = Super::getNode(*childIt);
		NodeBase* node = new NodeType(otherNode);
		node->originalIndex = childIt->originalIndex;
		Super::m_treeData.addClonedNode(node);
		childIt->index = i; // Set index to its place in this subnetwork to be able to find edge target below
		node->index = i;
	}

	NodeBase* parentPtr = &parent;
	// Clone edges
	for (NodeBase::sibling_iterator childIt(parent.begin_child()), endIt(parent.end_child());
			childIt != endIt; ++childIt)
	{
		NodeBase& node = *childIt;
		for (NodeBase::edge_iterator outEdgeIt(node.begin_outEdge()), endIt(node.end_outEdge());
				outEdgeIt != endIt; ++outEdgeIt)
		{
			const EdgeType& edge = **outEdgeIt;
			// If neighbour node is within the same module, add the link to this subnetwork.
			if (edge.target.parent == parentPtr)
			{
				Super::m_treeData.addEdge(node.index, edge.target.index, edge.data.weight, edge.data.flow);
			}
		}
	}

	double parentExit = Super::getNode(parent).data.exitFlow;

	Super::exitNetworkFlow = parentExit;
	Super::exitNetworkFlow_log_exitNetworkFlow = infomath::plogp(Super::exitNetworkFlow);
}

template<typename FlowType>
void InfomapGreedyTypeSpecialized<FlowType, WithMemory>::consolidatePhysicalNodes(std::vector<NodeBase*>& modules)
{
	std::map<unsigned int, std::map<unsigned int, unsigned int> > validate;

	for(unsigned int i = 0; i < m_numPhysicalNodes; ++i)
	{
		ModuleToMemNodes& modToMemNodes = m_physToModuleToMemNodes[i];
		for(ModuleToMemNodes::iterator overlapIt = modToMemNodes.begin(); overlapIt != modToMemNodes.end(); ++overlapIt)
		{
			if(++validate[overlapIt->first][i] > 1)
				throw std::domain_error("[InfomapGreedy::consolidateModules] Error updating physical nodes: duplication error");

			Super::getNode(*modules[overlapIt->first]).physicalNodes.push_back(PhysData(i, overlapIt->second.sumFlow));
		}
	}
}

template<typename FlowType>
void InfomapGreedyTypeSpecialized<FlowType, WithMemory>::generateNetworkFromChildren(NodeBase& parent)
{
	std::set<unsigned int> setOfPhysicalNodes;

	// Clone all nodes
	unsigned int numNodes = parent.childDegree();
	Super::m_treeData.reserveNodeCount(numNodes);
	unsigned int i = 0;
	for (NodeBase::sibling_iterator childIt(parent.begin_child()), endIt(parent.end_child());
			childIt != endIt; ++childIt, ++i)
	{
		NodeType& otherNode = Super::getNode(*childIt);
		NodeBase* node = new NodeType(otherNode);
		node->originalIndex = childIt->originalIndex;
		Super::m_treeData.addClonedNode(node);
		childIt->index = i; // Set index to its place in this subnetwork to be able to find edge target below
		node->index = i;

		for (unsigned int j = 0; j < otherNode.physicalNodes.size(); ++j)
		{
			PhysData& physData = otherNode.physicalNodes[j];
			setOfPhysicalNodes.insert(physData.physNodeIndex);
		}
	}

	// Re-index physical nodes
	std::map<unsigned int, unsigned int> subPhysIndexMap;
	unsigned int subPhysIndex = 0;
	for (std::set<unsigned int>::iterator it(setOfPhysicalNodes.begin()); it != setOfPhysicalNodes.end(); ++it, ++subPhysIndex)
	{
		subPhysIndexMap.insert(std::make_pair(*it, subPhysIndex));
	}

	for (typename TreeData::leafIterator leafIt(Super::m_treeData.begin_leaf()); leafIt != Super::m_treeData.end_leaf(); ++leafIt, ++i)
	{
		NodeType& node = Super::getNode(**leafIt);
		for (unsigned int j = 0; j < node.physicalNodes.size(); ++j)
		{
			PhysData& physData = node.physicalNodes[j];
			physData.physNodeIndex = subPhysIndexMap[physData.physNodeIndex];
		}
	}

	m_numPhysicalNodes = setOfPhysicalNodes.size();

	NodeBase* parentPtr = &parent;
	// Clone edges
	for (typename NodeBase::sibling_iterator childIt(parent.begin_child()), endIt(parent.end_child());
			childIt != endIt; ++childIt)
	{
		NodeBase& node = *childIt;
		for (NodeBase::edge_iterator outEdgeIt(node.begin_outEdge()), endIt(node.end_outEdge());
				outEdgeIt != endIt; ++outEdgeIt)
		{
			const EdgeType& edge = **outEdgeIt;
			// If neighbour node is within the same module, add the link to this subnetwork.
			if (edge.target.parent == parentPtr)
			{
				Super::m_treeData.addEdge(node.index, edge.target.index, edge.data.weight, edge.data.flow);
			}
		}
	}

	double parentExit = Super::getNode(parent).data.exitFlow;

	Super::exitNetworkFlow = parentExit;
	Super::exitNetworkFlow_log_exitNetworkFlow = infomath::plogp(Super::exitNetworkFlow);
}



template<typename FlowType>
void InfomapGreedyTypeSpecialized<FlowType, WithMemory>::saveHierarchicalNetwork(std::string rootName, bool includeLinks)
{
	HierarchicalNetwork& ioNetwork = Super::m_ioNetwork;

	ioNetwork.init(rootName, !Super::m_config.printAsUndirected(), Super::hierarchicalCodelength, Super::oneLevelCodelength, INFOMAP_VERSION);

	if (Super::m_config.printExpanded)
	{
		// Create vector of node names for memory nodes
		std::vector<std::string>& physicalNames = Super::m_nodeNames;
		std::vector<std::string> m2NodeNames(Super::m_treeData.numLeafNodes());
		unsigned int i = 0;
		for (typename TreeData::leafIterator leafIt(Super::m_treeData.begin_leaf()); leafIt != Super::m_treeData.end_leaf(); ++leafIt, ++i)
		{
			NodeType& node = getNode(**leafIt);
			M2Node& m2Node = node.m2Node;
			if (Super::m_config.isMultiplexNetwork())
				m2NodeNames[i] = io::Str() << physicalNames[m2Node.physIndex] << " | " << (m2Node.priorState + (Super::m_config.zeroBasedNodeNumbers? 0 : 1));
			else
				m2NodeNames[i] = io::Str() << physicalNames[m2Node.physIndex] << " | " << physicalNames[m2Node.priorState];
		}

		ioNetwork.prepareAddLeafNodes(Super::m_treeData.numLeafNodes());

		Super::buildHierarchicalNetworkHelper(ioNetwork, ioNetwork.getRootNode(), m2NodeNames);

		if (includeLinks)
		{
			for (typename TreeData::leafIterator leafIt(Super::m_treeData.begin_leaf()); leafIt != Super::m_treeData.end_leaf(); ++leafIt)
			{
				NodeBase& node = **leafIt;
				for (NodeBase::edge_iterator outEdgeIt(node.begin_outEdge()), endIt(node.end_outEdge());
						outEdgeIt != endIt; ++outEdgeIt)
				{
					EdgeType& edge = **outEdgeIt;
					ioNetwork.addLeafEdge(edge.source.originalIndex, edge.target.originalIndex, edge.data.flow);
				}
			}
		}
		return;
	}

	std::deque<std::pair<NodeBase*, HierarchicalNetwork::node_type*> > leafModules;

	Super::buildHierarchicalNetworkHelper(ioNetwork, ioNetwork.getRootNode(), leafModules);

	std::vector<std::map<unsigned int, IndexedFlow> > physicalNodes(leafModules.size());
	// Need a global map for links as the source network may be in different Infomap instance from the leaf modules
	std::vector<unsigned int> memNodeIndexToLeafModuleIndex(Super::m_treeData.numLeafNodes());
	unsigned int numCondensedNodes = 0;

	std::cout << "merging " << Super::m_treeData.numLeafNodes() << " memory nodes within " <<
			leafModules.size() << " modules...";
	std::cout << std::flush;

	// Aggregate memory nodes with the same physical node if in the same module
	for (unsigned int i = 0; i < leafModules.size(); ++i)
	{
		NodeBase* leafModule = leafModules[i].first;
		std::map<unsigned int, IndexedFlow>& condensedNodes = physicalNodes[i];
		for (NodeBase::sibling_iterator childIt(leafModule->begin_child()), endIt(leafModule->end_child());
				childIt != endIt; ++childIt)
		{
			const NodeType& node = Super::getNode(*childIt);
			std::pair<typename std::map<unsigned int, IndexedFlow>::iterator, bool> ret = condensedNodes.insert(std::make_pair(node.m2Node.physIndex, IndexedFlow(node.m2Node.physIndex, node.data)));
			if (!ret.second) // Add flow if physical node already exist
				ret.first->second.flowData += node.data; //TODO: If exitFlow should be correct, flow between memory nodes within same physical node should be subtracted.
			else // A new insertion was made
				++numCondensedNodes;
			memNodeIndexToLeafModuleIndex[node.originalIndex] = i;
		}
	}

	std::cout << " to " << numCondensedNodes << " nodes... " << std::flush;
	ioNetwork.prepareAddLeafNodes(numCondensedNodes);

	unsigned int sortedNodeIndex = 0;
	for (unsigned int i = 0; i < leafModules.size(); ++i)
	{
		// Sort the nodes on flow
		std::map<unsigned int, IndexedFlow>& condensedNodes = physicalNodes[i];
		typedef typename std::map<unsigned int, IndexedFlow>::iterator CondensedIterator;
		std::multimap<double, CondensedIterator, std::greater<double> > sortedNodes;
		for (typename std::map<unsigned int, IndexedFlow>::iterator it(condensedNodes.begin()); it != condensedNodes.end(); ++it)
		{
			// Store the iterator to keep the reference to the mapped IndexedFlow item;
			sortedNodes.insert(std::make_pair(it->second.flowData.flow, it));
		}

		// Add the condensed leaf nodes to the hierarchical network
		HierarchicalNetwork::node_type* parent = leafModules[i].second;
		for (typename std::multimap<double, CondensedIterator, std::greater<double> >::iterator it(sortedNodes.begin()); it != sortedNodes.end(); ++it)
		{
			CondensedIterator& condensedIt(it->second);
			IndexedFlow& nodeData = condensedIt->second;
			ioNetwork.addLeafNode(*parent, nodeData.flowData.flow, nodeData.flowData.exitFlow, Super::m_nodeNames[nodeData.index], sortedNodeIndex);
			// Remap to sorted indices to help link creation
			nodeData.index = sortedNodeIndex;
			++sortedNodeIndex;
		}
	}

	if (includeLinks)
	{
		for (typename TreeData::leafIterator leafIt(Super::m_treeData.begin_leaf()); leafIt != Super::m_treeData.end_leaf(); ++leafIt)
		{
			NodeBase& node = **leafIt;
			unsigned int leafModuleIndex = memNodeIndexToLeafModuleIndex[node.originalIndex];
			std::map<unsigned int, IndexedFlow>& condensedNodes = physicalNodes[leafModuleIndex];
			unsigned int sourceNodeIndex = condensedNodes.find(Super::getNode(node).m2Node.physIndex)->second.index;

			for (NodeBase::edge_iterator outEdgeIt(node.begin_outEdge()), endIt(node.end_outEdge());
					outEdgeIt != endIt; ++outEdgeIt)
			{
				EdgeType& edge = **outEdgeIt;
				unsigned int targetLeafModuleIndex = memNodeIndexToLeafModuleIndex[edge.target.originalIndex];
				std::map<unsigned int, IndexedFlow>& targetCondensedNodes = physicalNodes[targetLeafModuleIndex];
				unsigned int targetNodeIndex = targetCondensedNodes.find(Super::getNode(edge.target).m2Node.physIndex)->second.index;
				ioNetwork.addLeafEdge(sourceNodeIndex, targetNodeIndex, edge.data.flow);
			}
		}
	}
}

template<typename FlowType>
void InfomapGreedyTypeSpecialized<FlowType, WithMemory>::printClusterNumbers(std::ostream& out)
{
	out << "*Vertices " << Super::m_treeData.numLeafNodes() << "\n";
	out << "# from to moduleNr flow\n";
	for (typename TreeData::leafIterator leafIt(Super::m_treeData.begin_leaf()); leafIt != Super::m_treeData.end_leaf(); ++leafIt)
	{
		NodeType& node = getNode(**leafIt);
		M2Node& m2Node = node.m2Node;
		unsigned int index = node.parent->index;
		out << (m2Node.priorState + 1) << " " << (m2Node.physIndex + 1) << " " << (index + 1) << " " << node.data.flow << "\n";
	}
}


struct PhysNode
{
	PhysNode() : flow(0.0) {}
	double flow;
	std::map<unsigned int, double> edges;
};

template<typename FlowType>
void InfomapGreedyTypeSpecialized<FlowType, WithMemory>::printFlowNetwork(std::ostream& out)
{
	if (Super::m_config.printExpanded)
	{
		out << "# flow in network with " << Super::m_treeData.numLeafNodes() << " memory nodes (from-to) and " << Super::m_treeData.numLeafEdges() << " links\n";
		for (typename TreeData::leafIterator leafIt(Super::m_treeData.begin_leaf()); leafIt != Super::m_treeData.end_leaf(); ++leafIt)
		{
			NodeType& node = getNode(**leafIt);
			M2Node& m2Node = node.m2Node;
			out << m2Node << " (" << node.data << ")\n";
			for (NodeBase::edge_iterator edgeIt(node.begin_outEdge()), endEdgeIt(node.end_outEdge());
					edgeIt != endEdgeIt; ++edgeIt)
			{
				EdgeType& edge = **edgeIt;
				out << "  --> " << getNode(edge.target).m2Node << " (" << edge.data.flow << ")\n";
			}
			for (NodeBase::edge_iterator edgeIt(node.begin_inEdge()), endEdgeIt(node.end_inEdge());
					edgeIt != endEdgeIt; ++edgeIt)
			{
				EdgeType& edge = **edgeIt;
				out << "  <-- " << getNode(edge.source).m2Node << " (" << edge.data.flow << ")\n";
			}
		}
		return;
	}

//	std::map<unsigned int, PhysNode> physicalNetwork;
//	for (typename TreeData::leafIterator leafIt(Super::m_treeData.begin_leaf()); leafIt != Super::m_treeData.end_leaf(); ++leafIt)
//	{
//		NodeType& node = getNode(**leafIt);
//		M2Node& m2Node = node.m2Node;
//
//		PhysNode& physNode = physicalNetwork[m2Node.physIndex];
//		physNode.flow += node.data.flow;
//
//		out << m2Node << " (" << node.data << ")\n";
//		for (NodeBase::edge_iterator edgeIt(node.begin_outEdge()), endEdgeIt(node.end_outEdge());
//				edgeIt != endEdgeIt; ++edgeIt)
//		{
//			EdgeType& edge = **edgeIt;
//			out << "  --> " << getNode(edge.target).m2Node << " (" << edge.data.flow << ")\n";
//		}
//		for (NodeBase::edge_iterator edgeIt(node.begin_inEdge()), endEdgeIt(node.end_inEdge());
//				edgeIt != endEdgeIt; ++edgeIt)
//		{
//			EdgeType& edge = **edgeIt;
//			out << "  <-- " << getNode(edge.source).m2Node << " (" << edge.data.flow << ")\n";
//		}
//	}

}








#endif /* INFOMAPGREEDYTYPESPECIALIZED_H_ */
