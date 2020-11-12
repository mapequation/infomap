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
#include <iostream>
#include <cmath>
#include <numeric>
#include "../utils/Log.h"
#include "../core/StateNetwork.h"

namespace infomap {

inline void normalize(std::vector<double>& v, double sum) noexcept {
  for (auto& numerator : v) {
    numerator /= sum;
  }
}

inline void normalize(std::vector<double>& v) noexcept {
  const auto sum = std::accumulate(begin(v), end(v), 0.0);
  normalize(v, sum);
}

FlowCalculator::FlowCalculator(StateNetwork& network, const Config& config)
{
  Log() << "Calculating global network flow using flow model '" << config.flowModel << "'... " << std::flush;

  // Prepare data in sequence containers for fast access of individual elements
  // Map to zero-based dense indexing
  numNodes = network.numNodes();

  m_nodeFlow.assign(numNodes, 0.0);
  m_nodeTeleportRates.assign(numNodes, 0.0);

  nodeOutDegree.assign(numNodes, 0);
  sumLinkOutWeight.assign(numNodes, 0.0);

  unsigned int nodeIndex = 0;
  for (const auto& node : network.nodes()) {
    auto& nodeId = node.second.id;
    m_nodeIndexMap[nodeId] = nodeIndex;
    ++nodeIndex;
  }

  numLinks = network.numLinks();
  m_flowLinks.reserve(numLinks);
  sumLinkWeight = network.sumLinkWeight();
  sumUndirLinkWeight = 2 * sumLinkWeight - network.sumSelfLinkWeight();

  for (const auto& node : network.nodeLinkMap()) {
    auto sourceId = node.first.id;
    auto sourceIndex = m_nodeIndexMap[sourceId];

    const auto& links = node.second;
    for (const auto& link : links) {
      auto targetId = link.first.id;
      auto targetIndex = m_nodeIndexMap[targetId];
      auto linkWeight = link.second.weight;

      ++nodeOutDegree[sourceIndex];
      sumLinkOutWeight[sourceIndex] += linkWeight;
      m_nodeFlow[sourceIndex] += linkWeight / sumUndirLinkWeight;
      m_flowLinks.emplace_back(sourceIndex, targetIndex, linkWeight);

      if (sourceIndex != targetIndex) {
        if (config.isUndirectedFlow()) {
          ++nodeOutDegree[targetIndex];
          sumLinkOutWeight[targetIndex] += linkWeight;
        }
        if (config.flowModel != FlowModel::outdirdir)
          m_nodeFlow[targetIndex] += linkWeight / sumUndirLinkWeight;
      }
    }
  }

  bool normalizeNodeFlow = false;

  switch (config.flowModel) {
  case FlowModel::undirected:
    calcUndirectedFlow();
    break;
  case FlowModel::directed:
    calcDirectedFlow(network, config);
    break;
  case FlowModel::undirdir:
  case FlowModel::outdirdir:
    calcDirdirFlow(config.flowModel);
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
  for (auto& link : m_flowLinks) {
    link.flow /= sumUndirLinkWeight / 2;
  }
}

void FlowCalculator::calcDirdirFlow(FlowModel flowModel) noexcept
{
  if (flowModel == FlowModel::outdirdir)
    Log() << "\n  -> Counting only ingoing links.";
  else
    Log() << "\n  -> Using undirected links, switching to directed after steady state.";

  //Take one last power iteration
  const auto nodeFlowSteadyState = std::vector<double>(m_nodeFlow);
  m_nodeFlow.assign(numNodes, 0.0);

  for (const auto& link : m_flowLinks) {
    m_nodeFlow[link.target] += nodeFlowSteadyState[link.source] * link.flow / sumLinkOutWeight[link.source];
  }

  double sumNodeFlow = std::accumulate(begin(m_nodeFlow), end(m_nodeFlow), 0.0);

  // Update link data to represent flow instead of weight
  for (auto& link : m_flowLinks) {
    link.flow *= nodeFlowSteadyState[link.source] / sumLinkOutWeight[link.source] / sumNodeFlow;
  }
}

void FlowCalculator::calcRawdirFlow() noexcept
{
  // Treat the link weights as flow (after global normalization) and
  // do one power iteration to set the node flow
  m_nodeFlow.assign(numNodes, 0.0);

  for (auto& link : m_flowLinks) {
    link.flow /= sumLinkWeight;
    m_nodeFlow[link.target] += link.flow;
  }

  Log() << "\n  -> Using directed links with raw flow.";
  Log() << "\n  -> Total link weight: " << sumLinkWeight << ".";
}

void FlowCalculator::calcDirectedFlow(const StateNetwork& network, const Config& config) noexcept
{
  Log() << "\n  -> Using " << (config.recordedTeleportation ? "recorded" : "unrecorded") << " teleportation to " << (config.teleportToNodes ? "nodes" : "links") << ". " << std::flush;

  // Calculate the teleport rate distribution
  if (config.teleportToNodes) {
    double sumNodeWeights = 0.0;

    for (const auto& nodeIt : network.nodes()) {
      auto& node = nodeIt.second;
      m_nodeTeleportRates[m_nodeIndexMap[node.id]] = node.weight;
      sumNodeWeights += node.weight;
    }

    normalize(m_nodeTeleportRates, sumNodeWeights);
  } else {
    // Teleport to links

    // Teleport proportionally to out-degree, or in-degree if recorded teleportation.
    for (const auto& link : m_flowLinks) {
      auto toNode = config.recordedTeleportation ? link.target : link.source;
      m_nodeTeleportRates[toNode] += link.flow / sumLinkWeight;
    }
  }

  // Normalize link weights with respect to its source nodes total out-link weight;
  for (auto& link : m_flowLinks) {
    if (sumLinkOutWeight[link.source] > 0) {
      link.flow /= sumLinkOutWeight[link.source];
    }
  }

  // Collect dangling nodes
  auto danglings = std::vector<unsigned int>();
  for (unsigned int i = 0; i < numNodes; ++i) {
    if (nodeOutDegree[i] == 0) {
      danglings.push_back(i);
    }
  }

  // Calculate PageRank
  auto nodeFlowTmp = std::vector<double>(numNodes, 0.0);
  unsigned int numIterations = 0;
  double alpha = config.teleportationProbability;
  double beta = 1.0 - alpha;
  double error = 1.0;
  double danglingRank = 0.0;

  do {
    // Calculate dangling rank
    danglingRank = 0.0;
    for (const auto& danglingIndex : danglings) {
      danglingRank += m_nodeFlow[danglingIndex];
    }

    // Flow from teleportation
    for (unsigned int i = 0; i < numNodes; ++i) {
      nodeFlowTmp[i] = (alpha + beta * danglingRank) * m_nodeTeleportRates[i];
    }

    // Flow from links
    for (const auto& link : m_flowLinks) {
      nodeFlowTmp[link.target] += beta * link.flow * m_nodeFlow[link.source];
    }

    // Update node flow from the power iteration above and check if converged
    double sumNodeFlow = 0.0;
    double oldError = error;
    error = 0.0;
    for (unsigned int i = 0; i < numNodes; ++i) {
      sumNodeFlow += nodeFlowTmp[i];
      error += std::abs(nodeFlowTmp[i] - m_nodeFlow[i]);
      m_nodeFlow[i] = nodeFlowTmp[i];
    }

    // Normalize if needed
    if (std::abs(sumNodeFlow - 1.0) > 1.0e-10) {
      Log() << "(Normalizing ranks after " << numIterations << " power iterations with error " << (sumNodeFlow - 1.0) << ") ";

      for (unsigned int i = 0; i < numNodes; ++i) {
        m_nodeFlow[i] /= sumNodeFlow;
      }
    }

    // Perturb the system if equilibrium
    if (std::abs(error - oldError) < 1e-15) {
      alpha += 1.0e-10;
      beta = 1.0 - alpha;
    }

    numIterations++;
  } while ((numIterations < 200) && (error > 1.0e-15 || numIterations < 50));

  double sumNodeRank = 1.0;

  if (!config.recordedTeleportation) {
    // Take one last power iteration excluding the teleportation
    // and normalize node flow
    sumNodeRank = 1.0 - danglingRank;
    m_nodeFlow.assign(numNodes, 0.0);

    for (const auto& link : m_flowLinks) {
      m_nodeFlow[link.target] += link.flow * nodeFlowTmp[link.source] / sumNodeRank;
    }

    beta = 1.0;
  }


  // Update the links with their global flow from the PageRank values.
  // Note: beta is set to 1 if unrecorded teleportation
  for (auto& link : m_flowLinks) {
    link.flow *= beta * nodeFlowTmp[link.source] / sumNodeRank;
  }

  Log() << "\n  -> PageRank calculation done in " << numIterations << " iterations." << std::endl;
}

void FlowCalculator::finalize(StateNetwork& network, const Config& config, bool normalizeNodeFlow) noexcept
{
  // TODO: Skip bipartite flow adjustment for directed / rawdir / .. ?
  if (network.isBipartite() && !config.skipAdjustBipartiteFlow) {
    Log() << "\n  -> Using bipartite links.";
    // Only links between ordinary nodes and feature nodes in bipartite network
    // Don't code feature nodes -> distribute all flow from those to ordinary nodes
    auto bipartiteStartId = network.bipartiteStartId();
    auto bipartiteStartIndex = m_nodeIndexMap[bipartiteStartId];

    for (auto& link : m_flowLinks) {
      auto sourceIsFeature = link.source >= bipartiteStartIndex;

      if (sourceIsFeature) {
        m_nodeFlow[link.target] += link.flow;
        m_nodeFlow[link.source] = 0.0; // Doesn't matter if done multiple times on each node.
      } else {
        m_nodeFlow[link.source] += link.flow;
        m_nodeFlow[link.target] = 0.0; // Doesn't matter if done multiple times on each node.
      }
      //TODO: Should flow double before moving to nodes, does it cancel out in normalization?

      // Markov time 2 on the full network will correspond to markov time 1 between the real nodes.
      link.flow *= 2;
    }

    normalizeNodeFlow = true;
  }

  if (config.useNodeWeightsAsFlow) {
    Log() << "\n  -> Using node weights as flow.";

    for (auto& nodeIt : network.nodes()) {
      auto& node = nodeIt.second;
      m_nodeFlow[m_nodeIndexMap[node.id]] = node.weight;
    }

    normalizeNodeFlow = true;
  }

  if (normalizeNodeFlow) {
    normalize(m_nodeFlow);
  }

  // Write back flow to network
  //TODO: Add enter/exit flow
  double sumNodeFlow = 0.0;
  unsigned int nodeIndex = 0;

  for (auto& nodeIt : network.m_nodes) {
    auto& node = nodeIt.second;
    node.flow = m_nodeFlow[nodeIndex];
    sumNodeFlow += node.flow;
    ++nodeIndex;
  }

  double sumLinkFlow = 0.0;
  unsigned int linkIndex = 0;

  for (auto& node : network.m_nodeLinkMap) {
    for (auto& link : node.second) {
      auto& linkData = link.second;
      linkData.flow = m_flowLinks[linkIndex].flow;
      sumLinkFlow += linkData.flow;
      ++linkIndex;
    }
  }

  Log() << "\n  => Sum node flow: " << sumNodeFlow << ", sum link flow: " << sumLinkFlow << "\n";
}

} // namespace infomap
