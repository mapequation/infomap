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


#ifndef INFOMAPBASE_H_
#define INFOMAPBASE_H_
#include "TreeData.h"
#include <string>
#include "../io/Config.h"
#include "../utils/MersenneTwister.h"
#include <memory>
#include "../io/SafeFile.h"
#include <limits>
#include "../io/HierarchicalNetwork.h"

struct DepthStat;
class PartitionQueue;

class InfomapBase
{
public:
	InfomapBase(const Config& conf, NodeFactoryBase* nodeFactory)
	:	m_config(conf),
	 	m_rand(conf.seedToRandomNumberGenerator),
	 	m_treeData(nodeFactory),
	 	m_activeNetwork(m_nonLeafActiveNetwork),
	 	m_isCoarseTune(false),
	 	m_iterationCount(0),
	 	m_numNonTrivialTopModules(0),
	 	m_subLevel(0),
	 	m_TOP_LEVEL_ADDITION(1 << 20),
	 	oneLevelCodelength(0.0),
	 	codelength(0.0),
	 	indexCodelength(0.0),
	 	moduleCodelength(0.0),
	 	hierarchicalCodelength(0.0),
		bestHierarchicalCodelength(std::numeric_limits<double>::max()),
	 	bestIntermediateCodelength(std::numeric_limits<double>::max())
	{}

	virtual ~InfomapBase()
	{}

	void run();

	// Cannot be protected as they are called from inherited class through pointer to this class.
	const NodeBase* root() const { return m_treeData.root(); }

	void sortTree();

	virtual void buildHierarchicalNetwork(HierarchicalNetwork& data, bool includeLinks) = 0;

	virtual void printSubInfomapTree(std::ostream& out, const TreeData& originalData, const std::string& prefix = "");
	virtual void printSubInfomapTreeDebug(std::ostream& out, const TreeData& originalData, const std::string& prefix = "");


protected:

	virtual FlowDummy getNodeData(NodeBase& node) = 0;

	/**
	 * Set the exit (and enter) flow on the nodes.
	 *
	 * Note 1: Each node data has a enterFlow and exitFlow member, but
	 * for models with detailed balance (undirected and directed with teleportation)
	 * the enterFlow is equal to the exitFlow and declared as a reference to the exitFlow
	 * to simplify output methods. Thus, don't add to enterFlow for those cases if it should
	 * not add to the exitFlow.
	 *
	 * Note 2: The enter and exit flow defines what should be coded for in the
	 * map equation for the *modular levels*, not the leaf node level.
	 *
	 * The reason for that special case is to be able to code self-flow (self-links and
	 * self-teleportation). If adding self-flow to enter and exit flow though, the enclosing
	 * module will also aggregate those initial values, but self-flow should not be seen
	 * as exiting the enclosing module, just the node.
	 *
	 * Instead of having special cases for the flow aggregation to modular levels, take
	 * the special case when calculating the codelength, using the flow values rather
	 * than the enter/exit flow values for the leaf nodes.
	 *
	 */
	virtual void initEnterExitFlow() = 0;

	virtual void resetModuleFlowFromLeafNodes() = 0;

	/**
	 * Init the infomap term that only depends on the flow value on
	 * the leaf nodes.
	 */
	virtual void initConstantInfomapTerms() = 0;

	/**
	 * Allocate a module for each node, with flow data from the node.
	 */
	virtual void initModuleOptimization() = 0;

	/**
	 * Loop through each node and move it to the module predefined
	 * by the member vector m_moveTo.
	 */
	virtual void moveNodesToPredefinedModules() = 0;

	/**
	 * Loop through each node and move it to the module that reduces
	 * the total codelength the most. Start over until converged.
	 *
	 * @return The number of effective optimization rounds,
	 * i.e. zero if no move was made.
	 */
	virtual unsigned int optimizeModules() = 0;

