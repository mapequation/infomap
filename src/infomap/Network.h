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
#include <set>

#ifdef NS_INFOMAP
namespace infomap
{
#endif

struct Bigram;
struct Weight;
struct BipartiteLink;

class Network
{
public:
	typedef std::map<unsigned int, std::map<unsigned int, double> >	LinkMap;

	Network()
	:	m_config(Config()),
	 	m_numNodesFound(0),
	 	m_numNodes(0),
	 	m_sumNodeWeights(0.0),
		m_numDanglingNodes(0),
	 	m_numLinksFound(0),
	 	m_numLinks(0),
	 	m_totalLinkWeight(0.0),
	 	m_numAggregatedLinks(0),
	 	m_numSelfLinks(0),
	 	m_numSelfLinksFound(0),
		m_totalSelfLinkWeight(0),
		m_numLinksIgnoredByWeightThreshold(0),
		m_totalLinkWeightIgnored(0.0),
		m_addSelfLinks(m_config.selfTeleportationProbability > 0 && m_config.selfTeleportationProbability < 1),
		m_numAdditionalLinks(0),
		m_sumAdditionalLinkWeight(0.0),
	 	m_maxNodeIndex(std::numeric_limits<unsigned int>::min()),
	 	m_minNodeIndex(std::numeric_limits<unsigned int>::max()),
	 	m_minFeatureIndex(std::numeric_limits<unsigned int>::max()),
	 	m_bipartiteStartIndex(std::numeric_limits<unsigned int>::max()),
	 	m_indexOffset(m_config.zeroBasedNodeNumbers ? 0 : 1),
		m_numBipartiteNodes(0),
		m_isFinalized(false)
	{}
	Network(const Config& config)
	:	m_config(config),
	 	m_numNodesFound(0),
	 	m_numNodes(0),
	 	m_sumNodeWeights(0.0),
		m_numDanglingNodes(0),
	 	m_numLinksFound(0),
	 	m_numLinks(0),
	 	m_totalLinkWeight(0.0),
	 	m_numAggregatedLinks(0),
	 	m_numSelfLinks(0),
	 	m_numSelfLinksFound(0),
	 	m_totalSelfLinkWeight(0),
		m_numLinksIgnoredByWeightThreshold(0),
		m_totalLinkWeightIgnored(0.0),
		m_addSelfLinks(m_config.selfTeleportationProbability > 0 && m_config.selfTeleportationProbability < 1),
		m_numAdditionalLinks(0),
		m_sumAdditionalLinkWeight(0.0),
	 	m_maxNodeIndex(std::numeric_limits<unsigned int>::min()),
	 	m_minNodeIndex(std::numeric_limits<unsigned int>::max()),
	 	m_minFeatureIndex(std::numeric_limits<unsigned int>::max()),
	 	m_bipartiteStartIndex(std::numeric_limits<unsigned int>::max()),
	 	m_indexOffset(m_config.zeroBasedNodeNumbers ? 0 : 1),
		m_numBipartiteNodes(0),
		m_isFinalized(false)
	{}
	Network(const Network& other)
	:	m_config(other.m_config),
	 	m_numNodesFound(other.m_numNodesFound),
	 	m_numNodes(other.m_numNodes),
	 	m_sumNodeWeights(other.m_sumNodeWeights),
		m_numDanglingNodes(other.m_numDanglingNodes),
	 	m_numLinksFound(other.m_numLinksFound),
	 	m_numLinks(other.m_numLinks),
	 	m_totalLinkWeight(other.m_totalLinkWeight),
	 	m_numAggregatedLinks(other.m_numAggregatedLinks),
	 	m_numSelfLinks(other.m_numSelfLinks),
	 	m_numSelfLinksFound(other.m_numSelfLinksFound),
	 	m_totalSelfLinkWeight(other.m_totalSelfLinkWeight),
		m_numLinksIgnoredByWeightThreshold(0),
		m_totalLinkWeightIgnored(0.0),
		m_addSelfLinks(other.m_addSelfLinks),
		m_numAdditionalLinks(other.m_numAdditionalLinks),
		m_sumAdditionalLinkWeight(other.m_sumAdditionalLinkWeight),
	 	m_maxNodeIndex(other.m_maxNodeIndex),
	 	m_minNodeIndex(other.m_minNodeIndex),
	 	m_minFeatureIndex(other.m_minFeatureIndex),
	 	m_bipartiteStartIndex(other.m_bipartiteStartIndex),
	 	m_indexOffset(other.m_indexOffset),
		m_numBipartiteNodes(other.m_numBipartiteNodes),
		m_isFinalized(other.m_isFinalized)
	{}
	Network& operator=(const Network& other)
	{
		m_config = other.m_config;
	 	m_numNodesFound = other.m_numNodesFound;
	 	m_numNodes = other.m_numNodes;
	 	m_sumNodeWeights = other.m_sumNodeWeights;
		m_numDanglingNodes = other.m_numDanglingNodes;
	 	m_numLinksFound = other.m_numLinksFound;
	 	m_numLinks = other.m_numLinks;
	 	m_totalLinkWeight = other.m_totalLinkWeight;
	 	m_numAggregatedLinks = other.m_numAggregatedLinks;
	 	m_numSelfLinks = other.m_numSelfLinks;
	 	m_numSelfLinksFound = other.m_numSelfLinksFound;
	 	m_totalSelfLinkWeight = other.m_totalSelfLinkWeight;
		m_numLinksIgnoredByWeightThreshold = other.m_numLinksIgnoredByWeightThreshold;
		m_totalLinkWeightIgnored = other.m_totalLinkWeightIgnored;
	 	m_addSelfLinks = other.m_addSelfLinks;
		m_numAdditionalLinks = other.m_numAdditionalLinks;
		m_sumAdditionalLinkWeight = other.m_sumAdditionalLinkWeight;
	 	m_maxNodeIndex = other.m_maxNodeIndex;
	 	m_minNodeIndex = other.m_minNodeIndex;
	 	m_minFeatureIndex = other.m_minFeatureIndex;
	 	m_bipartiteStartIndex = other.m_bipartiteStartIndex;
	 	m_indexOffset = other.m_indexOffset;
	 	m_numBipartiteNodes = other.m_numBipartiteNodes;
		m_isFinalized = other.m_isFinalized;
	 	return *this;
	}

