/* ------------------------------------------------------------------------

 Infomap software package for multi-level network clustering

 * Copyright (c) 2013
 * For license and authors, see README.txt or http://www.mapequation.org

------------------------------------------------------------------------ */

#include "InfomapUndirdir.h"
#include "../utils/Logger.h"
#include <vector>
#include <cmath>
#include <utility>
#include <limits>
using std::pair;

InfomapUndirdir::InfomapUndirdir(const Config& conf)
:	InfomapGreedy<InfomapUndirdir>(conf)
 	{}


/**
 * Calculate the PageRank flow on nodes and edges using the power method.
 */
void InfomapUndirdir::calculateFlow()
{
	DEBUG_OUT("InfomapUndirdir::calculateFlow()... ");
	if (m_subLevel == 0)
	{
		RELEASE_OUT("Calculating flow on undirected->directed network... ");
	}

	unsigned int numNodes = m_treeData.numLeafNodes();
	double totalWeight = 0.0;
	std::vector<double> weightedDegree(numNodes);
	std::vector<double> weightedOutDegree(numNodes);

	unsigned int i = 0;
	for (TreeData::leafIterator leafIt(m_treeData.begin_leaf()), leafEnd(m_treeData.end_leaf());
			leafIt != leafEnd; ++leafIt)
	{
		NodeType& node = getNode(**leafIt);
		//		DEBUG_OUT("Node " << node << std::endl);
		double edgeWeights = 0.0;
		for (NodeBase::edge_iterator edgeIt(node.begin_outEdge()), endEdgeIt(node.end_outEdge());
				edgeIt != endEdgeIt; ++edgeIt)
		{
			EdgeType& edge = **edgeIt;
			edgeWeights += edge.data.weight;
			//			DEBUG_OUT("  --> " << edge.target << ", flow: " << edge.data.flow << std::endl);
		}
		weightedOutDegree[i] = edgeWeights;
		for (NodeBase::edge_iterator edgeIt(node.begin_inEdge()), endEdgeIt(node.end_inEdge());
				edgeIt != endEdgeIt; ++edgeIt)
		{
			EdgeType& edge = **edgeIt;
			edgeWeights += edge.data.weight;
			//			DEBUG_OUT("  <-- " << edge.source << ", flow: " << edge.data.flow << std::endl);
		}
		totalWeight += edgeWeights;
		weightedDegree[i] = edgeWeights;
		++i;
	}
	double normFactor(1.0/totalWeight);


	double totalNodeFlow = 0.0;
	i = 0;
	for (TreeData::leafIterator leafIt(m_treeData.begin_leaf()), leafEnd(m_treeData.end_leaf());
			leafIt != leafEnd; ++leafIt, ++i)
	{
		NodeType& node = getNode(**leafIt);
		node.data.flow = weightedDegree[i] * normFactor;
//		node.data.exitFlow = node.data.flow;
		//		DEBUG_OUT("Node " << node << std::endl);
//		for (NodeBase::edge_iterator edgeEdgeIt(node.begin_outEdge()), endEdgeIt(node.end_outEdge());
//				edgeEdgeIt != endEdgeIt; ++edgeEdgeIt)
//		{
//			EdgeType& edge = **edgeEdgeIt;
//			edge.data.flow = edge.data.weight / weightedDegree[i] * node.data.flow;
//			//			DEBUG_OUT("  --> " << edge.target << ", flow: " << edge.data.flow << std::endl);
//		}
		totalNodeFlow += node.data.flow;
	}

	// Switch to treat the links as directed and take one power iteration to set the final flow
	
	// Clear the node flow and let the flow on each edge aggregate on its target node.
	std::vector<double> undirectedFlow(numNodes);
	i = 0;
	for (TreeData::leafIterator leafIt(m_treeData.begin_leaf()), leafEnd(m_treeData.end_leaf());
			leafIt != leafEnd; ++leafIt, ++i)
	{
		undirectedFlow[i] = getNode(**leafIt).data.flow;
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
			double edgeFlow = edge.data.weight / weightedOutDegree[i] * undirectedFlow[i];
			node.data.exitFlow += edgeFlow;
			getNode(edge.target).data.enterFlow += edgeFlow;
			getNode(edge.target).data.flow += edgeFlow;
		}
	}
	double sumNodeFlow = 0.0;
	for (TreeData::leafIterator leafIt(m_treeData.begin_leaf()), leafEnd(m_treeData.end_leaf());
			leafIt != leafEnd; ++leafIt)
	{
		sumNodeFlow += getNode(**leafIt).data.flow;
	}

	// normalize
	double lastSumNodeFlow = 0.0;
	double lastSumEdgeFlow = 0.0;
	i = 0;
	for (TreeData::leafIterator leafIt(m_treeData.begin_leaf()), leafEnd(m_treeData.end_leaf());
			leafIt != leafEnd; ++leafIt, ++i)
	{
		NodeType& node = getNode(**leafIt);
		node.data.flow /= sumNodeFlow;
		node.data.enterFlow /= sumNodeFlow;
		node.data.exitFlow /= sumNodeFlow;
		for (NodeBase::edge_iterator edgeIt(node.begin_outEdge()), endEdgeIt(node.end_outEdge());
				edgeIt != endEdgeIt; ++edgeIt)
		{
			EdgeType& edge = **edgeIt;
//			edge.data.flow /= sumNodeFlow;
			edge.data.flow = edge.data.weight / weightedOutDegree[i] * undirectedFlow[i] / sumNodeFlow;
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
		DEBUG_OUT("Normalized sum node flow: " << lastSumNodeFlow << std::endl);
		DEBUG_OUT("Normalized sum edge flow: " << lastSumEdgeFlow << std::endl);

		RELEASE_OUT("done!" << std::endl);
	}
}


