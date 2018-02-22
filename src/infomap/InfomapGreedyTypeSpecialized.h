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
#include <ostream>

#ifdef NS_INFOMAP
namespace infomap
{
#endif

template<typename FlowType, typename NetworkType>
class InfomapGreedyTypeSpecialized : public InfomapGreedyCommon<InfomapGreedyTypeSpecialized<FlowType, NetworkType> >
{
	friend class InfomapGreedyCommon<InfomapGreedyTypeSpecialized<FlowType, NetworkType> >;
	typedef InfomapGreedyCommon<InfomapGreedyTypeSpecialized<FlowType, NetworkType> >		Super;
	typedef typename flowData_traits<FlowType>::detailed_balance_type 						DetailedBalanceType;
	typedef typename flowData_traits<FlowType>::directed_with_recorded_teleportation_type 	DirectedWithRecordedTeleportationType;
	typedef typename flowData_traits<FlowType>::teleportation_type 							TeleportationType;
	typedef Node<FlowType>																	NodeType;
	typedef Edge<NodeBase>																	EdgeType;
	typedef DeltaFlow																		DeltaFlowType;

public:
	typedef FlowType																		flow_type;
	InfomapGreedyTypeSpecialized(const Config& conf) :
		InfomapGreedyCommon<InfomapGreedyTypeSpecialized<FlowType, NetworkType> >(conf, new NodeFactory<FlowType>()) {}
	InfomapGreedyTypeSpecialized(const InfomapBase& infomap) :
		InfomapGreedyCommon<InfomapGreedyTypeSpecialized<FlowType, NetworkType> >(infomap, new NodeFactory<FlowType>()) {}
	virtual ~InfomapGreedyTypeSpecialized() {}

protected:

	virtual std::auto_ptr<InfomapBase> getNewInfomapInstanceWithoutMemory();

	virtual void initModuleOptimization();

	void calculateNodeFlow_log_nodeFlowForMemoryNetwork() {}

	void addContributionOfMovingMemoryNodes(NodeType& current,
		DeltaFlowType& oldModuleDelta, std::vector<DeltaFlowType>& moduleDeltaEnterExit,
		std::vector<unsigned int>& redirect, unsigned int& offset, unsigned int& numModuleLinks) {}

	void addContributionOfMovingMemoryNodes(NodeType& current,
		DeltaFlowType& oldModuleDelta, std::map<unsigned int, DeltaFlowType>& moduleDeltaFlow) {}

	void performMoveOfMemoryNode(NodeType& current, unsigned int oldModuleIndex, unsigned int bestModuleIndex) {}

	void performPredefinedMoveOfMemoryNode(NodeType& current, unsigned int oldModuleIndex, unsigned int bestModuleIndex, DeltaFlowType& oldModuleDelta, DeltaFlowType& newModuleDelta) {}

	double getDeltaCodelengthOnMovingMemoryNode(DeltaFlowType& oldModuleDelta, DeltaFlowType& newModuleDelta) { return 0.0; }

	void updateCodelengthOnMovingMemoryNode(DeltaFlowType& oldModuleDelta, DeltaFlowType& newModuleDelta) {}

	void consolidatePhysicalNodes(std::vector<NodeBase*>& modules) {}

	void generateNetworkFromChildren(NodeBase& parent);

	using Super::calculateCodelengthFromActiveNetwork;

	virtual std::vector<PhysData>& getPhysicalMembers(NodeBase& node) { return m_dummyPhysData; }

	virtual StateNode& getMemoryNode(NodeBase& node) { return m_dummyStateNode; }

	using Super::m_activeNetwork;
	using Super::m_moduleFlowData;
	using Super::m_moduleMembers;
	using Super::m_emptyModules;
	using Super::m_config;

	std::vector<PhysData> m_dummyPhysData;
	StateNode m_dummyStateNode;
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
	typedef MemDeltaFlow																	DeltaFlowType;
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
			InfomapGreedyCommon<InfomapGreedyTypeSpecialized<FlowType, WithMemory> >(conf, new MemNodeFactory<FlowType>()),
			m_numPhysicalNodes(0) {}
	InfomapGreedyTypeSpecialized(const InfomapBase& infomap) :
			InfomapGreedyCommon<InfomapGreedyTypeSpecialized<FlowType, WithMemory> >(infomap, new MemNodeFactory<FlowType>()),
			m_numPhysicalNodes(0) {}
	virtual ~InfomapGreedyTypeSpecialized() {}

protected:

	virtual std::auto_ptr<InfomapBase> getNewInfomapInstanceWithoutMemory();

	virtual bool preClusterMultiplexNetwork(bool printResults = false);

	virtual unsigned int aggregateFlowValuesFromLeafToRoot();

	virtual double calcCodelengthOnRootOfLeafNodes(const NodeBase& parent);
	virtual double calcCodelengthOnModuleOfModules(const NodeBase& parent);
	virtual double calcCodelengthOnModuleOfLeafNodes(const NodeBase& parent);

