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


#ifndef HIERARCHICALNETWORK_H_
#define HIERARCHICALNETWORK_H_

#include <vector>
#include <map>
#include <iostream>
#include <sstream>
#include <fstream>
#include <ios>
#include <deque>
#include <algorithm>
#include <set>
#include <functional>   // std::greater
#include <limits>
#include "SafeFile.h"

enum EdgeAggregationPolicy { NONE, PARTIAL, FULL };

//static const unsigned int SIZE_OF_UNSIGNED_INT = sizeof(unsigned int);		// 4 byte (32 bit)
//static const unsigned int SIZE_OF_CHAR = sizeof(char);						// 1 byte (8 bit)
//static const unsigned int SIZE_OF_FLOAT = sizeof(float); 					// 4 byte (32 bit)
//static const unsigned int SIZE_OF_UNSIGNED_SHORT = sizeof(unsigned short);	// 2 byte (16 bit)

namespace SerialTypes {
	typedef unsigned short nameSize_t;
	typedef float flow_t;
	typedef unsigned int childSize_t;
	typedef unsigned int childPos_t;
	typedef unsigned int edgeSize_t;

	template<typename T, typename U>
	T numeric_cast(U value)
	{
//		return value > std::numeric_limits<T>::max() ?
//				std::numeric_limits<T>::max() :
//				static_cast<T>(value);
		if (value > std::numeric_limits<T>::max()) {
			std::cout << " [Warning: truncating internal serial network size] ";
			return std::numeric_limits<T>::max();
		}
		return static_cast<T>(value);
	}
};

struct NodeData {
	NodeData(double flow = 0.0, double exitFlow = 0.0, std::string name = "") :
		flow(flow),
		enterFlow(0.0),
		exitFlow(exitFlow),
		teleportRate(0.0),
		danglingFlow(0.0),
		indexCodelength(0.0),
		moduleCodelength(0.0),
		name(name)
//		id(0)
	{}
	double flow;
	double enterFlow;
	double exitFlow;
	double teleportRate;
	double danglingFlow;
	double indexCodelength;
	double moduleCodelength;
	std::string name;
//	unsigned int id;
};


struct ChildEdge {
	ChildEdge(unsigned short int source, unsigned short int target, double flow)
	: source(source), target(target), flow(flow) {}
	SerialTypes::edgeSize_t source;
	SerialTypes::edgeSize_t target;
	mutable double flow;
};

struct EdgeComp {
	bool operator() (const ChildEdge& lhs, const ChildEdge& rhs) const
	{
		return lhs.source == rhs.source ? (lhs.target < rhs.target) : (lhs.source < rhs.source);
	}
};

/**
 *  A node class that is self-contained and can be used consistently for both leaf nodes
 *  and modules, including the root node.
 */
class SNode {
public:
	typedef std::deque<SNode*>				NodePtrList;
	typedef std::set<ChildEdge, EdgeComp>	ChildEdgeList;

	SNode(NodeData data, unsigned short depth, unsigned short parentIndex, unsigned int id) :
		data(data),
		depth(depth),
		depthBelow(0),
		parentNode(0),
		parentIndex(parentIndex),
		isLeaf(false),
		leafIndex(0),
		id(id)
	{
	}

	// Copy constructor
	SNode(SNode const& other) :
		data(other.data),
		depth(other.depth),
		depthBelow(other.depthBelow),
		parentNode(other.parentNode),
		parentIndex(other.parentIndex),
		isLeaf(false),
		leafIndex(0),
		id(other.id)
	{
	}

	// Copy assignment operator
	SNode& operator= (SNode const& other)
	{
		data = other.data;
		return *this;
	}

	// Destructor
	~SNode()
	{
		clear();
	}

	void clear()
	{
		for (int i = children.size() - 1; i >= 0; --i)
		{
			delete children[i];
		}
		children.clear();
		childEdges.clear();
	}

