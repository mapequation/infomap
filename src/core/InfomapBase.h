/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef INFOMAP_BASE_H_
#define INFOMAP_BASE_H_

#include "InfomapConfig.h"
#include "InfoEdge.h"
#include "InfoNode.h"
#include "InfomapOptimizerBase.h"
#include "iterators/InfomapIterator.h"
#include "../io/ClusterMap.h"
#include "../io/Network.h"
#include "../io/Output.h"
#include "../utils/Log.h"
#include "../utils/Date.h"
#include "../utils/Stopwatch.h"

#include <vector>
#include <algorithm>
#include <deque>
#include <map>
#include <unordered_map>
#include <limits>
#include <string>
#include <iostream>
#include <sstream>
#include <cassert>

namespace infomap {

namespace detail {
  class PartitionQueue;
  struct PerLevelStat;
} // namespace detail

class InfomapBase : public InfomapConfig<InfomapBase> {
  template <typename Objective>
  friend class InfomapOptimizer;

  void initOptimizer(bool forceNoMemory = false);

public:
  using PartitionQueue = detail::PartitionQueue;

  struct ActiveGraphStorageBreakdown {
    std::size_t activeNodePointerBytes = 0;
    std::size_t activeNodeToIdEntryBytes = 0;
    std::size_t activeNodeToIdBucketBytes = 0;
    std::size_t csrStateIdEntryBytes = 0;
    std::size_t csrStateIdBucketBytes = 0;
    std::size_t csrOutOffsetBytes = 0;
    std::size_t csrOutTargetBytes = 0;
    std::size_t csrOutFlowBytes = 0;
    std::size_t csrInOffsetBytes = 0;
    std::size_t csrInTargetBytes = 0;
    std::size_t csrInFlowIndexBytes = 0;
    std::size_t csrModuleIndexBytes = 0;
    std::size_t csrDirtyFlagBytes = 0;
    bool csrAvailable = false;

    std::size_t totalBytes() const noexcept
    {
      return activeNodePointerBytes
          + activeNodeToIdEntryBytes
          + activeNodeToIdBucketBytes
          + csrStateIdEntryBytes
          + csrStateIdBucketBytes
          + csrOutOffsetBytes
          + csrOutTargetBytes
          + csrOutFlowBytes
          + csrInOffsetBytes
          + csrInTargetBytes
          + csrInFlowIndexBytes
          + csrModuleIndexBytes
          + csrDirtyFlagBytes;
    }

    void maximize(const ActiveGraphStorageBreakdown& other) noexcept
    {
      activeNodePointerBytes = std::max(activeNodePointerBytes, other.activeNodePointerBytes);
      activeNodeToIdEntryBytes = std::max(activeNodeToIdEntryBytes, other.activeNodeToIdEntryBytes);
      activeNodeToIdBucketBytes = std::max(activeNodeToIdBucketBytes, other.activeNodeToIdBucketBytes);
      csrStateIdEntryBytes = std::max(csrStateIdEntryBytes, other.csrStateIdEntryBytes);
      csrStateIdBucketBytes = std::max(csrStateIdBucketBytes, other.csrStateIdBucketBytes);
      csrOutOffsetBytes = std::max(csrOutOffsetBytes, other.csrOutOffsetBytes);
      csrOutTargetBytes = std::max(csrOutTargetBytes, other.csrOutTargetBytes);
      csrOutFlowBytes = std::max(csrOutFlowBytes, other.csrOutFlowBytes);
      csrInOffsetBytes = std::max(csrInOffsetBytes, other.csrInOffsetBytes);
      csrInTargetBytes = std::max(csrInTargetBytes, other.csrInTargetBytes);
      csrInFlowIndexBytes = std::max(csrInFlowIndexBytes, other.csrInFlowIndexBytes);
      csrModuleIndexBytes = std::max(csrModuleIndexBytes, other.csrModuleIndexBytes);
      csrDirtyFlagBytes = std::max(csrDirtyFlagBytes, other.csrDirtyFlagBytes);
      csrAvailable = csrAvailable || other.csrAvailable;
    }
  };

  struct ConsolidationSnapshot {
    unsigned int level = 0;
    unsigned int activeNodeCount = 0;
    unsigned int moduleNodeCount = 0;
    unsigned int moduleEdgeCount = 0;
    bool replaceExistingModules = false;
  };

