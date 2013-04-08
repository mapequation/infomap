/* ------------------------------------------------------------------------

 Infomap software package for multi-level network clustering

 * Copyright (c) 2013
 * For license and authors, see README.txt or http://www.mapequation.org

------------------------------------------------------------------------ */

#include "TreeData.h"
//#include "flowData.h"
//#include "Node.h"

//template class TreeData<Node<FlowUndirected> >;
//template class TreeData<Node<FlowDirected> >;

TreeData::TreeData(NodeFactoryBase* nodeFactory)
:	m_nodeFactory(nodeFactory),
	m_numLeafEdges(0)
{
	m_root = m_nodeFactory->createNode("root", 1.0);
}

TreeData::~TreeData()
{
//	RELEASE_OUT("~TreeData() -> delete " << *m_root << "... #NodeCount: " << NodeBase::nodeCount());
	delete m_root;
//	RELEASE_OUT(" -> " << NodeBase::nodeCount() << "\n");
}

/**
 * Create new leaf nodes by copying the children of <code>parent</code>
 * TODO: Should let parent be const, but setting its index property to be able to rebuild the edge structure.
 * Do that in a preparation step instead.
 */
void TreeData::readFromSubNetwork(NodeBase* parent)
{
	// Clone all nodes
	unsigned int numNodes = parent->childDegree();
	reserveNodeCount(numNodes);
	unsigned int i = 0;
	for (NodeBase::sibling_iterator childIt(parent->begin_child()), endIt(parent->end_child());
			childIt != endIt; ++childIt, ++i)
	{
		addNewNode(*childIt); // New node copies the data from the other
		childIt->index = i; // Set index to its place in this subnetwork to be able to find edge target below
	}

	// Clone edges
	for (NodeBase::sibling_iterator childIt(parent->begin_child()), endIt(parent->end_child());
			childIt != endIt; ++childIt)
	{
		NodeBase& node = *childIt;
		for (NodeBase::edge_iterator outEdgeIt(node.begin_outEdge()), endIt(node.end_outEdge());
				outEdgeIt != endIt; ++outEdgeIt)
		{
			EdgeType edge = **outEdgeIt;
			// If neighbour node is within the same cluster, add the link to this subnetwork.
			if (edge.target.parent == parent)
			{
				addEdge(node.index, edge.target.index, edge.data.weight, edge.data.flow);
			}
			// else flow out of sub-network
		}
	}
}

unsigned int TreeData::calcSize()
{
	unsigned int numNodes = 0;
	for (NodeBase::const_dfs_pre_order_iterator nodeIt(m_root->begin_DFS());
			nodeIt != m_root->end_DFS(); ++nodeIt, ++numNodes)
	{}
	return numNodes;
}
