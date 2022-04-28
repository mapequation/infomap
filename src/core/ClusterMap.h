/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Eriksson, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_AGPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
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

  const NodePaths& nodePaths() const noexcept { return m_nodePaths; }

  const std::string& extension() const noexcept { return m_extension; }

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
