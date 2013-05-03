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


#include "InfomapDirectedUnrecordedTeleportation.h"
#include "../utils/Logger.h"
#include <vector>
#include <cmath>
#include <utility>
#include <iomanip>
#include <limits>

using std::pair;

InfomapDirectedUnrecordedTeleportation::InfomapDirectedUnrecordedTeleportation(const Config& conf)
:	InfomapGreedy<InfomapDirectedUnrecordedTeleportation>(conf)
 	{}


/**
 * Calculate the PageRank flow on nodes and edges using the power method.
 */
void InfomapDirectedUnrecordedTeleportation::calculateFlow()
{
	DEBUG_OUT("InfomapDirectedUnrecordedTeleportation::calculateFlow()... ");
	if (m_subLevel == 0)
	{
		RELEASE_OUT("Calculating flow on directed network for unrecorded teleportation using the eigenfactor model ");
		RELEASE_OUT("and uniform " << (m_config.teleportToNodes ?
				"node" : "link") << " teleportation model... " << std::flush);
	}

	unsigned int numNodes = m_treeData.numLeafNodes();

	// Set initial rate from a uniform distribution
	double initialRate = 1.0 / numNodes;
	std::vector<double> flowTemp(numNodes, initialRate);

	std::vector<unsigned int> danglings;

	unsigned int i = 0;
	double sumNodeWeight = 0.0;
	if (m_config.teleportToNodes)
	{
		for (TreeData::leafIterator leafIt(m_treeData.begin_leaf()), leafEnd(m_treeData.end_leaf());
				leafIt != leafEnd; ++leafIt, ++i)
		{
			NodeType& node = getNode(**leafIt);
			sumNodeWeight += node.data.teleportWeight;
		}
		if (m_subLevel == 0)
			RELEASE_OUT("sumTeleWeight: " << sumNodeWeight << ". ");
	}
	else // uniformlyToLinks
	{
		for (TreeData::leafIterator leafIt(m_treeData.begin_leaf()), leafEnd(m_treeData.end_leaf());
				leafIt != leafEnd; ++leafIt)
		{
			getNode(**leafIt).data.teleportWeight = 0.0;
		}
		for (TreeData::leafIterator leafIt(m_treeData.begin_leaf()), leafEnd(m_treeData.end_leaf());
				leafIt != leafEnd; ++leafIt)
		{
			NodeType& node = getNode(**leafIt);
			for (NodeBase::edge_iterator outEdgeIt(node.begin_outEdge()), endIt(node.end_outEdge());
					outEdgeIt != endIt; ++outEdgeIt)
			{
				double edgeWeight = (*outEdgeIt)->data.weight;
				// Teleport proportionally to outdegree in unrecorded teleportation model,
				// else node B would get zero flow in the network A --> B --> C
				node.data.teleportWeight += edgeWeight;
				sumNodeWeight += edgeWeight;
			}
		}
	}

	i = 0;
	for (TreeData::leafIterator leafIt(m_treeData.begin_leaf()), leafEnd(m_treeData.end_leaf());
			leafIt != leafEnd; ++leafIt, ++i)
	{
		NodeType& node = getNode(**leafIt);

		if (node.isDangling())
		{
			danglings.push_back(i);
		}
		// Set initial node flow to each node from uniform distribution
		node.data.flow = initialRate;
		// normalize the teleport weight from the sum above
		node.data.teleportWeight /= sumNodeWeight;

		node.data.enterFlow = 0.0;
		node.data.exitFlow = 0.0;

		// Normalize edge flow locally
		double sumOutWeights = 0.0;
		for (NodeBase::edge_iterator edgeIt(node.begin_outEdge()), endEdgeIt(node.end_outEdge());
				edgeIt != endEdgeIt; ++edgeIt)
		{
			sumOutWeights += (*edgeIt)->data.weight;
		}
		for (NodeBase::edge_iterator edgeIt(node.begin_outEdge()), endEdgeIt(node.end_outEdge());
				edgeIt != endEdgeIt; ++edgeIt)
		{
			(*edgeIt)->data.flow = (*edgeIt)->data.weight / sumOutWeights;
		}
		// Now the edge flow measures the transition probabilities for a random walker
		// following the edges proportionally to their weights.
	}

	int numIterations = 0;
	double alpha = m_config.teleportationProbability;
	double beta = 1.0 - alpha;

	double sqdiff = 1.0;
	double danglingRank = 0.0;
	do{
		// Calculate dangling size
		danglingRank = 0.0;
		for (std::vector<unsigned int>::iterator danglingsIt(danglings.begin()), danglingEnd(danglings.end());
				danglingsIt != danglingEnd; ++danglingsIt)
		{
			danglingRank += flowTemp[*danglingsIt];
		}

		// Flow from teleportation
		//		double teleportFlowPerNode = (alpha + beta*danglingRank) * initialRate;
		for (TreeData::leafIterator leafIt(m_treeData.begin_leaf()), leafEnd(m_treeData.end_leaf());
				leafIt != leafEnd; ++leafIt)
		{
			NodeType& node = getNode(**leafIt);
			//			node.data.flow = teleportFlowPerNode;
			node.data.flow = (alpha + beta*danglingRank) * node.data.teleportWeight;
		}

		i = 0;
		// Flow from network steps
		for (TreeData::leafIterator leafIt(m_treeData.begin_leaf()), leafEnd(m_treeData.end_leaf());
				leafIt != leafEnd; ++leafIt, ++i)
		{
			NodeType& node = getNode(**leafIt);
			for (NodeBase::edge_iterator edgeIt(node.begin_outEdge()), endEdgeIt(node.end_outEdge());
					edgeIt != endEdgeIt; ++edgeIt)
			{
				EdgeType& edge = **edgeIt;
				static_cast<NodeType&>(edge.target).data.flow += beta * edge.data.flow * flowTemp[i];
			}
		}

		double sum = 0.0;
		double sqdiff_old = sqdiff;
		sqdiff = 0.0;
		i = 0;
		for (TreeData::leafIterator leafIt(m_treeData.begin_leaf()), leafEnd(m_treeData.end_leaf());
				leafIt != leafEnd; ++leafIt, ++i)
		{
			NodeType& node = getNode(**leafIt);
			sum += node.data.flow;
			sqdiff += std::abs(node.data.flow - flowTemp[i]);
			flowTemp[i] = node.data.flow;
		}

		//		std::cout << "danglingRank: " << danglingRank << ", node rank: ";
		//		std::copy(flowTemp.begin(), flowTemp.end(), std::ostream_iterator<double>(std::cout, ", "));
		//		std::cout << " (sum: " << sum << ")" << std::endl;

		// Normalize if needed
		if (std::abs(sum - 1.0) > 1.0e-10)
		{
			DEBUG_OUT("Normalizing ranks... (error is " << (sum-1.0) << " after " <<
					numIterations << " iterations)." << std::endl);
			ALL_OUT("(Normalizing ranks on tree level " << m_subLevel << ". Error is " << (sum-1.0) << " after " <<
					numIterations << " iterations)." << std::endl);
			for (i = 0; i < numNodes; ++i)
			{
				flowTemp[i] /= sum;
			}
		}

		if(sqdiff == sqdiff_old) {
			alpha += 1.0e-10;
			beta = 1.0 - alpha;
		}

		++numIterations;
//	}  while((numIterations < 200) && (sqdiff > 1.0e-15 || numIterations < 50));
	}  while(sqdiff > 1.0e-10 && numIterations < 200);

	if (numIterations == 200)
	{
		RELEASE_OUT("!" << m_subLevel);
	}

	//	DEBUG_OUT(", danglingRank: " << danglingRank << std::endl);

	//	// Debug, test effect of scaling down the ranks:
	//	// Result: The optimum partition remains the same, codelength scales down equal to the flow!
	//	for (TreeData::leafIterator leafIt(m_treeData.begin_leaf()), leafEnd(m_treeData.end_leaf());
	//			leafIt != leafEnd; ++leafIt)
	//	{
	//		NodeType& node = getNode(**leafIt);
	//		node.data.flow *= 0.1;
	//	}


	double sumEdgeFlow = 0.0;
	double sumNodeFlow = 0.0;
	double sumDanglingFlow = 0.0;
	// Normalize the edge flow globally and set dangling flow and exit flow.
	for (TreeData::leafIterator leafIt(m_treeData.begin_leaf()), leafEnd(m_treeData.end_leaf());
			leafIt != leafEnd; ++leafIt)
	{
		NodeType& node = getNode(**leafIt);
		node.data.danglingFlow = node.isDangling() ? node.data.flow : 0.0;
		//		// For a node, everything exits except the flow that teleports to itself.
		//		node.data.exitFlow = node.data.flow - (alpha*node.data.flow + beta*node.data.danglingFlow) * node.data.teleportWeight;
		//		DEBUG_OUT("Node " << node << std::endl);
		double nodeOut = 0.0;
		for (NodeBase::edge_iterator edgeIt(node.begin_outEdge()), endEdgeIt(node.end_outEdge());
				edgeIt != endEdgeIt; ++edgeIt)
		{
			EdgeType& edge = **edgeIt;
			edge.data.flow *= node.data.flow;
			nodeOut += edge.data.flow;
			node.data.exitFlow += edge.data.flow;
			getNode(edge.target).data.enterFlow += edge.data.flow;

			//			DEBUG_OUT("  --> " << (*edgeIt)->data.flow << std::endl);
		}
		//		DEBUG_OUT("  ==> " << nodeOut << std::endl);
		sumEdgeFlow += nodeOut;
		sumNodeFlow += node.data.flow;
		sumDanglingFlow += node.data.danglingFlow;
	}

	double sumEnter = 0.0;
	double sumExit = 0.0;
	for (TreeData::leafIterator leafIt(m_treeData.begin_leaf()), leafEnd(m_treeData.end_leaf());
			leafIt != leafEnd; ++leafIt)
	{
		NodeType& node = getNode(**leafIt);
		sumEnter += node.data.enterFlow;
		sumExit += node.data.exitFlow;
		ASSERT(node.data.enterFlow < 1.0 && node.data.enterFlow >= 0.0);
	}

	// End with an eigenfactor step
	// Clear the node flow and let the flow on each edge aggregate on its target node.
	for (TreeData::leafIterator leafIt(m_treeData.begin_leaf()), leafEnd(m_treeData.end_leaf());
			leafIt != leafEnd; ++leafIt)
	{
		getNode(**leafIt).data.flow = 0.0;
	}
	i = 0;
	for (TreeData::leafIterator leafIt(m_treeData.begin_leaf()), leafEnd(m_treeData.end_leaf());
			leafIt != leafEnd; ++leafIt, ++i)
	{
		NodeType& node = getNode(**leafIt);
		for (NodeBase::edge_iterator edgeIt(node.begin_outEdge()), endEdgeIt(node.end_outEdge());
				edgeIt != endEdgeIt; ++edgeIt)
		{
			EdgeType& edge = **edgeIt;
			getNode(edge.target).data.flow += edge.data.flow;
		}
	}
	double sumEigenfactorFlow = 0.0;
	for (TreeData::leafIterator leafIt(m_treeData.begin_leaf()), leafEnd(m_treeData.end_leaf());
			leafIt != leafEnd; ++leafIt)
	{
		sumEigenfactorFlow += getNode(**leafIt).data.flow;
	}

	// normalize
	double lastSumNodeFlow = 0.0;
	double lastSumEdgeFlow = 0.0;
	for (TreeData::leafIterator leafIt(m_treeData.begin_leaf()), leafEnd(m_treeData.end_leaf());
			leafIt != leafEnd; ++leafIt)
	{
		NodeType& node = getNode(**leafIt);
		node.data.flow /= sumEigenfactorFlow;
		node.data.enterFlow /= sumEigenfactorFlow;
		node.data.exitFlow /= sumEigenfactorFlow;
		for (NodeBase::edge_iterator edgeIt(node.begin_outEdge()), endEdgeIt(node.end_outEdge());
				edgeIt != endEdgeIt; ++edgeIt)
		{
			EdgeType& edge = **edgeIt;
			edge.data.flow /= sumEigenfactorFlow;
			lastSumEdgeFlow += edge.data.flow;
		}
		lastSumNodeFlow += node.data.flow;
	}

	if (m_subLevel == 0)
	{
		DEBUG_OUT("Network size: " << numNodes << "/" << m_treeData.numLeafEdges() <<
				", numDanglings: " << danglings.size() <<
				", using teleport rate: " << m_config.teleportProb <<
				")" << std::endl);

		DEBUG_OUT("Sum node flow: " << sumNodeFlow << std::endl);
		DEBUG_OUT("Sum enter flow: " << sumEnter << std::endl);
		DEBUG_OUT("Sum exit flow: " << sumExit << std::endl);
		DEBUG_OUT("Sum eigenfactor flow: " << sumEigenfactorFlow << std::endl);
		DEBUG_OUT("Normalized sum node flow: " << lastSumNodeFlow << std::endl);
		DEBUG_OUT("Normalized sum edge flow: " << lastSumEdgeFlow << std::endl);


		DEBUG_OUT("done!\n (Converged with error " << sqdiff << " after " << numIterations <<
				" iterations with teleportation probablity: " << m_config.teleportProb <<
				", sumEdgeFlow: " << sumEdgeFlow << ", sumNodeFlow: " << sumNodeFlow <<
				", sumDanglingFlow: " << sumDanglingFlow << ") " << std::endl);

//		RELEASE_OUT("done!" << std::endl);
		RELEASE_OUT("done in " << numIterations << " iterations!" << std::endl);
	}
}