  struct BenchmarkStats {
    double calculateFlowSec = 0.0;
    double initNetworkSec = 0.0;
    double runPartitionSec = 0.0;
    double findTopModulesSec = 0.0;
    double fineTuneSec = 0.0;
    double coarseTuneSec = 0.0;
    double recursivePartitionSec = 0.0;
    double superModulesSec = 0.0;
    double superModulesFastSec = 0.0;
    unsigned int findTopModulesCalls = 0;
    unsigned int fineTuneCalls = 0;
    unsigned int coarseTuneCalls = 0;
    unsigned int recursivePartitionCalls = 0;
    unsigned int superModulesCalls = 0;
    unsigned int superModulesFastCalls = 0;
    unsigned int consolidationCount = 0;
    std::vector<ConsolidationSnapshot> consolidations;
    ActiveGraphStorageBreakdown lastActiveGraphStorage;
    ActiveGraphStorageBreakdown maxActiveGraphStorage;

    void reset()
    {
      calculateFlowSec = 0.0;
      initNetworkSec = 0.0;
      runPartitionSec = 0.0;
      findTopModulesSec = 0.0;
      fineTuneSec = 0.0;
      coarseTuneSec = 0.0;
      recursivePartitionSec = 0.0;
      superModulesSec = 0.0;
      superModulesFastSec = 0.0;
      findTopModulesCalls = 0;
      fineTuneCalls = 0;
      coarseTuneCalls = 0;
      recursivePartitionCalls = 0;
      superModulesCalls = 0;
      superModulesFastCalls = 0;
      consolidationCount = 0;
      consolidations.clear();
      lastActiveGraphStorage = {};
      maxActiveGraphStorage = {};
    }
  };

  struct ActiveGraphMaterialization {
    using ActiveNodeId = unsigned int;

    std::vector<InfoNode*> nodes;
    mutable std::unordered_map<InfoNode*, ActiveNodeId> nodeToId;

    void reset()
    {
      nodes.clear();
      decltype(nodeToId){}.swap(nodeToId);
    }

    std::size_t payloadBytes() const noexcept
    {
      return 0;
    }

    std::size_t nodePointerBytes() const noexcept
    {
      return nodes.capacity() * sizeof(InfoNode*);
    }

    std::size_t nodeToIdEntryBytes() const noexcept
    {
      return nodeToId.size() * sizeof(typename decltype(nodeToId)::value_type);
    }

    std::size_t nodeToIdBucketBytes() const noexcept
    {
      return nodeToId.bucket_count() * sizeof(void*);
    }

    std::size_t storageBytes() const noexcept
    {
      return nodePointerBytes() + nodeToIdEntryBytes() + nodeToIdBucketBytes();
    }

    std::size_t size() const noexcept
    {
      return nodes.size();
    }

    bool empty() const noexcept
    {
      return nodes.empty();
    }

    void ensureNodeToId() const
    {
      if (!nodeToId.empty() || nodes.empty()) {
        return;
      }
      nodeToId.max_load_factor(4.0f);
      nodeToId.reserve(nodes.size());
      for (std::size_t i = 0; i < nodes.size(); ++i) {
        nodeToId[const_cast<InfoNode*>(nodes[i])] = static_cast<ActiveNodeId>(i);
      }
    }

    ActiveNodeId idFor(const InfoNode& node) const
    {
      ensureNodeToId();
      auto it = nodeToId.find(const_cast<InfoNode*>(&node));
      if (it == nodeToId.end()) {
        throw std::out_of_range("ActiveGraphMaterialization::idFor() called for non-materialized node");
      }
      return it->second;
    }

    InfoNode& nodeFor(ActiveNodeId id) const
    {
      if (id >= nodes.size()) {
        throw std::out_of_range("ActiveGraphMaterialization::nodeFor() id out of range");
      }
      return *nodes[id];
    }

  };

  struct CsrMaterialization {
    std::unordered_map<unsigned int, unsigned int> stateIdToActiveId;
    std::vector<unsigned int> outOffsets;
    std::vector<unsigned int> outTargets;
    std::vector<double> outFlows;
    std::vector<unsigned int> inOffsets;
    std::vector<unsigned int> inTargets;
    std::vector<unsigned int> inFlowIndices;
    std::vector<unsigned int> moduleIndices;
    std::vector<unsigned char> dirtyFlags;
    bool available = false;