	virtual void initModuleOptimization();

	void calculateNodeFlow_log_nodeFlowForMemoryNetwork();

	void addContributionOfMovingMemoryNodes(NodeType& current,
		DeltaFlowType& oldModuleDelta, std::vector<DeltaFlowType>& moduleDeltaEnterExit,
		std::vector<unsigned int>& redirect, unsigned int& offset, unsigned int& numModuleLinks);

	void addContributionOfMovingMemoryNodes(NodeType& current,
			DeltaFlowType& oldModuleDelta, std::map<unsigned int, DeltaFlowType>& moduleDeltaFlow);

	void performMoveOfMemoryNode(NodeType& current, unsigned int oldModuleIndex, unsigned int bestModuleIndex);

	void performPredefinedMoveOfMemoryNode(NodeType& current, unsigned int oldModuleIndex, unsigned int bestModuleIndex, DeltaFlowType& oldModuleDelta, DeltaFlowType& newModuleDelta);

	double getDeltaCodelengthOnMovingMemoryNode(DeltaFlowType& oldModuleDelta, DeltaFlowType& newModuleDelta);

	void updateCodelengthOnMovingMemoryNode(DeltaFlowType& oldModuleDelta, DeltaFlowType& newModuleDelta);

	void consolidatePhysicalNodes(std::vector<NodeBase*>& modules);

	void generateNetworkFromChildren(NodeBase& parent);

	virtual void saveHierarchicalNetwork(HierarchicalNetwork& output, std::string rootName, bool includeLinks);

	virtual void printClusterNumbers(std::ostream& out);

	virtual void printFlowNetwork(std::ostream& out);

	virtual std::vector<PhysData>& getPhysicalMembers(NodeBase& node);

	virtual StateNode& getMemoryNode(NodeBase& node);

	using Super::calculateCodelengthFromActiveNetwork;

	using Super::m_activeNetwork;
	using Super::m_moduleFlowData;
	using Super::m_moduleMembers;
	using Super::m_emptyModules;
	using Super::m_config;

private:
	NodeType& getNode(NodeBase& node) { return static_cast<NodeType&>(node); }
	const NodeType& getNode(const NodeBase& node) const { return static_cast<const NodeType&>(node); }

