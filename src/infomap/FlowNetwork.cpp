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


#include "FlowNetwork.h"
#include <iostream>
#include <cmath>
#include "../utils/Logger.h"

#ifdef NS_INFOMAP
namespace infomap
{
#endif

void FlowNetwork::calculateFlow(const Network& network, const Config& config)
{
	Log() << "Calculating global flow... " << std::flush;

	// Prepare data in sequence containers for fast access of individual elements
	unsigned int numNodes = network.numNodes();
	const std::vector<double>& nodeOutDegree = network.outDegree();
	const std::vector<double>& sumLinkOutWeight = network.sumLinkOutWeight();
	m_nodeFlow.assign(numNodes, 0.0);
	m_nodeTeleportRates.assign(numNodes, 0.0);

	const LinkMap& linkMap = network.linkMap();
	unsigned int numLinks = network.numLinks();
	m_flowLinks.resize(numLinks);
	double totalLinkWeight = network.totalLinkWeight();
	double sumUndirLinkWeight = 2 * totalLinkWeight - network.totalSelfLinkWeight();
	unsigned int linkIndex = 0;

	for (LinkMap::const_iterator linkIt(linkMap.begin()); linkIt != linkMap.end(); ++linkIt)
	{
		unsigned int linkEnd1 = linkIt->first;
		const std::map<unsigned int, double>& subLinks = linkIt->second;
		for (std::map<unsigned int, double>::const_iterator subIt(subLinks.begin()); subIt != subLinks.end(); ++subIt, ++linkIndex)
		{
			unsigned int linkEnd2 = subIt->first;
			double linkWeight = subIt->second;

			m_nodeFlow[linkEnd1] += linkWeight / sumUndirLinkWeight;
			m_flowLinks[linkIndex] = Link(linkEnd1, linkEnd2, linkWeight);

			if (linkEnd1 != linkEnd2 && !config.outdirdir)
				m_nodeFlow[linkEnd2] += linkWeight / sumUndirLinkWeight;
		}
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
		Log() << "\n  -> Using directed links with raw flow.";
		Log() << "\n  -> Total link weight: " << totalLinkWeight << ".";
		Log() << std::endl;
		finalize(network, config, true);
		return;
	}

	if (!config.directed)
	{
		if (config.outdirdir)
			Log() << "\n  -> Counting only ingoing links.";
		else
			Log() << "\n  -> Using undirected links" << (config.undirdir? ", switching to directed after steady state." :
					".");
		Log() << std::endl;

		if (config.undirdir || config.outdirdir)
		{
			//Take one last power iteration
			std::vector<double> nodeFlowSteadyState(m_nodeFlow);
			m_nodeFlow.assign(numNodes, 0.0);
			for (LinkVec::iterator linkIt(m_flowLinks.begin()); linkIt != m_flowLinks.end(); ++linkIt)
			{
				Link& link = *linkIt;
				m_nodeFlow[link.target] += nodeFlowSteadyState[link.source] * link.flow / sumLinkOutWeight[link.source];
			}
			double sumNodeFlow = 0.0;
			for (unsigned int i = 0; i < m_nodeFlow.size(); ++i)
				sumNodeFlow += m_nodeFlow[i];
			// Update link data to represent flow instead of weight
			for (LinkVec::iterator linkIt(m_flowLinks.begin()); linkIt != m_flowLinks.end(); ++linkIt)
			{
				Link& link = *linkIt;
				link.flow *= nodeFlowSteadyState[link.source] / sumLinkOutWeight[link.source] / sumNodeFlow;
			}
			finalize(network, config, true);
		}
		else // undirected
		{
			for (unsigned int i = 0; i < numLinks; ++i)
				m_flowLinks[i].flow /= sumUndirLinkWeight;
			finalize(network, config);
		}
		return;
	}

	Log() << "\n  -> Using " << (config.recordedTeleportation ? "recorded" : "unrecorded") << " teleportation to " <<
			(config.teleportToNodes ? "nodes" : "links") << ". " << std::flush;


	// Calculate the teleport rate distribution
	if (config.teleportToNodes)
	{
		const std::vector<double>& nodeWeights = network.nodeWeights();
		if (nodeWeights.empty())
		{
			double rate = 1.0 / numNodes;
			for (unsigned int i = 0; i < numNodes; ++i)
				m_nodeTeleportRates[i] = rate;
		}
		else
		{
			for (unsigned int i = 0; i < numNodes; ++i)
				m_nodeTeleportRates[i] = nodeWeights[i] / network.sumNodeWeights();
		}
	}
	else
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
		if (sumLinkOutWeight[linkIt->source] > 0)
			linkIt->flow /= sumLinkOutWeight[linkIt->source];
	}

	// Collect dangling nodes
	std::vector<unsigned int> danglings;
	for (unsigned int i = 0; i < numNodes; ++i)
	{
		if (nodeOutDegree[i] == 0)
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
		for (unsigned int i = 0; i < danglings.size(); ++i)
		{
			danglingRank += m_nodeFlow[danglings[i]];
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
			Log() << "(Normalizing ranks after " <<	numIterations << " power iterations with error " << (sum-1.0) << ") ";
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

	Log() << "\n  -> PageRank calculation done in " << numIterations << " iterations." << std::endl;
	finalize(network, config);
}

void FlowNetwork::finalize(const Network& network, const Config& config, bool normalizeNodeFlow)
{
	// TODO: Skip bipartite flow adjustment for directed / rawdir / .. ?
	if (network.isBipartite() && !config.skipAdjustBipartiteFlow)
	{
		// Only links between ordinary nodes and feature nodes in bipartite network
		// Don't code feature nodes -> distribute all flow from those to ordinary nodes
		unsigned int minBipartiteNodeIndex = network.numNodes() - network.numBipartiteNodes();

		for (LinkVec::iterator linkIt(m_flowLinks.begin()); linkIt != m_flowLinks.end(); ++linkIt)
		{
			Link& link = *linkIt;
			link.flow *= 2; // Markov time 2 on the full network will correspond to markov time 1 between the real nodes.

			if (link.source >= minBipartiteNodeIndex) {
				m_nodeFlow[link.target] += link.flow;
				m_nodeFlow[link.source] = 0.0; // Doesn't matter if done multiple times on each node.
			}
			else if (config.parseAsUndirected()) {
				m_nodeFlow[link.source] += link.flow;
				m_nodeFlow[link.target] = 0.0; // Doesn't matter if done multiple times on each node.
			}
		}
		normalizeNodeFlow = true;
	}

	if (normalizeNodeFlow)
	{
		double sumNodeFlow = 0.0;
		for (unsigned int i = 0; i < m_nodeFlow.size(); ++i)
			sumNodeFlow += m_nodeFlow[i];
		for (unsigned int i = 0; i < m_nodeFlow.size(); ++i)
			m_nodeFlow[i] /= sumNodeFlow;
	}
}

#ifdef NS_INFOMAP
}
#endif