    void reset()
    {
      decltype(stateIdToActiveId){}.swap(stateIdToActiveId);
      outOffsets.clear();
      outTargets.clear();
      outFlows.clear();
      inOffsets.clear();
      inTargets.clear();
      inFlowIndices.clear();
      moduleIndices.clear();
      dirtyFlags.clear();
      available = false;
    }

    std::size_t adjacencyBytes() const noexcept
    {
      return stateIdEntryBytes()
          + stateIdBucketBytes()
          + outOffsetBytes()
          + outTargetBytes()
          + outFlowBytes()
          + inOffsetBytes()
          + inTargetBytes()
          + inFlowIndexBytes()
          + moduleIndexBytes()
          + dirtyFlagBytes();
    }

    std::size_t outOffsetBytes() const noexcept
    {
      return outOffsets.capacity() * sizeof(unsigned int);
    }

    std::size_t stateIdEntryBytes() const noexcept
    {
      return stateIdToActiveId.size() * sizeof(typename decltype(stateIdToActiveId)::value_type);
    }

    std::size_t stateIdBucketBytes() const noexcept
    {
      return stateIdToActiveId.bucket_count() * sizeof(void*);
    }

    std::size_t outTargetBytes() const noexcept
    {
      return outTargets.capacity() * sizeof(unsigned int);
    }

    std::size_t outFlowBytes() const noexcept
    {
      return outFlows.capacity() * sizeof(double);
    }

    std::size_t inOffsetBytes() const noexcept
    {
      return inOffsets.capacity() * sizeof(unsigned int);
    }

    std::size_t inTargetBytes() const noexcept
    {
      return inTargets.capacity() * sizeof(unsigned int);
    }

    std::size_t inFlowIndexBytes() const noexcept
    {
      return inFlowIndices.capacity() * sizeof(unsigned int);
    }

    std::size_t moduleIndexBytes() const noexcept
    {
      return moduleIndices.capacity() * sizeof(unsigned int);
    }

    std::size_t dirtyFlagBytes() const noexcept
    {
      return dirtyFlags.capacity() * sizeof(unsigned char);
    }
  };

  struct PointerBackend {
    using ActiveNodeId = ActiveGraphMaterialization::ActiveNodeId;

    struct EdgeView {
      ActiveNodeId neighbourId = 0;
      double weight = 0.0;
      double flow = 0.0;
    };

    PointerBackend(InfomapBase& owner, ActiveGraphMaterialization& materialization)
        : owner(owner),
          materialization(materialization) { }

    std::size_t size() const noexcept { return materialization.size(); }
    bool empty() const noexcept { return materialization.empty(); }
    void ensureReverseLookup() const { owner.ensureActiveGraphNodeToId(); }

    ActiveNodeId idFor(const InfoNode& node) const
    {
      owner.ensureActiveGraphNodeToId();
      return materialization.idFor(node);
    }
    InfoNode& nodeFor(ActiveNodeId id) const { return materialization.nodeFor(id); }
    unsigned int& moduleIndex(ActiveNodeId id) const { return nodeFor(id).index; }
    bool& dirty(ActiveNodeId id) const { return nodeFor(id).dirty; }

    template <typename Fn>
    void forEachOutEdge(ActiveNodeId id, Fn&& fn) const
    {
      auto& node = nodeFor(id);
      for (auto* edge : node.outEdges()) {
        fn(materialization.idFor(*edge->target), *edge->target, *edge);
      }
    }

    template <typename Fn>
    void forEachInEdge(ActiveNodeId id, Fn&& fn) const
    {
      auto& node = nodeFor(id);
      for (auto* edge : node.inEdges()) {
        fn(materialization.idFor(*edge->source), *edge->source, *edge);
      }
    }

    std::vector<EdgeView> outEdges(ActiveNodeId id) const
    {
      std::vector<EdgeView> edges;
      auto& node = nodeFor(id);
      edges.reserve(node.outDegree());
      for (auto* edge : node.outEdges()) {
        edges.push_back({
            materialization.idFor(*edge->target),
            edge->data.weight,
            edge->data.flow,
        });
      }
      return edges;
    }

    std::vector<EdgeView> inEdges(ActiveNodeId id) const
    {
      std::vector<EdgeView> edges;
      auto& node = nodeFor(id);
      edges.reserve(node.inDegree());
      for (auto* edge : node.inEdges()) {
        edges.push_back({
            materialization.idFor(*edge->source),
            edge->data.weight,
            edge->data.flow,
        });
      }
      return edges;
    }