	std::vector<ModuleToMemNodes> m_physToModuleToMemNodes; // vector[physicalNodeID] map<moduleID, {#memNodes, sumFlow}>
	unsigned int m_numPhysicalNodes;


};



template<typename FlowType, typename NetworkType>
inline
std::auto_ptr<InfomapBase> InfomapGreedyTypeSpecialized<FlowType, NetworkType>::getNewInfomapInstanceWithoutMemory()
{
	return std::auto_ptr<InfomapBase>(new InfomapGreedyTypeSpecialized<FlowType, WithoutMemory>(Super::m_config));
}

template<typename FlowType>
inline
std::auto_ptr<InfomapBase> InfomapGreedyTypeSpecialized<FlowType, WithMemory>::getNewInfomapInstanceWithoutMemory()
{
	return std::auto_ptr<InfomapBase>(new InfomapGreedyTypeSpecialized<FlowType, WithoutMemory>(Super::m_config));
}


template<typename FlowType>
inline bool InfomapGreedyTypeSpecialized<FlowType, WithMemory>::preClusterMultiplexNetwork(bool printResults)
{
	if (!m_config.isMultiplexNetwork())
		return false;

	Log() << "Pre-cluster multiplex network layer by layer... " << std::endl;

	unsigned int memNodeIndex = 0;
	std::map<StateNode, unsigned int> memNodeToIndex;
	std::map<unsigned int, Network> networks;
	for (TreeData::leafIterator leafIt(Super::m_treeData.begin_leaf()); leafIt != Super::m_treeData.end_leaf(); ++leafIt, ++memNodeIndex)
	{
		NodeType& source = getNode(**leafIt);
		// Map from state id to single index
		memNodeToIndex[source.stateNode] = memNodeIndex;
		unsigned int layer = source.stateNode.layer();
		if (source.isDangling()) {
			networks[layer].addNode(source.stateNode.physIndex);
		}
		else {
			bool isDanglingInLayer = true;
			for (NodeBase::edge_iterator outEdgeIt(source.begin_outEdge()), endIt(source.end_outEdge());
					outEdgeIt != endIt; ++outEdgeIt)
			{
				const EdgeType& edge = **outEdgeIt;
				NodeType& target = getNode(edge.target);
				if (target.stateNode.layer() == layer)
				{
					isDanglingInLayer = false;
					networks[layer].addLink(source.stateNode.physIndex, target.stateNode.physIndex);
				}
			}
			if (isDanglingInLayer) {
				networks[layer].addNode(source.stateNode.physIndex);
			}
		}
	}

	Config perLayerConfig;
	perLayerConfig.twoLevel = true;
	perLayerConfig.zeroBasedNodeNumbers = true;
	perLayerConfig.noFileOutput = true;
	perLayerConfig.adaptDefaults();
	bool isSilent = Log::isSilent();
	unsigned int moduleIndexOffset = 0;
	unsigned int numNodesInFirstOrderNetworks = 0;
	std::vector<unsigned int> modules(Super::numLeafNodes());

	for (std::map<unsigned int, Network>::iterator networkIt(networks.begin()); networkIt != networks.end(); ++networkIt)
	{
		unsigned int layer = networkIt->first;
		Network& network = networkIt->second;
		network.setConfig(perLayerConfig);
		network.finalizeAndCheckNetwork(false);
		Log() << "  Layer " << layer << ": Cluster " << network.numNodes() << " nodes and " << network.numLinks() << " links... ";
		numNodesInFirstOrderNetworks += network.numNodes();

		Log::setSilent(true);
		InfomapGreedyTypeSpecialized<FlowUndirected, WithoutMemory> infomap(perLayerConfig);
		HierarchicalNetwork tree(perLayerConfig);
		infomap.run(network, tree);
		Log::setSilent(isSilent);

		Log() << "-> Codelength " << tree.codelength() << " in " << tree.numTopModules() << " modules.\n";

		unsigned int negativeModuleOffsetDueToMissingAllNodesInAModule = 0;
		bool potentialMissing = true;
		bool lastModuleIndex = 0;
		unsigned int i = 0;
		for (LeafIterator leafIt(tree.leafIter()); !leafIt.isEnd(); ++leafIt, ++i)
		{
			unsigned int moduleIndex = leafIt.moduleIndex();
			std::map<StateNode, unsigned int>::iterator it = memNodeToIndex.find(StateNode(layer, leafIt->originalLeafIndex));
			if (it != memNodeToIndex.end()) {
				if (moduleIndex != lastModuleIndex && potentialMissing) {
					// Last node's module is missing!
					++negativeModuleOffsetDueToMissingAllNodesInAModule;
				}
				unsigned int adjustedGlobalModuleIndex = moduleIndex + moduleIndexOffset - negativeModuleOffsetDueToMissingAllNodesInAModule;
				modules[it->second] = adjustedGlobalModuleIndex;
				potentialMissing = false;
			}
			else {
				if (moduleIndex != lastModuleIndex) {
					// New module with missing node
					if (potentialMissing) {
						// Last missing!
						++negativeModuleOffsetDueToMissingAllNodesInAModule;
					}
					potentialMissing = true;
				}
				// Node doesn't exist in state network
				// Skip module if all nodes in this module doesn't exist
			}
			lastModuleIndex = moduleIndex;
		}
		if (potentialMissing) {
			// All nodes in last module missing
			++negativeModuleOffsetDueToMissingAllNodesInAModule;
			potentialMissing = false;
		}

		moduleIndexOffset += tree.numTopModules() - negativeModuleOffsetDueToMissingAllNodesInAModule;
	}

	unsigned int numModules = moduleIndexOffset;

	Log() << "Clustered " << numNodesInFirstOrderNetworks << " nodes in " <<
		networks.size() << " networks into " << numModules << " modules.\n";

	std::vector<NodeBase*> moduleNodes(numModules, 0);
	for (unsigned int i = 0; i < modules.size(); ++i) {
		unsigned int moduleIndex = modules[i];
		if (moduleNodes[moduleIndex] == 0)
			moduleNodes[moduleIndex] = Super::m_treeData.nodeFactory().createNode("", 0.0, 0.0);
		// Set child pointers from the module nodes to all leaf nodes
		moduleNodes[moduleIndex]->addChild(&Super::m_treeData.getLeafNode(i));
	}

	// Set the modules as children to the root node instead of the current leaf nodes
	Super::m_treeData.root()->releaseChildren();
	for (unsigned int i = 0; i < numModules; ++i) {
		if (moduleNodes[i] != 0) {
			Super::m_treeData.root()->addChild(moduleNodes[i]);
		}
		else {
			throw std::length_error("Implementation error in pre-cluster multiplex network. Please contact authors.");
		}
	}

	Log() << "\n -> Generated " << numModules << " modules." << std::endl;

	Super::initPreClustering(printResults);
	return true;
}

template<typename FlowType>
inline unsigned int InfomapGreedyTypeSpecialized<FlowType, WithMemory>::aggregateFlowValuesFromLeafToRoot()
{
	unsigned int numLevels = Super::aggregateFlowValuesFromLeafToRoot();
	// Also aggregate physical nodes
	for (NodeBase::post_depth_first_iterator it(Super::root()); !it.isEnd(); ++it)
	{
		NodeType& node = getNode(*it);
		if (!node.isRoot()) {
			NodeType& parent = getNode(*node.parent);
			for (unsigned int i = 0; i < node.physicalNodes.size(); ++i)
			{
				unsigned int isAggregated = false;
				for (unsigned int j = 0; j < parent.physicalNodes.size(); ++j) {
					if (parent.physicalNodes[j].physNodeIndex == node.physicalNodes[i].physNodeIndex) {
						parent.physicalNodes[j].sumFlowFromStateNode += node.physicalNodes[i].sumFlowFromStateNode;
						isAggregated = true;
						break;
					}
				}
				if (!isAggregated)
					parent.physicalNodes.push_back(node.physicalNodes[i]);
			}
		}
	}
	// Check correct aggregation
	const std::vector<PhysData>& rootPhysNodes = getNode(*Super::root()).physicalNodes;
	double sumRootFlow = 0.0;
	for (unsigned int i = 0; i < rootPhysNodes.size(); ++i) {
		sumRootFlow += rootPhysNodes[i].sumFlowFromStateNode;
	}
	if (std::abs(sumRootFlow - 1.0) > 1e-10)
		Log() << "Warning, aggregated physical flow is not exactly 1.0, but " << sumRootFlow << ".\n";

	return numLevels;
}

template<typename FlowType>
inline double InfomapGreedyTypeSpecialized<FlowType, WithMemory>::calcCodelengthOnRootOfLeafNodes(const NodeBase& parent)
{
	return Super::calcCodelengthOnModuleOfLeafNodes(parent);
}

template<typename FlowType>
inline double InfomapGreedyTypeSpecialized<FlowType, WithMemory>::calcCodelengthOnModuleOfLeafNodes(const NodeBase& parent)
{
	// return Super::calcCodelengthOnModuleOfLeafNodes(parent);
	const FlowType& parentData = getNode(parent).data;
	double parentFlow = parentData.flow;
	double parentExit = parentData.exitFlow;
	double totalParentFlow = parentFlow + parentExit;
	if (totalParentFlow < 1e-16)
		return 0.0;

	double indexLength = 0.0;
	// For each physical node
	const std::vector<PhysData>& physNodes = getNode(parent).physicalNodes;
	for (unsigned int i = 0; i < physNodes.size(); ++i)
	{
		indexLength -= infomath::plogp(physNodes[i].sumFlowFromStateNode / totalParentFlow);
	}
	indexLength -= infomath::plogp(parentExit / totalParentFlow);

	indexLength *= totalParentFlow;

	return indexLength;
}

template<typename FlowType>
inline double InfomapGreedyTypeSpecialized<FlowType, WithMemory>::calcCodelengthOnModuleOfModules(const NodeBase& parent)
{
	return Super::calcCodelengthOnModuleOfModules(parent);
}

template<typename FlowType, typename NetworkType>
void InfomapGreedyTypeSpecialized<FlowType, NetworkType>::initModuleOptimization()
{
	unsigned int numNodes = Super::m_activeNetwork.size();
	m_moduleFlowData.resize(numNodes);
	m_moduleMembers.assign(numNodes, 1);
	m_emptyModules.clear();
	m_emptyModules.reserve(numNodes);

	unsigned int i = 0;
	for (typename Super::activeNetwork_iterator it(m_activeNetwork.begin()), itEnd(m_activeNetwork.end());
			it != itEnd; ++it, ++i)
	{
		NodeType& node = Super::getNode(**it);
		node.index = i; // Unique module index for each node
		Super::m_moduleFlowData[i] = node.data;
		node.dirty = true;
	}

	// Initiate codelength terms for the initial state of one module per node
	Super::calculateCodelengthFromActiveNetwork();
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

	if (m_numPhysicalNodes == 0) {
		// Get max physical index (may be more than numNodes if non-contiguous indexing)
		unsigned int maxPhysIndex = 0;
		for (typename Super::activeNetwork_iterator it(Super::m_activeNetwork.begin()), itEnd(Super::m_activeNetwork.end());
				it != itEnd; ++it)
		{
			NodeType& node = getNode(**it);
			unsigned int numPhysicalMembers = node.physicalNodes.size();
			for(unsigned int j = 0; j < numPhysicalMembers; ++j)
			{
				PhysData& physData = node.physicalNodes[j];
				maxPhysIndex = std::max(maxPhysIndex, physData.physNodeIndex);
			}
		}
		m_numPhysicalNodes = maxPhysIndex + 1;
	}
	m_physToModuleToMemNodes.clear();
	m_physToModuleToMemNodes.resize(m_numPhysicalNodes);

	unsigned int i = 0;
	for (typename Super::activeNetwork_iterator it(Super::m_activeNetwork.begin()), itEnd(Super::m_activeNetwork.end());
			it != itEnd; ++it, ++i)
	{
		NodeType& node = getNode(**it);
		node.index = i; // Unique module index for each node
		Super::m_moduleFlowData[i] = node.data;
		node.dirty = true;

		unsigned int numPhysicalMembers = node.physicalNodes.size();
		for(unsigned int j = 0; j < numPhysicalMembers; ++j)
		{
			PhysData& physData = node.physicalNodes[j];
			m_physToModuleToMemNodes[physData.physNodeIndex].insert(m_physToModuleToMemNodes[physData.physNodeIndex].end(),
					std::make_pair(i, MemNodeSet(1, physData.sumFlowFromStateNode)));
		}
	}


	// Initiate codelength terms for the initial state of one module per node
	Super::calculateCodelengthFromActiveNetwork();
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
	DeltaFlowType& oldModuleDelta, std::vector<DeltaFlowType>& moduleDeltaEnterExit,
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
				double newPhysFlow = memNodeSet.sumFlow - physData.sumFlowFromStateNode;
				oldModuleDelta.sumDeltaPlogpPhysFlow += infomath::plogp(newPhysFlow) - infomath::plogp(oldPhysFlow);
				oldModuleDelta.sumPlogpPhysFlow += infomath::plogp(physData.sumFlowFromStateNode);
			}
			else // To where the multiple assigned node is moved
			{
				double oldPhysFlow = memNodeSet.sumFlow;
				double newPhysFlow = memNodeSet.sumFlow + physData.sumFlowFromStateNode;

				if (redirect[moduleIndex] >= offset)
				{
					moduleDeltaEnterExit[redirect[moduleIndex] - offset].sumDeltaPlogpPhysFlow += infomath::plogp(newPhysFlow) - infomath::plogp(oldPhysFlow);
					moduleDeltaEnterExit[redirect[moduleIndex] - offset].sumPlogpPhysFlow += infomath::plogp(physData.sumFlowFromStateNode);
				}
				else
				{
					redirect[moduleIndex] = offset + numModuleLinks;
					moduleDeltaEnterExit[numModuleLinks].module = moduleIndex;
					moduleDeltaEnterExit[numModuleLinks].deltaExit = 0.0;
					moduleDeltaEnterExit[numModuleLinks].deltaEnter = 0.0;
					moduleDeltaEnterExit[numModuleLinks].sumDeltaPlogpPhysFlow = infomath::plogp(newPhysFlow) - infomath::plogp(oldPhysFlow);
					moduleDeltaEnterExit[numModuleLinks].sumPlogpPhysFlow = infomath::plogp(physData.sumFlowFromStateNode);
					++numModuleLinks;
				}
			}
		}
	}
}

