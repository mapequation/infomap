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


#ifndef NODE_H_
#define NODE_H_
#include <vector>
#include <string>
#include <ostream>
#include <sstream>
#include "Edge.h"
#include "treeIterators.h"
#include "../utils/gap_iterator.h"
#include "../utils/Logger.h"
#include <memory>

#ifdef NS_INFOMAP
namespace infomap
{
#endif

class InfomapBase;

struct SubStructure
{
	SubStructure();
	~SubStructure();
	std::auto_ptr<InfomapBase> subInfomap;
	bool haveSubInfomapInstance() { return subInfomap.get() != 0; }
	bool exploredWithoutImprovement;
};

struct StateNode
{
	unsigned int stateIndex;
	unsigned int physIndex;
	double weight;
	StateNode() :
		stateIndex(0), physIndex(0), weight(0.0)
	{}
	StateNode(unsigned int physIndex) :
		stateIndex(0), physIndex(physIndex), weight(0.0)
	{}
	StateNode(unsigned int stateIndex, unsigned int physIndex, double weight = 0.0) :
		stateIndex(stateIndex), physIndex(physIndex), weight(weight)
	{}
	StateNode(const StateNode& other) :
		stateIndex(other.stateIndex), physIndex(other.physIndex), weight(other.weight)
	{}
	StateNode& operator=(const StateNode& other)
	{
		stateIndex = other.stateIndex;
		physIndex = other.physIndex;
		weight = other.weight;
		return *this;
	}

	unsigned int getPriorState() const
	{
		return stateIndex;
	}

	unsigned int layer() const
	{
		return stateIndex;
	}

	void subtractIndexOffset(unsigned int indexOffset)
	{
		stateIndex -= indexOffset;
		physIndex -= indexOffset;
	}

	bool operator<(StateNode other) const
	{
		return stateIndex == other.stateIndex ? physIndex < other.physIndex : stateIndex < other.stateIndex;
	}

	bool operator==(StateNode other) const
	{
		return stateIndex == other.stateIndex && physIndex == other.physIndex;
	}

	bool operator!=(StateNode other) const
	{
		return stateIndex != other.stateIndex || physIndex != other.physIndex;
	}

	friend std::ostream& operator<<(std::ostream& out, const StateNode& node)
	{
		return out << "(" << node.stateIndex << "-" << node.physIndex << ")";
	}

	std::string print(unsigned int indexOffset = 0) const
	{
		std::ostringstream out;
		out << stateIndex + indexOffset << " " << physIndex + indexOffset;
		return out.str();
	}

	std::string print(const std::vector<std::string>& names, unsigned int indexOffset = 0) const
	{
		std::ostringstream out;
		out << stateIndex + indexOffset << " " << names.at(physIndex);
		return out.str();
	}
};

class NodeBase
{
public:
	typedef Edge<NodeBase>	EdgeType;

	// Iterators
	typedef SiblingIterator<NodeBase*>					sibling_iterator;
	typedef SiblingIterator<NodeBase const*>			const_sibling_iterator;
	typedef LeafNodeIterator<NodeBase*>					leaf_iterator;
	typedef LeafNodeIterator<NodeBase const*>			const_leaf_iterator;
	typedef LeafModuleIterator<NodeBase*>				leaf_module_iterator;
	typedef LeafModuleIterator<NodeBase const*>			const_leaf_module_iterator;

	typedef DepthFirstIterator<NodeBase*, true>			pre_depth_first_iterator;
	typedef DepthFirstIterator<NodeBase const*, true>	const_pre_depth_first_iterator;
	typedef DepthFirstIterator<NodeBase*, false>		post_depth_first_iterator;
	typedef DepthFirstIterator<NodeBase const*, false>	const_post_depth_first_iterator;

	typedef std::vector<EdgeType*>::iterator			edge_iterator;
	typedef std::vector<EdgeType*>::const_iterator		const_edge_iterator;
	typedef gap_iterator<edge_iterator>					inout_edge_iterator;


	NodeBase();
	NodeBase(std::string name);
	NodeBase(const NodeBase& other);

	virtual ~NodeBase();

	static unsigned int nodeCount() { return s_nodeCount; }

	// ---------------------------- Tree iterators ----------------------------

	sibling_iterator begin_child()
	{ return sibling_iterator(firstChild); }

	sibling_iterator end_child()
	{ return sibling_iterator(0); }

	const_sibling_iterator begin_child() const
	{ return const_sibling_iterator(firstChild); }

	const_sibling_iterator end_child() const
	{ return const_sibling_iterator(0); }