	void addChild(SNode& child)
	{
		child.parentIndex = children.size();
		children.push_back(&child);
		child.parentNode = this;
	}

	SerialTypes::edgeSize_t numSerializableChildEdges() const
	{
		return SerialTypes::numeric_cast<SerialTypes::edgeSize_t>(childEdges.size());
	}


	NodeData data;

	unsigned short depth;
	unsigned short depthBelow;
	SNode* parentNode;
	unsigned short parentIndex; // The index of this node in its tree parent's child list.
	bool isLeaf;
	unsigned int leafIndex; // The index in the original network file if a leaf node.
	unsigned int id;
	NodePtrList children;
	ChildEdgeList childEdges;


public:

	// Capacity:
	unsigned int serializationSize(bool writeEdges)
	{
		using namespace SerialTypes;
		unsigned int size = sizeof(nameSize_t) + data.name.length() * sizeof(char);
		size += 2 * sizeof(flow_t);
		size += sizeof(childSize_t);

		if (children.size() > 0)
			size += sizeof(depthBelow) + sizeof(childPos_t);
		// The edges are printed out after the last child
		writeEdges = true;
		if (writeEdges && parentNode != NULL && static_cast<unsigned int>(parentIndex + 1) == parentNode->children.size())
		{
			// numEdges + {edges}
			size += sizeof(edgeSize_t) + parentNode->numSerializableChildEdges() * (2 * sizeof(edgeSize_t) + sizeof(flow_t));
		}
		return size;
	}

	// Operations:
	void serialize(SafeBinaryOutFile& outFile, SerialTypes::childPos_t childPosition, bool writeEdges)
	{
		using namespace SerialTypes;
//		outFile << static_cast<unsigned short>(data.name.length()); // unsigned short nameLength
		outFile << data.name;					// char* name
		outFile << static_cast<flow_t>(data.flow); // float flow
		outFile << static_cast<flow_t>(data.exitFlow); // float exitFlow
		childSize_t numChildren = numeric_cast<childSize_t>(children.size());
		outFile << numChildren; 			// unsigned short numChildren
		if (numChildren > 0)
		{
			outFile << depthBelow;			// unsigned short depthBelow
			outFile << childPosition;		// unsigned int childPosition
		}

		writeEdges = true;
		// Write edges after the last child of the parent node
		if (writeEdges && parentNode != NULL && static_cast<unsigned int>(parentIndex + 1) == parentNode->children.size())
		{
			const ChildEdgeList& edges = parentNode->childEdges;
			edgeSize_t numEdges = numeric_cast<edgeSize_t>(edges.size());
			// First sort the edges
			std::multimap<double, ChildEdge, std::greater<double> > sortedEdges;
			for (SNode::ChildEdgeList::const_iterator it = edges.begin(); it != edges.end(); ++it)
				sortedEdges.insert(std::make_pair(it->flow, *it));
			outFile << numEdges;
			std::multimap<double, ChildEdge, std::greater<double> >::const_iterator it(sortedEdges.begin());
			for (edgeSize_t i = 0; i < numEdges; ++i, ++it)
			{
				outFile << it->second.source;
				outFile << it->second.target;
				outFile << static_cast<float>(it->second.flow);
			}
		}
//		else {
//			outFile << 0; // numEdges 0, XXX: Skip bftree or btree and use the same format!!
//		}
	}

	unsigned short deserialize(SafeBinaryInFile& dataStream)
	{
		using namespace SerialTypes;
		flow_t flow = 0.0, exitFlow = 0.0;
		childSize_t numChildren = 0;
		childPos_t childPosition = 0;
		dataStream >> data.name >> flow >> exitFlow >> numChildren;
		if (numChildren > 0)
		{
			dataStream >> depthBelow >> childPosition;
		}
		data.flow = flow;
		data.exitFlow = exitFlow;

//		std::cout << depth << ":" << std::string(depth * 4, ' ') << "\"" << data.name << "\" (flow: " <<
//						data.flow << ", exitFlow: " << data.exitFlow << ", numChildren: " << numChildren <<
//						", depthBelow: " << depthBelow << ", childPos: " << childPosition <<")\n";
		return numChildren;
	}

