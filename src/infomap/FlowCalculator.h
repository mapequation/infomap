/* ------------------------------------------------------------------------

 Infomap software package for multi-level network clustering

 * Copyright (c) 2013
 * For license and authors, see README.txt or http://www.mapequation.org

------------------------------------------------------------------------ */

#ifndef FLOWCALCULATOR_H_
#define FLOWCALCULATOR_H_
#include "Network.h"


class FlowCalculator
{
public:
	FlowCalculator();
	virtual ~FlowCalculator();

	void calculateFlow(Network& network);

private:

//	double m_sumDanglingFlow;
//	std::vector<double> m_nodeFlow;
//	std::vector<double> m_sumLinkOutWeight; // Per leaf nodes
//	std::vector<unsigned int> m_nodeOutDegree;
//	LinkVec m_flowLinks;
//	Node m_tree;

//	NodeMap m_nodeMap;
//	LinkMap m_links;
//	double m_sumNodeWeights;
//	double m_sumDanglingFlow;
//	std::vector<double> m_nodeTeleportRates;
//	std::vector<double> m_nodeFlow;
//	std::vector<double> m_sumLinkOutWeight; // Per leaf nodes
//	std::vector<unsigned int> m_nodeOutDegree;
//	double m_totalLinkWeight; // On whole network
//	LinkVec m_flowLinks;
//	Node m_tree;
//	std::vector<Node*> m_leafNodes;



//	Flags m_flags;
};

#endif /* FLOWCALCULATOR_H_ */
