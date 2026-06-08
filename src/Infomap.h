/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef INFOMAP_H_
#define INFOMAP_H_

#include "core/InfomapBase.h"
#include "io/Config.h"

#include <string>
#include <utility>
#include <map>
#include <vector>

namespace infomap {

#ifndef SWIGPYTHON
struct LinkResult {
  LinkResult() = default;
  LinkResult(unsigned int source, unsigned int target, double weight, double flow)
      : source(source),
        target(target),
        weight(weight),
        flow(flow) {}

  unsigned int source = 0;
  unsigned int target = 0;
  double weight = 0.0;
  double flow = 0.0;
};
#endif

// Wrapper class for the Python API
struct InfomapWrapper : public InfomapBase {
public:
  InfomapWrapper() {}
  InfomapWrapper(const std::string& flags) : InfomapBase(flags) {}
#ifndef SWIG
  InfomapWrapper(const Config& conf) : InfomapBase(conf) {}
#endif
  ~InfomapWrapper() override = default;

  // ===================================================
  // Wrapper methods
  // ===================================================

  void readInputData(std::string filename = "", bool accumulate = true) { m_network.readInputData(std::move(filename), accumulate); }

  void addNode(unsigned int id) { m_network.addNode(id); }
  void addNode(unsigned int id, std::string name) { m_network.addNode(id, std::move(name)); }
  void addNode(unsigned int id, double weight) { m_network.addNode(id, weight); }
  void addNode(unsigned int id, std::string name, double weight) { m_network.addNode(id, std::move(name), weight); }

  void addName(unsigned int id, const std::string& name) { m_network.addName(id, name); }
  std::string getName(unsigned int id) const
  {
    auto& names = m_network.names();
    auto it = names.find(id);
    return it != names.end() ? it->second : "";
  }

  const std::map<unsigned int, std::string>& getNames() const { return m_network.names(); }

  void addPhysicalNode(unsigned int id, const std::string& name = "") { m_network.addPhysicalNode(id, name); }
  void addStateNode(unsigned int id, unsigned int physId) { m_network.addStateNode(id, physId); }
  unsigned int addMultilayerNode(unsigned int stateId, unsigned int layerId, unsigned int physicalId, double weight = 1.0) { return m_network.addMultilayerNode(stateId, layerId, physicalId, weight); }

  void addLink(unsigned int sourceId, unsigned int targetId, double weight = 1.0) { m_network.addLink(sourceId, targetId, weight); }
  void addLink(unsigned int sourceId, unsigned int targetId, unsigned long weight) { m_network.addLink(sourceId, targetId, weight); }
  void addLinks(const std::vector<unsigned int>& sourceIds, const std::vector<unsigned int>& targetIds, const std::vector<double>& weights) { m_network.addLinks(sourceIds, targetIds, weights); }
  bool removeLink(unsigned int sourceId, unsigned int targetId) { return m_network.removeLink(sourceId, targetId); }
  void addMultilayerLink(unsigned int layer1, unsigned int n1, unsigned int layer2, unsigned int n2, double weight = 1.0) { m_network.addMultilayerLink(layer1, n1, layer2, n2, weight); }
  void addMultilayerLink(unsigned int stateId1, unsigned int layer1, unsigned int n1, unsigned int stateId2, unsigned int layer2, unsigned int n2, double weight)
  {
    m_network.addMultilayerLink(stateId1, layer1, n1, stateId2, layer2, n2, weight);
  }
  void addMultilayerLinks(const std::vector<unsigned int>& sourceLayerIds,
                          const std::vector<unsigned int>& sourceNodeIds,
                          const std::vector<unsigned int>& targetLayerIds,
                          const std::vector<unsigned int>& targetNodeIds,
                          const std::vector<double>& weights)
  {
    m_network.addMultilayerLinks(sourceLayerIds, sourceNodeIds, targetLayerIds, targetNodeIds, weights);
  }
  void addMultilayerIntraLink(unsigned int layer, unsigned int n1, unsigned int n2, double weight) { m_network.addMultilayerIntraLink(layer, n1, n2, weight); }
  void addMultilayerIntraLinks(const std::vector<unsigned int>& layerIds,
                               const std::vector<unsigned int>& sourceNodeIds,
                               const std::vector<unsigned int>& targetNodeIds,
                               const std::vector<double>& weights)
  {
    m_network.addMultilayerIntraLinks(layerIds, sourceNodeIds, targetNodeIds, weights);
  }
  void addMultilayerInterLink(unsigned int layer1, unsigned int n, unsigned int layer2, double interWeight) { m_network.addMultilayerInterLink(layer1, n, layer2, interWeight); }
  void addMultilayerInterLinks(const std::vector<unsigned int>& sourceLayerIds,
                               const std::vector<unsigned int>& nodeIds,
                               const std::vector<unsigned int>& targetLayerIds,
                               const std::vector<double>& weights)
  {
    m_network.addMultilayerInterLinks(sourceLayerIds, nodeIds, targetLayerIds, weights);
  }

  void addMetaData(unsigned int nodeId, int meta) { m_network.addMetaData(nodeId, meta); }

  void setBipartiteStartId(unsigned int startId) { m_network.setBipartiteStartId(startId); }
  unsigned int bipartiteStartId() const { return m_network.bipartiteStartId(); }
  bool isMultilayerNetwork() const { return m_network.isMultilayerNetwork(); }
  bool haveMemoryInput() const { return m_network.haveMemoryInput(); }
  unsigned int numNodes() const { return m_network.numNodes(); }
  unsigned int numPhysicalNodes() const { return m_network.numPhysicalNodes(); }
  unsigned int numLinks() const { return m_network.numLinks(); }

