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


#ifndef MEMNETWORK_H_
#define MEMNETWORK_H_

#include "Network.h"

#include <map>
#include <vector>
#include <utility>
#include <set>

#include "../io/Config.h"
#include "../utils/types.h"
#include "Node.h"

#ifdef NS_INFOMAP
namespace infomap
{
#endif

using std::map;
using std::pair;

struct Link
{
	Link() : n1(0), n2(0), weight(0.0) {}
	Link(unsigned int n1, unsigned int n2, double weight) : n1(n1), n2(n2), weight(weight) {}

	unsigned int n1;
	unsigned int n2;
	double weight;
};

struct ComplementaryData
{
	typedef EasyMap<unsigned int, double> MapType;

	/*
	 * @params incomplete link
	 */
	ComplementaryData(unsigned int n1, unsigned int n2, double weight) :
		incompleteLink(n1, n2, weight),
		sumWeightExactMatch(0.0),
		sumWeightPartialMatch(0.0),
		sumWeightShiftedMatch(0.0)
	{}

	void addExactMatch(unsigned int missing, double weight) {
		sumWeightExactMatch += weight;
		exactMatch.getOrSet(missing) += weight;
	}
	void addPartialMatch(unsigned int missing, double weight) {
		sumWeightPartialMatch += weight;
		partialMatch.getOrSet(missing) += weight;
	}
	void addShiftedMatch(unsigned int missing, double weight) {
		sumWeightShiftedMatch += weight;
		shiftedMatch.getOrSet(missing) += weight;
	}

	Link incompleteLink; // -1 n2 n3
	MapType exactMatch; // match x n2 n3 -> x n2 n3
	double sumWeightExactMatch;
	MapType partialMatch; // match x n2 y -> x n2 n3
	double sumWeightPartialMatch;
	MapType shiftedMatch; // match x y n2 -> y n2 n3
	double sumWeightShiftedMatch;
};

class MemNetwork: public Network
{
public:
	// typedef map<pair<StateNode, StateNode>, double> StateLinkMap;
	typedef map<StateNode, map<StateNode, double> > StateLinkMap; // Main key is first state-node, sub-key is second state-node
	typedef map<StateNode, unsigned int> StateNodeMap;

	MemNetwork() :
		Network(),
		m_totStateNodeWeight(0.0),
		m_numStateLinksFound(0),
		m_numStateLinks(0),
		m_totStateLinkWeight(0.0),
		m_numAggregatedStateLinks(0),
		m_numMemorySelfLinks(0),
		m_totalMemorySelfLinkWeight(0.0),
		m_numIncompleteStateLinksFound(0),
		m_numIncompleteStateLinks(0),
		m_numAggregatedIncompleteStateLinks(0),
		m_numStateNodesFound(0),
	 	m_maxStateIndex(std::numeric_limits<unsigned int>::min()),
	 	m_minStateIndex(std::numeric_limits<unsigned int>::max())
	{}

	MemNetwork(const Config& config) :
		Network(config),
		m_totStateNodeWeight(0.0),
		m_numStateLinksFound(0),
		m_numStateLinks(0),
		m_totStateLinkWeight(0.0),
		m_numAggregatedStateLinks(0),
		m_numMemorySelfLinks(0),
		m_totalMemorySelfLinkWeight(0.0),
		m_numIncompleteStateLinksFound(0),
		m_numIncompleteStateLinks(0),
		m_numAggregatedIncompleteStateLinks(0),
		m_numStateNodesFound(0),
	 	m_maxStateIndex(std::numeric_limits<unsigned int>::min()),
	 	m_minStateIndex(std::numeric_limits<unsigned int>::max())
	{}

	MemNetwork(const MemNetwork& other) :
		Network(other.m_config),
		m_totStateNodeWeight(other.m_totStateNodeWeight),
		m_numStateLinksFound(other.m_numStateLinksFound),
		m_numStateLinks(other.m_numStateLinks),
		m_totStateLinkWeight(other.m_totStateLinkWeight),
		m_numAggregatedStateLinks(other.m_numAggregatedStateLinks),
		m_numMemorySelfLinks(other.m_numMemorySelfLinks),
		m_totalMemorySelfLinkWeight(other.m_totalMemorySelfLinkWeight),
		m_numIncompleteStateLinksFound(other.m_numIncompleteStateLinksFound),
		m_numIncompleteStateLinks(other.m_numIncompleteStateLinks),
		m_numAggregatedIncompleteStateLinks(other.m_numAggregatedIncompleteStateLinks),
		m_numStateNodesFound(other.m_numStateNodesFound),
	 	m_maxStateIndex(other.m_maxStateIndex),
	 	m_minStateIndex(other.m_minStateIndex)
	{}

