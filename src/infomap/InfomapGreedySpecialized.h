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


#ifndef INFOMAPGREEDYSPECIALIZED_H_
#define INFOMAPGREEDYSPECIALIZED_H_
#include "InfomapGreedy.h"
#include "flowData.h"
#include <map>

#ifdef NS_INFOMAP
namespace infomap
{
#endif

/**
 * Infomap methods specialized on the flow type, e.g. including teleportation flow if coding teleportation.
 * As methods can't be partially specialized, the network type template variable is dropped, so
 * this class don't know if it is a memory network or not.
 */
template<typename FlowType>
class InfomapGreedySpecialized : public InfomapGreedy<InfomapGreedySpecialized<FlowType> >
{
	friend class InfomapGreedy<InfomapGreedySpecialized<FlowType> >;
	typedef InfomapGreedySpecialized<FlowType>							SelfType;
	typedef InfomapGreedy<SelfType>										Super;
	typedef typename flowData_traits<FlowType>::detailed_balance_type 	DetailedBalanceType;
	typedef typename flowData_traits<FlowType>::directed_with_recorded_teleportation_type DirectedWithRecordedTeleportationType;
	typedef typename flowData_traits<FlowType>::teleportation_type 		TeleportationType;
	typedef Node<FlowType>												NodeType;
	typedef Edge<NodeBase>												EdgeType;
public:

	InfomapGreedySpecialized(const Config& conf, NodeFactoryBase* nodeFactory) :
		InfomapGreedy<SelfType>(conf, nodeFactory),
		m_sumDanglingFlow(0.0) {}
	InfomapGreedySpecialized(const InfomapBase& infomap, NodeFactoryBase* nodeFactory) :
		InfomapGreedy<SelfType>(infomap, nodeFactory),
		m_sumDanglingFlow(0.0) {}
	virtual ~InfomapGreedySpecialized() {}

protected:
	void addTeleportationFlowOnLeafNodes() {}
	void addTeleportationFlowOnModules() {}

	virtual void initEnterExitFlow();

	void addTeleportationDeltaFlowOnOldModuleIfMove(NodeType& nodeToMove, DeltaFlow& oldModuleDeltaFlow) {}
	void addTeleportationDeltaFlowOnNewModuleIfMove(NodeType& nodeToMove, DeltaFlow& newModuleDeltaFlow) {}

	template<typename DeltaFlowType>
	void addTeleportationDeltaFlowIfMove(NodeType& current, std::vector<DeltaFlowType>& moduleDeltaExits, unsigned int numModuleLinks) {}
	template<typename DeltaFlowType>
	void addTeleportationDeltaFlowIfMove(NodeType& current, std::map<unsigned int, DeltaFlowType>& moduleDeltaFlow) {}

	double getDeltaCodelengthOnMovingNode(NodeType& current, DeltaFlow& oldModuleDelta, DeltaFlow& newModuleDelta);
	void updateCodelengthOnMovingNode(NodeType& current, DeltaFlow& oldModuleDelta, DeltaFlow& newModuleDelta);

	void updateFlowOnMovingNode(NodeType& current, DeltaFlow& oldModuleDelta, DeltaFlow& newModuleDelta);

