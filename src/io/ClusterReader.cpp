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
#include "../utils/Logger.h"
#include "convert.h"
#include "SafeFile.h"

#ifdef NS_INFOMAP
namespace infomap
{
#endif

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

#ifdef NS_INFOMAP
}
#endif
