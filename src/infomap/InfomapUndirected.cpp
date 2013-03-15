/* ------------------------------------------------------------------------

 Infomap software package for multi-level network clustering

 * Copyright (c) 2013
 * For license and authors, see README.txt or http://www.mapequation.org

------------------------------------------------------------------------ */

#include "InfomapUndirected.h"
#include "../utils/Logger.h"
#include <utility>
#include <limits>

using std::pair;


void InfomapUndirected::calculateFlow()
{
	unsigned int numNodes = m_treeData.numLeafNodes();
	DEBUG_OUT("InfomapUndirected::calculateFlow() from " << numNodes << " nodes and " << m_treeData.numLeafEdges() <<
			" edges... ");

	double totalWeight = 0.0;

	std::vector<double> nodeWeights(numNodes);

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
		for (NodeBase::edge_iterator edgeIt(node.begin_inEdge()), endEdgeIt(node.end_inEdge());
				edgeIt != endEdgeIt; ++edgeIt)
		{
			EdgeType& edge = **edgeIt;
			edgeWeights += edge.data.weight;
			//			DEBUG_OUT("  <-- " << edge.source << ", flow: " << edge.data.flow << std::endl);
		}
		totalWeight += edgeWeights;
		nodeWeights[i] = edgeWeights;
		++i;
	}
	double normFactor(1.0/totalWeight);


	double totalNodeFlow = 0.0;
	i = 0;
	for (TreeData::leafIterator leafIt(m_treeData.begin_leaf()), leafEnd(m_treeData.end_leaf());
			leafIt != leafEnd; ++leafIt)
	{
		NodeType& node = getNode(**leafIt);
		node.data.flow = nodeWeights[i] * normFactor;
		node.data.exitFlow = node.data.flow;
		//		DEBUG_OUT("Node " << node << std::endl);
		for (NodeBase::edge_iterator edgeEdgeIt(node.begin_outEdge()), endEdgeIt(node.end_outEdge());
				edgeEdgeIt != endEdgeIt; ++edgeEdgeIt)
		{
			EdgeType& edge = **edgeEdgeIt;
			edge.data.flow = edge.data.weight / nodeWeights[i] * node.data.flow;
			//			DEBUG_OUT("  --> " << edge.target << ", flow: " << edge.data.flow << std::endl);
		}
		totalNodeFlow += node.data.flow;
		++i;
	}

	DEBUG_OUT("done! Total edge weight: " << totalWeight << std::endl);
}


/**
 * For each node:
 * 1. Calculate the change in codelength for a move to each of its neighbouring modules or to a single module
 * 2. Move to the one that reduces the codelength the most.
 *
 * The first step would require O(d^2), where d is the degree, if calculating the full change at each neighbour,
 * but a special data structure is used to accumulate the marginal effect of each link on its target, giving O(d).
 */
