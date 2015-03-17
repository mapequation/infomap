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


#include "TreeData.h"

#ifdef NS_INFOMAP
namespace infomap
{
#endif

TreeData::TreeData(NodeFactoryBase* nodeFactory)
:	m_nodeFactory(nodeFactory),
	m_numLeafEdges(0)
{
	m_root = m_nodeFactory->createNode("root", 1.0);
}

TreeData::~TreeData()
{
	delete m_root;
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
	for (NodeBase::const_pre_depth_first_iterator nodeIt(m_root); !nodeIt.isEnd(); ++nodeIt, ++numNodes)
	{}
	return numNodes;
}

#ifdef NS_INFOMAP
}
#endif
