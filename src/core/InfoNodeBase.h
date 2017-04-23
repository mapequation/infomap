/*
 * InfoNodeBase.h
 *
 *  Created on: 23 feb 2015
 *      Author: Daniel
 */

#ifndef SRC_CLUSTERING_INFONODEBASE_H_
#define SRC_CLUSTERING_INFONODEBASE_H_


#include <iostream>
#include "treeIterators.h"
#include <vector>
#include "FlowData.h"
#include "InfoEdge.h"
#include "infomapIterators.h"
#include "../utils/iterators.h"
#include <limits>
#include "../utils/exceptions.h"

namespace infomap {

class InfoNodeBase
{
public:
	typedef Edge<InfoNodeBase>								EdgeType;

	// Iterators
	typedef SiblingIterator<InfoNodeBase*>					sibling_iterator;
	typedef SiblingIterator<InfoNodeBase const*>			const_sibling_iterator;
	typedef LeafNodeIterator<InfoNodeBase*>					leaf_iterator;
	typedef LeafNodeIterator<InfoNodeBase const*>			const_leaf_iterator;
	typedef LeafModuleIterator<InfoNodeBase*>				leaf_module_iterator;
	typedef LeafModuleIterator<InfoNodeBase const*>			const_leaf_module_iterator;

	typedef DepthFirstIterator<InfoNodeBase*, true>			pre_depth_first_iterator;
	typedef DepthFirstIterator<InfoNodeBase const*, true>	const_pre_depth_first_iterator;
	typedef DepthFirstIterator<InfoNodeBase*, false>		post_depth_first_iterator;
	typedef DepthFirstIterator<InfoNodeBase const*, false>	const_post_depth_first_iterator;

	typedef InfomapDepthFirstIterator<InfoNodeBase*>		infomap_depth_first_iterator;
	typedef InfomapDepthFirstIterator<InfoNodeBase const*>	const_infomap_depth_first_iterator;

	typedef InfomapClusterIterator<InfoNodeBase*>			infomap_cluster_iterator;
	typedef InfomapClusterIterator<InfoNodeBase const*>		const_infomap_cluster_iterator;

	typedef std::vector<EdgeType*>::iterator				edge_iterator;
	typedef std::vector<EdgeType*>::const_iterator			const_edge_iterator;
//	typedef gap_iterator<edge_iterator>						inout_edge_iterator;
	typedef IterWrapper<edge_iterator> 						edge_iterator_wrapper;
	typedef IterWrapper<const_edge_iterator> 				const_edge_iterator_wrapper;

//	typedef IterWrapper<infomap_depth_first_iterator> 		infomap_iterator_wrapper;
//	typedef IterWrapper<const_infomap_depth_first_iterator> const_infomap_iterator_wrapper;

	typedef IterWrapper<infomap_cluster_iterator> 			infomap_iterator_wrapper;
	typedef IterWrapper<const_infomap_cluster_iterator> 	const_infomap_iterator_wrapper;

public:

	FlowData data;
	unsigned int index = 0; // Temporary index used in finding best module
//	unsigned int originalIndex = 0; // Index in the original network (for leaf nodes)
	const unsigned int uid = 0; // Unique id for the leaf nodes
	const unsigned int physicalId = 0; // Physical id equals uid for first order networks, otherwise can be non-unique
	const unsigned int stateId = 0; // State id for second order networks (prior physical node / multiplex level etc)

	InfoNodeBase* owner = nullptr; // Infomap owner (if this is an Infomap root)
	InfoNodeBase* parent = nullptr;
	InfoNodeBase* previous = nullptr; // sibling
	InfoNodeBase* next = nullptr; // sibling
	InfoNodeBase* firstChild = nullptr;
	InfoNodeBase* lastChild = nullptr;
	InfoNodeBase* collapsedFirstChild = nullptr;
	InfoNodeBase* collapsedLastChild = nullptr;
	double codelength = 0.0; //TODO: Better design for hierarchical stuff!?
	bool dirty = false;