/**
 * For each node:
 * 1. Calculate the change in codelength for a move to each of its neighbouring modules or to an empty module
 * 2. Move to the one that reduces the codelength the most.
 *
 * The first step would require O(d^2), where d is the degree, if calculating the full change at each neighbour,
 * but a special data structure is used to accumulate the marginal effect of each link on its target, giving O(d).
 */
unsigned int InfomapDirectedUnrecordedTeleportation::optimizeModulesImpl()
{
	DEBUG_OUT("InfomapDirectedUnrecordedTeleportation::optimize()... ");

	unsigned int numNodes = m_activeNetwork.size();
	// Get random enumeration of nodes
	std::vector<unsigned int> randomOrder(numNodes);
	infomath::getRandomizedIndexVector(randomOrder, m_rand);


	// module -> (deltaOutFlow, deltaInFlow)
	std::vector<pair<unsigned int, pair<double, double> > > moduleDeltaExits(numNodes);
	std::vector<unsigned int> redirect(numNodes, 0);
	unsigned int offset = 1;
	unsigned int maxOffset = std::numeric_limits<unsigned int>::max() - 1 - numNodes;

	//	DEBUG_OUT("Begin checking " << numNodes << " nodes in random order for best merge, starting with codelength " <<
	//			codelength << "..." << std::endl);

	unsigned int numMoved = 0;
	for (unsigned int i = 0; i < numNodes; ++i)
	{
		// Reset offset before overflow
		if (offset > maxOffset)
		{
			redirect.assign(numNodes, 0);
			offset = 1;
		}

		// Pick nodes in random order
		unsigned int flip = randomOrder[i];
		NodeType& current = getNode(*m_activeNetwork[flip]);

		//		DEBUG_OUT("Checking node: " << current << " in module " << current.index << "... ");

		// If no links connecting this node with other nodes, it won't move into others,
		// and others won't move into this. TODO: Always best leave it alone?
//		if (current.degree() == 0)
		if (current.degree() == 0 ||
			(m_config.includeSelfLinks &&
			(current.outDegree() == 1 && current.inDegree() == 1) &&
			(**current.begin_outEdge()).target == current))
		{
			DEBUG_OUT("SKIPPING isolated node " << current << std::endl);
			continue;
		}

		// Create vector with module links
		unsigned int numModuleLinks = 0;
		if (current.isDangling())
		{
			redirect[current.index] = offset + numModuleLinks;
			moduleDeltaExits[numModuleLinks].first = current.index;
			moduleDeltaExits[numModuleLinks].second.first = 0.0;
			moduleDeltaExits[numModuleLinks].second.second = 0.0;
			++numModuleLinks;
		}
		else
		{
			// For all outlinks
			for (NodeBase::edge_iterator edgeIt(current.begin_outEdge()), endIt(current.end_outEdge());
					edgeIt != endIt; ++edgeIt)
			{
				EdgeType& edge = **edgeIt;
				if (edge.isSelfPointing())
					continue;
				NodeType& neighbour = getNode(edge.target);

				if (redirect[neighbour.index] >= offset)
				{
					moduleDeltaExits[redirect[neighbour.index] - offset].second.first += edge.data.flow;
				}
				else
				{
					redirect[neighbour.index] = offset + numModuleLinks;
					moduleDeltaExits[numModuleLinks].first = neighbour.index;
					moduleDeltaExits[numModuleLinks].second.first = edge.data.flow;
					moduleDeltaExits[numModuleLinks].second.second = 0.0;
					++numModuleLinks;
				}
			}
		}
		// For all inlinks
		for (NodeBase::edge_iterator edgeIt(current.begin_inEdge()), endIt(current.end_inEdge());
				edgeIt != endIt; ++edgeIt)
		{
			EdgeType& edge = **edgeIt;
			if (edge.isSelfPointing())
				continue;
			NodeType& neighbour = getNode(edge.source);

			if (redirect[neighbour.index] >= offset)
			{
				moduleDeltaExits[redirect[neighbour.index] - offset].second.second += edge.data.flow;
			}
			else
			{
				redirect[neighbour.index] = offset + numModuleLinks;
				moduleDeltaExits[numModuleLinks].first = neighbour.index;
				moduleDeltaExits[numModuleLinks].second.first = 0.0;
				moduleDeltaExits[numModuleLinks].second.second = edge.data.flow;
				++numModuleLinks;
			}
		}

		//		// For teleportation and dangling nodes
		//		double alpha = m_config.teleportProb;
		//		double beta = 1.0 - alpha;
		//		unsigned int oldM = current.index;
		//		double additionalTeleInFlowOldModuleIfNodeIsMovedOut = (alpha*current.data.flow + beta*current.data.danglingFlow)*(m_moduleFlowData[oldM].teleportWeight-current.data.teleportWeight);
		//		double additionalTeleOutFlowOldModuleIfNodeIsMovedOut = (alpha*(m_moduleFlowData[oldM].flow-current.data.flow) + beta*(m_moduleFlowData[oldM].danglingFlow-current.data.danglingFlow))*current.data.teleportWeight;
		//		for (unsigned int j = 0; j < numModuleLinks; ++j)
		//		{
		//			unsigned int otherModule = moduleDeltaExits[j].first;
		//			if(otherModule == oldM) {
		//				moduleDeltaExits[j].second.first += additionalTeleInFlowOldModuleIfNodeIsMovedOut;
		//				moduleDeltaExits[j].second.second += additionalTeleOutFlowOldModuleIfNodeIsMovedOut;
		//			}
		//			else {
		//				moduleDeltaExits[j].second.first += (alpha*current.data.flow + beta*current.data.danglingFlow)*m_moduleFlowData[otherModule].teleportWeight;
		//				moduleDeltaExits[j].second.second += (alpha*m_moduleFlowData[otherModule].flow + beta*m_moduleFlowData[otherModule].danglingFlow)*current.data.teleportWeight;
		//			}
		//		}

		unsigned int oldM = current.index;
		double additionalOutFlowOldModuleIfNodeIsMovedOut = 0.0;//additionalTeleOutFlowOldModuleIfNodeIsMovedOut;
		double additionalInFlowOldModuleIfNodeIsMovedOut = 0.0;//additionalTeleInFlowOldModuleIfNodeIsMovedOut;
		// Calculate flow to/from own module (only value from teleportation if no link to own module)
		if(redirect[current.index] >= offset)
		{
			additionalOutFlowOldModuleIfNodeIsMovedOut = moduleDeltaExits[redirect[oldM] - offset].second.first;
			additionalInFlowOldModuleIfNodeIsMovedOut = moduleDeltaExits[redirect[oldM] - offset].second.second;
		}

		// Option to move to empty module (if node not already alone)
		if (m_moduleMembers[current.index] > 1 && m_emptyModules.size() > 0)
		{
			moduleDeltaExits[numModuleLinks].first = m_emptyModules.back();
			moduleDeltaExits[numModuleLinks].second.first = 0.0;
			moduleDeltaExits[numModuleLinks].second.second = 0.0;
			++numModuleLinks;
		}

		ASSERT(numModuleLinks > 0);
		// Randomize link order for optimized search
		for (unsigned int j = 0; j < numModuleLinks - 1; ++j)
		{
			unsigned int randPos = j + m_rand.randInt(numModuleLinks - j - 1);
			std::swap(moduleDeltaExits[j], moduleDeltaExits[randPos]);
		}

		double additionalExitOldModuleIfMoved = additionalInFlowOldModuleIfNodeIsMovedOut + additionalOutFlowOldModuleIfNodeIsMovedOut;
		unsigned int bestModuleIndex = current.index;
		double bestDeltaExitOtherModule = 0.0;
		double bestDeltaCodelength = 0.0;

		// Find the move that minimizes the description length
		for (unsigned int j = 0; j < numModuleLinks; ++j)
		{
			unsigned int otherModule = moduleDeltaExits[j].first;
			if(otherModule != current.index)
			{
				double reductionInExitFlowOtherModule = moduleDeltaExits[j].second.first + moduleDeltaExits[j].second.second;

				double deltaCodelength = getDeltaCodelength(current, additionalExitOldModuleIfMoved, otherModule, reductionInExitFlowOtherModule);

				if (deltaCodelength < bestDeltaCodelength)
				{
					bestModuleIndex = otherModule;
					bestDeltaExitOtherModule = reductionInExitFlowOtherModule;
					bestDeltaCodelength = deltaCodelength;
				}

			}
		}

		//		if (bestDeltaCodelength > -1.0e-12)
		//		{
		//			bestDeltaCodelength = 0.0;
		//			bestModuleIndex = current.index;
		//		}

		// Make best possible move
		if(bestModuleIndex != current.index)
		{
			//Update empty module vector
			if(m_moduleMembers[bestModuleIndex] == 0)
			{
				m_emptyModules.pop_back();
			}
			if(m_moduleMembers[current.index] == 1)
			{
				m_emptyModules.push_back(current.index);
			}

			//			DEBUG_OUT(" --> Moved node " << current << " from module " << current.index <<
			//				" to " << bestModuleIndex << " => " << numActiveModules() <<
			//				" modules.");
			updateCodelength(current, additionalExitOldModuleIfMoved, bestModuleIndex, bestDeltaExitOtherModule);
			//			DEBUG_OUT("    (best out/in: " << tmpOut << "/" << tmpIn << ", best_delta: " << bestDeltaCodelength << std::endl);

			m_moduleMembers[current.index] -= 1;
			m_moduleMembers[bestModuleIndex] += 1;

			//			DEBUG_OUT(" --> Moved node " << current << " from module " << current.index <<
			//					" to " << bestModuleIndex << " => " << numActiveModules() <<
			//					" modules with codelength: " << codelength << std::endl);
			current.index = bestModuleIndex;
			++numMoved;
		}

		offset += numNodes;
	}
	DEBUG_OUT("done! Moved " << numMoved << " nodes into " << numActiveModules() << " modules to codelength: " <<
			codelength << std::endl);

	return numMoved;
}