template<typename FlowType>
inline
void InfomapGreedyTypeSpecialized<FlowType, WithMemory>::addContributionOfMovingMemoryNodes(NodeType& current,
	DeltaFlowType& oldModuleDelta, std::map<unsigned int, DeltaFlowType>& moduleDeltaFlow)
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
				double newPhysFlow = memNodeSet.sumFlow - physData.sumFlowFromStateNode;
				oldModuleDelta.sumDeltaPlogpPhysFlow += infomath::plogp(newPhysFlow) - infomath::plogp(oldPhysFlow);
				oldModuleDelta.sumPlogpPhysFlow += infomath::plogp(physData.sumFlowFromStateNode);
			}
			else // To where the multiple assigned node is moved
			{
				double oldPhysFlow = memNodeSet.sumFlow;
				double newPhysFlow = memNodeSet.sumFlow + physData.sumFlowFromStateNode;

				// moduleDeltaFlow[moduleIndex].addMemFlowTerms(infomath::plogp(newPhysFlow) - infomath::plogp(oldPhysFlow), infomath::plogp(physData.sumFlowFromStateNode));
				DeltaFlowType& physNeighbourDeltaFlow = moduleDeltaFlow[moduleIndex];
				physNeighbourDeltaFlow.module = moduleIndex; // Make sure module is correct if created new
				physNeighbourDeltaFlow.sumDeltaPlogpPhysFlow = infomath::plogp(newPhysFlow) - infomath::plogp(oldPhysFlow);
				physNeighbourDeltaFlow.sumPlogpPhysFlow = infomath::plogp(physData.sumFlowFromStateNode);
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
		memNodeSet.sumFlow -= physData.sumFlowFromStateNode;
		if (--memNodeSet.numMemNodes == 0)
			moduleToMemNodes.erase(overlapIt);

		// Add contribution to new module
		overlapIt = moduleToMemNodes.find(bestModuleIndex);
		if (overlapIt == moduleToMemNodes.end())
			moduleToMemNodes.insert(std::make_pair(bestModuleIndex, MemNodeSet(1, physData.sumFlowFromStateNode)));
		else {
			MemNodeSet& memNodeSet = overlapIt->second;
			++memNodeSet.numMemNodes;
			memNodeSet.sumFlow += physData.sumFlowFromStateNode;
		}
	}
}


