/* ------------------------------------------------------------------------

 Infomap software package for multi-level network clustering

 * Copyright (c) 2013
 * For license and authors, see README.txt or http://www.mapequation.org

------------------------------------------------------------------------ */

#include "TreeDataWriter.h"
#include <iostream>


TreeDataWriter::TreeDataWriter(const TreeData& tree) :
m_tree(tree)
{
}

/**
 * Print the module indices down to the leaf nodes like:
 * 0:		First super module
 * 0:0:		First sub module
 * 0:0:0:	Leaf node
 * 0:0:1:	Leaf node
 * 0:1:		Second sub module
 * 0:1:0:	Leaf node
 * 1:		Second super module
 * 1:0:		...
 */
void TreeDataWriter::writeTree(std::ostream& out, bool collapseLeafs)
{
	uint oldDepth = 0;
	std::vector<unsigned int> indexStack;
	const NodeBase* root = m_tree.root();
	for (NodeBase::const_dfs_pre_order_iterator nodeIt(root->begin_DFS());
			nodeIt != root->end_DFS(); ++nodeIt)
	{
		if (collapseLeafs)
		{
			if (nodeIt->isLeaf())
			{
				if (nodeIt->next == 0)
				{
//					for (unsigned int i = 0; i < indexStack.size(); ++i)
//						out << "  ";
//					out << "(" << nodeIt->parent->childDegree() << ")" << std::endl;
					out << "(" << nodeIt->parent->childDegree() << ")\t(node id: " << *nodeIt->parent << ")" << std::endl;
				}
				continue;
			}
		}
		if (nodeIt.base() == root)
		{
//			indexStack.push_back(0);
		}
		else if (nodeIt.depth() == oldDepth) // ->next
		{
			++indexStack.back();
		}
		else if (nodeIt.depth() > oldDepth) // ->firstChild
		{
			indexStack.push_back(0);
		}
		else // ->parent[->parent->...]->sibling
		{
			while (indexStack.size() > nodeIt.depth())
			{
				indexStack.pop_back();
			}
			++indexStack.back();
		}
		oldDepth = nodeIt.depth();

//		writeIndexStack(out, indexStack);
		std::copy(indexStack.begin(), indexStack.end(), std::ostream_iterator<uint>(out,":"));
		if (collapseLeafs && nodeIt.next()->isLeaf())
			continue;
		out << "\t(node id: " << *nodeIt << ")" << std::endl;
	}
}

void TreeDataWriter::writeTopGraph(std::ostream& out)
{
//	const NodeBase& root = *m_tree.root();
//	for (NodeBase::sibling_iterator childIt(root.begin_child()); childIt != root.end_child(); ++childIt)
//	{
//		const NodeBase& node = *childIt;
//		out << "Node " << node << std::endl;
//		for (NodeBase::const_edge_iterator outEdgeIt(node.begin_outEdge()); outEdgeIt != node.end_outEdge(); ++outEdgeIt)
//		{
//			EdgeType& edge = **outEdgeIt;
//			out << "\t--> " << edge.target << ", flow: " << edge.data.flow << std::endl;
//		}
//	}
}

void TreeDataWriter::writeLeafNodes(std::ostream& out)
{
	out << "Writing leaf network with " << m_tree.numLeafNodes() << " nodes and " << m_tree.numLeafEdges() << " edges.\n";
	unsigned int i = 0;
	for (TreeData::const_leafIterator nodeIt(m_tree.begin_leaf());
			nodeIt != m_tree.end_leaf(); ++nodeIt, ++i)
	{
		out << i << " (" << **nodeIt << ")" << std::endl;
	}
}