unsigned int InfomapDirectedUnrecordedTeleportation::moveNodesToPredefinedModulesImpl()
{
	DEBUG_OUT("InfomapDirectedUnrecordedTeleportation::moveNodesToPredefinedModules()..." << std::endl);
	unsigned int numNodes = m_activeNetwork.size();

	DEBUG_OUT("Begin moving " << numNodes << " nodes to predefined modules, starting with codelength " <<
			codelength << "..." << std::endl);

	unsigned int numMoved = 0;
	for(unsigned int k = 0; k < numNodes; ++k)
	{
		NodeType& current = getNode(*m_activeNetwork[k]);
		unsigned int oldM = current.index; // == k
		unsigned int newM = m_moveTo[k];

		//		DEBUG_OUT("Move current node: " << current << " from module " << oldM << "(" << current.index <<
		//				") to module " << newM << "..." << std::endl);

		if (newM != oldM)
		{
			double deltaOutFlowOldM = 0.0;
			double deltaInFlowOldM = 0.0;
			double deltaOutFlowNewM = 0.0;
			double deltaInFlowNewM = 0.0;

			//			// For teleportation
			//			double nodeTeleFlow = alpha*current.data.flow + beta*current.data.danglingFlow;
			//			// module tele-flow self-out
			//			deltaOutFlowOldM += nodeTeleFlow * (m_moduleFlowData[oldM].teleportWeight - current.data.teleportWeight);
			//			deltaOutFlowNewM += nodeTeleFlow * m_moduleFlowData[newM].teleportWeight;
			//			// module tele-flow self-in
			//			deltaInFlowOldM += (alpha*(m_moduleFlowData[oldM].flow - current.data.flow) +
			//					beta*(m_moduleFlowData[oldM].danglingFlow - current.data.danglingFlow)) * current.data.teleportWeight;
			//			deltaInFlowNewM += (alpha*m_moduleFlowData[newM].flow +	beta*m_moduleFlowData[newM].danglingFlow) * current.data.teleportWeight;

			// For all outlinks
			for (NodeBase::edge_iterator edgeIt(current.begin_outEdge()), endIt(current.end_outEdge());
					edgeIt != endIt; ++edgeIt)
			{
				EdgeType& edge = **edgeIt;
				if (edge.isSelfPointing())
					continue;
				unsigned int otherModule = edge.target.index;
				if (otherModule == oldM)
				{
					deltaOutFlowOldM += edge.data.flow;
				}
				else if (otherModule == newM)
				{
					deltaOutFlowNewM += edge.data.flow;
				}
			}

			// For all inlinks
			for (NodeBase::edge_iterator edgeIt(current.begin_inEdge()), endIt(current.end_inEdge());
					edgeIt != endIt; ++edgeIt)
			{
				EdgeType& edge = **edgeIt;
				if (edge.isSelfPointing())
					continue;
				unsigned int otherModule = edge.source.index;
				if (otherModule == oldM)
				{
					deltaInFlowOldM += edge.data.flow;
				}
				else if (otherModule == newM)
				{
					deltaInFlowNewM += edge.data.flow;
				}
			}


			//			DEBUG_OUT("Update flow, old: " << deltaOutFlowOldM << ", new: " << deltaOutFlowNewM << std::endl);

			//Update empty module vector
			if(m_moduleMembers[newM] == 0)
			{
				m_emptyModules.pop_back();
			}
			if(m_moduleMembers[current.index] == 1)
			{
				m_emptyModules.push_back(current.index);
			}

			//			DEBUG_OUT(" --> Moved node " << current << " from module " << current.index <<
			//				" to " << bestModuleIndex << " => " << numActiveModules() <<
			//				" modules.");

			updateCodelength(current, deltaOutFlowOldM + deltaInFlowOldM, newM, deltaOutFlowNewM + deltaInFlowNewM);
			//			DEBUG_OUT("    (best out/in: " << tmpOut << "/" << tmpIn << ", best_delta: " << bestDeltaCodelength << std::endl);

			m_moduleMembers[current.index] -= 1;
			m_moduleMembers[newM] += 1;

			//			DEBUG_OUT(" --> Moved node " << current << " from module " << current.index <<
			//					" to " << newM << " => " << numActiveModules() <<
			//					" modules with codelength: " << codelength << std::endl);
			current.index = newM;
			++numMoved;
		}
	}
	DEBUG_OUT("Done! Moved " << numMoved << " nodes into " << numActiveModules() << " modules to codelength: " << codelength << std::endl);

	return numMoved;
}