template<typename FlowType>
inline
void InfomapGreedyTypeSpecialized<FlowType, WithMemory>::performPredefinedMoveOfMemoryNode(NodeType& current,
	unsigned int oldModuleIndex, unsigned int bestModuleIndex, DeltaFlowType& oldModuleDelta, DeltaFlowType& newModuleDelta)
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
		double newPhysFlow = memNodeSet.sumFlow - physData.sumFlowFromStateNode;
		oldModuleDelta.sumDeltaPlogpPhysFlow += infomath::plogp(newPhysFlow) - infomath::plogp(oldPhysFlow);
		oldModuleDelta.sumPlogpPhysFlow += infomath::plogp(physData.sumFlowFromStateNode);
		memNodeSet.sumFlow -= physData.sumFlowFromStateNode;
		if (--memNodeSet.numMemNodes == 0)
			moduleToMemNodes.erase(overlapIt);


		// Add contribution to new module
		overlapIt = moduleToMemNodes.find(bestModuleIndex);
		if (overlapIt == moduleToMemNodes.end())
		{
			moduleToMemNodes.insert(std::make_pair(bestModuleIndex, MemNodeSet(1, physData.sumFlowFromStateNode)));
			oldPhysFlow = 0.0;
			newPhysFlow = physData.sumFlowFromStateNode;
			newModuleDelta.sumDeltaPlogpPhysFlow += infomath::plogp(newPhysFlow) - infomath::plogp(oldPhysFlow);
			newModuleDelta.sumPlogpPhysFlow += infomath::plogp(physData.sumFlowFromStateNode);
		}
		else
		{
			MemNodeSet& memNodeSet = overlapIt->second;
			oldPhysFlow = memNodeSet.sumFlow;
			newPhysFlow = memNodeSet.sumFlow + physData.sumFlowFromStateNode;
			newModuleDelta.sumDeltaPlogpPhysFlow += infomath::plogp(newPhysFlow) - infomath::plogp(oldPhysFlow);
			newModuleDelta.sumPlogpPhysFlow += infomath::plogp(physData.sumFlowFromStateNode);
			++memNodeSet.numMemNodes;
			memNodeSet.sumFlow += physData.sumFlowFromStateNode;
		}

	}
}