	std::vector<PhysData> physicalNodes;

protected:
//	SubStructure m_subStructure;
	// std::unique_ptr<InfomapBase> m_infomap;
	unsigned int m_childDegree = 0;
	bool m_childrenChanged = false;
	unsigned int m_numLeafMembers = 0;

	std::vector<EdgeType*> m_outEdges;
	std::vector<EdgeType*> m_inEdges;

	InfoNodeBase(const FlowData& flowData)
	: data(flowData) {};

	// For first order nodes, physicalId equals uid
	InfoNodeBase(const FlowData& flowData, unsigned int uid)
	: data(flowData), uid(uid), physicalId(uid) {};

	InfoNodeBase(const FlowData& flowData, unsigned int uid, unsigned int physicalId)
	: data(flowData), uid(uid), physicalId(physicalId) {};

	InfoNodeBase(const FlowData& flowData, unsigned int uid, unsigned int physicalId, unsigned int stateId)
	: data(flowData), uid(uid), physicalId(physicalId), stateId(stateId) {};

public:
	InfoNodeBase() {};

	InfoNodeBase(const InfoNodeBase& other)
	:	data(other.data),
		index(other.index),
		uid(other.uid),
		physicalId(other.physicalId),
		stateId(other.stateId),
		parent(other.parent),
		previous(other.previous),
		next(other.next),
		firstChild(other.firstChild),
		lastChild(other.lastChild),
		collapsedFirstChild(other.collapsedFirstChild),
		collapsedLastChild(other.collapsedLastChild),
		codelength(other.codelength),
		dirty(other.dirty),
//		m_subStructure(other.m_subStructure),
		m_childDegree(other.m_childDegree),
		m_childrenChanged(other.m_childrenChanged),
		m_numLeafMembers(other.m_numLeafMembers)
	{}


	virtual ~InfoNodeBase();

	//TODO: Copy ctor

	// ---------------------------- Getters ----------------------------


	template<typename InfoNode>
	static const InfoNode& get(const InfoNodeBase& node) {
		return static_cast<const InfoNode&>(node);
	}


	/**
	* Return reference to derived class
	*/
	// InfoNode& get()
	// {
	// 	return static_cast<InfoNode&>(*this);
	// }

	// InfoNode& operator() ()
	// {
	// 	return get();
	// }

	// ---------------------------- Infomap ----------------------------

	// template<typename Infomap = InfomapBase<Node>>
	// Infomap& getInfomap();

	virtual InfoNodeBase* getInfomapRoot() { return nullptr; }

	/**
	 * Number of physical nodes in memory nodes
	 */
	unsigned int numPhysicalNodes() const { return physicalNodes.size(); }

	// ---------------------------- Tree iterators ----------------------------

	sibling_iterator begin_child()
	{ return sibling_iterator(firstChild); }

	sibling_iterator end_child()
	{ return sibling_iterator(nullptr); }

	// Default iteration on children
	sibling_iterator begin()
	{ return sibling_iterator(firstChild); }

	sibling_iterator end()
	{ return sibling_iterator(nullptr); }

	const_sibling_iterator begin() const
	{ return const_sibling_iterator(firstChild); }

	const_sibling_iterator end() const
	{ return const_sibling_iterator(nullptr); }

	const_sibling_iterator begin_child() const
	{ return const_sibling_iterator(firstChild); }

	const_sibling_iterator end_child() const
	{ return const_sibling_iterator(nullptr); }

	leaf_iterator begin_leaf()
	{ return leaf_iterator(firstChild); }

	leaf_iterator end_leaf()
	{ return leaf_iterator(nullptr); }

	pre_depth_first_iterator begin_depthFirst()
	{ return pre_depth_first_iterator(this); }

	pre_depth_first_iterator end_depthFirst()
	{ return pre_depth_first_iterator(nullptr); }

