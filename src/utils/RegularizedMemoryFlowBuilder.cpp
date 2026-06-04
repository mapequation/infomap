/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "RegularizedMemoryFlowBuilder.h"

#include "../io/Config.h"
#include "../utils/convert.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>

namespace infomap {
namespace {

  using Transition = RegularizedMemoryFlowResult::Transition;
  using Key = std::pair<unsigned int, unsigned int>;

  constexpr double kConvergenceThreshold = 1.0e-15;

  std::string stateStructureError(const std::string& message)
  {
    return io::Str() << "Regularized memory networks require valid single-step second-order state structure: " << message;
  }

  void normalize(std::vector<double>& values)
  {
    double sum = std::accumulate(values.begin(), values.end(), 0.0);
    if (sum == 0.0) {
      throw std::runtime_error("Regularized memory stationary flow has zero total flow.");
    }
    for (auto& value : values) {
      value /= sum;
    }
  }

} // namespace

RegularizedMemoryFlowBuilder::RegularizedMemoryFlowBuilder(const StateNetwork& network,
                                                           const Config& config,
                                                           const std::unordered_map<unsigned int, unsigned int>& nodeIndexMap,
                                                           const std::vector<detail::FlowLink>& observedLinks)
    : m_network(network),
      m_config(config),
      m_nodeIndexMap(nodeIndexMap),
      m_observedLinks(observedLinks)
{
}

RegularizedMemoryFlowResult RegularizedMemoryFlowBuilder::build() const
{
  RegularizedMemoryFlowResult result;

  const unsigned int numStates = m_network.numNodes();
  const unsigned int numPhysicalNodes = m_network.numPhysicalNodes();
  if (numStates == 0 || numPhysicalNodes == 0) {
    throw std::runtime_error("Regularized memory flow requires a non-empty state network.");
  }

  std::vector<unsigned int> stateIds(numStates, 0);
  std::vector<unsigned int> physicalIds(numStates, 0);
  std::map<unsigned int, unsigned int> physicalIdToIndex;

  for (const auto& nodeIt : m_network.nodes()) {
    const auto index = m_nodeIndexMap.at(nodeIt.second.id);
    stateIds[index] = nodeIt.second.id;
    physicalIds[index] = nodeIt.second.physicalId;
    physicalIdToIndex.emplace(nodeIt.second.physicalId, physicalIdToIndex.size());
  }

  std::vector<unsigned int> context(numStates, 0);
  std::vector<bool> hasContext(numStates, false);
  for (const auto& link : m_observedLinks) {
    const auto candidateContext = physicalIds[link.source];
    if (hasContext[link.target] && context[link.target] != candidateContext) {
      throw std::runtime_error(stateStructureError(io::Str()
                                                   << "state node " << stateIds[link.target] << " receives contexts "
                                                   << context[link.target] << " and " << candidateContext << "."));
    }
    context[link.target] = candidateContext;
    hasContext[link.target] = true;
  }

  std::map<Key, unsigned int> targetByPhysicalContext;
  for (unsigned int index = 0; index < numStates; ++index) {
    if (!hasContext[index]) {
      continue;
    }
    Key key { physicalIds[index], context[index] };
    auto inserted = targetByPhysicalContext.emplace(key, index);
    if (!inserted.second) {
      throw std::runtime_error(stateStructureError(io::Str()
                                                   << "multiple state nodes map to physical/context pair ("
                                                   << key.first << ", " << key.second << ")."));
    }
  }

  const unsigned int numObservedPhysicalNodes = physicalIdToIndex.size();
  if (numObservedPhysicalNodes == 0 || numObservedPhysicalNodes > numPhysicalNodes) {
    throw std::runtime_error("Regularized memory flow found inconsistent physical node indexing.");
  }

  std::vector<double> sigmaOut(numObservedPhysicalNodes, 0.0);
  std::vector<double> sigmaIn(numObservedPhysicalNodes, 0.0);
  std::vector<std::set<unsigned int>> distinctOut(numObservedPhysicalNodes);
  std::vector<std::set<unsigned int>> distinctIn(numObservedPhysicalNodes);

  for (const auto& link : m_observedLinks) {
    const auto sourcePhysical = physicalIds[link.source];
    const auto targetPhysical = physicalIds[link.target];
    const auto sourcePhysicalIndex = physicalIdToIndex.at(sourcePhysical);
    const auto targetPhysicalIndex = physicalIdToIndex.at(targetPhysical);
    sigmaOut[sourcePhysicalIndex] += link.flow;
    sigmaIn[targetPhysicalIndex] += link.flow;
    distinctOut[sourcePhysicalIndex].insert(targetPhysical);
    distinctIn[targetPhysicalIndex].insert(sourcePhysical);
  }

  double sumKappa = 0.0;
  double sumSigma = 0.0;
  for (unsigned int i = 0; i < numObservedPhysicalNodes; ++i) {
    sumKappa += distinctIn[i].size() + distinctOut[i].size();
    sumSigma += sigmaIn[i] + sigmaOut[i];
  }
  if (sumSigma == 0.0 || sumKappa == 0.0) {
    throw std::runtime_error("Regularized memory flow requires observed physical transitions.");
  }

  const double lambda = m_config.regularizationStrength * std::log(static_cast<double>(numPhysicalNodes))
      / (static_cast<double>(numPhysicalNodes) * static_cast<double>(numPhysicalNodes));
  const double scale = sumKappa / sumSigma;

  result.posterior.assign(numStates, {});
  std::vector<std::map<unsigned int, double>> numerators(numStates);
  std::vector<double> denominator(numStates, 0.0);

  for (const auto& link : m_observedLinks) {
    numerators[link.source][link.target] += link.flow;
    denominator[link.source] += link.flow;
  }

  for (unsigned int source = 0; source < numStates; ++source) {
    const auto sourcePhysical = physicalIds[source];
    const auto sourcePhysicalIndex = physicalIdToIndex.at(sourcePhysical);
    const auto kappaSourceOut = static_cast<double>(distinctOut[sourcePhysicalIndex].size());
    const auto sigmaSourceOut = sigmaOut[sourcePhysicalIndex];
    if (kappaSourceOut == 0.0 || sigmaSourceOut == 0.0 || lambda == 0.0) {
      continue;
    }

    for (const auto& physicalTarget : physicalIdToIndex) {
      const auto targetPhysical = physicalTarget.first;
      const auto targetPhysicalIndex = physicalTarget.second;
      if (m_config.noSelfLinks && sourcePhysical == targetPhysical) {
        continue;
      }
      auto targetIt = targetByPhysicalContext.find(Key { targetPhysical, sourcePhysical });
      if (targetIt == targetByPhysicalContext.end()) {
        continue;
      }

      const auto kappaTargetIn = static_cast<double>(distinctIn[targetPhysicalIndex].size());
      const auto sigmaTargetIn = sigmaIn[targetPhysicalIndex];
      if (kappaTargetIn == 0.0 || sigmaTargetIn == 0.0) {
        continue;
      }

      const double gamma = lambda * scale * sigmaSourceOut * sigmaTargetIn / (kappaSourceOut * kappaTargetIn);
      if (gamma <= 0.0 || !std::isfinite(gamma)) {
        continue;
      }
      numerators[source][targetIt->second] += gamma;
      denominator[source] += gamma;
    }
  }

  for (unsigned int source = 0; source < numStates; ++source) {
    if (denominator[source] == 0.0) {
      throw std::runtime_error(io::Str()
                               << "Regularized memory source state " << stateIds[source]
                               << " at physical node " << physicalIds[source]
                               << " has no observed outgoing support and no valid second-order prior targets. "
                               << "Remove the sink state or run without --regularized.");
    }
    for (const auto& targetNumerator : numerators[source]) {
      result.posterior[source].push_back({ targetNumerator.first, targetNumerator.second / denominator[source] });
    }
  }

  result.nodeFlow.assign(numStates, 1.0 / static_cast<double>(numStates));
  std::vector<double> nextFlow(numStates, 0.0);
  for (unsigned int iteration = 0; iteration < m_config.maxFlowIterations; ++iteration) {
    std::fill(nextFlow.begin(), nextFlow.end(), 0.0);
    for (unsigned int source = 0; source < numStates; ++source) {
      for (const auto& transition : result.posterior[source]) {
        nextFlow[transition.first] += result.nodeFlow[source] * transition.second;
      }
    }
    normalize(nextFlow);

    double error = 0.0;
    for (unsigned int i = 0; i < numStates; ++i) {
      error += std::abs(nextFlow[i] - result.nodeFlow[i]);
    }
    result.nodeFlow = nextFlow;
    result.error = error;
    result.iterations = iteration + 1;
    if (iteration >= 50 && error <= kConvergenceThreshold) {
      break;
    }
  }
  result.converged = result.error <= kConvergenceThreshold || result.iterations < m_config.maxFlowIterations;

  result.nodeTeleportWeights.assign(numStates, 0.0);
  result.nodeTeleportFlow.assign(numStates, 0.0);
  result.enterFlow.assign(numStates, 0.0);
  result.exitFlow.assign(numStates, 0.0);
  result.observedLinkFlow.assign(m_observedLinks.size(), 0.0);

  for (unsigned int source = 0; source < numStates; ++source) {
    StateNetwork::StateNode sourceNode(stateIds[source]);
    auto& effectiveTargets = result.effectiveLinkMap[sourceNode];
    for (const auto& transition : result.posterior[source]) {
      const auto target = transition.first;
      const double flow = result.nodeFlow[source] * transition.second;
      StateNetwork::StateNode targetNode(stateIds[target]);
      auto inserted = effectiveTargets.insert(std::make_pair(targetNode, StateNetwork::LinkData(transition.second)));
      auto& data = inserted.first->second;
      if (inserted.second) {
        data.flow = flow;
      } else {
        data.weight += transition.second;
        data.flow += flow;
      }
      if (source != target) {
        result.exitFlow[source] += flow;
        result.enterFlow[target] += flow;
      }
    }
  }

  for (unsigned int i = 0; i < m_observedLinks.size(); ++i) {
    const auto& link = m_observedLinks[i];
    result.observedLinkFlow[i] = denominator[link.source] > 0.0
        ? result.nodeFlow[link.source] * link.flow / denominator[link.source]
        : 0.0;
  }

  return result;
}

} // namespace infomap
