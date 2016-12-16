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

#ifdef NS_INFOMAP
namespace infomap
{
#endif

class MultiplexNetwork : public MemNetwork
{
public:
	typedef std::map<unsigned int, double> InterLinkMap;
	typedef std::map<unsigned int, double> IntraLinkMap;
	typedef std::map<StateNode, std::map<StateNode, double> > MultiplexLinkMap;

	MultiplexNetwork() :
		MemNetwork(),
		m_numIntraLinksFound(0),
		m_numInterLinksFound(0),
		m_numMultiplexLinksFound(0)
	{}
	MultiplexNetwork(const Config& config) :
		MemNetwork(config),
		m_numIntraLinksFound(0),
		m_numInterLinksFound(0),
		m_numMultiplexLinksFound(0)
	{}
	virtual ~MultiplexNetwork() {}

	virtual void readInputData(std::string filename = "");

	virtual void finalizeAndCheckNetwork(bool printSummary = true);

	virtual void addMultiplexLink(int layer1, int node1, int layer2, int node2, double w);

	void addMemoryNetworkFromMultiplexLinks();

protected:

	void parseMultiplexNetwork(std::string filename);

	void parseMultipleNetworks();

	unsigned int adjustForDifferentNumberOfNodes();

	void generateMemoryNetworkWithInterLayerLinksFromData();

	void generateMemoryNetworkWithSimulatedInterLayerLinks();
    
	void generateMemoryNetworkWithJensenShannonSimulatedInterLayerLinks();

	double calculateJensenShannonDivergence(bool &intersect, const IntraLinkMap &layer1OutLinks, double sumOutLinkWeightLayer1, const IntraLinkMap &layer2OutLinks, double sumOutLinkWeightLayer2);
	double calculateJensenShannonDivergence(bool &intersect, const IntraLinkMap &layer1OutLinks, const IntraLinkMap &layer1OppositeOutLinks, double sumOutLinkWeightLayer1, const IntraLinkMap &layer2OutLinks, const IntraLinkMap &layer2OppositeOutLinks, double sumOutLinkWeightLayer2);
	double calculateJensenShannonDivergence(bool &intersect, std::vector<const IntraLinkMap *> &layer1LinksVec, double sumOutLinkWeightLayer1, std::vector<const IntraLinkMap *> &layer2LinksVec, double sumOutLinkWeightLayer2);
	IntraLinkMap::const_iterator *getUndirLinkItPtr(std::vector<pair<IntraLinkMap::const_iterator,IntraLinkMap::const_iterator> > &outLinkItVec);
	bool undirLinkRemains(std::vector<pair<IntraLinkMap::const_iterator,IntraLinkMap::const_iterator> > &outLinkItVec);


	bool createIntraLinksToNeighbouringNodesInTargetLayer(StateLinkMap::iterator stateSourceIt,
	unsigned int nodeIndex, unsigned int targetLayer, const LinkMap& targetLayerLinks,
	double linkWeightNormalizationFactor, double stateNodeWeightNormalizationFactor);
	bool createIntraLinksToNeighbouringNodesInTargetLayer(unsigned int sourceLayer,
	unsigned int nodeIndex, unsigned int targetLayer, const LinkMap& targetLayerLinks,
	double linkWeightNormalizationFactor, double stateNodeWeightNormalizationFactor);

	// Helper methods

	/**
	 * Parse intra-network links (links within a network) until end or new header.
	 * @return The last line parsed, which may be a new header.
	 */
	std::string parseIntraLinks(std::ifstream& file);

	/**
	 * Parse inter-network links (links between networks) until end or new header.
	 * @return The last line parsed, which may be a new header.
	 */
	std::string parseInterLinks(std::ifstream& file);

	/**
	 * Parse general multiplex links until end or new header.
	 * @return The last line parsed, which may be a new header.
	 */
	std::string parseMultiplexLinks(std::ifstream& file);

	/**
	 * Parse a string of intra link data for a certain network, "layer node node [weight]".
	 * If no weight data can be extracted, the default value 1.0 will be used.
	 * @throws an error if not enough data can be extracted.
	 */
	void parseIntraLink(const std::string& line, unsigned int& layer, unsigned int& node1, unsigned int& node2, double& weight);

	/**
	 * Parse a string of inter link data for a certain node, "layer node layer [weight]".
	 * If no weight data can be extracted, the default value 1.0 will be used.
	 * @throws an error if not enough data can be extracted.
	 */
	void parseInterLink(const std::string& line, unsigned int& layer1, unsigned int& node, unsigned int& layer2, double& weight);

	/**
	 * Parse a string of general multiplex link data, "layer1 node1 layer2 node2 weight".
	 * If no weight data can be extracted, the default value 1.0 will be used.
	 * @throws an error if not enough data can be extracted.
	 */
	void parseMultiplexLink(const std::string& line, unsigned int& node, unsigned int& level1, unsigned int& level2, unsigned int& node2, double& weight);

	

	// Member variables

	unsigned int m_numIntraLinksFound;
	std::deque<Network> m_networks;

	unsigned int m_numInterLinksFound;
	std::map<StateNode, InterLinkMap> m_interLinks; // {(layer,node)} -> ({linkedLayer} -> {weight})
	std::map<unsigned int, unsigned int> m_interLinkLayers;

	unsigned int m_numMultiplexLinksFound;
	MultiplexLinkMap m_multiplexLinks; // {(layer,node)} -> ({(layer,node)} -> {weight})
	std::map<unsigned int, unsigned int> m_multiplexLinkLayers;
};

#ifdef NS_INFOMAP
}
#endif

#endif /* MULTIPLEXNETWORK_H_ */
