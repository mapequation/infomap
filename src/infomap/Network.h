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


#ifndef NETWORK_H_
#define NETWORK_H_
#include <string>
#include <map>
#include <vector>
#include <utility>
#include "../io/Config.h"
#include <limits>
#include <sstream>

class Network
{
public:
	typedef std::map<unsigned int, std::map<unsigned int, double> >	LinkMap;

	Network()
	:	m_config(Config()),
	 	m_numNodesFound(0),
	 	m_numNodes(0),
	 	m_sumNodeWeights(0.0),
	 	m_numLinksFound(0),
	 	m_numLinks(0),
	 	m_totalLinkWeight(0.0),
	 	m_numAggregatedLinks(0),
	 	m_numSelfLinks(0),
	 	m_totalSelfLinkWeight(0),
	 	m_maxNodeIndex(std::numeric_limits<unsigned int>::min()),
	 	m_minNodeIndex(std::numeric_limits<unsigned int>::max()),
	 	m_indexOffset(m_config.zeroBasedNodeNumbers ? 0 : 1)
	{}
	Network(const Config& config)
	:	m_config(config),
	 	m_numNodesFound(0),
	 	m_numNodes(0),
	 	m_sumNodeWeights(0.0),
	 	m_numLinksFound(0),
	 	m_numLinks(0),
	 	m_totalLinkWeight(0.0),
	 	m_numAggregatedLinks(0),
	 	m_numSelfLinks(0),
	 	m_totalSelfLinkWeight(0),
	 	m_maxNodeIndex(std::numeric_limits<unsigned int>::min()),
	 	m_minNodeIndex(std::numeric_limits<unsigned int>::max()),
	 	m_indexOffset(m_config.zeroBasedNodeNumbers ? 0 : 1)
	{}
	Network(const Network& other)
	:	m_config(other.m_config),
	 	m_numNodesFound(other.m_numNodesFound),
	 	m_numNodes(other.m_numNodes),
	 	m_sumNodeWeights(other.m_sumNodeWeights),
	 	m_numLinksFound(other.m_numLinksFound),
	 	m_numLinks(other.m_numLinks),
	 	m_totalLinkWeight(other.m_totalLinkWeight),
	 	m_numAggregatedLinks(other.m_numAggregatedLinks),
	 	m_numSelfLinks(other.m_numSelfLinks),
	 	m_totalSelfLinkWeight(other.m_totalSelfLinkWeight),
	 	m_maxNodeIndex(other.m_maxNodeIndex),
	 	m_minNodeIndex(other.m_minNodeIndex),
	 	m_indexOffset(other.m_indexOffset)
	{}
	Network& operator=(const Network& other)
	{
		m_config = other.m_config;
	 	m_numNodesFound = other.m_numNodesFound;
	 	m_numNodes = other.m_numNodes;
	 	m_sumNodeWeights = other.m_sumNodeWeights;
	 	m_numLinksFound = other.m_numLinksFound;
	 	m_numLinks = other.m_numLinks;
	 	m_totalLinkWeight = other.m_totalLinkWeight;
	 	m_numAggregatedLinks = other.m_numAggregatedLinks;
	 	m_numSelfLinks = other.m_numSelfLinks;
	 	m_totalSelfLinkWeight = other.m_totalSelfLinkWeight;
	 	m_maxNodeIndex = other.m_maxNodeIndex;
	 	m_minNodeIndex = other.m_minNodeIndex;
	 	m_indexOffset = other.m_indexOffset;
	 	return *this;
	}

	virtual ~Network() {}

	virtual void readInputData(std::string filename = "");

	/**
	 * Add a weighted link between two nodes.
	 * @return true if a new link was inserted, false if skipped due to cutoff limit or aggregated to existing link
	 */
	bool addLink(unsigned int n1, unsigned int n2, double weight);

	/**
	 * Run after adding links to check for non-feasible values and set the
	 * node count if not specified in the network, and outDegree and sumLinkOutWeight.
	 * @param desiredNumberOfNodes Set the desired number of nodes, or leave at
	 * zero to set it automatically to match the highest node number defined on
	 * the links.
	 */
	void finalizeAndCheckNetwork(unsigned int desiredNumberOfNodes = 0);

	void printParsingResult();

	void printNetworkAsPajek(std::string filename);

	unsigned int numNodes() const { return m_numNodes; }
	const std::vector<std::string>& nodeNames() const { return m_nodeNames; }
	const std::vector<double>& nodeWeights() const { return m_nodeWeights; }
	double sumNodeWeights() const { return m_sumNodeWeights; }
	const std::vector<double>& outDegree() const { return m_outDegree; }
	const std::vector<double>& sumLinkOutWeight() const { return m_sumLinkOutWeight; }

	const LinkMap& linkMap() const { return m_links; }
	unsigned int numLinks() const { return m_numLinks; }
	double totalLinkWeight() const { return m_totalLinkWeight; }
	double totalSelfLinkWeight() const { return m_totalSelfLinkWeight; }

	void swapNodeNames(std::vector<std::string>& target) { target.swap(m_nodeNames); }

protected:

	void parsePajekNetwork(std::string filename);
	void parseLinkList(std::string filename);
	void parseSparseLinkList(std::string filename);
	void parsePajekNetworkWithoutIOStreams(std::string filename);
	void parseLinkListWithoutIOStreams(std::string filename);

	void zoom();

	// Helper methods
	/**
	 * Parse a string of link data.
	 * If no weight data can be extracted, the default value 1.0 will be used.
	 * @throws an error if not both node numbers can be extracted.
	 */
	void parseLink(const std::string& line, unsigned int& n1, unsigned int& n2, double& weight);
	void parseLink(char line[], unsigned int& n1, unsigned int& n2, double& weight);

	/**
	 * Insert ordinary link, indexed on first node and aggregated if exist
	 * @note Called by addLink
	 * @return true if a new link was inserted, false if aggregated
	 */
	bool insertLink(unsigned int n1, unsigned int n2, double weight);

	virtual void initNodeDegrees();


	Config m_config;

	unsigned int m_numNodesFound;
	unsigned int m_numNodes;
	std::vector<std::string> m_nodeNames;
	std::vector<double> m_nodeWeights;
	double m_sumNodeWeights;
	std::vector<double> m_outDegree;
	std::vector<double> m_sumLinkOutWeight;

	LinkMap m_links;
	unsigned int m_numLinksFound;
	unsigned int m_numLinks;
	double m_totalLinkWeight; // On whole network
	unsigned int m_numAggregatedLinks;
	unsigned int m_numSelfLinks;
	double m_totalSelfLinkWeight; // On whole network

	// Checkers
	unsigned int m_maxNodeIndex; // On links
	unsigned int m_minNodeIndex; // On links

	// Helpers
	std::istringstream m_extractor;
	unsigned int m_indexOffset;

};

#endif /* NETWORK_H_ */