unsigned int InfomapUndirected::optimizeModulesImpl()
{
	DEBUG_OUT("InfomapUndirected::optimize()... ");

	unsigned int numNodes = m_activeNetwork.size();
	// Get random enumeration of nodes
	std::vector<unsigned int> randomOrder(numNodes);
	infomath::getRandomizedIndexVector(randomOrder, m_rand);


	std::vector<pair<unsigned int,double> > moduleDeltaExits(numNodes);
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
		unsigned int currentModuleIndex = current.index;

//		DEBUG_OUT("Checking current node: " << current << " in module " << current.index << "... ");

		// If no links connecting this node with other nodes, it won't move into others,
		// and others won't move into this. TODO: Always best leave it alone?
		if (current.degree() == 0)
		{
			DEBUG_OUT("SKIPPING isolated node " << current << std::endl);
			//TODO: If not skipping self-links, this yields different results from moveNodesToPredefinedModules!!
			ASSERT(!m_config.includeSelfLinks);
			continue;
		}

		// Create vector with module links
		unsigned int numModuleLinks = 0;
//		for (NodeBase::inout_edge_iterator edgeIt(current.begin_inoutEdge()), endIt(current.end_inoutEdge());
//				edgeIt != endIt; ++edgeIt)
//		{
//			EdgeType& edge = **edgeIt;
//			NodeType& neighbour = getNode(edge.other(current));
//
//			if (redirect[neighbour.index] >= offset)
//			{
//				moduleDeltaExits[redirect[neighbour.index] - offset].second += edge.data.flow;
//			}
//			else
//			{
//				redirect[neighbour.index] = offset + numModuleLinks;
//				moduleDeltaExits[numModuleLinks].first = neighbour.index;
//				moduleDeltaExits[numModuleLinks].second = edge.data.flow;
//				++numModuleLinks;
//			}
//		}

		// For all outlinks
		for (NodeBase::edge_iterator edgeIt(current.begin_outEdge()), endIt(current.end_outEdge());
				edgeIt != endIt; ++edgeIt)
		{
			EdgeType& edge = **edgeIt;
			if (edge.isSelfPointing())
				continue;
			unsigned int otherModule = edge.target.index;
			if (redirect[otherModule] >= offset)
			{
				moduleDeltaExits[redirect[otherModule] - offset].second += edge.data.flow;
			}
			else
			{
				redirect[otherModule] = offset + numModuleLinks;
				moduleDeltaExits[numModuleLinks].first = otherModule;
				moduleDeltaExits[numModuleLinks].second = edge.data.flow;
				++numModuleLinks;
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
			if (redirect[otherModule] >= offset)
			{
				moduleDeltaExits[redirect[otherModule] - offset].second += edge.data.flow;
			}
			else
			{
				redirect[otherModule] = offset + numModuleLinks;
				moduleDeltaExits[numModuleLinks].first = otherModule;
				moduleDeltaExits[numModuleLinks].second = edge.data.flow;
				++numModuleLinks;
			}
		}

		// Calculate exit weight to own module (zero if no link to own module)
		double deltaExitOldModule = 0.0;
		if (redirect[currentModuleIndex] >= offset)
		{
			deltaExitOldModule = moduleDeltaExits[redirect[currentModuleIndex] - offset].second;
		}

		// Option to move to empty module (if node not already alone)
		if (m_moduleMembers[currentModuleIndex] > 1 && m_emptyModules.size() > 0)
		{
			moduleDeltaExits[numModuleLinks].first = m_emptyModules.back();
			moduleDeltaExits[numModuleLinks].second = 0.0;
			++numModuleLinks;
		}

		ASSERT(numModuleLinks > 0);
		// Randomize link order for optimized search
		for (unsigned int j = 0; j < numModuleLinks - 1; ++j)
		{
			unsigned int randPos = j + m_rand.randInt(numModuleLinks - j - 1);
			std::swap(moduleDeltaExits[j], moduleDeltaExits[randPos]);
		}

		unsigned int bestModuleIndex = currentModuleIndex;
		double bestDeltaExitOtherModule = 0.0;
		double bestDeltaCodelength = 0.0;

		// Find the move that minimizes the description length
		for (unsigned int j = 0; j < numModuleLinks; ++j)
		{
			unsigned int otherModule = moduleDeltaExits[j].first;
			if(otherModule != currentModuleIndex)
			{
				double deltaExitOtherModule = moduleDeltaExits[j].second;

				double deltaCodelength = getDeltaCodelength(current, 2*deltaExitOldModule, otherModule, 2*deltaExitOtherModule);

				if (deltaCodelength < bestDeltaCodelength)
				{
					bestModuleIndex = otherModule;
					bestDeltaExitOtherModule = deltaExitOtherModule;
					bestDeltaCodelength = deltaCodelength;
				}

			}
		}

		// Make best possible move
		if(bestModuleIndex != currentModuleIndex)
		{
			//Update empty module vector
			if(m_moduleMembers[bestModuleIndex] == 0)
			{
				m_emptyModules.pop_back();
			}
			if(m_moduleMembers[currentModuleIndex] == 1)
			{
				m_emptyModules.push_back(currentModuleIndex);
			}


			// Each exit flow is also enter flow
			updateCodelength(current, 2*deltaExitOldModule, bestModuleIndex, 2*bestDeltaExitOtherModule);

			m_moduleMembers[currentModuleIndex] -= 1;
			m_moduleMembers[bestModuleIndex] += 1;

			current.index = bestModuleIndex;
			++numMoved;
//			DEBUG_OUT(" --> Moved to module: " << bestModuleIndex << " => codelength: " << codelength << std::endl);
		}

		offset += numNodes;
	}
	DEBUG_OUT("done! Moved " << numMoved << " nodes into " << numActiveModules() << " modules to codelength: " <<
			codelength << std::endl);

	return numMoved;
}


