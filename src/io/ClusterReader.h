/* ------------------------------------------------------------------------

 Infomap software package for multi-level network clustering

 * Copyright (c) 2013
 * For license and authors, see README.txt or http://www.mapequation.org

------------------------------------------------------------------------ */

#ifndef CLUSTERREADER_H_
#define CLUSTERREADER_H_
#include <vector>
#include <string>
using std::string;

class ClusterReader
{
public:
	ClusterReader(unsigned int numNodes);
	~ClusterReader();

	void readData(const string filename);

	const std::vector<unsigned int>& getClusterData() const
	{
		return m_clusterData;
	}

private:
	std::vector<unsigned int> m_clusterData;
};



#endif /* CLUSTERREADER_H_ */
