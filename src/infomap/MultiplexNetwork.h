/**********************************************************************************

 Infomap software package for multi-level network clustering

 Copyright (c) 2013 Daniel Edler, Martin Rosvall

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


#ifndef MULTIPLEXNETWORK_H_
#define MULTIPLEXNETWORK_H_

#include "MemNetwork.h"

#include <map>
#include <deque>
#include <string>

struct InterLinkKey;

class MultiplexNetwork : public MemNetwork
{
public:

	MultiplexNetwork(const Config& config) :
		MemNetwork(config)
	{}
	virtual ~MultiplexNetwork() {}

	virtual void readInputData();

protected:

	void parseMultiplexNetwork(std::string filename);

	void generateMemoryNetwork();

	// Helper methods

	/**
	 * Parse a string of intra link data for a certain network, "level node node weight".
	 * If no weight data can be extracted, the default value 1.0 will be used.
	 * @throws an error if not enough data can be extracted.
	 */
	void parseIntraLink(const std::string& line, unsigned int& level, unsigned int& n1, unsigned int& n2, double& weight);

	/**
	 * Parse a string of inter link data for a certain node, "node level level weight".
	 * If no weight data can be extracted, the default value 1.0 will be used.
	 * @throws an error if not enough data can be extracted.
	 */
	void parseInterLink(const std::string& line, unsigned int& node, unsigned int& level1, unsigned int& level2, double& weight);

	// Member variables

	std::deque<Network> m_networks;

	std::map<InterLinkKey, double> m_interLinks; // {node level level} -> {weight}
};

struct InterLinkKey
{
	InterLinkKey(unsigned int nodeIndex = 0, unsigned int layer1 = 0, unsigned int layer2 = 0) :
		nodeIndex(nodeIndex), layer1(layer1), layer2(layer2) {}
	InterLinkKey(const InterLinkKey& other) :
		nodeIndex(other.nodeIndex), layer1(other.layer1), layer2(other.layer2) {}
	InterLinkKey& operator=(const InterLinkKey& other) {
		nodeIndex = other.nodeIndex; layer1 = other.layer1; layer2 = other.layer2; return *this;
	}
	bool operator<(InterLinkKey other) const
	{
		return nodeIndex == other.nodeIndex ?
				(layer1 == other.layer1? layer2 < other.layer2 : layer1 < other.layer1) :
				nodeIndex < other.nodeIndex;
	}

	unsigned int nodeIndex;
	unsigned int layer1;
	unsigned int layer2;
};

#endif /* MULTIPLEXNETWORK_H_ */