inline
void InfomapUndirected::updateCodelength(NodeType& current, double deltaExitOldModule,
		unsigned int newModule, double deltaExitNewModule)
{
	using infomath::plogp;
	unsigned int oldModule = current.index;

	enterFlow -= \
			m_moduleFlowData[oldModule].exitFlow + \
			m_moduleFlowData[newModule].exitFlow;
	exit_log_exit -= \
			plogp(m_moduleFlowData[oldModule].exitFlow) + \
			plogp(m_moduleFlowData[newModule].exitFlow);
	flow_log_flow -= \
			plogp(m_moduleFlowData[oldModule].exitFlow + m_moduleFlowData[oldModule].flow) + \
			plogp(m_moduleFlowData[newModule].exitFlow + m_moduleFlowData[newModule].flow);

	m_moduleFlowData[oldModule].exitFlow -= current.data.exitFlow - deltaExitOldModule;
	m_moduleFlowData[newModule].exitFlow += current.data.exitFlow - deltaExitNewModule;
	m_moduleFlowData[oldModule].flow -= current.data.flow;
	m_moduleFlowData[newModule].flow += current.data.flow;

	enterFlow += \
			m_moduleFlowData[oldModule].exitFlow + \
			m_moduleFlowData[newModule].exitFlow;
	exit_log_exit += \
			plogp(m_moduleFlowData[oldModule].exitFlow) + \
			plogp(m_moduleFlowData[newModule].exitFlow);
	flow_log_flow += \
			plogp(m_moduleFlowData[oldModule].exitFlow + m_moduleFlowData[oldModule].flow) + \
			plogp(m_moduleFlowData[newModule].exitFlow + m_moduleFlowData[newModule].flow);

	enterFlow_log_enterFlow = plogp(enterFlow);

//	codelength = enterFlow_log_enterFlow - 2.0*exit_log_exit + flow_log_flow - nodeFlow_log_nodeFlow;
	indexCodelength = enterFlow_log_enterFlow - exit_log_exit - exitNetworkFlow_log_exitNetworkFlow;
	moduleCodelength = -exit_log_exit + flow_log_flow - nodeFlow_log_nodeFlow;
	codelength = indexCodelength + moduleCodelength;
}

unsigned int InfomapUndirected::moveNodesToPredefinedModulesImpl()
{
	DEBUG_OUT("InfomapUndirected::moveNodesToPredefinedModules()..." << std::endl);
	unsigned int numNodes = m_activeNetwork.size();
	ASSERT(m_moveTo.size() == numNodes)

	DEBUG_OUT("Begin moving " << numNodes << " nodes to predefined modules, starting with codelength " <<
			codelength << "..." << std::endl);

	unsigned int numMoved = 0;
	for(unsigned int k = 0; k < numNodes; ++k)
	{
		NodeType& current = getNode(*m_activeNetwork[k]);
		unsigned int oldM = current.index; // == k
		unsigned int newM = m_moveTo[k];

		DEBUG_OUT("Move current node: " << current << " from module " << oldM << "(" << current.index <<
				") to module " << newM << "..." << std::endl);

		if (newM != oldM)
		{
			double outFlowOldM = 0.0;
			double inFlowOldM = 0.0;
			double outFlowNewM = 0.0;
			double inFlowNewM = 0.0;


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
					outFlowOldM += edge.data.flow;
				}
				else if (otherModule == newM)
				{
					outFlowNewM += edge.data.flow;
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
					inFlowOldM += edge.data.flow;
				}
				else if (otherModule == newM)
				{
					inFlowNewM += edge.data.flow;
				}
			}

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

			double additionInExitOldModule = outFlowOldM + inFlowOldM;
			double reductionInExitNewModule = outFlowNewM + inFlowNewM;

			updateCodelength(current, 2*additionInExitOldModule, newM, 2*reductionInExitNewModule);
//			DEBUG_OUT("    (best out/in: " << tmpOut << "/" << tmpIn << ", best_delta: " << bestDeltaCodelength << std::endl);

			m_moduleMembers[current.index] -= 1;
			m_moduleMembers[newM] += 1;

			DEBUG_OUT(" --> Moved node " << current << " from module " << current.index <<
					" to " << newM << " => " << numActiveModules() <<
					" modules with codelength: " << codelength << std::endl);
			current.index = newM;
			++numMoved;
		}
	}


	return numMoved;
}