  private:
    InfomapBase& owner;
    ActiveGraphMaterialization& materialization;
  };

  using PointerActiveGraph = PointerBackend;

  struct CsrBackend {
    using ActiveNodeId = ActiveGraphMaterialization::ActiveNodeId;

    struct EdgeView {
      ActiveNodeId neighbourId = 0;
      double flow = 0.0;
    };

    struct EdgeSpan {
      const unsigned int* targets = nullptr;
      const double* flows = nullptr;
      std::size_t size = 0;
    };

    struct InEdgeSpan {
      const unsigned int* targets = nullptr;
      const unsigned int* flowIndices = nullptr;
      const double* outFlows = nullptr;
      std::size_t size = 0;

      double flowAt(std::size_t index) const
      {
        return outFlows[flowIndices[index]];
      }
    };

    CsrBackend(ActiveGraphMaterialization& materialization, CsrMaterialization& csrMaterialization)
        : materialization(materialization),
          csrMaterialization(csrMaterialization) { }

    bool available() const noexcept { return csrMaterialization.available; }
    std::size_t size() const noexcept { return materialization.size(); }
    bool empty() const noexcept { return materialization.empty(); }

    ActiveNodeId idFor(const InfoNode& node) const
    {
      auto it = csrMaterialization.stateIdToActiveId.find(node.stateId);
      if (it == csrMaterialization.stateIdToActiveId.end()) {
        throw std::out_of_range("CsrBackend::idFor() called for non-materialized node");
      }
      return it->second;
    }
    InfoNode& nodeFor(ActiveNodeId id) const
    {
      assert(id < materialization.nodes.size());
      return *materialization.nodes[id];
    }
    unsigned int& moduleIndex(ActiveNodeId id) const
    {
      assert(id < csrMaterialization.moduleIndices.size());
      return csrMaterialization.moduleIndices[id];
    }

    struct DirtyRef {
      std::vector<unsigned char>* flags = nullptr;
      std::size_t index = 0;

      DirtyRef& operator=(bool value)
      {
        (*flags)[index] = value ? 1u : 0u;
        return *this;
      }

      operator bool() const
      {
        return (*flags)[index] != 0;
      }
    };

    DirtyRef dirty(ActiveNodeId id) const
    {
      assert(id < csrMaterialization.dirtyFlags.size());
      return DirtyRef{&csrMaterialization.dirtyFlags, id};
    }

    unsigned int* moduleIndicesData() noexcept { return csrMaterialization.moduleIndices.data(); }
    const unsigned int* moduleIndicesData() const noexcept { return csrMaterialization.moduleIndices.data(); }
    unsigned char* dirtyFlagsData() noexcept { return csrMaterialization.dirtyFlags.data(); }
    const unsigned char* dirtyFlagsData() const noexcept { return csrMaterialization.dirtyFlags.data(); }

    template <typename Fn>
    void forEachOutEdge(ActiveNodeId id, Fn&& fn) const
    {
      const auto edges = outEdges(id);
      for (std::size_t i = 0; i < edges.size; ++i) {
        EdgeView edge{
            edges.targets[i],
            edges.flows[i],
        };
        fn(edge.neighbourId, *materialization.nodes[edge.neighbourId], edge);
      }
    }

    template <typename Fn>
    void forEachInEdge(ActiveNodeId id, Fn&& fn) const
    {
      const auto edges = inEdges(id);
      for (std::size_t i = 0; i < edges.size; ++i) {
        EdgeView edge{
            edges.targets[i],
            edges.flowAt(i),
        };
        fn(edge.neighbourId, *materialization.nodes[edge.neighbourId], edge);
      }
    }

    EdgeSpan outEdges(ActiveNodeId id) const
    {
      if (!available()) return {};
      assert(id + 1 < csrMaterialization.outOffsets.size());
      const auto begin = csrMaterialization.outOffsets[id];
      const auto end = csrMaterialization.outOffsets[id + 1];
      return {
          csrMaterialization.outTargets.data() + begin,
          csrMaterialization.outFlows.data() + begin,
          end - begin,
      };
    }