	/**
	 * Take the non-empty dynamic modules from the optimization of the active network
	 * and create module nodes to insert above the active network in the tree. Also
	 * aggregate the links from the active network to inter-module links in the new
	 * modular network.
	 *
	 * @replaceExistingStructure If true, it doesn't add any depth to the tree but
	 * replacing either the existing modular parent structure (if <code>asSubModules</code>
	 * is true) or the active network itself if it's not the leaf level, in which
	 * case it will add a level of depth to the tree anyway.
	 *
	 * @asSubModules Set to true to consolidate the dynamic modules as submodules
	 * under existing modules, and store existing parent structure on the index
	 * member of the submodules. Presupposes that the active network already have
	 * a modular parent structure, and that the dynamic structure that will be
	 * consolidated actually contains nothing but strict sub-structures of the
	 * existing modules, i.e. that the index property of two nodes with different
	 * parent must be different. Presumably the sub-module structure initiated on
	 * the active network is found by partitioning each existing module.
	 *
	 * @return The number of created modules
	 */
	virtual unsigned int consolidateModules(bool replaceExistingStructure = true, bool asSubModules = false) = 0;

	virtual std::auto_ptr<InfomapBase> getNewInfomapInstance() = 0;

	virtual double calcCodelengthFromFlowWithinOrExit(const NodeBase& parent) = 0;

	virtual double calcCodelengthFromEnterWithinOrExit(const NodeBase& parent) = 0;

	virtual void generateNetworkFromChildren(NodeBase& parent) = 0;

	virtual void transformNodeFlowToEnterFlow(NodeBase* parent) = 0;

	virtual void cloneFlowData(const NodeBase& source, NodeBase& target) = 0;

	virtual void printNodeRanks(std::ostream& out) = 0;

	virtual void printFlowNetwork(std::ostream& out) = 0;

	virtual void printMap(std::ostream& out) = 0;

	virtual void sortTree(NodeBase& parent) = 0;

	NodeBase* root() { return m_treeData.root(); }

	unsigned int numNonTrivialTopModules() { return m_numNonTrivialTopModules; }
	unsigned int numTopModules() { return m_treeData.root()->childDegree(); }

	unsigned int numLeafNodes() { return m_treeData.numLeafNodes(); }

	virtual unsigned int numDynamicModules() = 0;

	bool isTopLevel() { return (m_subLevel & (m_TOP_LEVEL_ADDITION-1)) == 0; }
	bool isSuperLevelOnTopLevel() { return m_subLevel == m_TOP_LEVEL_ADDITION; }

	unsigned long int getSeedFromCodelength(double value)
	{
		return static_cast<unsigned long int>(value/m_config.minimumCodelengthImprovement);
	}

	void reseed(unsigned long int seed) { m_rand.seed(seed); }

private:
	void runPartition();
	double partitionAndQueueNextLevel(PartitionQueue& partitionQueue, bool tryIndexing = true);
	void tryIndexingIteratively();
	unsigned int findSuperModulesIterativelyFast(PartitionQueue& partitionQueue);
	unsigned int deleteSubLevels();
	void queueTopModules(PartitionQueue& partitionQueue);
	bool processPartitionQueue(PartitionQueue& queue, PartitionQueue& nextLevel, bool tryIndexing = true);
	void sortPartitionQueue(PartitionQueue& queue);
	void partition(unsigned int recursiveCount = 0, bool fast = false, bool forceConsolidation = true);
	void mergeAndConsolidateRepeatedly(bool forceConsolidation = false, bool fast = false);
	void generalTune(unsigned int level);
	void fineTune();
	void coarseTune(unsigned int recursiveCount = 0);
	/**
	 * For each module, create a new infomap instance and clone the interior structure of the module
	 * as the new network to partition. Collect the results by populating the index member of each
	 * leaf node with the sub-module structure found by partitioning each module.
	 */
	void partitionEachModule(unsigned int recursiveCount = 0, bool fast = false);
	void initSubNetwork(NodeBase& parent, bool recalculateFlow = false);
	void initSuperNetwork(NodeBase& parent);
	void setActiveNetworkFromChildrenOfRoot();
	void setActiveNetworkFromLeafs();
	void consolidateExternalClusterData();
	bool initNetwork();
	bool checkAndConvertBinaryTree();
	void printNetworkData(std::string filename = "", bool sort = true);
	void printClusterVector(std::ostream& out);
	void printTree(std::ostream& out, const NodeBase& root, const std::string& prefix = "");
	void printPerLevelCodelength(std::ostream& out);
	void aggregatePerLevelCodelength(std::vector<double>& indexCodelengths, std::vector<double>& leafLengths, unsigned int level = 0);
	void aggregatePerLevelCodelength(NodeBase& root, std::vector<double>& indexCodelengths, std::vector<double>& leafLengths, unsigned int level);
	DepthStat calcMaxAndAverageDepth();
	void calcMaxAndAverageDepthHelper(NodeBase& root, unsigned int& maxDepth, double& sumLeafDepth,	unsigned int currentDepth);

