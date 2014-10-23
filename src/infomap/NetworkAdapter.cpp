/*
 * NetworkAdapter.cpp
 *
 *  Created on: 16 okt 2014
 *      Author: Daniel
 */

#include "NetworkAdapter.h"

#include <deque>
#include <iostream>
#include <sstream>
#include <memory>

#include "../io/convert.h"
#include "../io/SafeFile.h"
#include "../io/TreeDataWriter.h"


void NetworkAdapter::addExternalHierarchy(std::string filename)
{

	readHumanReadableTree(filename);

}


void NetworkAdapter::readHumanReadableTree(std::string filename)
{
	std::string line;
	std::string buf;
	SafeInFile input(filename.c_str());
	std::cout << "Parsing tree '" << filename << "'... " << std::flush;

	std::auto_ptr<NodeBase> root(m_treeData.nodeFactory().createNode("tmpRoot", 1.0, 0.0));
	root->originalIndex = 0; // Use originalIndex as depth in tree for modules
	std::vector<double> flowValues(m_numNodes);
	bool gotOriginalIndex = true;
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
				child->originalIndex = node->originalIndex + 1; // Use originalIndex as depth for modules
			}
			node = node->lastChild;
		}
		node->name = name;
		node->originalIndex = originalIndex;;
		flowValues[originalIndex] = flow;
		++nodeCount;
	}
	if (nodeCount < m_numNodes)
		throw MisMatchError("There are less nodes in the tree than in the network.");

	std::cout << "done!" << std::endl;

	if (!gotOriginalIndex) {
		std::cout << "Not implemented for old .tree format (missing original index data, ignoring tree structure.\n";
		return;
	}

//	std::cout << "------ tree: \n";
//	for (NodeBase::pre_depth_first_iterator it(root.get()); !it.isEnd(); ++it) {
//		std::cout << std::string(it.depth(), ' ') << it->childIndex() << "\n";
//	}

//	std::cout << "------ top modules: \n";
//	for (NodeBase::sibling_iterator it(root->begin_child()); !it.isEnd(); ++it) {
//		std::cout << it->childIndex() << "\n";
//	}

//	std::cout << "re-root:" << std::endl;
	// Re-root loaded tree
	m_treeData.root()->releaseChildren();
	NodeBase::sibling_iterator superModuleIt(root->begin_child());
	while (!superModuleIt.isEnd())
		m_treeData.root()->addChild((superModuleIt++).base());

//	for (NodeBase::sibling_iterator superModuleIt(root->begin_child()); !superModuleIt.isEnd(); ++superModuleIt)
//	{
//		std::cout << superModuleIt->childIndex() << ", ";
//		m_treeData.root()->addChild(superModuleIt.base());
//	}
	root->releaseChildren();

//	TreeDataWriter treeWriter(m_treeData);
//	treeWriter.writeTree(std::cout);

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


//	std::cout << "re-leaf:" << std::endl;
//	treeWriter.writeTree(std::cout);
//	std::cout << "-------" << std::endl;

}

