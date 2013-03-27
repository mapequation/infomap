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
	struct Node
	{
		Node(unsigned int lvl, Node* parent) :
			flow(0.0),
			enter(0.0),
			exit(0.0),
			teleportRate(0.0),
			danglingFlow(0.0)
		{}

		double flow;
		double enter;
		double exit;
		double teleportRate;
		double danglingFlow;
		std::string name;
	};

	struct Link
	{
		Link(unsigned int sourceIndex = 0, unsigned int targetIndex = 0, double weight = 0.0) :
			source(sourceIndex),
			target(targetIndex),
			weight(weight),
			flow(weight)
		{}
		Link(const Link& other) :
			source(other.source),
			target(other.target),
			weight(other.weight),
			flow(other.flow)
		{}
		unsigned int source;
		unsigned int target;
		double weight;
		double flow;
	};


public:
	typedef std::map<std::string, int>								NodeMap;
	typedef std::map<std::pair<unsigned int, unsigned int>, double>	LinkMap;
	typedef std::vector<Link>										LinkVec;

	Network(const Config& config)
	:	m_config(config),
	 	m_numNodes(0),
	 	m_sumNodeWeights(0.0),
	 	m_totalLinkWeight(0.0),
	 	m_numSelfLinks(0)
	{}
	virtual ~Network() {}

	void readFromFile(std::string filename);
	void calculateFlow();
	void printNetworkAsPajek(std::string filename);

	unsigned int numNodes() { return m_numNodes; }
	const std::vector<std::string>& nodeNames() { return m_nodeNames; }
	const std::vector<double>& nodeTeleportRates() { return m_nodeTeleportRates; }
	const std::vector<double>& nodeFlow() { return m_nodeFlow; }

	const LinkVec& links() { return m_flowLinks; }

private:

	void parsePajekNetwork(std::string filename);
	void parseLinkList(std::string filename);
	void parseSparseLinkList(std::string filename);

	void zoom();

	const Config& m_config;

	// For the network data
	unsigned int m_numNodes;
	std::vector<std::string> m_nodeNames;
	std::vector<Node*> m_leafNodes;
	std::vector<double> m_nodeTeleportRates;
	double m_sumNodeWeights;
	NodeMap m_nodeMap;
	double m_totalLinkWeight; // On whole network
	LinkMap m_links;
	unsigned int m_numSelfLinks;

	// For the flow calculation
	std::vector<unsigned int> m_nodeOutDegree;
	std::vector<double> m_nodeFlow;
	LinkVec m_flowLinks;
	std::vector<double> m_sumLinkOutWeight; // Per leaf nodes

//	double m_sumDanglingFlow;
//	Node m_tree;
};

#endif /* NETWORK_H_ */
