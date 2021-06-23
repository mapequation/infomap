/*
 * InfomapBase.h
 *
 *  Created on: 23 feb 2015
 *      Author: Daniel
 */

#ifndef SRC_CLUSTERING_INFOMAPBASE_H_
#define SRC_CLUSTERING_INFOMAPBASE_H_

#include "InfomapConfig.h"
#include "InfoEdge.h"
#include "InfoNode.h"
#include "InfomapIterator.h"
#include "ClusterMap.h"
#include "../io/Network.h"
#include "../utils/Log.h"
#include "../utils/Date.h"
#include "../utils/Stopwatch.h"

#include <vector>
#include <deque>
#include <map>
#include <limits>
#include <string>
#include <iostream>
#include <sstream>

namespace infomap {

struct PerLevelStat;

namespace detail {
  class PartitionQueue;
} // namespace detail

class InfomapBase : public InfomapConfig<InfomapBase> {
  template <typename Objective>
  friend class InfomapOptimizer;

protected:
  using EdgeType = Edge<InfoNode>;

public:
  using PartitionQueue = detail::PartitionQueue;

  InfomapBase() : InfomapConfig<InfomapBase>() {}

  explicit InfomapBase(const Config& conf) : InfomapConfig<InfomapBase>(conf), m_network(conf) {}

  explicit InfomapBase(const std::string flags) : InfomapConfig<InfomapBase>(flags)
  {
    m_network.setConfig(*this);
    m_initialParameters = m_currentParameters = flags;
  }

  virtual ~InfomapBase() = default;


  // ===================================================
  // Getters
  // ===================================================

  const Network& network() const;

  Network& network();

  InfoNode& root();
  const InfoNode& root() const;

  // InfomapIterator tree(int maxClusterLevel = std::numeric_limits<unsigned int>::max())
  // { return InfomapIterator(&root(), maxClusterLevel); }

  InfomapIterator iterTree(int maxClusterLevel = 1)
  {
    return InfomapIterator(&root(), maxClusterLevel);
  }

  InfomapIteratorPhysical iterTreePhysical(int maxClusterLevel = 1)
  {
    return InfomapIteratorPhysical(&root(), maxClusterLevel);
  }

  InfomapModuleIterator iterModules(int maxClusterLevel = 1)
  {
    return InfomapModuleIterator(&root(), maxClusterLevel);
  }

  InfomapLeafModuleIterator iterLeafModules(int maxClusterLevel = 1)
  {
    return InfomapLeafModuleIterator(&root(), maxClusterLevel);
  }

  InfomapLeafIterator iterLeafNodes(int maxClusterLevel = 1)
  {
    return InfomapLeafIterator(&root(), maxClusterLevel);
  }

  InfomapLeafIteratorPhysical iterLeafNodesPhysical(int maxClusterLevel = 1)
  {
    return InfomapLeafIteratorPhysical(&root(), maxClusterLevel);
  }

  InfomapIterator begin(int maxClusterLevel = 1)
  {
    return InfomapIterator(&root(), maxClusterLevel);
  }

  InfomapIterator end()
  {
    return InfomapIterator(nullptr);
  }

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

  virtual double getMetaCodelength(bool unweighted = false) const { return 0.0; }

  virtual double codelength() const { return m_hierarchicalCodelength; }

  const std::vector<double>& codelengths() const { return m_codelengths; }

  virtual double getIndexCodelength() const = 0;

  virtual double getModuleCodelength() const = 0; // m_hierarchicalCodelength - getIndexCodelength()

  double getHierarchicalCodelength() const;

  double getOneLevelCodelength() const { return m_oneLevelCodelength; }

  double getRelativeCodelengthSavings() const
  {
    auto oneLevelCodelength = getOneLevelCodelength();
    return oneLevelCodelength < 1e-16 ? 0 : 1.0 - codelength() / oneLevelCodelength;
  }

  bool isFullNetwork() const { return m_isMain && m_aggregationLevel == 0; }
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

  bool haveHardPartition() const;

  std::vector<InfoNode*>& activeNetwork() const;

  // ===================================================
  // IO
  // ===================================================

  virtual std::ostream& toString(std::ostream& out) const;

  const std::map<unsigned int, unsigned int>& getInitialPartition() const { return m_initialPartition; }