template<typename FlowType>
double InfomapGreedyTypeSpecialized<FlowType, WithMemory>::getDeltaCodelengthOnMovingMemoryNode(DeltaFlowType& oldModuleDelta, DeltaFlowType& newModuleDelta)
{
	double delta_nodeFlow_log_nodeFlow = oldModuleDelta.sumDeltaPlogpPhysFlow + newModuleDelta.sumDeltaPlogpPhysFlow + oldModuleDelta.sumPlogpPhysFlow - newModuleDelta.sumPlogpPhysFlow;
	return -delta_nodeFlow_log_nodeFlow;
}

template<typename FlowType>
void InfomapGreedyTypeSpecialized<FlowType, WithMemory>::updateCodelengthOnMovingMemoryNode(DeltaFlowType& oldModuleDelta, DeltaFlowType& newModuleDelta)
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
	Super::root()->setChildDegree(Super::numLeafNodes());

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

			getNode(*modules[overlapIt->first]).physicalNodes.push_back(PhysData(i, overlapIt->second.sumFlow));
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
		NodeType& otherNode = getNode(*childIt);
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
	Super::root()->setChildDegree(Super::numLeafNodes());

	// Re-index physical nodes
	std::map<unsigned int, unsigned int> subPhysIndexMap;
	unsigned int subPhysIndex = 0;
	for (std::set<unsigned int>::iterator it(setOfPhysicalNodes.begin()); it != setOfPhysicalNodes.end(); ++it, ++subPhysIndex)
	{
		subPhysIndexMap.insert(std::make_pair(*it, subPhysIndex));
	}

	for (typename TreeData::leafIterator leafIt(Super::m_treeData.begin_leaf()); leafIt != Super::m_treeData.end_leaf(); ++leafIt, ++i)
	{
		NodeType& node = getNode(**leafIt);
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

	double parentExit = getNode(parent).data.exitFlow;

	Super::exitNetworkFlow = parentExit;
	Super::exitNetworkFlow_log_exitNetworkFlow = infomath::plogp(Super::exitNetworkFlow);
}



