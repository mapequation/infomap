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


#include "ClusterReader.h"
#include "../utils/Log.h"
#include "convert.h"
#include "SafeFile.h"
#include <algorithm>

namespace infomap {

/**
 * Read cluster data
 *
 * Example file for a network with 6 nodes and two clusters:
 * *Vertices 6
 * 1
 * 1
 * 2
 * 1
 * 2
 * 2
 *
 * Example with node mapping
 * # nodeId clusterId
 * 1 1
 * 2 1
 * 4 1
 * 3 2
 * 5 2
 * 6 2
 *
 */
void ClusterReader::readData(const string filename)
{
	SafeInFile input(filename.c_str());
	std::string line;
	std::istringstream lineStream;
	unsigned int numVertices = 0;

	while(!std::getline(input, line).fail())
	{
		if (line.length() == 0 || line[0] == '#')
			continue;

		lineStream.clear();
		lineStream.str(line);

		if (line[0] == '*')
		{
			std::string buf;
			lineStream >> buf;
			if(buf == "*Vertices" || buf == "*vertices") {
				if (!(lineStream >> numVertices))
					throw FileFormatError(io::Str() << "Can't parse an integer after '" << buf <<
							"' as the number of nodes.");
			}
			else {
				throw FileFormatError(io::Str() << "Unrecognized heading '" << line << " in .clu file.");
			}

			if (numVertices == 0)
				throw FileFormatError("Number of vertices declared in the cluster data file is zero.");
			continue;
		}
		else
		{
			parseClusterLine(line);
		}
	}

	unsigned int zeroMinusOne = 0;
	--zeroMinusOne;
	if (m_maxNodeIndex == zeroMinusOne)
		throw InputDomainError(io::Str() << "Integer overflow, be sure to use zero-based node numbering if the node numbers start from zero.");

}

void ClusterReader::readTree(std::string filename, InfoNode& infomapRoot)
{
	// Check infomap root
	unsigned int numLeafNodes = infomapRoot.childDegree();
	for (auto& node : infomapRoot) {
		if (!node.isLeaf())
			throw InternalOrderError("readTree called with already existing modular structure, must be cleared first");
	}


	std::string line;
	std::string buf;
	SafeInFile input(filename.c_str());
	Log() << "Parsing tree '" << filename << "'... " << std::flush;

	InfoNode root(1.0);
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
		unsigned int stateId = 0;
		if (!(ss >> stateId))
			throw BadConversionError(io::Str() << "Can't parse node id line " << lineNr << " ('" << line << "').");


		// Analyze the path and build up the tree
		ss.clear(); // Clear the eofbit from last extraction!
		ss.str(treePath);
		unsigned int childIndex;
		InfoNode* node = &root;
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
				InfoNode* child = new InfoNode();
				node->addChild(child);
			}
			node = node->lastChild;
			++depth;
		}
		// node->name = name;
		node->stateId = stateId;
		// node->data.flow = flow;
		++nodeCount;
		maxDepth = std::max(maxDepth, depth);
	}

	if (maxDepth < 2)
		throw InputDomainError("No modular solution found in file.");

	if (nodeCount < numLeafNodes) {
		// Add unassigned nodes to their own modules
		unsigned int numUnassignedNodes = numLeafNodes - nodeCount;
		Log() << "\n -> Warning: " << numUnassignedNodes << " unassigned nodes are put in their own modules.";
	}

	if (numNodesNotFound > 0)
		Log() << "\n -> Warning: " << numNodesNotFound << " nodes not found in network.";


	Log() << "done! Found " << maxDepth << " levels." << std::endl;
	if (!header.empty())
		Log(1) << " -> Parsed header: '" << header << "'" << std::endl;
}

void ClusterReader::parseClusterLine(std::string line)
{
	std::istringstream lineStream(line);
	// Parse nodeIndex clusterIndex, or only clusterIndex
	unsigned int nodeIndex, clusterIndex;
	if (!(lineStream >> nodeIndex))
		throw FileFormatError(io::Str() << "Couldn't parse integer from line '" << line << "'");
	if (!(lineStream >> clusterIndex)) {
		clusterIndex = nodeIndex; // Only one column => assume natural order for node indices
		nodeIndex = m_numParsedRows + m_indexOffset;
	}

	nodeIndex -= m_indexOffset; // Get zero-based indexing
	m_clusters[nodeIndex] = clusterIndex;
	m_maxNodeIndex = std::max(m_maxNodeIndex, nodeIndex);
	++m_numParsedRows;
}

}