  // ===================================================
  // Init
  // ===================================================

  double calcEntropyRate();

  InfomapBase& setInitialPartition(const std::map<unsigned int, unsigned int>& moduleIds);

  // ===================================================
  // Run
  // ===================================================

  virtual void run(std::string parameters = "");

  virtual void run(Network& network);

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

  /**
   * Provide an initial hierarchical partition of the network
   *
   * @param tree A tree path for each node
   */
  InfomapBase& initTree(const NodePaths& tree);

  virtual void init();

  virtual void runPartition();

  virtual void restoreHardPartition();

  void sortTreeOnFlow();

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

  virtual void aggregateFlowValuesFromLeafToRoot();

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

  virtual void moveActiveNodesToPredefinedModules(std::vector<unsigned int>& modules) = 0;

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
  // Run: Hierarchical Partition: *
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

  std::string getOutputFileHeader();

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
   * Write Newick tree to a .tre file.
   * @param filename the filename for the output file. If empty, use default
   * based on output directory and input file name
   * @param states if memory network, print the state-level network without merging physical nodes within modules
   * @return the filename written to
   */
  std::string writeNewickTree(std::string filename = "", bool states = false);

  std::string writeJsonTree(std::string filename = "", bool states = false);

  std::string writeCsvTree(std::string filename = "", bool states = false);

  /**
   * Write tree to a .clu file.
   * @param filename the filename for the output file. If empty, use default
   * based on output directory and input file name
   * @param states if memory network, print the state-level network without merging physical nodes within modules
   * @param moduleIndexLevel the depth from the root on which to advance module index.
   * Value 1 (default) will give the module index on the coarsest level, 2 the level below and so on.
   * Value -1 will give the module index for the lowest level, i.e. the finest modular structure.
   * @return the filename written to
   */
  std::string writeClu(std::string filename = "", bool states = false, int moduleIndexLevel = 1);

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

  /**
   * Write Newick tree to output stream
   * @param states, write state-level tree, else aggregate physical nodes within modules
   */
  void writeNewickTree(std::ostream& outStream, bool states = false);

  /**
   * Write JSON tree to output stream
   * @param states, write state-level tree, else aggregate physical nodes within modules
   */
  void writeJsonTree(std::ostream& outStream, bool states = false);

  /**
   * Write CSV tree to output stream
   * @param states, write state-level tree, else aggregate physical nodes within modules
   */
  void writeCsvTree(std::ostream& outStream, bool states = false);

  InfoNode m_root;
  std::vector<InfoNode*> m_leafNodes;
  std::vector<InfoNode*> m_moduleNodes;
  std::vector<InfoNode*>* m_activeNetwork = nullptr;

  std::vector<InfoNode*> m_originalLeafNodes;

  Network m_network;
  std::map<unsigned int, unsigned int> m_initialPartition = {}; // nodeId -> moduleId

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
  std::vector<double> m_codelengths;

  Date m_startDate;
  Date m_endDate;
  Stopwatch m_elapsedTime = Stopwatch(false);
  std::string m_initialParameters = "";
  std::string m_currentParameters = "";
};


struct PerLevelStat {
  double codelength() const { return indexLength + leafLength; }

  unsigned int numNodes() const { return numModules + numLeafNodes; }

  unsigned int numModules = 0;
  unsigned int numLeafNodes = 0;
  double indexLength = 0.0;
  double leafLength = 0.0;
};


namespace detail {

  class PartitionQueue {
    using PendingModule = InfoNode*;

  public:
    using size_t = std::deque<PendingModule>::size_type;

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

    size_t size() const { return m_queue.size(); }

    void resize(size_t size) { m_queue.resize(size); }

    PendingModule& operator[](size_t i) { return m_queue[i]; }

    unsigned int level = 1;
    unsigned int numNonTrivialModules = 0;
    double flow = 0.0;
    double nonTrivialFlow = 0.0;
    bool skip = false;
    double indexCodelength = 0.0; // Consolidated
    double leafCodelength = 0.0; // Consolidated
    double moduleCodelength = 0.0; // Left to improve on next level

  private:
    std::deque<PendingModule> m_queue;
  };

} // namespace detail

} // namespace infomap

#endif /* SRC_CLUSTERING_INFOMAPBASE_H_ */
