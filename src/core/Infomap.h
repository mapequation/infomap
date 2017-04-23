/*
 * Infomap.h
 *
 *  Created on: 4 mar 2015
 *      Author: Daniel
 */

#ifndef MODULES_CLUSTERING_CLUSTERING_INFOMAP_H_
#define MODULES_CLUSTERING_CLUSTERING_INFOMAP_H_

#include "InfomapBase.h"
#include <set>

namespace infomap {

template<typename Node, typename Optimizer>
class Infomap: public InfomapBase<Node> {
	using Base = InfomapBase<Node>;
	template<typename InfoNode>
	using ThisInfomapType = Infomap<InfoNode, Optimizer>;

protected:
//	using Base::EdgeType;
//	using EdgeType = Base::EdgeType;
	using EdgeType = Edge<InfoNodeBase>;
public:
	Infomap() : InfomapBase<Node>() {}
	virtual ~Infomap() {}

	// ===================================================
	// IO
	// ===================================================

	virtual std::ostream& toString(std::ostream& out) const;

	// ===================================================
	// Getters
	// ===================================================

	using Base::root;

	using Base::numLeafNodes;

	using Base::numTopModules;

	using Base::numLevels;

	virtual double getCodelength() const;

	virtual double getIndexCodelength() const;

	virtual double getModuleCodelength() const;

protected:

	using Base::get;

	virtual InfomapBase<Node>& getInfomap(InfoNodeBase& node);

	using Base::isMainInfomap;

	virtual unsigned int numActiveModules() const;

	using Base::activeNetwork;

	// ===================================================
	// Run: Init: *
	// ===================================================

	// Init terms that is constant for the whole network
	virtual void initNetwork();

	virtual void initSuperNetwork();

	virtual double calcCodelength(const InfoNodeBase& parent) const;

	// ===================================================
	// Run: Partition: *
	// ===================================================

	virtual void initPartition();

	virtual unsigned int optimizeActiveNetwork();

	virtual void moveActiveNodesToPredifinedModules(std::vector<unsigned int>& modules);

	virtual void consolidateModules(bool replaceExistingModules = true);

	// ===================================================
	// Partition: findTopModulesRepeatedly: *
	// ===================================================

	/**
	 * @inherit
	 */
	virtual bool restoreConsolidatedOptimizationPointIfNoImprovement(bool forceRestore = false);

	// ===================================================
	// Debug: *
	// ===================================================

	virtual void printDebug();

	// ===================================================
	// Protected members
	// ===================================================

	Optimizer m_optimizer;

