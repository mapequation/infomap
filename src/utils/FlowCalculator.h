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


#ifndef FLOWCALCULATOR_H_
#define FLOWCALCULATOR_H_

#include <map>
#include <vector>

namespace infomap {

struct Config;
class StateNetwork;
enum class FlowModel;

namespace detail {
  struct FlowLink {
    explicit FlowLink(unsigned int sourceIndex = 0, unsigned int targetIndex = 0, double weight = 0.0)
        : source(sourceIndex),
          target(targetIndex),
          weight(weight),
          flow(weight) { }

    unsigned int source;
    unsigned int target;
    double weight;
    double flow;
  };
} // namespace detail

/**
 * Calculate flow on network based on different flow models
 */
class FlowCalculator {
public:
  static void calculateFlow(StateNetwork& network, const Config &config) noexcept {
    FlowCalculator f(network, config);
  }

protected:
  FlowCalculator(StateNetwork& network, const Config& config);

  using FlowLink = detail::FlowLink;

  void calcUndirectedFlow() noexcept;
  void calcDirectedFlow(const StateNetwork& network, const Config& config) noexcept;
  void calcDirdirFlow(FlowModel flowModel) noexcept;
  void calcRawdirFlow() noexcept;

  void finalize(StateNetwork& network, const Config& config, bool normalizeNodeFlow) noexcept;

  unsigned int numNodes;
  unsigned int numLinks;

  double sumLinkWeight;
  double sumUndirLinkWeight;

  std::map<unsigned int, unsigned int> m_nodeIndexMap;
  std::vector<double> m_nodeFlow;
  std::vector<double> m_nodeTeleportRates;
  std::vector<double> sumLinkOutWeight;
  std::vector<unsigned int> nodeOutDegree;
  std::vector<FlowLink> m_flowLinks;
};

} // namespace infomap

#endif /* FLOWCALCULATOR_H_ */