	virtual ~Network() {}

	void setConfig(const Config& config) { m_config = config; }

	virtual void readInputData(std::string filename = "");

	unsigned int addNodes(const std::vector<std::string>& names);

	bool addNode(unsigned int nodeIndex);

	/**
	 * Add a weighted link between two nodes.
	 * @return true if a new link was inserted, false if skipped due to cutoff limit or aggregated to existing link
	 */
	bool addLink(unsigned int n1, unsigned int n2, double weight = 1.0);
	
	bool addBipartiteLink(unsigned int n1, unsigned int n2, double weight = 1.0);

	bool addBipartiteLink(unsigned int featureNode, unsigned int node, bool swapOrder, double weight = 1.0);

	/**
	 * Change this network to a bipartite network
	 * @param bipartiteStartIndex Nodes equal to or above this index are treated as feature nodes
	 */
	void setBipartiteNodesFrom(unsigned int bipartiteStartIndex);

	/**
	 * Run after adding links to check for non-feasible values and set the
	 * node count if not specified in the network, and outDegree and sumLinkOutWeight.
	 * @param desiredNumberOfNodes Set the desired number of nodes, or leave at
	 * zero to set it automatically to match the highest node number defined on
	 * the links.
	 */
	void finalizeAndCheckNetwork(bool printSummary = true, unsigned int desiredNumberOfNodes = 0);

	void printParsingResult(bool onlySummary = false);

	std::string getParsingResultSummary();

	virtual void printNetworkAsPajek(std::string filename) const;

	virtual void printStateNetwork(std::string filename) const;

	unsigned int numNodes() const { return m_numNodes; }
	const std::vector<std::string>& nodeNames() const { return m_nodeNames; }
	const std::vector<double>& nodeWeights() const { return m_nodeWeights; }
	double sumNodeWeights() const { return m_sumNodeWeights; }
	const std::vector<double>& outDegree() const { return m_outDegree; }
	const std::vector<double>& sumLinkOutWeight() const { return m_sumLinkOutWeight; }
	bool haveNode(unsigned int nodeIndex) const { return m_nodes.count(nodeIndex) != 0; }