	using Base::m_root;
	using Base::m_numNonTrivialTopModules;
	using Base::m_oneLevelCodelength;
};

// ===================================================
// IO
// ===================================================

template<typename Node, typename Optimizer>
std::ostream& Infomap<Node, Optimizer>::toString(std::ostream& out) const
{
	return out << m_optimizer;
}

// ===================================================
// Getters
// ===================================================

template<typename Node, typename Optimizer>
inline
InfomapBase<Node>& Infomap<Node, Optimizer>::getInfomap(InfoNodeBase& node) {
	// return get(node).template getInfomap<ThisInfomapType>();
	return get(node).getInfomap();
}


template<typename Node, typename Optimizer>
unsigned int Infomap<Node, Optimizer>::numActiveModules() const
{
	return m_optimizer.numActiveModules();
}

template<typename Node, typename Optimizer>
double Infomap<Node, Optimizer>::getCodelength() const
{
	return m_optimizer.codelength();
}

template<typename Node, typename Optimizer>
double Infomap<Node, Optimizer>::getIndexCodelength() const
{
	return m_optimizer.indexCodelength();
}

template<typename Node, typename Optimizer>
double Infomap<Node, Optimizer>::getModuleCodelength() const
{
	return m_optimizer.moduleCodelength();
}


// ===================================================
// Run: Init: *
// ===================================================

template<typename Node, typename Optimizer>
void Infomap<Node, Optimizer>::initNetwork()
{
	Log(3) << "Infomap::initNetwork()...\n";
	m_optimizer.initNetwork(m_root);

	if (!isMainInfomap())
		m_optimizer.initSubNetwork(m_root);
}

template<typename Node, typename Optimizer>
void Infomap<Node, Optimizer>::initSuperNetwork()
{
	Log(3) << "Infomap::initSuperNetwork()...\n";
	m_optimizer.initSuperNetwork(m_root);
}

template<typename Node, typename Optimizer>
inline
double Infomap<Node, Optimizer>::calcCodelength(const InfoNodeBase& parent) const
{
	return m_optimizer.calcCodelength(parent);
}

// ===================================================
// Run: Partition: *
// ===================================================

template<typename Node, typename Optimizer>
void Infomap<Node, Optimizer>::initPartition()
{
	m_optimizer.initPartition(this->activeNetwork());
	Log(2) << "Initiated to codelength " << *this << " in " << numActiveModules() << " modules." << std::endl;
}


template<typename Node, typename Optimizer>
inline
unsigned int Infomap<Node, Optimizer>::optimizeActiveNetwork()
{
	return m_optimizer.optimizeModules(this->coreLoopLimit, this->minimumCodelengthImprovement);
}

template<typename Node, typename Optimizer>
inline
void Infomap<Node, Optimizer>::moveActiveNodesToPredifinedModules(std::vector<unsigned int>& modules)
{
	m_optimizer.moveActiveNodesToPredifinedModules(modules);
}


template<typename Node, typename Optimizer>
inline
void Infomap<Node, Optimizer>::consolidateModules(bool replaceExistingModules)
{
//	Log(1) << "Consolidate modules to codelength " << m_optimizer << "..." << std::endl;

	auto& network = activeNetwork();
	unsigned int numNodes = network.size();
	std::vector<InfoNodeBase*> modules(numNodes, 0);

	auto& moduleFlowData = m_optimizer.getModuleFlowData();

	InfoNodeBase& firstActiveNode = *network[0];
	unsigned int level = firstActiveNode.depth();
	unsigned int leafLevel = numLevels();

	if (leafLevel == 1)
		replaceExistingModules = false;


	// Release children pointers on current parent(s) to put new modules between
	for (InfoNodeBase* n : network) {
		n->parent->releaseChildren(); // Safe to call multiple times
	}

	// Create the new module nodes and re-parent the active network from its common parent to the new module level
	for (unsigned int i = 0; i < numNodes; ++i)
	{
		InfoNodeBase* node = network[i];
		unsigned int moduleIndex = node->index;
		if (modules[moduleIndex] == 0)
		{
			modules[moduleIndex] = new Node(moduleFlowData[moduleIndex]);
			node->parent->addChild(modules[moduleIndex]);
			modules[moduleIndex]->index = moduleIndex;
		}
		modules[moduleIndex]->addChild(node);
	}


	// Aggregate links from lower level to the new modular level
	typedef std::pair<InfoNodeBase*, InfoNodeBase*> NodePair;
	typedef std::map<NodePair, double> EdgeMap;
	EdgeMap moduleLinks;

	for (auto& node : network)
	{
		InfoNodeBase* parent = node->parent;
		for (auto& e : node->outEdges())
		{
			EdgeType& edge = *e;
			InfoNodeBase* otherParent = edge.target.parent;
			if (otherParent != parent)
			{
				InfoNodeBase *m1 = parent, *m2 = otherParent;
				// If undirected, the order may be swapped to aggregate the edge on an opposite one
				if (!this->directedEdges && m1->index > m2->index)
					std::swap(m1, m2);
				// Insert the node pair in the edge map. If not inserted, add the flow value to existing node pair.
				std::pair<EdgeMap::iterator, bool> ret = \
						moduleLinks.insert(std::make_pair(NodePair(m1, m2), edge.data.flow));
				if (!ret.second)
					ret.first->second += edge.data.flow;
			}
		}
	}

	// Add the aggregated edge flow structure to the new modules
	for (auto& e : moduleLinks)
	{
		const NodePair& nodePair = e.first;
		nodePair.first->addOutEdge(*nodePair.second, 0.0, e.second);
	}

	if (replaceExistingModules)
	{
		if (level == 1) {
			Log(4) << "Consolidated super modules, removing old modules..." << std::endl;
			for (auto& node : network)
				node->replaceWithChildren();
		}
		else if (level == 2) {
			Log(4) << "Consolidated sub-modules, removing modules..." << std::endl;
			unsigned int moduleIndex = 0;
			for (InfoNodeBase& module : root()) {
				// Store current modular structure on the sub-modules
				for (auto& subModule : module)
					subModule.index = moduleIndex;
				++moduleIndex;
			}
			root().replaceChildrenWithGrandChildren();
		}
	}

	// Calculate the number of non-trivial modules
	m_numNonTrivialTopModules = 0;
	for (auto& module : m_root)
	{
		if (module.childDegree() != 1)
			++m_numNonTrivialTopModules;
	}

	m_optimizer.consolidateModules(modules);
}


// ===================================================
// Partition: findTopModulesRepeatedly: *
// ===================================================

template<typename Node, typename Optimizer>
inline
bool Infomap<Node, Optimizer>::restoreConsolidatedOptimizationPointIfNoImprovement(bool forceRestore)
{
	return m_optimizer.restoreObjectiveToConsolidatedStateIfNoImprovement(forceRestore);
}

// ===================================================
// Debug: *
// ===================================================

template<typename Node, typename Optimizer>
inline
void Infomap<Node, Optimizer>::printDebug()
{
	m_optimizer.printDebug();
}


} /* namespace ClusteringModule */

#endif /* MODULES_CLUSTERING_CLUSTERING_INFOMAP_H_ */