	MemNetwork& operator=(const MemNetwork& other)
	{
		Network::operator=(other);
		m_totStateNodeWeight = other.m_totStateNodeWeight;
		m_numStateLinksFound = other.m_numStateLinksFound;
		m_numStateLinks = other.m_numStateLinks;
		m_totStateLinkWeight = other.m_totStateLinkWeight;
		m_numAggregatedStateLinks = other.m_numAggregatedStateLinks;
		m_numMemorySelfLinks = other.m_numMemorySelfLinks;
		m_totalMemorySelfLinkWeight = other.m_totalMemorySelfLinkWeight;
		m_numIncompleteStateLinksFound = other.m_numIncompleteStateLinksFound;
		m_numIncompleteStateLinks = other.m_numIncompleteStateLinks;
		m_numAggregatedIncompleteStateLinks = other.m_numAggregatedIncompleteStateLinks;
		m_numStateNodesFound = other.m_numStateNodesFound;
	 	m_maxStateIndex = other.m_maxStateIndex;
	 	m_minStateIndex = other.m_minStateIndex;
		return *this;
	}

	virtual ~MemNetwork() {}

	virtual void readInputData(std::string filename = "");

	/**
	 * Add a weighted link between two memory nodes.
	 * @return true if a new link was inserted, false if skipped due to cutoff limit or aggregated to existing link
	 */
	bool addStateLink(unsigned int n1PriorState, unsigned int n1, unsigned int n2PriorState, unsigned int n2, double weight);
	bool addStateLink(unsigned int n1PriorState, unsigned int n1, unsigned int n2PriorState, unsigned int n2, double weight, double firstStateNodeWeight, double secondStateNodeWeight);
	bool addStateLink(StateLinkMap::iterator firstStateNode, unsigned int n2PriorState, unsigned int n2, double weight, double firstStateNodeWeight, double secondStateNodeWeight);
	bool addStateLink(const StateNode& s1, const StateNode& s2, double weight);

	void addStateNode(unsigned int priorState, unsigned int nodeIndex, double weight);
	void addStateNode(StateNode& stateNode);

	virtual void finalizeAndCheckNetwork(bool printSummary = true);

	virtual void printParsingResult(bool includeFirstOrderData = false);

	unsigned int numStateNodes() const { return m_stateNodes.size(); }
	unsigned int numPhysicalNodes() const { return m_physNodes.size(); }
	const StateNodeMap& stateNodeMap() const { return m_stateNodeMap; }
	const std::vector<double>& stateNodeWeights() const { return m_stateNodeWeights; }
	double totalStateNodeWeight() const { return m_totStateNodeWeight; }
	const StateLinkMap& stateLinkMap() const { return m_stateLinks; }
	unsigned int numStateLinks() const { return m_numStateLinks; }
	double totalStateLinkWeight() const { return m_totStateLinkWeight; }
	double totalMemorySelfLinkWeight() const { return m_totalMemorySelfLinkWeight; }

	const map<StateNode, double>& stateNodes() const { return m_stateNodes; }

	virtual void printNetworkAsPajek(std::string filename) const;

	virtual void printStateNetwork(std::string filename) const;

	virtual void disposeLinks();

protected:

	void parseTrigram(std::string filename);

	void parseStateNetwork(std::string filename);

	std::string parseStateNodes(std::ifstream& file);

	std::string parseStateLinks(std::ifstream& file);