	const LinkMap& linkMap() const { return m_links; }
	unsigned int numLinks() const { return m_numLinks; }
	double totalLinkWeight() const { return m_totalLinkWeight; }
	double totalSelfLinkWeight() const { return m_totalSelfLinkWeight; }

	bool isBipartite() const { return m_bipartiteStartIndex < std::numeric_limits<unsigned int>::max(); }
	// bool isBipartite() const { return m_numBipartiteNodes > 0; }
	unsigned int numBipartiteNodes() const { return m_numBipartiteNodes; }

	void initNodeNames();
	void swapNodeNames(std::vector<std::string>& target) { target.swap(m_nodeNames); }

	void generateOppositeLinkMap(LinkMap& oppositeLinks);

	virtual void disposeLinks() { m_links.clear(); }

	const Config& config() { return m_config; }

	bool isFinalized() { return m_isFinalized; }
	
protected:

	void parsePajekNetwork(std::string filename);
	void parseLinkList(std::string filename);
	void parseSparseLinkList(std::string filename);
	void parsePajekNetworkWithoutIOStreams(std::string filename);
	void parseLinkListWithoutIOStreams(std::string filename);
	void parseGeneralNetwork(std::string filename);
	void parseBipartiteNetwork(std::string filename);

	void zoom();

	// Helper methods

	std::string parseLinks(std::ifstream& file);

	std::string parseBipartiteLinks(std::ifstream& file);

	/**
	 * Parse a string of link data.
	 * If no weight data can be extracted, the default value 1.0 will be used.
	 * @throws an error if not both node numbers can be extracted.
	 */
	void parseLink(const std::string& line, unsigned int& n1, unsigned int& n2, double& weight);
	void parseLink(char line[], unsigned int& n1, unsigned int& n2, double& weight);

	/**
	 * Parse a bipartite link of format "f1 n1 1.0" for a link between
	 * feature node 1 to ordinary node 1 with weight 1.0.
	 * The order of the feature nodes and ordinary nodes can be swapped.
	 * Store the numberical id (minus possible indexOffset for non-zerobased indexing)
	 * on the referenced uints.
	 * @return true if the input order was swapped
	 */
	bool parseBipartiteLink(const std::string& line, unsigned int& featureNode, unsigned int& node, double& weight);

	/**
	 * Insert ordinary link, indexed on first node and aggregated if exist
	 * @note Called by addLink
	 * @return true if a new link was inserted, false if aggregated
	 */
	bool insertLink(unsigned int n1, unsigned int n2, double weight);

	/**
	* Insert node if not exist
	*/
	bool insertNode(unsigned int nodeIndex);

	virtual void initNodeDegrees();

	/**
	* Read lines from file until it starts with '*'
	* and return that line.
	*/
	std::string skipUntilHeader(std::ifstream& file);

	/**
	 * Parse vertices
	 * @return The line after the vertices
	 */
	std::string parseVertices(std::ifstream& file, bool required = true);

	/**
	 * Parse vertices under the heading
	 * @return The line after the vertices
	 */
	std::string parseVertices(std::ifstream& file, std::string heading, bool required = true);


	Config m_config;

	unsigned int m_numNodesFound;
	unsigned int m_numNodes;
	std::vector<std::string> m_nodeNames;
	std::vector<double> m_nodeWeights;
	double m_sumNodeWeights;
	std::vector<double> m_outDegree;
	std::vector<double> m_sumLinkOutWeight;
	unsigned int m_numDanglingNodes;
	std::set<unsigned int> m_nodes;

	LinkMap m_links;
	unsigned int m_numLinksFound;
	unsigned int m_numLinks;
	double m_totalLinkWeight; // On whole network
	unsigned int m_numAggregatedLinks;
	unsigned int m_numSelfLinks;
	unsigned int m_numSelfLinksFound;
	double m_totalSelfLinkWeight; // On whole network
	unsigned int m_numLinksIgnoredByWeightThreshold;
	double m_totalLinkWeightIgnored;

	// Zooming
	bool m_addSelfLinks;
	unsigned int m_numAdditionalLinks;
	unsigned int m_sumAdditionalLinkWeight;

