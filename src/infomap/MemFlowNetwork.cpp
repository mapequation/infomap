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
	std::cout << "Calculating global flow... " << std::flush;
	const MemNetwork& network = static_cast<const MemNetwork&>(net);

	// Prepare data in sequence containers for fast access of individual elements
	unsigned int numM2Nodes = network.numM2Nodes();
//	const std::vector<double>& nodeOutDegree = network.outDegree();
//	const std::vector<double>& sumLinkOutWeight = network.sumLinkOutWeight();
	std::vector<double> nodeOutDegree(network.outDegree());
	std::vector<double> sumLinkOutWeight(network.sumLinkOutWeight());
	m_nodeFlow.assign(numM2Nodes, 0.0);
	m_nodeTeleportRates.assign(numM2Nodes, 0.0);

	const MemNetwork::M2LinkMap& linkMap = network.m2LinkMap();
	unsigned int numLinks = network.numM2Links();
	m_flowLinks.resize(numLinks);
	double totalM2LinkWeight = network.totalM2LinkWeight();
	double sumUndirLinkWeight = 2 * totalM2LinkWeight - network.totalMemorySelfLinkWeight();
	unsigned int linkIndex = 0;
	const MemNetwork::M2NodeMap& nodeMap = network.m2NodeMap();

	for (MemNetwork::M2LinkMap::const_iterator linkIt(linkMap.begin()); linkIt != linkMap.end(); ++linkIt)
	{
		const M2Node& m2source = linkIt->first;
		const std::map<M2Node, double>& subLinks = linkIt->second;
		for (std::map<M2Node, double>::const_iterator subIt(subLinks.begin()); subIt != subLinks.end(); ++subIt, ++linkIndex)
		{
			const M2Node& m2target = subIt->first;
			double linkWeight = subIt->second;

			// Get the indices for the m2 nodes
			MemNetwork::M2NodeMap::const_iterator nodeMapIt = nodeMap.find(m2source);
			if (nodeMapIt == nodeMap.end())
				throw InputDomainError(io::Str() << "Couldn't find mapped index for source M2 node " << m2source);
			unsigned int sourceIndex = nodeMapIt->second;
			nodeMapIt = nodeMap.find(m2target);
			if (nodeMapIt == nodeMap.end())
				throw InputDomainError(io::Str() << "Couldn't find mapped index for target M2 node " << m2target);
			unsigned int targetIndex = nodeMapIt->second;

			m_nodeFlow[sourceIndex] += linkWeight;// / sumUndirLinkWeight;
			m_flowLinks[linkIndex] = Link(sourceIndex, targetIndex, linkWeight);

			if (sourceIndex != targetIndex && !config.outdirdir)
				m_nodeFlow[targetIndex] += linkWeight;// / sumUndirLinkWeight;

		}
	}

	m_m2nodes.resize(numM2Nodes);
	for (MemNetwork::M2NodeMap::const_iterator m2nodeIt(nodeMap.begin()); m2nodeIt != nodeMap.end(); ++m2nodeIt)
	{
		m_m2nodes[m2nodeIt->second] = m2nodeIt->first;
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
				throw InputDomainError(io::Str() << "Memory node (" << linkEnd1 << ", " << linkEnd2 << ") not indexed!");
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
		if (nodeOutDegree[i] == 0)
		{
			++numDanglingM2Nodes;
			// We are in physIndex, lookup all mem nodes in that physical node
			const PhysToMemWeightMap& physToMem = netPhysToMem[m_m2nodes[i].physIndex];
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
						++nodeOutDegree[from];
						sumLinkOutWeight[from] += linkWeight;
						sumExtraLinkWeight += linkWeight;
						m_nodeFlow[from] += linkWeight;
						if (config.isUndirected()) {
							++nodeOutDegree[to];
							sumLinkOutWeight[to] += linkWeight;
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
	if (m_flowLinks.size() - numLinks != 0)
		std::cout << "(added " << (m_flowLinks.size() - numLinks) << " links to " <<
			numDanglingM2Nodes << " dangling memory nodes -> " << m_flowLinks.size() << " links) " << std::flush;

	totalM2LinkWeight += sumExtraLinkWeight;
	sumUndirLinkWeight = 2 * totalM2LinkWeight - network.totalMemorySelfLinkWeight();
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
			link.flow /= totalM2LinkWeight;
			m_nodeFlow[link.target] += link.flow;
		}
		//Normalize node flow
		double sumNodeRank = 0.0;
		for (unsigned int i = 0; i < numM2Nodes; ++i)
			sumNodeRank += m_nodeFlow[i];
		for (unsigned int i = 0; i < numM2Nodes; ++i)
			m_nodeFlow[i] /= sumNodeRank;
		std::cout << "using directed links with raw flow... done!" << std::endl;
		std::cout << "Total link weight: " << totalM2LinkWeight << "\n";
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
				m_nodeFlow[link.target] += nodeFlowSteadyState[link.source] * link.flow / sumLinkOutWeight[link.source];
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
				link.flow *= nodeFlowSteadyState[link.source] / sumLinkOutWeight[link.source] / sumNodeRank;
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

	std::cout << "using " << (config.recordedTeleportation ? "recorded" : "unrecorded") << " teleportation to memory " <<
				(config.teleportToNodes ? "nodes" : "links") << "... " << std::flush;
	if (config.originallyUndirected)
	{
		if (config.recordedTeleportation || !config.teleportToNodes)
			std::cout << "(warning: should be unrecorded teleportation to nodes to correspond to undirected flow on physical network) " << std::flush;
		else
			std::cout << "(corresponding to undirected flow on physical network) " << std::flush;
	}


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
			m_nodeTeleportRates[toNode] += linkIt->flow / totalM2LinkWeight;
		}
	}

	// Normalize link weights with respect to its source nodes total out-link weight;
	for (LinkVec::iterator linkIt(m_flowLinks.begin()); linkIt != m_flowLinks.end(); ++linkIt)
	{
		linkIt->flow /= sumLinkOutWeight[linkIt->source];
	}

	// Collect dangling nodes
	std::vector<unsigned int> danglings;
	for (unsigned int i = 0; i < numM2Nodes; ++i)
	{
		if (nodeOutDegree[i] == 0)
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
