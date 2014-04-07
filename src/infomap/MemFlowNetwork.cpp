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


#include "MemFlowNetwork.h"
#include "MemNetwork.h"
#include <iostream>
#include <cmath>
#include "../io/convert.h"
#include <map>

MemFlowNetwork::MemFlowNetwork() {
	// TODO Auto-generated constructor stub

}

MemFlowNetwork::~MemFlowNetwork() {
	// TODO Auto-generated destructor stub
}


void MemFlowNetwork::calculateFlow(const Network& net, const Config& config)
{
	if (!config.isMemoryNetwork())
	{
		FlowNetwork::calculateFlow(net, config);
		return;
	}
	std::cout << "Calculating global flow... ";
	const MemNetwork& network = static_cast<const MemNetwork&>(net);

//	typedef map<pair<M2Node, M2Node>, double> M2LinkMap;
	std::cout << "on memory network with " << network.m2LinkMap().size() << " memory links... ";

	// Prepare data in sequence containers for fast access of individual elements
	unsigned int numM2Nodes = network.numM2Nodes();
	m_nodeOutDegree.assign(numM2Nodes, 0);
	m_sumLinkOutWeight.assign(numM2Nodes, 0.0);
	m_nodeFlow.assign(numM2Nodes, 0.0);
	m_nodeTeleportRates.assign(numM2Nodes, 0.0);

	const MemNetwork::M2LinkMap& linkMap = network.m2LinkMap();
	unsigned int numLinks = linkMap.size();
	m_flowLinks.resize(numLinks);
	double totalLinkWeight = network.totalM2LinkWeight();
	double sumUndirLinkWeight = 2 * totalLinkWeight - network.totalMemorySelfLinkWeight();
	unsigned int linkIndex = 0;
	const MemNetwork::M2NodeMap& nodeMap = network.m2NodeMap();

	for (MemNetwork::M2LinkMap::const_iterator linkIt(linkMap.begin()); linkIt != linkMap.end(); ++linkIt, ++linkIndex)
	{
		const std::pair<M2Node, M2Node>& linkM2Ends = linkIt->first;
		// Get the indices for the m2 nodes
		MemNetwork::M2NodeMap::const_iterator nodeMapIt = nodeMap.find(linkM2Ends.first);
		if (nodeMapIt == nodeMap.end())
			throw InputDomainError(io::Str() << "Couldn't find mapped index for source M2 node " << linkM2Ends.first);
		unsigned int sourceIndex = nodeMapIt->second;
		nodeMapIt = nodeMap.find(linkM2Ends.second);
		if (nodeMapIt == nodeMap.end())
			throw InputDomainError(io::Str() << "Couldn't find mapped index for target M2 node " << linkM2Ends.second);
		unsigned int targetIndex = nodeMapIt->second;
		++m_nodeOutDegree[sourceIndex];
		double linkWeight = linkIt->second;
		m_sumLinkOutWeight[sourceIndex] += linkWeight;

		if (sourceIndex != targetIndex)
		{
			// Possibly aggregate if no self-link
			if (config.isUndirected()) {
				m_sumLinkOutWeight[targetIndex] += linkWeight;
				++m_nodeOutDegree[targetIndex];
			}
			if (!config.outdirdir)
				m_nodeFlow[targetIndex] += linkWeight;// / sumUndirLinkWeight;
		}

		m_nodeFlow[sourceIndex] += linkWeight;// / sumUndirLinkWeight;
		m_flowLinks[linkIndex] = Link(sourceIndex, targetIndex, linkWeight);
	}

//	double sumNodeFlow = 0.0;
//	for (unsigned int i = 0; i < numM2Nodes; ++i)
//		sumNodeFlow += m_nodeFlow[i];
//
//	std::cout << "\nsumNodeFlow: " << sumNodeFlow << " (" << sumNodeFlow / sumUndirLinkWeight << "), totM2LinkWeight: " << network.totalM2LinkWeight() << ", totalMemorySelfLinkWeight: " << network.totalMemorySelfLinkWeight() << ", sumUndirLinkWeight: " << sumUndirLinkWeight << "\n";


	/**
	 * vector<multimap<double,int> > N1net(network.Nnode); // M1 Network
  ////////////// Create M1 network with transition probabilities from M1 nodes to M2 nodes //////////////////////////////////////
	 * for(map<pair<int,int>,double>::iterator it = network.Links.begin(); it != network.Links.end(); it++)
    N1net[it->first.first].insert(make_pair(it->second,network.M2nodeMap[make_pair(it->first.first,it->first.second)]));

    N1net[m1link.first (= n2), in 0..numM1Nodes] = multimap<linkWeight, M2 node index for M2Node(m1LinkEnds = (n2,n3))>

	 * int Ndangl = 0;
  // Add M1 flow to dangling M2 nodes
  for(int i=0;i<Nnode;i++){
    if(node[i]->outLinks.size() == 0){
      Ndangl++;
	  // for all transitions from n2 to (n2,n3)
      for(multimap<double,int>::iterator it = N1net[node[i]->physicalNodes[0].first].begin(); it != N1net[node[i]->physicalNodes[0].first].end(); it++){

        int from = i;
        int to = it->second;
        double weight = it->first;
        if(weight > 0.0){
          if(from == to){
            NselfLinks++;
          }
          else{
            node[from]->outLinks.push_back(make_pair(to,weight));
            node[to]->inLinks.push_back(make_pair(from,weight));
          }
        }


      }
    }
  }
  cout << "-->Added first order flow to " << Ndangl << " dangling memory nodes." << endl;
	 *
	 */


	m_memIndexToPhys.resize(numM2Nodes);
	for (MemNetwork::M2NodeMap::const_iterator m2nodeIt(nodeMap.begin()); m2nodeIt != nodeMap.end(); ++m2nodeIt)
	{
		m_memIndexToPhys[m2nodeIt->second] = m2nodeIt->first;
	}

	unsigned int numM1Nodes = network.numNodes();
	typedef std::multimap<double, unsigned int> PhysToMemWeightMap;
	std::vector<PhysToMemWeightMap> netPhysToMem(numM1Nodes);

	const LinkMap& m1LinkMap = network.linkMap();

	for (LinkMap::const_iterator linkIt(m1LinkMap.begin()); linkIt != m1LinkMap.end(); ++linkIt)
	{
		unsigned int linkEnd1 = linkIt->first;
		const std::map<unsigned int, double>& subLinks = linkIt->second;
		for (std::map<unsigned int, double>::const_iterator subIt(subLinks.begin()); subIt != subLinks.end(); ++subIt)
		{
			unsigned int linkEnd2 = subIt->first;
			double linkWeight = subIt->second;
			MemNetwork::M2NodeMap::const_iterator m2it = nodeMap.find(M2Node(linkEnd1, linkEnd2));
			if (m2it == nodeMap.end())
				throw InputDomainError("Memory node not indexed!");
			unsigned int m2nodeIndex = m2it->second;
			PhysToMemWeightMap& physMap = netPhysToMem[linkEnd1];
			physMap.insert(std::make_pair(linkWeight, m2nodeIndex));
		}
	}

	// Add M1 flow to dangling M2 nodes
	unsigned int numDanglingM2Nodes = 0;
	unsigned int numSelfLinks = 0;
	double sumExtraLinkWeight = 0.0;
	for (unsigned int i = 0; i < numM2Nodes; ++i)
	{
		if (m_nodeOutDegree[i] == 0)
		{
			++numDanglingM2Nodes;
			// We are in phys2, lookup all mem nodes in that physical node
			const PhysToMemWeightMap& physToMem = netPhysToMem[m_memIndexToPhys[i].phys2];
			for(PhysToMemWeightMap::const_iterator it = physToMem.begin(); it != physToMem.end(); it++)
			{
				unsigned int from = i;
				unsigned int to = it->second;
				double linkWeight = it->first;
				if(linkWeight > 0.0) {
					if(from == to) {
						++numSelfLinks;
					}
					else {
						++m_nodeOutDegree[from];
						m_sumLinkOutWeight[from] += linkWeight;
						sumExtraLinkWeight += linkWeight;
						m_nodeFlow[from] += linkWeight;
						if (config.isUndirected()) {
							++m_nodeOutDegree[to];
							m_sumLinkOutWeight[to] += linkWeight;
						}
						if (!config.outdirdir) {
							m_nodeFlow[to] += linkWeight;
						}
						if (from >= numM2Nodes || to >= numM2Nodes) {
							std::cout << "\nRange error adding dangling links " << from << " " << to << " !!!";
						}
						m_flowLinks.push_back(Link(from, to, linkWeight));
					}
				}
			}

		}
	}
	std::cout << "--> Added " << (m_flowLinks.size() - numLinks) << " links with first order flow to " <<
			numDanglingM2Nodes << " dangling memory nodes -> " << m_flowLinks.size() << " links." << std::endl;

	totalLinkWeight += sumExtraLinkWeight;
	sumUndirLinkWeight = 2 * totalLinkWeight - network.totalMemorySelfLinkWeight();
	numLinks = m_flowLinks.size();


	// Normalize node flow
	for (unsigned int i = 0; i < numM2Nodes; ++i)
		m_nodeFlow[i] /= sumUndirLinkWeight;



	if (config.rawdir)
	{
		// Treat the link weights as flow (after global normalization) and
		// do one power iteration to set the node flow
		m_nodeFlow.assign(numM2Nodes, 0.0);
		for (LinkVec::iterator linkIt(m_flowLinks.begin()); linkIt != m_flowLinks.end(); ++linkIt)
		{
			Link& link = *linkIt;
			link.flow /= totalLinkWeight;
			m_nodeFlow[link.target] += link.flow;
		}
		//Normalize node flow
		double sumNodeRank = 0.0;
		for (unsigned int i = 0; i < numM2Nodes; ++i)
			sumNodeRank += m_nodeFlow[i];
		for (unsigned int i = 0; i < numM2Nodes; ++i)
			m_nodeFlow[i] /= sumNodeRank;
		std::cout << "using directed links with raw flow... done!" << std::endl;
		std::cout << "Total link weight: " << totalLinkWeight << "\n";
		return;
	}

	if (!config.directed)
	{
		if (config.undirdir || config.outdirdir)
		{
			//Take one last power iteration
			std::vector<double> nodeFlowSteadyState(m_nodeFlow);
			m_nodeFlow.assign(numM2Nodes, 0.0);
			for (LinkVec::iterator linkIt(m_flowLinks.begin()); linkIt != m_flowLinks.end(); ++linkIt)
			{
				Link& link = *linkIt;
				m_nodeFlow[link.target] += nodeFlowSteadyState[link.source] * link.flow / m_sumLinkOutWeight[link.source];
			}
			//Normalize node flow
			double sumNodeRank = 0.0;
			for (unsigned int i = 0; i < numM2Nodes; ++i)
				sumNodeRank += m_nodeFlow[i];
			for (unsigned int i = 0; i < numM2Nodes; ++i)
				m_nodeFlow[i] /= sumNodeRank;
			// Update link data to represent flow instead of weight
			for (LinkVec::iterator linkIt(m_flowLinks.begin()); linkIt != m_flowLinks.end(); ++linkIt)
			{
				Link& link = *linkIt;
				link.flow *= nodeFlowSteadyState[link.source] / m_sumLinkOutWeight[link.source] / sumNodeRank;
			}
		}
		else // undirected
		{
			for (unsigned int i = 0; i < numLinks; ++i)
				m_flowLinks[i].flow /= sumUndirLinkWeight;
		}

		if (config.outdirdir)
			std::cout << "counting only ingoing links... done!" << std::endl;
		else
			std::cout << "using undirected links" << (config.undirdir? ", switching to directed after steady state... done!" :
					"... done!") << std::endl;
		return;
	}

	std::cout << "using " << (config.recordedTeleportation ? "recorded" : "unrecorded") << " teleportation to " <<
			(config.teleportToNodes ? "nodes" : "links") << "... " << std::flush;


	// Calculate the teleport rate distribution
	if (config.teleportToNodes)
	{
		const std::vector<double>& nodeWeights = network.m2NodeWeights();
		for (unsigned int i = 0; i < numM2Nodes; ++i)
			m_nodeTeleportRates[i] = nodeWeights[i] / network.totalM2NodeWeight();
	}
	else // Teleport to links
	{
		// Teleport proportionally to out-degree, or in-degree if recorded teleportation.
		for (LinkVec::iterator linkIt(m_flowLinks.begin()); linkIt != m_flowLinks.end(); ++linkIt)
		{
			unsigned int toNode = config.recordedTeleportation ? linkIt->target : linkIt->source;
			m_nodeTeleportRates[toNode] += linkIt->flow / totalLinkWeight;
		}
	}


	// Normalize link weights with respect to its source nodes total out-link weight;
	for (LinkVec::iterator linkIt(m_flowLinks.begin()); linkIt != m_flowLinks.end(); ++linkIt)
	{
		linkIt->flow /= m_sumLinkOutWeight[linkIt->source];
	}

	// Collect dangling nodes
	std::vector<unsigned int> danglings;
	for (unsigned int i = 0; i < numM2Nodes; ++i)
	{
		if (m_nodeOutDegree[i] == 0)
			danglings.push_back(i);
	}

	// Calculate PageRank
	std::vector<double> nodeFlowTmp(numM2Nodes, 0.0);
	unsigned int numIterations = 0;
	double alpha = config.teleportationProbability;
	double beta = 1.0 - alpha;
	double sqdiff = 1.0;
	double danglingRank = 0.0;
	do
	{
		// Calculate dangling rank
		danglingRank = 0.0;
		for (std::vector<unsigned int>::iterator danglingIt(danglings.begin()); danglingIt != danglings.end(); ++danglingIt)
		{
			danglingRank += m_nodeFlow[*danglingIt];
		}

		// Flow from teleportation
		for (unsigned int i = 0; i < numM2Nodes; ++i)
		{
			nodeFlowTmp[i] = (alpha + beta*danglingRank) * m_nodeTeleportRates[i];
		}

		// Flow from links
		for (LinkVec::iterator linkIt(m_flowLinks.begin()); linkIt != m_flowLinks.end(); ++linkIt)
		{
			Link& link = *linkIt;
			nodeFlowTmp[link.target] += beta * link.flow * m_nodeFlow[link.source];
		}

		// Update node flow from the power iteration above and check if converged
		double sum = 0.0;
		double sqdiff_old = sqdiff;
		sqdiff = 0.0;
		for (unsigned int i = 0; i < numM2Nodes; ++i)
		{
			sum += nodeFlowTmp[i];
			sqdiff += std::abs(nodeFlowTmp[i] - m_nodeFlow[i]);
			m_nodeFlow[i] = nodeFlowTmp[i];
		}

		// Normalize if needed
		if (std::abs(sum - 1.0) > 1.0e-10)
		{
			std::cout << "(Normalizing ranks after " <<	numIterations << " power iterations with error " << (sum-1.0) << ") ";
			for (unsigned int i = 0; i < numM2Nodes; ++i)
			{
				m_nodeFlow[i] /= sum;
			}
			std::cerr << "DEBUG BREAK!!" << std::endl;
			break;
		}

		// Perturb the system if equilibrium
		if(sqdiff == sqdiff_old)
		{
			alpha += 1.0e-10;
			beta = 1.0 - alpha;
		}

		numIterations++;
	}  while((numIterations < 200) && (sqdiff > 1.0e-15 || numIterations < 50));

	double sumNodeRank = 1.0;

	if (!config.recordedTeleportation)
	{
		//Take one last power iteration excluding the teleportation (and normalize node flow to sum 1.0)
		sumNodeRank = 1.0 - danglingRank;
		std::cout << "\nDangling rank: " << danglingRank << " -> sumNodeRank: " << sumNodeRank;
		m_nodeFlow.assign(numM2Nodes, 0.0);
		for (LinkVec::iterator linkIt(m_flowLinks.begin()); linkIt != m_flowLinks.end(); ++linkIt)
		{
			Link& link = *linkIt;
			m_nodeFlow[link.target] += link.flow * nodeFlowTmp[link.source] / sumNodeRank;
		}
		beta = 1.0;
	}


	// Update the links with their global flow from the PageRank values. (Note: beta is set to 1 if unrec)
	for (LinkVec::iterator linkIt(m_flowLinks.begin()); linkIt != m_flowLinks.end(); ++linkIt)
	{
		linkIt->flow *= beta * nodeFlowTmp[linkIt->source] / sumNodeRank;
	}

	std::cout << "done in " << numIterations << " iterations!" << std::endl;
}
