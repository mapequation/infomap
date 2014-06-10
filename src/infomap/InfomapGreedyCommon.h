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


#ifndef INFOMAPGREEDYCOMMON_H_
#define INFOMAPGREEDYCOMMON_H_
#include "InfomapGreedySpecialized.h"
#include <memory>


template<typename InfomapGreedyDerivedType>
class InfomapGreedyCommon : public InfomapGreedySpecialized<typename derived_traits<InfomapGreedyDerivedType>::flow_type>
{
	typedef typename derived_traits<InfomapGreedyDerivedType>::flow_type				FlowType;
	// typedef FlowDirectedNonDetailedBalance 												FlowType; // DEBUG!!
	typedef InfomapGreedySpecialized<FlowType>							Super;
	typedef typename flowData_traits<FlowType>::detailed_balance_type 	DetailedBalanceType;
	typedef typename flowData_traits<FlowType>::directed_with_recorded_teleportation_type DirectedWithRecordedTeleportationType;
	typedef typename flowData_traits<FlowType>::teleportation_type 		TeleportationType;
	typedef typename flowData_traits<FlowType>::is_directed_type		IsDirectedType;
	typedef MemNode<FlowType>											NodeType;//TODO: Specialize node type on network type!
	typedef Edge<NodeBase>												EdgeType;
public:

	InfomapGreedyCommon(const Config& conf) : InfomapGreedySpecialized<FlowType>(conf) {}
	virtual ~InfomapGreedyCommon() {}


private:

	InfomapGreedyDerivedType& derived();

protected:

	virtual std::auto_ptr<InfomapBase> getNewInfomapInstance();

	virtual double calcCodelengthOnRootOfLeafNodes(const NodeBase& parent);
	virtual double calcCodelengthOnModuleOfModules(const NodeBase& parent);
	virtual double calcCodelengthOnModuleOfLeafNodes(const NodeBase& parent);

	virtual void calculateCodelengthFromActiveNetwork(DetailedBalance);
	virtual void calculateCodelengthFromActiveNetwork(NoDetailedBalance);

	virtual unsigned int optimizeModules();

	unsigned int tryMoveEachNodeIntoBestModule();

	virtual void moveNodesToPredefinedModules();

	virtual unsigned int consolidateModules(bool replaceExistingStructure, bool asSubModules);


};


template<typename InfomapGreedyDerivedType>
inline
InfomapGreedyDerivedType& InfomapGreedyCommon<InfomapGreedyDerivedType>::derived()
{
	return static_cast<InfomapGreedyDerivedType&>(*this);
}


template<typename InfomapGreedyDerivedType>
inline
std::auto_ptr<InfomapBase> InfomapGreedyCommon<InfomapGreedyDerivedType>::getNewInfomapInstance()
{
	return std::auto_ptr<InfomapBase>(new InfomapGreedyDerivedType(Super::m_config));
}

template<typename InfomapGreedyDerivedType>
inline double InfomapGreedyCommon<InfomapGreedyDerivedType>::calcCodelengthOnRootOfLeafNodes(const NodeBase& parent)
{
	return calcCodelengthOnModuleOfLeafNodes(parent);
}

template<typename InfomapGreedyDerivedType>
inline double InfomapGreedyCommon<InfomapGreedyDerivedType>::calcCodelengthOnModuleOfLeafNodes(const NodeBase& parent)
{
	const FlowType& parentData = Super::getNode(parent).data;
	double parentFlow = parentData.flow;
	double parentExit = parentData.exitFlow;
	double totalParentFlow = parentFlow + parentExit;
	if (totalParentFlow < 1e-16)
		return 0.0;

	double indexLength = 0.0;
	// For each child
	for (NodeBase::const_sibling_iterator childIt(parent.begin_child()), endIt(parent.end_child());
			childIt != endIt; ++childIt)
	{
		indexLength -= infomath::plogp(Super::getNode(*childIt).data.flow / totalParentFlow);
	}
	indexLength -= infomath::plogp(parentExit / totalParentFlow);

	indexLength *= totalParentFlow;

	return indexLength;
}

