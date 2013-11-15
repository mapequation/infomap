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


#ifndef NETWORK_H_
#define NETWORK_H_
#include <string>
#include <map>
#include <vector>
#include <utility>
#include "../io/Config.h"

class Network
{
public:
	typedef std::map<std::string, int>								NodeMap;
	typedef std::map<std::pair<unsigned int, unsigned int>, double>	LinkMap;

	Network(const Config& config)
	:	m_config(config),
	 	m_numNodes(0),
	 	m_sumNodeWeights(0.0),
	 	m_totalLinkWeight(0.0),
	 	m_numSelfLinks(0)
	{}
	virtual ~Network() {}

	void readFromFile(std::string filename);
	void printNetworkAsPajek(std::string filename);

	unsigned int numNodes() const { return m_numNodes; }
	const std::vector<std::string>& nodeNames() const { return m_nodeNames; }
	const std::vector<double>& nodeTeleportRates() const { return m_nodeWeights; }
	double sumNodeWeights() const { return m_sumNodeWeights; }

	const LinkMap& linkMap() const { return m_links; }
	double totalLinkWeight() const { return m_totalLinkWeight; }

private:

	void parsePajekNetwork(std::string filename);
	void parseLinkList(std::string filename);
	void parseSparseLinkList(std::string filename);
	void parsePajekNetworkCStyle(std::string filename);
	bool parseEdgeCStyle(char line[], unsigned int& sourceIndex, unsigned int& targetIndex, double& weight);
	void parseLinkListCStyle(std::string filename);

	void zoom();

	const Config& m_config;

	unsigned int m_numNodes;
	std::vector<std::string> m_nodeNames;
	std::vector<double> m_nodeWeights;
	double m_sumNodeWeights;
	NodeMap m_nodeMap;
	double m_totalLinkWeight; // On whole network
	LinkMap m_links;
	unsigned int m_numSelfLinks;

};

#endif /* NETWORK_H_ */
