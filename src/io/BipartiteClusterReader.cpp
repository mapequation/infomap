/*
 * BipartiteClusterReader.cpp
 *
 *  Created on: 25 maj 2015
 *      Author: Daniel
 */

#include "BipartiteClusterReader.h"
#include <sstream>
#include "convert.h"

#ifdef NS_INFOMAP
namespace infomap
{
#endif


/**
 * Read bipartite cluster data, where node id:s are prefixed
 * by 'n' for ordinary nodes and 'f' for feature nodes.
 *
 * # nodeId clusterId
 * n1 1
 * n2 1
 * n4 1
 * f1 1
 * n3 2
 * n5 2
 * n6 2
 * f2 2
 *
 */

void BipartiteClusterReader::parseClusterLine(std::string line)
{
	std::istringstream lineStream(line);

	std::string nodeId;
	unsigned int clusterId;
	if (!(lineStream >> nodeId >> clusterId))
		throw FileFormatError(io::Str() << "Can't parse bipartite cluster data from line '" << line << "'");
	unsigned int nodeIndex;
	if ((nodeId[0] != 'n' && nodeId[0] != 'f') || nodeId.length() == 1 || !(std::istringstream( nodeId.substr(1) ) >> nodeIndex)) {
		throw FileFormatError(io::Str() << "Can't parse bipartite node id (a numerical id prefixed by 'n' or 'f') from line '" << line << "'");
	}

	nodeIndex -= m_indexOffset; // Get zero-based indexing
	if (nodeId[0] == 'n')
		m_clusters[nodeIndex] = clusterId;
	else
		m_featureClusters[nodeIndex] = clusterId;
	m_maxNodeIndex = std::max(m_maxNodeIndex, nodeIndex);
	++m_numParsedRows;
}

#ifdef NS_INFOMAP
} /* namespace infomap */
#endif
