/*
 * InfomapBase.h
 *
 *  Created on: 23 feb 2015
 *      Author: Daniel
 */

#ifndef SRC_CLUSTERING_INFOMAPBASE_H_
#define SRC_CLUSTERING_INFOMAPBASE_H_

#include "InfomapConfig.h"
#include <string>
#include <iostream>
#include <sstream>
#include "InfoEdge.h"
#include <vector>
#include <deque>
#include "../utils/Log.h"
#include <limits>
#include "PartitionQueue.h"
#include <limits>
// #include "StateNetwork.h"
#include "../io/Network.h"
#include "InfoNode.h"
#include "InfomapIterator.h"

namespace infomap {

struct PerLevelStat;
// class InfoNode;

//template<typename Node, template<typename,typename> class Optimizer = GreedyOptimizer<Node, MapEquation>>
class InfomapBase : public InfomapConfig<InfomapBase>
{
	template<typename Objective>
	friend class InfomapOptimizer;
protected:
	using EdgeType = Edge<InfoNode>;

public:
	
	InfomapBase() : InfomapConfig<InfomapBase>() {}

	InfomapBase(const Config& conf) :
		InfomapConfig<InfomapBase>(conf),
		m_network(conf)
	{}

	InfomapBase(const std::string flags) :
		InfomapConfig<InfomapBase>(flags)
	{
		m_network.setConfig(*this);
	}
	
	virtual ~InfomapBase() {}


	// ===================================================
	// Getters
	// ===================================================

	const Network& network() const;

	Network& network();
	
	InfoNode& root();
	const InfoNode& root() const;

	// InfomapIterator tree(int maxClusterLevel = std::numeric_limits<unsigned int>::max())
	// { return InfomapIterator(&root(), maxClusterLevel); }

	InfomapIterator iterTree(int maxClusterLevel = -1)
	{ return InfomapIterator(&root(), maxClusterLevel); }

	InfomapIteratorPhysical iterTreePhysical(int maxClusterLevel = -1)
	{ return InfomapIteratorPhysical(&root(), maxClusterLevel); }
	
	InfomapModuleIterator iterModules(int maxClusterLevel = -1)
	{ return InfomapModuleIterator(&root(), maxClusterLevel); }
	
	InfomapLeafModuleIterator iterLeafModules(int maxClusterLevel = -1)
	{ return InfomapLeafModuleIterator(&root(), maxClusterLevel); }
	
	InfomapLeafIterator iterLeafNodes(int maxClusterLevel = -1)
	{ return InfomapLeafIterator(&root(), maxClusterLevel); }
	
	InfomapLeafIteratorPhysical iterLeafNodesPhysical(int maxClusterLevel = -1)
	{ return InfomapLeafIteratorPhysical(&root(), maxClusterLevel); }

	InfomapIterator begin(int maxClusterLevel = -1)
	{ return InfomapIterator(&root(), maxClusterLevel); }

	InfomapIterator end()
	{ return InfomapIterator(nullptr); }

	unsigned int numLeafNodes() const;

	const std::vector<InfoNode*>& leafNodes() const;

	unsigned int numTopModules() const;

	virtual unsigned int numActiveModules() const = 0;

	unsigned int numNonTrivialTopModules() const;

	bool haveModules() const;

	bool haveNonTrivialModules() const;

	/**
	 * Number of node levels below the root in current Infomap instance, 1 if no modules
	 */
	unsigned int numLevels() const;

	/**
	 * Get maximum depth of any child in the tree, following possible sub Infomap instances
	 */
	unsigned int maxTreeDepth() const;

	virtual double getCodelength() const = 0;

	virtual double codelength() { return m_hierarchicalCodelength; }

	virtual double getIndexCodelength() const = 0;

	virtual double getModuleCodelength() const = 0;

	double getHierarchicalCodelength() const;

	double getOneLevelCodelength() const { return m_oneLevelCodelength; }

