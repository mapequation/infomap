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


#ifndef MEMFLOWNETWORK_H_
#define MEMFLOWNETWORK_H_

#include "FlowNetwork.h"
#include "MemNetwork.h"

#ifdef NS_INFOMAP
namespace infomap
{
#endif

class MemFlowNetwork: public FlowNetwork {
public:
	MemFlowNetwork() {}
	virtual ~MemFlowNetwork() {}

	virtual void calculateFlow(const Network& network, const Config& config);

	const std::vector<StateNode>& getStateNodes() const { return m_statenodes; }

protected:
	std::vector<StateNode> m_statenodes;
};

#ifdef NS_INFOMAP
}
#endif

#endif /* MEMFLOWNETWORK_H_ */
