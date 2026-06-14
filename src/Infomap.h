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
#include <stdexcept>

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
  InfomapWrapper(const Config& conf) : InfomapBase(conf) {}
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

  void addLink(unsigned int sourceId, unsigned int targetId, double weight = 1.0) { m_network.addLink(sourceId, targetId, weight); }
  void addLink(unsigned int sourceId, unsigned int targetId, unsigned long weight) { m_network.addLink(sourceId, targetId, weight); }
  void addLinks(const std::vector<unsigned int>& sourceIds, const std::vector<unsigned int>& targetIds, const std::vector<double>& weights) { m_network.addLinks(sourceIds, targetIds, weights); }
  void addMultilayerLink(unsigned int layer1, unsigned int n1, unsigned int layer2, unsigned int n2, double weight = 1.0) { m_network.addMultilayerLink(layer1, n1, layer2, n2, weight); }
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

  // Resolve a physical (node_id, layer_id) to the internally generated state id.
  // The network must be built first (state nodes are created on the fly).
  // Throws std::out_of_range if the combination is not present (issue #616).
  unsigned int getMultilayerStateId(unsigned int nodeId, unsigned int layerId) const
  {
    const auto& map = m_network.layerNodeToStateId();
    auto layerIt = map.find(layerId);
    if (layerIt != map.end()) {
      auto nodeIt = layerIt->second.find(nodeId);
      if (nodeIt != layerIt->second.end())
        return nodeIt->second;
    }
    throw std::out_of_range("No state id for (node " + std::to_string(nodeId) + ", layer " + std::to_string(layerId) + "); build the multilayer network before requesting it");
  }

  void setBipartiteStartId(unsigned int startId) { m_network.setBipartiteStartId(startId); }

  std::map<std::pair<unsigned int, unsigned int>, double> getLinks(bool flow) const
  {
    std::map<std::pair<unsigned int, unsigned int>, double> links;

    for (const auto& node : m_network.nodeLinkMap()) {
      const auto sourceId = node.first;

      for (const auto& link : node.second) {
        const auto targetId = link.first;
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
      const auto sourceId = node.first;

      for (const auto& link : node.second) {
        const auto targetId = link.first;
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

  using InfomapBase::codelength;
  using InfomapBase::getEntropyRate;
#if INFOMAP_FEATURE_LOSSY_MAP_EQUATION
  using InfomapBase::getLossyRate;
  using InfomapBase::getLossyDistortion;
#endif
  using InfomapBase::getMultilevelModules;
  using InfomapBase::iterLeafNodes;
  using InfomapBase::iterTree;
  using InfomapBase::run;
};

} // namespace infomap

#endif // INFOMAP_H_