//void InfomapUndirected::generateNetworkFromChildren(NodeBase* parent)
//{
//	DEBUG_OUT("InfomapUndirected::generateNetworkFromChildren()... " << std::endl);
//
//	exitNetworkFlow = 0.0;
//
////	sortTree(*parent);
//
//	// Clone all nodes
//	unsigned int numNodes = parent->childDegree();
//	m_treeData.reserveNodeCount(numNodes);
//	unsigned int i = 0;
//	for (NodeBase::sibling_iterator childIt(parent->begin_child()), endIt(parent->end_child());
//			childIt != endIt; ++childIt, ++i)
//	{
//		NodeBase* node = new NodeType(getNode(*childIt));
//		node->originalIndex = childIt->originalIndex;
//		if (m_debug)
//		{
//			node->originalIndex = i;
//		}
//		m_treeData.addClonedNode(node);
//		childIt->index = i; // Set index to its place in this subnetwork to be able to find edge target below
//		node->index = i;
////		if (numNodes == 18)
////		{
////			RELEASE_OUT("Adding node: " << node->originalIndex << ": " << getNode(*childIt) << std::endl);
////		}
//	}
//
////	if (m_debug)
////	{
////		NodeBase& first = *parent->begin_child();
////		RELEASE_OUT("\n" << first.index << std::endl);
////		for (NodeBase::edge_iterator outEdgeIt(first.begin_outEdge()), endIt(first.end_outEdge());
////				outEdgeIt != endIt; ++outEdgeIt)
////		{
////			EdgeType edge = **outEdgeIt;
////			RELEASE_OUT(" --> " << edge.target.index << " (" << edge.data.flow << ")" << std::endl);
////		}
////		for (NodeBase::edge_iterator inEdgeIt(first.begin_inEdge()), endIt(first.end_inEdge());
////				inEdgeIt != endIt; ++inEdgeIt)
////		{
////			EdgeType edge = **inEdgeIt;
////			RELEASE_OUT(" <-- " << edge.source.index << " (" << edge.data.flow << ")" << std::endl);
////		}
////	}
//
//	// Clone edges
//	for (NodeBase::sibling_iterator childIt(parent->begin_child()), endIt(parent->end_child());
//			childIt != endIt; ++childIt)
//	{
//		NodeBase& node = *childIt;
//		for (NodeBase::edge_iterator outEdgeIt(node.begin_outEdge()), endIt(node.end_outEdge());
//				outEdgeIt != endIt; ++outEdgeIt)
//		{
//			EdgeType edge = **outEdgeIt;
//			// If neighbour node is within the same cluster, add the link to this subnetwork.
//			if (edge.target.parent == parent)
//			{
////				ASSERT(node.index != edge.target.index);
//				m_treeData.addEdge(node.index, edge.target.index, edge.data.weight, edge.data.flow);
//			}
//			else
//			{
//				exitNetworkFlow += edge.data.flow;
//			}
//		}
//
//		// Also add exit flow from in-links (only for undirected network)
//		for (NodeBase::edge_iterator inEdgeIt(node.begin_inEdge()), endIt(node.end_inEdge());
//				inEdgeIt != endIt; ++inEdgeIt)
//		{
//			EdgeType edge = **inEdgeIt;
//			if (edge.source.parent != parent)
//			{
//				exitNetworkFlow += edge.data.flow;
//			}
//		}
//	}
//
//	//DEBUG!! Normalize edge flow
//
////	if (m_debug)
////		RELEASE_OUT(std::endl);
////
////	for (TreeData::leafIterator nodeIt(m_treeData.begin_leaf()), endIt(m_treeData.end_leaf());
////			nodeIt != endIt; ++nodeIt)
////	{
////		NodeBase& node = **nodeIt;
////		double sumEdgeFlow = 0.0;
////		for (NodeBase::edge_iterator outEdgeIt(node.begin_outEdge()), endIt(node.end_outEdge());
////				outEdgeIt != endIt; ++outEdgeIt)
////		{
////			sumEdgeFlow += (**outEdgeIt).data.flow;
////		}
////
////		// Also add exit flow from in-links (only for undirected network)
////		for (NodeBase::edge_iterator inEdgeIt(node.begin_inEdge()), endIt(node.end_inEdge());
////				inEdgeIt != endIt; ++inEdgeIt)
////		{
////			sumEdgeFlow += (**inEdgeIt).data.flow;
////		}
////
////		if (m_debug)
////			RELEASE_OUT(node.originalIndex << " (" << getNode(node).data.flow << ")" << std::endl);
////
////		for (NodeBase::edge_iterator outEdgeIt(node.begin_outEdge()), endIt(node.end_outEdge());
////				outEdgeIt != endIt; ++outEdgeIt)
////		{
////			EdgeType& edge = **outEdgeIt;
////			if (m_debug)
////				RELEASE_OUT("   " << edge.target.originalIndex << " (" << edge.data.flow << ")");
//////			edge.data.flow /= getNode(node).data.flow;
////			edge.data.flow /= sumEdgeFlow;
////			if (m_debug)
////				RELEASE_OUT(" -> " << edge.data.flow << std::endl);
////		}
////	}
////
//	if (m_debug)
//	{
//		std::string outName(m_config.outDirectory + FileURI(m_config.inDataFilename).getName() + ".graph");
//		SafeOutFile out(outName.c_str());
//		RELEASE_OUT(" (Writing current network to " << outName << "... ) ");
//		writeLeafNetwork(out);
//		out.close();
//	}
//
//
//
//	exitNetworkFlow_log_exitNetworkFlow = infomath::plogp(exitNetworkFlow);
//}

