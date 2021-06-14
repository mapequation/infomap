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
  static void calculateFlow(StateNetwork& network, const Config& config) noexcept
  {
    FlowCalculator f(network, config);
  }

protected:
  FlowCalculator(StateNetwork&, const Config&);

  using FlowLink = detail::FlowLink;

  void calcUndirectedFlow() noexcept;
  void calcDirectedFlow(const StateNetwork&, const Config&) noexcept;
  void calcDirectedBipartiteFlow(const StateNetwork&, const Config&) noexcept;
  void calcDirdirFlow(const Config&) noexcept;
  void calcRawdirFlow() noexcept;

  void finalize(StateNetwork&, const Config&, bool) noexcept;

  unsigned int numNodes;
  unsigned int numLinks;
  unsigned int nonDanglingStartIndex = 0;
  unsigned int bipartiteStartIndex = 0;
  unsigned int bipartiteLinkStartIndex = 0;

  double sumLinkWeight;
  double sumUndirLinkWeight;

  std::map<unsigned int, unsigned int> nodeIndexMap;
  std::vector<double> nodeFlow;
  std::vector<double> nodeTeleportRates;
  std::vector<double> sumLinkOutWeight;
  std::vector<unsigned int> nodeOutDegree;
  std::vector<FlowLink> flowLinks;
};

} // namespace infomap

#endif /* FLOWCALCULATOR_H_ */