double InfomapDirectedUnrecordedTeleportation::getDeltaCodelength(NodeType& current, double additionalExitOldModuleIfMoved,
		unsigned int newModule, double reductionInExitFlowNewModule)
{
	using infomath::plogp;
	unsigned int oldModule = current.index;

	double delta_enter = plogp(enterFlow + additionalExitOldModuleIfMoved - reductionInExitFlowNewModule) - enterFlow_log_enterFlow;

	double delta_enter_log_enter = \
			- plogp(m_moduleFlowData[oldModule].enterFlow) \
			- plogp(m_moduleFlowData[newModule].enterFlow) \
			+ plogp(m_moduleFlowData[oldModule].enterFlow - current.data.enterFlow + additionalExitOldModuleIfMoved) \
			+ plogp(m_moduleFlowData[newModule].enterFlow + current.data.enterFlow - reductionInExitFlowNewModule);

	double delta_exit_log_exit = \
			- plogp(m_moduleFlowData[oldModule].exitFlow) \
			- plogp(m_moduleFlowData[newModule].exitFlow) \
			+ plogp(m_moduleFlowData[oldModule].exitFlow - current.data.exitFlow + additionalExitOldModuleIfMoved) \
			+ plogp(m_moduleFlowData[newModule].exitFlow + current.data.exitFlow - reductionInExitFlowNewModule);

	double delta_flow_log_flow = \
			- plogp(m_moduleFlowData[oldModule].exitFlow + m_moduleFlowData[oldModule].flow) \
			- plogp(m_moduleFlowData[newModule].exitFlow + m_moduleFlowData[newModule].flow) \
			+ plogp(m_moduleFlowData[oldModule].exitFlow + m_moduleFlowData[oldModule].flow \
					- current.data.exitFlow - current.data.flow + additionalExitOldModuleIfMoved) \
					+ plogp(m_moduleFlowData[newModule].exitFlow + m_moduleFlowData[newModule].flow \
							+ current.data.exitFlow + current.data.flow - reductionInExitFlowNewModule);

	double deltaL = delta_enter - delta_enter_log_enter - delta_exit_log_exit + delta_flow_log_flow;
	return deltaL;
}