	// Checkers
	unsigned int m_maxNodeIndex; // On links
	unsigned int m_minNodeIndex; // On links
	unsigned int m_minFeatureIndex; // On bipartite links
	unsigned int m_bipartiteStartIndex;

	// Helpers
	std::istringstream m_extractor;
	unsigned int m_indexOffset;

	// Bipartite
	std::map<BipartiteLink, Weight> m_bipartiteLinks;
	unsigned int m_numBipartiteNodes;

	// Other
	bool m_isFinalized;

};

struct Bigram
{
	unsigned int first, second;
	Bigram(unsigned int first = 0, unsigned int second = 0) : first(first), second(second) {}

	bool operator<(const Bigram other) const
	{
		return first == other.first ? second < other.second : first < other.first;
	}
};

struct BipartiteLink
{
	unsigned int featureNode, node;
	bool swapOrder;
	BipartiteLink(unsigned int featureNode = 0, unsigned int node = 0, bool swapOrder = false)
	: featureNode(featureNode), node(node), swapOrder(swapOrder) {}

	bool operator<(const BipartiteLink other) const
	{
		if (swapOrder != other.swapOrder)
			return other.swapOrder;
		
		return featureNode == other.featureNode ? node < other.node : featureNode < other.featureNode;
	}
};

// Struct to make the weight initialized to zero by default in a map
struct Weight
{
	double weight;
	Weight(double weight = 0) : weight(weight) {}

	Weight& operator+=(double w)
	{
		weight += w;
		return *this;
	}
};

template<typename key_t, typename subkey_t, typename value_t>
class MapMap
{
public:
	typedef std::map<subkey_t, value_t> submap_t;
	typedef std::map<key_t, submap_t> map_t;
	MapMap() :
	m_size(0),
	m_numAggregations(0),
	m_sumValue(0)
	{}
	virtual ~MapMap() {}

	bool insert(key_t key1, subkey_t key2, value_t value)
	{
		++m_size;
		m_sumValue += value;

		// Aggregate link weights if they are definied more than once
		typename map_t::iterator firstIt = m_data.lower_bound(key1);
		if (firstIt != m_data.end() && firstIt->first == key1)
		{
			std::pair<typename submap_t::iterator, bool> ret2 = firstIt->second.insert(std::make_pair(key2, value));
			if (!ret2.second)
			{
				ret2.first->second += value;
				++m_numAggregations;
				--m_size;
				return false;
			}
		}
		else
		{
			m_data.insert(firstIt, std::make_pair(key1, submap_t()))->second.insert(std::make_pair(key2, value));
		}

		return true;
	}

	unsigned int size() { return m_size; }
	unsigned int numAggregations() { return m_numAggregations; }
	value_t sumValue() { return m_sumValue; }


private:
	map_t m_data;
	unsigned int m_size;
	unsigned int m_numAggregations;
	value_t m_sumValue;
};

typedef MapMap<unsigned int, unsigned int, double> LinkMapMap;

template<typename key_t, typename value_t>
class EasyMap : public std::map<key_t, value_t>
{
public:
	typedef std::map<key_t, value_t> map_t;
	typedef EasyMap<key_t, value_t> self_t;
	value_t& getOrSet(const key_t& key, value_t defaultValue = 0)
	{
		typename self_t::iterator it = this->lower_bound(key);
		if (it != this->end() && it->first == key)
			return it->second;
		return this->insert(it, std::make_pair(key, defaultValue))->second;
	}
};

struct Triple
{
	Triple() :
		n1(0), n2(0), n3(0) {}
	Triple(unsigned int value1, unsigned int value2, unsigned int value3) :
		n1(value1), n2(value2), n3(value3) {}
	Triple(const Triple& other) :
		n1(other.n1), n2(other.n2), n3(other.n3) {}
	~Triple() {}

	bool operator<(const Triple& other) const
	{
		return n1 == other.n1 ? (n2 == other.n2 ? n3 < other.n3 : n2 < other.n2) : n1 < other.n1;
	}

	bool operator==(const Triple& other) const
	{
		return n1 == other.n1 && n2 == other.n2 && n3 == other.n3;
	}

	unsigned int n1;
	unsigned int n2;
	unsigned int n3;
};

#ifdef NS_INFOMAP
}
#endif

#endif /* NETWORK_H_ */