	/**
	 * Create trigrams from first order data by chaining pair of overlapping links.
	 * Example pair (1,2) and (2,3) will be chained
	 * and (2,1) (2,3) only if undirected
	 *
	 * Example of ordinary network:
	 * n1 n2 w12
	 * n1 n3 w13
	 * n2 n3 w23
	 * n2 n4 w24
	 * n3 n4 w34
	 *
	 * Its corresponding trigram (for directed input):
	 * n1 n2 n3 w23
	 * n1 n2 n4 w24
	 * n1 n3 n4 w34
	 * n2 n3 n4 w34
	 * +first-order links for dangling memory nodes
	 *
	 * For undirected input, the ordinary network
	 * will first be inflated for both directions:
	 * n1 n2 w12
	 * n1 n3 w13
	 * n2 n3 w23
	 * n2 n4 w24
	 * n3 n4 w34
	 * n2 n1 w12
	 * n3 n1 w13
	 * n3 n2 w23
	 * n4 n2 w24
	 * n4 n3 w34
	 *
	 * Its corresponding trigram (for undirected input):
	 * n1 n2 n1 w12
	 * n1 n2 n3 w23
	 * n1 n2 n4 w24
	 * n1 n3 n1 w13
	 * n1 n3 n2 w23
	 * n1 n3 n4 w34
	 * n2 n3 n1 w13
	 * n2 n3 n2 w32
	 * n2 n3 n4 w34
	 * n2 n1 n2 w12
	 * n2 n1 n3 w13
	 * n3 n1 n2 w12
	 * n3 n1 n3 w13
	 * n3 n2 n1 w12
	 * n3 n2 n3 w23
	 * n3 n2 n4 w24
	 * n4 n2 n1 w12
	 * n4 n2 n3 w23
	 * n4 n2 n4 w24
	 * n4 n3 n1 w13
	 * n4 n3 n2 w23
	 * n4 n3 n4 w34
	 */
	void simulateMemoryFromOrdinaryNetwork();

	void simulateMemoryToIncompleteData();

	// Helper methods

	void parseStateNode(const std::string& line, StateNode& stateNode);

	/**
	 * Parse a string of link data.
	 * If no weight data can be extracted, the default value 1.0 will be used.
	 * Note that the first node number can be negative, which means that memory
	 * information is missing.
	 * @throws an error if not both node numbers can be extracted.
	 */
	void parseStateLink(const std::string& line, int& n1, unsigned int& n2, unsigned int& n3, double& weight);
	void parseStateLink(char line[], int& n1, unsigned int& n2, unsigned int& n3, double& weight);

	/**
	 * Insert memory link, indexed on first state-node and aggregated if exist
	 * @note Called by addStateLink
	 * @return true if a new link was inserted, false if aggregated
	 */
	bool insertStateLink(unsigned int n1PriorState, unsigned int n1, unsigned int n2PriorState, unsigned int n2, double weight);
	bool insertStateLink(const StateNode& s1, const StateNode& s2, double weight);
	bool insertStateLink(StateLinkMap::iterator firstStateNode, unsigned int n2PriorState, unsigned int n2, double weight);

	bool addIncompleteStateLink(unsigned int n1, unsigned int n2, double weight);

	unsigned int addMissingPhysicalNodes();

	virtual void initNodeDegrees();

	map<StateNode, double> m_stateNodes;
	StateNodeMap m_stateNodeMap;
	std::vector<double> m_stateNodeWeights; // out weights on memory nodes
	double m_totStateNodeWeight;
	LinkMap m_incompleteStateLinks;
	std::set<unsigned int> m_physNodes;

	unsigned int m_numStateLinksFound;
	unsigned int m_numStateLinks;
	StateLinkMap m_stateLinks; // Raw data from file

	double m_totStateLinkWeight;
	unsigned int m_numAggregatedStateLinks;
	unsigned int m_numMemorySelfLinks;
	double m_totalMemorySelfLinkWeight;

	unsigned int m_numIncompleteStateLinksFound;
	unsigned int m_numIncompleteStateLinks;
	unsigned int m_numAggregatedIncompleteStateLinks;

	unsigned int m_numStateNodesFound;
	// std::deque<StateNode> m_stateNodes;

	// Checkers
	unsigned int m_maxStateIndex;
	unsigned int m_minStateIndex;
};

inline
bool MemNetwork::addStateLink(unsigned int n1PriorState, unsigned int n1, unsigned int n2PriorState, unsigned int n2, double weight)
{
	return addStateLink(n1PriorState, n1, n2PriorState, n2, weight, weight, 0.0);
}

inline
void MemNetwork::addStateNode(unsigned int previousState, unsigned int nodeIndex, double weight)
{
	StateNode stateNode(previousState, nodeIndex, weight);
	addStateNode(stateNode);
}

#ifdef NS_INFOMAP
}
#endif

#endif /* MEMNETWORK_H_ */
