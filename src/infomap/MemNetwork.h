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


#ifndef MEMNETWORK_H_
#define MEMNETWORK_H_

#include "Network.h"

#include <map>
#include <utility>

#include "../io/Config.h"
#include "../utils/types.h"
using std::map;
using std::pair;
#include <deque>

struct M2Node
{
	unsigned int phys1;
	unsigned int phys2;
	M2Node() :
		phys1(0), phys2(0)
	{}
	M2Node(unsigned int phys1, unsigned int phys2) :
		phys1(phys1), phys2(phys2)
	{}
	M2Node(const M2Node& other) :
		phys1(other.phys1), phys2(other.phys2)
	{}

	bool operator<(M2Node other) const
	{
		return phys1 == other.phys1 ? phys2 < other.phys2 : phys1 < other.phys1;
	}
	friend std::ostream& operator<<(std::ostream& out, const M2Node& node)
	{
		return out << "(" << node.phys1 << "-" << node.phys2 << ")";
	}
};

class MemNetwork: public Network
{
public:
	typedef map<pair<M2Node, M2Node>, double> M2LinkMap;
	typedef map<M2Node, unsigned int> M2NodeMap;

	MemNetwork(const Config& config) :
		Network(config),
		m_totM2NodeWeight(0.0),
		m_totM2LinkWeight(0.0),
		m_numMemorySelfLinks(0),
		m_totalMemorySelfLinkWeight(0.0)
	{}
	virtual ~MemNetwork() {}

	virtual void readInputData();

	unsigned int numM2Nodes() const { return m_m2Nodes.size(); }
	const M2LinkMap& m2LinkMap() const { return m_m2Links; }
	const M2NodeMap& m2NodeMap() const { return m_m2NodeMap; }
	const std::vector<double>& m2NodeWeights() const { return m_m2NodeWeights; }
	double totalM2NodeWeight() const { return m_totM2NodeWeight; }
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

	map<M2Node, double> m_m2Nodes;
	M2LinkMap m_m2Links; // Raw data from file
	M2NodeMap m_m2NodeMap;
	std::vector<double> m_m2NodeWeights;
	double m_totM2NodeWeight;
	double m_totM2LinkWeight;
	unsigned int m_numMemorySelfLinks;
	double m_totalMemorySelfLinkWeight;

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

#endif /* MEMNETWORK_H_ */