/**
 * Try to minimize the codelength by trying to move nodes into the same modules as neighbouring nodes.
 * For each node:
 * 1. Calculate the change in codelength for a move to each of its neighbouring modules or to an empty module
 * 2. Move to the one that reduces the codelength the most, if any.
 *
 * The first step would require O(d^2), where d is the degree, if calculating the full change at each neighbour,
 * but a special data structure is used to accumulate the marginal effect of each link on its target, giving O(d).
 *
 * @return The number of nodes moved.
 */
unsigned int InfomapUndirdir::optimizeModulesImpl()
{
	DEBUG_OUT("InfomapUndirdir::optimize()... ");

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
			//TODO: If not skipping self-links, this yields different results from moveNodesToPredefinedModules!!
			ASSERT(!m_config.includeSelfLinks);
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

		double additionalOutFlowOldModuleIfNodeIsMovedOut = 0.0;//additionalTeleOutFlowOldModuleIfNodeIsMovedOut;
		double additionalInFlowOldModuleIfNodeIsMovedOut = 0.0;//additionalTeleInFlowOldModuleIfNodeIsMovedOut;
		// Calculate flow to/from own module (only value from teleportation if no link to own module)
		if(redirect[current.index] >= offset)
		{
			additionalOutFlowOldModuleIfNodeIsMovedOut = moduleDeltaExits[redirect[current.index] - offset].second.first;
			additionalInFlowOldModuleIfNodeIsMovedOut = moduleDeltaExits[redirect[current.index] - offset].second.second;
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

unsigned int InfomapUndirdir::moveNodesToPredefinedModulesImpl()
{
	DEBUG_OUT("InfomapUndirdir::moveNodesToPredefinedModules()..." << std::endl);
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


double InfomapUndirdir::getDeltaCodelength(NodeType& current, double additionalExitOldModuleIfMoved,
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
void InfomapUndirdir::updateCodelength(NodeType& current, double additionalExitOldModuleIfMoved,
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

void InfomapUndirdir::calculateCodelengthFromActiveNetwork()
{
	DEBUG_OUT("InfomapUndirdir::calculateCodelengthFromActiveNetwork()... ");
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

void InfomapUndirdir::recalculateCodelengthFromActiveNetwork()
{
	DEBUG_OUT("InfomapUndirdir::recalculateCodelengthFromActiveNetwork()... ");
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


