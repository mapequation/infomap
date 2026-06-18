/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "StateNetwork.h"
#include "../utils/FlowCalculator.h"
#include "../utils/Log.h"
#include "../io/SafeFile.h"
#include <algorithm>
#include <cstdlib>
#include <deque>
#include <stdexcept>
#include <utility>

namespace infomap {

std::pair<StateNetwork::NodeMap::iterator, bool> StateNetwork::addStateNode(const StateNode& node)
{
  auto ret = m_nodes.insert(StateNetwork::NodeMap::value_type(node.id, node));
  if (ret.second) {
    // If state node didn't exist, also create the associated physical node
    addPhysicalNode(node.physicalId);
    if (node.id != node.physicalId) {
      m_haveMemoryInput = true;
    }
  }
  return ret;
}

std::pair<StateNetwork::NodeMap::iterator, bool> StateNetwork::addStateNode(unsigned int id, unsigned int physId)
{
  m_higherOrderInputMethodCalled = true;
  return addStateNode(StateNode(id, physId));
}

std::pair<StateNetwork::NodeMap::iterator, bool> StateNetwork::addNode(unsigned int id)
{
  return addStateNode(StateNode(id));
}

std::pair<StateNetwork::NodeMap::iterator, bool> StateNetwork::addNode(unsigned int id, double weight)
{
  auto ret = addNode(id);
  auto& node = ret.first->second;
  node.weight = weight;
  return ret;
}

std::pair<StateNetwork::NodeMap::iterator, bool> StateNetwork::addNode(unsigned int id, std::string name)
{
  m_names[id] = std::move(name);
  return addNode(id);
}

std::pair<StateNetwork::NodeMap::iterator, bool> StateNetwork::addNode(unsigned int id, std::string name, double weight)
{
  m_names[id] = std::move(name);
  return addNode(id, weight);
}

StateNetwork::PhysNode& StateNetwork::addPhysicalNode(unsigned int physId)
{
  auto& physNode = m_physNodes[physId];
  physNode.physId = physId;
  m_sumNodeWeight += 1.0;
  return physNode;
}

StateNetwork::PhysNode& StateNetwork::addPhysicalNode(unsigned int physId, double weight)
{
  auto& physNode = addPhysicalNode(physId);
  physNode.weight = weight;
  m_sumNodeWeight += weight;
  return physNode;
}

StateNetwork::PhysNode& StateNetwork::addPhysicalNode(unsigned int physId, const std::string& name)
{
  auto& physNode = addPhysicalNode(physId);
  m_names[physId] = name;
  m_sumNodeWeight += 1.0;
  return physNode;
}

StateNetwork::PhysNode& StateNetwork::addPhysicalNode(unsigned int physId, double weight, const std::string& name)
{
  auto& physNode = addPhysicalNode(physId);
  physNode.weight = weight;
  m_sumNodeWeight += weight;
  m_names[physId] = name;
  return physNode;
}

std::pair<std::map<unsigned int, std::string>::iterator, bool> StateNetwork::addName(unsigned int id, const std::string& name)
{
  return m_names.insert(std::make_pair(id, name));
}

bool StateNetwork::addLink(unsigned int sourceId, unsigned int targetId, double weight)
{
  // THROWAWAY instrumentation: measure pure parse cost by skipping all storage
  // work (node + link map inserts). Combine with INFOMAP_STOP_AFTER=read_build.
  static const bool skipBuild = std::getenv("INFOMAP_SKIP_BUILD") != nullptr;
  if (skipBuild) {
    return false;
  }

  if (weight < m_config.weightThreshold || weight <= 0) {
    ++m_numLinksIgnoredByWeightThreshold;
    m_totalLinkWeightIgnored += weight;
    return false;
  }
  m_totalLinkWeightAdded += weight;

  if (sourceId == targetId) {
    ++m_numSelfLinksFound;
    if (m_config.noSelfLinks) {
      return false;
    }
    ++m_numSelfLinks;
    m_sumSelfLinkWeight += weight;
  }

  addNode(sourceId);
  addNode(targetId);

  m_sumLinkWeight += weight;

  if (m_useMapBuild) {
    return addLinkToMap(sourceId, targetId, weight); // mode B (multilayer)
  }

  // mode A (first-order): deferred storage -- append now, aggregate in
  // finalizeLinks(). New-vs-aggregated isn't known until then, so the return
  // value becomes "accepted" (passed the filters above) rather than "new unique".
  if (m_linksFinalized) {
    definalize(); // re-entered build (e.g. accumulate=true read, addLink after run)
  }
  ++m_rawLinkCount;
  m_linkBuffer.push_back({ sourceId, targetId, weight });
  return true;
}

bool StateNetwork::addLinkToMap(unsigned int sourceId, unsigned int targetId, double weight)
{
  // Mode B (multilayer): eager aggregation into the nested map, exactly as the
  // pre-CSR build did. Multilayer expansion reads this map and outWeights()
  // mid-build, so both must stay populated with the original arrival-order sums.
  if (m_linksFinalized) {
    // A link added after a CSR build: the map is still the source of truth in
    // mode B, so just invalidate the cached CSR for the next finalize.
    m_linksFinalized = false;
  }

  ++m_numLinks;

  bool addedNewLink = true;

  // Aggregate link weights if they are defined more than once
  // TODO: Sort source/target order if undirected?
  auto& outLinks = m_nodeLinkMap[sourceId];
  if (outLinks.empty()) {
    outLinks[targetId] = weight;
  } else {
    auto ret = outLinks.insert(std::make_pair(targetId, LinkData(weight)));
    auto& linkData = ret.first->second;
    if (!ret.second) {
      linkData.weight += weight;
      ++m_numAggregatedLinks;
      --m_numLinks;
      addedNewLink = false;
    } else {
      linkData.weight = weight;
    }
  }
  m_outWeights[sourceId] += weight;
  return addedNewLink;
}

bool StateNetwork::addLink(unsigned int sourceId, unsigned int targetId, unsigned long weight)
{
  return addLink(sourceId, targetId, static_cast<double>(weight));
}

void StateNetwork::addLinks(const std::vector<unsigned int>& sourceIds, const std::vector<unsigned int>& targetIds, const std::vector<double>& weights)
{
  if (sourceIds.size() != targetIds.size() || sourceIds.size() != weights.size()) {
    throw std::invalid_argument("sourceIds, targetIds, and weights must have the same length");
  }

  for (std::size_t i = 0; i < sourceIds.size(); ++i) {
    addLink(sourceIds[i], targetIds[i], weights[i]);
  }
}

bool StateNetwork::removeLink(unsigned int sourceId, unsigned int targetId)
{
  auto itSource = m_nodeLinkMap.find(sourceId);
  if (itSource == m_nodeLinkMap.end()) {
    return false;
  }

  auto& targetMap = itSource->second;
  auto itTarget = targetMap.find(targetId);

  if (itTarget == targetMap.end()) {
    return false;
  }

  double weight = itTarget->second.weight;

  targetMap.erase(itTarget);
  if (targetMap.empty()) {
    m_nodeLinkMap.erase(itSource);
  } else {
    --m_numAggregatedLinks;
  }

  --m_numLinks;
  m_sumLinkWeight -= weight;
  m_totalLinkWeightAdded -= weight;

  if (sourceId == targetId) {
    --m_numSelfLinksFound;
    --m_numSelfLinks;
    m_sumSelfLinkWeight -= weight;
  }

  m_outWeights[sourceId] -= weight;

  return true;
}

bool StateNetwork::undirectedToDirected()
{
  // Collect links in separate data structure to not risk iterating newly added links
  if (!m_config.isUndirectedFlow()) {
    return false;
  }
  std::deque<StateLink> oppositeLinks;
  for (auto& linkIt : m_nodeLinkMap) {
    unsigned int sourceId = linkIt.first;
    const auto& subLinks = linkIt.second;
    for (auto& subIt : subLinks) {
      unsigned int targetId = subIt.first;
      if (targetId == sourceId) {
        continue; // Self-links are treated as directed on undirected networks
      }
      double weight = subIt.second.weight;
      oppositeLinks.emplace_back(targetId, sourceId, weight);
    }
  }
  for (auto& link : oppositeLinks) {
    addLink(link.source, link.target, link.weight);
  }
  return true;
}

void StateNetwork::clearLinks()
{
  m_nodeLinkMap.clear();
}

void StateNetwork::clear()
{
  m_nodes.clear();
  m_nodeLinkMap.clear();
  m_physNodes.clear();
  m_outWeights.clear();
  m_names.clear();

  m_haveDirectedInput = false;
  m_haveMemoryInput = false;
  m_numStateNodesFound = 0;
  m_numLinks = 0;
  m_numSelfLinksFound = 0;
  m_sumLinkWeight = 0.0;
  m_numSelfLinks = 0;
  m_sumSelfLinkWeight = 0.0;
  m_numAggregatedLinks = 0;
  m_totalLinkWeightAdded = 0.0;
  m_numLinksIgnoredByWeightThreshold = 0;
  m_totalLinkWeightIgnored = 0.0;
}

void StateNetwork::writeStateNetwork(const std::string& filename) const
{
  if (filename.empty())
    throw std::runtime_error("writeStateNetwork called with empty filename");

  SafeOutFile outFile(filename, std::ios_base::out, m_config.overwriteOutput());

  outFile << "# v" << INFOMAP_VERSION << "\n"
          << "# ./Infomap " << m_config.parsedString << "\n";

  if (!m_names.empty()) {
    outFile << "*Vertices\n";
    for (auto& nameIt : m_names) {
      auto& physId = nameIt.first;
      auto& name = nameIt.second;
      outFile << physId << " \"" << name << "\"\n";
    }
  }

  outFile << "*States\n";
  outFile << "# stateId physicalId\n";
  for (const auto& nodeIt : nodes()) {
    const auto& node = nodeIt.second;
    outFile << node.id << " " << node.physicalId;
    // Optional name
    if (!node.name.empty())
      outFile << " \"" << node.name << "\"";
    outFile << "\n";
  }

  outFile << "*Links\n";
  forEachLink([&](unsigned int s, unsigned int t, double weight, double&) {
    outFile << nodeId(s) << " " << nodeId(t) << " " << weight << "\n";
  });
  outFile.commit();
}

void StateNetwork::writePajekNetwork(const std::string& filename, bool printFlow) const
{
  if (filename.empty())
    throw std::runtime_error("writePajekNetwork called with empty filename");

  SafeOutFile outFile(filename, std::ios_base::out, m_config.overwriteOutput());

  outFile << "# v" << INFOMAP_VERSION << "\n"
          << "# ./Infomap " << m_config.parsedString << "\n";
  if (haveMemoryInput())
    outFile << "# State network as physical network\n";

  outFile << "*Vertices\n";
  outFile << "#id name " << (printFlow ? "flow" : "weight") << "\n";
  for (const auto& nodeIt : nodes()) {
    const auto& node = nodeIt.second;
    outFile << node.id << " \"";
    // Name, default to id
    const auto& nameIt = haveMemoryInput() ? m_names.end() : m_names.find(node.id);
    if (nameIt != m_names.end())
      outFile << nameIt->second;
    else
      outFile << node.id;
    outFile << "\" " << (printFlow ? node.flow : node.weight) << "\n";
  }

  outFile << (m_config.printAsUndirected() ? "*Edges" : "*Arcs") << "\n";
  outFile << "#source target " << (printFlow ? "flow" : "weight") << "\n";
  forEachLink([&](unsigned int s, unsigned int t, double weight, double& flow) {
    outFile << nodeId(s) << " " << nodeId(t) << " " << (printFlow ? flow : weight) << "\n";
  });
  outFile.commit();
}

std::pair<StateNetwork::NodeMap::iterator, bool> StateNetwork::addStateNodeWithAutogeneratedId(unsigned int physId)
{
  // Keys sorted with std::less comparator, so last key is the largest
  unsigned int stateId = m_nodes.empty() ? 0 : m_nodes.crbegin()->first + 1;
  return addStateNode(stateId, physId);
}

std::pair<StateNetwork::NodeMap::iterator, bool> StateNetwork::addStateNodeWithDeterministicId(unsigned int physId, unsigned int layerId, unsigned int numLayersLog2)
{
  unsigned int stateId = physId << (numLayersLog2 + 1) | layerId;
  return addStateNode(stateId, physId);
}

unsigned int StateNetwork::indexOfId(unsigned int id) const
{
  return static_cast<unsigned int>(
      std::lower_bound(m_nodeIds.begin(), m_nodeIds.end(), id) - m_nodeIds.begin());
}

void StateNetwork::finalizeLinks()
{
  // Idempotent: build the consumed CSR exactly once. A second call would rebuild
  // from a freed buffer (mode A -> empty CSR) or re-read flow-less map entries
  // (mode B -> wiped flows), so mutations after finalize go through definalize()
  // / addLinkToMap() which reset m_linksFinalized instead of rebuilding here.
  if (m_linksFinalized) {
    return;
  }
  if (m_useMapBuild) {
    buildCsrFromMap(); // mode B (multilayer): sorted+aggregated map -> CSR
  } else {
    buildCsrFromBuffer(); // mode A (first-order): sort + merge the flat buffer -> CSR
  }
  m_linksFinalized = true;
}

void StateNetwork::buildCsrFromBuffer()
{
  // Node index = sorted position in m_nodes (every link endpoint was addNode'd
  // during addLink), so index->id is direct and id->index is a binary search.
  m_nodeIds.clear();
  m_nodeIds.reserve(m_nodes.size());
  for (const auto& n : m_nodes) {
    m_nodeIds.push_back(n.first);
  }

  // Sort an index permutation rather than the 16-byte triples: std::sort is
  // in-place (no N-element temporary, unlike stable_sort), and the index array
  // is 4 B/link instead of 16, cutting the finalize transient peak. The
  // comparator orders by (source, target) and breaks ties on the arrival index,
  // a total order whose ties preserve arrival order -- so the left-to-right
  // weight merge below sums duplicates exactly as the old map's '+=' did,
  // bit-for-bit identical.
  const std::size_t numTriples = m_linkBuffer.size();
  std::vector<unsigned int> order(numTriples);
  for (std::size_t i = 0; i < numTriples; ++i) {
    order[i] = static_cast<unsigned int>(i);
  }
  const auto& buf = m_linkBuffer;
  std::sort(order.begin(), order.end(), [&buf](unsigned int a, unsigned int b) {
    if (buf[a].source != buf[b].source) return buf[a].source < buf[b].source;
    if (buf[a].target != buf[b].target) return buf[a].target < buf[b].target;
    return a < b; // arrival-order tiebreak keeps the weight sum bit-exact
  });

  const unsigned int numNodes = static_cast<unsigned int>(m_nodeIds.size());
  m_linkOffsets.assign(numNodes + 1, 0);
  m_linkTargets.clear();
  m_linkWeights.clear();
  m_linkTargets.reserve(numTriples);
  m_linkWeights.reserve(numTriples);

  std::size_t k = 0;
  unsigned int curSourceIndex = 0;
  while (k < numTriples) {
    const unsigned int src = buf[order[k]].source;
    const unsigned int tgt = buf[order[k]].target;
    double w = buf[order[k]].weight;
    std::size_t m = k + 1;
    while (m < numTriples && buf[order[m]].source == src && buf[order[m]].target == tgt) {
      w += buf[order[m]].weight; // arrival order via the index tiebreak
      ++m;
    }
    const unsigned int srcIdx = indexOfId(src);
    while (curSourceIndex < srcIdx) {
      m_linkOffsets[++curSourceIndex] = static_cast<unsigned int>(m_linkTargets.size());
    }
    m_linkTargets.push_back(indexOfId(tgt));
    m_linkWeights.push_back(w);
    k = m;
  }
  while (curSourceIndex < numNodes) {
    m_linkOffsets[++curSourceIndex] = static_cast<unsigned int>(m_linkTargets.size());
  }

  // Free the build buffer + permutation before sizing the flow array, to keep
  // the transient peak down.
  std::vector<LinkTriple>().swap(m_linkBuffer);
  std::vector<unsigned int>().swap(order);

  m_numLinks = static_cast<unsigned int>(m_linkTargets.size());
  m_numAggregatedLinks = m_rawLinkCount - m_numLinks;
  m_linkFlows.assign(m_linkTargets.size(), 0.0);

  // m_outWeights is deliberately NOT derived in mode A: first-order consumers
  // don't read it (only the multilayer mode-B expansion does, where it's kept
  // eager in addLinkToMap). Leaving it empty avoids a ~per-source std::map.
  m_outWeights.clear();
}

void StateNetwork::definalize()
{
  // Move CSR rows back into the flat buffer so building can continue. Merged
  // weights become single occurrences; a later finalizeLinks() re-sorts and
  // re-merges them (and any newly appended links).
  m_linkBuffer.clear();
  m_rawLinkCount = 0;
  for (unsigned int s = 0; s < m_nodeIds.size(); ++s) {
    for (unsigned int e = m_linkOffsets[s]; e < m_linkOffsets[s + 1]; ++e) {
      m_linkBuffer.push_back({ m_nodeIds[s], m_nodeIds[m_linkTargets[e]], m_linkWeights[e] });
      ++m_rawLinkCount;
    }
  }
  m_linkOffsets.clear();
  m_linkTargets.clear();
  m_linkWeights.clear();
  m_linkFlows.clear();
  m_linksFinalized = false;
}

void StateNetwork::buildCsrFromMap()
{
  m_nodeIds.clear();
  m_nodeIds.reserve(m_nodes.size());
  for (const auto& n : m_nodes) {
    m_nodeIds.push_back(n.first);
  }

  const unsigned int numNodes = static_cast<unsigned int>(m_nodeIds.size());
  m_linkOffsets.assign(numNodes + 1, 0);
  m_linkTargets.clear();
  m_linkWeights.clear();
  m_linkFlows.clear();
  m_linkTargets.reserve(m_numLinks);
  m_linkWeights.reserve(m_numLinks);
  m_linkFlows.reserve(m_numLinks);

  unsigned int curSourceIndex = 0;
  unsigned int linkCount = 0;
  for (const auto& node : m_nodeLinkMap) {
    const unsigned int srcIdx = indexOfId(node.first);
    while (curSourceIndex < srcIdx) {
      m_linkOffsets[++curSourceIndex] = linkCount;
    }
    for (const auto& link : node.second) {
      m_linkTargets.push_back(indexOfId(link.first));
      m_linkWeights.push_back(link.second.weight);
      m_linkFlows.push_back(link.second.flow);
      ++linkCount;
    }
  }
  while (curSourceIndex < numNodes) {
    m_linkOffsets[++curSourceIndex] = linkCount;
  }
}

} // namespace infomap
