/*
 * InfoNode.h
 *
 *  Created on: 19 feb 2015
 *      Author: Daniel
 */

#ifndef SRC_CLUSTERING_INFONODE_H_
#define SRC_CLUSTERING_INFONODE_H_

// #include "InfomapBase.h"
#include <memory>
#include <iostream>
#include <vector>
#include "FlowData.h"
#include "InfoEdge.h"
#include "infomapIterators.h"
#include "treeIterators.h"
#include "../utils/iterators.h"
#include <limits>
#include "../utils/exceptions.h"

namespace infomap {

class InfomapBase;

class InfoNode
{
public:
	typedef Edge<InfoNode>								EdgeType;

	// Iterators
	typedef SiblingIterator<InfoNode*>					sibling_iterator;
	typedef SiblingIterator<InfoNode const*>			const_sibling_iterator;
	typedef LeafNodeIterator<InfoNode*>					leaf_iterator;
	typedef LeafNodeIterator<InfoNode const*>			const_leaf_iterator;
	typedef LeafModuleIterator<InfoNode*>				leaf_module_iterator;
	typedef LeafModuleIterator<InfoNode const*>			const_leaf_module_iterator;

	typedef DepthFirstIterator<InfoNode*, true>			pre_depth_first_iterator;
	typedef DepthFirstIterator<InfoNode const*, true>	const_pre_depth_first_iterator;
	typedef DepthFirstIterator<InfoNode*, false>		post_depth_first_iterator;
	typedef DepthFirstIterator<InfoNode const*, false>	const_post_depth_first_iterator;

	typedef InfomapDepthFirstIterator<InfoNode*>		infomap_depth_first_iterator;
	typedef InfomapDepthFirstIterator<InfoNode const*>	const_infomap_depth_first_iterator;

	typedef InfomapClusterIterator<InfoNode*>			infomap_cluster_iterator;
	typedef InfomapClusterIterator<InfoNode const*>		const_infomap_cluster_iterator;

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
	/*const*/ unsigned int stateId = 0; // Unique state node id for the leaf nodes
	/*const*/ unsigned int physicalId = 0; // Physical id equals stateId for first order networks, otherwise can be non-unique
	/*const*/ unsigned int layerId = 0; // Layer id for multilayer networks

	InfoNode* owner = nullptr; // Infomap owner (if this is an Infomap root)
	InfoNode* parent = nullptr;
	InfoNode* previous = nullptr; // sibling
	InfoNode* next = nullptr; // sibling
	InfoNode* firstChild = nullptr;
	InfoNode* lastChild = nullptr;
	InfoNode* collapsedFirstChild = nullptr;
	InfoNode* collapsedLastChild = nullptr;
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

	// std::unique_ptr<InfomapBase> m_infomap;
	InfomapBase* m_infomap = nullptr;

public:

	InfoNode(const FlowData& flowData)
	: data(flowData) {};

	// For first order nodes, physicalId equals stateId
	InfoNode(const FlowData& flowData, unsigned int stateId)
	: data(flowData), stateId(stateId), physicalId(stateId) {};

	InfoNode(const FlowData& flowData, unsigned int stateId, unsigned int physicalId)
	: data(flowData), stateId(stateId), physicalId(physicalId) {};

	InfoNode(const FlowData& flowData, unsigned int stateId, unsigned int physicalId, unsigned int layerId)
	: data(flowData), stateId(stateId), physicalId(physicalId), layerId(layerId) {};

	InfoNode() {};

	InfoNode(const InfoNode& other)
	:	data(other.data),
		index(other.index),
		stateId(other.stateId),
		physicalId(other.physicalId),
		layerId(other.layerId),
		parent(other.parent),
		previous(other.previous),
		next(other.next),
		firstChild(other.firstChild),
		lastChild(other.lastChild),
		collapsedFirstChild(other.collapsedFirstChild),
		collapsedLastChild(other.collapsedLastChild),
		codelength(other.codelength),
		dirty(other.dirty),
		m_childDegree(other.m_childDegree),
		m_childrenChanged(other.m_childrenChanged),
		m_numLeafMembers(other.m_numLeafMembers)
	{}


	~InfoNode();

	
	InfoNode& operator=(const InfoNode& other)
	{
		data = other.data;
		index = other.index;
		stateId = other.stateId;
		physicalId = other.physicalId;
		layerId = other.layerId;
		parent = other.parent;
		previous = other.previous;
		next = other.next;
		firstChild = other.firstChild;
		lastChild = other.lastChild;
		collapsedFirstChild = other.collapsedFirstChild;
		collapsedLastChild = other.collapsedLastChild;
		codelength = other.codelength;
		dirty = other.dirty;
		m_childDegree = other.m_childDegree;
		m_childrenChanged = other.m_childrenChanged;
		m_numLeafMembers = other.m_numLeafMembers;
		return *this;
	}

	// ---------------------------- Getters ----------------------------


	// ---------------------------- Infomap ----------------------------
	InfomapBase& getInfomap();

	InfomapBase& setInfomap(InfomapBase*);

	InfoNode* getInfomapRoot();

	InfoNode const* getInfomapRoot() const;

	/**
	 * Dispose the Infomap instance if it exists
	 * @return true if an existing Infomap instance was deleted
	 */
	bool disposeInfomap();

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

	infomap_cluster_iterator end_tree()
	{ return infomap_cluster_iterator(nullptr); }

	const_infomap_cluster_iterator begin_tree(unsigned int maxClusterLevel = std::numeric_limits<unsigned int>::max()) const
	{ return const_infomap_cluster_iterator(this, maxClusterLevel); }

	const_infomap_cluster_iterator end_tree() const
	{ return const_infomap_cluster_iterator(nullptr); }

	infomap_depth_first_iterator begin_treePath(unsigned int maxClusterLevel = std::numeric_limits<unsigned int>::max())
	{ return infomap_depth_first_iterator(this, maxClusterLevel); }

	infomap_depth_first_iterator end_treePath()
	{ return infomap_depth_first_iterator(nullptr); }

	const_infomap_depth_first_iterator begin_treePath(unsigned int maxClusterLevel = std::numeric_limits<unsigned int>::max()) const
	{ return const_infomap_depth_first_iterator(this, maxClusterLevel); }

	const_infomap_depth_first_iterator end_treePath() const
	{ return const_infomap_depth_first_iterator(nullptr); }

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

	unsigned int firstDepthBelow() const;

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

	bool operator ==(const InfoNode& rhs) const
	{ return this == &rhs; }

	bool operator !=(const InfoNode& rhs) const
	{ return this != &rhs; }


	friend std::ostream& operator<<(std::ostream& out, const InfoNode& node) {
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

	void addChild(InfoNode* child);

	void releaseChildren();

	/**
	 * If not already having a single child, replace children
	 * with a single new node, assuming grandchildren.
	 * @return the single child
	 */
	InfoNode& replaceChildrenWithOneNode();

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

	EdgeType* addOutEdge(InfoNode& target, double weight, double flow = 0.0)
	{
		EdgeType* edge = new EdgeType(*this, target, weight, flow);
		m_outEdges.push_back(edge);
		target.m_inEdges.push_back(edge);
		return edge;
	}

private:
	void calcChildDegree();

};

}

#endif /* SRC_CLUSTERING_INFONODE_H_ */
