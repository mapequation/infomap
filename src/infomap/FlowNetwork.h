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


#ifndef FLOWNETWORK_H_
#define FLOWNETWORK_H_

#include <string>
#include <map>
#include <vector>
#include <utility>
#include "Network.h"
#include "../io/Config.h"

#ifdef NS_INFOMAP
namespace infomap
{
#endif

class FlowNetwork
{
public:
	struct Link
	{
		Link(unsigned int sourceIndex = 0, unsigned int targetIndex = 0, double weight = 0.0) :
			source(sourceIndex),
			target(targetIndex),
			weight(weight),
			flow(weight)
		{}
		Link(const Link& other) :
			source(other.source),
			target(other.target),
			weight(other.weight),
			flow(other.flow)
		{}
		unsigned int source;
		unsigned int target;
		double weight;
		double flow;
	};

	typedef Network::LinkMap										LinkMap;
	typedef std::vector<Link>										LinkVec;

	FlowNetwork() {}
	virtual ~FlowNetwork() {}

	virtual void calculateFlow(const Network& network, const Config& config);

	const std::vector<double>& getNodeFlow() const { return m_nodeFlow; }
	const std::vector<double>& getNodeTeleportRates() const { return m_nodeTeleportRates; }
	const LinkVec& getFlowLinks() const { return m_flowLinks; }

protected:

	void finalize(const Network& network, const Config& config, bool normalizeNodeFlow = false);

	std::vector<double> m_nodeFlow;
	std::vector<double> m_nodeTeleportRates;
	LinkVec m_flowLinks;

};

#ifdef NS_INFOMAP
}
#endif

#endif /* FLOWNETWORK_H_ */
