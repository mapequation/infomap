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


#include "HierarchicalNetwork.h"
#include <map>
#include <stdexcept>
#include "convert.h"

void HierarchicalNetwork::init(std::string networkName, bool directedEdges,
			double codelength, double oneLevelCodelength, std::string infomapVersion)
{
	// First clear if necessary
	clear();

	m_networkName = networkName;
	m_directedEdges = directedEdges;
	m_numLeafEdges = 0;
	m_numNodesInTree = 1;
	m_maxDepth = 0;
	m_codelength = codelength;
	m_oneLevelCodelength = oneLevelCodelength;
	m_infomapVersion = infomapVersion;
}

void HierarchicalNetwork::clear()
{
	m_rootNode.clear();
}



SNode& HierarchicalNetwork::addNode(SNode& parent, double flow, double exitFlow)
{
	SNode* n = new SNode(NodeData(flow, exitFlow), parent.depth + 1, parent.children.size(), m_numNodesInTree);
	//TODO: Let the node be created in the node class as that is responsible for deleting it!
	parent.addChild(*n);
	++m_numNodesInTree;
	return *n;
}

SNode& HierarchicalNetwork::addLeafNode(SNode& parent, double flow, double exitFlow, std::string name, unsigned int leafIndex)
{
	if (leafIndex > m_leafNodes.size())
		throw std::range_error("In HierarchicalNetwork::addLeafNode(), leaf index out of range or missed calling prepare method.");
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

void HierarchicalNetwork::prepareAddLeafNodes(unsigned int numLeafNodes)
{
	m_leafNodes.resize(numLeafNodes);
}

bool HierarchicalNetwork::addLeafEdge(unsigned int sourceLeafNodeIndex, unsigned int targetLeafNodeIndex, double flow)
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
	bool createdNewEdge = source->parentNode->createChildEdge(source->parentIndex, target->parentIndex, flow, m_directedEdges);

	++m_numLeafEdges;
	return createdNewEdge;
}

void HierarchicalNetwork::propagateNodeNameUpInHierarchy(SNode& node)
{
	if (node.parentNode != 0 && node.parentIndex == 0)
	{
		node.parentNode->data.name = io::Str() << node.data.name << (node.isLeaf? ",." : ".");
		propagateNodeNameUpInHierarchy(*node.parentNode);
	}
}

void HierarchicalNetwork::writeStreamableTree(const std::string& fileName, bool writeEdges)
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

//		std::cout << "\nMeta data:\n";
//		std::cout << "  directedEdges: " << m_directedEdges << "\n";
//		std::cout << "  networkName: " << m_networkName << "\n";
//		std::cout << "  numLeafNodes: " << numLeafNodes << "\n";
//		std::cout << "  numLeafEdges: " << m_numLeafEdges << "\n";
//		std::cout << "  numNodeInTree: " << m_numNodesInTree << "\n";
//		std::cout << "  maxDepth: " << m_maxDepth << "\n";
//		std::cout << "  oneLevelCodelength: " << m_oneLevelCodelength << "\n";
//		std::cout << "  codelength: " << m_codelength << "\n";

	std::deque<SNode*> nodeList;
	nodeList.push_back(&m_rootNode);
	m_rootNode.data.name = m_networkName;
	unsigned int childPosition = out.size() + m_rootNode.serializationSize(writeEdges);
	using namespace SerialTypes;
	while (nodeList.size() > 0) // visit tree breadth first
	{
		SNode& node = *nodeList.front();

		// Write current node to the file
		node.serialize(out, childPosition, writeEdges);
		// add the children of the current node to the list and aggregate their binary sizes to the children pointer
		childSize_t numChildren = numeric_cast<childSize_t>(node.children.size());
		for (childSize_t i = 0; i < numChildren; ++i)
		{
			SNode* child = node.children[i];
			nodeList.push_back(child);
			childPosition += child->serializationSize(writeEdges);
		}

		// remove the printed node from the list
		nodeList.pop_front();
	}
}

