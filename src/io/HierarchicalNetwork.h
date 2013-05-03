/**********************************************************************************

 Infomap software package for multi-level network clustering

 Copyright (c) 2013 Daniel Edler, Martin Rosvall
 
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
#include "SafeFile.h"

enum EdgeAggregationPolicy { NONE, PARTIAL, FULL };

static const unsigned int SIZE_OF_UNSIGNED_INT = sizeof(unsigned int);		// 4 byte (32 bit)
static const unsigned int SIZE_OF_CHAR = sizeof(char);						// 1 byte (8 bit)
static const unsigned int SIZE_OF_FLOAT = sizeof(float); 					// 4 byte (32 bit)
static const unsigned int SIZE_OF_UNSIGNED_SHORT = sizeof(unsigned short);	// 2 byte (16 bit)
static const unsigned int MIN_TOTAL_SIZE = SIZE_OF_UNSIGNED_SHORT + SIZE_OF_FLOAT + SIZE_OF_UNSIGNED_SHORT;

struct NodeData {
	NodeData(double flow = 0.0, std::string name = "") :
		flow(flow),
		name(name)
	{}
	unsigned int level;
	double flow;
	double enter;
	double exit;
	double teleportRate;
	double danglingFlow;
	double indexCodelength;
	double moduleCodelength;
	std::string name;
	unsigned int id;
};

class SNode; // Forward declaration

class SEdge {
public:
	SEdge(SNode* source, SNode* target, double flow) :
		source(source),
		target(target),
		flow(flow),
		parentEdge(0),
		pathComputed(false),
		depthFromCommonAncestor(0),
		targetUpPath(0)
	{
	}

	// TODO: Have the FullNode::prepareEdgeSerialization in this class, need separate implementation to different file for linker.
	unsigned int serializationSize()
	{
//		if (!pathComputed)
//		{
//			source->prepareEdgeSerialization(*this);
//			pathComputed = true;
//		}
		return (depthFromCommonAncestor+1) * SIZE_OF_UNSIGNED_SHORT + SIZE_OF_FLOAT;
	}

	// Operations:
	void serialize(ofstream_binary& outFile) const
	{
		// How many parent calls needed to reach the common ancestor
		outFile << depthFromCommonAncestor;
		// The path from common ancestor to target node
		for (std::vector<unsigned short>::const_reverse_iterator rit = targetUpPath.rbegin(); rit < targetUpPath.rend(); ++rit)
			outFile << *rit;
		// The edge weight
		outFile << static_cast<float>(flow);
	}

	SNode* source;
	SNode* target;
	double flow;
	SEdge* parentEdge; // The aggregated edge between the parent nodes depending on the aggregation policy

	bool pathComputed;
	unsigned short depthFromCommonAncestor;
	std::vector<unsigned short> targetUpPath;

	static EdgeAggregationPolicy aggregationPolicy;
};



/**
 *  A node class that is self-contained and can be used consistently for both leaf nodes
 *  and modules, including the root node.
 */
class SNode {
public:
	typedef std::deque<SNode*>		NodePtrList;
	typedef std::deque<SEdge*>		EdgePtrList;

	SNode(NodeData data, unsigned short depth = 0, unsigned short parentIndex = 0, unsigned int leafIndex = 0) :
		data(data),
		depth(depth),
		parentIndex(parentIndex),
		leafIndex(leafIndex),
		parentNode(0)
	{
	}

	// Copy constructor
	SNode(SNode const& other) :
		data(other.data),
		depth(other.depth),
		parentIndex(other.parentIndex),
		leafIndex(other.leafIndex),
		parentNode(other.parentNode)
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
		for (int i = children.size() - 1; i >= 0; --i) {
			delete children[i];
		}
		children.clear();
		// Only delete the outEdges in each node as the inEdges are a collection of other node's outEdges.
		for (int i = outEdges.size() - 1; i >= 0; --i) {
			delete outEdges[i];
		}
		outEdges.clear();
		inEdges.clear();
	}

	void addChild(SNode& child)
	{
		child.parentIndex = children.size();
		children.push_back(&child);
		child.parentNode = this;
	}


	NodeData data;

	unsigned short depth;
	unsigned short parentIndex; // The index of this node in its tree parent's child list.
	unsigned int leafIndex; // The index in the original network file if a leaf node.
	SNode* parentNode;
	NodePtrList children;
	EdgePtrList inEdges;
	EdgePtrList outEdges;