template<typename FlowType>
void InfomapGreedyTypeSpecialized<FlowType, WithMemory>::saveHierarchicalNetwork(HierarchicalNetwork& output, std::string rootName, bool includeLinks)
{
	HierarchicalNetwork& ioNetwork = output;

	ioNetwork.init(rootName, Super::hierarchicalCodelength, Super::oneLevelCodelength);

	unsigned int indexOffset = m_config.zeroBasedNodeNumbers ? 0 : 1;

	if (Super::m_config.printExpanded)
	{
		// Create vector of node names for memory nodes
		std::vector<std::string>& physicalNames = Super::m_nodeNames;
		std::vector<std::string> stateNodeNames(Super::m_treeData.numLeafNodes());
		unsigned int i = 0;
		for (typename TreeData::leafIterator leafIt(Super::m_treeData.begin_leaf()); leafIt != Super::m_treeData.end_leaf(); ++leafIt, ++i)
		{
			NodeType& node = getNode(**leafIt);
			StateNode& stateNode = node.stateNode;
			if (Super::m_config.isMultiplexNetwork())
				stateNodeNames[i] = io::Str() << physicalNames[stateNode.physIndex] << " | " << (stateNode.layer() + indexOffset);
			else
				stateNodeNames[i] = stateNode.print(physicalNames, indexOffset);
		}

		ioNetwork.prepareAddLeafNodes(Super::m_treeData.numLeafNodes());

		Super::buildHierarchicalNetworkHelper(ioNetwork, ioNetwork.getRootNode(), stateNodeNames);

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

	Log() << "merging " << Super::m_treeData.numLeafNodes() << " memory nodes within " <<
			leafModules.size() << " modules..." << std::flush;

	// Aggregate memory nodes with the same physical node if in the same module
	for (unsigned int i = 0; i < leafModules.size(); ++i)
	{
		NodeBase* leafModule = leafModules[i].first;
		std::map<unsigned int, IndexedFlow>& condensedNodes = physicalNodes[i];
		for (NodeBase::sibling_iterator childIt(leafModule->begin_child()), endIt(leafModule->end_child());
				childIt != endIt; ++childIt)
		{
			const NodeType& node = getNode(*childIt);
			std::pair<typename std::map<unsigned int, IndexedFlow>::iterator, bool> ret = condensedNodes.insert(std::make_pair(node.stateNode.physIndex, IndexedFlow(node.stateNode.physIndex, node.data)));
			if (!ret.second) // Add flow if physical node already exist
				ret.first->second.flowData += node.data; //TODO: If exitFlow should be correct, flow between memory nodes within same physical node should be subtracted.
			else // A new insertion was made
				++numCondensedNodes;
			memNodeIndexToLeafModuleIndex[node.originalIndex] = i;
		}
	}

	Log() << " to " << numCondensedNodes << " nodes... " << std::flush;
	ioNetwork.prepareAddLeafNodes(numCondensedNodes);

	unsigned int sortedNodeIndex = 0;
	for (unsigned int i = 0; i < leafModules.size(); ++i)
	{
		// Sort the nodes on flow
		std::map<unsigned int, IndexedFlow>& condensedNodes = physicalNodes[i];
		typedef typename std::map<unsigned int, IndexedFlow>::iterator CondensedIterator;
		std::multimap<double, CondensedIterator, std::greater<double> > sortedNodes;
		for (CondensedIterator it(condensedNodes.begin()); it != condensedNodes.end(); ++it)
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
			unsigned int physIndex = condensedIt->first;
			ioNetwork.addLeafNode(*parent, nodeData.flowData.flow, nodeData.flowData.exitFlow, Super::m_nodeNames[nodeData.index], sortedNodeIndex, nodeData.index, false, 0, physIndex);
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
			unsigned int sourceNodeIndex = condensedNodes.find(getNode(node).stateNode.physIndex)->second.index;

			for (NodeBase::edge_iterator outEdgeIt(node.begin_outEdge()), endIt(node.end_outEdge());
					outEdgeIt != endIt; ++outEdgeIt)
			{
				EdgeType& edge = **outEdgeIt;
				unsigned int targetLeafModuleIndex = memNodeIndexToLeafModuleIndex[edge.target.originalIndex];
				std::map<unsigned int, IndexedFlow>& targetCondensedNodes = physicalNodes[targetLeafModuleIndex];
				unsigned int targetNodeIndex = targetCondensedNodes.find(getNode(edge.target).stateNode.physIndex)->second.index;
				ioNetwork.addLeafEdge(sourceNodeIndex, targetNodeIndex, edge.data.flow);
			}
		}
	}
}