	bool isFullNetwork() { return m_isMain && m_aggregationLevel == 0; }
	bool isFirstLoop() { return m_tuneIterationIndex == 0 && isFullNetwork(); }

	// virtual InfomapBase& getInfomap(InfoNode& node);

	virtual InfomapBase* getNewInfomapInstance() const = 0;
	virtual InfomapBase* getNewInfomapInstanceWithoutMemory() const = 0;

	InfomapBase& getSubInfomap(InfoNode& node);
	InfomapBase& getSuperInfomap(InfoNode& node);

	/**
	* Only the main infomap reads an external cluster file if exist
	*/
	InfomapBase& setIsMain(bool isMain);
	InfomapBase& setSubLevel(unsigned int level);

	bool isTopLevel() const;
	bool isSuperLevelOnTopLevel() const;
	bool isMainInfomap() const;

	virtual bool haveMemory() const {
		return this->isMemoryNetwork();
	}

	bool haveHardPartition() const;

	std::vector<InfoNode*>& activeNetwork() const;

	// ===================================================
	// IO
	// ===================================================

	virtual std::ostream& toString(std::ostream& out) const;


	// ===================================================
	// Run
	// ===================================================

	virtual void run();

	virtual void run(const std::map<unsigned int, unsigned int>& clusterIds);

	virtual void run(Network& network);

	virtual void run(Network& network, const std::map<unsigned int, unsigned int>& clusterIds);

	// ===================================================
	// Run: *
	// ===================================================

	InfomapBase& initNetwork(Network& network);
	InfomapBase& initNetwork(InfoNode& parent, bool asSuperNetwork = false);


	void generateSubNetwork(Network& network);
	virtual void generateSubNetwork(InfoNode& parent);

	/**
	 * Init categorical meta data on all nodes from a file with the following format:
	 * # nodeId metaData
	 * 1 1
	 * 2 1
	 * 3 2
	 * 4 2
	 * 5 3
	 * 
	 */
	InfomapBase& initMetaData(std::string metaDataFile);

	/**
	 * Provide an initial partition of the network.
	 *
	 * @param clusterDataFile A .clu file containing cluster data.
	 * @param hard If true, the provided clusters will not be splitted. This reduces the
	 * effective network size during the optimization phase but the hard partitions are
	 * after that replaced by the original nodes.
	 */
	InfomapBase& initPartition(std::string clusterDataFile, bool hard = false);

	/**
	 * Provide an initial partition of the network.
	 *
	 * @param clusterIds map from nodeId to clusterId, doesn't have to be complete
	 * @param hard If true, the provided clusters will not be splitted. This reduces the
	 * effective network size during the optimization phase but the hard partitions are
	 * after that replaced by the original nodes.
	 */
	InfomapBase& initPartition(const std::map<unsigned int, unsigned int>& clusterIds, bool hard = false);

	/**
	 * Provide an initial partition of the network.
	 *
	 * @param clusters Each sub-vector contain node IDs for all nodes that should be merged.
	 * @param hard If true, the provided clusters will not be splitted. This reduces the
	 * effective network size during the optimization phase but the hard partitions are
	 * after that replaced by the original nodes.
	 */
	InfomapBase& initPartition(std::vector<std::vector<unsigned int>>& clusters, bool hard = false);

	/**
	 * Provide an initial partition of the network.
	 *
	 * @param modules Module indices for each node
	 */
	InfomapBase& initPartition(std::vector<unsigned int>& modules, bool hard = false);

	virtual void init();

	virtual void runPartition();

	virtual void restoreHardPartition();

	virtual void writeResult();

	// ===================================================
	// runPartition: *
	// ===================================================

	virtual void hierarchicalPartition();

	virtual void partition();


	// ===================================================
	// runPartition: init: *
	// ===================================================

	/**
	* Done in network?
	*/
	virtual void initEnterExitFlow();

	// Init terms that is constant for the whole network
	virtual void initNetwork() = 0;