/**
 * @additionalExitOldModuleIfMoved = additionalOutFlowOldModuleIfMoved + additionalInFlowOldModuleIfMoved;
 * @reductionInExitFlowNewModule = reductionInOutFlowFlowNewModule + reductionInInFlowFlowNewModule
 */
inline
void InfomapDirectedUnrecordedTeleportation::updateCodelength(NodeType& current, double additionalExitOldModuleIfMoved,
		unsigned int newModule, double reductionInExitFlowNewModule)
{
	using infomath::plogp;
	unsigned int oldModule = current.index;

	enterFlow -= \
			m_moduleFlowData[oldModule].enterFlow + \
			m_moduleFlowData[newModule].enterFlow;
	enter_log_enter -= \
			plogp(m_moduleFlowData[oldModule].enterFlow) + \
			plogp(m_moduleFlowData[newModule].enterFlow);
	exit_log_exit -= \
			plogp(m_moduleFlowData[oldModule].exitFlow) + \
			plogp(m_moduleFlowData[newModule].exitFlow);
	flow_log_flow -= \
			plogp(m_moduleFlowData[oldModule].exitFlow + m_moduleFlowData[oldModule].flow) + \
			plogp(m_moduleFlowData[newModule].exitFlow + m_moduleFlowData[newModule].flow);

	//TODO: Why symmetric flow here? Shouldn't enter and exit be adjusted with different values?
	m_moduleFlowData[oldModule].enterFlow -= current.data.enterFlow - additionalExitOldModuleIfMoved;
	m_moduleFlowData[newModule].enterFlow += current.data.enterFlow - reductionInExitFlowNewModule;
	m_moduleFlowData[oldModule].exitFlow -= current.data.exitFlow - additionalExitOldModuleIfMoved;
	m_moduleFlowData[newModule].exitFlow += current.data.exitFlow - reductionInExitFlowNewModule;
	m_moduleFlowData[oldModule].flow -= current.data.flow;
	m_moduleFlowData[newModule].flow += current.data.flow;
	m_moduleFlowData[oldModule].danglingFlow -= current.data.danglingFlow;
	m_moduleFlowData[newModule].danglingFlow += current.data.danglingFlow;
	m_moduleFlowData[oldModule].teleportWeight -= current.data.teleportWeight;
	m_moduleFlowData[newModule].teleportWeight += current.data.teleportWeight;

	enterFlow += \
			m_moduleFlowData[oldModule].enterFlow + \
			m_moduleFlowData[newModule].enterFlow;
	enter_log_enter += \
			plogp(m_moduleFlowData[oldModule].enterFlow) + \
			plogp(m_moduleFlowData[newModule].enterFlow);
	exit_log_exit += \
			plogp(m_moduleFlowData[oldModule].exitFlow) + \
			plogp(m_moduleFlowData[newModule].exitFlow);
	flow_log_flow += \
			plogp(m_moduleFlowData[oldModule].exitFlow + m_moduleFlowData[oldModule].flow) + \
			plogp(m_moduleFlowData[newModule].exitFlow + m_moduleFlowData[newModule].flow);

	enterFlow_log_enterFlow = plogp(enterFlow);

//	codelength = enterFlow_log_enterFlow - enter_log_enter - exit_log_exit + flow_log_flow - nodeFlow_log_nodeFlow;
	indexCodelength = enterFlow_log_enterFlow - enter_log_enter - exitNetworkFlow_log_exitNetworkFlow;
	moduleCodelength = -exit_log_exit + flow_log_flow - nodeFlow_log_nodeFlow;
	codelength = indexCodelength + moduleCodelength;
	//	codelength = enterFlow_log_enterFlow - 2*exit_log_exit + flow_log_flow - nodeFlow_log_nodeFlow;
	//	DEBUG_OUT(" ==> codelength: " << (enterFlow_log_enterFlow - exit_log_exit) << " + " <<
	//    		  (-exit_log_exit + flow_log_flow - nodeFlow_log_nodeFlow) << " = " << codelength << std::endl);
}