void HierarchicalNetwork::readStreamableTree(const std::string& fileName)
{
	std::cout << "Read streamable tree from file '" << fileName << "'... ";
	SafeBinaryInFile dataStream(fileName.c_str());
	std::string magicTag, infomapVersion, infomapOptions;
	unsigned int numLeafNodes, numLeafEdges, numNodesInTree;
	dataStream >> magicTag;
	if (magicTag != "Infomap")
		throw FileFormatError("The first content of the file doesn't match the format.");
	dataStream >> m_infomapVersion
		>> infomapOptions
		>> m_directedEdges
		>> m_networkName
		>> numLeafNodes
		>> numLeafEdges
		>> numNodesInTree
		>> m_maxDepth
		>> m_oneLevelCodelength
		>> m_codelength;

	std::cout << "\nMetadata:\n";
	std::cout << "  Infomap version: \"" << m_infomapVersion << "\"" << std::endl;
	std::cout << "  Infomap options: " << infomapOptions << std::endl;
	std::cout << "  Directed edges: " << m_directedEdges << std::endl;
	std::cout << "  Network name: \"" << m_networkName << "\"" << std::endl;
	std::cout << "  Num leaf nodes: " << numLeafNodes << std::endl;
	std::cout << "  Num leaf edges: " << numLeafEdges << std::endl;
	std::cout << "  Num nodes in tree: " << numNodesInTree << std::endl;
	std::cout << "  Max depth: " << m_maxDepth << std::endl;
	std::cout << "  One-level codelength: " << m_oneLevelCodelength << std::endl;
	std::cout << "  Codelength: " << m_codelength << std::endl;

	std::deque<SNode*> nodeList;
	nodeList.push_back(&m_rootNode);
	unsigned int numEdges = 0;
	while (nodeList.size() > 0) // visit tree breadth first
	{
		SNode& node = *nodeList.front();
		nodeList.pop_front();
		unsigned short numChildren = node.deserialize(dataStream);
		for (unsigned short i = 0; i < numChildren; ++i)
		{
			SNode& child = addNode(node, 0.0, 0.0);
			nodeList.push_back(&child);
		}
		if (node.parentNode != NULL && static_cast<unsigned int>(node.parentIndex + 1) == node.parentNode->children.size())
		{
			numEdges += node.parentNode->deserializeEdges(dataStream, m_directedEdges);
		}
		if (m_numNodesInTree > numNodesInTree)
			throw FileFormatError("Tree overflow");
	}
	std::cout << "Done! Deserialized " << m_numNodesInTree << " nodes and " << numEdges << " links.\n";

	m_numLeafEdges = numLeafEdges;
}


void HierarchicalNetwork::writeHumanReadableTree(const std::string& fileName, bool writeHierarchicalNetworkEdges)
{
	SafeOutFile out(fileName.c_str());
	out << "# Network '" << m_networkName << "', size: " << m_leafNodes.size() << " nodes in " << m_maxDepth << " levels, codelength: " << m_codelength << " bits.\n";

	writeHumanReadableTreeRecursiveHelper(out, m_rootNode);
	if (writeHierarchicalNetworkEdges)
		writeHumanReadableTreeFlowLinksRecursiveHelper(out, m_rootNode);
}

void HierarchicalNetwork::writeHumanReadableTreeRecursiveHelper(std::ostream& out, SNode& node, std::string prefix)
{
	for (unsigned int i = 0; i < node.children.size(); ++i)
	{
		SNode& child = *node.children[i];
		if (child.isLeaf)
		{
			out << prefix << (i+1) << " " << child.data.flow << " \"" << child.data.name << "\"\n";
//			out << prefix << (i+1) << " " << child.data.flow << " " << child.data.enterFlow << " " << child.data.exitFlow << " \"" << child.data.name << "\"\n";
		}
		else
		{
			const std::string subPrefix = io::Str() << prefix << (i+1) << ":";
			writeHumanReadableTreeRecursiveHelper(out, child, subPrefix);
		}
	}
}

