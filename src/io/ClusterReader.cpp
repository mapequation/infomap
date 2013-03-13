/* ------------------------------------------------------------------------

 Infomap software package for multi-level network clustering

 * Copyright (c) 2013
 * For license and authors, see README.txt or http://www.mapequation.org

------------------------------------------------------------------------ */

#include "ClusterReader.h"
#include "../utils/Logger.h"
#include "convert.h"
#include "SafeFile.h"

ClusterReader::ClusterReader(unsigned int numNodes)
:	m_clusterData(numNodes)
{
}

ClusterReader::~ClusterReader()
{
}

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
	SafeInFile clusterFile(filename.c_str());
	DEBUG_OUT("ClusterReader::readData() from file '" << filename << "'..." << std::endl);

	unsigned int numNodes = m_clusterData.size();
	string line;
	if (!std::getline(clusterFile, line))
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

	std::ostringstream oss;
	unsigned int numParsedValues = 0;
	while(std::getline(clusterFile, line))
	{
	    std::istringstream lineStream(line);
		unsigned int clusterIndex;
		if (!(lineStream >> clusterIndex))
			throw FileFormatError("Couldn't parse cluster data from line " + numParsedValues);

		// Assume data range [1..numNodes]
//		if (clusterIndex == 0 || clusterIndex > m_clusterData.size())
//			throw FileFormatError((oss << "Cluster value for node " << (numParsedValues+1) << " (" << clusterIndex << ")" <<
//					" is out of the valid range [1..numNodes]", oss.str()));
//	    m_clusterData[numParsedValues] = clusterIndex - 1;

		// Assume data range [0..numNodes-1]
		if (clusterIndex >= m_clusterData.size())
			throw FileFormatError((oss << "Cluster value for node " << (numParsedValues+1) << " (" << clusterIndex << ")" <<
					" is out of the valid range [0..numNodes-1]", oss.str()));
	    m_clusterData[numParsedValues] = clusterIndex;

//	    DEBUG_OUT(" parsed value: " << clusterIndex << std::endl);
		++numParsedValues;
		if (numParsedValues == numNodes)
		{
			break;
		}
	}
	if (numParsedValues != numNodes)
	{
		std::stringstream ss;
		ss << "Could only read cluster data for " << numParsedValues << " nodes, but the given network contains " <<
				numNodes << " nodes.";
		throw FileFormatError(ss.str());
	}
	DEBUG_OUT("Successfully parsed " << numNodes << " lines of cluster data." << std::endl);
}
