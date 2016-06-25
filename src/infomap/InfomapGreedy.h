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


#ifndef INFOMAPGREEDY_H_
#define INFOMAPGREEDY_H_

#include <deque>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <utility>
#include <vector>

#include "../io/convert.h"
#include "../io/Config.h"
#include "../io/HierarchicalNetwork.h"
#include "../io/version.h"
#include "../utils/infomath.h"
#include "Edge.h"
#include "flowData.h"
#include "flowData_traits.h"
#include "InfomapBase.h"
#include "MemNetwork.h"
#include "Node.h"
#include "NodeFactory.h"
#include "treeIterators.h"
#include "TreeData.h"

#ifdef NS_INFOMAP
namespace infomap
{
#endif

template<typename InfomapImplementation>
class InfomapGreedy : public InfomapBase
{
public:
	typedef typename derived_traits<InfomapImplementation>::flow_type	FlowType;
	typedef typename flowData_traits<FlowType>::detailed_balance_type 	DetailedBalanceType;
	typedef typename flowData_traits<FlowType>::directed_with_recorded_teleportation_type DirectedWithRecordedTeleportationType;
	typedef typename flowData_traits<FlowType>::teleportation_type 		TeleportationType;
	typedef Node<FlowType>												NodeType;
	typedef Edge<NodeBase>												EdgeType;

	InfomapGreedy(const Config& conf, NodeFactoryBase* nodeFactory)
	:	InfomapBase(conf, nodeFactory),
	 	nodeFlow_log_nodeFlow(0.0),
		flow_log_flow(0.0),
		exit_log_exit(0.0),
		enter_log_enter(0.0),
		enterFlow(0.0),
		enterFlow_log_enterFlow(0.0),
	 	exitNetworkFlow(0.0),
	 	exitNetworkFlow_log_exitNetworkFlow(0.0)
	{
		FlowType& rootData = getNode(*root()).data;
		rootData.flow = 1.0;
		rootData.exitFlow = 0.0;
	}
	InfomapGreedy(const InfomapBase& infomap, NodeFactoryBase* nodeFactory)
	:	InfomapBase(infomap, nodeFactory),
	 	nodeFlow_log_nodeFlow(0.0),
		flow_log_flow(0.0),
		exit_log_exit(0.0),
		enter_log_enter(0.0),
		enterFlow(0.0),
		enterFlow_log_enterFlow(0.0),
	 	exitNetworkFlow(0.0),
	 	exitNetworkFlow_log_exitNetworkFlow(0.0)
	{
		FlowType& rootData = getNode(*root()).data;
		rootData.flow = 1.0;
		rootData.exitFlow = 0.0;
	}
	virtual ~InfomapGreedy() {}

protected:

	virtual void resetModuleFlowFromLeafNodes();

	virtual void resetModuleFlow(NodeBase& node);

	virtual void transformNodeFlowToEnterFlow(NodeBase* parent);

	virtual void cloneFlowData(const NodeBase& source, NodeBase& target);

	virtual void printNodeRanks(std::ostream& out);

	virtual void printFlowNetwork(std::ostream& out);

	virtual void sortTree(NodeBase& parent);

	virtual void saveHierarchicalNetwork(HierarchicalNetwork& output, std::string rootName, bool includeLinks);
	void buildHierarchicalNetworkHelper(HierarchicalNetwork& hierarchicalNetwork, HierarchicalNetwork::node_type& parent, std::vector<std::string>& leafNodeNames, NodeBase* node = 0);
	// Don't add leaf nodes, but collect all leaf modules instead
	void buildHierarchicalNetworkHelper(HierarchicalNetwork& hierarchicalNetwork, HierarchicalNetwork::node_type& parent, std::deque<std::pair<NodeBase*, HierarchicalNetwork::node_type*> >& leafModules, NodeBase* node = 0);

	NodeType& getNode(NodeBase& node);
	const NodeType& getNode(const NodeBase& node) const;

	unsigned int numActiveModules();
	virtual unsigned int numDynamicModules();

	virtual FlowDummy getNodeData(NodeBase& node);

	virtual void debugPrintInfomapTerms();

private:
	InfomapImplementation& getImpl();
	InfomapImplementation& getImpl(InfomapBase& infomap);

protected:
	std::vector<FlowType> m_moduleFlowData;
	std::vector<unsigned int> m_moduleMembers;
	std::vector<unsigned int> m_emptyModules;

	double nodeFlow_log_nodeFlow; // constant while the leaf network is the same
	double flow_log_flow; // node.(flow + exitFlow)
	double exit_log_exit;
	double enter_log_enter;
	double enterFlow;
	double enterFlow_log_enterFlow;