  std::map<std::pair<unsigned int, unsigned int>, double> getLinks(bool flow) const
  {
    std::map<std::pair<unsigned int, unsigned int>, double> links;

    for (const auto& node : m_network.nodeLinkMap()) {
      const auto sourceId = node.first.id;

      for (const auto& link : node.second) {
        const auto targetId = link.first.id;
        links[{ sourceId, targetId }] = flow ? link.second.flow : link.second.weight;
      }
    }

    return links;
  }

#ifndef SWIGPYTHON
  std::vector<LinkResult> getLinkResults() const
  {
    std::vector<LinkResult> links;
    links.reserve(m_network.numLinks());

    for (const auto& node : m_network.nodeLinkMap()) {
      const auto sourceId = node.first.id;

      for (const auto& link : node.second) {
        const auto targetId = link.first.id;
        links.emplace_back(sourceId, targetId, link.second.weight, link.second.flow);
      }
    }

    return links;
  }
#endif

  std::map<unsigned int, unsigned int> getModules(int level = 1, bool states = false)
  {
    if (haveMemory() && !states) {
      throw std::runtime_error("Cannot get modules on higher-order network without states.");
    }
    std::map<unsigned int, unsigned int> modules;
    for (auto it = iterTree(level); !it.isEnd(); ++it) {
      auto& node = *it;
      if (node.isLeaf()) {
        modules[states ? node.stateId : node.physicalId] = it.moduleId();
      }
    }
    return modules;
  }

  InfomapIterator iterTree(int maxClusterLevel = 1) { return InfomapBase::iterTree(maxClusterLevel); }
  InfomapIteratorPhysical iterTreePhysical(int maxClusterLevel = 1) { return InfomapBase::iterTreePhysical(maxClusterLevel); }
  InfomapLeafModuleIterator iterLeafModules(int maxClusterLevel = 1) { return InfomapBase::iterLeafModules(maxClusterLevel); }
  InfomapLeafIterator iterLeafNodes(int maxClusterLevel = 1) { return InfomapBase::iterLeafNodes(maxClusterLevel); }
  InfomapLeafIteratorPhysical iterLeafNodesPhysical(int maxClusterLevel = 1) { return InfomapBase::iterLeafNodesPhysical(maxClusterLevel); }

  unsigned int numLeafNodes() const { return InfomapBase::numLeafNodes(); }
  unsigned int numTopModules() const { return InfomapBase::numTopModules(); }
  unsigned int numNonTrivialTopModules() const { return InfomapBase::numNonTrivialTopModules(); }
  unsigned int numLevels() const { return InfomapBase::numLevels(); }
  unsigned int maxTreeDepth() const { return InfomapBase::maxTreeDepth(); }
  bool haveModules() const { return InfomapBase::haveModules(); }

  double codelength() const { return InfomapBase::codelength(); }
  const std::vector<double>& codelengths() const { return InfomapBase::codelengths(); }
  double getIndexCodelength() const { return InfomapBase::getIndexCodelength(); }
  double getModuleCodelength() const { return InfomapBase::getModuleCodelength(); }
  double getHierarchicalCodelength() const { return InfomapBase::getHierarchicalCodelength(); }
  double getOneLevelCodelength() const { return InfomapBase::getOneLevelCodelength(); }
  double getRelativeCodelengthSavings() const { return InfomapBase::getRelativeCodelengthSavings(); }
  double getEntropyRate() { return InfomapBase::getEntropyRate(); }
  double getMaxEntropy() { return InfomapBase::getMaxEntropy(); }
  double getMetaCodelength(bool unweighted = false) const { return InfomapBase::getMetaCodelength(unweighted); }
  bool haveMemory() const { return InfomapBase::haveMemory(); }
  double elapsedTime() const { return InfomapBase::getElapsedTime().getElapsedTimeInSec(); }

  const std::map<unsigned int, unsigned int>& getInitialPartition() const { return InfomapBase::getInitialPartition(); }
  InfomapWrapper& setInitialPartition(const std::map<unsigned int, unsigned int>& moduleIds)
  {
    InfomapBase::setInitialPartition(moduleIds);
    return *this;
  }
  std::map<unsigned int, std::vector<unsigned int>> getMultilevelModules(bool states = false) { return InfomapBase::getMultilevelModules(states); }

  std::string writeTree(const std::string& filename = "", bool states = false) { return InfomapBase::writeTree(filename, states); }
  std::string writeFlowTree(const std::string& filename = "", bool states = false) { return InfomapBase::writeFlowTree(filename, states); }
  std::string writeNewickTree(const std::string& filename = "", bool states = false) { return InfomapBase::writeNewickTree(filename, states); }
  std::string writeJsonTree(const std::string& filename = "", bool states = false) { return InfomapBase::writeJsonTree(filename, states); }
  std::string writeCsvTree(const std::string& filename = "", bool states = false) { return InfomapBase::writeCsvTree(filename, states); }
  std::string writeClu(const std::string& filename = "", bool states = false, int moduleIndexLevel = 1) { return InfomapBase::writeClu(filename, states, moduleIndexLevel); }
  void writePajekNetwork(const std::string& filename, bool printFlow = false) const { m_network.writePajekNetwork(filename, printFlow); }
  void writeStateNetwork(const std::string& filename) const { m_network.writeStateNetwork(filename); }

  InfomapWrapper& setDirected(bool value)
  {
    InfomapBase::setDirected(value);
    return *this;
  }
  bool getFlowModelIsSet() const { return flowModelIsSet; }
  bool getStateInput() const { return stateInput; }
  bool getMultilayerInput() const { return multilayerInput; }

#ifndef SWIG
  using InfomapBase::run;
#endif
  void run(const std::string& parameters = "") { InfomapBase::run(parameters); }
};

} // namespace infomap

#endif // INFOMAP_H_
