/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef FLOW_CALCULATOR_H_
#define FLOW_CALCULATOR_H_

#include <map>
#include <vector>

namespace infomap {

struct Config;
class InfomapBase;
class StateNetwork;

namespace detail {
  struct FlowLink {
    unsigned int source;
    unsigned int target;
    double flow;
  };
} // namespace detail

/**
 * Calculate flow on network based on different flow models
 */
class FlowCalculator {
public:
  FlowCalculator(StateNetwork&, const Config&, InfomapBase* interruptOwner = nullptr);

private:
  void calcUndirectedFlow() noexcept;
  void calcDirectedFlow(const StateNetwork&, const Config&);
  void calcUndirectedRegularizedFlow(const StateNetwork&, const Config&) noexcept;
  void calcDirectedRegularizedFlow(const StateNetwork&, const Config&);
  void calcDirectedBipartiteFlow(const StateNetwork&, const Config&);
  void calcDirdirFlow(const Config&) noexcept;
  void calcRawdirFlow() noexcept;
  void usePrecomputedFlow(const StateNetwork&, const Config&);

  void finalize(StateNetwork&, const Config&, bool) noexcept;

  unsigned int numNodes;
  unsigned int nonDanglingStartIndex = 0;
  unsigned int bipartiteStartIndex = 0;
  unsigned int bipartiteLinkStartIndex = 0;

  double sumLinkWeight = 0;
  double sumWeightedDegree = 0;

  std::map<unsigned int, unsigned int> nodeIndexMap;
  std::vector<double> nodeFlow;
  std::vector<double> nodeTeleportWeights;
  std::vector<double> nodeTeleportFlow;
  std::vector<double> enterFlow;
  std::vector<double> exitFlow;
  std::vector<double> sumLinkOutWeight;
  std::vector<unsigned int> nodeOutDegree;
  using FlowLink = detail::FlowLink;
  std::vector<FlowLink> flowLinks;
  InfomapBase* m_interruptOwner = nullptr;
};

inline void calculateFlow(StateNetwork& network, const Config& config, InfomapBase* interruptOwner = nullptr)
{
  FlowCalculator(network, config, interruptOwner);
}

} // namespace infomap

#endif // FLOW_CALCULATOR_H_
