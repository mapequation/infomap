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

#include <cmath>
#include <iostream>
#include <iterator>
#include <map>
#include <utility>
#include <vector>

#include "../io/convert.h"
#include "../io/Config.h"
#include "../utils/Logger.h"

#ifdef NS_INFOMAP
namespace infomap
{
#endif

void MemFlowNetwork::calculateFlow(const Network& net, const Config& config)
{
	if (!config.isMemoryNetwork())
	{
		FlowNetwork::calculateFlow(net, config);
		return;
	}
	Log() << "Calculating global flow... " << std::flush;
	const MemNetwork& network = static_cast<const MemNetwork&>(net);

	// Prepare data in sequence containers for fast access of individual elements
	unsigned int numStateNodes = network.numStateNodes();
	// Copy to be able to modify
	std::vector<double> nodeOutDegree(network.outDegree());
	std::vector<double> sumLinkOutWeight(network.sumLinkOutWeight());
	m_nodeFlow.assign(numStateNodes, 0.0);
	m_nodeTeleportRates.assign(numStateNodes, 0.0);

	const MemNetwork::StateLinkMap& linkMap = network.stateLinkMap();
	unsigned int numLinks = network.numStateLinks();
	m_flowLinks.resize(numLinks);
	double totalStateLinkWeight = network.totalStateLinkWeight();
	double sumUndirLinkWeight = 2 * totalStateLinkWeight - network.totalMemorySelfLinkWeight();
	unsigned int linkIndex = 0;
	const MemNetwork::StateNodeMap& nodeMap = network.stateNodeMap();

	for (MemNetwork::StateLinkMap::const_iterator linkIt(linkMap.begin()); linkIt != linkMap.end(); ++linkIt)
	{
		const StateNode& statesource = linkIt->first;
		const std::map<StateNode, double>& subLinks = linkIt->second;
		for (std::map<StateNode, double>::const_iterator subIt(subLinks.begin()); subIt != subLinks.end(); ++subIt, ++linkIndex)
		{
			const StateNode& statetarget = subIt->first;
			double linkWeight = subIt->second;

			// Get the indices for the state nodes
			MemNetwork::StateNodeMap::const_iterator nodeMapIt = nodeMap.find(statesource);
			if (nodeMapIt == nodeMap.end())
				throw InputDomainError(io::Str() << "Couldn't find mapped index for source State node " << statesource);
			unsigned int sourceIndex = nodeMapIt->second;
			nodeMapIt = nodeMap.find(statetarget);
			if (nodeMapIt == nodeMap.end())
				throw InputDomainError(io::Str() << "Couldn't find mapped index for target State node " << statetarget);
			unsigned int targetIndex = nodeMapIt->second;

			m_nodeFlow[sourceIndex] += linkWeight;// / sumUndirLinkWeight;
			m_flowLinks[linkIndex] = Link(sourceIndex, targetIndex, linkWeight);

			if (sourceIndex != targetIndex && !config.outdirdir)
				m_nodeFlow[targetIndex] += linkWeight;// / sumUndirLinkWeight;

		}
	}

	m_statenodes.resize(numStateNodes);
	for (MemNetwork::StateNodeMap::const_iterator statenodeIt(nodeMap.begin()); statenodeIt != nodeMap.end(); ++statenodeIt)
	{
		m_statenodes[statenodeIt->second] = statenodeIt->first;
	}

	if (!config.isStateNetwork() && config.completeDanglingMemoryNodes)
	{
		unsigned int numM1Nodes = network.numNodes();
		typedef std::multimap<double, unsigned int> PhysToMemWeightMap;
		std::vector<PhysToMemWeightMap> netPhysToMem(numM1Nodes);

		const LinkMap& m1LinkMap = network.linkMap();
		// Map middle column in trigrams to target state nodes (source to link for m1 links)
		for (LinkMap::const_iterator linkIt(m1LinkMap.begin()); linkIt != m1LinkMap.end(); ++linkIt)
		{
			unsigned int linkEnd1 = linkIt->first;
			const std::map<unsigned int, double>& subLinks = linkIt->second;
			for (std::map<unsigned int, double>::const_iterator subIt(subLinks.begin()); subIt != subLinks.end(); ++subIt)
			{
				unsigned int linkEnd2 = subIt->first;
				double linkWeight = subIt->second;
				MemNetwork::StateNodeMap::const_iterator stateit = nodeMap.find(StateNode(linkEnd1, linkEnd2));
				if (stateit == nodeMap.end())
					throw InputDomainError(io::Str() << "Memory node (" << linkEnd1 << ", " << linkEnd2 << ") not indexed!");
				unsigned int statenodeIndex = stateit->second;
				PhysToMemWeightMap& physMap = netPhysToMem[linkEnd1];
				physMap.insert(std::make_pair(linkWeight, statenodeIndex));
			}
		}

	// Other ways to complete dangling nodes...
		// for (unsigned int i = 0; i < numStateNodes; ++i)
		// {
		// 	const StateNode& statenode = m_statenodes[i];
		// 	PhysToMemWeightMap& physMap = netPhysToMem[statenode.priorState];
		// 	double stateweight = network.stateNodes().find(statenode)->second;
		// 	physMap.insert(std::make_pair(stateweight, i));
		// }

		// for (map<StateNode, double>::const_iterator stateit = network.stateNodes.begin(); stateit != network.stateNodes.end(); ++stateit)
		// {
		// 	const StateNode& statenode = stateit->first;
		// 	double weight = stateit->second;
		// 	PhysToMemWeightMap& physMap = netPhysToMem[statenode.priorState];
		// 	physMap.insert(std::make_pair(weight, statenodeIndex));
		// }

		// for (unsigned int i = 0; i < m_flowLinks.size(); ++i)
		// {
		// 	const Link& link = m_flowLinks[i];
		// 	const StateNode& statenode = m_statenodes[link.target];
		// 	PhysToMemWeightMap& physMap = netPhysToMem[statenode.priorState];
		// 	physMap.insert(std::make_pair(link.weight, link.target));
		// }

		// Add M1 flow to dangling State nodes
		unsigned int numDanglingStateNodes = 0;
		unsigned int numDanglingStateNodesCompleted = 0;
		unsigned int numSelfLinks = 0;
		double sumExtraLinkWeight = 0.0;
		for (unsigned int i = 0; i < numStateNodes; ++i)
		{
			if (nodeOutDegree[i] == 0)
			{
				++numDanglingStateNodes;
				// We are in physIndex, lookup all mem nodes in that physical node
				// and add a link to the target node of those mem nodes (pre-mapped above)
				const PhysToMemWeightMap& physToMem = netPhysToMem[m_statenodes[i].physIndex];
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
							if (nodeOutDegree[from] == 0) {
								++numDanglingStateNodesCompleted;
							}
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
							if (from >= numStateNodes || to >= numStateNodes) {
								Log() << "\nRange error adding dangling links " << from << " " << to << " !!!";
							}
							m_flowLinks.push_back(Link(from, to, linkWeight));
						}
					}
				}

			}
		}
		if (m_flowLinks.size() - numLinks != 0)
			Log() << "\n  -> Added " << (m_flowLinks.size() - numLinks) << " links to " <<
				numDanglingStateNodesCompleted << " dangling memory nodes -> " << m_flowLinks.size() << " links" <<
				"\n  -> " << (numDanglingStateNodes - numDanglingStateNodesCompleted) << " dangling nodes left" << std::flush;

		totalStateLinkWeight += sumExtraLinkWeight;
		sumUndirLinkWeight = 2 * totalStateLinkWeight - network.totalMemorySelfLinkWeight();
		numLinks = m_flowLinks.size();
	}

	// Normalize node flow
	for (unsigned int i = 0; i < numStateNodes; ++i)
		m_nodeFlow[i] /= sumUndirLinkWeight;


	if (config.rawdir)
	{
		// Treat the link weights as flow (after global normalization) and
		// do one power iteration to set the node flow
		m_nodeFlow.assign(numStateNodes, 0.0);
		for (LinkVec::iterator linkIt(m_flowLinks.begin()); linkIt != m_flowLinks.end(); ++linkIt)
		{
			Link& link = *linkIt;
			link.flow /= totalStateLinkWeight;
			m_nodeFlow[link.target] += link.flow;
		}
		//Normalize node flow
		double sumNodeRank = 0.0;
		for (unsigned int i = 0; i < numStateNodes; ++i)
			sumNodeRank += m_nodeFlow[i];
		for (unsigned int i = 0; i < numStateNodes; ++i)
			m_nodeFlow[i] /= sumNodeRank;
		Log() << "\n  -> Using directed links with raw flow.";
		Log() << "\n  -> Total link weight: " << totalStateLinkWeight << ".";
		Log() << std::endl;
		return;
	}

	if (!config.directed)
	{
		if (config.undirdir || config.outdirdir)
		{
			//Take one last power iteration
			std::vector<double> nodeFlowSteadyState(m_nodeFlow);
			m_nodeFlow.assign(numStateNodes, 0.0);
			for (LinkVec::iterator linkIt(m_flowLinks.begin()); linkIt != m_flowLinks.end(); ++linkIt)
			{
				Link& link = *linkIt;
				m_nodeFlow[link.target] += nodeFlowSteadyState[link.source] * link.flow / sumLinkOutWeight[link.source];
			}
			//Normalize node flow
			double sumNodeRank = 0.0;
			for (unsigned int i = 0; i < numStateNodes; ++i)
				sumNodeRank += m_nodeFlow[i];
			for (unsigned int i = 0; i < numStateNodes; ++i)
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
			Log() << "\n  -> Counting only ingoing links.";
		else
			Log() << "\n  -> Using undirected links" << (config.undirdir? ", switching to directed after steady state." :
					".");
		Log() << std::endl;
		return;
	}

	Log() << "\n  -> Using " << (config.recordedTeleportation ? "recorded" : "unrecorded") << " teleportation to memory " <<
				(config.teleportToNodes ? "nodes" : "links") << " " << std::flush;
	if (config.originallyUndirected)
	{
		if (config.recordedTeleportation || !config.teleportToNodes)
			Log() << "(warning: should be unrecorded teleportation to nodes to correspond to undirected flow on physical network) " << std::flush;
		else
			Log() << "(corresponding to undirected flow on physical network) " << std::flush;
	}


	// Calculate the teleport rate distribution
	if (config.teleportToNodes)
	{
		const std::vector<double>& nodeWeights = network.stateNodeWeights();
		for (unsigned int i = 0; i < numStateNodes; ++i) {
//			m_nodeTeleportRates[i] = sumLinkOutWeight[i] / totalStateLinkWeight;
			// Use original state weights (without m1-completed weights for dangling state nodes)
			m_nodeTeleportRates[i] = nodeWeights[i] / network.totalStateNodeWeight();
		}
	}
	else // Teleport to links
	{
		// Teleport proportionally to out-degree, or in-degree if recorded teleportation.
		for (LinkVec::iterator linkIt(m_flowLinks.begin()); linkIt != m_flowLinks.end(); ++linkIt)
		{
			unsigned int toNode = config.recordedTeleportation ? linkIt->target : linkIt->source;
			m_nodeTeleportRates[toNode] += linkIt->flow / totalStateLinkWeight;
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
	for (unsigned int i = 0; i < numStateNodes; ++i)
	{
		if (nodeOutDegree[i] == 0)
			danglings.push_back(i);
	}

	// Calculate PageRank
	std::vector<double> nodeFlowTmp(numStateNodes, 0.0);
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
		for (unsigned int i = 0; i < numStateNodes; ++i)
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
		for (unsigned int i = 0; i < numStateNodes; ++i)
		{
			sum += nodeFlowTmp[i];
			sqdiff += std::abs(nodeFlowTmp[i] - m_nodeFlow[i]);
			m_nodeFlow[i] = nodeFlowTmp[i];
		}

		// Normalize if needed
		if (std::abs(sum - 1.0) > 1.0e-10)
		{
			Log() << "(Normalizing ranks after " <<	numIterations << " power iterations with error " << (sum-1.0) << ") ";
			for (unsigned int i = 0; i < numStateNodes; ++i)
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
	}  while((numIterations < 300) && (sqdiff > 1.0e-15 || numIterations < 50));

	double sumNodeRank = 1.0;

	if (!config.recordedTeleportation)
	{
		//Take one last power iteration excluding the teleportation (and normalize node flow to sum 1.0)
		sumNodeRank = 1.0 - danglingRank;
		m_nodeFlow.assign(numStateNodes, 0.0);
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
}

#ifdef NS_INFOMAP
}
#endif