    InEdgeSpan inEdges(ActiveNodeId id) const
    {
      if (!available()) return {};
      assert(id + 1 < csrMaterialization.inOffsets.size());
      const auto begin = csrMaterialization.inOffsets[id];
      const auto end = csrMaterialization.inOffsets[id + 1];
      return {
          csrMaterialization.inTargets.data() + begin,
          csrMaterialization.inFlowIndices.data() + begin,
          csrMaterialization.outFlows.data(),
          end - begin,
      };
    }

  private:
    ActiveGraphMaterialization& materialization;
    CsrMaterialization& csrMaterialization;
  };

  InfomapBase() : InfomapConfig<InfomapBase>() { initOptimizer(); }

  explicit InfomapBase(const Config& conf) : InfomapConfig<InfomapBase>(conf), m_network(conf) { initOptimizer(); }

  explicit InfomapBase(const std::string& flags, bool isCli = false) : InfomapConfig<InfomapBase>(flags, isCli)
  {
    initOptimizer();
    m_network.setConfig(*this);
    m_initialParameters = m_currentParameters = flags;
  }

  virtual ~InfomapBase() = default;

  // ===================================================
  // Iterators
  // ===================================================

  InfomapIterator iterTree(int maxClusterLevel = 1) { return { &root(), maxClusterLevel }; }

  InfomapIteratorPhysical iterTreePhysical(int maxClusterLevel = 1) { return { &root(), maxClusterLevel }; }

  InfomapModuleIterator iterModules(int maxClusterLevel = 1) { return { &root(), maxClusterLevel }; }

  InfomapLeafModuleIterator iterLeafModules(int maxClusterLevel = 1) { return { &root(), maxClusterLevel }; }

  InfomapLeafIterator iterLeafNodes(int maxClusterLevel = 1) { return { &root(), maxClusterLevel }; }

  InfomapLeafIteratorPhysical iterLeafNodesPhysical(int maxClusterLevel = 1) { return { &root(), maxClusterLevel }; }

  InfomapIterator begin(int maxClusterLevel = 1) { return { &root(), maxClusterLevel }; }

  InfomapIterator end() const { return InfomapIterator(nullptr); }

  // ===================================================
  // Getters
  // ===================================================

  Network& network() { return m_network; }
  const Network& network() const { return m_network; }

  InfoNode& root() { return m_root; }
  const InfoNode& root() const { return m_root; }

  unsigned int numLeafNodes() const { return m_leafNodes.size(); }

  const std::vector<InfoNode*>& leafNodes() const { return m_leafNodes; }

  unsigned int numTopModules() const { return m_root.childDegree(); }

  unsigned int numActiveModules() const { return m_optimizer->numActiveModules(); }

  unsigned int numNonTrivialTopModules() const { return m_numNonTrivialTopModules; }

  bool haveModules() const { return !m_root.isLeaf() && !m_root.firstChild->isLeaf(); }

  bool haveNonTrivialModules() const { return numNonTrivialTopModules() > 0; }

  /**
   * Number of node levels below the root in current Infomap instance, 1 if no modules
   */
  unsigned int numLevels() const;

  /**
   * Get maximum depth of any child in the tree, following possible sub Infomap instances
   */
  unsigned int maxTreeDepth() const;

  double getCodelength() const { return m_optimizer->getCodelength(); }

  double getMetaCodelength(bool unweighted = false) const { return m_optimizer->getMetaCodelength(unweighted); }

  double codelength() const { return m_hierarchicalCodelength; }

  const std::vector<double>& codelengths() const { return m_codelengths; }

  double getIndexCodelength() const { return m_optimizer->getIndexCodelength(); }

  double getModuleCodelength() const { return m_hierarchicalCodelength - m_optimizer->getIndexCodelength(); }

  double getHierarchicalCodelength() const { return m_hierarchicalCodelength; }

  double getOneLevelCodelength() const { return m_oneLevelCodelength; }

  double getRelativeCodelengthSavings() const
  {
    auto oneLevelCodelength = getOneLevelCodelength();
    return oneLevelCodelength < 1e-16 ? 0 : 1.0 - codelength() / oneLevelCodelength;
  }