template<typename InfomapGreedyDerivedType>
inline double InfomapGreedyCommon<InfomapGreedyDerivedType>::calcCodelengthOnModuleOfModules(const NodeBase& parent)
{
	const FlowType& parentData = Super::getNode(parent).data;
	double parentExit = parentData.exitFlow;
	if (parentData.flow < 1e-16)
		return 0.0;

	// H(x) = -xlog(x), T = q + SUM(p), q = exitFlow, p = enterFlow
	// Normal format
	// L = q * -log(q/T) + p * SUM(-log(p/T))
	// Compact format
	// L = T * ( H(q/T) + SUM( H(p/T) ) )
	// Expanded format
	// L = q * -log(q) - q * -log(T) + SUM( p * -log(p) - p * -log(T) ) = T * log(T) - q*log(q) - SUM( p*log(p) )
	// As T is not known, use expanded format to avoid two loops
	double sumEnter = 0.0;
	double sumEnterLogEnter = 0.0;
	for (NodeBase::const_sibling_iterator childIt(parent.begin_child()), endIt(parent.end_child());
			childIt != endIt; ++childIt)
	{
		const double& enterFlow = Super::getNode(*childIt).data.enterFlow; // rate of enter to finer level
		sumEnter += enterFlow;
		sumEnterLogEnter += infomath::plogp(enterFlow);
	}
	// The possibilities from this module: Either exit to coarser level or enter one of its children
	double totalCodewordUse = parentExit + sumEnter;

	return infomath::plogp(totalCodewordUse) - sumEnterLogEnter - infomath::plogp(parentExit);
}

/**
 * Specialized for the case when enter flow equals exit flow
 */
template<typename InfomapGreedyDerivedType>
void InfomapGreedyCommon<InfomapGreedyDerivedType>::calculateCodelengthFromActiveNetwork(DetailedBalance)
{
	Super::exit_log_exit = 0.0;
//	enter_log_enter = 0.0;
	Super::enterFlow = 0.0;
	Super::flow_log_flow = 0.0;

	// For each module
	for (typename Super::activeNetwork_iterator it(Super::m_activeNetwork.begin()), itEnd(Super::m_activeNetwork.end());
			it != itEnd; ++it)
	{
		NodeType& node = Super::getNode(**it);
		// own node/module codebook
		Super::flow_log_flow += infomath::plogp(node.data.flow + node.data.exitFlow);

		// use of index codebook
		Super::enterFlow      += node.data.exitFlow;
//		enter_log_enter += infomath::plogp(node.data.enterFlow);
		Super::exit_log_exit += infomath::plogp(node.data.exitFlow);
	}

	Super::enterFlow += Super::exitNetworkFlow;
	Super::enterFlow_log_enterFlow = infomath::plogp(Super::enterFlow);

	derived().calculateNodeFlow_log_nodeFlowForMemoryNetwork();
	
	Super::indexCodelength = Super::enterFlow_log_enterFlow - Super::exit_log_exit - Super::exitNetworkFlow_log_exitNetworkFlow;
	Super::moduleCodelength = -Super::exit_log_exit + Super::flow_log_flow - Super::nodeFlow_log_nodeFlow;
	Super::codelength = Super::indexCodelength + Super::moduleCodelength;
}

/**
 * Specialized for the case when enter and exit flow may differ
 */