	// For hierarchical
	double exitNetworkFlow;
	double exitNetworkFlow_log_exitNetworkFlow;

};

template<typename InfomapImplementation>
inline
void InfomapGreedy<InfomapImplementation>::printNodeRanks(std::ostream& out)
{
	out << "#node-flow\n";
	for (TreeData::leafIterator it(m_treeData.begin_leaf()), itEnd(m_treeData.end_leaf());
			it != itEnd; ++it)
	{
		out << getNode(**it).data.flow << '\n';
	}
}

template<typename InfomapImplementation>
inline
void InfomapGreedy<InfomapImplementation>::printFlowNetwork(std::ostream& out)
{
	unsigned int indexOffset = m_config.zeroBasedNodeNumbers ? 0 : 1;
	for (TreeData::leafIterator nodeIt(m_treeData.begin_leaf());
			nodeIt != m_treeData.end_leaf(); ++nodeIt)
	{
		NodeBase& node = **nodeIt;
		out << node.originalIndex + indexOffset << " (" << getNode(node).data << ")\n";
		for (NodeBase::edge_iterator edgeIt(node.begin_outEdge()), endEdgeIt(node.end_outEdge());
				edgeIt != endEdgeIt; ++edgeIt)
		{
			EdgeType& edge = **edgeIt;
			out << "  --> " << edge.target.originalIndex + indexOffset << " (" << edge.data.flow << ")\n";
		}
		for (NodeBase::edge_iterator edgeIt(node.begin_inEdge()), endEdgeIt(node.end_inEdge());
				edgeIt != endEdgeIt; ++edgeIt)
		{
			EdgeType& edge = **edgeIt;
			out << "  <-- " << edge.source.originalIndex + indexOffset << " (" << edge.data.flow << ")\n";
		}
	}
}

template<typename InfomapImplementation>
inline
void InfomapGreedy<InfomapImplementation>::sortTree(NodeBase& parent)
{
	if (parent.getSubInfomap() != 0)
	{
		parent.getSubInfomap()->sortTree();
	}
	std::multimap<double, NodeBase*, std::greater<double> > sortedModules;
	for (NodeBase::sibling_iterator childIt(parent.begin_child()), endChildIt(parent.end_child());
			childIt != endChildIt; ++childIt)
	{
		sortTree(*childIt);
		double rank = getNode(*childIt).data.flow;
		sortedModules.insert(std::pair<double, NodeBase*>(rank, childIt.base()));
	}
	parent.releaseChildren();
	unsigned int sortedIndex = 0;
	for (std::multimap<double, NodeBase*>::iterator it(sortedModules.begin()), itEnd(sortedModules.end());
			it != itEnd; ++it, ++sortedIndex)
	{
		parent.addChild(it->second);
		it->second->index = sortedIndex;
	}
}


template<typename InfomapImplementation>
inline
void InfomapGreedy<InfomapImplementation>::saveHierarchicalNetwork(HierarchicalNetwork& output, std::string rootName, bool includeLinks)
{
	output.init(rootName, hierarchicalCodelength, oneLevelCodelength);

	output.prepareAddLeafNodes(m_treeData.numLeafNodes());

	buildHierarchicalNetworkHelper(output, output.getRootNode(), m_nodeNames);

	if (includeLinks)
	{
		for (TreeData::leafIterator leafIt(m_treeData.begin_leaf()); leafIt != m_treeData.end_leaf(); ++leafIt)
		{
			NodeBase& node = **leafIt;
			for (NodeBase::edge_iterator outEdgeIt(node.begin_outEdge()), endIt(node.end_outEdge());
					outEdgeIt != endIt; ++outEdgeIt)
			{
				EdgeType& edge = **outEdgeIt;
				output.addLeafEdge(edge.source.originalIndex, edge.target.originalIndex, edge.data.flow);
			}
		}
	}
}

template<typename InfomapImplementation>
inline
void InfomapGreedy<InfomapImplementation>::buildHierarchicalNetworkHelper(HierarchicalNetwork& hierarchicalNetwork, HierarchicalNetwork::node_type& parent, std::vector<std::string>& leafNodeNames, NodeBase* rootNode)
{
	if (rootNode == 0)
		rootNode = root();

	if (rootNode->getSubInfomap() != 0)
	{
		getImpl(*rootNode->getSubInfomap()).buildHierarchicalNetworkHelper(hierarchicalNetwork, parent, leafNodeNames);
		return;
	}

	for (NodeBase::sibling_iterator childIt(rootNode->begin_child()), endIt(rootNode->end_child());
			childIt != endIt; ++childIt)
	{
		const NodeType& node = getNode(*childIt);
		if (node.isLeaf())
		{
			if (m_config.isMemoryNetwork()) {
				const StateNode& stateNode = getMemoryNode(*childIt);
				hierarchicalNetwork.addLeafNode(parent, node.data.flow, node.data.exitFlow, leafNodeNames[node.originalIndex],
						node.originalIndex, node.originalIndex, true, stateNode.stateIndex, stateNode.physIndex);

			}
			else
				hierarchicalNetwork.addLeafNode(parent, node.data.flow, node.data.exitFlow, leafNodeNames[node.originalIndex],
				node.originalIndex, node.originalIndex, false, 0, node.originalIndex);
		}
		else
		{
			SNode& newParent = hierarchicalNetwork.addNode(parent, node.data.flow, node.data.exitFlow);
			buildHierarchicalNetworkHelper(hierarchicalNetwork, newParent, leafNodeNames, childIt.base());
		}
	}
}

template<typename InfomapImplementation>
inline
void InfomapGreedy<InfomapImplementation>::buildHierarchicalNetworkHelper(HierarchicalNetwork& hierarchicalNetwork, HierarchicalNetwork::node_type& parent, std::deque<std::pair<NodeBase*, HierarchicalNetwork::node_type*> >& leafModules, NodeBase* rootNode)
{
	if (rootNode == 0)
		rootNode = root();

	if (rootNode->getSubInfomap() != 0)
	{
		getImpl(*rootNode->getSubInfomap()).buildHierarchicalNetworkHelper(hierarchicalNetwork, parent, leafModules);
		return;
	}

//	if (rootNode->childDegree() == 0)
//		return;
	if (rootNode->firstChild->isLeaf())
		leafModules.push_back(std::make_pair(rootNode, &parent));
	else
	{
		for (NodeBase::sibling_iterator childIt(rootNode->begin_child()), endIt(rootNode->end_child());
				childIt != endIt; ++childIt)
		{
			const NodeType& node = getNode(*childIt);
			SNode& newParent = hierarchicalNetwork.addNode(parent, node.data.flow, node.data.exitFlow);
			buildHierarchicalNetworkHelper(hierarchicalNetwork, newParent, leafModules, childIt.base());
		}
	}
}

template<typename InfomapImplementation>
void InfomapGreedy<InfomapImplementation>::resetModuleFlowFromLeafNodes()
{
	// Reset from top to bottom
	resetModuleFlow(*root());

	// Aggregate from bottom to top
	for (TreeData::leafIterator leafIt(m_treeData.begin_leaf()); leafIt != m_treeData.end_leaf(); ++leafIt)
	{
		NodeBase* node = *leafIt;
		double flow = getNode(*node).data.flow;
		while (node = node->parent, node != 0)
		{
			getNode(*node).data.flow += flow;
		}
	}
}

template<typename InfomapImplementation>
void InfomapGreedy<InfomapImplementation>::resetModuleFlow(NodeBase& node)
{
	getNode(node).data.flow = 0.0;
	for (NodeBase::sibling_iterator childIt(node.begin_child()), endIt(node.end_child());
			childIt != endIt; ++childIt)
	{
		if (!childIt->isLeaf())
			resetModuleFlow(*childIt);
	}
}

template<typename InfomapImplementation>
inline
void InfomapGreedy<InfomapImplementation>::transformNodeFlowToEnterFlow(NodeBase* parent)
{
	for (NodeBase::sibling_iterator moduleIt(parent->begin_child()), endIt(parent->end_child());
			moduleIt != endIt; ++moduleIt)
	{
		NodeType& module = getNode(*moduleIt);
		module.data.flow = module.data.enterFlow;
	}
}

template<typename InfomapImplementation>
void InfomapGreedy<InfomapImplementation>::cloneFlowData(const NodeBase& source, NodeBase& target)
{
	getNode(target).data = getNode(source).data;
}

template<typename InfomapImplementation>
inline
typename InfomapGreedy<InfomapImplementation>::NodeType& InfomapGreedy<InfomapImplementation>::getNode(NodeBase& node)
{
	return static_cast<NodeType&>(node);
}

template<typename InfomapImplementation>
inline
const typename InfomapGreedy<InfomapImplementation>::NodeType& InfomapGreedy<InfomapImplementation>::getNode(const NodeBase& node) const
{
	return static_cast<const NodeType&>(node);
}

template<typename InfomapImplementation>
inline
unsigned int InfomapGreedy<InfomapImplementation>::numActiveModules()
{
	return m_activeNetwork.size() - m_emptyModules.size();
}

template<typename InfomapImplementation>
inline
unsigned int InfomapGreedy<InfomapImplementation>::numDynamicModules()
{
	return m_activeNetwork.size() - m_emptyModules.size();
}

template<typename InfomapImplementation>
inline
InfomapImplementation& InfomapGreedy<InfomapImplementation>::getImpl()
{
	return static_cast<InfomapImplementation&>(*this);
}

template<typename InfomapImplementation>
inline
InfomapImplementation& InfomapGreedy<InfomapImplementation>::getImpl(InfomapBase& infomap)
{
	return static_cast<InfomapImplementation&>(infomap);
}

template<typename InfomapImplementation>
inline
FlowDummy InfomapGreedy<InfomapImplementation>::getNodeData(NodeBase& node)
{
	return FlowDummy(getNode(node).data);
}

template<typename InfomapImplementation>
inline
void InfomapGreedy<InfomapImplementation>::debugPrintInfomapTerms()
{
	Log() << "(moduleLength: " << -exit_log_exit << " + " << flow_log_flow << " - " << nodeFlow_log_nodeFlow << " = " << moduleCodelength <<")\n";
}

#ifdef NS_INFOMAP
}
#endif

#endif /* INFOMAPGREEDY_H_ */