  double getEntropyRate() { return m_entropyRate; }
  double getMaxEntropy() { return m_maxEntropy; }
  double getMaxFlow() { return m_maxFlow; }
  const BenchmarkStats& benchmarkStats() const noexcept { return m_benchmarkStats; }
  ActiveGraphStorageBreakdown activeGraphStorageBreakdown() const noexcept
  {
    return {
        m_activeGraphMaterialization.nodePointerBytes(),
        m_activeGraphMaterialization.nodeToIdEntryBytes(),
        m_activeGraphMaterialization.nodeToIdBucketBytes(),
        m_csrMaterialization.stateIdEntryBytes(),
        m_csrMaterialization.stateIdBucketBytes(),
        m_csrMaterialization.outOffsetBytes(),
        m_csrMaterialization.outTargetBytes(),
        m_csrMaterialization.outFlowBytes(),
        m_csrMaterialization.inOffsetBytes(),
        m_csrMaterialization.inTargetBytes(),
        m_csrMaterialization.inFlowIndexBytes(),
        m_csrMaterialization.moduleIndexBytes(),
        m_csrMaterialization.dirtyFlagBytes(),
        m_csrMaterialization.available,
    };
  }
  ActiveGraphMaterialization& activeGraphMaterialization() noexcept { return m_activeGraphMaterialization; }
  const ActiveGraphMaterialization& activeGraphMaterialization() const noexcept { return m_activeGraphMaterialization; }
  CsrMaterialization& csrMaterialization() noexcept { return m_csrMaterialization; }
  const CsrMaterialization& csrMaterialization() const noexcept { return m_csrMaterialization; }
  PointerBackend pointerBackend() noexcept { return PointerBackend(*this, m_activeGraphMaterialization); }
  PointerActiveGraph pointerActiveGraph() noexcept { return pointerBackend(); }
  CsrBackend csrBackend() noexcept { return CsrBackend(m_activeGraphMaterialization, m_csrMaterialization); }

  const Date& getStartDate() const { return m_startDate; }
  const Stopwatch& getElapsedTime() const { return m_elapsedTime; }

  std::vector<InfoNode*>& activeNetwork() const { return *m_activeNetwork; }

  std::map<unsigned int, std::vector<unsigned int>> getMultilevelModules(bool states = false);

  // ===================================================
  // IO
  // ===================================================

  std::ostream& toString(std::ostream& out) const { return m_optimizer->toString(out); }

  // ===================================================
  // Run
  // ===================================================

  using InitialPartition = std::map<unsigned int, unsigned int>;

  const InitialPartition& getInitialPartition() const { return m_initialPartition; }

  InfomapBase& setInitialPartition(const InitialPartition& moduleIds)
  {
    m_initialPartition = moduleIds;
    return *this;
  }

  void run(const std::string& parameters = "");

  void run(Network& network);

private:
  bool isFullNetwork() const { return m_isMain && m_aggregationLevel == 0; }
  bool isFirstLoop() const { return m_tuneIterationIndex == 0 && isFullNetwork(); }

  InfomapBase* getNewInfomapInstance() const { return new InfomapBase(getConfig()); }
  InfomapBase* getNewInfomapInstanceWithoutMemory() const
  {
    auto im = new InfomapBase();
    im->initOptimizer(true);
    return im;
  }

  InfomapBase& getSubInfomap(InfoNode& node) const
  {
    return node.setInfomap(getNewInfomapInstance())
        .setIsMain(false)
        .setSubLevel(m_subLevel + 1)
        .setNonMainConfig(*this);
  }

  InfomapBase& getSuperInfomap(InfoNode& node) const
  {
    return node.setInfomap(getNewInfomapInstanceWithoutMemory())
        .setIsMain(false)
        .setSubLevel(m_subLevel + SUPER_LEVEL_ADDITION)
        .setNonMainConfig(*this);
  }

  /**
   * Only the main infomap reads an external cluster file if exist
   */
  InfomapBase& setIsMain(bool isMain)
  {
    m_isMain = isMain;
    return *this;
  }

  InfomapBase& setSubLevel(unsigned int level)
  {
    m_subLevel = level;
    return *this;
  }

  bool isTopLevel() const { return (m_subLevel & (SUPER_LEVEL_ADDITION - 1)) == 0; }

  bool isSuperLevelOnTopLevel() const { return m_subLevel == SUPER_LEVEL_ADDITION; }

  bool isMainInfomap() const { return m_isMain; }

  bool haveHardPartition() const { return !m_originalLeafNodes.empty(); }

  // ===================================================
  // Run: *
  // ===================================================

  InfomapBase& initNetwork(Network& network);
  InfomapBase& initNetwork(InfoNode& parent, bool asSuperNetwork = false);

