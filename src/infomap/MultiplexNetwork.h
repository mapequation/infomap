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


#ifndef MULTIPLEXNETWORK_H_
#define MULTIPLEXNETWORK_H_

#include "MemNetwork.h"

#include <map>
#include <deque>
#include <string>

class MultiplexNetwork : public MemNetwork
{
public:
	typedef std::map<unsigned int, double> InterLinkMap;

	MultiplexNetwork(const Config& config) :
		MemNetwork(config)
	{}
	virtual ~MultiplexNetwork() {}

	virtual void readInputData(std::string filename = "");

protected:

	void parseMultiplexNetwork(std::string filename);

	void parseMultipleNetworks();

	void adjustForDifferentNumberOfNodes();

	void generateMemoryNetworkWithInterLayerLinksFromData();

	void generateMemoryNetworkWithSimulatedInterLayerLinks();

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

	std::map<M2Node, InterLinkMap> m_interLinks; // {(layer,node)} -> ({linkedLayer} -> {weight})
};

#endif /* MULTIPLEXNETWORK_H_ */
