/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_AGPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "FlowCalculator.h"
#include "../utils/Log.h"
#include "../utils/infomath.h"
#include "../core/StateNetwork.h"
#include <iostream>
#include <cmath>
#include <numeric>
#include <limits>
#include <algorithm>
#include <functional>
#include <unordered_map>

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
    : numNodes(network.numNodes())
{
  Log() << "Calculating global network flow using flow model '" << config.flowModel << "'... " << std::flush;

  // Prepare data in sequence containers for fast access of individual elements
  // Map to zero-based dense indexing
  nodeFlow.assign(numNodes, 0.0);
  nodeTeleportWeights.assign(numNodes, 0.0); // Fraction of teleportation flow landing on node i

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
    if (config.flowModel != FlowModel::directed) {
      // Preserve node order
      for (const auto& node : network.nodes()) {
        const auto nodeId = node.second.id;
        nodeIndexMap[nodeId] = nodeIndex++;
      }
    } else {
      // Store dangling nodes out-of-order,
      // with dangling nodes first to optimize calculation of dangling rank

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
  }

  flowLinks.resize(network.numLinks(), { 0, 0, 0.0 });
  sumLinkWeight = network.sumLinkWeight();
  sumWeightedDegree = network.sumWeightedDegree();

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
  double undirectedLinkNormalization = 2 * sumLinkWeight - network.sumSelfLinkWeight();

  for (const auto& node : nodeLinkMap) {
    const auto sourceId = node.first.id;
    const auto sourceIndex = nodeIndexMap[sourceId];

    for (const auto& link : node.second) {
      const auto targetId = link.first.id;
      const auto targetIndex = nodeIndexMap[targetId];
      const auto linkWeight = link.second.weight;

      ++nodeOutDegree[sourceIndex];
      sumLinkOutWeight[sourceIndex] += linkWeight;
      nodeFlow[sourceIndex] += linkWeight / undirectedLinkNormalization;

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
        if (config.flowModel != FlowModel::outdirdir) {
          nodeFlow[targetIndex] += linkWeight / undirectedLinkNormalization;
        }
      }
    }
  }

  bool normalizeNodeFlow = false;

  switch (config.flowModel) {
  case FlowModel::undirected:
    if (config.regularized) {
      if (config.isMultilayerNetwork())
        calcUndirectedRegularizedMultilayerFlow(network, config);
      else
        calcUndirectedRegularizedFlow(network, config);
    } else {
      calcUndirectedFlow();
    }
    break;
  case FlowModel::directed:
    if (network.isBipartite() && config.bipartiteTeleportation) {
      calcDirectedBipartiteFlow(network, config);
    } else {
      if (config.regularized) {
        if (config.isMultilayerNetwork())
          calcDirectedRegularizedMultilayerFlow(network, config);
        else
          calcDirectedRegularizedFlow(network, config);
      } else {
        calcDirectedFlow(network, config);
      }
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
  case FlowModel::precomputed:
    usePrecomputedFlow(network, config);
    normalizeNodeFlow = true;
    break;
  }

  finalize(network, config, normalizeNodeFlow);
}

void FlowCalculator::calcUndirectedFlow() noexcept
{
  Log() << "\n  -> Using undirected links.";

  // Flow is outgoing transition probability times source node flow
  // = w_ij / s_ij * s_ij / sum(s_ij) = w_ij / sum(s_ij)
  // Count twice for non-loops to cover flow in both directions
  // Assuming convention to treat self-links as directed

  for (auto& link : flowLinks) {
    link.flow /= sumWeightedDegree;
    if (link.source != link.target) {
      link.flow *= 2;
    }
  }
}

