#ifndef INFOMAP_TEST_UTILS_H_
#define INFOMAP_TEST_UTILS_H_

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace infomap {
namespace test {

inline std::string repoPath(const std::string& relativePath)
{
  return std::string(INFOMAP_TEST_ROOT) + "/" + relativePath;
}

inline std::vector<std::vector<unsigned int>> canonicalPartition(const std::map<unsigned int, unsigned int>& modules)
{
  std::map<unsigned int, std::vector<unsigned int>> groupedModules;
  for (const auto& module : modules) {
    groupedModules[module.second].push_back(module.first);
  }

  std::vector<std::vector<unsigned int>> partition;
  for (auto& groupedModule : groupedModules) {
    auto& nodes = groupedModule.second;
    std::sort(nodes.begin(), nodes.end());
    partition.push_back(nodes);
  }

  std::sort(partition.begin(), partition.end());
  return partition;
}

inline std::string defaultFlags(const std::string& extraFlags = "")
{
  return extraFlags.empty() ? "--seed 123 --num-trials 1 --silent" : "--seed 123 --num-trials 1 --silent " + extraFlags;
}

} // namespace test
} // namespace infomap

#endif // INFOMAP_TEST_UTILS_H_