	unsigned short deserializeEdges(SafeBinaryInFile& dataStream, bool directedEdges)
	{
		using namespace SerialTypes;
		edgeSize_t numEdges;
		dataStream >> numEdges;
		edgeSize_t source = 0, target = 0;
		flow_t flow = 0.0;
//		std::cout << "---- Child edges to \"" << data.name << "\":\n";
		for (edgeSize_t i = 0; i < numEdges; ++i)
		{
			dataStream >> source >> target >> flow;
//			std::cout << source << " " << target << " " << flow << "\n";
			createChildEdge(source, target, flow, directedEdges);
		}
		return numEdges;
	}

	// Accessors:
	SNode* lastChild()
	{
		return children.empty() ? NULL : children.back();
	}

	SNode* firstChild()
	{
		return children.empty() ? NULL : children.front();
	}

	SNode* nextSibling()
	{
		if (parentNode == NULL || static_cast<unsigned int>(parentIndex + 1) == parentNode->children.size())
			return NULL;
		return parentNode->children[parentIndex + 1];
	}


	/**
	 * Create an edge between two children, defined by their child indices.
	 * @return true if a new edge was inserted, false if it was aggregated to an existing edge.
	 */
	bool createChildEdge(SerialTypes::edgeSize_t sourceIndex, SerialTypes::edgeSize_t targetIndex, double flow, bool directed)
	{
		if (!directed && sourceIndex > targetIndex)
			std::swap(sourceIndex, targetIndex);

		ChildEdge edge(sourceIndex, targetIndex, flow);

		std::pair<ChildEdgeList::iterator, bool> ret = childEdges.insert(edge);
		if (!ret.second)
		{
			ret.first->flow += flow;
			return false;
		}
		return true;
	}

};


class LeafIterator
{
public:

	LeafIterator()
	:	m_current(NULL),
	 	m_depth(0)
	{}

	explicit
	LeafIterator(SNode* nodePointer)
	:	m_current(nodePointer),
	 	m_depth(0)
	{
		if (m_current != 0)
		{
			while(m_current->firstChild() != NULL)
			{
				m_current = m_current->firstChild();
				++m_depth;
			}
		}
	}



	LeafIterator(const LeafIterator& other)
	:	m_current(other.m_current),
	 	m_depth(other.m_depth)
	{}

	LeafIterator & operator= (const LeafIterator& other)
	{
		m_current = other.m_current;
		m_depth = other.m_depth;
		return *this;
	}

	SNode* base() const
	{ return m_current; }

	// Forward iterator requirements
	SNode&
	operator*() const
	{ return *m_current; }

	SNode*
	operator->() const
	{ return m_current; }

	LeafIterator&
	operator++()
	{
		while(m_current->nextSibling() == NULL)
		{
			m_current = m_current->parentNode;
			--m_depth;
			if(m_current == NULL)
				return *this;
		}

		m_current = m_current->nextSibling();

		if (m_current != NULL)
		{
			while(m_current->firstChild() != NULL)
			{
				m_current = m_current->firstChild();
				++m_depth;
			}
		}
		return *this;
	}

	LeafIterator
	operator++(int)
	{
		LeafIterator copy(*this);
		++(*this);
		return copy;
	}

	unsigned int depth() const
	{
		return m_depth;
	}

	bool operator==(const LeafIterator& rhs) const
	{
		return m_current == rhs.m_current;
	}

	bool operator!=(const LeafIterator& rhs) const
	{
		return !(m_current == rhs.m_current);
	}

private:
	SNode* m_current;
	unsigned int m_depth;
};


class HierarchicalNetwork
{
public:
	typedef SNode	node_type;

