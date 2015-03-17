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


#ifndef TREEDATA_H_
#define TREEDATA_H_
#include <string>
#include <vector>
//#include "Edge.h"
#include "Node.h"
#include "NodeFactory.h"
#include <memory>

#ifdef NS_INFOMAP
namespace infomap
{
#endif

class TreeData
{
	friend class InfomapBase; // Expose m_leafNodes to InfomapBase to use as active network in fine-tune
public:
	typedef std::vector<NodeBase*>::iterator		leafIterator;
	typedef std::vector<NodeBase*>::const_iterator	const_leafIterator;
	typedef NodeBase::EdgeType						EdgeType;

	TreeData(NodeFactoryBase*);
	virtual ~TreeData();

	void readFromSubNetwork(NodeBase* parent);

	const NodeFactoryBase& nodeFactory() const { return *m_nodeFactory; }

	// ---------------------------- Iterators and element access ----------------------------
	leafIterator begin_leaf()
	{ return m_leafNodes.begin(); }

	leafIterator end_leaf()
	{ return m_leafNodes.end(); }

	const_leafIterator begin_leaf() const
	{ return m_leafNodes.begin(); }

	const_leafIterator end_leaf() const
	{ return m_leafNodes.end(); }

	NodeBase* root()
	{ return m_root; }

	const NodeBase* root() const
	{ return m_root; }

	NodeBase& getLeafNode(unsigned int index)
	{
		ASSERT(index < m_leafNodes.size());
		return *m_leafNodes[index];
	}

	const NodeBase& getLeafNode(unsigned int index) const
	{
		ASSERT(index < m_leafNodes.size());
		return *m_leafNodes[index];
	}

	// ---------------------------- Capacity: ----------------------------

	unsigned int numLeafNodes() const
	{ return m_leafNodes.size(); }

	unsigned int numLeafEdges() const
	{ return m_numLeafEdges; }

	unsigned int calcSize();

	// ---------------------------- Manipulation: ----------------------------

	void reserveNodeCount(unsigned int nodeCount)
	{
		m_leafNodes.reserve(nodeCount);
	}

	void addNewNode(const NodeBase& other)
	{
		NodeBase* node = m_nodeFactory->createNode(other);
		m_root->addChild(node);
		node->originalIndex = m_leafNodes.size();
		m_leafNodes.push_back(node);
	}

	void addNewNode(std::string name, double flow, double teleportWeight)
	{
		NodeBase* node = m_nodeFactory->createNode(name, flow, teleportWeight);
		m_root->addChild(node);
		node->originalIndex = m_leafNodes.size();
		m_leafNodes.push_back(node);
	}

	void addClonedNode(NodeBase* node)
	{
		m_root->addChild(node);
		m_leafNodes.push_back(node);
	}

	void addEdge(unsigned int sourceIndex, unsigned int targetIndex, double weight, double flow)
	{
		NodeBase* source = m_leafNodes[sourceIndex];
		NodeBase* target = m_leafNodes[targetIndex];
		source->addOutEdge(*target, weight, flow);
		++m_numLeafEdges;
//		EdgeType* edge = source->addOutEdge(*target, weight);
//		m_leafEdges.push_back(edge);
	}



private:
	std::auto_ptr<NodeFactoryBase> m_nodeFactory;
	NodeBase* m_root;
	std::vector<NodeBase*> m_leafNodes;
	unsigned int m_numLeafEdges;
//	std::vector<EdgeType*> m_leafEdges;

};

#ifdef NS_INFOMAP
}
#endif

#endif /* TREEDATA_H_ */