	std::vector<NodeBase*> m_nonLeafActiveNetwork;

protected:
	typedef std::vector<NodeBase*>::iterator	activeNetwork_iterator;
	const Config m_config;
	MTRand m_rand;
	TreeData m_treeData;
	std::vector<NodeBase*>& m_activeNetwork; // Points either to m_nonLeafActiveNetwork or m_treeData.m_leafNodes
	std::vector<unsigned int> m_moveTo;
	bool m_isCoarseTune;
	unsigned int m_iterationCount;
	unsigned int m_numNonTrivialTopModules;
	unsigned int m_subLevel;
	const unsigned int m_TOP_LEVEL_ADDITION;
	double oneLevelCodelength;
	double codelength;
	double indexCodelength;
	double moduleCodelength;
	double hierarchicalCodelength;
	double bestHierarchicalCodelength;
	double bestIntermediateCodelength;
	std::ostringstream bestIntermediateStatistics;

};

struct PendingModule
{
	PendingModule() : module(0) {}
	PendingModule(NodeBase* m) : module(m) {}
	NodeBase& operator*() { return *module; }
	NodeBase* module;
};

#include <deque>
class PartitionQueue
{
public:
	typedef std::deque<PendingModule>::size_type size_t;
	PartitionQueue() :
		level(1),
		numNonTrivialModules(0),
		flow(0.0),
		nonTrivialFlow(0.0),
		skip(false),
		indexCodelength(0.0),
		leafCodelength(0.0),
		moduleCodelength(0.0)
	{}

	void swap(PartitionQueue& other)
	{
		std::swap(level, other.level);
		std::swap(numNonTrivialModules, other.numNonTrivialModules);
		std::swap(flow, other.flow);
		std::swap(nonTrivialFlow, other.nonTrivialFlow);
		std::swap(skip, other.skip);
		std::swap(indexCodelength, other.indexCodelength);
		std::swap(leafCodelength, other.leafCodelength);
		std::swap(moduleCodelength, other.moduleCodelength);
		m_queue.swap(other.m_queue);
	}

	size_t size() { return m_queue.size(); }
	void resize(size_t size) { m_queue.resize(size); }
	PendingModule& operator[](size_t i) { return m_queue[i]; }

	unsigned int level;
	unsigned int numNonTrivialModules;
	double flow;
	double nonTrivialFlow;
	bool skip;
	double indexCodelength; // Consolidated
	double leafCodelength; // Consolidated
	double moduleCodelength; // Left to improve on next level

private:
	std::deque<PendingModule> m_queue;
};

struct TwoLevelCodelength
{
	TwoLevelCodelength(double indexLength = 0.0, double moduleLength = 0.0)
	: indexLength(indexLength), moduleLength(moduleLength) {}
	double codelength() { return indexLength + moduleLength; }
	double indexLength;
	double moduleLength;
};

struct DepthStat
{
	DepthStat(unsigned int depth = 0, double aveDepth = 0.0)
	: maxDepth(depth), averageDepth(aveDepth) {}
	unsigned int maxDepth;
	double averageDepth;
};


#endif /* INFOMAPBASE_H_ */
