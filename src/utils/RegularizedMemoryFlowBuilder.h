/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef REGULARIZED_MEMORY_FLOW_BUILDER_H_
#define REGULARIZED_MEMORY_FLOW_BUILDER_H_

#include "../core/StateNetwork.h"
#include "FlowLink.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace infomap {

struct RegularizedMemoryFlowResult {
  using Transition = std::pair<unsigned int, double>;

  std::vector<std::vector<Transition>> posterior;
  std::vector<double> nodeFlow;
  std::vector<double> nodeTeleportWeights;
  std::vector<double> nodeTeleportFlow;
  std::vector<double> enterFlow;
  std::vector<double> exitFlow;
  std::vector<double> observedLinkFlow;
  StateNetwork::NodeLinkMap effectiveLinkMap;
  unsigned int iterations = 0;
  double error = 0.0;
  bool converged = true;
};

class RegularizedMemoryFlowBuilder {
public:
  RegularizedMemoryFlowBuilder(const StateNetwork& network,
                               const Config& config,
                               const std::unordered_map<unsigned int, unsigned int>& nodeIndexMap,
                               const std::vector<detail::FlowLink>& observedLinks);

  RegularizedMemoryFlowResult build() const;

private:
  const StateNetwork& m_network;
  const Config& m_config;
  const std::unordered_map<unsigned int, unsigned int>& m_nodeIndexMap;
  const std::vector<detail::FlowLink>& m_observedLinks;
};

} // namespace infomap

#endif // REGULARIZED_MEMORY_FLOW_BUILDER_H_