	virtual void initSuperNetwork() = 0;

	virtual double calcCodelength(const InfoNode& parent) const = 0;

	/**
	 * Calculate and store codelength on all modules in the tree
	 * @param includeRoot Also calculate the codelength on the root node
	 * @return the hierarchical codelength
	 */
	virtual double calcCodelengthOnTree(bool includeRoot = true);

	/**
	 * Partition layer by layer and
	 */
	// virtual void preClusterMultilayerNetwork();


	// ===================================================
	// Run: Partition: *
	// ===================================================

	virtual void setActiveNetworkFromLeafs();

	virtual void setActiveNetworkFromChildrenOfRoot();

	virtual void initPartition() = 0;

	virtual void findTopModulesRepeatedly(unsigned int maxLevels);

	virtual unsigned int fineTune();

	virtual unsigned int coarseTune();

	/**
	 * Return the number of effective core loops, i.e. not the last if not at coreLoopLimit
	 */
	virtual unsigned int optimizeActiveNetwork() = 0;

	virtual void moveActiveNodesToPredifinedModules(std::vector<unsigned int>& modules) = 0;

	virtual void consolidateModules(bool replaceExistingModules = true) = 0;

	void calculateNumNonTrivialTopModules();

	unsigned int calculateMaxDepth();

	// ===================================================
	// Partition: findTopModulesRepeatedly: *
	// ===================================================

	/**
	 * Return true if restored to consolidated optimization state
	 */
	virtual bool restoreConsolidatedOptimizationPointIfNoImprovement(bool forceRestore = false) = 0;


	// ===================================================
	// Run: HierarcicalPartition: *
	// ===================================================

	/**
	 * Find super modules applying the whole two-level algorithm on the
	 * top modules iteratively
	 * @param levelLimit The maximum number of super module levels allowed
	 * @return number of levels created
	 */
	virtual unsigned int findHierarchicalSuperModules(unsigned int superLevelLimit = std::numeric_limits<unsigned int>::max());

	/**
	 * Find super modules fast by merge and consolidate top modules iteratively
	 * @param levelLimit The maximum number of super module levels allowed
	 * @return number of levels created
	 */
	virtual unsigned int findHierarchicalSuperModulesFast(unsigned int superLevelLimit = std::numeric_limits<unsigned int>::max());

	virtual void transformNodeFlowToEnterFlow(InfoNode& parent);

	virtual void resetFlowOnModules();

	virtual unsigned int removeModules();

	virtual unsigned int removeSubModules(bool recalculateCodelengthOnTree);

	virtual unsigned int recursivePartition();

	void queueTopModules(PartitionQueue& partitionQueue);

	void queueLeafModules(PartitionQueue& partitionQueue);

	bool processPartitionQueue(PartitionQueue& queue, PartitionQueue& nextLevel);

	// ===================================================
	// Output: *
	// ===================================================

	/**
	 * Write tree to a .tree file.
	 * @param filename the filename for the output file. If empty, use default 
	 * based on output directory and input file name
	 * @param states if memory network, print the state-level network without merging physical nodes within modules
	 * @return the filename written to
	 */
	std::string writeTree(std::string filename = "", bool states = false);

	/**
	 * Write flow tree to a .ftree file.
	 * This is the same as a .tree file but appended with links aggregated
	 * within modules on all levels in the tree
	 * @param filename the filename for the output file. If empty, use default 
	 * based on output directory and input file name
	 * @param states if memory network, print the state-level network without merging physical nodes within modules
	 * @return the filename written to
	 */
	std::string writeFlowTree(std::string filename = "", bool states = false);

	/**
	 * Write tree to a .clu file.
	 * @param filename the filename for the output file. If empty, use default 
	 * based on output directory and input file name
	 * @param states if memory network, print the state-level network without merging physical nodes within modules
	 * @param moduleIndexLevel the depth from the root on which to advance module index.
	 * Value 1 will give the module index on the coarsest level, 2 the level below and so on. Default
	 * value -1 will give the module index for the lowest level, i.e. the finest modular structure.
	 * @return the filename written to
	 */
	std::string writeClu(std::string filename = "", bool states = false, int moduleIndexLevel = -1);