void InfomapDirectedUnrecordedTeleportation::calculateCodelengthFromActiveNetwork()
{
//	RELEASE_OUT("########### Calculate codelength from active network: ############" << std::endl);
//	RELEASE_OUT("(*)");

	DEBUG_OUT("InfomapDirectedUnrecordedTeleportation::calculateCodelengthFromActiveNetwork()... ");
	//		nodeFlow_log_nodeFlow = 0.0;
	enter_log_enter = 0.0;
	flow_log_flow = 0.0;
	exit_log_exit = 0.0;
	enterFlow = 0.0;

	// For each module
	for (activeNetwork_iterator it(m_activeNetwork.begin()), itEnd(m_activeNetwork.end());
			it != itEnd; ++it)
	{
		NodeType& node = getNode(**it);
		//			nodeFlow_log_nodeFlow += infomath::plogp(node.data.flow);
		// own node/module codebook
		flow_log_flow += infomath::plogp(node.data.flow + node.data.exitFlow);

		// use of index codebook
		enter_log_enter += infomath::plogp(node.data.enterFlow);
		exit_log_exit += infomath::plogp(node.data.exitFlow);
		enterFlow += node.data.enterFlow;
	}
	enterFlow += exitNetworkFlow;
	enterFlow_log_enterFlow = infomath::plogp(enterFlow);

//	codelength = enterFlow_log_enterFlow - enter_log_enter - exit_log_exit + flow_log_flow - nodeFlow_log_nodeFlow;
	indexCodelength = enterFlow_log_enterFlow - enter_log_enter - exitNetworkFlow_log_exitNetworkFlow;
	moduleCodelength = -exit_log_exit + flow_log_flow - nodeFlow_log_nodeFlow;
	codelength = indexCodelength + moduleCodelength;
	DEBUG_OUT("done! Codelength for " << m_activeNetwork.size() << " nodes: " << codelength << std::endl);
}