  void generateSubNetwork(Network& network);
  void generateSubNetwork(InfoNode& parent);

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
  InfomapBase& initMetaData(const std::string& metaDataFile);

  /**
   * Provide an initial partition of the network.
   *
   * @param clusterDataFile A .clu file containing cluster data.
   * @param hard If true, the provided clusters will not be splitted. This reduces the
   * effective network size during the optimization phase but the hard partitions are
   * after that replaced by the original nodes.
   */
  InfomapBase& initPartition(const std::string& clusterDataFile, bool hard = false, const Network* network = nullptr);

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

  void init();

  void runPartition();

  void restoreHardPartition();

  void writeResult(int trial = -1);

  // ===================================================
  // runPartition: *
  // ===================================================

  void hierarchicalPartition();

  void partition();

  // ===================================================
  // runPartition: init: *
  // ===================================================

  /**
   * Done in network?
   */
  void initEnterExitFlow();

  void aggregateFlowValuesFromLeafToRoot();

  // Init terms that is constant for the whole network
  void initTree() { return m_optimizer->initTree(); }

  void initNetwork() { return m_optimizer->initNetwork(); }

  void initSuperNetwork() { return m_optimizer->initSuperNetwork(); }

  double calcCodelength(const InfoNode& parent) const { return m_optimizer->calcCodelength(parent); }

  /**
   * Calculate and store codelength on all modules in the tree
   * @param includeRoot Also calculate the codelength on the root node
   * @return the hierarchical codelength
   */
  double calcCodelengthOnTree(InfoNode& root, bool includeRoot = true) const;

  // ===================================================
  // Run: Partition: *
  // ===================================================

  void setActiveNetworkFromLeafs()
  {
    m_activeNetwork = &m_leafNodes;
    materializeActiveGraphPayload();
  }

  void setActiveNetworkFromChildrenOfRoot();

  void materializeActiveGraphPayload();
  void materializeLeafLevelCsr();
  void updateActiveGraphStorageBenchmarkStats();
  void ensureActiveGraphNodeToId();
  void syncActiveGraphStateToHierarchy();

  void initPartition() { return m_optimizer->initPartition(); }

  void findTopModulesRepeatedly(unsigned int maxLevels);

  unsigned int fineTune();

  unsigned int coarseTune();

  /**
   * Return the number of effective core loops, i.e. not the last if not at coreLoopLimit
   */
  unsigned int optimizeActiveNetwork();

  void moveActiveNodesToPredefinedModules(std::vector<unsigned int>& modules);

  void consolidateModules(bool replaceExistingModules = true);

  unsigned int calculateNumNonTrivialTopModules() const;

  unsigned int calculateMaxDepth() const;

  // ===================================================
  // Partition: findTopModulesRepeatedly: *
  // ===================================================

  /**
   * Return true if restored to consolidated optimization state
   */
  bool restoreConsolidatedOptimizationPointIfNoImprovement(bool forceRestore = false)
  {
    return m_optimizer->restoreConsolidatedOptimizationPointIfNoImprovement(forceRestore);
  }

  // ===================================================
  // Run: Hierarchical Partition: *
  // ===================================================

  /**
   * Find super modules applying the whole two-level algorithm on the
   * top modules iteratively
   * @param levelLimit The maximum number of super module levels allowed
   * @return number of levels created
   */
  unsigned int findHierarchicalSuperModules(unsigned int superLevelLimit = std::numeric_limits<unsigned int>::max());

  /**
   * Find super modules fast by merge and consolidate top modules iteratively
   * @param levelLimit The maximum number of super module levels allowed
   * @return number of levels created
   */
  unsigned int findHierarchicalSuperModulesFast(unsigned int superLevelLimit = std::numeric_limits<unsigned int>::max());

  void transformNodeFlowToEnterFlow(InfoNode& parent);

  void resetFlowOnModules();

  unsigned int removeModules();

  unsigned int removeSubModules(bool recalculateCodelengthOnTree);

  unsigned int recursivePartition();

  void queueTopModules(PartitionQueue& partitionQueue);

  void queueLeafModules(PartitionQueue& partitionQueue);

  bool processPartitionQueue(PartitionQueue& queue, PartitionQueue& nextLevel) const;

  void resetBenchmarkStats() { m_benchmarkStats.reset(); }