private:
	std::map<SNode*, SEdge*> mapOfAggregatedOutEdges; // The key object is the target node


public:

	// Capacity:
	unsigned int serializationSize(bool writeEdges)
	{
		unsigned int size = MIN_TOTAL_SIZE + data.name.length() * SIZE_OF_CHAR + (children.size() > 0 ? SIZE_OF_UNSIGNED_INT : 0);
		if (writeEdges)
		{
			size += SIZE_OF_UNSIGNED_SHORT; // outDegree
			for (SNode::EdgePtrList::iterator it = outEdges.begin(); it != outEdges.end(); ++it)
				size += (*it)->serializationSize();
		}
		return size;
	}

	// Operations:
	void serialize(ofstream_binary& outFile, unsigned int childPosition, bool writeEdges)
	{
		outFile << static_cast<unsigned short>(data.name.length()); // unsigned short nameLength
		outFile << data.name;					// char* name
		outFile << static_cast<float>(data.flow); // float pageRank
		unsigned short numChildren = static_cast<unsigned short>(children.size());
		outFile << numChildren; 			// unsigned short numChildren
		if (numChildren > 0)
			outFile << childPosition;		// unsigned int childPosition

		if (writeEdges)
		{
			unsigned short outDegree = static_cast<unsigned short>(outEdges.size());
			outFile << outDegree;
			for (SNode::EdgePtrList::const_iterator it = outEdges.begin(); it != outEdges.end(); ++it)
				(*it)->serialize(outFile);
		}
	}

	// Accessors:
	SNode& lastChild()
	{
		return *children.back();
	}

	SNode& firstChild()
	{
		return *children.front();
	}



	void createEdge(SNode* target, double flow)
	{
		createEdge(this, target, flow, NULL);
	}

private:

	/**
	 * Create a weighted edge between the source and target node.
	 * If a non-null child edge is provided, its parentEdge property will be set to the created edge.
	 */
	void createEdge(SNode* source, SNode* target, float flow, SEdge* childEdge = NULL)
	{
		SEdge* edge;
		std::map<SNode*, SEdge*>::iterator find = source->mapOfAggregatedOutEdges.find(target);
		if (find == source->mapOfAggregatedOutEdges.end()) // Add the weight if an edge is already created between the nodes
		{
			edge = new SEdge(source, target, flow);
			buildShortestPath(*edge);
			std::cout << "\tCreated new edge: " << toString(*edge) << " on depth: " << source->depth << std::endl;
			source->outEdges.push_back(edge);
			target->inEdges.push_back(edge);

			// Map the edge so that other leaf edges may be aggregated to it.
			source->mapOfAggregatedOutEdges.insert(std::make_pair(target, edge));

			if (childEdge != NULL)
				childEdge->parentEdge = edge;

			// Create the edge between the parent nodes recursively while different.
			if (source->parentNode != target->parentNode)
				createEdge(source->parentNode, target->parentNode, flow, edge);
		}
		else
		{
			// The parent edge already exists, only aggregate the weight recursively
			edge = find->second;
			if (childEdge != NULL)
				childEdge->parentEdge = edge;
			std::cout << "\tAdd weight (" << flow << ") to " << toString(*edge) << std::endl;
			do
			{
				edge->flow += flow;
				edge = edge->parentEdge;
			}
			while (edge != NULL);
		}
	}


	void buildShortestPath(SEdge& edge)
	{
		unsigned short depthFromCommonAncestor = 0;
		SNode* sourceAncestor = edge.source;
		SNode* targetAncestor = edge.target;
		while (sourceAncestor != targetAncestor)
		{
			++depthFromCommonAncestor;
			edge.targetUpPath.push_back(targetAncestor->parentIndex);
			sourceAncestor = sourceAncestor->parentNode;
			targetAncestor = targetAncestor->parentNode;
		}
		edge.depthFromCommonAncestor = depthFromCommonAncestor;
	}


	std::string toString(const SEdge& edge)
	{
		std::ostringstream oss;
		oss << edge.source->data.name << " --> " << edge.target->data.name << " (" << edge.flow << ")";
		return oss.str();
	}

};

class HierarchicalNetwork
{
public:
	typedef SNode	node_type;