	HierarchicalNetwork()
	:	m_rootNode(1.0, 0, 0, 0),
		m_networkName(""),
		m_leafNodes(0),
		m_directedEdges(false),
		m_numLeafEdges(0),
		m_numNodesInTree(1),
		m_maxDepth(0),
		m_codelength(0.0),
		m_oneLevelCodelength(0.0),
		m_infomapVersion("")
		{}

	virtual ~HierarchicalNetwork() {}

	void init(std::string networkName, bool directedEdges, double codelength, double oneLevelCodelength, std::string infomapVersion);

	void clear();

	SNode& getRootNode() { return m_rootNode; }

	SNode& addNode(SNode& parent, double flow, double exitFlow);

	SNode& addLeafNode(SNode& parent, double flow, double exitFlow, std::string name, unsigned int leafIndex);

	void prepareAddLeafNodes(unsigned int numLeafNodes);

	/**
	 * Add flow-edges to the tree. This method can aggregate the edges between the leaf-nodes
	 * up in the tree based on the edge aggregation policy.
	 * @param aggregationFilter A flag to decide the type of aggregation.
	 * @return true if a new edge was inserted, false if it was aggregated to an existing edge
	 * @note The EdgeAggregationPolicy enumeration lists the available aggregation policies.
	 */
	bool addLeafEdge(unsigned int sourceLeafNodeIndex, unsigned int targetLeafNodeIndex, double flow);

	void propagateNodeNameUpInHierarchy(SNode& node);


	/**
	 * Print the network using a breadth-first algorithm. Each node keeps a pointer
	 * to where its children is located in the file if it has any.
	 *
	 * File format:
	 *
	 * [Header]
	 * unsigned int		rootNodePosition
	 * unsigned int		numNodesTotal (numLeafNodes + numModules including the rootNode)
	 *
	 * [Network]
	 * [depth 0]
	 * rootNode
	 *
	 * [depth 1]
	 * firstModule
	 * secondModule
	 * ...
	 * lastModule
	 * edgesBetweenChildrenOfRootNode
	 *
	 * [depth 2]
	 * firstChildOfFirstModule
	 * secondChildOfFirstModule
	 * ...
	 * lastChildOfFirstModule
	 * edgesBetweenChildrenOfFirstModuleOnDepth1
	 *
	 * firstChildOfSecondModule
	 * ...
	 * lastChildOfSecondModule
	 * edgesBetweenChildrenOfSecondModuleOnDepth1
	 *
	 * ...
	 *
	 * [EOF]
	 *
	 * Each binary node contains a pointer to where its children is located in the file
	 */
	void writeStreamableTree(const std::string& fileName, bool writeEdges);

	void readStreamableTree(const std::string& fileName);

	void writeHumanReadableTree(const std::string& fileName, bool writeHierarchicalNetworkEdges = false);

	void writeMap(const std::string& fileName);

private:

	void writeHumanReadableTreeRecursiveHelper(std::ostream& out, SNode& node, std::string prefix = "");
	void writeHumanReadableTreeFlowLinksRecursiveHelper(std::ostream& out, SNode& node, std::string prefix = "");

	static bool compareLeafNodePredicate(const SNode* lhs, const SNode* rhs) { return (lhs->leafIndex < rhs->leafIndex); }

	void sortLeafNodes()
	{
		std::cout << "Sort leaf nodes according to original order... ";
		std::sort(m_leafNodes.begin(), m_leafNodes.end(), compareLeafNodePredicate);
		std::cout << "done!" << std::endl;
	}



	SNode m_rootNode;
	std::string m_networkName;
	SNode::NodePtrList m_leafNodes;
	bool m_directedEdges;
	unsigned int m_numLeafEdges;
	unsigned int m_numNodesInTree;
	unsigned int m_maxDepth;
	double m_codelength;
	double m_oneLevelCodelength;
	std::string m_infomapVersion;

};

#endif /* HIERARCHICALNETWORK_H_ */