  void recordConsolidationSnapshot(unsigned int level,
                                   unsigned int activeNodeCount,
                                   unsigned int moduleNodeCount,
                                   unsigned int moduleEdgeCount,
                                   bool replaceExistingModules)
  {
    ++m_benchmarkStats.consolidationCount;
    m_benchmarkStats.consolidations.push_back({ level, activeNodeCount, moduleNodeCount, moduleEdgeCount, replaceExistingModules });
  }

public:
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
  std::string writeTree(const std::string& filename = "", bool states = false) { return infomap::writeTree(*this, m_network, filename, states); }

  /**
   * Write flow tree to a .ftree file.
   * This is the same as a .tree file but appended with links aggregated
   * within modules on all levels in the tree
   * @param filename the filename for the output file. If empty, use default
   * based on output directory and input file name
   * @param states if memory network, print the state-level network without merging physical nodes within modules
   * @return the filename written to
   */
  std::string writeFlowTree(const std::string& filename = "", bool states = false) { return infomap::writeFlowTree(*this, m_network, filename, states); }

  /**
   * Write Newick tree to a .tre file.
   * @param filename the filename for the output file. If empty, use default
   * based on output directory and input file name
   * @param states if memory network, print the state-level network without merging physical nodes within modules
   * @return the filename written to
   */
  std::string writeNewickTree(const std::string& filename = "", bool states = false) { return infomap::writeNewickTree(*this, filename, states); }

  std::string writeJsonTree(const std::string& filename = "", bool states = false, bool writeLinks = false) { return infomap::writeJsonTree(*this, m_network, filename, states, writeLinks); }

  std::string writeCsvTree(const std::string& filename = "", bool states = false) { return infomap::writeCsvTree(*this, m_network, filename, states); }

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
  std::string writeClu(const std::string& filename = "", bool states = false, int moduleIndexLevel = 1) { return infomap::writeClu(*this, m_network, filename, states, moduleIndexLevel); }

private:
  // ===================================================
  // Debug: *
  // ===================================================

  void printDebug() const { return m_optimizer->printDebug(); }

  // ===================================================
  // Members
  // ===================================================

protected:
  InfoNode m_root;
  std::vector<InfoNode*> m_leafNodes;
  std::vector<InfoNode*> m_moduleNodes;
  std::vector<InfoNode*>* m_activeNetwork = nullptr;

  std::vector<InfoNode*> m_originalLeafNodes;

  Network m_network;
  InitialPartition m_initialPartition = {}; // nodeId -> moduleId

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
  std::vector<unsigned int> m_numTopModules;
  double m_entropyRate = 0.0;
  double m_maxEntropy = 0.0;
  double m_maxFlow = 0.0;

  double m_sumDanglingFlow = 0.0;

  Date m_startDate;
  Date m_endDate;
  Stopwatch m_elapsedTime = Stopwatch(false);
  std::string m_initialParameters;
  std::string m_currentParameters;
  BenchmarkStats m_benchmarkStats;
  ActiveGraphMaterialization m_activeGraphMaterialization;
  CsrMaterialization m_csrMaterialization;
  bool m_disableCsrMaterialization = false;

  std::unique_ptr<InfomapOptimizerBase> m_optimizer;
};

/**
 * Print per level statistics
 */
unsigned int printPerLevelCodelength(const InfoNode& parent, std::ostream& out);

void aggregatePerLevelCodelength(const InfoNode& parent, std::vector<detail::PerLevelStat>& perLevelStat, unsigned int level = 0);

namespace detail {

  struct PerLevelStat {
    double codelength() const { return indexLength + leafLength; }

    unsigned int numNodes() const { return numModules + numLeafNodes; }

    unsigned int numModules = 0;
    unsigned int numLeafNodes = 0;
    double indexLength = 0.0;
    double leafLength = 0.0;
  };

  class PartitionQueue {
    using PendingModule = InfoNode*;

    std::deque<PendingModule> m_queue;

  public:
    unsigned int level = 1;
    unsigned int numNonTrivialModules = 0;
    double flow = 0.0;
    double nonTrivialFlow = 0.0;
    bool skip = false;
    double indexCodelength = 0.0; // Consolidated
    double leafCodelength = 0.0; // Consolidated
    double moduleCodelength = 0.0; // Left to improve on next level

    using size_t = std::deque<PendingModule>::size_type;

    void swap(PartitionQueue& other) noexcept
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
  };

} // namespace detail

} // namespace infomap

#endif // INFOMAP_BASE_H_
