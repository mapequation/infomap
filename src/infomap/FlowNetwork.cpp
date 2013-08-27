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


#include "FlowNetwork.h"
#include <iostream>
#include <cmath>

FlowNetwork::FlowNetwork()
{
}

FlowNetwork::~FlowNetwork()
{
}

void FlowNetwork::calculateFlow(const Network& network, const Config& config)
{
	std::cout << "Calculating global flow... ";

	// Prepare data in sequence containers for fast access of individual elements
	unsigned int numNodes = network.numNodes();
	m_nodeOutDegree.assign(numNodes, 0);
	m_sumLinkOutWeight.assign(numNodes, 0.0);
	m_nodeFlow.assign(numNodes, 0.0);
	m_nodeTeleportRates.assign(numNodes, 0.0);

	const LinkMap& linkMap = network.linkMap();
	unsigned int numLinks = linkMap.size();
	m_flowLinks.resize(numLinks);
	double totalLinkWeight = network.totalLinkWeight();
	double sumUndirLinkWeight = (config.isUndirected() ? 1 : 2) * totalLinkWeight;
	unsigned int linkIndex = 0;

	for (LinkMap::const_iterator linkIt(linkMap.begin()); linkIt != linkMap.end(); ++linkIt, ++linkIndex)
	{
		const std::pair<unsigned int, unsigned int>& linkEnds = linkIt->first;
		++m_nodeOutDegree[linkEnds.first];
		double linkWeight = linkIt->second;
		m_sumLinkOutWeight[linkEnds.first] += linkWeight;
		if (config.isUndirected())
			m_sumLinkOutWeight[linkEnds.second] += linkWeight;
		m_nodeFlow[linkEnds.first] += linkWeight / sumUndirLinkWeight;
		if (!config.outdirdir)
			m_nodeFlow[linkEnds.second] += linkWeight / sumUndirLinkWeight;
		m_flowLinks[linkIndex] = Link(linkEnds.first, linkEnds.second, linkWeight);
	}

	if (config.rawdir)
	{
		// Treat the link weights as flow (after global normalization) and
		// do one power iteration to set the node flow
		m_nodeFlow.assign(numNodes, 0.0);
		for (LinkVec::iterator linkIt(m_flowLinks.begin()); linkIt != m_flowLinks.end(); ++linkIt)
		{
			Link& link = *linkIt;
			link.flow /= totalLinkWeight;
			m_nodeFlow[link.target] += link.flow;
		}
		//Normalize node flow
		double sumNodeRank = 0.0;
		for (unsigned int i = 0; i < numNodes; ++i)
			sumNodeRank += m_nodeFlow[i];
		for (unsigned int i = 0; i < numNodes; ++i)
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
			m_nodeFlow.assign(numNodes, 0.0);
			for (LinkVec::iterator linkIt(m_flowLinks.begin()); linkIt != m_flowLinks.end(); ++linkIt)
			{
				Link& link = *linkIt;
				m_nodeFlow[link.target] += nodeFlowSteadyState[link.source] * link.flow / m_sumLinkOutWeight[link.source];
			}
			//Normalize node flow
			double sumNodeRank = 0.0;
			for (unsigned int i = 0; i < numNodes; ++i)
				sumNodeRank += m_nodeFlow[i];
			for (unsigned int i = 0; i < numNodes; ++i)
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
		const std::vector<double>& nodeWeights = network.nodeTeleportRates();
		for (unsigned int i = 0; i < numNodes; ++i)
			m_nodeTeleportRates[i] = nodeWeights[i] / network.sumNodeWeights();
	}
	else // Teleport to nodes
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
	for (unsigned int i = 0; i < numNodes; ++i)
	{
		if (m_nodeOutDegree[i] == 0)
			danglings.push_back(i);
	}

	// Calculate PageRank
	std::vector<double> nodeFlowTmp(numNodes, 0.0);
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
		for (unsigned int i = 0; i < numNodes; ++i)
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
		for (unsigned int i = 0; i < numNodes; ++i)
		{
			sum += nodeFlowTmp[i];
			sqdiff += std::abs(nodeFlowTmp[i] - m_nodeFlow[i]);
			m_nodeFlow[i] = nodeFlowTmp[i];
		}

		// Normalize if needed
		if (std::abs(sum - 1.0) > 1.0e-10)
		{
			std::cout << "(Normalizing ranks after " <<	numIterations << " power iterations with error " << (sum-1.0) << ") ";
			for (unsigned int i = 0; i < numNodes; ++i)
			{
				m_nodeFlow[i] /= sum;
			}
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
		m_nodeFlow.assign(numNodes, 0.0);
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
