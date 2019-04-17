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


#include "FlowCalculator.h"
#include <iostream>
#include <cmath>
#include "../utils/Log.h"
#include "../core/StateNetwork.h"

namespace infomap {

void FlowCalculator::calculateFlow(StateNetwork& network, const Config& config)
{
	Log() << "Calculating global network flow using flow model '" << config.flowModel << "'... " << std::flush;

	// Prepare data in sequence containers for fast access of individual elements
    // Map to zero-based dense indexing
	unsigned int numNodes = network.numNodes();

	m_nodeFlow.assign(numNodes, 0.0);
	m_nodeTeleportRates.assign(numNodes, 0.0);

	std::vector<unsigned int> nodeOutDegree(numNodes, 0);
	std::vector<double> sumLinkOutWeight(numNodes, 0.0);
    
	unsigned int nodeIndex = 0;
	for (auto& nodeIt : network.nodes()) {
		auto& networkNode = nodeIt.second;
		m_nodeIndexMap[networkNode.id] = nodeIndex;
		++nodeIndex;
	}

	unsigned int numLinks = network.numLinks();
	m_flowLinks.reserve(numLinks);
	double sumLinkWeight = network.sumLinkWeight();
	double sumUndirLinkWeight = 2 * sumLinkWeight - network.sumSelfLinkWeight();

    for (auto& linkIt : network.nodeLinkMap()) {
		unsigned int linkSourceId = linkIt.first.id;
		unsigned int sourceIndex = m_nodeIndexMap[linkSourceId];

		const auto& subLinks = linkIt.second;
		for (auto& subIt : subLinks)
		{
			unsigned int linkTargetId = subIt.first.id;
			unsigned int targetIndex = m_nodeIndexMap[linkTargetId];
			double linkWeight = subIt.second.weight;

			++nodeOutDegree[sourceIndex];
			sumLinkOutWeight[sourceIndex] += linkWeight;
			m_nodeFlow[sourceIndex] += linkWeight / sumUndirLinkWeight;
			m_flowLinks.push_back(Link(sourceIndex, targetIndex, linkWeight));

			// Log() << "\n" << linkSourceId << " (" << sourceIndex << ") -> " << linkTargetId << " (" << targetIndex << ")"
			// 	<< ", weight: " << linkWeight << ". ";

			if (sourceIndex != targetIndex) {
				if (config.isUndirectedFlow()) {
					++nodeOutDegree[targetIndex];
					sumLinkOutWeight[targetIndex] += linkWeight;
				}
				if (config.flowModel != FlowModel::outdirdir)
					m_nodeFlow[targetIndex] += linkWeight / sumUndirLinkWeight;
			}
		}
	}

	if (config.flowModel == FlowModel::rawdir)
	{
		// Treat the link weights as flow (after global normalization) and
		// do one power iteration to set the node flow
		m_nodeFlow.assign(numNodes, 0.0);
		for (auto& link : m_flowLinks)
		{
			link.flow /= sumLinkWeight;
			m_nodeFlow[link.target] += link.flow;
		}
		Log() << "\n  -> Using directed links with raw flow.";
		Log() << "\n  -> Total link weight: " << sumLinkWeight << ".";
		finalize(network, config, true);
		return;
	}

	if (config.flowModel != FlowModel::directed)
	{
		if (config.flowModel == FlowModel::outdirdir)
			Log() << "\n  -> Counting only ingoing links.";
		else
			Log() << "\n  -> Using undirected links" << (config.undirdir? ", switching to directed after steady state." :
					".");

		if (config.flowModel == FlowModel::undirdir || config.flowModel == FlowModel::outdirdir)
		{
			//Take one last power iteration
			std::vector<double> nodeFlowSteadyState(m_nodeFlow);
			m_nodeFlow.assign(numNodes, 0.0);
			for (auto& link : m_flowLinks)
			{
				m_nodeFlow[link.target] += nodeFlowSteadyState[link.source] * link.flow / sumLinkOutWeight[link.source];
			}
			double sumNodeFlow = 0.0;
			for (unsigned int i = 0; i < m_nodeFlow.size(); ++i)
				sumNodeFlow += m_nodeFlow[i];
			// Update link data to represent flow instead of weight
			for (auto& link : m_flowLinks)
			{
				link.flow *= nodeFlowSteadyState[link.source] / sumLinkOutWeight[link.source] / sumNodeFlow;
			}
			finalize(network, config, true);
		}
		else // undirected
		{
			for (unsigned int i = 0; i < numLinks; ++i)
				m_flowLinks[i].flow /= sumUndirLinkWeight / 2;
			finalize(network, config);
		}
		return;
	}

	Log() << "\n  -> Using " << (config.recordedTeleportation ? "recorded" : "unrecorded") << " teleportation to " <<
			(config.teleportToNodes ? "nodes" : "links") << ". " << std::flush;

	// Log() << "\nnode flow: ";
	// double sum1 = 0.0;
	// for (auto& f : m_nodeFlow) {
	// 	Log() << f << ", ";
	// 	sum1 += f;
	// }
	// Log() << "\n => sum node flow: " << sum1;

	// Log() << "\nTeleport rates: ";
	// Calculate the teleport rate distribution
	if (config.teleportToNodes)
	{
		double sumNodeWeights = 0.0;
		for (auto& nodeIt : network.nodes()) {
			auto& networkNode = nodeIt.second;
			m_nodeTeleportRates[m_nodeIndexMap[networkNode.id]] = networkNode.weight;
			sumNodeWeights += networkNode.weight;
		}
		for (auto& weight : m_nodeTeleportRates) {
			weight /= sumNodeWeights;
		}
	}
	else // Teleport to links
	{
		// Teleport proportionally to out-degree, or in-degree if recorded teleportation.
		for (auto& link : m_flowLinks)
		{
			unsigned int toNode = config.recordedTeleportation ? link.target : link.source;
			m_nodeTeleportRates[toNode] += link.flow / sumLinkWeight;
			// Log() << "\nrate[" << toNode << "] += " << link.flow << " / " << sumLinkWeight << " = " << link.flow / sumLinkWeight << " => " << m_nodeTeleportRates[toNode];
		}
	}

	// double sumTeleportRates = 0.0;
	// Log() << "\n=> ";
	// for (auto& r : m_nodeTeleportRates) {
	// 	sumTeleportRates += r;
	// 	Log() << r << ", ";
	// }
	// Log() << "\n=> Sum: " << sumTeleportRates;

	double sumLinkFlow = 0.0;
	// Log() << "\nLinks:";
	// Normalize link weights with respect to its source nodes total out-link weight;
	for (auto& link : m_flowLinks)
	{
		// Log() << "\n" << linkIt->source << " -> " << linkIt->target << ": " <<  linkIt->flow;
		// Log() << ", sumLinkOutWeight[" << linkIt->source << "]: " << sumLinkOutWeight[linkIt->source];
		if (sumLinkOutWeight[link.source] > 0)
			link.flow /= sumLinkOutWeight[link.source];
		sumLinkFlow += link.flow;
		// Log() << " => sumLinkFlow += " << linkIt->flow << " = " << sumLinkFlow;
	}

	// Collect dangling nodes
	std::vector<unsigned int> danglings;
	for (unsigned int i = 0; i < numNodes; ++i)
	{
		if (nodeOutDegree[i] == 0)
			danglings.push_back(i);
	}

	// Log() << "\nsumLinkFlow: " << sumLinkFlow;

	// Log() << "\nDanglings: ";
	// for (auto& d : danglings) {
	// 	Log() << d << ", ";
	// }

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
		for (auto& danglingIndex : danglings)
		{
			danglingRank += m_nodeFlow[danglingIndex];
		}

		// Flow from teleportation
		for (unsigned int i = 0; i < numNodes; ++i)
		{
			nodeFlowTmp[i] = (alpha + beta*danglingRank) * m_nodeTeleportRates[i];
		}

		// Flow from links
		for (auto& link : m_flowLinks)
		{
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
			// break;
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
		for (auto& link : m_flowLinks)
		{
			m_nodeFlow[link.target] += link.flow * nodeFlowTmp[link.source] / sumNodeRank;
		}
		beta = 1.0;
	}


	// Update the links with their global flow from the PageRank values. (Note: beta is set to 1 if unrec)
	for (auto& link : m_flowLinks)
	{
		link.flow *= beta * nodeFlowTmp[link.source] / sumNodeRank;
	}

	Log() << "\n  -> PageRank calculation done in " << numIterations << " iterations." << std::endl;
	finalize(network, config);
}

void FlowCalculator::finalize(StateNetwork& network, const Config& config, bool normalizeNodeFlow)
{
	// TODO: Skip bipartite flow adjustment for directed / rawdir / .. ?
	if (network.isBipartite() && !config.skipAdjustBipartiteFlow)
	{
		Log() << "\n  -> Using bipartite links.";
		// Only links between ordinary nodes and feature nodes in bipartite network
		// Don't code feature nodes -> distribute all flow from those to ordinary nodes
		unsigned int bipartiteStartId = network.bipartiteStartId();

		for (auto& link : m_flowLinks)
		{
			unsigned int sourceIsFeature = link.source >= bipartiteStartId;

			if (sourceIsFeature) {
				m_nodeFlow[link.target] += link.flow;
				m_nodeFlow[link.source] = 0.0; // Doesn't matter if done multiple times on each node.
			}
			else { // if (config.parseAsUndirected()) {
				m_nodeFlow[link.source] += link.flow;
				m_nodeFlow[link.target] = 0.0; // Doesn't matter if done multiple times on each node.
			}
			//TODO: Should flow double before moving to nodes, does it cancel out in normalization?
			link.flow *= 2; // Markov time 2 on the full network will correspond to markov time 1 between the real nodes.
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

	// Write back flow to network
	//TODO: Add enter/exit flow
	double sumNodeFlow = 0.0;
	unsigned int nodeIndex = 0;
	for (auto& nodeIt : network.m_nodes) {
		auto& networkNode = nodeIt.second;
		networkNode.flow = m_nodeFlow[nodeIndex];
		sumNodeFlow += networkNode.flow;
		++nodeIndex;
	}

	double sumLinkFlow = 0.0;
	unsigned int linkIndex = 0;
	for (auto& linkIt : network.m_nodeLinkMap) {
		for (auto& subIt : linkIt.second)
		{
			auto& linkData = subIt.second;
			linkData.flow = m_flowLinks[linkIndex].flow;
			sumLinkFlow += linkData.flow;
			++linkIndex;
		}
	}
	Log() << "\n  => Sum node flow: " << sumNodeFlow << ", sum link flow: " << sumLinkFlow << "\n";
}

}
