/*
 * ClusterMap.h
 *
 *  Created on: 8 apr 2015
 *      Author: Daniel
 */

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
  void readClusterData(const std::string& filename, bool includeFlow = false);

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
  void readTree(const std::string& filename, /* [[maybe_unused]] */ bool includeFlow);
  void readClu(const std::string& filename, bool includeFlow);

private:
  std::map<unsigned int, unsigned int> m_clusterIds;
  std::map<unsigned int, double> m_flowData;
  NodePaths m_nodePaths;
  std::string m_extension;
  bool m_isHigherOrder = false;
};

} // namespace infomap

#endif /* _CLUSTERMAP_H_ */