	const_pre_depth_first_iterator begin_depthFirst() const
	{ return const_pre_depth_first_iterator(this); }

	const_pre_depth_first_iterator end_depthFirst() const
	{ return const_pre_depth_first_iterator(nullptr); }

	infomap_cluster_iterator begin_tree(unsigned int maxClusterLevel = std::numeric_limits<unsigned int>::max())
	{ return infomap_cluster_iterator(this, maxClusterLevel); }

	const_infomap_cluster_iterator begin_tree(unsigned int maxClusterLevel = std::numeric_limits<unsigned int>::max()) const
	{ return const_infomap_cluster_iterator(this, maxClusterLevel); }

	infomap_depth_first_iterator begin_infomapDepthFirst(unsigned int maxClusterLevel = std::numeric_limits<unsigned int>::max())
	{ return infomap_depth_first_iterator(this, maxClusterLevel); }

	const_infomap_depth_first_iterator begin_infomapDepthFirst(unsigned int maxClusterLevel = std::numeric_limits<unsigned int>::max()) const
	{ return const_infomap_depth_first_iterator(this, maxClusterLevel); }

	infomap_iterator_wrapper infomapTree(unsigned int maxClusterLevel = std::numeric_limits<unsigned int>::max()) {
		return infomap_iterator_wrapper(infomap_cluster_iterator(this, maxClusterLevel), infomap_cluster_iterator(nullptr));
	}

	const_infomap_iterator_wrapper infomapTree(unsigned int maxClusterLevel = std::numeric_limits<unsigned int>::max()) const {
		return const_infomap_iterator_wrapper(const_infomap_cluster_iterator(this, maxClusterLevel), const_infomap_cluster_iterator(nullptr));
	}

	// ---------------------------- Graph iterators ----------------------------

	edge_iterator begin_outEdge()
	{ return m_outEdges.begin(); }

	edge_iterator end_outEdge()
	{ return m_outEdges.end(); }

	edge_iterator begin_inEdge()
	{ return m_inEdges.begin(); }

	edge_iterator end_inEdge()
	{ return m_inEdges.end(); }

//	inout_edge_iterator begin_inoutEdge()
//	{ return inout_edge_iterator(begin_inEdge(), end_inEdge(), begin_outEdge()); }
//
//	inout_edge_iterator	end_inoutEdge()
//	{ return inout_edge_iterator(end_outEdge()); }

	edge_iterator_wrapper outEdges() {
		return edge_iterator_wrapper(m_outEdges);
	}

	edge_iterator_wrapper inEdges() {
		return edge_iterator_wrapper(m_inEdges);
	}

	// ---------------------------- Capacity ----------------------------

	unsigned int childDegree() const;

	bool isLeaf() const;
	bool isLeafModule() const;
	bool isRoot() const;

	unsigned int depth() const;

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

//	InfomapBase* getSubInfomap()
//	{ return m_subStructure.subInfomap.get(); }
//
//	const InfomapBase* getSubInfomap() const
//	{ return m_subStructure.subInfomap.get(); }
//
//	SubStructure& getSubStructure()
//	{ return m_subStructure; }
//
//	const SubStructure& getSubStructure() const
//	{ return m_subStructure; }

	// ---------------------------- Order ----------------------------
	bool isFirst() const
	{ return !parent || parent->firstChild == this; }

	bool isLast() const
	{ return !parent || parent->lastChild == this; }

	unsigned int childIndex() const;

	// ---------------------------- Operators ----------------------------

	bool operator ==(const InfoNodeBase& rhs) const
	{ return this == &rhs; }

	bool operator !=(const InfoNodeBase& rhs) const
	{ return this != &rhs; }


	friend std::ostream& operator<<(std::ostream& out, const InfoNodeBase& node) {
		if (node.isLeaf())
			out << "[" << node.physicalId << "]";
		else
			out << "[module]";
		return out;
	}

