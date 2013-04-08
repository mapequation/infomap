/* ------------------------------------------------------------------------

 Infomap software package for multi-level network clustering

 * Copyright (c) 2013
 * For license and authors, see README.txt or http://www.mapequation.org

------------------------------------------------------------------------ */

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
	unsigned int totalLinkWeight() const { return m_totalLinkWeight; }

private:

	void parsePajekNetwork(std::string filename);
	void parseLinkList(std::string filename);
	void parseSparseLinkList(std::string filename);

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
