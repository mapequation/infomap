#ifndef INFOMAP_TEST_UTILS_H_
#define INFOMAP_TEST_UTILS_H_

#include "Infomap.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace infomap {
namespace test {

struct EdgeFixtureLink {
  unsigned int sourceId = 0;
  unsigned int targetId = 0;
  double weight = 1.0;
};

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

inline std::vector<EdgeFixtureLink> readEdgeFixture(const std::string& relativePath)
{
  std::ifstream input(repoPath("test/fixtures/" + relativePath));
  if (!input) {
    throw std::runtime_error("Could not open edge fixture: " + relativePath);
  }

  std::vector<EdgeFixtureLink> links;
  std::string rawLine;
  unsigned int lineNumber = 0;
  while (std::getline(input, rawLine)) {
    ++lineNumber;
    auto commentPos = rawLine.find('#');
    auto line = rawLine.substr(0, commentPos);

    if (line.find_first_not_of(" \t\r\n") == std::string::npos) {
      continue;
    }

    std::stringstream lineStream(line);
    EdgeFixtureLink link;
    if (!(lineStream >> link.sourceId >> link.targetId)) {
      throw std::runtime_error("Invalid edge fixture line " + std::to_string(lineNumber) + " in " + relativePath);
    }
    if (!(lineStream >> link.weight)) {
      link.weight = 1.0;
    }
    links.push_back(link);
  }

  return links;
}

inline void addEdgeFixtureLinks(InfomapWrapper& im, const std::string& relativePath)
{
  for (const auto& link : readEdgeFixture(relativePath)) {
    im.addLink(link.sourceId, link.targetId, link.weight);
  }
}

} // namespace test
} // namespace infomap

#endif // INFOMAP_TEST_UTILS_H_