	double m_sumDanglingFlow;
};


template<>
inline
void InfomapGreedySpecialized<FlowDirectedWithTeleportation>::addTeleportationFlowOnLeafNodes()
{
	double alpha = m_config.teleportationProbability;
	double beta = 1.0 - alpha;
	for (TreeData::leafIterator it(m_treeData.begin_leaf()), itEnd(m_treeData.end_leaf());
			it != itEnd; ++it)
	{
		NodeType& node = getNode(**it);
		// Don't let self-teleportation add to the enter/exit flow (i.e. multiply with (1.0 - node.data.teleportWeight))
		node.data.exitFlow += (alpha * node.data.flow + beta * node.data.danglingFlow) * (1.0 - node.data.teleportWeight);
		node.data.enterFlow += (alpha * (1.0 - node.data.flow) + beta * (m_sumDanglingFlow - node.data.danglingFlow)) * node.data.teleportWeight;
	}
}

template<>
inline
void InfomapGreedySpecialized<FlowDirectedWithTeleportation>::addTeleportationFlowOnModules()
{
	double alpha = m_config.teleportationProbability;
	double beta = 1.0 - alpha;

	for (NodeBase::pre_depth_first_iterator it(root()); !it.isEnd(); ++it)
	{
		NodeType& node = getNode(*it);
		if (!node.isLeaf())
		{
			// Don't code self-teleportation
			node.data.enterFlow += (alpha * (1.0 - node.data.flow) + beta * (m_sumDanglingFlow - node.data.danglingFlow)) * node.data.teleportWeight;
			node.data.exitFlow += (alpha * node.data.flow + beta * node.data.danglingFlow) * (1.0 - node.data.teleportWeight);
		}
	}
}

template<typename FlowType>
inline
void InfomapGreedySpecialized<FlowType>::initEnterExitFlow()
{
	for (TreeData::leafIterator it(Super::m_treeData.begin_leaf()), itEnd(Super::m_treeData.end_leaf());
			it != itEnd; ++it)
	{
		NodeBase& node = **it;
		for (NodeBase::edge_iterator edgeIt(node.begin_outEdge()), edgeEnd(node.end_outEdge());
				edgeIt != edgeEnd; ++edgeIt)
		{
			EdgeType& edge = **edgeIt;
			// Possible self-links should not add to enter and exit flow in its enclosing module
			if (edge.source != edge.target)
			{
				// For undirected links, this automatically adds to both direction (as enterFlow = &exitFlow)
				Super::getNode(edge.source).data.exitFlow += edge.data.flow;
				Super::getNode(edge.target).data.enterFlow += edge.data.flow;
			}
		}
	}

}

/**
 * Specialization for directed with teleportation.
 */
template<>
inline
void InfomapGreedySpecialized<FlowDirectedWithTeleportation>::initEnterExitFlow()
{
	for (TreeData::leafIterator it(m_treeData.begin_leaf()), itEnd(m_treeData.end_leaf());
			it != itEnd; ++it)
	{
		NodeBase& node = **it;
		FlowType& data = getNode(node).data;
		// Also store the flow to use for teleportation as the flow can be transformed
		data.teleportSourceFlow = data.flow;
		if (node.isDangling())
		{
			m_sumDanglingFlow += data.flow;
			data.danglingFlow = data.flow;
		}
		else
		{
			for (NodeBase::edge_iterator edgeIt(node.begin_outEdge()), edgeEnd(node.end_outEdge());
					edgeIt != edgeEnd; ++edgeIt)
			{
				EdgeType& edge = **edgeIt;
				// Possible self-links should not add to enter and exit flow in its enclosing module
				if (edge.source != edge.target)
				{
					getNode(edge.source).data.exitFlow += edge.data.flow;
					getNode(edge.target).data.enterFlow += edge.data.flow;
				}
			}
		}
	}

	addTeleportationFlowOnLeafNodes();
}

template<>
inline
void InfomapGreedySpecialized<FlowDirectedWithTeleportation>::addTeleportationDeltaFlowOnOldModuleIfMove(NodeType& nodeToMove, DeltaFlow& oldModuleDeltaFlow)
{
	double alpha = m_config.teleportationProbability;
	double beta = 1.0 - alpha;
	FlowType& oldModuleFlowData = m_moduleFlowData[oldModuleDeltaFlow.module];
	oldModuleDeltaFlow.deltaExit += (alpha*nodeToMove.data.teleportSourceFlow + beta*nodeToMove.data.danglingFlow) * (oldModuleFlowData.teleportWeight - nodeToMove.data.teleportWeight);
	oldModuleDeltaFlow.deltaEnter += (alpha*(oldModuleFlowData.teleportSourceFlow - nodeToMove.data.teleportSourceFlow) +
			beta*(oldModuleFlowData.danglingFlow - nodeToMove.data.danglingFlow)) * nodeToMove.data.teleportWeight;
}

template<>
inline
void InfomapGreedySpecialized<FlowDirectedWithTeleportation>::addTeleportationDeltaFlowOnNewModuleIfMove(NodeType& nodeToMove, DeltaFlow& newModuleDeltaFlow)
{
	double alpha = m_config.teleportationProbability;
	double beta = 1.0 - alpha;
	FlowType& newModuleFlowData = m_moduleFlowData[newModuleDeltaFlow.module];
	newModuleDeltaFlow.deltaExit += (alpha*nodeToMove.data.teleportSourceFlow + beta*nodeToMove.data.danglingFlow) * newModuleFlowData.teleportWeight;
	newModuleDeltaFlow.deltaEnter += (alpha*newModuleFlowData.teleportSourceFlow +	beta*newModuleFlowData.danglingFlow) * nodeToMove.data.teleportWeight;
}

template<>
template<typename DeltaFlowType>
inline
void InfomapGreedySpecialized<FlowDirectedWithTeleportation>::addTeleportationDeltaFlowIfMove(NodeType& current, std::vector<DeltaFlowType>& moduleDeltaExits, unsigned int numModuleLinks)
{
	for (unsigned int j = 0; j < numModuleLinks; ++j)
	{
		unsigned int moduleIndex = moduleDeltaExits[j].module;
		if (moduleIndex == current.index)
			addTeleportationDeltaFlowOnOldModuleIfMove(current, moduleDeltaExits[j]);
		else
			addTeleportationDeltaFlowOnNewModuleIfMove(current, moduleDeltaExits[j]);
	}
}

template<>
template<typename DeltaFlowType>
inline
void InfomapGreedySpecialized<FlowDirectedWithTeleportation>::addTeleportationDeltaFlowIfMove(NodeType& current, std::map<unsigned int, DeltaFlowType>& moduleDeltaFlow)
{
	for (typename std::map<unsigned int, DeltaFlowType>::iterator it(moduleDeltaFlow.begin()); it != moduleDeltaFlow.end(); ++it)
	{
		unsigned int moduleIndex = it->first;
		if (moduleIndex == current.index)
			addTeleportationDeltaFlowOnOldModuleIfMove(current, it->second);
		else
			addTeleportationDeltaFlowOnNewModuleIfMove(current, it->second);
	}
}



template<typename FlowType>
inline
double InfomapGreedySpecialized<FlowType>::getDeltaCodelengthOnMovingNode(NodeType& current,
		DeltaFlow& oldModuleDelta, DeltaFlow& newModuleDelta)
{
	using infomath::plogp;
	std::vector<FlowType>& moduleFlowData = Super::m_moduleFlowData;
	unsigned int oldModule = oldModuleDelta.module;
	unsigned int newModule = newModuleDelta.module;
	double deltaEnterExitOldModule = oldModuleDelta.deltaEnter + oldModuleDelta.deltaExit;
	double deltaEnterExitNewModule = newModuleDelta.deltaEnter + newModuleDelta.deltaExit;

	double delta_enter = plogp(Super::enterFlow + deltaEnterExitOldModule - deltaEnterExitNewModule) - Super::enterFlow_log_enterFlow;

	double delta_enter_log_enter = \
			- plogp(moduleFlowData[oldModule].enterFlow) \
			- plogp(moduleFlowData[newModule].enterFlow) \
			+ plogp(moduleFlowData[oldModule].enterFlow - current.data.enterFlow + deltaEnterExitOldModule) \
			+ plogp(moduleFlowData[newModule].enterFlow + current.data.enterFlow - deltaEnterExitNewModule);

	double delta_exit_log_exit = \
			- plogp(moduleFlowData[oldModule].exitFlow) \
			- plogp(moduleFlowData[newModule].exitFlow) \
			+ plogp(moduleFlowData[oldModule].exitFlow - current.data.exitFlow + deltaEnterExitOldModule) \
			+ plogp(moduleFlowData[newModule].exitFlow + current.data.exitFlow - deltaEnterExitNewModule);

	double delta_flow_log_flow = \
			- plogp(moduleFlowData[oldModule].exitFlow + moduleFlowData[oldModule].flow) \
			- plogp(moduleFlowData[newModule].exitFlow + moduleFlowData[newModule].flow) \
			+ plogp(moduleFlowData[oldModule].exitFlow + moduleFlowData[oldModule].flow \
					- current.data.exitFlow - current.data.flow + deltaEnterExitOldModule) \
			+ plogp(moduleFlowData[newModule].exitFlow + moduleFlowData[newModule].flow \
					+ current.data.exitFlow + current.data.flow - deltaEnterExitNewModule);

	double deltaL = delta_enter - delta_enter_log_enter - delta_exit_log_exit + delta_flow_log_flow;
	return deltaL;
}

template<>
inline
double InfomapGreedySpecialized<FlowUndirected>::getDeltaCodelengthOnMovingNode(NodeType& current,
		DeltaFlow& oldModuleDelta, DeltaFlow& newModuleDelta)
{
	using infomath::plogp;
	std::vector<FlowType>& moduleFlowData = Super::m_moduleFlowData;
	unsigned int oldModule = oldModuleDelta.module;
	unsigned int newModule = newModuleDelta.module;
	double deltaEnterExitOldModule = oldModuleDelta.deltaEnter + oldModuleDelta.deltaExit;
	double deltaEnterExitNewModule = newModuleDelta.deltaEnter + newModuleDelta.deltaExit;

	// Double the effect as each link works in both directions
	deltaEnterExitOldModule *= 2;
	deltaEnterExitNewModule *= 2;

	double delta_exit = plogp(enterFlow + deltaEnterExitOldModule - deltaEnterExitNewModule) - enterFlow_log_enterFlow;

	double delta_exit_log_exit = \
			- plogp(moduleFlowData[oldModule].exitFlow) \
			- plogp(moduleFlowData[newModule].exitFlow) \
			+ plogp(moduleFlowData[oldModule].exitFlow - current.data.exitFlow + deltaEnterExitOldModule) \
			+ plogp(moduleFlowData[newModule].exitFlow + current.data.exitFlow - deltaEnterExitNewModule);

	double delta_flow_log_flow = \
			- plogp(moduleFlowData[oldModule].exitFlow + moduleFlowData[oldModule].flow) \
			- plogp(moduleFlowData[newModule].exitFlow + moduleFlowData[newModule].flow) \
			+ plogp(moduleFlowData[oldModule].exitFlow + moduleFlowData[oldModule].flow \
					- current.data.exitFlow - current.data.flow + deltaEnterExitOldModule) \
			+ plogp(moduleFlowData[newModule].exitFlow + moduleFlowData[newModule].flow \
					+ current.data.exitFlow + current.data.flow - deltaEnterExitNewModule);

	double deltaL = delta_exit - 2.0*delta_exit_log_exit + delta_flow_log_flow;
	return deltaL;
}


/**
 * Update the codelength to reflect the move of node current
 * in oldModuleDelta to newModuleDelta
 * (Specialized for undirected flow and when exitFlow == enterFlow)
 */
template<typename FlowType>
inline
void InfomapGreedySpecialized<FlowType>::updateCodelengthOnMovingNode(NodeType& current,
		DeltaFlow& oldModuleDelta, DeltaFlow& newModuleDelta)
{
	using infomath::plogp;
	std::vector<FlowType>& moduleFlowData = Super::m_moduleFlowData;
	unsigned int oldModule = oldModuleDelta.module;
	unsigned int newModule = newModuleDelta.module;
	double deltaEnterExitOldModule = oldModuleDelta.deltaEnter + oldModuleDelta.deltaExit;
	double deltaEnterExitNewModule = newModuleDelta.deltaEnter + newModuleDelta.deltaExit;

	Super::enterFlow -= \
			moduleFlowData[oldModule].enterFlow + \
			moduleFlowData[newModule].enterFlow;
	Super::enter_log_enter -= \
			plogp(moduleFlowData[oldModule].enterFlow) + \
			plogp(moduleFlowData[newModule].enterFlow);
	Super::exit_log_exit -= \
			plogp(moduleFlowData[oldModule].exitFlow) + \
			plogp(moduleFlowData[newModule].exitFlow);
	Super::flow_log_flow -= \
			plogp(moduleFlowData[oldModule].exitFlow + moduleFlowData[oldModule].flow) + \
			plogp(moduleFlowData[newModule].exitFlow + moduleFlowData[newModule].flow);


	moduleFlowData[oldModule] -= current.data;
	moduleFlowData[newModule] += current.data;

	moduleFlowData[oldModule].enterFlow += deltaEnterExitOldModule;
	moduleFlowData[oldModule].exitFlow += deltaEnterExitOldModule;
	moduleFlowData[newModule].enterFlow -= deltaEnterExitNewModule;
	moduleFlowData[newModule].exitFlow -= deltaEnterExitNewModule;

	Super::enterFlow += \
			moduleFlowData[oldModule].enterFlow + \
			moduleFlowData[newModule].enterFlow;
	Super::enter_log_enter += \
			plogp(moduleFlowData[oldModule].enterFlow) + \
			plogp(moduleFlowData[newModule].enterFlow);
	Super::exit_log_exit += \
			plogp(moduleFlowData[oldModule].exitFlow) + \
			plogp(moduleFlowData[newModule].exitFlow);
	Super::flow_log_flow += \
			plogp(moduleFlowData[oldModule].exitFlow + moduleFlowData[oldModule].flow) + \
			plogp(moduleFlowData[newModule].exitFlow + moduleFlowData[newModule].flow);

	Super::enterFlow_log_enterFlow = plogp(Super::enterFlow);

	Super::indexCodelength = Super::enterFlow_log_enterFlow - Super::enter_log_enter - Super::exitNetworkFlow_log_exitNetworkFlow;
	Super::moduleCodelength = -Super::exit_log_exit + Super::flow_log_flow - Super::nodeFlow_log_nodeFlow;
	Super::codelength = Super::indexCodelength + Super::moduleCodelength;
}

template<>
inline
void InfomapGreedySpecialized<FlowUndirected>::updateCodelengthOnMovingNode(NodeType& current,
		DeltaFlow& oldModuleDelta, DeltaFlow& newModuleDelta)
{
	using infomath::plogp;
	std::vector<FlowType>& moduleFlowData = Super::m_moduleFlowData;
	unsigned int oldModule = oldModuleDelta.module;
	unsigned int newModule = newModuleDelta.module;
	double deltaEnterExitOldModule = oldModuleDelta.deltaEnter + oldModuleDelta.deltaExit;
	double deltaEnterExitNewModule = newModuleDelta.deltaEnter + newModuleDelta.deltaExit;

	// Double the effect as each link works in both directions
	deltaEnterExitOldModule *= 2;
	deltaEnterExitNewModule *= 2;

	enterFlow -= \
			moduleFlowData[oldModule].enterFlow + \
			moduleFlowData[newModule].enterFlow;
	exit_log_exit -= \
			plogp(moduleFlowData[oldModule].exitFlow) + \
			plogp(moduleFlowData[newModule].exitFlow);
	flow_log_flow -= \
			plogp(moduleFlowData[oldModule].exitFlow + moduleFlowData[oldModule].flow) + \
			plogp(moduleFlowData[newModule].exitFlow + moduleFlowData[newModule].flow);


	moduleFlowData[oldModule] -= current.data;
	moduleFlowData[newModule] += current.data;

	moduleFlowData[oldModule].exitFlow += deltaEnterExitOldModule;
	moduleFlowData[newModule].exitFlow -= deltaEnterExitNewModule;

	enterFlow += \
			moduleFlowData[oldModule].enterFlow + \
			moduleFlowData[newModule].enterFlow;
	exit_log_exit += \
			plogp(moduleFlowData[oldModule].exitFlow) + \
			plogp(moduleFlowData[newModule].exitFlow);
	flow_log_flow += \
			plogp(moduleFlowData[oldModule].exitFlow + moduleFlowData[oldModule].flow) + \
			plogp(moduleFlowData[newModule].exitFlow + moduleFlowData[newModule].flow);

	enterFlow_log_enterFlow = plogp(enterFlow);

	indexCodelength = enterFlow_log_enterFlow - exit_log_exit - exitNetworkFlow_log_exitNetworkFlow;
	moduleCodelength = -exit_log_exit + flow_log_flow - nodeFlow_log_nodeFlow;
	codelength = indexCodelength + moduleCodelength;
}

template<typename FlowType>
inline
void InfomapGreedySpecialized<FlowType>::updateFlowOnMovingNode(NodeType& current,
		DeltaFlow& oldModuleDelta, DeltaFlow& newModuleDelta)
{
	std::vector<FlowType>& moduleFlowData = Super::m_moduleFlowData;
	unsigned int oldModule = oldModuleDelta.module;
	unsigned int newModule = newModuleDelta.module;
	double deltaEnterExitOldModule = oldModuleDelta.deltaEnter + oldModuleDelta.deltaExit;
	double deltaEnterExitNewModule = newModuleDelta.deltaEnter + newModuleDelta.deltaExit;

	moduleFlowData[oldModule] -= current.data;
	moduleFlowData[newModule] += current.data;

	moduleFlowData[oldModule].exitFlow += deltaEnterExitOldModule;
	moduleFlowData[newModule].exitFlow -= deltaEnterExitNewModule;
}

template<>
inline
void InfomapGreedySpecialized<FlowUndirected>::updateFlowOnMovingNode(NodeType& current,
		DeltaFlow& oldModuleDelta, DeltaFlow& newModuleDelta)
{
	std::vector<FlowType>& moduleFlowData = Super::m_moduleFlowData;
	unsigned int oldModule = oldModuleDelta.module;
	unsigned int newModule = newModuleDelta.module;
	double deltaEnterExitOldModule = oldModuleDelta.deltaEnter + oldModuleDelta.deltaExit;
	double deltaEnterExitNewModule = newModuleDelta.deltaEnter + newModuleDelta.deltaExit;

	// Double the effect as each link works in both directions
	deltaEnterExitOldModule *= 2;
	deltaEnterExitNewModule *= 2;

	moduleFlowData[oldModule] -= current.data;
	moduleFlowData[newModule] += current.data;

	moduleFlowData[oldModule].exitFlow += deltaEnterExitOldModule;
	moduleFlowData[newModule].exitFlow -= deltaEnterExitNewModule;
}

#ifdef NS_INFOMAP
}
#endif

#endif /* INFOMAPGREEDYSPECIALIZED_H_ */