void HierarchicalNetwork::writeHumanReadableTreeFlowLinksRecursiveHelper(std::ostream& out, SNode& node, std::string prefix)
{
	if (node.children.empty())
		return;

	// Write edges after the last child of the parent node
	const SNode::ChildEdgeList& edges = node.childEdges;
	// First sort the edges
	std::multimap<double, ChildEdge, std::greater<double> > sortedEdges;
	for (SNode::ChildEdgeList::const_iterator it = edges.begin(); it != edges.end(); ++it)
		sortedEdges.insert(std::make_pair(it->flow, *it));
	if (prefix.empty())
		out << "*Edges " << edges.size() << ", module 'root':(" << node.children.size() << ")\n";
	else
		out << "*Edges " << edges.size() << ", module " << prefix << "(" << node.children.size() << ")\n";
	std::multimap<double, ChildEdge, std::greater<double> >::const_iterator it(sortedEdges.begin());

	for (unsigned int i = 0; i < edges.size(); ++i, ++it)
	{
		out << (it->second.source + 1) << " " << (it->second.target + 1) << " " << it->second.flow << "\n";
	}

	for (unsigned int i = 0; i < node.children.size(); ++i)
	{
		SNode& child = *node.children[i];

		if (!child.isLeaf)
		{
			const std::string subPrefix = io::Str() << prefix << (i+1) << ":";
			writeHumanReadableTreeFlowLinksRecursiveHelper(out, child, subPrefix);
		}
	}
}

void HierarchicalNetwork::writeMap(const std::string& fileName)
{
	if (m_maxDepth < 2)
	{
		std::cout << "(skipping .map, no modular solution) ";
		return;
	}

	// First collect the leaf nodes under each top module, sorted on flow
	unsigned int numModules = m_rootNode.children.size();
	typedef std::multimap<double, SNode*, std::greater<double> > NodeMap;
	std::vector<NodeMap> nodeMaps(numModules);
	unsigned int numNodes = 0;

	for (unsigned int i = 0; i < numModules; ++i)
	{
		SNode& module = *m_rootNode.children[i];
		if (module.children.empty())
			continue;

		NodeMap& nodeMap = nodeMaps[i];
		SNode* node = &module;
		LeafIterator li(node);
		LeafIterator liEnd(module.nextSibling());
		while (li != liEnd)
		{
//			std::cout << i << std::string(module.depth, ' ') << ": inserting " << li->data.name << "\n";
			nodeMap.insert(std::make_pair(li->data.flow, li.base()));
			++li;
			++numNodes;
		}

	}


	SafeOutFile out(fileName.c_str());

	out << "# modules: " << numModules << "\n";
	out << "# modulelinks: " << m_rootNode.childEdges.size() << "\n";
	out << "# nodes: " << numNodes << "\n";
	out << "# links: " << m_numLeafEdges << "\n";
	out << "# codelength: " << m_codelength << "\n";
	out << "*" << (m_directedEdges ? "Directed" : "Undirected") << "\n";

	out << "*Modules " << numModules << "\n";
	for (unsigned int i = 0; i < numModules; ++i)
	{
		SNode& module = *m_rootNode.children[i];
		NodeMap& leafNodes = nodeMaps[i];
		SNode& biggestLeafNode = *(leafNodes.begin()->second); // Use the biggest leaf node under each super module to name the super module
		out << (i + 1) << " \"" << biggestLeafNode.data.name << ",...\" " << module.data.flow << " " << module.data.exitFlow << "\n";
	}

	out << "*Nodes " << numNodes << "\n";
	for (unsigned int i = 0; i < numModules; ++i)
	{
		NodeMap& nodeMap = nodeMaps[i];
		unsigned int nodeNumber = 1;
		for (NodeMap::iterator it(nodeMap.begin()), itEnd(nodeMap.end());
				it != itEnd; ++it, ++nodeNumber)
		{
			out << (i + 1) << ":" << nodeNumber << " \"" << it->second->data.name << "\" " <<
				it->first << "\n";
		}
	}


	out << "*Links " << m_rootNode.childEdges.size() << "\n";
	for (SNode::ChildEdgeList::iterator it(m_rootNode.childEdges.begin()); it != m_rootNode.childEdges.end(); ++it)
	{
		const ChildEdge& edge = *it;
		out << (edge.source+1) << " " << (edge.target+1) << " " << edge.flow << "\n";
	}
}


