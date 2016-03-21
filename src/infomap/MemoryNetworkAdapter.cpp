/*
 * MemoryNetworkAdapter.cpp
 *
 *  Created on: 26 jun 2015
 *      Author: Daniel
 */

#include "MemoryNetworkAdapter.h"

#include <algorithm>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../io/convert.h"
#include "../io/ClusterReader.h"
#include "../io/Config.h"
#include "../io/SafeFile.h"
#include "../utils/FileURI.h"
#include "../utils/Logger.h"
#include "NodeFactory.h"
#include "treeIterators.h"
#include "TreeData.h"


#ifdef NS_INFOMAP
namespace infomap
{
#endif



bool MemoryNetworkAdapter::readExternalHierarchy(std::string filename)
{
	generateMemoryNodeMap();

	bool ok = true;
	try
	{
		FileURI file(filename);
		if (file.getExtension() == "clu")
			readClu(filename);
		else if (file.getExtension() == "tree")
			readHumanReadableTree(filename);
		else
			throw std::invalid_argument("Extension to external cluster data not recognized.");
	}
	catch (std::exception& err)
	{
		std::cerr << "Error: \"" << err.what() << "\" Ignoring external cluster data.\n";
		ok = false;
	}

	return ok;
}

void MemoryNetworkAdapter::readClu(std::string filename)
{
	Log() << "Parsing memory node clusters from '" << filename << "'... " << std::flush;
	SafeInFile input(filename.c_str());
	std::string line;
	std::istringstream lineStream;
	std::map<unsigned int, unsigned int> clusters;
	unsigned int maxNodeIndex = 0;
	unsigned int numParsedNodes = 0;
	unsigned int numNodesNotFound = 0;

	// # [prior_node | layer] node cluster flow
	while(!std::getline(input, line).fail())
	{
		if (line.length() == 0 || line[0] == '#')
			continue;

		lineStream.clear();
		lineStream.str(line);

		// Parse priorId nodeId clusterId
		unsigned int priorIndex, nodeIndex, clusterIndex;
		if (!(lineStream >> priorIndex >> nodeIndex >> clusterIndex))
			throw FileFormatError(io::Str() << "Couldn't parse [state_node, node, cluster] from line '" << line << "'");

		++numParsedNodes;

		// Get zero-based indexing
		priorIndex -= m_indexOffset;
		nodeIndex -= m_indexOffset;

		StateNode stateNode(priorIndex, nodeIndex);
		std::map<StateNode, unsigned int>::iterator memIt = m_memNodeToIndex.find(stateNode);
		if (memIt == m_memNodeToIndex.end()) {
			++numNodesNotFound;
			continue;
		}

		maxNodeIndex = std::max(maxNodeIndex, std::max(priorIndex, nodeIndex));

		unsigned int memNodeIndex = memIt->second;

		clusters[memNodeIndex] = clusterIndex;
	}

	unsigned int zeroMinusOne = 0;
	--zeroMinusOne;
	if (maxNodeIndex == zeroMinusOne)
		throw InputDomainError(io::Str() << "Integer overflow, be sure to use zero-based node numbering if the node numbers start from zero.");

	if (numNodesNotFound > 0)
		Log() << "\n -> Warning: " << numNodesNotFound << " memory nodes not found in network.";


	// Re-map cluster id:s to zero-based compact indices
	std::map<unsigned int, unsigned int> clusterIdToNumber;
	unsigned int clusterNumber = 1; // Start from 1 and use default int() (0) as indicating not assigned
	for (std::map<unsigned int, unsigned int>::const_iterator it(clusters.begin()); it != clusters.end(); ++it) {
		unsigned int clusterId = it->second;
		unsigned int& n = clusterIdToNumber[clusterId];
		if (n == 0) {
			n = clusterNumber; // A new cluster id
			++clusterNumber;
		}
	}

	Log() << "\n -> Parsed " << clusterNumber - 1 << " unique clusters for " << clusters.size() << " nodes.";

	// Store the parsed cluster indices in a vector
	std::vector<unsigned int> modules(m_numNodes);
	std::vector<unsigned int> selectedNodes(m_numNodes, 0);
	for (std::map<unsigned int, unsigned int>::const_iterator it(clusters.begin()); it != clusters.end(); ++it) {
		unsigned int nodeIndex = it->first;
		unsigned int moduleIndex = clusterIdToNumber[it->second] - 1; // To zero-based indexing
		++selectedNodes[nodeIndex];
		modules[nodeIndex] = moduleIndex;
	}

	// Put non-selected nodes (if any) in its own module
	unsigned int numNonSelectedNodes = 0;
	for (unsigned int i = 0; i < m_numNodes; ++i) {
		if (selectedNodes[i] == 0) {
			modules[i] = clusterNumber - 1;
			++clusterNumber;
			++numNonSelectedNodes;
		}
	}

	if (numNonSelectedNodes > 0)
		Log() << "\n -> Put the rest " << numNonSelectedNodes << " nodes in their own modules";

	unsigned int numModules = clusterNumber - 1;

	// Create and store the module nodes in a random access array, and add to root
	std::vector<NodeBase*> moduleNodes(numModules, 0);
	for (unsigned int i = 0; i < m_numNodes; ++i) {
		unsigned int clusterIndex = modules[i];
		if (moduleNodes[clusterIndex] == 0)
			moduleNodes[clusterIndex] = m_treeData.nodeFactory().createNode("", 0.0, 0.0);
		// Add all leaf nodes to the modules defined by the parsed cluster indices
		moduleNodes[clusterIndex]->addChild(&m_treeData.getLeafNode(i));
	}

	// Release leaf nodes from root to add the modules inbetween
	m_treeData.root()->releaseChildren();
	for (unsigned int i = 0; i < numModules; ++i)
		m_treeData.root()->addChild(moduleNodes[i]);

	Log() << "\n -> Generated " << numModules << " modules." << std::endl;
}

void MemoryNetworkAdapter::readHumanReadableTree(std::string filename)
{
	std::string line;
	std::string buf;
	SafeInFile input(filename.c_str());
	Log() << "Parsing memory node tree from '" << filename << "'... " << std::flush;

	std::auto_ptr<NodeBase> root(m_treeData.nodeFactory().createNode("tmpRoot", 1.0, 0.0));
	std::vector<double> flowValues(m_numNodes);
	unsigned int indexOffset = m_config.zeroBasedNodeNumbers ? 0 : 1;
	std::string header = "";
	unsigned int lineNr = 0;
	std::istringstream ss;
	unsigned int nodeCount = 0;
	unsigned int maxDepth = 0;
	unsigned int numNodesNotFound = 0; // Ignore nodes that doesn't exist in network

	while(!std::getline(input, line).fail())
	{
		++lineNr;
		if (line.length() == 0)
			continue;
		if (line[0] == '#')
		{
			if (lineNr == 1) {
				header = line; // e.g. '# Codelength = 8.45977 bits.'
			}
			continue;
		}

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
		unsigned int priorId = 0;
		unsigned int nodeId = 0;
		if(!(ss >> priorId >> nodeId))
			throw BadConversionError(io::Str() << "Can't parse memory node index pair from line " << lineNr << " ('" << line << "').");

		priorId -= indexOffset;
		nodeId -= indexOffset;
		StateNode stateNode(priorId, nodeId);
		std::map<StateNode, unsigned int>::iterator memIt = m_memNodeToIndex.find(stateNode);
		if (memIt == m_memNodeToIndex.end()) {
			++numNodesNotFound;
			continue;
		}

		unsigned int originalIndex = memIt->second;

		// Analyze the path and build up the tree
		ss.clear(); // Clear the eofbit from last extraction!
		ss.str(treePath);
		unsigned int childIndex;
		NodeBase* node = root.get();
		unsigned int depth = 0;
		while (ss >> childIndex)
		{
			ss.get(); // Extract the delimiting character also
			if (childIndex == 0)
				throw FileFormatError("There is a '0' in the tree path, lowest allowed integer is 1.");
			--childIndex;
			// Create new node if path doesn't exist
			if (node->childDegree() <= childIndex)
			{
				NodeBase* child = m_treeData.nodeFactory().createNode("", 0.0, 0.0);
				node->addChild(child);
			}
			node = node->lastChild;
			++depth;
		}
		node->name = name;
		node->originalIndex = originalIndex;;
		flowValues[originalIndex] = flow;
		++nodeCount;
		maxDepth = std::max(maxDepth, depth);
	}

	if (maxDepth < 2)
		throw InputDomainError("No modular solution found in file.");

	if (nodeCount < m_numNodes) {
		// Add unassigned nodes to their own modules
		unsigned int numUnassignedNodes = m_numNodes - nodeCount;
		Log() << "\n -> Warning: " << numUnassignedNodes << " unassigned nodes are put in their own modules.";
	}

	if (numNodesNotFound > 0) {
		Log() << "\n -> Warning: " << numNodesNotFound << " memory nodes not found in network.";
	}

	// Re-root loaded tree
	m_treeData.root()->releaseChildren();
	NodeBase::sibling_iterator superModuleIt(root->begin_child());
	while (!superModuleIt.isEnd())
		m_treeData.root()->addChild((superModuleIt++).base());

	root->releaseChildren();

	std::vector<unsigned int> assignedNodes(m_numNodes, 0);

	// Replace externally loaded leaf nodes with network nodes
	for (NodeBase::leaf_module_iterator leafModuleIt(m_treeData.root()); !leafModuleIt.isEnd(); ++leafModuleIt)
	{
		std::vector<NodeBase*> leafNodes(leafModuleIt->childDegree());
		unsigned int childIndex = 0;
		for (NodeBase::sibling_iterator nodeIt(leafModuleIt->begin_child()); !nodeIt.isEnd(); ++nodeIt, ++childIndex)
		{
			leafNodes[childIndex] = &m_treeData.getLeafNode(nodeIt->originalIndex);
			++assignedNodes[nodeIt->originalIndex];
		}
		leafModuleIt->deleteChildren();
		for (childIndex = 0; childIndex < leafNodes.size(); ++childIndex)
			leafModuleIt->addChild(leafNodes[childIndex]);
	}

	// Put unassigned nodes in their own modules
	if (nodeCount < m_numNodes) {
		for (unsigned int i = 0; i < m_numNodes; ++i) {
			if (assignedNodes[i] == 0) {
				NodeBase* module = m_treeData.nodeFactory().createNode("", 0.0, 0.0);
				m_treeData.root()->addChild(module);
				module->addChild(&m_treeData.getLeafNode(i));
			}
		}
	}

	Log() << "done! Found " << maxDepth << " levels." << std::endl;
	if (!header.empty())
		Log(1) << " -> Parsed header: '" << header << "'" << std::endl;
}

void MemoryNetworkAdapter::generateMemoryNodeMap()
{
	m_memNodeToIndex.clear();
	unsigned int nodeIndex = 0;
	for (TreeData::const_leafIterator leafIt(m_treeData.begin_leaf()); leafIt != m_treeData.end_leaf(); ++leafIt, ++nodeIndex)
	{
		StateNode stateNode = (**leafIt).getStateNode();
		m_memNodeToIndex[stateNode] = nodeIndex;
	}
}


#ifdef NS_INFOMAP
} /* namespace infomap */
#endif
