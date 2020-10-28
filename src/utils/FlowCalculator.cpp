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


#include "FlowCalculator.h"
#include "../utils/Log.h"
#include "../core/StateNetwork.h"
#include <iostream>
#include <cmath>
#include <numeric>
#include <algorithm>

namespace infomap {

template <typename T>
inline void normalize(std::vector<T>& v, const T sum) noexcept
{
  for (auto& numerator : v) {
    numerator /= sum;
  }
}

template <typename T>
inline void normalize(std::vector<T>& v) noexcept
{
  const auto sum = std::accumulate(cbegin(v), cend(v), T {});
  normalize(v, sum);
}

FlowCalculator::FlowCalculator(StateNetwork& network, const Config& config)
{
  Log() << "Calculating global network flow using flow model '" << config.flowModel << "'... " << std::flush;

  // Prepare data in sequence containers for fast access of individual elements
  // Map to zero-based dense indexing
  numNodes = network.numNodes();

  nodeFlow.assign(numNodes, 0.0);
  nodeTeleportRates.assign(numNodes, 0.0);

  nodeOutDegree.assign(numNodes, 0);
  sumLinkOutWeight.assign(numNodes, 0.0);

  unsigned int nodeIndex = 0;
  const auto& nodeLinkMap = network.nodeLinkMap();

  if (network.isBipartite()) {
    // Preserve node order
    for (const auto& node : network.nodes()) {
      const auto nodeId = node.second.id;
      nodeIndexMap[nodeId] = nodeIndex++;
    }

    auto bipartiteStartId = network.bipartiteStartId();
    bipartiteStartIndex = nodeIndexMap[bipartiteStartId];

  } else {
    // Store dangling nodes out-of-order,
    // with dangling nodes first to optimize calculation of dangilng rank

    for (const auto& node : network.nodes()) {
      const auto isDangling = nodeLinkMap.find(node.second) == nodeLinkMap.end();
      if (!isDangling) continue;

      const auto& nodeId = node.second.id;
      nodeIndexMap[nodeId] = nodeIndex++;
    }

    nonDanglingStartIndex = nodeIndex;

    for (const auto& node : network.nodes()) {
      const auto isDangling = nodeLinkMap.find(node.second) == nodeLinkMap.end();
      if (isDangling) continue;

      const auto& nodeId = node.second.id;
      nodeIndexMap[nodeId] = nodeIndex++;
    }
  }

  numLinks = network.numLinks();
  flowLinks.resize(numLinks, { 0, 0, 0.0 });
  sumLinkWeight = network.sumLinkWeight();
  sumUndirLinkWeight = 2 * sumLinkWeight - network.sumSelfLinkWeight();

  if (network.isBipartite()) {
    const auto bipartiteStartId = network.bipartiteStartId();

    for (const auto& node : nodeLinkMap) {
      const auto sourceIsFeature = node.first.id >= bipartiteStartId;
      if (sourceIsFeature) continue;
      bipartiteLinkStartIndex += node.second.size();
    }
  }

  unsigned int linkIndex = 0;
  unsigned int featureLinkIndex = bipartiteLinkStartIndex; // bipartite case

  for (const auto& node : nodeLinkMap) {
    const auto sourceId = node.first.id;
    const auto sourceIndex = nodeIndexMap[sourceId];

    for (const auto& link : node.second) {
      const auto targetId = link.first.id;
      const auto targetIndex = nodeIndexMap[targetId];
      const auto linkWeight = link.second.weight;

      ++nodeOutDegree[sourceIndex];
      sumLinkOutWeight[sourceIndex] += linkWeight;
      nodeFlow[sourceIndex] += linkWeight / sumUndirLinkWeight;

      if (network.isBipartite() && sourceId >= network.bipartiteStartId()) {
        // Link from feature node to ordinary node
        flowLinks[featureLinkIndex].source = sourceIndex;
        flowLinks[featureLinkIndex].target = targetIndex;
        flowLinks[featureLinkIndex].flow = linkWeight;
        ++featureLinkIndex;
      } else {
        // Ordinary link, or unipartite
        flowLinks[linkIndex].source = sourceIndex;
        flowLinks[linkIndex].target = targetIndex;
        flowLinks[linkIndex].flow = linkWeight;
        ++linkIndex;
      }

      if (sourceIndex != targetIndex) {
        if (config.isUndirectedFlow()) {
          ++nodeOutDegree[targetIndex];
          sumLinkOutWeight[targetIndex] += linkWeight;
        }
        if (config.flowModel != FlowModel::outdirdir)
          nodeFlow[targetIndex] += linkWeight / sumUndirLinkWeight;
      }
    }
  }

  bool normalizeNodeFlow = false;

  switch (config.flowModel) {
  case FlowModel::undirected:
    calcUndirectedFlow();
    break;
  case FlowModel::directed:
    if (network.isBipartite() && config.bipartiteTeleportation) {
      calcDirectedBipartiteFlow(network, config);
    } else {
      calcDirectedFlow(network, config);
    }
    break;
  case FlowModel::undirdir:
  case FlowModel::outdirdir:
    calcDirdirFlow(config);
    normalizeNodeFlow = true;
    break;
  case FlowModel::rawdir:
    calcRawdirFlow();
    normalizeNodeFlow = true;
    break;
  }

  finalize(network, config, normalizeNodeFlow);
}

void FlowCalculator::calcUndirectedFlow() noexcept
{
  Log() << "\n  -> Using undirected links.";

  const auto halfWeight = sumUndirLinkWeight / 2;
  for (auto& link : flowLinks) {
    link.flow /= halfWeight;
  }
}

void FlowCalculator::calcDirdirFlow(const Config& config) noexcept
{
  if (config.flowModel == FlowModel::outdirdir)
    Log() << "\n  -> Counting only ingoing links.";
  else
    Log() << "\n  -> Using undirected links, switching to directed after steady state.";

  //Take one last power iteration
  const std::vector<double> nodeFlowSteadyState(nodeFlow);
  nodeFlow.assign(numNodes, 0.0);

  for (const auto& link : flowLinks) {
    nodeFlow[link.target] += nodeFlowSteadyState[link.source] * link.flow / sumLinkOutWeight[link.source];
  }

  double sumNodeFlow = std::accumulate(cbegin(nodeFlow), cend(nodeFlow), 0.0);

  // Update link data to represent flow instead of weight
  for (auto& link : flowLinks) {
    link.flow *= nodeFlowSteadyState[link.source] / sumLinkOutWeight[link.source] / sumNodeFlow;
  }
}

void FlowCalculator::calcRawdirFlow() noexcept
{
  Log() << "\n  -> Using directed links with raw flow.";
  Log() << "\n  -> Total link weight: " << sumLinkWeight << ".";

  // Treat the link weights as flow (after global normalization) and
  // do one power iteration to set the node flow
  nodeFlow.assign(numNodes, 0.0);

  for (auto& link : flowLinks) {
    link.flow /= sumLinkWeight;
    nodeFlow[link.target] += link.flow;
  }
}

struct IterationResult {
  double alpha;
  double beta;
};

template <typename Iteration>
IterationResult powerIterate(double alpha, Iteration&& iter)
{
  unsigned int iterations = 0;
  double beta = 1.0 - alpha;
  double err = 0.0;

  do {
    double oldErr = err;
    err = iter(iterations, alpha, beta);

    // Perturb the system if equilibrium
    if (std::abs(err - oldErr) < 1e-15) {
      alpha += 1.0e-10;
      beta = 1.0 - alpha;
    }

    ++iterations;
  } while (iterations < 200 && (err > 1.0e-15 || iterations < 50));

  Log() << "\n  -> PageRank calculation done in " << iterations << " iterations." << std::endl;

  return { alpha, beta };
}

void FlowCalculator::calcDirectedFlow(const StateNetwork& network, const Config& config) noexcept
{
  Log() << "\n  -> Using " << (config.recordedTeleportation ? "recorded" : "unrecorded") << " teleportation to " << (config.teleportToNodes ? "nodes" : "links") << ". " << std::flush;

  // Calculate the teleport rate distribution
  if (config.teleportToNodes) {
    double sumNodeWeights = 0.0;

    for (const auto& nodeIt : network.nodes()) {
      auto& node = nodeIt.second;
      nodeTeleportRates[nodeIndexMap[node.id]] = node.weight;
      sumNodeWeights += node.weight;
    }

    normalize(nodeTeleportRates, sumNodeWeights);
  } else {
    // Teleport to links

    // Teleport proportionally to out-degree, or in-degree if recorded teleportation.
    for (const auto& link : flowLinks) {
      auto toNode = config.recordedTeleportation ? link.target : link.source;
      nodeTeleportRates[toNode] += link.flow / sumLinkWeight;
    }
  }

  // Normalize link weights with respect to its source nodes total out-link weight;
  for (auto& link : flowLinks) {
    if (sumLinkOutWeight[link.source] > 0) {
      link.flow /= sumLinkOutWeight[link.source];
    }
  }

  std::vector<double> nodeFlowTmp(numNodes, 0.0);
  double danglingRank;

  // Calculate PageRank
  const auto iteration = [&](const auto iteration, const double alpha, const double beta) {
    danglingRank = std::accumulate(cbegin(nodeFlow), cbegin(nodeFlow) + nonDanglingStartIndex, 0.0);

    // Flow from teleportation
    const auto teleportationFlow = alpha + beta * danglingRank;
    for (unsigned int i = 0; i < numNodes; ++i) {
      nodeFlowTmp[i] = teleportationFlow * nodeTeleportRates[i];
    }

    // Flow from links
    for (const auto& link : flowLinks) {
      nodeFlowTmp[link.target] += beta * link.flow * nodeFlow[link.source];
    }

    // Update node flow from the power iteration above and check if converged
    double nodeFlowDiff = -1.0; // Start with -1.0 so we don't have to subtract it later
    double error = 0.0;
    for (unsigned int i = 0; i < numNodes; ++i) {
      nodeFlowDiff += nodeFlowTmp[i];
      error += std::abs(nodeFlowTmp[i] - nodeFlow[i]);
    }

    nodeFlow = nodeFlowTmp;

    // Normalize if needed
    if (std::abs(nodeFlowDiff) > 1.0e-10) {
      Log() << "(Normalizing ranks after " << iteration << " power iterations with error " << nodeFlowDiff << ") ";
      normalize(nodeFlow, nodeFlowDiff + 1.0);
    }

    return error;
  };

  const auto result = powerIterate(config.teleportationProbability, iteration);

  double sumNodeRank = 1.0;
  double beta = result.beta;

  if (!config.recordedTeleportation) {
    // Take one last power iteration excluding the teleportation
    // and normalize node flow
    sumNodeRank = 1.0 - danglingRank;
    nodeFlow.assign(numNodes, 0.0);

    for (const auto& link : flowLinks) {
      nodeFlow[link.target] += link.flow * nodeFlowTmp[link.source] / sumNodeRank;
    }

    beta = 1.0;
  }

  // Update the links with their global flow from the PageRank values.
  // Note: beta is set to 1 if unrecorded teleportation
  for (auto& link : flowLinks) {
    link.flow *= beta * nodeFlowTmp[link.source] / sumNodeRank;
  }
}

void FlowCalculator::calcDirectedBipartiteFlow(const StateNetwork& network, const Config& config) noexcept
{
  Log() << "\n  -> Using bipartite " << (config.recordedTeleportation ? "recorded" : "unrecorded") << " teleportation to " << (config.teleportToNodes ? "nodes" : "links") << ". " << std::flush;

  const auto bipartiteStartId = network.bipartiteStartId();

  if (config.teleportToNodes) {
    for (const auto& nodeIt : network.nodes()) {
      auto& node = nodeIt.second;
      if (node.id < bipartiteStartId) {
        nodeTeleportRates[nodeIndexMap[node.id]] = node.weight;
      }
    }
  } else {
    // Teleport proportionally to out-degree, or in-degree if recorded teleportation.
    // Two-step degree: sum of products between incoming and outgoing links from bipartite nodes

    if (config.recordedTeleportation) {
      for (auto link = begin(flowLinks) + bipartiteLinkStartIndex; link != end(flowLinks); ++link) {
        // target is an ordinary node
        nodeTeleportRates[link->target] += link->flow;
      }
    } else {
      // Unrecorded teleportation

      for (auto link = begin(flowLinks); link != begin(flowLinks) + bipartiteLinkStartIndex; ++link) {
        // source is an ordinary node
        nodeTeleportRates[link->source] += link->flow;
      }
    }
  }

  normalize(nodeTeleportRates);

  nodeFlow = nodeTeleportRates;

  // Normalize link weights with respect to its source nodes total out-link weight;
  for (auto& link : flowLinks) {
    if (sumLinkOutWeight[link.source] > 0) {
      link.flow /= sumLinkOutWeight[link.source];
    }
  }

  std::vector<unsigned int> danglingIndices;
  for (size_t i = 0; i < numNodes; ++i) {
    if (nodeOutDegree[i] == 0) {
      danglingIndices.push_back(i);
    }
  }

  std::vector<double> nodeFlowTmp(numNodes, 0.0);
  double danglingRank;

  // Calculate two-step PageRank
  const auto iteration = [&](const auto iteration, const double alpha, const double beta) {
    danglingRank = 0.0;
    for (const auto& i : danglingIndices) {
      danglingRank += nodeFlow[i];
    }

    // Flow from teleportation
    const auto teleportationFlow = alpha + beta * danglingRank;
    for (unsigned int i = 0; i < bipartiteStartIndex; ++i) {
      nodeFlowTmp[i] = teleportationFlow * nodeTeleportRates[i];
    }

    for (unsigned int i = bipartiteStartIndex; i < numNodes; ++i) {
      nodeFlowTmp[i] = 0.0;
    }

    // Flow from links
    // First step
    for (auto link = begin(flowLinks); link != begin(flowLinks) + bipartiteLinkStartIndex; ++link) {
      nodeFlow[link->target] += beta * link->flow * nodeFlow[link->source];
    }

    // Second step back to primary nodes
    for (auto link = begin(flowLinks) + bipartiteLinkStartIndex; link != end(flowLinks); ++link) {
      nodeFlowTmp[link->target] += link->flow * nodeFlow[link->source];
    }

    // Update node flow from the power iteration above and check if converged
    double nodeFlowDiff = -1.0;
    double error = 0.0;
    for (unsigned int i = 0; i < bipartiteStartIndex; ++i) {
      nodeFlowDiff += nodeFlowTmp[i];
      error += std::abs(nodeFlowTmp[i] - nodeFlow[i]);
    }

    nodeFlow = nodeFlowTmp;

    // Normalize if needed
    if (std::abs(nodeFlowDiff) > 1.0e-10) {
      Log() << "(Normalizing ranks after " << iteration << " power iterations with error " << nodeFlowDiff << ") ";
      normalize(nodeFlow, nodeFlowDiff + 1.0);
    }

    return error;
  };

  const auto result = powerIterate(config.teleportationProbability, iteration);

  double sumNodeRank = 1.0;
  double beta = result.beta;

  if (!config.recordedTeleportation) {
    //Take one last power iteration excluding the teleportation (and normalize node flow to sum 1.0)
    sumNodeRank = 1.0 - danglingRank;
    nodeFlow.assign(numNodes, 0.0);

    for (auto link = begin(flowLinks); link != begin(flowLinks) + bipartiteLinkStartIndex; ++link) {
      nodeFlowTmp[link->target] += link->flow * nodeFlowTmp[link->source];
    }
    // Second step back to primary nodes
    for (auto link = begin(flowLinks) + bipartiteLinkStartIndex; link != end(flowLinks); ++link) {
      nodeFlow[link->target] += link->flow * nodeFlowTmp[link->source];
    }

    beta = 1.0;
  }

  // Update the links with their global flow from the PageRank values.
  // Note: beta is set to 1 if unrecorded teleportation
  for (auto& link : flowLinks) {
    link.flow *= beta * nodeFlowTmp[link.source] / sumNodeRank;
  }
}

void FlowCalculator::finalize(StateNetwork& network, const Config& config, bool normalizeNodeFlow) noexcept
{
  // TODO: Skip bipartite flow adjustment for directed / rawdir / .. ?
  if (network.isBipartite()) {
    Log() << "\n  -> Using bipartite links.";

    if (!config.skipAdjustBipartiteFlow && !config.bipartiteTeleportation) {
      // Only links between ordinary nodes and feature nodes in bipartite network
      // Don't code feature nodes -> distribute all flow from those to ordinary nodes
      for (auto& link : flowLinks) {
        auto sourceIsFeature = link.source >= bipartiteStartIndex;

        if (sourceIsFeature) {
          nodeFlow[link.target] += link.flow;
          nodeFlow[link.source] = 0.0; // Doesn't matter if done multiple times on each node.
        } else {
          nodeFlow[link.source] += link.flow;
          nodeFlow[link.target] = 0.0; // Doesn't matter if done multiple times on each node.
        }
        // TODO: Should flow double before moving to nodes, does it cancel out in normalization?

        // Markov time 2 on the full network will correspond to markov time 1 between the real nodes.
        link.flow *= 2;
      }
      // TODO: Should flow double before moving to nodes, does it cancel out in normalization?

      normalizeNodeFlow = true;

    } else if (config.bipartiteTeleportation) {
      for (auto& link : flowLinks) {
        // Markov time 2 on the full network will correspond to markov time 1 between the real nodes.
        link.flow *= 2;
      }
    }
  }

  if (config.useNodeWeightsAsFlow) {
    Log() << "\n  -> Using node weights as flow.";

    for (auto& nodeIt : network.nodes()) {
      auto& node = nodeIt.second;
      nodeFlow[nodeIndexMap[node.id]] = node.weight;
    }

    normalizeNodeFlow = true;
  }

  if (normalizeNodeFlow) {
    normalize(nodeFlow);
  }

  // Write back flow to network
  // TODO: Add enter/exit flow
  double sumNodeFlow = 0.0;

  for (auto& nodeIt : network.m_nodes) {
    auto& node = nodeIt.second;
    const auto nodeIndex = nodeIndexMap[node.id];
    node.flow = nodeFlow[nodeIndex];
    node.weight = nodeTeleportRates[nodeIndex];
    sumNodeFlow += node.flow;
  }

  double sumLinkFlow = 0.0;
  unsigned int linkIndex = 0;
  auto featureLinkIndex = bipartiteLinkStartIndex;

  for (auto& node : network.m_nodeLinkMap) {
    for (auto& link : node.second) {
      auto& linkData = link.second;
      if (network.isBipartite() && node.first.id >= network.bipartiteStartId()) {
        linkData.flow = flowLinks[featureLinkIndex++].flow;
      } else {
        linkData.flow = flowLinks[linkIndex++].flow;
      }
      sumLinkFlow += linkData.flow;
    }
  }

  Log() << "\n  => Sum node flow: " << sumNodeFlow << ", sum link flow: " << sumLinkFlow << "\n";
}

} // namespace infomap
