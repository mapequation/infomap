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

	unsigned int numParsedValues = 0;
	unsigned int maxClusterIndex = 0;

	while(getline(input, line))
	{
		std::istringstream lineStream(line);
		unsigned int clusterIndex;
		if (!(lineStream >> clusterIndex))
			throw FileFormatError(io::Str() << "Couldn't parse cluster data from line " << (numParsedValues+1));

		clusterIndex -= m_indexOffset; // Get zero-based indices
		m_clusters[numParsedValues] = clusterIndex;
		maxClusterIndex = std::max(maxClusterIndex, clusterIndex);
		++numParsedValues;
		if (numParsedValues == m_numNodes)
			break;
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

	std::cout << "done! " << std::flush;
}