void InfomapUndirected::writeLeafNetwork(std::ostream& out)
{
	DEBUG_OUT("InfomapUndirected::writeLeafNetwork()...");
	out << "*Vertices " << m_treeData.numLeafNodes() << std::endl;

	typedef std::multimap<double, unsigned int, std::greater<double> > mapped_type; // edges
	typedef std::multimap<double, std::pair<unsigned int, std::pair<double, mapped_type> >, std::greater<double> > map_type; // nodes

	map_type sortedNodes;
	for (TreeData::leafIterator nodeIt(m_treeData.begin_leaf());
			nodeIt != m_treeData.end_leaf(); ++nodeIt)
	{
		NodeBase& node = **nodeIt;
		mapped_type edges;
		for (NodeBase::edge_iterator edgeIt(node.begin_outEdge()), endEdgeIt(node.end_outEdge());
				edgeIt != endEdgeIt; ++edgeIt)
		{
			EdgeType& edge = **edgeIt;
			edges.insert(std::make_pair(edge.data.flow, edge.target.originalIndex));
		}
		for (NodeBase::edge_iterator edgeIt(node.begin_inEdge()), endEdgeIt(node.end_inEdge());
				edgeIt != endEdgeIt; ++edgeIt)
		{
			EdgeType& edge = **edgeIt;
			edges.insert(std::make_pair(edge.data.flow, edge.source.originalIndex));
		}
		sortedNodes.insert(std::make_pair(getNode(node).data.flow,
				std::make_pair(node.originalIndex, std::make_pair(getNode(node).data.exitFlow, edges))));
	}

	for (map_type::iterator it(sortedNodes.begin()); it != sortedNodes.end(); ++it)
	{
		out << it->second.first << " (" << it->first << " / " << it->second.second.first << ")" << std::endl;
		mapped_type& edges = it->second.second.second;
		for (mapped_type::iterator edgeIt(edges.begin()); edgeIt != edges.end(); ++edgeIt)
		{
			out << "   " << edgeIt->second << " (" << edgeIt->first << ")" << std::endl;
		}
	}
}

