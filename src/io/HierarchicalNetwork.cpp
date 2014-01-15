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


#include "HierarchicalNetwork.h"

void HierarchicalNetwork::readStreamableTree(const std::string& fileName)
{
	std::cout << "Read streamable tree from file '" << fileName << "'..." << std::endl;
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

	std::cout << "Metadata:\n";
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
		if (node.parentNode != NULL && node.parentIndex + 1 == node.parentNode->children.size())
		{
			unsigned short numEdges = node.deserializeEdges(dataStream, m_directedEdges);
		}
		if (m_numNodesInTree > numNodesInTree)
			throw FileFormatError("Tree overflow");
	}
	std::cout << "Done! Num nodes deserialized: " << m_numNodesInTree << "\n";
}

void HierarchicalNetwork::writeMap(const std::string& fileName)
{

}