template<typename FlowType>
void InfomapGreedyTypeSpecialized<FlowType, WithMemory>::printClusterNumbers(std::ostream& out)
{
	unsigned int indexOffset = m_config.zeroBasedNodeNumbers ? 0 : 1;
	out << "# '" << m_config.parsedArgs << "' -> " << Super::numLeafNodes() << " nodes " <<
		"partitioned in " << m_config.elapsedTime() << " from codelength " <<
		io::toPrecision(Super::oneLevelCodelength, 9, true) << " in one level to codelength " <<
		io::toPrecision(Super::codelength, 9, true) << ".\n";

	if (m_config.printExpanded)
	{
		out << "# columns: from to moduleNr flow\n";

		out << "*Vertices " << Super::m_treeData.numLeafNodes() << "\n";
		for (typename TreeData::leafIterator leafIt(Super::m_treeData.begin_leaf()); leafIt != Super::m_treeData.end_leaf(); ++leafIt)
		{
			NodeType& node = getNode(**leafIt);
			StateNode& stateNode = node.stateNode;
			unsigned int index = node.parent->index; // module index
			out << stateNode.print() << " " << (index + 1) << " " << node.data.flow << "\n";
		}
	}
	else
	{
		out << "# columns: nodeIndex [(module1, flowInModule1), (module2, flowInModule2),...]\n";

		std::map<unsigned int, std::map<unsigned int, double> > modules;
		for (typename TreeData::leafIterator leafIt(Super::m_treeData.begin_leaf()); leafIt != Super::m_treeData.end_leaf(); ++leafIt)
		{
			NodeType& node = getNode(**leafIt);
			StateNode& stateNode = node.stateNode;
			unsigned int index = node.parent->index; // module index
			modules[stateNode.physIndex][index] += node.data.flow;
		}
		for (std::map<unsigned int, std::map<unsigned int, double> >::const_iterator it(modules.begin());
				it != modules.end(); ++it)
		{
			unsigned int physNode = it->first + indexOffset;
			const std::map<unsigned int, double>& overlapping = it->second;
			out << physNode << " [";
			for (std::map<unsigned int, double>::const_iterator moduleIt(overlapping.begin());
					moduleIt != overlapping.end(); ++moduleIt)
			{
				out << "(" << moduleIt->first << ", " << moduleIt->second << "), ";
			}
			out << "\b\b]\n";
		}
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
	unsigned int indexOffset = m_config.zeroBasedNodeNumbers ? 0 : 1;
	if (Super::m_config.printExpanded)
	{
		out << "# flow in network with " << Super::m_treeData.numLeafNodes() << " memory nodes (from-to) and " << Super::m_treeData.numLeafEdges() << " links\n";
		for (typename TreeData::leafIterator leafIt(Super::m_treeData.begin_leaf()); leafIt != Super::m_treeData.end_leaf(); ++leafIt)
		{
			NodeType& node = getNode(**leafIt);
			StateNode& stateNode = node.stateNode;
			out << "(" << stateNode.print(indexOffset) << ") (" << node.data << ")\n";
			for (NodeBase::edge_iterator edgeIt(node.begin_outEdge()), endEdgeIt(node.end_outEdge());
					edgeIt != endEdgeIt; ++edgeIt)
			{
				EdgeType& edge = **edgeIt;
				StateNode& stateTarget = getNode(edge.target).stateNode;
				out << "  --> " << "(" << stateTarget.print(indexOffset) << ") (" << edge.data.flow << ")\n";
			}
			for (NodeBase::edge_iterator edgeIt(node.begin_inEdge()), endEdgeIt(node.end_inEdge());
					edgeIt != endEdgeIt; ++edgeIt)
			{
				EdgeType& edge = **edgeIt;
				StateNode& stateSource = getNode(edge.source).stateNode;
				out << "  <-- " << "(" << stateSource.print(indexOffset) << ") (" << edge.data.flow << ")\n";
			}
		}
		return;
	}
	else
	{
		//TODO: Implement printing of flow network for unexpanded memory network!
		Log() << "Notice: Printing flow network currently only implemented for expanded memory network.\n";
	}

//	std::map<unsigned int, PhysNode> physicalNetwork;
//	for (typename TreeData::leafIterator leafIt(Super::m_treeData.begin_leaf()); leafIt != Super::m_treeData.end_leaf(); ++leafIt)
//	{
//		NodeType& node = getNode(**leafIt);
//		StateNode& stateNode = node.stateNode;
//
//		PhysNode& physNode = physicalNetwork[stateNode.physIndex];
//		physNode.flow += node.data.flow;
//
//		out << stateNode << " (" << node.data << ")\n";
//		for (NodeBase::edge_iterator edgeIt(node.begin_outEdge()), endEdgeIt(node.end_outEdge());
//				edgeIt != endEdgeIt; ++edgeIt)
//		{
//			EdgeType& edge = **edgeIt;
//			out << "  --> " << getNode(edge.target).stateNode << " (" << edge.data.flow << ")\n";
//		}
//		for (NodeBase::edge_iterator edgeIt(node.begin_inEdge()), endEdgeIt(node.end_inEdge());
//				edgeIt != endEdgeIt; ++edgeIt)
//		{
//			EdgeType& edge = **edgeIt;
//			out << "  <-- " << getNode(edge.source).stateNode << " (" << edge.data.flow << ")\n";
//		}
//	}

}



template<typename FlowType>
inline
std::vector<PhysData>& InfomapGreedyTypeSpecialized<FlowType, WithMemory>::getPhysicalMembers(NodeBase& node)
{
	return getNode(node).physicalNodes;
}

template<typename FlowType>
inline
StateNode& InfomapGreedyTypeSpecialized<FlowType, WithMemory>::getMemoryNode(NodeBase& node)
{
	return getNode(node).stateNode;
}

#ifdef NS_INFOMAP
}
#endif

#endif /* INFOMAPGREEDYTYPESPECIALIZED_H_ */
