/*
 * NetworkAdapter.cpp
 *
 *  Created on: 16 okt 2014
 *      Author: Daniel
 */

#include "NetworkAdapter.h"

#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <algorithm>

#include "../io/convert.h"
#include "../io/SafeFile.h"
#include "../utils/FileURI.h"
#include "Node.h"
#include "NodeFactory.h"
#include "treeIterators.h"


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
		std::cout << "Error: \"" << err.what() << "\"\n";
		ok = false;
	}

	return ok;
}

void NetworkAdapter::readClu(std::string filename)
{
	SafeInFile input(filename.c_str());
	std::string line;
	std::cout << "Parsing '" << filename << "'... " << std::flush;

	if (!getline(input, line))
		throw FileFormatError("First line of cluster data file couldn't be read.");

	std::istringstream lineStream(line);
	string tmp;
	if (!(lineStream >> tmp))
		throw FileFormatError("First line of cluster data file couldn't be read.");
	unsigned int numVertices;
	if (!(lineStream >> numVertices))
		throw FileFormatError("First line of cluster data file doesn't match '*Vertices x' where x is the number of vertices.");
	if (numVertices == 0)
		throw FileFormatError("Number of vertices declared in the cluster data file is zero.");

	std::vector<unsigned int> clusters(m_numNodes);
	unsigned int numParsedValues = 0;
	unsigned int maxClusterIndex = 0;

	while(getline(input, line))
	{
		std::istringstream lineStream(line);
		unsigned int clusterIndex;
		if (!(lineStream >> clusterIndex))
			throw FileFormatError(io::Str() << "Couldn't parse cluster data from line " << (numParsedValues+1));

		// Assume data range [1..numNodes]
		if (clusterIndex == 0 || clusterIndex > m_numNodes)
			throw FileFormatError(io::Str() << "Cluster value for node " << (numParsedValues+1) << " (" << clusterIndex << ")" <<
					" is out of the valid range [1..numNodes]");
		--clusterIndex; // Zero-based
		clusters[numParsedValues] = clusterIndex;
		maxClusterIndex = std::max(maxClusterIndex, clusterIndex);
		++numParsedValues;
		if (numParsedValues == m_numNodes)
			break;
	}
	if (numParsedValues != m_numNodes)
		throw FileFormatError(io::Str() << "Could only read cluster data for " << numParsedValues << " nodes, but the given network contains " <<
				m_numNodes << " nodes.");

	unsigned int numModules = maxClusterIndex + 1;
	std::vector<NodeBase*> modules(numModules);

	// Create and store the module nodes in a random access array, and add to root
	for (unsigned int i = 0; i < numModules; ++i)
	{
		NodeBase* module = m_treeData.nodeFactory().createNode("", 0.0, 0.0);
		modules[i] = module;
	}

	// Check that no modules are empty
	std::vector<unsigned int> childDegrees(numModules);
	for (unsigned int i = 0; i < m_numNodes; ++i)
		++childDegrees[clusters[i]];

	for (unsigned int i = 0; i < numModules; ++i)
	{
		if (childDegrees[i] == 0)
			throw InputDomainError(io::Str() << "Module " << (i + 1) << " is empty.");
	}

	// Add all leaf nodes to the modules defined by the parsed cluster indices
	for (unsigned int i = 0; i < m_numNodes; ++i)
	{
		modules[clusters[i]]->addChild(&m_treeData.getLeafNode(i));
	}

	// Release leaf nodes from root to add the modules inbetween
	m_treeData.root()->releaseChildren();
	for (unsigned int i = 0; i < numModules; ++i)
	{
		m_treeData.root()->addChild(modules[i]);
	}

	std::cout << "done! Found " << numModules + 1 << " modules." << std::endl;
}

void NetworkAdapter::readHumanReadableTree(std::string filename)
{
	std::string line;
	std::string buf;
	SafeInFile input(filename.c_str());
	std::cout << "Parsing tree '" << filename << "'... " << std::flush;

	std::auto_ptr<NodeBase> root(m_treeData.nodeFactory().createNode("tmpRoot", 1.0, 0.0));
	std::vector<double> flowValues(m_numNodes);
	bool gotOriginalIndex = true;
	std::string header;
	unsigned int lineNr = 0;
	std::istringstream ss;
	unsigned int nodeCount = 0;
	unsigned int maxDepth = 0;
	while(getline(input, line))
	{
		++lineNr;
		if (line[0] == '#')
		{
			if (lineNr == 1)
				header = line; // e.g. '# Codelength = 8.45977 bits.'
			continue;
		}
		if (nodeCount > m_numNodes)
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
		unsigned int originalIndex = 0;
		gotOriginalIndex = (ss >> originalIndex);

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
	if (nodeCount < m_numNodes)
		throw MisMatchError("There are less nodes in the tree than in the network.");

	if (!gotOriginalIndex)
		throw FileFormatError("Not implemented for old .tree format (missing original index column).");

	if (maxDepth < 2)
		throw InputDomainError("No modular solution found in file.");

	// Re-root loaded tree
	m_treeData.root()->releaseChildren();
	NodeBase::sibling_iterator superModuleIt(root->begin_child());
	while (!superModuleIt.isEnd())
		m_treeData.root()->addChild((superModuleIt++).base());

	root->releaseChildren();

	// Replace externally loaded leaf nodes with network nodes
	for (NodeBase::leaf_module_iterator leafModuleIt(m_treeData.root()); !leafModuleIt.isEnd(); ++leafModuleIt)
	{
		std::vector<NodeBase*> leafNodes(leafModuleIt->childDegree());
		unsigned int childIndex = 0;
		for (NodeBase::sibling_iterator nodeIt(leafModuleIt->begin_child()); !nodeIt.isEnd(); ++nodeIt, ++childIndex)
		{
			leafNodes[childIndex] = &m_treeData.getLeafNode(nodeIt->originalIndex);
		}
		leafModuleIt->deleteChildren();
		for (childIndex = 0; childIndex < leafNodes.size(); ++childIndex)
			leafModuleIt->addChild(leafNodes[childIndex]);
	}

	std::cout << "done! Found " << maxDepth << " levels." << std::endl;
}