	leaf_iterator begin_leaf()
	{ return leaf_iterator(firstChild); }

	leaf_iterator end_leaf()
	{ return leaf_iterator(0); }

	pre_depth_first_iterator begin_depthFirst()
	{ return pre_depth_first_iterator(this); }

	pre_depth_first_iterator end_depthFirst()
	{ return pre_depth_first_iterator(0); }

	const_pre_depth_first_iterator begin_depthFirst() const
	{ return const_pre_depth_first_iterator(this); }

	const_pre_depth_first_iterator end_depthFirst() const
	{ return const_pre_depth_first_iterator(0); }

	// ---------------------------- Graph iterators ----------------------------

	edge_iterator begin_outEdge()
	{ return m_outEdges.begin(); }

	edge_iterator end_outEdge()
	{ return m_outEdges.end(); }

	edge_iterator begin_inEdge()
	{ return m_inEdges.begin(); }

	edge_iterator end_inEdge()
	{ return m_inEdges.end(); }

	inout_edge_iterator begin_inoutEdge()
	{ return inout_edge_iterator(begin_inEdge(), end_inEdge(), begin_outEdge()); }

	inout_edge_iterator	end_inoutEdge()
	{ return inout_edge_iterator(end_outEdge()); }

	// ---------------------------- Capacity ----------------------------

	unsigned int childDegree() const;

	bool isLeaf() const;
	bool isLeafModule() const;
	bool isRoot() const;

	unsigned int numLeafMembers()
	{ return m_numLeafMembers; }

	bool isDangling()
	{ return m_outEdges.empty(); }

	unsigned int outDegree()
	{ return m_outEdges.size(); }

	unsigned int inDegree()
	{ return m_inEdges.size(); }

	unsigned int degree()
	{ return outDegree() + inDegree(); }

	InfomapBase* getSubInfomap()
	{ return m_subStructure.subInfomap.get(); }

	const InfomapBase* getSubInfomap() const
	{ return m_subStructure.subInfomap.get(); }

	SubStructure& getSubStructure()
	{ return m_subStructure; }

	const SubStructure& getSubStructure() const
	{ return m_subStructure; }

	const NodeBase* getSubInfomapRoot() const;

	NodeBase* getSubInfomapRoot();

	// ---------------------------- Order ----------------------------
	bool isFirst() const
	{ return !parent || parent->firstChild == this; }

	bool isLast() const
	{ return !parent || parent->lastChild == this; }

	unsigned int childIndex() const;

	// ---------------------------- Operators ----------------------------

	bool operator ==(const NodeBase& rhs) const
	{ return this == &rhs; }
//	{ return id == rhs.id; }

	bool operator !=(const NodeBase& rhs) const
	{ return this != &rhs; }
//	{ return id != rhs.id; }

	friend std::ostream& operator<<(std::ostream& out, const NodeBase& node)
	{
//		return out << "n" << node.id;// << " (" << node.data << ")";
//		return out << node.name << " (" << node.data << ")";
		if (node.name.empty())
			return out << "n" << node.id;
		return out << "n" << node.id << ":" << node.name;
	}

//	friend std::ostream& operator<<(std::ostream& out, const NodeBase* node)
//	{
//		return node == 0 ? (out << "null") : (out << *node);
//	}

	//debug
	virtual void printData(std::ostream& out)
	{
	}

	//debug
	std::string getNeighbourhood()
	{
		std::ostringstream oss;
		oss << "parent: " << parent << ", prev: " << previous << ", next: " << next <<
				", firstChild: " << firstChild << ", lastChild: " << lastChild <<
				", childDegree: " << m_childDegree;
		return oss.str();
	}

	// ---------------------------- Mutators ----------------------------

	// After change, set the child degree if known instead of lazily computing it by traversing the linked list
	void setChildDegree(unsigned int value);

	void setNumLeafNodes(unsigned int value);

	void addChild(NodeBase* child);

	void releaseChildren();

	/**
	 * @return 1 if the node is removed, otherwise 0
	 */
	unsigned int replaceWithChildren();

	void replaceWithChildrenDebug();

	/**
	 * @return The number of children removed
	 */
	unsigned int replaceChildrenWithGrandChildren();

	void replaceChildrenWithGrandChildrenDebug();

	void remove(bool removeChildren);

	void deleteChildren();