template<typename InfomapGreedyDerivedType>
void InfomapGreedyCommon<InfomapGreedyDerivedType>::calculateCodelengthFromActiveNetwork(NoDetailedBalance)
{
	Super::enter_log_enter = 0.0;
	Super::flow_log_flow = 0.0;
	Super::exit_log_exit = 0.0;
	Super::enterFlow = 0.0;

	// For each module
	for (typename Super::activeNetwork_iterator it(Super::m_activeNetwork.begin()), itEnd(Super::m_activeNetwork.end());
			it != itEnd; ++it)
	{
		NodeType& node = Super::getNode(**it);
		// own node/module codebook
		Super::flow_log_flow += infomath::plogp(node.data.flow + node.data.exitFlow);

		// use of index codebook
		Super::enter_log_enter += infomath::plogp(node.data.enterFlow);
		Super::exit_log_exit += infomath::plogp(node.data.exitFlow);
		Super::enterFlow += node.data.enterFlow;
	}
	Super::enterFlow += Super::exitNetworkFlow;
	Super::enterFlow_log_enterFlow = infomath::plogp(Super::enterFlow);

	derived().calculateNodeFlow_log_nodeFlowForMemoryNetwork();

	Super::indexCodelength = Super::enterFlow_log_enterFlow - Super::enter_log_enter - Super::exitNetworkFlow_log_exitNetworkFlow;
	Super::moduleCodelength = -Super::exit_log_exit + Super::flow_log_flow - Super::nodeFlow_log_nodeFlow;
	Super::codelength = Super::indexCodelength + Super::moduleCodelength;
}



