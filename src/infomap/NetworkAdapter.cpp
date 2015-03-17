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
		std::cerr << "Error: \"" << err.what() << "\"\n";
		ok = false;
	}

	return ok;
}

void NetworkAdapter::readClu(std::string filename)
{
	ClusterReader cluReader(m_numNodes);

	cluReader.readData(filename);
	const std::vector<unsigned int>& clusters = cluReader.clusters();
	unsigned int numModules = cluReader.numModules();

	// Create and store the module nodes in a random access array, and add to root
	std::vector<NodeBase*> modules(numModules);
	for (unsigned int i = 0; i < numModules; ++i)
	{
		NodeBase* module = m_treeData.nodeFactory().createNode("", 0.0, 0.0);
		modules[i] = module;
	}

	// Add all leaf nodes to the modules defined by the parsed cluster indices
	for (unsigned int i = 0; i < m_numNodes; ++i)
		modules[clusters[i]]->addChild(&m_treeData.getLeafNode(i));

	// Release leaf nodes from root to add the modules inbetween
	m_treeData.root()->releaseChildren();
	for (unsigned int i = 0; i < numModules; ++i)
		m_treeData.root()->addChild(modules[i]);

	Log() << "Found " << numModules << " modules." << std::endl;
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
	std::string header;
	unsigned int lineNr = 0;
	std::istringstream ss;
	unsigned int nodeCount = 0;
	unsigned int maxDepth = 0;

	while(!std::getline(input, line).fail())
	{
		++lineNr;
		if (line.length() == 0 || line[0] == '#')
			continue;
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
		gotOriginalIndex = !!(ss >> originalIndex);

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

	Log() << "done! Found " << maxDepth << " levels." << std::endl;
}

#ifdef NS_INFOMAP
}
#endif
