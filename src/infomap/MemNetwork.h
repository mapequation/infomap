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
#include <utility>
#include <deque>

#include "../io/Config.h"
#include "../utils/types.h"
using std::map;
using std::pair;

struct M2Node
{
	unsigned int priorState;
	unsigned int physIndex;
	M2Node() :
		priorState(0), physIndex(0)
	{}
	M2Node(unsigned int priorState, unsigned int physIndex) :
		priorState(priorState), physIndex(physIndex)
	{}
	M2Node(const M2Node& other) :
		priorState(other.priorState), physIndex(other.physIndex)
	{}

	bool operator<(M2Node other) const
	{
		return priorState == other.priorState ? physIndex < other.physIndex : priorState < other.priorState;
	}

	bool operator==(M2Node other) const
	{
		return priorState == other.priorState && physIndex == other.physIndex;
	}

	bool operator!=(M2Node other) const
	{
		return priorState != other.priorState || physIndex != other.physIndex;
	}

	friend std::ostream& operator<<(std::ostream& out, const M2Node& node)
	{
		return out << "(" << node.priorState << "-" << node.physIndex << ")";
	}
};

class MemNetwork: public Network
{
public:
	// typedef map<pair<M2Node, M2Node>, double> M2LinkMap;
	typedef map<M2Node, map<M2Node, double> > M2LinkMap; // Main key is first m2-node, sub-key is second m2-node
	typedef map<M2Node, unsigned int> M2NodeMap;

	MemNetwork(const Config& config) :
		Network(config),
		m_totM2NodeWeight(0.0),
		m_numM2LinksFound(0),
		m_numM2Links(0),
		m_totM2LinkWeight(0.0),
		m_numAggregatedM2Links(0),
		m_numMemorySelfLinks(0),
		m_totalMemorySelfLinkWeight(0.0)
	{}
	virtual ~MemNetwork() {}

	virtual void readInputData(std::string filename = "");

	unsigned int numM2Nodes() const { return m_m2Nodes.size(); }
	const M2NodeMap& m2NodeMap() const { return m_m2NodeMap; }
	const std::vector<double>& m2NodeWeights() const { return m_m2NodeWeights; }
	double totalM2NodeWeight() const { return m_totM2NodeWeight; }
	const M2LinkMap& m2LinkMap() const { return m_m2Links; }
	unsigned int numM2Links() const { return m_numM2Links; }
	double totalM2LinkWeight() const { return m_totM2LinkWeight; }
	double totalMemorySelfLinkWeight() const { return m_totalMemorySelfLinkWeight; }

protected:

	void parseTrigram(std::string filename);
	void parseMultiplex(std::string filename);

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

	// Helper methods
	/**
	 * Parse a string of link data.
	 * If no weight data can be extracted, the default value 1.0 will be used.
	 * @throws an error if not both node numbers can be extracted.
	 */
	void parseM2Link(const std::string& line, unsigned int& n1, unsigned int& n2, unsigned int& n3, double& weight);
	void parseM2Link(char line[], unsigned int& n1, unsigned int& n2, unsigned int& n3, double& weight);

	/**
	 * Add a weighted link between two memory nodes.
	 * @return true if a new link was inserted, false if skipped due to cutoff limit or aggregated to existing link
	 */
	bool addM2Link(unsigned int n1PriorState, unsigned int n1, unsigned int n2PriorState, unsigned int n2, double weight);
	bool addM2Link(unsigned int n1PriorState, unsigned int n1, unsigned int n2PriorState, unsigned int n2, double weight, double firstM2NodeWeight, double secondM2NodeWeight);

	void addM2Node(unsigned int priorState, unsigned int nodeIndex, double weight);
	void addM2Node(M2Node m2node, double weight);

	/**
	 * Insert memory link, indexed on first m2-node and aggregated if exist
	 * @note Called by addM2Link
	 * @return true if a new link was inserted, false if aggregated
	 */
	bool insertM2Link(unsigned int n1PriorState, unsigned int n1, unsigned int n2PriorState, unsigned int n2, double weight);
	bool insertM2Link(M2LinkMap::iterator firstM2Node, unsigned int n2PriorState, unsigned int n2, double weight);

	virtual void finalizeAndCheckNetwork();

	virtual void initNodeDegrees();

	virtual void printParsingResult(bool includeFirstOrderData = true);

	map<M2Node, double> m_m2Nodes;
	M2NodeMap m_m2NodeMap;
	std::vector<double> m_m2NodeWeights;
	double m_totM2NodeWeight;

	unsigned int m_numM2LinksFound;
	unsigned int m_numM2Links;
	M2LinkMap m_m2Links; // Raw data from file

	double m_totM2LinkWeight;
	unsigned int m_numAggregatedM2Links;
	unsigned int m_numMemorySelfLinks;
	double m_totalMemorySelfLinkWeight;

};

inline
bool MemNetwork::addM2Link(unsigned int n1PriorState, unsigned int n1, unsigned int n2PriorState, unsigned int n2, double weight)
{
	return addM2Link(n1PriorState, n1, n2PriorState, n2, weight, weight, 0.0);
}

inline
void MemNetwork::addM2Node(unsigned int previousState, unsigned int nodeIndex, double weight)
{
	m_m2Nodes[M2Node(previousState, nodeIndex)] += weight;
	m_totM2NodeWeight += weight;
}

inline
void MemNetwork::addM2Node(M2Node m2Node, double weight)
{
	m_m2Nodes[m2Node] += weight;
	m_totM2NodeWeight += weight;
}

#endif /* MEMNETWORK_H_ */
