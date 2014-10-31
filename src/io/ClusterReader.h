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


#ifndef CLUSTERREADER_H_
#define CLUSTERREADER_H_

#include <string>
#include <vector>

using std::string;

class ClusterReader
{
public:
	ClusterReader(unsigned int numNodes, bool zeroBasedIndexing = false)
	: m_numNodes(numNodes),
	  m_numModules(0),
	  m_indexOffset(zeroBasedIndexing? 0 : 1),
	  m_clusters(numNodes)
	{}

	virtual ~ClusterReader() {}

	void readData(const string filename);

	const std::vector<unsigned int>& clusters() const
	{
		return m_clusters;
	}

	unsigned int numModules() const
	{
		return m_numModules;
	}

private:
	unsigned int m_numNodes;
	unsigned int m_numModules;
	unsigned int m_indexOffset;
	std::vector<unsigned int> m_clusters;
};



#endif /* CLUSTERREADER_H_ */
