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
 * The cluster data file should be a list of cluster indices, where the lowest is 1 and largest the number of nodes.
 *
 * Example file for a network with 6 nodes and two clusters:
 * *Vertices 6
 * 1
 * 1
 * 2
 * 1
 * 2
 * 2
 */
void ClusterReader::readData(const string filename)
{
	Log() << "Parsing '" << filename << "'... " << std::flush;
	SafeInFile input(filename.c_str());
	std::string line;
	std::istringstream lineStream;
	unsigned int numVertices = 0;
	unsigned int numParsedValues = 0;
	unsigned int maxClusterIndex = 0;

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
			unsigned int clusterIndex;
			if (!(lineStream >> clusterIndex))
				throw FileFormatError(io::Str() << "Couldn't parse cluster data from line '" << line << "'");

			clusterIndex -= m_indexOffset; // Get zero-based indices
			m_clusters[numParsedValues] = clusterIndex;
			maxClusterIndex = std::max(maxClusterIndex, clusterIndex);
			++numParsedValues;
			if (numParsedValues == m_numNodes)
				break;
		}
	}

	if (numParsedValues != m_numNodes)
		throw FileFormatError(io::Str() << "Could only read cluster data for " << numParsedValues << " nodes, but the given network contains " <<
				m_numNodes << " nodes.");

	unsigned int zeroMinusOne = 0;
	--zeroMinusOne;
	if (maxClusterIndex == zeroMinusOne)
		throw InputDomainError(io::Str() << "Integer overflow, be sure to use zero-based node numbering if the node numbers start from zero.");
	if (maxClusterIndex >= m_numNodes)
		throw InputDomainError(io::Str() << "Max module number (" << maxClusterIndex + m_indexOffset <<
				") is larger than the number of nodes");

	m_numModules = maxClusterIndex + 1;

	// Calculate the number of nodes per module to check for compactness and non-emptyness
	std::vector<unsigned int> moduleSize(m_numModules);
	for (unsigned int i = 0; i < m_numNodes; ++i)
	{
		++moduleSize[m_clusters[i]];
	}

	for (unsigned int i = 0; i < m_numModules; ++i)
	{
		if (moduleSize[i] == 0)
			throw InputDomainError(io::Str() << "Module " << (i + 1) << " is empty.");
	}

	Log() << "done! " << std::flush;
}

#ifdef NS_INFOMAP
}
#endif
