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
#include <set>
#include "SafeFile.h"

enum EdgeAggregationPolicy { NONE, PARTIAL, FULL };

static const unsigned int SIZE_OF_UNSIGNED_INT = sizeof(unsigned int);		// 4 byte (32 bit)
static const unsigned int SIZE_OF_CHAR = sizeof(char);						// 1 byte (8 bit)
static const unsigned int SIZE_OF_FLOAT = sizeof(float); 					// 4 byte (32 bit)
static const unsigned int SIZE_OF_UNSIGNED_SHORT = sizeof(unsigned short);	// 2 byte (16 bit)

// Min node size: name size + numChildren + flow + exitFlow
static const unsigned int MIN_TOTAL_SIZE = 2*SIZE_OF_UNSIGNED_SHORT + 2*SIZE_OF_FLOAT;
// source + target + flow
static const unsigned int CHILD_EDGE_SIZE = 2*SIZE_OF_UNSIGNED_SHORT + SIZE_OF_FLOAT;

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
	unsigned short int source;
	unsigned short int target;
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
		for (int i = children.size() - 1; i >= 0; --i) {
			delete children[i];
		}
		children.clear();
	}

	void addChild(SNode& child)
	{
		child.parentIndex = children.size();
		children.push_back(&child);
		child.parentNode = this;
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
		unsigned int size = MIN_TOTAL_SIZE + data.name.length() * SIZE_OF_CHAR;
		if (children.size() > 0)
			size += SIZE_OF_UNSIGNED_INT + SIZE_OF_UNSIGNED_SHORT;
		// The edges are printed out after the last child
		if (writeEdges && parentNode != NULL && static_cast<unsigned int>(parentIndex + 1) == parentNode->children.size())
		{
			// numEdges + {edges}
			size += parentNode->childEdges.size() * CHILD_EDGE_SIZE + SIZE_OF_UNSIGNED_SHORT;
		}
		return size;
	}

	// Operations:
	void serialize(SafeBinaryOutFile& outFile, unsigned int childPosition, bool writeEdges)
	{
//		outFile << static_cast<unsigned short>(data.name.length()); // unsigned short nameLength
		outFile << data.name;					// char* name
		outFile << static_cast<float>(data.flow); // float flow
		outFile << static_cast<float>(data.exitFlow); // float exitFlow
		unsigned short numChildren = static_cast<unsigned short>(children.size());
		outFile << numChildren; 			// unsigned short numChildren
		if (numChildren > 0)
		{
			outFile << depthBelow;			// unsigned short depthBelow
			outFile << childPosition;		// unsigned int childPosition
		}

		// Write edges after the last child of the parent node
		if (writeEdges && parentNode != NULL && static_cast<unsigned int>(parentIndex + 1) == parentNode->children.size())
		{
			const ChildEdgeList& edges = parentNode->childEdges;
			unsigned short numEdges = static_cast<unsigned short>(edges.size());
			// First sort the edges
			std::multimap<double, ChildEdge, std::greater<double> > sortedEdges;
			for (SNode::ChildEdgeList::const_iterator it = edges.begin(); it != edges.end(); ++it)
				sortedEdges.insert(std::make_pair(it->flow, *it));
			outFile << numEdges;
			for (std::multimap<double, ChildEdge, std::greater<double> >::const_iterator it =
					sortedEdges.begin(); it != sortedEdges.end(); ++it)
			{
				outFile << it->second.source;
				outFile << it->second.target;
				outFile << static_cast<float>(it->second.flow);
			}
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


	void createChildEdge(unsigned short int sourceIndex, unsigned short int targetIndex, double flow, bool directed)
	{
		if (!directed && sourceIndex > targetIndex)
			std::swap(sourceIndex, targetIndex);

		ChildEdge edge(sourceIndex, targetIndex, flow);

		ChildEdgeList::iterator it = childEdges.find(edge);
		if (it == childEdges.end())
			childEdges.insert(edge);
		else
			it->flow += flow;

	}

};

class HierarchicalNetwork
{
public:
	typedef SNode	node_type;

	HierarchicalNetwork(std::string networkName, unsigned int numLeafNodes, bool directedEdges,
			double codelength, double oneLevelCodelength, std::string infomapVersion)
	:	m_rootNode(1.0, 0, 0, 0),
	 	m_networkName(networkName),
		m_leafNodes(numLeafNodes),
		m_directedEdges(directedEdges),
		m_numLeafEdges(0),
		m_numNodesInTree(1),
		m_maxDepth(0),
		m_codelength(codelength),
		m_oneLevelCodelength(oneLevelCodelength),
		m_infomapVersion(infomapVersion)
	{}

	virtual ~HierarchicalNetwork() {}


	SNode& getRootNode()
	{
		return m_rootNode;
	}

	SNode& addNode(SNode& parent, double flow, double exitFlow)
	{
		SNode* n = new SNode(NodeData(flow, exitFlow), parent.depth + 1, parent.children.size(), m_numNodesInTree);
		//TODO: Let the node be created in the node class as that is responsible for deleting it!
		parent.addChild(*n);
		++m_numNodesInTree;
		return *n;
	}

	SNode& addLeafNode(SNode& parent, double flow, double exitFlow, std::string name, unsigned int leafIndex)
	{
		SNode& n = addNode(parent, flow, exitFlow);
		n.data.name = name;
		n.isLeaf = true;
		n.leafIndex = leafIndex;
		m_leafNodes[leafIndex] = &n;
		propagateNodeNameUpInHierarchy(n);
		if (n.depth > m_maxDepth)
			m_maxDepth = n.depth;
		SNode* node = n.parentNode;
		unsigned short currentDepthBelow = 1;
		while (node != NULL && node->depthBelow < currentDepthBelow)
		{
			node->depthBelow = currentDepthBelow++;
			node = node->parentNode;
		}

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

//		source->createEdge(target, flow, m_directedEdges);

		// Only add links under the same parent
		while (source->parentNode != target->parentNode)
		{
			source = source->parentNode;
			target = target->parentNode;
		}
		source->parentNode->createChildEdge(source->parentIndex, target->parentIndex, flow, m_directedEdges);

		++m_numLeafEdges;
	}

	void propagateNodeNameUpInHierarchy(SNode& node)
	{
		if (node.parentNode != 0 && node.parentIndex == 0)
		{
			node.parentNode->data.name = io::Str() << node.data.name << (node.isLeaf? ",." : ".");
			propagateNodeNameUpInHierarchy(*node.parentNode);
		}
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
	void writeStreamableTree(const std::string& fileName, bool writeEdges)
	{
		SafeBinaryOutFile out(fileName.c_str());

		std::string magicTag ("Infomap");
		unsigned int numLeafNodes = m_leafNodes.size();
		std::string infomapOptions("");

		out << magicTag;
		out << m_infomapVersion;
		out << infomapOptions;
		out << m_directedEdges;
		out << m_networkName;
		out << numLeafNodes;
		out << m_numLeafEdges;
		out << m_numNodesInTree;
		out << m_maxDepth;
		out << m_oneLevelCodelength;
		out << m_codelength;

		std::deque<SNode*> nodeList;
		nodeList.push_back(&m_rootNode);
		m_rootNode.data.name = m_networkName;
		unsigned int childPosition = out.size() + m_rootNode.serializationSize(writeEdges);
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