void FlowCalculator::calcDirdirFlow(const Config& config) noexcept
{
  if (config.flowModel == FlowModel::outdirdir)
    Log() << "\n  -> Counting only ingoing links.";
  else
    Log() << "\n  -> Using undirected links, switching to directed after steady state.";

  // Take one last power iteration
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

void FlowCalculator::usePrecomputedFlow(const StateNetwork& network, const Config&)
{
  Log() << "\n  -> Using directed links with precomputed flow from input data.";
  Log() << "\n  -> Total link flow: " << sumLinkWeight << ".";

  if (network.haveFileInput()) {
    if (network.haveMemoryInput() && !network.haveStateNodeWeights()) {
      Log() << std::endl;
      throw std::runtime_error("Missing node flow in input data. Should be passed as a third field under a *States section.");
    }
    if (!network.haveMemoryInput() && !network.haveNodeWeights()) {
      Log() << std::endl;
      throw std::runtime_error("Missing node flow in input data. Should be passed as a third field under a *Vertices section.");
    }
  }

  // Treat the link weights as flow
  nodeFlow.assign(numNodes, 0.0);
  double sumFlow = 0.0;

  for (const auto& nodeIt : network.nodes()) {
    auto& node = nodeIt.second;
    nodeFlow[nodeIndexMap[node.id]] = node.weight;
    sumFlow += node.weight;
  }
  Log() << "\n  -> Total node flow: " << sumFlow << ".";

  if (infomath::isEqual(sumFlow, 0)) {
    throw std::runtime_error("Missing node flow. Set it on the node weight property.");
  }
  if (!infomath::isEqual(sumFlow, 1)) {
    if (infomath::isEqual(sumFlow, numNodes) && infomath::isEqual(nodeFlow[0], 1)) {
      Log() << "\n  Warning: Node flow sums to the number of nodes, is node flow provided or is default node weights used? Normalizing.";
    } else {
      Log() << "\n  Warning: Node flow sums to " << sumFlow << ", normalizing.";
    }
    for (unsigned int i = 0; i < numNodes; ++i) {
      nodeFlow[i] /= sumFlow;
    }
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
    // double oldErr = err;
    err = iter(iterations, alpha, beta);

    // // Perturb the system if equilibrium
    // if (std::abs(err - oldErr) < 1e-15) {
    //   alpha += 1.0e-10;
    //   beta = 1.0 - alpha;
    // }

    ++iterations;
  } while (iterations < 200 && (err > 1.0e-15 || iterations < 50));

  Log() << "\n  -> PageRank calculation done in " << iterations << " iterations.\n";

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
      nodeTeleportWeights[nodeIndexMap[node.id]] = node.weight;
      sumNodeWeights += node.weight;
    }

    normalize(nodeTeleportWeights, sumNodeWeights);
  } else {
    // Teleport to links

    // Teleport proportionally to out-degree, or in-degree if recorded teleportation.
    for (const auto& link : flowLinks) {
      auto toNode = config.recordedTeleportation ? link.target : link.source;
      nodeTeleportWeights[toNode] += link.flow / sumLinkWeight;
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
  const auto iteration = [&](const auto iter, const double alpha, const double beta) {
    danglingRank = std::accumulate(cbegin(nodeFlow), cbegin(nodeFlow) + nonDanglingStartIndex, 0.0);

    // Flow from teleportation
    const auto teleportationFlow = alpha + beta * danglingRank;
    for (unsigned int i = 0; i < numNodes; ++i) {
      nodeFlowTmp[i] = teleportationFlow * nodeTeleportWeights[i];
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
      Log() << "(Normalizing flow after " << iter << " power iterations with error " << nodeFlowDiff << ") ";
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

  if (config.multilayerSelfInterLinks) {
    // Log() << "(adjusting flow for self-inter-links...) ";
    // auto r = config.multilayerRelaxRate;
    // for (unsigned int i = 0; i < numNodes; ++i) {
    //   nodeFlow[i] *= (1 - r);
    // }
    // double sumFlow = 1 - r;

    // // for (auto& nodeIt : network.m_nodes) {
    // //   auto& node = nodeIt.second;
    // //   const auto nodeIndex = nodeIndexMap[node.id];
    // // }

    // for (const auto& link : flowLinks) {
    //   bool isIntra = true; //asdf
    //   if (isIntra) {
    //     double f = r * nodeFlowTmp[link.source] * link.flow;
    //     sumFlow += f;
    //     nodeFlow[link.target] += f;
    //   }
    // }
    // for (unsigned int i = 0; i < numNodes; ++i) {
    //   nodeFlow[i] /= sumFlow;
    // }
  }

  // Update the links with their global flow from the PageRank values.
  // Note: beta is set to 1 if unrecorded teleportation
  for (auto& link : flowLinks) {
    link.flow *= beta * nodeFlowTmp[link.source] / sumNodeRank;
  }
}

void FlowCalculator::calcDirectedRegularizedFlow(const StateNetwork& network, const Config& config) noexcept
{
  Log() << "\n  -> Using recorded teleportation to nodes according to a fully connected Bayesian prior. " << std::flush;

  // Calculate node weights w_i = s_i/k_i, where s_i is the node strength (weighted degree) and k_i the (unweighted) degree
  unsigned int N = network.numNodes();

  std::vector<unsigned int> k_out(N, 0);
  std::vector<unsigned int> k_in(N, 0);
  std::vector<double> s_out(N, 0);
  std::vector<double> s_in(N, 0);
  double sum_s = sumWeightedDegree;
  unsigned int sum_k = network.sumDegree();
  double average_weight = sum_s / sum_k;

  for (auto& link : flowLinks) {
    k_out[link.source] += 1;
    s_out[link.source] += link.flow;
    k_in[link.target] += 1;
    s_in[link.target] += link.flow;
  }

  double min_u_out = std::numeric_limits<double>::max();
  double min_u_in = std::numeric_limits<double>::max();
  for (unsigned int i = 0; i < N; ++i) {
    if (k_out[i] > 0) {
      min_u_out = std::min(min_u_out, s_out[i] / k_out[i]);
    }
    if (k_in[i] > 0) {
      min_u_in = std::min(min_u_in, s_in[i] / k_in[i]);
    }
  }

  auto u_out = [s_out, k_out, min_u_out](auto i) { return k_out[i] == 0 ? min_u_out : s_out[i] / k_out[i]; };
  auto u_in = [s_in, k_in, min_u_in](auto i) { return k_in[i] == 0 ? min_u_in : s_in[i] / k_in[i]; };

  unsigned int numNodesAsTeleportationTargets = config.noSelfLinks ? N - 1 : N;
  double lambda = config.regularizationStrength * std::log(N) / numNodesAsTeleportationTargets;
  double u_t = average_weight;

  double sum_u_in = 0.0;
  for (unsigned int i = 0; i < N; ++i) {
    sum_u_in += u_in(i);
  }

  for (unsigned int i = 0; i < N; ++i) {
    nodeTeleportWeights[i] = u_in(i) / sum_u_in;
  }

  std::function<double(unsigned int)> t_out_withoutSelfLinks = [lambda, u_t, u_out, u_in, sum_u_in](unsigned int i) { return lambda / u_t * u_out(i) * (sum_u_in - u_in(i)); };
  std::function<double(unsigned int)> t_out_withSelfLinks = [lambda, u_t, u_out, sum_u_in](unsigned int i) { return lambda / u_t * u_out(i) * sum_u_in; };
  auto t_out = config.noSelfLinks ? t_out_withoutSelfLinks : t_out_withSelfLinks;

  std::vector<double> alpha(N, 0);
  for (unsigned int i = 0; i < N; ++i) {
    auto t_i = t_out(i);
    alpha[i] = t_i / (s_out[i] + t_i); // = 1 for dangling nodes
    if (config.noSelfLinks) {
      // Inflate to adjust for no self-teleportation
      // TODO: Check possible side-effects
      alpha[i] /= 1 - nodeTeleportWeights[i];
    }
  }

  // Normalize link weights with respect to its source nodes total out-link weight;
  for (auto& link : flowLinks) {
    if (sumLinkOutWeight[link.source] > 0) {
      link.flow /= sumLinkOutWeight[link.source];
    }
  }

  std::vector<double> nodeFlowTmp(numNodes, 0.0);

  // Calculate PageRank
  const auto iteration = [&](const auto iter) {
    double teleTmp = 0.0;
    for (unsigned int i = 0; i < N; ++i) {
      teleTmp += alpha[i] * nodeFlow[i];
    }

    for (unsigned int i = 0; i < N; ++i) {
      nodeFlowTmp[i] = nodeTeleportWeights[i] * (config.noSelfLinks ? (teleTmp - alpha[i] * nodeFlow[i]) : teleTmp);
    }

    // Flow from links
    for (const auto& link : flowLinks) {
      double beta = 1 - alpha[link.source] * (config.noSelfLinks ? 1 - nodeTeleportWeights[link.source] : 1);
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
      Log() << "(Normalizing ranks after " << iter << " power iterations with error " << nodeFlowDiff << ") ";
      normalize(nodeFlow, nodeFlowDiff + 1.0);
    }

    return error;
  };

  unsigned int iterations = 0;
  double err = 0.0;

  do {
    double oldErr = err;
    err = iteration(iterations);

    // Perturb the system if equilibrium
    if (std::abs(err - oldErr) < 1e-15) {
    }

    ++iterations;
  } while (iterations < 200 && (err > 1.0e-15 || iterations < 50));

  Log() << "\n  -> PageRank calculation done in " << iterations << " iterations.\n";

  double sumNodeRank = 1.0;
  for (auto& link : flowLinks) {
    double beta = 1 - alpha[link.source] * (config.noSelfLinks ? 1 - nodeTeleportWeights[link.source] : 1);
    link.flow *= beta * nodeFlow[link.source] / sumNodeRank;
  }

  nodeTeleportFlow.assign(numNodes, 0.0);
  for (unsigned int i = 0; i < N; ++i) {
    nodeTeleportFlow[i] = nodeFlow[i] * alpha[i];
  }
}

void FlowCalculator::calcDirectedRegularizedMultilayerFlow(const StateNetwork& network, const Config& config) noexcept
{
  Log() << "\n  -> Using regularized multilayer flow. " << std::flush;

  // Calculate node weights w_i = s_i/k_i, where s_i is the node strength (weighted degree) and k_i the (unweighted) degree
  unsigned int N = network.numNodes();
  unsigned int N_phys = network.numPhysicalNodes();
  unsigned int L = network.numLayers();
  // unsigned int N_states = network.numNodes();
  // double nodeWeight = 1.0 / N;
  double intraOutWeight = config.regularizationStrength * std::log(N_phys) / L;
  // double interOutWeight = config.regularizationStrength * std::log(L);

  // Log() << "\n N: " << N_phys << ", N_states: " << N << ", L: " << L << "\n";
  // Log() << "ln(N)/(NL): " << std::log(N_phys) / (N_phys * L) << "\n";
  // Log() << "ln(N)/(L): " << std::log(N_phys) / (L) << "\n";

  std::vector<unsigned int> layerIds(N, 0);
  std::vector<unsigned int> physicalIds(N, 0);
  const auto& nodes = network.nodes();
  for (unsigned int i = 0; i < N; ++i) {
    layerIds[i] = nodes.at(i).layerId;
    physicalIds[i] = nodes.at(i).physicalId;
  }

  std::unordered_map<unsigned int, unsigned int> layerIdToIndex;
  unsigned int layerIndex = 0;
  for (unsigned int layerId : network.layers()) {
    layerIdToIndex[layerId] = layerIndex++;
    // Log() << "Layer " << layerId << " -> index " << layerIdToIndex[layerId] << "\n";
  }
  // Log() << "\n -> " << layerIdToIndex.size() << " layers...\n";

  std::vector<bool> isInterLink(flowLinks.size(), false);
  std::vector<unsigned int> layerIndices(N);

  for (const auto& node : network.nodes()) {
    // const auto nodeId = node.second.id;
    layerIndices[nodeIndexMap[node.second.id]] = layerIdToIndex[node.second.layerId];
    // nodeTeleportWeights[nodeIndexMap[nodeId]] = node.weight;
    // if (layerIdToIndex.count(node.second.layerId) == 0) {
    //   layerIdToIndex[node.second.layerId] = layerIndex++;
    // }
    // Log() << "Node (physId: " << node.second.physicalId << ", layerId: " << node.second.layerId << ") -> index: " << nodeIndexMap[node.second.id] << "\n";
  }

  unsigned int linkIndex = 0;

  // Log() << "\nLinks:\n";

  const auto& nodeLinkMap = network.nodeLinkMap();
  for (const auto& node : nodeLinkMap) {
    for (const auto& link : node.second) {
      isInterLink[linkIndex] = physicalIds[node.first.id] == physicalIds[link.first.id];
      // Log() << linkIndex << ": (" << layerIds[node.first.id] << "," << physicalIds[node.first.id] << ") -> (" << layerIds[link.first.id] << "," << physicalIds[link.first.id] << ") is inter: " << isInterLink[linkIndex] << "\n";
      ++linkIndex;
    }
  }

  std::vector<unsigned int> k_out(N, 0);
  std::vector<unsigned int> k_in(N, 0);
  std::vector<double> s_out(N, 0);
  std::vector<double> s_in(N, 0);
  std::vector<double> inter_out(N, 0);
  // double sum_s = sumWeightedDegree;
  // unsigned int sum_k = network.sumDegree();
  // double average_weight = sum_s / sum_k;

  linkIndex = 0;
  for (auto& link : flowLinks) {
    if (isInterLink[linkIndex++]) {
      inter_out[link.source] += link.flow;
    } else {
      k_out[link.source] += 1;
      s_out[link.source] += link.flow;
      k_in[link.target] += 1;
      s_in[link.target] += link.flow;
      // if (link.source == 0) {
      //   Log() << link.source << " -> " << link.target << " => k_out[0] -> " << k_out[link.source] << "\n";
      // }
    }
  }

  // auto gamma = [s_out, intraOutWeight, interOutWeight](auto i) { return 1 + interOutWeight / (s_out[i] + intraOutWeight); };

  // double min_u_out = std::numeric_limits<double>::max();
  // double min_u_in = std::numeric_limits<double>::max();
  // for (unsigned int i = 0; i < N; ++i) {
  //   if (k_out[i] > 0) {
  //     min_u_out = std::min(min_u_out, s_out[i] / k_out[i]);
  //   }
  //   if (k_in[i] > 0) {
  //     min_u_in = std::min(min_u_in, s_in[i] / k_in[i]);
  //   }
  // }

  // auto u_out = [s_out, k_out, min_u_out](auto i) { return k_out[i] == 0 ? min_u_out : s_out[i] / k_out[i]; };
  // auto u_in = [s_in, k_in, min_u_in](auto i) { return k_in[i] == 0 ? min_u_in : s_in[i] / k_in[i]; };

  // unsigned int numNodesAsTeleportationTargets = config.noSelfLinks ? N - 1 : N;
  // double lambda = config.regularizationStrength * std::log(N) / numNodesAsTeleportationTargets;
  // double u_t = average_weight;

  // double sum_u_in = 0.0;
  // for (unsigned int i = 0; i < N; ++i) {
  //   sum_u_in += u_in(i);
  // }

  // Log() << "\nNodes:\n";
  for (unsigned int i = 0; i < N; ++i) {
    // nodeTeleportWeights[i] = u_in(i) / sum_u_in;
    // nodeTeleportWeights[i] = 1 / N;
    nodeTeleportWeights[i] = 1.0 / N_phys;
    // nodeTeleportWeights[i] = 1.0 / (config.noSelfLinks ? N_phys - 1 : N_phys);
    // Log() << i << ") k_out: " << k_out[i] << ", s_out: " << s_out[i] << ", inter_out: " << inter_out[i] << "\n";
  }

  // std::function<double(unsigned int)> t_out_withoutSelfLinks = [lambda, u_t, u_out, u_in, sum_u_in](unsigned int i) { return lambda / u_t * u_out(i) * (sum_u_in - u_in(i)); };
  // std::function<double(unsigned int)> t_out_withSelfLinks = [lambda, u_t, u_out, sum_u_in](unsigned int i) { return lambda / u_t * u_out(i) * sum_u_in; };
  // auto t_out = config.noSelfLinks ? t_out_withoutSelfLinks : t_out_withSelfLinks;

  auto intraLayerTeleRate = [s_out, k_out, intraOutWeight](auto i) { return k_out[i] == 0 ? 1 : intraOutWeight / (intraOutWeight + s_out[i]); };

  std::vector<double> alpha(N, 0);
  std::vector<double> alphaInter(N, 0);
  // Log() << "\nTele probabilities:\n";
  for (unsigned int i = 0; i < N; ++i) {
    // auto t_i = t_out(i);
    alpha[i] = intraLayerTeleRate(i); // = 1 for dangling nodes
    alphaInter[i] = inter_out[i] / (inter_out[i] + s_out[i] + intraOutWeight);
    if (config.noSelfLinks) {
      // Inflate to adjust for no self-teleportation
      // TODO: Check possible side-effects
      alpha[i] /= 1 - nodeTeleportWeights[i];
      // alphaInter[i] /= 1 - nodeTeleportWeights[i];
    }
    // Log() << i << ": intra: " << alpha[i] << ", inter: " << alphaInter[i] << "\n";
    // Log() << i << ": intra: " << alpha[i] << ", inter: " << alphaInter[i] << " (inter_out: " << inter_out[i] << ", s_out: " << s_out[i] << ", intra_prior_out: " << intraOutWeight << ")\n";
  }

  // Log() << "\nLink probabilities:\n";
  // Normalize link weights to probabilities, separate for intra and inter links
  linkIndex = 0;
  for (auto& link : flowLinks) {
    // if (sumLinkOutWeight[link.source] > 0) {
    //   link.flow /= sumLinkOutWeight[link.source];
    // }
    if (isInterLink[linkIndex++]) {
      link.flow /= inter_out[link.source];
    } else {
      if (k_out[link.source] > 0) {
        link.flow /= s_out[link.source];
      }
    }
    // Log() << link.source << " -> " << link.target << ": " << link.flow << "\n";
  }

  std::vector<double> unrecordedInterFlow(N, 0);
  std::vector<double> nodeFlowTmp(numNodes, 0.0);
  std::vector<double> layerTeleFlow(L, 0.0);

  for (unsigned int i = 0; i < N; ++i) {
    nodeFlow[i] = 1.0 / N;
  }

  // Calculate two-step PageRank:
  const auto iteration = [&](const auto iter) {
    // Log() << "\nIter " << iter << ":\n";

    // 1. Unrecorded inter-layer step: push fraction of flow on inter-layer links to temporary location
    linkIndex = 0;
    unrecordedInterFlow.assign(N, 0.0);
    for (auto& link : flowLinks) {
      if (!isInterLink[linkIndex++]) {
        continue;
      }
      unrecordedInterFlow[link.target] += alphaInter[link.source] * nodeFlow[link.source] * link.flow;
      // unrecordedInterFlow[link.target] += alphaInter[link.source] * nodeFlow[link.source] * link.flow * (config.noSelfLinks ? 1 - nodeTeleportWeights[link.source] : 1);
      // Log() << "  " << link.source << " -> " << link.target << ": unrecorded[" << link.target << "] += " << nodeFlow[link.source] << " * " << alphaInter[link.source] << " * " << link.flow << "\n";
    }

    // double sumFlow = 0.0;
    // double sumUnrecordedFlow = 0.0;
    // for (unsigned int i = 0; i < N; ++i) {
    //   sumFlow += nodeFlow[i];
    //   sumUnrecordedFlow += unrecordedInterFlow[i];
    // }
    // Log() << "  Sum flow: " << sumFlow << "\n";
    // Log() << "  Sum unrecorded flow: " << sumUnrecordedFlow << "\n";

    // 2. Recorded intra-layer step: push rest of flow plus temporarily stored flow to intra-layer with intra-layer teleportation
    layerTeleFlow.assign(L, 0.0);
    for (unsigned int i = 0; i < N; ++i) {
      layerTeleFlow[layerIndices[i]] += alpha[i] * ((1 - alphaInter[i]) * nodeFlow[i] + unrecordedInterFlow[i]);
      // Log() << "Node " << i << " in layer " << layerIndices[i] << ": alpha: " << alpha[i] << ", non-inter flow: " << (1 - alphaInter[i]) * nodeFlow[i] << ", unrecorded flow: " << unrecordedInterFlow[i] << ", += " << alpha[i] * ((1 - alphaInter[i]) * nodeFlow[i] + unrecordedInterFlow[i]) << " -> " << layerTeleFlow[layerIndices[i]] << "\n";
    }

    // for (unsigned int i = 0; i < layerTeleFlow.size(); ++i) {
    //   Log() << "Layer " << i << " tele flow: " << layerTeleFlow[i] << "\n";
    // }

    for (unsigned int i = 0; i < N; ++i) {
      nodeFlowTmp[i] = nodeTeleportWeights[i] * (layerTeleFlow[layerIndices[i]] - (config.noSelfLinks ? (alpha[i] * nodeFlow[i]) : 0));
      // nodeFlowTmp[i] = nodeTeleportWeights[i] * layerTeleFlow[layerIndices[i]];
      // Log() << i << ": tele flow: " << nodeFlowTmp[i] << "\n";
    }

    // Flow from links
    linkIndex = 0;
    for (const auto& link : flowLinks) {
      if (isInterLink[linkIndex++]) {
        continue;
      }
      double beta = 1 - alpha[link.source] * (config.noSelfLinks ? 1 - nodeTeleportWeights[link.source] : 1);
      // double beta = 1 - alpha[link.source];
      nodeFlowTmp[link.target] += beta * link.flow * ((1 - alphaInter[link.source]) * nodeFlow[link.source] + unrecordedInterFlow[link.source]);
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
      Log() << "(Normalizing ranks after " << iter << " power iterations with error " << nodeFlowDiff << ") ";
      if (std::abs(nodeFlowDiff) > 1.0e-4) {
        throw std::runtime_error(io::Str() << "Total flow differs from 1 by " << nodeFlowDiff << " after " << iter << " iterations. Please report the issue.\n");
      }
      normalize(nodeFlow, nodeFlowDiff + 1.0);
    }

    return error;
  };

  unsigned int iterations = 0;
  unsigned int maxIterations = 200;
  double err = 0.0;

  do {
    double oldErr = err;
    err = iteration(iterations);

    // Perturb the system if equilibrium
    if (std::abs(err - oldErr) < 1e-15) {
    }

    ++iterations;
  } while (iterations < maxIterations && (err > 1.0e-15 || iterations < 50));

  if (iterations == maxIterations && err > 1e-14) {
    Log() << "  -> Warning: PageRank calculation stopped after maximum " << iterations << " with diff " << err << ".\n";
  } else {
    Log() << "\n  -> PageRank calculation done in " << iterations << " iterations.\n";
  }

  // Log() << std::setprecision(16);
  // Log() << "Flow: ";
  double sumTeleFlow = 0.0;
  for (unsigned int i = 0; i < layerTeleFlow.size(); ++i) {
    sumTeleFlow += layerTeleFlow[i];
  }
  Log() << "Sum tele flow: " << sumTeleFlow << "\n";

  // double sumNodeRank = 1.0;
  // for (auto& link : flowLinks) {
  //   double beta = 1 - alpha[link.source] * (config.noSelfLinks ? 1 - nodeTeleportWeights[link.source] : 1);
  //   link.flow *= beta * nodeFlow[link.source] / sumNodeRank;
  // }
  linkIndex = 0;
  double sumLinkFlow = 0.0;
  double sumInterLinkFlow = 0.0;
  enterFlow.assign(numNodes, 0.0);
  exitFlow.assign(numNodes, 0.0);
  // unrecorded inter-layer flow, skip adding enter/exit to intermediate node for unrecorded flow?
  for (auto& link : flowLinks) {
    if (isInterLink[linkIndex++]) {
      // Log() << " | Inter-link " << linkIndex << ": (" << layerIds[link.source] << "," << physicalIds[link.source] << ") -> (" << layerIds[link.target] << "," << physicalIds[link.target] << ") p_i_inter: " << alphaInter[link.source] << ", flow: " << nodeFlow[link.source] << ", p_ij_inter: " << link.flow << " -> flow " << alphaInter[link.source] * nodeFlow[link.source] * link.flow << "\n";
      link.flow = alphaInter[link.source] * nodeFlow[link.source] * link.flow;
      sumInterLinkFlow += link.flow;
      // Need to add enter/exit flow to eventually collapse
      exitFlow[link.source] += link.flow;
      enterFlow[link.target] += link.flow;
      // Remove unrecorded enter and exit flow on intermediate nodes removes collapse
      // exitFlow[link.target] -= link.flow;
      // enterFlow[link.target] -= link.flow;
      // }
    } else {
      double beta = 1 - alpha[link.source];
      link.flow = beta * link.flow * ((1 - alphaInter[link.source]) * nodeFlow[link.source] + unrecordedInterFlow[link.source]);
      // Log() << " - Intra-link " << linkIndex << ": (" << layerIds[link.source] << "," << physicalIds[link.source] << ") -> (" << layerIds[link.target] << "," << physicalIds[link.target] << "), flow: " << link.flow << "\n";
      exitFlow[link.source] += link.flow;
      enterFlow[link.target] += link.flow;
    }
    sumLinkFlow += link.flow;
    // Log() << linkIndex << ": (" << layerIds[link.source] << "," << physicalIds[link.source] << ") -> (" << layerIds[link.target] << "," << physicalIds[link.target] << ") is inter: " << isInterLink[linkIndex] << ", flow: " << link.flow << "\n";
  }
  Log() << "=> Sum link flow: " << sumLinkFlow << ", sum inter link flow: " << sumInterLinkFlow << ", sum intra link flow: " << sumLinkFlow - sumInterLinkFlow << "\n";
  Log() << "Sum intra link flow plus tele flow: " << sumTeleFlow + sumLinkFlow - sumInterLinkFlow << "\n";

  nodeTeleportFlow.assign(numNodes, 0.0);
  for (unsigned int i = 0; i < N; ++i) {
    // nodeTeleportFlow[i] = nodeFlow[i] * alpha[i];
    nodeTeleportFlow[i] = alpha[i] * ((1 - alphaInter[i]) * nodeFlow[i] + unrecordedInterFlow[i]);
    // Log() << i << ": " << nodeFlow[i] << ", tele-flow: " << nodeTeleportFlow[i] << "\n";
    // Log() << i << " (" << layerIds[i] << "," << physicalIds[i] << "): " << nodeFlow[i] << ", tele-flow: " << nodeTeleportFlow[i] << ", alpha: " << alpha[i] << ", alphaInter: " << alphaInter[i] << ", unrecordedInter: " << unrecordedInterFlow[i] << ", tele-weight: " << nodeTeleportWeights[i] << "\n";

    exitFlow[i] += nodeTeleportFlow[i] * (1 - nodeTeleportWeights[i]); // + node.intraLayerTeleFlow * (1 - node.intraLayerTeleWeight);
    enterFlow[i] += (layerTeleFlow[layerIndices[i]] - nodeTeleportFlow[i]) * nodeTeleportWeights[i];
    // double unrecordedTeleFlow = alpha[i] * (1 - alphaInter[i]) * nodeFlow[i];
    // exitFlow[i] += unrecordedTeleFlow * (1 - nodeTeleportWeights[i]); // + node.intraLayerTeleFlow * (1 - node.intraLayerTeleWeight);
    // enterFlow[i] += (layerTeleFlow[layerIndices[i]] - unrecordedTeleFlow) * nodeTeleportWeights[i];
    // Log() << i << " (" << layerIds[i] << "," << physicalIds[i] << "): enter: " << enterFlow[i] << ", exit: " << exitFlow[i] << "\n";
  }
}

void FlowCalculator::calcUndirectedRegularizedFlow(const StateNetwork& network, const Config& config) noexcept
{
  Log() << "\n  -> Using recorded teleportation to nodes according to a fully connected Bayesian prior. " << std::flush;

  // Calculate node weights w_i = s_i/k_i, where s_i is the node strength (weighted degree) and k_i the (unweighted) degree
  unsigned int N = network.numNodes();
  std::vector<unsigned int> k(N, 0);
  std::vector<double> s(N, 0);
  double sum_s = sumWeightedDegree;
  unsigned int sum_k = network.sumDegree();
  double average_weight = sum_s / sum_k;

  for (auto& link : flowLinks) {
    k[link.source] += 1;
    s[link.source] += link.flow;
    if (link.source != link.target) {
      k[link.target] += 1;
      s[link.target] += link.flow;
    }
  }

  double min_u = std::numeric_limits<double>::max();
  for (unsigned int i = 0; i < N; ++i) {
    if (k[i] > 0) {
      min_u = std::min(min_u, s[i] / k[i]);
    }
  }

  auto u = [s, k, min_u](auto i) { return k[i] == 0 ? min_u : s[i] / k[i]; };

  unsigned int numNodesAsTeleportationTargets = config.noSelfLinks ? N - 1 : N;
  double lambda = config.regularizationStrength * std::log(N) / numNodesAsTeleportationTargets;
  double u_t = average_weight;

  double sum_u = 0.0;
  for (unsigned int i = 0; i < N; ++i) {
    sum_u += u(i);
  }

  // nodeTeleportWeights is the fraction of teleportation flow landing on each node. This is proportional to u_in
  for (unsigned int i = 0; i < N; ++i) {
    nodeTeleportWeights[i] = u(i) / sum_u;
  }

  std::function<double(unsigned int)> t_withoutSelfLinks = [lambda, u_t, u, sum_u](unsigned int i) { return lambda / u_t * u(i) * (sum_u - u(i)); };
  std::function<double(unsigned int)> t_withSelfLinks = [lambda, u_t, u, sum_u](unsigned int i) { return lambda / u_t * u(i) * sum_u; };
  auto t = config.noSelfLinks ? t_withoutSelfLinks : t_withSelfLinks;

  std::vector<double> alpha(N, 0);
  double sum_t = 0.0;
  for (unsigned int i = 0; i < N; ++i) {
    auto t_i = t(i);
    alpha[i] = t_i / (s[i] + t_i);
    if (config.noSelfLinks) {
      // Inflate to adjust for no self-teleportation
      // TODO: No later side effects of cheating here? Need to normalize targets instead?
      alpha[i] /= 1 - nodeTeleportWeights[i];
    }
    sum_t += t_i;
  }

  for (auto& link : flowLinks) {
    if (sumLinkOutWeight[link.source] > 0) {
      link.flow /= sumLinkOutWeight[link.source];
    }
  }

  for (unsigned int i = 0; i < N; ++i) {
    nodeFlow[i] = (s[i] + t(i)) / (sum_s + sum_t);
  }

  nodeTeleportFlow.assign(numNodes, 0.0);
  for (unsigned int i = 0; i < N; ++i) {
    nodeTeleportFlow[i] = nodeFlow[i] * alpha[i];
  }

  for (auto& link : flowLinks) {
    // TODO: Side effect from inflating alpha, need real alpha here.
    double beta = 1 - alpha[link.source] * (config.noSelfLinks ? 1 - nodeTeleportWeights[link.source] : 1);
    link.flow *= beta * nodeFlow[link.source] * 2;
  }
}

void FlowCalculator::calcUndirectedRegularizedMultilayerFlow(const StateNetwork& network, const Config& config)
{
  if (config.multilayerTest == 0) {
    calcUndirectedRegularizedFlow(network, config);
    return;
  }
  Log() << "\n  -> Using undirected regularized multilayer flow. " << std::flush;
  throw std::runtime_error("Undirected multilayer regularization not implemented");

  // double lambda = config.regularizationStrength * std::log(N) / numNodesAsTeleportationTargets;

  unsigned int N = network.numPhysicalNodes();
  unsigned int N_states = network.numNodes();
  // unsigned int L = network.numLayers();
  // double n = N / L; // average number of nodes per layer

  // Default tele degree is log(N_states)
  // We want log(N), so adjust with regularization strength

  // double defaultLambda = std::log(N_states) / N_states;

  // double lambdaIntra = std::log(N) / (L * N);
  // double lambdaInter = std::log(L) / (L * N);
  // double lambda = lambdaIntra + lambdaInter;
  // double fractionIntraFlow = std::log(N) / (std::log(N) + L * std::log(L));

  // double newLambda = lambdaIntra + lambdaInter;

  Config tmpConfig(config);
  // tmpConfig.regularizationStrength *= newLambda / defaultLambda;
  // Log() << "\n!! old lambda: " << defaultLambda << ", new lambda: " << newLambda << "\n";
  double lambdaFactor = std::log(N) / std::log(N_states);
  tmpConfig.regularizationStrength *= lambdaFactor;
  Log() << "\n  -> Using regularized multilayer flow (lambda factor " << lambdaFactor << "). " << std::flush;
  // Log() << "\n!! lambda factor: " << lambdaFactor << "\n";
  // Log() << "Fraction intra flow: " << fractionIntraFlow << "\n";

  // for (auto& link : flowLinks) {
  //   k[link.source] += 1;
  //   s[link.source] += link.flow;
  //   if (link.source != link.target) {
  //     k[link.target] += 1;
  //     s[link.target] += link.flow;
  //   }
  // }

  calcUndirectedRegularizedFlow(network, tmpConfig);
}

void FlowCalculator::calcDirectedBipartiteFlow(const StateNetwork& network, const Config& config) noexcept
{
  Log() << "\n  -> Using bipartite " << (config.recordedTeleportation ? "recorded" : "unrecorded") << " teleportation to " << (config.teleportToNodes ? "nodes" : "links") << ". " << std::flush;

  const auto bipartiteStartId = network.bipartiteStartId();

  if (config.teleportToNodes) {
    for (const auto& nodeIt : network.nodes()) {
      auto& node = nodeIt.second;
      if (node.id < bipartiteStartId) {
        nodeTeleportWeights[nodeIndexMap[node.id]] = node.weight;
      }
    }
  } else {
    // Teleport proportionally to out-degree, or in-degree if recorded teleportation.
    // Two-step degree: sum of products between incoming and outgoing links from bipartite nodes

    if (config.recordedTeleportation) {
      for (auto link = begin(flowLinks) + bipartiteLinkStartIndex; link != end(flowLinks); ++link) {
        // target is an ordinary node
        nodeTeleportWeights[link->target] += link->flow;
      }
    } else {
      // Unrecorded teleportation

      for (auto link = begin(flowLinks); link != begin(flowLinks) + bipartiteLinkStartIndex; ++link) {
        // source is an ordinary node
        nodeTeleportWeights[link->source] += link->flow;
      }
    }
  }

  normalize(nodeTeleportWeights);

  nodeFlow = nodeTeleportWeights;

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
  const auto iteration = [&](const auto iter, const double alpha, const double beta) {
    danglingRank = 0.0;
    for (const auto& i : danglingIndices) {
      danglingRank += nodeFlow[i];
    }

    // Flow from teleportation
    const auto teleportationFlow = alpha + beta * danglingRank;
    for (unsigned int i = 0; i < bipartiteStartIndex; ++i) {
      nodeFlowTmp[i] = teleportationFlow * nodeTeleportWeights[i];
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
      Log() << "(Normalizing ranks after " << iter << " power iterations with error " << nodeFlowDiff << ") ";
      normalize(nodeFlow, nodeFlowDiff + 1.0);
    }

    return error;
  };

  const auto result = powerIterate(config.teleportationProbability, iteration);

  double sumNodeRank = 1.0;
  double beta = result.beta;

  if (!config.recordedTeleportation) {
    // Take one last power iteration excluding the teleportation (and normalize node flow to sum 1.0)
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
  unsigned int N = network.numNodes();
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
  double sumNodeFlow = 0.0;
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

  // unsigned int N_phys = network.numPhysicalNodes();
  // unsigned int L = network.numLayers();
  // double fractionIntraFlow = std::log(N_phys) / (std::log(N_phys) + L * std::log(L));
  // double fractionIntraFlow = 1 / L;
  double fractionIntraFlow = 1;
  // if (config.multilayerTest < 2) {
  //   fractionIntraFlow = 0;
  // }

  double sumTeleFlow = 0.0;
  // double sumIntraTeleFlow = 0.0;

  for (auto& nodeIt : network.m_nodes) {
    auto& node = nodeIt.second;
    const auto nodeIndex = nodeIndexMap[node.id];
    node.flow = nodeFlow[nodeIndex];
    node.weight = nodeTeleportWeights[nodeIndex];
    node.teleFlow = !nodeTeleportFlow.empty() ? nodeTeleportFlow[nodeIndex] : nodeFlow[nodeIndex] * (nodeOutDegree[nodeIndex] == 0 ? 1 : config.teleportationProbability);
    node.intraLayerTeleFlow = fractionIntraFlow * node.teleFlow;
    node.teleFlow *= 1 - fractionIntraFlow;
    node.intraLayerTeleWeight = nodeTeleportWeights[nodeIndex];
    node.enterFlow = node.flow;
    node.exitFlow = node.flow;
    // Log() << nodeIndex << ": teleFlow: " << node.teleFlow << ", intraTele: " << node.intraLayerTeleFlow << ", intraTeleWeight: " << node.intraLayerTeleWeight << "\n";
    sumTeleFlow += node.teleFlow;
    // sumIntraTeleFlow += node.intraLayerTeleFlow;

    if (!config.noSelfLinks) {
      // Remove self-teleportation flow
      node.enterFlow -= node.teleFlow * node.weight;
      node.exitFlow -= node.teleFlow * node.weight;

      // Remove self-link flow
      unsigned int norm = config.isUndirectedFlow() ? 2 : 1;
      auto& outLinks = network.m_nodeLinkMap[node.id];
      for (auto& link : outLinks) {
        auto& linkData = link.second;
        if (node.id == link.first.id) {
          node.enterFlow -= linkData.flow / norm;
          node.exitFlow -= linkData.flow / norm;
          break;
        }
      }
    }

    sumNodeFlow += node.flow;
  }

  // Enter/exit flow
  if (enterFlow.size() == 0) {
    enterFlow.assign(numNodes, 0.0);
    exitFlow.assign(numNodes, 0.0);

    for (auto& link : flowLinks) {
      if (link.source != link.target) {
        exitFlow[link.source] += link.flow;
        enterFlow[link.target] += link.flow;
      }
    }
    // Add teleportation flow
    for (auto& nodeIt : network.m_nodes) {
      auto& node = nodeIt.second;
      const auto nodeIndex = nodeIndexMap[node.id];
      exitFlow[nodeIndex] += node.teleFlow * (1 - node.weight);
      enterFlow[nodeIndex] += (sumTeleFlow - node.teleFlow) * node.weight;
    }
  }

  if (!config.isUndirectedClustering() && !config.regularized) {
    enterFlow.assign(N, 0);
    exitFlow.assign(N, 0);
    double alpha = config.teleportationProbability;
    double sumDanglingFlow = 0.0;
    for (unsigned int i = 0; i < N; ++i) {
      if (nodeOutDegree[i] == 0) {
        sumDanglingFlow += nodeFlow[i];
      }
    }
    for (auto& nodeIt : network.m_nodes) {
      auto& node = nodeIt.second;
      const auto sourceIndex = nodeIndexMap[node.id];
      auto& outLinks = network.m_nodeLinkMap[node.id];
      double danglingFlow = outLinks.empty() ? node.flow : 0.0;
      if (config.recordedTeleportation) {
        // Don't let self-teleportation add to the enter/exit flow (i.e. multiply with (1.0 - node.data.teleportWeight))
        exitFlow[sourceIndex] += alpha * node.flow * (1.0 - node.weight);
        enterFlow[sourceIndex] += (alpha * (1.0 - node.flow) + (1 - alpha) * (sumDanglingFlow - danglingFlow)) * node.weight;
      }
      for (auto& link : outLinks) {
        auto& linkData = link.second;
        const auto targetIndex = nodeIndexMap[link.first.id];
        exitFlow[sourceIndex] += linkData.flow;
        enterFlow[targetIndex] += linkData.flow;
      }
    }
  }
  // Save enter/exit flow on nodes
  for (auto& nodeIt : network.m_nodes) {
    auto& node = nodeIt.second;
    const auto nodeIndex = nodeIndexMap[node.id];
    node.enterFlow = enterFlow[nodeIndex];
    node.exitFlow = exitFlow[nodeIndex];
  }

  Log() << "\n  => Sum node flow: " << sumNodeFlow << ", sum link flow: " << sumLinkFlow << "\n";
}

} // namespace infomap
