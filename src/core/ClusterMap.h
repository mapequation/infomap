/*******************************************************************************
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
 ******************************************************************************/

#ifndef _CLUSTERMAP_H_
#define _CLUSTERMAP_H_

#include <string>
#include <map>
#include <vector>
#include <deque>

namespace infomap {

using Path = std::deque<unsigned int>; // 1-based indexing

using NodePath = std::pair<unsigned int, Path>;

using NodePaths = std::vector<NodePath>;

class ClusterMap {
public:
  void readClusterData(const std::string& filename, bool includeFlow = false, const std::map<unsigned int, std::map<unsigned int, unsigned int>>* layerNodeToStateId = nullptr);

  const std::map<unsigned int, unsigned int>& clusterIds() const noexcept
  {
    return m_clusterIds;
  }

  const std::map<unsigned int, double>& getFlow() const noexcept
  {
    return m_flowData;
  }

  const NodePaths& nodePaths() const noexcept { return m_nodePaths; }

  const std::string& extension() const noexcept { return m_extension; }

  bool isHigherOrder() const noexcept { return m_isHigherOrder; }

protected:
  void readTree(const std::string& filename, bool includeFlow, const std::map<unsigned int, std::map<unsigned int, unsigned int>>* layerNodeToStateId = nullptr);
  void readClu(const std::string& filename, bool includeFlow, const std::map<unsigned int, std::map<unsigned int, unsigned int>>* layerNodeToStateId = nullptr);

private:
  std::map<unsigned int, unsigned int> m_clusterIds;
  std::map<unsigned int, double> m_flowData;
  NodePaths m_nodePaths;
  std::string m_extension;
  bool m_isHigherOrder = false;
};

} // namespace infomap

#endif /* _CLUSTERMAP_H_ */
