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
#include "../utils/Logger.h"

#ifdef NS_INFOMAP
namespace infomap
{
#endif

void HierarchicalNetwork::init(std::string networkName, double codelength, double oneLevelCodelength)
{
	// First clear if necessary
	clear();

	m_networkName = networkName;
	m_numLeafEdges = 0;
	m_numNodesInTree = 1;
	m_maxDepth = 0;
	m_codelength = codelength;
	m_oneLevelCodelength = oneLevelCodelength;
}

void HierarchicalNetwork::clear()
{
	m_rootNode.clear();
}

void HierarchicalNetwork::clear(const Config& conf)
{
	clear();
	m_config = conf;
}



SNode& HierarchicalNetwork::addNode(SNode& parent, double flow, double exitFlow)
{
	SNode* n = new SNode(NodeData(flow, exitFlow), parent.depth + 1, parent.children.size(), m_numNodesInTree);
	//TODO: Let the node be created in the node class as that is responsible for deleting it!
	parent.addChild(*n);
	++m_numNodesInTree;
	return *n;
}

SNode& HierarchicalNetwork::addLeafNode(SNode& parent, double flow, double exitFlow, const std::string& name, unsigned int leafIndex)
{
	return addLeafNode(parent, flow, exitFlow, name, leafIndex, leafIndex);
}

SNode& HierarchicalNetwork::addLeafNode(SNode& parent, double flow, double exitFlow, const std::string& name, unsigned int leafIndex, unsigned int originalIndex)
{
	if (leafIndex > m_leafNodes.size())
		throw std::range_error("In HierarchicalNetwork::addLeafNode(), leaf index out of range or missed calling prepare method.");
	SNode& n = addNode(parent, flow, exitFlow);
	n.data.name = name;
	n.isLeaf = true;
	n.originalLeafIndex = originalIndex;
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

SNode& HierarchicalNetwork::addLeafNode(SNode& parent, double flow, double exitFlow, const std::string& name, unsigned int leafIndex,
		unsigned int originalIndex, bool isMemoryNode, unsigned int stateIndex, unsigned int physIndex)
{
	SNode& n = addLeafNode(parent, flow, exitFlow, name, leafIndex, originalIndex);
	n.isMemoryNode = isMemoryNode;
	n.stateIndex = stateIndex;
	n.physIndex = physIndex;
	return n;
}

void HierarchicalNetwork::prepareAddLeafNodes(unsigned int numLeafNodes)
{
	m_numLeafNodes = numLeafNodes;
	m_leafNodes.assign(numLeafNodes, 0);
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

	out << magicTag;
	out << m_config.version;
	out << m_infomapOptions;
	out << m_directedEdges;
	out << m_networkName;
	out << m_numLeafNodes;
	out << m_numLeafEdges;
	out << m_numNodesInTree;
	out << m_maxDepth;
	out << m_oneLevelCodelength;
	out << m_codelength;

//		Log() << "\nMeta data:\n";
//		Log() << "  directedEdges: " << m_directedEdges << "\n";
//		Log() << "  networkName: " << m_networkName << "\n";
//		Log() << "  numLeafNodes: " << numLeafNodes << "\n";
//		Log() << "  numLeafEdges: " << m_numLeafEdges << "\n";
//		Log() << "  numNodeInTree: " << m_numNodesInTree << "\n";
//		Log() << "  maxDepth: " << m_maxDepth << "\n";
//		Log() << "  oneLevelCodelength: " << m_oneLevelCodelength << "\n";
//		Log() << "  codelength: " << m_codelength << "\n";

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
	Log() << "Read streamable tree from file '" << fileName << "'... ";
	SafeBinaryInFile dataStream(fileName.c_str());
	std::string magicTag;
	unsigned int numNodesInTree;
	dataStream >> magicTag;
	if (magicTag != "Infomap")
		throw FileFormatError("The first content of the file doesn't match the format.");
	dataStream >> m_infomapVersion
		>> m_infomapOptions
		>> m_directedEdges
		>> m_networkName
		>> m_numLeafNodes
		>> m_numLeafEdges
		>> numNodesInTree
		>> m_maxDepth
		>> m_oneLevelCodelength
		>> m_codelength;

	Log() << "\nMetadata:\n";
	Log() << "  Infomap version: \"" << m_infomapVersion << "\"" << std::endl;
	Log() << "  Infomap options: " << m_infomapOptions << std::endl;
	Log() << "  Directed edges: " << m_directedEdges << std::endl;
	Log() << "  Network name: \"" << m_networkName << "\"" << std::endl;
	Log() << "  Num leaf nodes: " << m_numLeafNodes << std::endl;
	Log() << "  Num leaf edges: " << m_numLeafEdges << std::endl;
	Log() << "  Num nodes in tree: " << numNodesInTree << std::endl;
	Log() << "  Max depth: " << m_maxDepth << std::endl;
	Log() << "  One-level codelength: " << m_oneLevelCodelength << std::endl;
	Log() << "  Codelength: " << m_codelength << std::endl;

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
		// Parse edges after last child for each module
		if (node.parentNode != NULL && static_cast<unsigned int>(node.parentIndex + 1) == node.parentNode->children.size())
		{
			numEdges += node.parentNode->deserializeEdges(dataStream, m_directedEdges);
		}
		if (m_numNodesInTree > numNodesInTree)
			throw FileFormatError("Tree overflow");
	}
	Log() << "Done! Deserialized " << m_numNodesInTree << " nodes and " << numEdges << " links.\n";
}


void HierarchicalNetwork::writeClu(const std::string& fileName, int moduleIndexDepth)
{
	markNodesToSkip();

	SafeOutFile out(fileName.c_str());

	out << "# '" << m_infomapOptions << "' -> " << m_numLeafNodes << " nodes ";
	if (m_numLeafEdges > 0)
		out << "and " << m_numLeafEdges << " links ";
	out << "partitioned in " << m_config.elapsedTime() << " from codelength " <<
		io::toPrecision(m_oneLevelCodelength, 9, true) << " in one level to codelength " <<
		io::toPrecision(m_codelength, 9, true) << " in " << m_maxDepth << " levels.\n";
	if (m_config.printExpanded) {
		if (m_config.isMultiplexNetwork())
			out << "# layer node cluster flow:\n";
		else
			out << "# state_node node cluster flow:\n";
	}
	else
		out << "# node cluster flow:\n";

	unsigned int indexOffset = m_config.zeroBasedNodeNumbers? 0 : 1;
	for (TreeIterator it(&m_rootNode, moduleIndexDepth); !it.isEnd(); ++it) {
		SNode &node = *it;
		if (node.isLeafNode()) {
			if (m_config.isBipartite()) {
				if (node.originalLeafIndex < m_config.minBipartiteNodeIndex)
					out << 'n' << node.originalLeafIndex + indexOffset << " " << it.moduleIndex() + 1 << " " << node.data.flow << "\n";
				else
					out << 'f' << node.originalLeafIndex + indexOffset - m_config.minBipartiteNodeIndex << " " << it.moduleIndex() + 1 << " " << node.data.flow << "\n";
			}
			else {
				if (m_config.printExpanded && node.isMemoryNode)
					out << node.printState(indexOffset) << " " << it.moduleIndex() + 1 << " " << node.data.flow << "\n";
				else
					out << node.originalLeafIndex + indexOffset << " " << it.moduleIndex() + 1 << " " << node.data.flow << "\n";
			}
		}
	}
}

void HierarchicalNetwork::writeMap(const std::string& fileName)
{
	if (m_maxDepth < 2)
	{
		Log() << "(skipping .map, no modular solution) ";
		return;
	}

	markNodesToSkip();

	// First collect the leaf nodes under each top module, sorted on flow
	// Note that modules can be skipped
	unsigned int numModules = m_rootNode.children.size();
	typedef std::multimap<double, SNode*, std::greater<double> > NodeMap;
	std::vector<NodeMap> nodeMaps;
	nodeMaps.reserve(numModules); // Reserve size for max number of modules (no skipped)

	unsigned int numNodes = 0;
	for (TreeIterator it(&m_rootNode, 1); !it.isEnd(); ++it) {
		if (it->isLeafNode()) {
			if (it.moduleIndex() >= nodeMaps.size())
				nodeMaps.push_back(NodeMap());
			 nodeMaps[it.moduleIndex()].insert(std::make_pair(it->data.flow, it.base()));
			 ++numNodes;
		}
	}
	numModules = nodeMaps.size();

	SafeOutFile out(fileName.c_str());

	out << "# modules: " << numModules << "\n";
	out << "# modulelinks: " << m_rootNode.childEdges.size() << "\n";
	out << "# nodes: " << numNodes << "\n";
	out << "# links: " << m_numLeafEdges << "\n";
	out << "# codelength: " << m_codelength << "\n";
	out << "*" << (m_directedEdges ? "Directed" : "Undirected") << "\n";

	out << "*Modules " << numModules << "\n";
	for (ChildIterator it(&m_rootNode); !it.isEnd(); ++it)
	{
		SNode& module = *it;
		unsigned int moduleIndex = it.childIndex();
		NodeMap& leafNodes = nodeMaps[moduleIndex];
		SNode& biggestLeafNode = *(leafNodes.begin()->second); // Use the biggest leaf node under each super module to name the super module
		out << (moduleIndex + 1) << " \"" << biggestLeafNode.data.name << ",...\" " << module.data.flow << " " << module.data.exitFlow << "\n";
	}

	out << "*Nodes " << numNodes << "\n";
	for (ChildIterator moduleIt(&m_rootNode); !moduleIt.isEnd(); ++moduleIt)
	{
		unsigned int moduleIndex = moduleIt.childIndex();
		NodeMap& leafNodes = nodeMaps[moduleIndex];
		unsigned int nodeNumber = 1;
		for (NodeMap::iterator it(leafNodes.begin()), itEnd(leafNodes.end());
				it != itEnd; ++it, ++nodeNumber)
		{
			out << (moduleIndex + 1) << ":" << nodeNumber << " \"" << it->second->data.name << "\" " <<
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

void HierarchicalNetwork::writeHumanReadableTree(const std::string& fileName, bool writeHierarchicalNetworkEdges)
{
	markNodesToSkip();

	SafeOutFile out(fileName.c_str());
	out << "# '" << m_infomapOptions << "' -> " << m_numLeafNodes << " nodes ";
	if (m_numLeafEdges > 0)
		out << "and " << m_numLeafEdges << " links ";
	out << "partitioned in " << m_config.elapsedTime() << " from codelength " <<
		io::toPrecision(m_oneLevelCodelength, 9, true) << " in one level to codelength " <<
		io::toPrecision(m_codelength, 9, true) << " in " << m_maxDepth << " levels.\n";

	if (m_config.printExpanded) {
		if (m_config.isMultiplexNetwork())
			out << "# path flow name layer node:\n";
		else
			out << "# path flow name state_node node:\n";
	}
	else
		out << "# path flow name node:\n";

	unsigned int indexOffset = m_config.zeroBasedNodeNumbers? 0 : 1;
	for (TreeIterator it(&m_rootNode, 2); !it.isEnd(); ++it) {
		SNode &node = *it;
		if (node.isLeafNode()) {
			out << io::stringify(it.path(), ":", 1) << " " << node.data.flow << " \"" << node.data.name << "\" ";
			if (m_config.isBipartite()) {
				if (node.originalLeafIndex < m_config.minBipartiteNodeIndex)
					out << 'n' << node.originalLeafIndex + indexOffset;
				else
					out << 'f' << node.originalLeafIndex + indexOffset - m_config.minBipartiteNodeIndex;
			}
			else {
				if (m_config.printExpanded && node.isMemoryNode)
					out << node.printState(indexOffset);
				else
					out << node.originalLeafIndex + indexOffset;
			}
			out << "\n";
		}
	}

	if (!writeHierarchicalNetworkEdges)
		return;
	
	// out << "#*Links numEdges path numChildren exitFlow\n";
	out << "*Links " << (m_directedEdges ? "directed" : "undirected") << "\n";
	out << "#*Links path exitFlow numEdges numChildren\n";

	for (TreeIterator it(&m_rootNode); !it.isEnd(); ++it)
	{
		SNode &node = *it;
		if (node.isLeafNode())
			continue;

		// Write edges after the last child of the parent node
		const SNode::ChildEdgeList& edges = node.childEdges;
		if (it.path().empty())
			// out << "*Links " << edges.size() << " root " << node.children.size() << " 0.0\n";
			out << "*Links root 0.0 " << edges.size() << " " << node.children.size() << "\n";
		else
			// out << "*Links " << edges.size() << " " << io::stringify(it.path(), ":", 1) <<
			// 	" " << node.children.size() << " " << node.data.exitFlow << "\n";
			out << "*Links " << io::stringify(it.path(), ":", 1) << " " << node.data.exitFlow << " " <<
				edges.size() <<	" " << node.children.size() << "\n";

		// Print sorted edges
		std::multimap<double, ChildEdge, std::greater<double> > sortedEdges;
		for (SNode::ChildEdgeList::const_iterator it = edges.begin(); it != edges.end(); ++it)
			sortedEdges.insert(std::make_pair(it->flow, *it));

		std::multimap<double, ChildEdge, std::greater<double> >::const_iterator edgeIt(sortedEdges.begin());
		for (unsigned int i = 0; i < edges.size(); ++i, ++edgeIt)
		{
			out << (edgeIt->second.source + 1) << " " << (edgeIt->second.target + 1) << " " << edgeIt->second.flow << "\n";
		}
	}
}

void HierarchicalNetwork::markNodesToSkip()
{
	if (m_config.maxNodeIndexVisible == 0)
		return;

	// First assume all should be skipped
	for (TreeIterator it(&m_rootNode); !it.isEnd(); ++it) {
		it->skip = true;
	}

	// Then propagate from unskipped leaf nodes up in tree to unmark branches with included leaf nodes
	for (SNode::NodePtrList::iterator it = m_leafNodes.begin(); it != m_leafNodes.end(); ++it) {
		SNode* node = *it;
		if (node->originalLeafIndex <= m_config.maxNodeIndexVisible) {
			do {
				node->skip = false;
				node = node->parentNode;
			} while (node != NULL);
		}
	}
}

void HierarchicalNetwork::readHumanReadableTree(const std::string& fileName)
{
	if (m_leafNodes.empty())
		throw InternalOrderError("Hierarchical network not initialized before parsing tree.");
	std::string line;
	std::string buf;
	SafeInFile input(fileName.c_str());
	Log() << "Parsing tree '" << fileName << "'... " << std::flush;

	std::string header;
	unsigned int lineNr = 0;
	std::istringstream ss;
	unsigned int nodeCount = 0;
	while(getline(input, line))
	{
		++lineNr;
		if (line[0] == '#')
		{
			if (lineNr == 1)
				header = line; // e.g. '# Codelength = 8.45977 bits.'
			continue;
		}
		if (nodeCount > m_leafNodes.size())
			throw MisMatchError("There are more nodes in the tree than in the network.");

		ss.clear();
		ss.str(line);
		std::string treePath;
		if (!(ss >> treePath))
			throw BadConversionError(io::Str() << "Can't parse node path from line " << lineNr << " ('" << line << "').");
		double flow;
		if (!(ss >> flow))
			throw BadConversionError(io::Str() << "Can't parse node flow from line " << lineNr << " ('" << line << "').");
		std::string name;
		// Get the name by extracting the rest of the stream until the first quotation mark and then the last.
		if (!getline(ss, name, '"'))
			throw BadConversionError(io::Str() << "Can't parse node name from line " << lineNr << " ('" << line << "').");
		if (!getline(ss, name, '"'))
			throw BadConversionError(io::Str() << "Can't parse node name from line " << lineNr << " ('" << line << "').");
//		unsigned int originalIndex = 0;
//		bool gotOriginalIndex = (ss >> originalIndex);

		// Analyze the path and build up the tree
		ss.clear(); // Clear the eofbit from last extraction!
		ss.str(treePath);
		unsigned int childIndex;
		SNode* node = &m_rootNode;
		while (ss >> childIndex)
		{
			ss.get(); // Extract the delimiting character also
			if (childIndex == 0)
				throw FileFormatError("There is a '0' in the tree path, lowest allowed integer is 1.");
			--childIndex;
			// Create new node if path doesn't exist
			if (node->children.size() <= childIndex)
			{
				SNode& child = addNode(*node, 0.0, 0.0);
				node->children.push_back(&child);
			}
			node = node->children.back();
		}
		node->data.flow = flow;
		node->data.name = name;
		node->isLeaf = true;
		++nodeCount;
	}
	if (nodeCount < m_leafNodes.size())
		throw MisMatchError("There are less nodes in the tree than in the network.");

	Log() << "done!" << std::endl;
}

#ifdef NS_INFOMAP
}
#endif