	// ---------------------------- Mutators ----------------------------

	/**
	 * Clear a cloned node to initial state
	 */
	void initClean();

	/**
	 * Release the children and store the child pointers for later expansion
	 * @return the number of children collapsed
	 */
	unsigned int collapseChildren();

	/**
	 * Expand collapsed children
	 * @return the number of collapsed children expanded
	 */
	unsigned int expandChildren();

	// ------ OLD -----

	// After change, set the child degree if known instead of lazily computing it by traversing the linked list
	void setChildDegree(unsigned int value);

	void setNumLeafNodes(unsigned int value);

	void addChild(InfoNodeBase* child);

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

	EdgeType* addOutEdge(InfoNodeBase& target, double weight, double flow = 0.0)
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

private:
	void calcChildDegree();

};


InfoNodeBase::~InfoNodeBase()
{
	deleteChildren();

	if (next != 0)
		next->previous = previous;
	if (previous != 0)
		previous->next = next;
	if (parent != 0)
	{
		if (parent->firstChild == this)
			parent->firstChild = next;
		if (parent->lastChild == this)
			parent->lastChild = previous;
	}

	// Delete outgoing edges.
	// TODO: Renders ingoing edges invalid. Assume or assert that all nodes on the same level are deleted?
	for (edge_iterator outEdgeIt(begin_outEdge());
			outEdgeIt != end_outEdge(); ++outEdgeIt)
	{
		delete *outEdgeIt;
	}

}


inline
bool InfoNodeBase::isLeaf() const
{
	return firstChild == nullptr;
}

inline
bool InfoNodeBase::isLeafModule() const
{
	return firstChild != nullptr && firstChild->firstChild == nullptr;
}

inline
bool InfoNodeBase::isRoot() const
{
	return parent == nullptr;
}

inline
unsigned int InfoNodeBase::depth() const
{
	unsigned int depth = 0;
	InfoNodeBase* n = parent;
	while (n != nullptr) {
		++depth;
		n = n->parent;
	}
	return depth;
}

inline
unsigned int InfoNodeBase::childIndex() const
{
	unsigned int childIndex = 0;
	const InfoNodeBase* n(this);
	while (n->previous) {
		n = n->previous;
		++childIndex;
	}
	return childIndex;
}

inline
unsigned int InfoNodeBase::childDegree() const
{
//	if (!m_childrenChanged)
		return m_childDegree;
//	calcChildDegree();
//	return m_childDegree;
}

inline
void InfoNodeBase::addChild(InfoNodeBase* child)
{
//	m_childrenChanged = true;
	if (firstChild == 0)
	{
//		DEBUG_OUT("\t\t-->Add node " << *child << " as FIRST CHILD to parent " << *this << std::endl);
		child->previous = nullptr;
		firstChild = child;
	}
	else
	{
//		DEBUG_OUT("\t\t-->Add node " << *child << " as LAST CHILD to parent " << *this << std::endl);
		child->previous = lastChild;
		lastChild->next = child;
	}
	lastChild = child;
	child->next = nullptr;
	child->parent = this;
	++m_childDegree;
}

inline
void InfoNodeBase::releaseChildren()
{
	firstChild = nullptr;
	lastChild = nullptr;
	m_childDegree = 0;
}

inline
unsigned int InfoNodeBase::replaceChildrenWithGrandChildren()
{
	if (firstChild == nullptr)
		return 0;
	InfoNodeBase::sibling_iterator nodeIt = begin_child();
	unsigned int numOriginalChildrenLeft = m_childDegree;
	unsigned int numChildrenReplaced = 0;
	do
	{
		InfoNodeBase* n = nodeIt.base();
		++nodeIt;
		numChildrenReplaced += n->replaceWithChildren();
	}
	while (--numOriginalChildrenLeft != 0);
	return numChildrenReplaced;
}

inline
unsigned int InfoNodeBase::replaceWithChildren()
{
	if (isLeaf() || isRoot())
		return 0;

	// Reparent children
	unsigned int deltaChildDegree = 0;
	InfoNodeBase* child = firstChild;
	do
	{
		child->parent = parent;
		child = child->next;
		++deltaChildDegree;
	} while (child != nullptr);
	parent->m_childDegree += deltaChildDegree - 1; // -1 as this node is deleted

	firstChild->previous = previous;
	lastChild->next = next;

	if (parent->firstChild == this) {
		parent->firstChild = firstChild;
	} else {
		previous->next = firstChild;
	}

	if (parent->lastChild == this) {
		parent->lastChild = lastChild;
	} else {
		next->previous = lastChild;
	}

	// Release connected nodes before delete, otherwise children are deleted and neighbours are reconnected.
	firstChild = nullptr;
	lastChild = nullptr;
	next = nullptr;
	previous = nullptr;
	parent = nullptr;
	delete this;
	return 1;
}

inline
void InfoNodeBase::replaceChildrenWithGrandChildrenDebug()
{
	if (firstChild == 0)
		return;
	InfoNodeBase::sibling_iterator nodeIt = begin_child();
	unsigned int numOriginalChildrenLeft = m_childDegree;
	do
	{
		InfoNodeBase* n = nodeIt.base();
		++nodeIt;
		n->replaceWithChildrenDebug();
	}
	while (--numOriginalChildrenLeft != 0);
}


inline
void InfoNodeBase::replaceWithChildrenDebug()
{
	if (isLeaf() || isRoot())
		return;


	// Reparent children
	unsigned int deltaChildDegree = 0;
	InfoNodeBase* child = firstChild;
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
	firstChild = nullptr;
	lastChild = nullptr;
	next = nullptr;
	previous = nullptr;
	parent = nullptr;

	delete this;
}

inline
void InfoNodeBase::deleteChildren()
{
	if (firstChild == nullptr)
		return;

	sibling_iterator nodeIt = begin_child();
	do
	{
		InfoNodeBase* n = nodeIt.base();
		++nodeIt;
		delete n;
	}
	while (nodeIt.base() != nullptr);

	firstChild = nullptr;
	lastChild = nullptr;
	m_childDegree = 0;
}

inline
void InfoNodeBase::calcChildDegree()
{
	m_childrenChanged = false;
	if (firstChild == nullptr)
		m_childDegree = 0;
	else if (firstChild == lastChild)
		m_childDegree = 1;
	else
	{
		m_childDegree = 0;
		for (sibling_iterator childIt(begin_child()), endIt(end_child());
				childIt != endIt; ++childIt, ++m_childDegree);
	}
}

inline
void InfoNodeBase::setChildDegree(unsigned int value)
{
	m_childDegree = value;
	m_childrenChanged = false;
}

inline
void InfoNodeBase::setNumLeafNodes(unsigned int value)
{
	m_numLeafMembers = value;
}

inline
void InfoNodeBase::initClean()
{
	releaseChildren();
	previous = next = parent = nullptr;

	physicalNodes.clear();
}

inline
unsigned int InfoNodeBase::collapseChildren()
{
	std::swap(collapsedFirstChild, firstChild);
	std::swap(collapsedLastChild, lastChild);
	unsigned int numCollapsedChildren = childDegree();
	releaseChildren();
	return numCollapsedChildren;
}

inline
unsigned int InfoNodeBase::expandChildren()
{
	bool haveCollapsedChildren = collapsedFirstChild != nullptr;
	if (haveCollapsedChildren) {
		if (firstChild != nullptr || lastChild != nullptr)
			throw InternalOrderError("Expand collapsed children called on a node that already has children.");
		std::swap(collapsedFirstChild, firstChild);
		std::swap(collapsedLastChild, lastChild);
		calcChildDegree();
		return childDegree();
	}
	return 0;
}

}


#endif /* SRC_CLUSTERING_INFONODEBASE_H_ */
