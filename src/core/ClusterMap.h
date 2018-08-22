/*
 * ClusterMap.h
 *
 *  Created on: 8 apr 2015
 *      Author: Daniel
 */

#ifndef SRC_CLUSTERING_CLUSTERING_CLUSTERMAP_H_
#define SRC_CLUSTERING_CLUSTERING_CLUSTERMAP_H_

#include <string>
#include "../io/SafeFile.h"
#include "../io/convert.h"
#include <sstream>

namespace infomap {

class ClusterMap
{
public:
	ClusterMap() {}

	void readClusterData(std::string filename, bool includeFlow = false);

	std::map<unsigned int, unsigned int>& clusterIds() {
		return m_clusterIds;
	}
	std::map<unsigned int, double > & getFlow(){
		return m_flowData;
	}

private:
	std::map<unsigned int, unsigned int> m_clusterIds;
	std::map<unsigned int, double> m_flowData;
};

void ClusterMap::readClusterData(std::string filename, bool includeFlow)
{
	Log() << "Read initial partition from '" << filename << "'... " << std::flush;
	SafeInFile input(filename.c_str());
	std::string line;
	std::istringstream lineStream;
	std::map<unsigned int, unsigned int> clusterData;

	while(!std::getline(input, line).fail())
	{
		if (line.length() == 0 || line[0] == '#' || line[0] == '*')
			continue;

		lineStream.clear();
		lineStream.str(line);

		unsigned int nodeId;
		unsigned int clusterId;
		if (!(lineStream >> nodeId >> clusterId))
			throw FileFormatError(io::Str() << "Couldn't parse node key and cluster id from line '" << line << "'");
		m_clusterIds[nodeId] = clusterId;

		auto flow = 0.0;
		if(includeFlow && lineStream >> flow){
			m_flowData[nodeId] = flow;
		}

	}
}

}

#endif /* SRC_CLUSTERING_CLUSTERING_CLUSTERMAP_H_ */