	HierarchicalNetwork(unsigned int numLeafNodes)
	:	m_rootNode(1.0),
		m_leafNodes(numLeafNodes),
		m_numNodesInTree(1)
	{}

	virtual ~HierarchicalNetwork() {}


	SNode& getRootNode()
	{
		return m_rootNode;
	}

	SNode& addNode(SNode& parent, double flow)
	{
		SNode* n = new SNode(NodeData(flow), parent.depth + 1, parent.children.size());
		//TODO: Let the node be created in the node class as that is responsible for deleting it!
		parent.addChild(*n);
		++m_numNodesInTree;
		return *n;
	}

	SNode& addLeafNode(SNode& parent, double flow, std::string name, unsigned int leafIndex)
	{
		SNode& n = addNode(parent, flow);
		n.data.name = name;
		n.leafIndex = leafIndex;
		m_leafNodes[leafIndex] = &n;
		return n;
	}

	/**
	 * Add flow-edges to the tree. This method can aggregate the edges between the leaf-nodes
	 * up in the tree based on the edge aggregation policy.
	 * @param aggregationFilter A flag to decide the type of aggregation.
	 * @note The EdgeAggregationPolicy enumeration lists the available aggregation policies.
	 */
	void addLeafEdge(unsigned int sourceLeafNodeIndex, unsigned int targetLeafNodeIndex, double flow)
	{

		SNode* source = m_leafNodes[sourceLeafNodeIndex];
		SNode* target = m_leafNodes[targetLeafNodeIndex];

		// Allow only horizontal flow edges
		if (source->depth > target->depth)
			do { source = source->parentNode; } while (source->depth != target->depth);
		else if (source->depth < target->depth)
			do { target = target->parentNode; } while (source->depth != target->depth);

		source->createEdge(target, flow);
	}



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
	 * binary rootNode
	 *
	 * [depth 1]
	 * binary firstModule
	 * binary secondModule
	 * binary ...
	 *
	 * [depth 2]
	 * binary firstChildOfFirstModule
	 * binary secondChildOfFirstModule
	 * binary ...
	 * binary firstChildOfSecondModule
	 * binary ...
	 *
	 * [EOF]
	 *
	 * Each binary node contains a pointer to where its children is located in the file
	 */
	void writeStreamableTree(const std::string& fileName, bool writeEdges)
	{
//		SafeOutFile out(fileName.c_str());
		SafeOutFileBinary out(fileName.c_str());
//		SafeOutFileDebug out(fileName.c_str());

		std::string magicTag ("infomap");
		unsigned short firstTagLength = magicTag.length();
		unsigned short formatVersion = 1;
		unsigned int childPosition = SIZE_OF_UNSIGNED_SHORT + firstTagLength * SIZE_OF_CHAR + SIZE_OF_UNSIGNED_SHORT + SIZE_OF_UNSIGNED_INT * 2;
//		unsigned int childPosition = SIZE_OF_UNSIGNED_INT * 2;
		unsigned int numNodesTotal = m_numNodesInTree;


		out << firstTagLength;
		out << magicTag;
		out << formatVersion;
		out << childPosition;
		out << numNodesTotal;

		std::deque<SNode*> nodeList;
		nodeList.push_back(&m_rootNode);
		childPosition += m_rootNode.serializationSize(writeEdges);
		while (nodeList.size() > 0) // visit tree breadth first
		{
			SNode& node = *nodeList.front();

			// Write current node to the file
			node.serialize(out, childPosition, writeEdges);
			// add the children of the current node to the list and aggregate their binary sizes to the children pointer
			for (unsigned int i = 0; i < node.children.size(); ++i)
			{
				SNode* child = node.children[i];
				nodeList.push_back(child);
				childPosition += child->serializationSize(writeEdges);
			}

			// remove the printed node from the list
			nodeList.pop_front();
		}
	}

private:

	static bool compareLeafNodePredicate(const SNode* lhs, const SNode* rhs) { return (lhs->leafIndex < rhs->leafIndex); }

	void sortLeafNodes()
	{
		std::cout << "Sort leaf nodes according to original order... ";
		std::sort(m_leafNodes.begin(), m_leafNodes.end(), compareLeafNodePredicate);
		std::cout << "done!" << std::endl;
	}



	SNode m_rootNode;
	SNode::NodePtrList m_leafNodes;
	unsigned int m_numNodesInTree;

};

#endif /* HIERARCHICALNETWORK_H_ */
