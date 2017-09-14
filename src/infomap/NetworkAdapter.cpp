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

#include "NetworkAdapter.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "../io/convert.h"
#include "../io/ClusterReader.h"
#include "../io/BipartiteClusterReader.h"
#include "../io/SafeFile.h"
#include "../utils/FileURI.h"
#include "Node.h"
#include "NodeFactory.h"
#include "treeIterators.h"
#include "../utils/Logger.h"

#ifdef NS_INFOMAP
namespace infomap
{
#endif

bool NetworkAdapter::readExternalHierarchy(std::string filename)
{
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

void NetworkAdapter::readClu(std::string filename)
{
	if (m_config.isBipartite()) {
		readBipartiteClu(filename);
		return;
	}

	Log() << "Parsing '" << filename << "'... " << std::flush;
	ClusterReader cluReader(m_config.zeroBasedNodeNumbers);
	cluReader.readData(filename);
	const std::map<unsigned int, unsigned int>& clusters = cluReader.clusters();

	// Handle non-existent nodes with a warning instead
	// if (cluReader.maxNodeIndex() >= m_numNodes)
	// 	throw InputDomainError(io::Str() << "Max node index in cluster file is " << cluReader.maxNodeIndex() <<
	// 			" but there are only " << m_numNodes << " in the network.");

	Log() << "done!";

	if (cluReader.numParsedRows() > clusters.size())
		Log() << "\n -> Warning: " << (cluReader.numParsedRows() - clusters.size()) << " duplicate node indices!";


	// Re-map cluster id:s to zero-based compact indices
	std::map<unsigned int, unsigned int> clusterIdToNumber;
	unsigned int clusterNumber = 1; // Start from 1 and use default int() (0) as indicating not assigned
	for (std::map<unsigned int, unsigned int>::const_iterator it(clusters.begin()); it != clusters.end(); ++it) {
		unsigned int nodeIndex = it->first;
		if (nodeIndex >= m_numNodes) {
			// Skip re-map cluster id:s for non-existent nodes
			continue;
		}
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
	unsigned int numNodesNotFound = 0;
	for (std::map<unsigned int, unsigned int>::const_iterator it(clusters.begin()); it != clusters.end(); ++it) {
		unsigned int nodeIndex = it->first;
		if (nodeIndex >= m_numNodes) {
			++numNodesNotFound;
		}
		else {
			unsigned int moduleIndex = clusterIdToNumber[it->second] - 1; // To zero-based indexing
			++selectedNodes[nodeIndex];
			modules[nodeIndex] = moduleIndex;
		}
	}

	if (numNodesNotFound > 0)
		Log() << "\n -> Warning: " << numNodesNotFound << " nodes not found in network.";

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

void NetworkAdapter::readBipartiteClu(std::string filename)
{
	Log() << "Parsing '" << filename << "'... " << std::flush;
	BipartiteClusterReader cluReader(m_config.zeroBasedNodeNumbers);
	cluReader.readData(filename);
	const std::map<unsigned int, unsigned int>& nClusters = cluReader.clusters();
	const std::map<unsigned int, unsigned int>& fClusters = cluReader.featureClusters();

	unsigned int numOrdinaryNodes = m_config.minBipartiteNodeIndex;
	unsigned int numFeatureNodes = m_numNodes - numOrdinaryNodes;
	if (cluReader.maxNodeIndex() >= m_config.minBipartiteNodeIndex)
		throw InputDomainError(io::Str() << "Max node index in cluster file is " << cluReader.maxNodeIndex() <<
				" but there are only " << numOrdinaryNodes << " ordinary nodes in the network.");

	if (cluReader.maxFeatureNodeIndex() >= numFeatureNodes)
		throw InputDomainError(io::Str() << "Max feature node index in cluster file is " << cluReader.maxFeatureNodeIndex() <<
				" but there are only " << numFeatureNodes << " feature nodes in the network.");

	Log() << "done!";

	unsigned int numMappedNodes = nClusters.size() + fClusters.size();
	if (cluReader.numParsedRows() > numMappedNodes)
		Log() << "\n -> Warning: " << (cluReader.numParsedRows() - numMappedNodes) << " duplicate node indices!";


	// Re-map cluster id:s to zero-based compact indices
	std::map<unsigned int, unsigned int> clusterIdToNumber;
	unsigned int clusterNumber = 1; // Start from 1 and use default int() (0) as indicating not assigned
	for (std::map<unsigned int, unsigned int>::const_iterator it(nClusters.begin()); it != nClusters.end(); ++it) {
		unsigned int clusterId = it->second;
		unsigned int& n = clusterIdToNumber[clusterId];
		if (n == 0) {
			n = clusterNumber; // A new cluster id
			++clusterNumber;
		}
	}
	for (std::map<unsigned int, unsigned int>::const_iterator it(fClusters.begin()); it != fClusters.end(); ++it) {
		unsigned int clusterId = it->second;
		unsigned int& n = clusterIdToNumber[clusterId];
		if (n == 0) {
			n = clusterNumber; // A new cluster id
			++clusterNumber;
		}
	}

	Log() << "\n -> Parsed " << clusterNumber - 1 << " unique clusters for " << numMappedNodes << " nodes.";

	// Store the parsed cluster indices in a vector
	std::vector<unsigned int> modules(m_numNodes);
	std::vector<unsigned int> selectedNodes(m_numNodes, 0);
	for (std::map<unsigned int, unsigned int>::const_iterator it(nClusters.begin()); it != nClusters.end(); ++it) {
		unsigned int nodeIndex = it->first;
		unsigned int moduleIndex = clusterIdToNumber[it->second] - 1; // To zero-based indexing
		++selectedNodes[nodeIndex];
		modules[nodeIndex] = moduleIndex;
	}
	for (std::map<unsigned int, unsigned int>::const_iterator it(fClusters.begin()); it != fClusters.end(); ++it) {
		unsigned int nodeIndex = it->first + m_config.minBipartiteNodeIndex; // Offset for feature nodes
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

void NetworkAdapter::readHumanReadableTree(std::string filename)
{
	std::string line;
	std::string buf;
	SafeInFile input(filename.c_str());
	Log() << "Parsing tree '" << filename << "'... " << std::flush;

	std::auto_ptr<NodeBase> root(m_treeData.nodeFactory().createNode("tmpRoot", 1.0, 0.0));
	std::vector<double> flowValues(m_numNodes);
	bool gotOriginalIndex = true;
	unsigned int indexOffset = m_config.zeroBasedNodeNumbers ? 0 : 1;
	std::string header = "";
	unsigned int lineNr = 0;
	std::istringstream ss;
	unsigned int nodeCount = 0;
	unsigned int maxDepth = 0;
	unsigned int numNodesNotFound = 0;
	std::string section = "";

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
		if (line[0] == '*') {
			// New section, abort tree parsing.
			section = line;
			break;
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
		unsigned int originalIndex = 0;
		gotOriginalIndex = !!(ss >> originalIndex);

		originalIndex -= indexOffset;

		if (originalIndex >= m_numNodes) {
			++numNodesNotFound;
			continue;
		}

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

	if (!gotOriginalIndex)
		throw FileFormatError("Not implemented for old .tree format (missing original index column).");

	if (maxDepth < 2)
		throw InputDomainError("No modular solution found in file.");

	if (nodeCount < m_numNodes) {
		// Add unassigned nodes to their own modules
		unsigned int numUnassignedNodes = m_numNodes - nodeCount;
		Log() << "\n -> Warning: " << numUnassignedNodes << " unassigned nodes are put in their own modules.";
	}

	if (numNodesNotFound > 0)
		Log() << "\n -> Warning: " << numNodesNotFound << " nodes not found in network.";

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

#ifdef NS_INFOMAP
}
#endif
