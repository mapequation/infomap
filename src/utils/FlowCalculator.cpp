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
#include <functional>
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
  nodeTeleportWeights.assign(numNodes, 0.0); // Fraction of teleportation flow landing on node i
  // nodeTeleportFlow.assign(numNodes, 0.0); // Outgoing teleportation flow from node i

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

  numLinks = network.numLinks();
  flowLinks.resize(numLinks, { 0, 0, 0.0 });
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

  for (const auto& node : nodeLinkMap) {
    const auto sourceId = node.first.id;
    const auto sourceIndex = nodeIndexMap[sourceId];

    for (const auto& link : node.second) {
      const auto targetId = link.first.id;
      const auto targetIndex = nodeIndexMap[targetId];
      const auto linkWeight = link.second.weight;

      ++nodeOutDegree[sourceIndex];
      sumLinkOutWeight[sourceIndex] += linkWeight;
      nodeFlow[sourceIndex] += linkWeight / sumWeightedDegree;

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
          nodeFlow[targetIndex] += linkWeight / sumWeightedDegree;
        }
      }
    }
  }

  bool normalizeNodeFlow = false;

  switch (config.flowModel) {
  case FlowModel::undirected:
    if (config.regularized) {
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
  const auto iteration = [&](const auto iteration, const double alpha, const double beta) {
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

  auto s = [s_in, s_out](auto i) { return s_in[i] + s_out[i]; };
  auto k = [k_in, k_out](auto i) { return k_in[i] + k_out[i]; };
  // TODO: Use minimum weight instead of 1 as default if no link
  auto u_out = [s_out, k_out](auto i) { return k_out[i] == 0 ? 1 : s_out[i] / k_out[i]; };
  auto u_in = [s_in, k_in](auto i) { return k_in[i] == 0 ? 1 : s_in[i] / k_in[i]; };
  auto u_ij = [u_in, u_out](auto i, auto j) { return u_out(i) * u_in(j); };

  unsigned int numNodesAsTeleportationTargets = config.noSelfLinks ? N - 1 : N;
  double lambda = config.regularizationStrength * std::log(N) / numNodesAsTeleportationTargets;
  double u_t = average_weight;
  // for (unsigned int i = 0; i < N; ++i) {
  //   u_t += k(i) == 0 ? 0 : s(i) / k(i);
  // }
  double sum_u_in = 0.0;
  double sum_s_in = 0.0;
  for (unsigned int i = 0; i < N; ++i) {
    sum_u_in += u_in(i);
    sum_s_in += s_in[i];
  }

  // nodeTeleportWeights is the fraction of teleportation flow landing on each node. This is proportional to u_in
  // Log() << "\n\n Node tele weights:\n";
  for (unsigned int i = 0; i < N; ++i) {
    nodeTeleportWeights[i] = u_in(i) / sum_u_in;
    // nodeTeleportWeights[i] = s_in[i] / sum_s_in;
    // Log() << "  " << i << ": " << nodeTeleportWeights[i] << " (u_in: " << u_in(i) <<
    // ", k_in: " << k_in[i] << ", s_in: " << s_in[i] << ")\n";
  }

  auto t_ij = [lambda, u_t, u_ij](auto i, auto j) { return lambda / u_t * u_ij(i, j); };
  // auto t_out_withoutSelfLinks = [lambda, u_t, u_out, u_in, sum_u_in](unsigned int i) { return lambda/u_t * u_out(i) * (sum_u_in - u_in(i)); };
  // auto t_out_withSelfLinks = [lambda, u_t, u_out, sum_u_in](unsigned int i) { return lambda/u_t * u_out(i) * sum_u_in; };
  std::function<double(unsigned int)> t_out_withoutSelfLinks = [lambda, u_t, u_out, u_in, sum_u_in](unsigned int i) { return lambda / u_t * u_out(i) * (sum_u_in - u_in(i)); };
  std::function<double(unsigned int)> t_out_withSelfLinks = [lambda, u_t, u_out, sum_u_in](unsigned int i) { return lambda / u_t * u_out(i) * sum_u_in; };
  auto t_out = config.noSelfLinks ? t_out_withoutSelfLinks : t_out_withSelfLinks;

  std::vector<double> alpha(N, 0);
  // Log() << "\nt_i (lambda: " << lambda << ", u_t: " << u_t << ", sum_u_in: " << sum_u_in << "): ";
  double sumAlpha = 0.0;
  for (unsigned int i = 0; i < N; ++i) {
    auto t_i = t_out(i);
    alpha[i] = t_i / (s_out[i] + t_i); // = 1 for dangling nodes
    if (config.noSelfLinks) {
      // Inflate to adjust for no self-teleportation
      // TODO: Check possible side-effects
      alpha[i] /= 1 - nodeTeleportWeights[i];
    }
    sumAlpha += alpha[i];
  }
  // Log() << "\n";

  // for (unsigned int i = 0; i < N; ++i) {
  //   Log() << i << ": k_out: " << k_out[i] << ", k_in: " << k_in[i] << ", " <<
  //   "s_out: " << s_out[i] << ", s_in: " << s_in[i] << ", t_out: " << t_out(i) << ", alpha: " << alpha[i] << "\n";
  // }

  // Log() << "\n";

  // Normalize link weights with respect to its source nodes total out-link weight;
  for (auto& link : flowLinks) {
    if (sumLinkOutWeight[link.source] > 0) {
      link.flow /= sumLinkOutWeight[link.source];
    }
  }

  std::vector<double> nodeFlowTmp(numNodes, 0.0);

  // Calculate PageRank
  const auto iteration = [&](const auto iteration) {
    // Flow from teleportation, remove self-teleportation
    // double teleportationFlow = 0.0;
    double teleTmp = 0.0;
    for (unsigned int i = 0; i < N; ++i) {
      // teleportationFlow += alpha[i] * nodeFlow[i];
      // teleTmp += alpha[i] * nodeFlow[i] / (sum_u_in - u_in(i));
      teleTmp += alpha[i] * nodeFlow[i];
    }
    // double tmp1 = 0;
    for (unsigned int i = 0; i < N; ++i) {
      // nodeFlowTmp[i] = u_in(i) / sum_u_in * (teleportationFlow - alpha[i] * nodeFlow[i]);
      // nodeFlowTmp[i] = u_in(i) * (teleTmp - alpha[i] * nodeFlow[i] / (sum_u_in - u_in(i)));
      // nodeFlowTmp[i] = nodeTeleportWeights[i] * (teleTmp - alpha[i] * nodeFlow[i]);
      nodeFlowTmp[i] = nodeTeleportWeights[i] * (config.noSelfLinks ? (teleTmp - alpha[i] * nodeFlow[i]) : teleTmp);
      // nodeFlowTmp[i] = nodeTeleportWeights[i] * teleTmp;
      // tmp1 += nodeFlowTmp[i];
      // if (i == 0 && iteration < 2) {
      //   Log() << "\nNode 0: u_in: " << u_in(i) << ", teleTmp: " << teleTmp << ", alpha: " << alpha[i] <<
      //   ", nodeFlow: " << nodeFlow[i] << ", sum_u_in: " << sum_u_in << "\n";
      // }
    }
    // if (iteration < 2 || iteration == 49) {
    //   Log() << "Iter " << iteration << ": Sum tele tmp: " << tmp1 << ", teleTmp: " << teleTmp << ", sumAlpha: " << sumAlpha << "\n";
    // }
    // double tmp2 = 0.0;
    // for (unsigned int i = 0; i < N; ++i) {
    //   nodeFlowTmp[i] /= tmp1/teleTmp;
    //   tmp2 += nodeFlowTmp[i];
    // }
    // if (iteration < 2) {
    //   Log() << "=> Sum 2: " << tmp2 << "\n";
    // }


    // Flow from links
    for (const auto& link : flowLinks) {
      double beta = 1 - alpha[link.source] * (config.noSelfLinks ? 1 - nodeTeleportWeights[link.source] : 1);
      nodeFlowTmp[link.target] += beta * link.flow * nodeFlow[link.source];
      // nodeFlowTmp[link.target] += (1 - alpha[link.source]) * link.flow * nodeFlow[link.source];
    }

    // Update node flow from the power iteration above and check if converged
    double nodeFlowDiff = -1.0; // Start with -1.0 so we don't have to subtract it later
    double error = 0.0;
    for (unsigned int i = 0; i < numNodes; ++i) {
      nodeFlowDiff += nodeFlowTmp[i];
      error += std::abs(nodeFlowTmp[i] - nodeFlow[i]);
    }
    // Log() << iteration << ": teleportFlow: " << teleportationFlow << ", tele nodeFlow: " << tmp1 <<
    // ", nodeFlowDiff: " << nodeFlowDiff << ", error: " << error << "\n";

    nodeFlow = nodeFlowTmp;

    // Normalize if needed
    if (std::abs(nodeFlowDiff) > 1.0e-10) {
      Log() << "(Normalizing ranks after " << iteration << " power iterations with error " << nodeFlowDiff << ") ";
      normalize(nodeFlow, nodeFlowDiff + 1.0);
    }

    return error;
  };

  // Log() << "\nDo power iteration!\n";
  // const auto result = powerIterate2(iteration);
  unsigned int iterations = 0;
  double err = 0.0;

  do {
    double oldErr = err;
    err = iteration(iterations);

    // Perturb the system if equilibrium
    if (std::abs(err - oldErr) < 1e-15) {
    }

    ++iterations;
    // if (iterations == 10) {
    //   break;
    // }
  } while (iterations < 200 && (err > 1.0e-15 || iterations < 50));

  Log() << "\n  -> PageRank calculation done in " << iterations << " iterations." << std::endl;

  double sumNodeRank = 1.0;
  // double tmpN = 0.0;
  // for (auto& flow : nodeFlow) {
  //   tmpN += flow;
  // }
  // double tmpE = 0;
  // Update the links with their global flow from the PageRank values.
  for (auto& link : flowLinks) {
    double beta = 1 - alpha[link.source] * (config.noSelfLinks ? 1 - nodeTeleportWeights[link.source] : 1);
    link.flow *= beta * nodeFlow[link.source] / sumNodeRank;
    // tmpE += link.flow;
  }

  // enterFlow = nodeFlow;
  // exitFlow = nodeFlow;
  nodeTeleportFlow.assign(numNodes, 0.0);
  for (unsigned int i = 0; i < N; ++i) {
    nodeTeleportFlow[i] = nodeFlow[i] * alpha[i];
  }


  // Log() << "\nSum node flow: " << tmpN << ", sum link flow: " << tmpE << "\n\n";
  // Log() << "Node flow (sum: " << tmpN << "):\n";
  // for (unsigned int i = 0; i < N; ++i)
  //   Log() << i << ": flow:  " << nodeFlow[i] << ", teleWeight: " << nodeTeleportWeights[i] << ", alpha: " << alpha[i] << ", adj alpha: " << alpha[i] * (1 - nodeTeleportWeights[i]) << ", t_out: " << t_out(i) << ", s_out: " << s_out[i] << ", k_out: " << k_out[i] << "\n";
  // Log() << "Link flow (sum: " << tmpE << "):\n";
  // // for (auto& link : flowLinks)
  // //   Log() << "  " << link.flow << ", ";
  // Log() << "\n";
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

  auto u = [s, k](auto i) { return k[i] == 0 ? 1 : s[i] / k[i]; };
  auto u_ij = [u](auto i, auto j) { return u(i) * u(j); };

  unsigned int numNodesAsTeleportationTargets = config.noSelfLinks ? N - 1 : N;
  double lambda = config.regularizationStrength * std::log(N) / numNodesAsTeleportationTargets;
  double u_t = average_weight;

  // Log() << "\nLambda: " << lambda << ", avg weight: " << u_t << ", norm: " << lambda * u_t << "\n";

  // for (unsigned int i = 0; i < N; ++i) {
  //   u_t += k(i) == 0 ? 0 : s(i) / k(i);
  // }
  double sum_u = 0.0;
  for (unsigned int i = 0; i < N; ++i) {
    sum_u += u(i);
  }

  // nodeTeleportWeights is the fraction of teleportation flow landing on each node. This is proportional to u_in
  // Log() << "\n\n Node tele weights:\n";
  for (unsigned int i = 0; i < N; ++i) {
    nodeTeleportWeights[i] = u(i) / sum_u;
    // Log() << "  " << i << ": " << nodeTeleportWeights[i] << " (u: " << u(i) << ", k: " << k[i] << ", s: " << s[i] << ")\n";
  }

  auto t_ij = [lambda, u_t, u_ij](auto i, auto j) { return lambda / u_t * u_ij(i, j); };
  // auto t_out_withoutSelfLinks = [lambda, u_t, u_out, u_in, sum_u](unsigned int i) { return lambda/u_t * u_out(i) * (sum_u - u_in(i)); };
  // auto t_out_withSelfLinks = [lambda, u_t, u_out, sum_u](unsigned int i) { return lambda/u_t * u_out(i) * sum_u; };
  std::function<double(unsigned int)> t_withoutSelfLinks = [lambda, u_t, u, sum_u](unsigned int i) { return lambda / u_t * u(i) * (sum_u - u(i)); };
  std::function<double(unsigned int)> t_withSelfLinks = [lambda, u_t, u, sum_u](unsigned int i) { return lambda / u_t * u(i) * sum_u; };
  auto t = config.noSelfLinks ? t_withoutSelfLinks : t_withSelfLinks;

  std::vector<double> alpha(N, 0);
  // Log() << "\nt_i (lambda: " << lambda << ", u_t: " << u_t << ", sum_u: " << sum_u << "): ";
  double sumAlpha = 0.0;
  double sum_t = 0.0;
  double sum_ts = 0.0;
  double sum_t_to_ts = 0.0;
  for (unsigned int i = 0; i < N; ++i) {
    auto t_i = t(i);
    alpha[i] = t_i / (s[i] + t_i);
    sum_t_to_ts += alpha[i];
    if (config.noSelfLinks) {
      // Inflate to adjust for no self-teleportation
      // TODO: No later side effects of cheating here? Need to normalize targets instead?
      alpha[i] /= 1 - nodeTeleportWeights[i];
    }
    sumAlpha += alpha[i];
    sum_t += t_i;
    sum_ts += t_i + s[i];
  }
  // Log() << "\n";

  // Log() << "Sum alpha: " << sumAlpha << ", sum t/(t+s): " << sum_t_to_ts << ", sum t: " << sum_t << ", sum t+s: " << sum_ts << ":\n";
  // for (unsigned int i = 0; i < N; ++i) {
  //   Log() << i << ": t: " << t(i) << ", alpha: " << alpha[i] << ", t/(s+t): " << t(i) / (s[i] + t(i)) << "\n";
  // }

  for (auto& link : flowLinks) {
    if (sumLinkOutWeight[link.source] > 0) {
      link.flow /= sumLinkOutWeight[link.source];
    }
  }

  for (unsigned int i = 0; i < N; ++i) {
    nodeFlow[i] = (s[i] + t(i)) / (sum_s + sum_t);
  }

  // enterFlow = nodeFlow;
  // exitFlow = nodeFlow;
  nodeTeleportFlow.assign(numNodes, 0.0);
  for (unsigned int i = 0; i < N; ++i) {
    nodeTeleportFlow[i] = nodeFlow[i] * alpha[i];
  }

  // double sumNodeRank = 1.0;
  // double tmpN = 0.0;
  // for (auto& flow : nodeFlow) {
  //   tmpN += flow;
  // }
  // double tmpE = 0;
  // Update the links with their global flow from the undirected PageRank values.
  // Log() << "\nLink flow:\n";
  for (auto& link : flowLinks) {
    // TODO: Side effect from inflating alpha, need real alpha here.
    double beta = 1 - alpha[link.source] * (config.noSelfLinks ? 1 - nodeTeleportWeights[link.source] : 1);
    link.flow *= beta * nodeFlow[link.source] * 2;
    // tmpE += link.flow;
    // Double for non-loops to capture flow in both directions, assuming symmetric tele flow
    // double teleFlow = nodeTeleportFlow[link.source] * nodeTeleportWeights[link.target] * (link.source == link.target ? 1 : 2);
    // Log() << "  " << link.source << " - " << link.target << ": " << link.flow << " + " << teleFlow << " = " << (link.flow + teleFlow) << "\n";
  }

  // Log() << "\nLink teleportation flow:\n";
  // double sumLinkTeleFlow = 0.0;
  // for (unsigned int i = 0; i < N; ++i) {
  //   for (unsigned int j = i; j < N; ++j) {
  //     if (i != j || config.noSelfLinks) {
  //       sumLinkTeleFlow += nodeTeleportFlow[i] * nodeTeleportWeights[j] * (i == j ? 1 : 2);
  //       // if (i < 5 && j < 5)
  //       //   Log() << "  " << i << " - " << j << ": " << nodeTeleportFlow[i] * nodeTeleportWeights[j] * 2 << "\n";
  //     }
  //   }
  // }
  // Log() << "Sum link tele flow: " << sumLinkTeleFlow << "\n";

  // Log() << "\nSum node flow: " << tmpN << ", sum link flow: " << tmpE << "\n\n";
  // Log() << "Node flow (sum: " << tmpN << "):\n";
  // for (unsigned int i = 0; i < N; ++i)
  //   Log() << "  " << nodeFlow[i] << ", alpha: " << alpha[i] << ", t: " << t(i) << ", s: " << s[i] << ", k: " << k[i] << ", u: " << u(i) << "\n";
  // Log() << "Link flow (sum: " << tmpE << " + tele: " << sumLinkTeleFlow << " = " << (tmpE + sumLinkTeleFlow) << "):\n";
  // for (auto& link : flowLinks)
  //   Log() << "  " << link.flow << ", ";
  // Log() << "\n";
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
  const auto iteration = [&](const auto iteration, const double alpha, const double beta) {
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
      Log() << "(Normalizing ranks after " << iteration << " power iterations with error " << nodeFlowDiff << ") ";
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
  // auto getDefaultTeleFlow = [nodeFlow, nodeOutDegree, config](unsigned int i) {
  //   auto alpha = nodeOutDegree[i] == 0 ? 1 : config.teleportationProbability;
  //   return alpha * nodeFlow[i];
  // }

  // Log() << "\nFinal node flow:\n";
  double sumTeleFlow = 0.0;
  for (auto& nodeIt : network.m_nodes) {
    auto& node = nodeIt.second;
    const auto nodeIndex = nodeIndexMap[node.id];
    node.flow = nodeFlow[nodeIndex];
    node.weight = nodeTeleportWeights[nodeIndex];
    node.teleFlow = !nodeTeleportFlow.empty() ? nodeTeleportFlow[nodeIndex] : nodeFlow[nodeIndex] * (nodeOutDegree[nodeIndex] == 0 ? 1 : config.teleportationProbability);
    node.enterFlow = node.flow;
    node.exitFlow = node.flow;

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
    // if (config.isUndirectedFlow()) {
    //   node.enterFlow *= 0.5;
    //   node.exitFlow *= 0.5;
    // }
    sumNodeFlow += node.flow;
    sumTeleFlow += node.teleFlow;
    // Log() << "\nNode " << node.id << " (idx: " << nodeIndex << "): flow: " << node.flow << ", weight: " << node.weight << ", teleFlow: " << node.teleFlow << " (=> alpha: " << (node.teleFlow / node.flow) << ", adj: " << (node.teleFlow / node.flow * (1 - node.weight)) << ")"; //, enter + exit: " << (node.enterFlow + node.exitFlow) << ", ratio: " << (node.enterFlow + node.exitFlow) / node.flow;
  }
  // Log() << "\n";


  // Enter/exit flow

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
    for (auto& nodeIt : network.m_nodes) {
      auto& node = nodeIt.second;
      const auto nodeIndex = nodeIndexMap[node.id];
      node.enterFlow = enterFlow[nodeIndex];
      node.exitFlow = exitFlow[nodeIndex];
    }
  }

  Log() << "\n  => Sum node flow: " << sumNodeFlow << ", sum link flow: " << sumLinkFlow << "\n";


  // Log() << "sumLinkWeight: " << sumLinkWeight << "\n";
  // Log() << "Node flow:\n";
  // for (auto& nodeIt : network.m_nodes) {
  //   auto& node = nodeIt.second;
  //   auto ind = nodeIndexMap[node.id];
  //   auto k = nodeOutDegree[ind];
  //   Log() << node.id << ":  " << node.flow << " (enter: " << node.enterFlow << ", exit: " << node.exitFlow << ", k: " << k << ")\n";
  // }
  // Log() << "\n";
}

} // namespace infomap