template<typename InfomapGreedyDerivedType>
inline
unsigned int InfomapGreedyCommon<InfomapGreedyDerivedType>::optimizeModules()
{
	DEBUG_OUT("\nInfomapGreedyCommon<InfomapGreedyDerivedType>::optimizeModules()");
	unsigned int numOptimizationRounds = 0;
	double oldCodelength = Super::codelength;
	unsigned int loopLimit = Super::m_config.coreLoopLimit;
	if (Super::m_config.coreLoopLimit > 0 && Super::m_config.randomizeCoreLoopLimit)
		loopLimit = static_cast<unsigned int>(Super::m_rand() * Super::m_config.coreLoopLimit) + 1;

	// Iterate while the optimization loop moves some nodes within the dynamic modular structure
	do
	{
		oldCodelength = Super::codelength;
		tryMoveEachNodeIntoBestModule(); // returns numNodesMoved
		++numOptimizationRounds;
	} while (numOptimizationRounds != loopLimit &&
			Super::codelength < oldCodelength - Super::m_config.minimumCodelengthImprovement);

	return numOptimizationRounds;
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
template<typename InfomapGreedyDerivedType>
inline
unsigned int InfomapGreedyCommon<InfomapGreedyDerivedType>::tryMoveEachNodeIntoBestModule()
{
	unsigned int numNodes = Super::m_activeNetwork.size();
	// Get random enumeration of nodes
	std::vector<unsigned int> randomOrder(numNodes);
	infomath::getRandomizedIndexVector(randomOrder, Super::m_rand);

	std::vector<DeltaFlow> moduleDeltaEnterExit(numNodes);
	std::vector<unsigned int> redirect(numNodes, 0);
	unsigned int offset = 1;
	unsigned int maxOffset = std::numeric_limits<unsigned int>::max() - 1 - numNodes;


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
		NodeType& current = Super::getNode(*Super::m_activeNetwork[flip]);

		if (!current.dirty)
			continue;

		// If no links connecting this node with other nodes, it won't move into others,
		// and others won't move into this. TODO: Always best leave it alone?
//		if (current.degree() == 0)
		if (current.degree() == 0 ||
			(Super::m_config.includeSelfLinks &&
			(current.outDegree() == 1 && current.inDegree() == 1) &&
			(**current.begin_outEdge()).target == current))
		{
			DEBUG_OUT("SKIPPING isolated node " << current << "\n");
			//TODO: If not skipping self-links, this yields different results from moveNodesToPredefinedModules!!
			ASSERT(!m_config.includeSelfLinks);
			current.dirty = false;
			continue;
		}

		// Create vector with module links

		unsigned int numModuleLinks = 0;
		if (current.isDangling())
		{
			redirect[current.index] = offset + numModuleLinks;
			moduleDeltaEnterExit[numModuleLinks].module = current.index;
			moduleDeltaEnterExit[numModuleLinks].deltaExit = 0.0;
			moduleDeltaEnterExit[numModuleLinks].deltaEnter = 0.0;
			moduleDeltaEnterExit[numModuleLinks].sumDeltaPlogpPhysFlow = 0.0;
			moduleDeltaEnterExit[numModuleLinks].sumPlogpPhysFlow = 0.0;
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
				NodeType& neighbour = Super::getNode(edge.target);

				if (redirect[neighbour.index] >= offset)
				{
					moduleDeltaEnterExit[redirect[neighbour.index] - offset].deltaExit += edge.data.flow;
				}
				else
				{
					redirect[neighbour.index] = offset + numModuleLinks;
					moduleDeltaEnterExit[numModuleLinks].module = neighbour.index;
					moduleDeltaEnterExit[numModuleLinks].deltaExit = edge.data.flow;
					moduleDeltaEnterExit[numModuleLinks].deltaEnter = 0.0;
					moduleDeltaEnterExit[numModuleLinks].sumDeltaPlogpPhysFlow = 0.0;
					moduleDeltaEnterExit[numModuleLinks].sumPlogpPhysFlow = 0.0;
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
			NodeType& neighbour = Super::getNode(edge.source);

			if (redirect[neighbour.index] >= offset)
			{
				moduleDeltaEnterExit[redirect[neighbour.index] - offset].deltaEnter += edge.data.flow;
			}
			else
			{
				redirect[neighbour.index] = offset + numModuleLinks;
				moduleDeltaEnterExit[numModuleLinks].module = neighbour.index;
				moduleDeltaEnterExit[numModuleLinks].deltaExit = 0.0;
				moduleDeltaEnterExit[numModuleLinks].deltaEnter = edge.data.flow;
				moduleDeltaEnterExit[numModuleLinks].sumDeltaPlogpPhysFlow = 0.0;
				moduleDeltaEnterExit[numModuleLinks].sumPlogpPhysFlow = 0.0;
				++numModuleLinks;
			}
		}

		// If alone in the module, add virtual link to the module (used when adding teleportation)
		if (redirect[current.index] < offset)
		{
			redirect[current.index] = offset + numModuleLinks;
			moduleDeltaEnterExit[numModuleLinks].module = current.index;
			moduleDeltaEnterExit[numModuleLinks].deltaExit = 0.0;
			moduleDeltaEnterExit[numModuleLinks].deltaEnter = 0.0;
			moduleDeltaEnterExit[numModuleLinks].sumDeltaPlogpPhysFlow = 0.0;
			moduleDeltaEnterExit[numModuleLinks].sumPlogpPhysFlow = 0.0;
			++numModuleLinks;
		}


		// Empty function if no teleportation coding model
		Super::addTeleportationDeltaFlowIfMove(current, moduleDeltaEnterExit, numModuleLinks);

		// Option to move to empty module (if node not already alone)
		if (Super::m_moduleMembers[current.index] > 1 && Super::m_emptyModules.size() > 0)
		{
			moduleDeltaEnterExit[numModuleLinks].module = Super::m_emptyModules.back();
			moduleDeltaEnterExit[numModuleLinks].deltaExit = 0.0;
			moduleDeltaEnterExit[numModuleLinks].deltaEnter = 0.0;
			moduleDeltaEnterExit[numModuleLinks].sumDeltaPlogpPhysFlow = 0.0;
			moduleDeltaEnterExit[numModuleLinks].sumPlogpPhysFlow = 0.0;
			++numModuleLinks;
		}

		// Store the DeltaFlow of the current module
		DeltaFlow oldModuleDelta(moduleDeltaEnterExit[redirect[current.index] - offset]);


		// For memory networks
		derived().addContributionOfMovingMemoryNodes(current, oldModuleDelta, moduleDeltaEnterExit, redirect, offset, numModuleLinks);


		// Randomize link order for optimized search
		for (unsigned int j = 0; j < numModuleLinks - 1; ++j)
		{
			unsigned int randPos = j + Super::m_rand.randInt(numModuleLinks - j - 1);
			swap(moduleDeltaEnterExit[j], moduleDeltaEnterExit[randPos]);
		}

		DeltaFlow bestDeltaModule(oldModuleDelta);
		double bestDeltaCodelength = 0.0;

		// Find the move that minimizes the description length
		for (unsigned int j = 0; j < numModuleLinks; ++j)
		{
			unsigned int otherModule = moduleDeltaEnterExit[j].module;
			if(otherModule != current.index)
			{
				double deltaCodelength = Super::getDeltaCodelengthOnMovingNode(current, oldModuleDelta, moduleDeltaEnterExit[j]);
				deltaCodelength += derived().getDeltaCodelengthOnMovingMemoryNode(oldModuleDelta, moduleDeltaEnterExit[j]);

				if (deltaCodelength < bestDeltaCodelength)
				{
					bestDeltaModule = moduleDeltaEnterExit[j];
					bestDeltaCodelength = deltaCodelength;
				}

			}
		}

		// Make best possible move
		if(bestDeltaModule.module != current.index)
		{
			unsigned int bestModuleIndex = bestDeltaModule.module;
			//Update empty module vector
			if(Super::m_moduleMembers[bestModuleIndex] == 0)
			{
				Super::m_emptyModules.pop_back();
			}
			if(Super::m_moduleMembers[current.index] == 1)
			{
				Super::m_emptyModules.push_back(current.index);
			}

			Super::updateCodelengthOnMovingNode(current, oldModuleDelta, bestDeltaModule);
			derived().updateCodelengthOnMovingMemoryNode(oldModuleDelta, bestDeltaModule);

			Super::m_moduleMembers[current.index] -= 1;
			Super::m_moduleMembers[bestModuleIndex] += 1;

			unsigned int oldModuleIndex = current.index;
			current.index = bestModuleIndex;

			// Update physical node map on move for memory networks
			derived().performMoveOfMemoryNode(current, oldModuleIndex, bestModuleIndex);

			++numMoved;

			// Mark neighbours as dirty
			for (NodeBase::edge_iterator edgeIt(current.begin_outEdge()), endIt(current.end_outEdge());
					edgeIt != endIt; ++edgeIt)
				(*edgeIt)->target.dirty = true;
			for (NodeBase::edge_iterator edgeIt(current.begin_inEdge()), endIt(current.end_inEdge());
					edgeIt != endIt; ++edgeIt)
				(*edgeIt)->source.dirty = true;
		}
		else
			current.dirty = false;

		offset += numNodes;
	}

	return numMoved;
}

template<typename InfomapGreedyDerivedType>
void InfomapGreedyCommon<InfomapGreedyDerivedType>::moveNodesToPredefinedModules()
{
	// Size of active network and cluster array should match.
	ASSERT(m_moveTo.size() == m_activeNetwork.size());

	unsigned int numNodes = Super::m_activeNetwork.size();

	DEBUG_OUT("Begin moving " << numNodes << " nodes to predefined modules, starting with codelength " <<
			codelength << "..." << std::endl);

	unsigned int numMoved = 0;
	for(unsigned int k = 0; k < numNodes; ++k)
	{
		NodeType& current = Super::getNode(*Super::m_activeNetwork[k]);
		unsigned int oldM = current.index; // == k
		unsigned int newM = Super::m_moveTo[k];

		if (newM != oldM)
		{
			DeltaFlow oldModuleDelta(oldM, 0.0, 0.0, 0.0, 0.0);
			DeltaFlow newModuleDelta(newM, 0.0, 0.0, 0.0, 0.0);

			Super::addTeleportationDeltaFlowOnOldModuleIfMove(current, oldModuleDelta);
			Super::addTeleportationDeltaFlowOnNewModuleIfMove(current, newModuleDelta);

			// For all outlinks
			for (NodeBase::edge_iterator edgeIt(current.begin_outEdge()), endIt(current.end_outEdge());
					edgeIt != endIt; ++edgeIt)
			{
				EdgeType& edge = **edgeIt;
				if (edge.isSelfPointing())
					continue;
				unsigned int otherModule = edge.target.index;
				if (otherModule == oldM)
					oldModuleDelta.deltaExit += edge.data.flow;
				else if (otherModule == newM)
					newModuleDelta.deltaExit += edge.data.flow;
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
					oldModuleDelta.deltaEnter += edge.data.flow;
				else if (otherModule == newM)
					newModuleDelta.deltaEnter += edge.data.flow;
			}


			// For memory networks
			derived().performPredefinedMoveOfMemoryNode(current, oldM, newM, oldModuleDelta, newModuleDelta);


			//Update empty module vector
			if(Super::m_moduleMembers[newM] == 0)
			{
				Super::m_emptyModules.pop_back();
			}
			if(Super::m_moduleMembers[oldM] == 1)
			{
				Super::m_emptyModules.push_back(oldM);
			}

			Super::updateCodelengthOnMovingNode(current, oldModuleDelta, newModuleDelta);
			derived().updateCodelengthOnMovingMemoryNode(oldModuleDelta, newModuleDelta);

			Super::m_moduleMembers[oldM] -= 1;
			Super::m_moduleMembers[newM] += 1;

			current.index = newM;
			++numMoved;
		}
	}
	DEBUG_OUT("Done! Moved " << numMoved << " nodes into " << numActiveModules() << " modules to codelength: " << codelength << std::endl);
}


template<typename InfomapGreedyDerivedType>
unsigned int InfomapGreedyCommon<InfomapGreedyDerivedType>::consolidateModules(bool replaceExistingStructure, bool asSubModules)
{
	unsigned int numNodes = Super::m_activeNetwork.size();
	std::vector<NodeBase*> modules(numNodes, 0);

	bool activeNetworkAlreadyHaveModuleLevel = Super::m_activeNetwork[0]->parent != Super::root();
	bool activeNetworkIsLeafNetwork = Super::m_activeNetwork[0]->isLeaf();


	if (asSubModules)
	{
		ASSERT(activeNetworkAlreadyHaveModuleLevel);
		// Release the pointers from modules to leaf nodes so that the new submodules will be inserted as its only children.
		for (NodeBase::sibling_iterator moduleIt(Super::root()->begin_child()), moduleEnd(Super::root()->end_child());
				moduleIt != moduleEnd; ++moduleIt)
		{
			moduleIt->releaseChildren();
		}
	}
	else
	{
		// Happens after optimizing fine-tune and when moving leaf nodes to super clusters
		if (activeNetworkAlreadyHaveModuleLevel)
		{
			DEBUG_OUT("Replace existing " << numTopModules() << " modules with its children before consolidating the " <<
					numActiveModules() << " dynamic modules... ");
			Super::root()->replaceChildrenWithGrandChildren();
			ASSERT(m_activeNetwork[0]->parent == root());
		}
		Super::root()->releaseChildren();
	}

	// Create the new module nodes and re-parent the active network from its common parent to the new module level
	for (unsigned int i = 0; i < numNodes; ++i)
	{
		NodeBase* node = Super::m_activeNetwork[i];
		unsigned int moduleIndex = node->index;
		if (modules[moduleIndex] == 0)
		{
			modules[moduleIndex] = new NodeType(Super::m_moduleFlowData[moduleIndex]);
			node->parent->addChild(modules[moduleIndex]);
			modules[moduleIndex]->index = moduleIndex;
			// If node->parent is a module, its former children (leafnodes) has been released above, getting only submodules
//			if (node->parent->firstChild == node)
//				node->parent->firstChild = modules[moduleIndex];
		}
		modules[moduleIndex]->addChild(node);
	}

	if (asSubModules)
	{
		DEBUG_OUT("Consolidated " << numActiveModules() << " submodules under " << numTopModules() << " modules, " <<
				"store module structure before releasing it..." << std::endl);
		// Store the module structure on the submodules
		unsigned int moduleIndex = 0;
		for (NodeBase::sibling_iterator moduleIt(Super::root()->begin_child()), endIt(Super::root()->end_child());
				moduleIt != endIt; ++moduleIt, ++moduleIndex)
		{
			for (NodeBase::sibling_iterator subModuleIt(moduleIt->begin_child()), endIt(moduleIt->end_child());
					subModuleIt != endIt; ++subModuleIt)
			{
				subModuleIt->index = moduleIndex;
			}
		}
		if (replaceExistingStructure)
		{
			// Remove the module level
			Super::root()->replaceChildrenWithGrandChildren();
		}
	}


	// Aggregate links from lower level to the new modular level
	typedef std::pair<NodeBase*, NodeBase*> NodePair;
	typedef std::map<NodePair, double> EdgeMap;
	EdgeMap moduleLinks;

	for (typename Super::activeNetwork_iterator nodeIt(Super::m_activeNetwork.begin()), nodeEnd(Super::m_activeNetwork.end());
			nodeIt != nodeEnd; ++nodeIt)
	{
		NodeBase* node = *nodeIt;

		NodeBase* parent = node->parent;
		for (NodeBase::edge_iterator edgeIt(node->begin_outEdge()), edgeEnd(node->end_outEdge());
				edgeIt != edgeEnd; ++edgeIt)
		{
			EdgeType* edge = *edgeIt;
			NodeBase* otherParent = edge->target.parent;

			if (otherParent != parent)
			{
				NodeBase *m1 = parent, *m2 = otherParent;
				// If undirected, the order may be swapped to aggregate the edge on an opposite one
				if (!IsDirectedType() && m1->index > m2->index)
					std::swap(m1, m2);
				// Insert the node pair in the edge map. If not inserted, add the flow value to existing node pair.
				std::pair<EdgeMap::iterator, bool> ret = \
						moduleLinks.insert(std::make_pair(NodePair(m1, m2), edge->data.flow));
				if (!ret.second)
					ret.first->second += edge->data.flow;
			}
		}
	}

	// Add the aggregated edge flow structure to the new modules
	for (EdgeMap::const_iterator edgeIt(moduleLinks.begin()), edgeEnd(moduleLinks.end());
			edgeIt != edgeEnd; ++edgeIt)
	{
		const NodePair& nodePair = edgeIt->first;
		nodePair.first->addOutEdge(*nodePair.second, 0.0, edgeIt->second);
	}

	// Replace active network with its children if not at leaf level.
	if (!activeNetworkIsLeafNetwork && replaceExistingStructure)
	{
		for (typename Super::activeNetwork_iterator nodeIt(Super::m_activeNetwork.begin()), nodeEnd(Super::m_activeNetwork.end());
				nodeIt != nodeEnd; ++nodeIt)
		{
			(*nodeIt)->replaceWithChildren();
		}
	}

	// Calculate the number of non-trivial modules
	Super::m_numNonTrivialTopModules = 0;
	for (NodeBase::sibling_iterator moduleIt(Super::root()->begin_child()), endIt(Super::root()->end_child());
			moduleIt != endIt; ++moduleIt)
	{
		if (moduleIt->childDegree() != 1)
			++Super::m_numNonTrivialTopModules;
	}

	// For memory networks
	derived().consolidatePhysicalNodes(modules);

	return Super::numActiveModules();
}




#endif /* INFOMAPGREEDYCOMMON_H_ */