void InfomapDirectedUnrecordedTeleportation::recalculateCodelengthFromActiveNetwork()
{
	RELEASE_OUT("########### Recalculate codelength from active network: ############" << std::endl);
	DEBUG_OUT("InfomapDirectedUnrecordedTeleportation::recalculateCodelengthFromActiveNetwork()... ");
	enter_log_enter = 0.0;
	exit_log_exit = 0.0;
	flow_log_flow = 0.0;
	enterFlow = 0.0;
	unsigned int numNodes = m_activeNetwork.size();
	for (unsigned int i = 0; i < numNodes; ++i)
	{
		m_moduleFlowData[i] = 0;
	}

	for (unsigned int i = 0; i < numNodes; ++i)
	{
		NodeType& current = getNode(*m_activeNetwork[i]);
		m_moduleFlowData[current.index].flow += current.data.flow;
		m_moduleFlowData[current.index].teleportWeight += current.data.teleportWeight;
		for (NodeBase::edge_iterator outEdgeIt(current.begin_outEdge()), endIt(current.end_outEdge());
				outEdgeIt != endIt; ++outEdgeIt)
		{
			EdgeType& edge = **outEdgeIt;
			NodeType& neighbour = getNode(edge.target);
			if (neighbour.index != current.index)
			{
				m_moduleFlowData[current.index].exitFlow += edge.data.flow;
				m_moduleFlowData[neighbour.index].enterFlow += edge.data.flow;
			}
		}
	}

	using infomath::plogp;
	for (unsigned int i = 0; i < numNodes; ++i)
	{
		enter_log_enter += plogp(m_moduleFlowData[i].enterFlow);
		exit_log_exit += plogp(m_moduleFlowData[i].exitFlow);
		flow_log_flow += plogp(m_moduleFlowData[i].exitFlow + m_moduleFlowData[i].flow);
		enterFlow += m_moduleFlowData[i].enterFlow;
	}
	enterFlow += exitNetworkFlow;
	enterFlow_log_enterFlow = plogp(enterFlow);

//	codelength = enterFlow_log_enterFlow - enter_log_enter - exit_log_exit + flow_log_flow - nodeFlow_log_nodeFlow;
	indexCodelength = enterFlow_log_enterFlow - enter_log_enter - exitNetworkFlow_log_exitNetworkFlow;
	moduleCodelength = -exit_log_exit + flow_log_flow - nodeFlow_log_nodeFlow;
	codelength = indexCodelength + moduleCodelength;

	DEBUG_OUT("$$$$$$$$$ Tuned codelength for dir unrec: " << codelength << std::endl);
}