	/**
	 * Write modular network to a .map file.
	 * @param filename the filename for the output file. If empty, use default 
	 * based on output directory and input file name
	 * @param states if memory network, print the state-level network without merging physical nodes within modules
	 * @param moduleIndexLevel the depth from the root on which to advance module index.
	 * Value 1 will give the module index on the coarsest level, 2 the level below and so on. Default
	 * value -1 will give the module index for the lowest level, i.e. the finest modular structure.
	 * @return the filename written to
	 */
	std::string writeMap(std::string filename = "", bool states = false, int moduleIndexLevel = -1);

	/**
	* Print per level statistics
	* @param out The output stream to print the per level statistics to
	*/
	unsigned int printPerLevelCodelength(std::ostream& out);

	void aggregatePerLevelCodelength(std::vector<PerLevelStat>& perLevelStat, unsigned int level = 0);

	void aggregatePerLevelCodelength(InfoNode& parent, std::vector<PerLevelStat>& perLevelStat, unsigned int level);

	// ===================================================
	// Debug: *
	// ===================================================

	virtual void printDebug() {}
	void printRSS(unsigned int minVerboseLevel = 0);

	// ===================================================
	// Members
	// ===================================================

protected:

	virtual void initOptimizer(bool forceNoMemory = false) = 0;

	/**
	 * Write tree to output stream
	 * @param states, write state-level tree, else aggregate physical nodes within modules
	 */
	void writeTree(std::ostream& outStream, bool states = false);

	/**
	 * Write tree links to output stream
	 */
	void writeTreeLinks(std::ostream& outStream, bool states = false);

	InfoNode m_root;
	std::vector<InfoNode*> m_leafNodes;
	std::vector<InfoNode*> m_moduleNodes;
	std::vector<InfoNode*>* m_activeNetwork = nullptr;

	std::vector<InfoNode*> m_originalLeafNodes;

	Network m_network;	

	const unsigned int SUPER_LEVEL_ADDITION = 1 << 20;
	bool m_isMain = true;
	unsigned int m_subLevel = 0;

	bool m_calculateEnterExitFlow = false;

	double m_oneLevelCodelength = 0.0;
	unsigned int m_numNonTrivialTopModules = 0;
	unsigned int m_tuneIterationIndex = 0;
	bool m_isCoarseTune = false;
	unsigned int m_aggregationLevel = 0;

	double m_hierarchicalCodelength = 0.0;
};


struct PerLevelStat
{
	PerLevelStat() {}
	double codelength() { return indexLength + leafLength; }
	unsigned int numNodes() { return numModules + numLeafNodes; }
	unsigned int numModules = 0;
	unsigned int numLeafNodes = 0;
	double indexLength = 0.0;
	double leafLength = 0.0;
};

struct PerIterationStats
{
	PerIterationStats()
	:	iterationIndex(0),
		numTopModules(0),
		numBottomModules(0),
		topPerplexity(0.0),
		bottomPerplexity(0.0),
		topOverlap(0.0),
		bottomOverlap(0.0),
		codelength(0.0),
		maxDepth(0),
		weightedDepth(0.0),
		seconds(0.0),
		isMinimum(false) {}
	unsigned int iterationIndex;
	unsigned int numTopModules;
	unsigned int numBottomModules;
	double topPerplexity; // Perplexity of top module flow distribution
	double bottomPerplexity;
	double topOverlap; // Average number of modules per physical node
	double bottomOverlap;
	double codelength;
	unsigned int maxDepth;
	double weightedDepth;
	double seconds;
	bool isMinimum;
};

}

#endif /* SRC_CLUSTERING_INFOMAPBASE_H_ */
