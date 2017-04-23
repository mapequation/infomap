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

template<typename Key>
class ClusterMap
{
public:
	ClusterMap() {}

	void readClusterData(std::string filename, bool includeFlow = false);

	std::map<Key, unsigned int>& clusterIds() {
		return m_clusterIds;
	}
	std::map<Key, double > & getFlow(){
		return m_flowData;
	}

private:
	std::map<Key, unsigned int> m_clusterIds;
	std::map<Key, double> m_flowData;
};

template<typename Key>
void ClusterMap<Key>::readClusterData(std::string filename, bool includeFlow)
{
	Log() << "Read initial partition from '" << filename << "'... " << std::flush;
	SafeInFile input(filename.c_str());
	std::string line;
	std::istringstream lineStream;
	std::map<Key, unsigned int> clusterData;

	while(!std::getline(input, line).fail())
	{
		if (line.length() == 0 || line[0] == '#' || line[0] == '*')
			continue;

		lineStream.clear();
		lineStream.str(line);

		Key nodeKey;
		unsigned int clusterId;
		if (!(lineStream >> nodeKey >> clusterId))
			throw FileFormatError(io::Str() << "Couldn't parse node key and cluster id from line '" << line << "'");
		m_clusterIds[nodeKey] = clusterId;

		auto flow = 0.0;
		if(includeFlow && lineStream >> flow){
			m_flowData[nodeKey] = flow;
		}

	}
}

}

#endif /* SRC_CLUSTERING_CLUSTERING_CLUSTERMAP_H_ */