	EdgeType* addOutEdge(NodeBase& target, double weight, double flow = 0.0)
	{
		EdgeType* edge = new EdgeType(*this, target, weight, flow);
		m_outEdges.push_back(edge);
		target.m_inEdges.push_back(edge);
		return edge;
	}

//	/**
//	 * Attach an instance of Infomap on the node to have its life cycle synchronized with the parent node.
//	 * @Note If an Infomap instance already exists, it will be deleted first.
//	 */
//	void attachSubInfomapInstance(InfomapBase* subInfomap)
//	{
//		m_subStructure.subInfomap = subInfomap;
//	}

	// Dummy node for non-memory nodes
	virtual StateNode getStateNode()
	{
		return StateNode();
	}

	virtual unsigned int getPhysicalIndex()
	{
		return originalIndex;
	}

private:
	void calcChildDegree();

public:
	unsigned long id;
	std::string name;
	unsigned int index; // Temporary index used in finding best module
	unsigned int originalIndex; // Index in the original network (for leaf nodes)

	NodeBase* parent;
	NodeBase* previous; // sibling
	NodeBase* next; // sibling
	NodeBase* firstChild;
	NodeBase* lastChild;
	NodeBase* owner; // Infomap owner (if this is an Infomap root)
	double codelength; //TODO: Better design for hierarchical stuff!?
	bool dirty;

protected:
	SubStructure m_subStructure;
	unsigned int m_childDegree;
	bool m_childrenChanged;
	unsigned int m_numLeafMembers;

	static long s_nodeCount;
	static unsigned long s_UID;

	std::vector<EdgeType*> m_outEdges;
	std::vector<EdgeType*> m_inEdges;

	// TODO: Add list of physical nodes for memory nodes, see Node.h#26

};

inline
bool NodeBase::isLeaf() const
{
	return firstChild == 0;
}

inline
bool NodeBase::isLeafModule() const
{
	return firstChild != 0 && firstChild->firstChild == 0;
}

inline
bool NodeBase::isRoot() const
{
	return parent == 0;
}

inline
unsigned int NodeBase::childIndex() const
{
	unsigned int childIndex = 0;
	const NodeBase* n(this);
	while (n->previous) {
		n = n->previous;
		++childIndex;
	}
	return childIndex;
}

inline
unsigned int NodeBase::childDegree() const
{
//	if (!m_childrenChanged)
		return m_childDegree;
//	calcChildDegree();
//	return m_childDegree;
}

inline
void NodeBase::addChild(NodeBase* child)
{
//	m_childrenChanged = true;
	if (firstChild == 0)
	{
//		DEBUG_OUT("\t\t-->Add node " << *child << " as FIRST CHILD to parent " << *this << std::endl);
		child->previous = 0;
		firstChild = child;
	}
	else
	{
//		DEBUG_OUT("\t\t-->Add node " << *child << " as LAST CHILD to parent " << *this << std::endl);
		child->previous = lastChild;
		lastChild->next = child;
	}
	lastChild = child;
	child->next = 0;
	child->parent = this;
	++m_childDegree;
}

inline
void NodeBase::releaseChildren()
{
	firstChild = 0;
	lastChild = 0;
	m_childDegree = 0;
}

inline
void NodeBase::remove(bool removeChildren)
{
	firstChild = removeChildren ? 0 : firstChild;
	delete this;
}

inline
unsigned int NodeBase::replaceChildrenWithGrandChildren()
{
	if (firstChild == 0)
		return 0;
	NodeBase::sibling_iterator nodeIt = begin_child();
	unsigned int numOriginalChildrenLeft = m_childDegree;
	unsigned int numChildrenReplaced = 0;
	do
	{
		NodeBase* n = nodeIt.base();
		++nodeIt;
		numChildrenReplaced += n->replaceWithChildren();
	}
	while (--numOriginalChildrenLeft != 0);
	return numChildrenReplaced;
}

inline
unsigned int NodeBase::replaceWithChildren()
{
	if (isLeaf() || isRoot())
		return 0;

	// Reparent children
	unsigned int deltaChildDegree = 0;
	NodeBase* child = firstChild;
	do
	{
		child->parent = parent;
		child = child->next;
		++deltaChildDegree;
	} while (child != 0);
	parent->m_childDegree += deltaChildDegree - 1; // -1 as this node is deleted

	if (parent->firstChild == this) {
		parent->firstChild = firstChild;
	} else {
		previous->next = firstChild;
		firstChild->previous = previous;
	}

	if (parent->lastChild == this) {
		parent->lastChild = lastChild;
	} else {
		next->previous = lastChild;
		lastChild->next = next;
	}

	// Release connected nodes before delete, otherwise children are deleted and neighbours are reconnected.
	firstChild = 0;
	next = 0;
	previous = 0;
	parent = 0;
	delete this;
	return 1;
}

inline
void NodeBase::replaceChildrenWithGrandChildrenDebug()
{
	if (firstChild == 0)
		return;
	NodeBase::sibling_iterator nodeIt = begin_child();
	unsigned int numOriginalChildrenLeft = m_childDegree;
	do
	{
		NodeBase* n = nodeIt.base();
		++nodeIt;
		n->replaceWithChildrenDebug();
	}
	while (--numOriginalChildrenLeft != 0);
}


inline
void NodeBase::replaceWithChildrenDebug()
{
	if (isLeaf() || isRoot())
		return;


	// Reparent children
	unsigned int deltaChildDegree = 0;
	NodeBase* child = firstChild;
	do
	{
		child->parent = parent;
		child = child->next;
		++deltaChildDegree;
	} while (child != 0);
	parent->m_childDegree += deltaChildDegree - 1; // -1 as this node is deleted

	if (parent->firstChild == this) {
		parent->firstChild = firstChild;
	} else {
		previous->next = firstChild;
		firstChild->previous = previous;
	}

	if (parent->lastChild == this) {
		parent->lastChild = lastChild;
	} else {
		next->previous = lastChild;
		lastChild->next = next;
	}

	// Release connected nodes before delete, otherwise children are deleted and neighbours are reconnected.
	firstChild = 0;
	next = 0;
	previous = 0;
	parent = 0;

	delete this;
}

//inline
//void NodeBase::replaceWithChildren()
//{
//	if (isLeaf() || isRoot())
//		return;
//	NodeBase* child = firstChild->next;
//	while (child != 0)
//	{
//		parent->addChild(child->previous);
//		child = child->next;
//	}
//	parent->addChild(lastChild);
//	firstChild = 0;
//	delete this;
//	--parent->m_childDegree;
//}

inline
void NodeBase::setChildDegree(unsigned int value)
{
	m_childDegree = value;
	m_childrenChanged = false;
}

inline
void NodeBase::setNumLeafNodes(unsigned int value)
{
	m_numLeafMembers = value;
}


template <typename T>
class Node : public NodeBase
{
public:
	typedef Node<T>			node_type;
	typedef Edge<node_type>	edge_type;

