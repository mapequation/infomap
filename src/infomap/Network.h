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
	typedef std::map<unsigned int, std::map<unsigned int, double> >	LinkMap;

	Network(const Config& config)
	:	m_config(config),
	 	m_numNodes(0),
	 	m_sumNodeWeights(0.0),
	 	m_numLinks(0),
	 	m_totalLinkWeight(0.0),
	 	m_numAggregatedLinks(0),
	 	m_numSelfLinks(0),
	 	m_totalSelfLinkWeight(0)
	{}
	virtual ~Network() {}

	virtual void readInputData();
	void printNetworkAsPajek(std::string filename);

	unsigned int numNodes() const { return m_numNodes; }
	const std::vector<std::string>& nodeNames() const { return m_nodeNames; }
	const std::vector<double>& nodeTeleportRates() const { return m_nodeWeights; }
	double sumNodeWeights() const { return m_sumNodeWeights; }

	const LinkMap& linkMap() const { return m_links; }
	unsigned int numLinks() const { return m_numLinks; }
	double totalLinkWeight() const { return m_totalLinkWeight; }
	double totalSelfLinkWeight() const { return m_totalSelfLinkWeight; }

	void swapNodeNames(std::vector<std::string>& target) { target.swap(m_nodeNames); }

protected:

	void parsePajekNetwork(std::string filename);
	void parseLinkList(std::string filename);
	void parseSparseLinkList(std::string filename);
	void parsePajekNetworkCStyle(std::string filename);
	bool parseEdgeCStyle(char line[], unsigned int& sourceIndex, unsigned int& targetIndex, double& weight);
	void parseLinkListCStyle(std::string filename);

	void zoom();

	// Helper methods
	/**
	 * Add ordinary link, indexed on first node and aggregated if exist
	 * @return true if a new link was inserted, false if aggregated
	 */
	bool addLink(unsigned int n1, unsigned int n2, double weight);

	const Config& m_config;

	NodeMap m_nodeMap;
	unsigned int m_numNodes;
	std::vector<std::string> m_nodeNames;
	std::vector<double> m_nodeWeights;
	double m_sumNodeWeights;

	LinkMap m_links;
	unsigned int m_numLinks;
	double m_totalLinkWeight; // On whole network
	unsigned int m_numAggregatedLinks;
	unsigned int m_numSelfLinks;
	double m_totalSelfLinkWeight; // On whole network

};

#endif /* NETWORK_H_ */
