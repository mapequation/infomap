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

#ifndef SRC_INFOMAP_NETWORKADAPTER_H_
#define SRC_INFOMAP_NETWORKADAPTER_H_

#include <string>

#include "../io/Config.h"
#include "TreeData.h"

#ifdef NS_INFOMAP
namespace infomap
{
#endif

class NetworkAdapter {
public:
	NetworkAdapter(const Config& config, TreeData& treeData) :
		m_config(config),
		m_treeData(treeData),
		m_numNodes(treeData.numLeafNodes()),
	 	m_indexOffset(m_config.zeroBasedNodeNumbers ? 0 : 1)
	{};
	virtual ~NetworkAdapter() {};

	virtual bool readExternalHierarchy(std::string filename);

protected:
	virtual void readClu(std::string filename);
	virtual void readBipartiteClu(std::string filename);
	virtual void readHumanReadableTree(std::string filename);


protected:
	const Config& m_config;
	TreeData& m_treeData;
	unsigned int m_numNodes;
	unsigned int m_indexOffset;
};

#ifdef NS_INFOMAP
}
#endif

#endif /* SRC_INFOMAP_NETWORKADAPTER_H_ */