	Node() : NodeBase()
	{}
	Node(std::string name) : NodeBase(name)
	{}
	Node(std::string name, double flow, double teleWeight) : NodeBase(name), data(flow, teleWeight)
	{}
	Node(T data) : NodeBase(), data(data)
	{}
	Node(const node_type& other) : NodeBase(), data(other.data)
	{}
	virtual ~Node()
	{}

	friend std::ostream& operator<<(std::ostream& out, const node_type& node)
	{
//		return out << "n" << node.id << " (" << node.data << ")";
		return out << node.name << " " << node.data.flow;
	}

	//debug
	virtual void printData(std::ostream& out)
	{
		out << data;
	}

	T data; // Flow data
};

struct PhysData
{
	PhysData(unsigned int physNodeIndex, double sumFlowFromStateNode = 0.0)
	: physNodeIndex(physNodeIndex), sumFlowFromStateNode(sumFlowFromStateNode)
	{}
	PhysData(const PhysData& other) : physNodeIndex(other.physNodeIndex), sumFlowFromStateNode(other.sumFlowFromStateNode) {}
	unsigned int physNodeIndex;
	double sumFlowFromStateNode; // The amount of flow from the memory node in this physical node
};


template <typename T>
class MemNode : public Node<T>
{
public:
	typedef MemNode<T>			node_type;
	typedef Node<T>				node_base_type;

	MemNode() : node_base_type()
	{}
	MemNode(std::string name) : node_base_type(name)
	{}
	MemNode(std::string name, double flow, double teleWeight) : node_base_type(name, flow, teleWeight)
	{}
	MemNode(T data) : node_base_type(data)
	{}
	MemNode(const node_type& other) : node_base_type(other.data), stateNode(other.stateNode), physicalNodes(other.physicalNodes)
	{}

	virtual ~MemNode()
	{}

	friend std::ostream& operator<<(std::ostream& out, const node_type& node)
	{
		return out << "(name: " << node.name << ", flow: " << node.data.flow << ", phys: " << node.stateNode << ")";
	}

	virtual StateNode getStateNode()
	{
		return stateNode;
	}

	virtual unsigned int getPhysicalIndex()
	{
		return stateNode.physIndex;
	}

	StateNode stateNode;

	std::vector<PhysData> physicalNodes;
};

#ifdef NS_INFOMAP
}
#endif

#endif /* NODE_H_ */
