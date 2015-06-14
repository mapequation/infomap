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
#include <map>

#ifdef NS_INFOMAP
namespace infomap
{
#endif

using std::string;

class ClusterReader
{
public:
	ClusterReader(bool zeroBasedIndexing = false)
	: m_indexOffset(zeroBasedIndexing? 0 : 1),
	  m_maxNodeIndex(0),
	  m_numParsedRows(0)
	{}

	virtual ~ClusterReader() {}

	virtual void readData(const string filename);

	const std::map<unsigned int, unsigned int>& clusters() const
	{
		return m_clusters;
	}

	unsigned int maxNodeIndex() const
	{
		return m_maxNodeIndex;
	}

	unsigned int numParsedRows() const
	{
		return m_numParsedRows;
	}

protected:
	virtual void parseClusterLine(std::string line);

	unsigned int m_indexOffset;
	unsigned int m_maxNodeIndex;
	unsigned int m_numParsedRows;
	std::map<unsigned int, unsigned int> m_clusters;
};

#ifdef NS_INFOMAP
}
#endif

#endif /* CLUSTERREADER_H_ */
