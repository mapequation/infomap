#ifndef INFOMAP_TEST_UTILS_H_
#define INFOMAP_TEST_UTILS_H_

#include "vendor/doctest.h"
#include "Infomap.h"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <set>
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

inline std::string fixturePath(const std::string& relativePath)
{
  return repoPath("test/fixtures/" + relativePath);
}

inline std::string networkFixturePath(const std::string& filename)
{
  return fixturePath("networks/" + filename);
}

inline std::string clusterFixturePath(const std::string& filename)
{
  return fixturePath("clusters/" + filename);
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
  std::ifstream input(fixturePath(relativePath));
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

template <typename SetupFn>
inline std::unique_ptr<InfomapWrapper> makeRunningInfomap(SetupFn&& setup, const std::string& extraFlags = "")
{
  std::unique_ptr<InfomapWrapper> im(new InfomapWrapper(defaultFlags(extraFlags)));
  setup(*im);
  im->run();
  return im;
}

inline std::string readTextFile(const std::string& path)
{
  std::ifstream input(path.c_str());
  if (!input) {
    throw std::runtime_error("Could not open text file: " + path);
  }

  std::ostringstream output;
  output << input.rdbuf();
  return output.str();
}

inline void readNetworkFixture(InfomapWrapper& im, const std::string& filename, bool accumulate = true)
{
  im.readInputData(networkFixturePath(filename), accumulate);
}

inline std::vector<unsigned int> sortedLeafIds(InfomapWrapper& im, bool states = false)
{
  std::vector<unsigned int> ids;
  for (auto it = im.iterLeafNodes(); !it.isEnd(); ++it) {
    const auto& node = *it;
    ids.push_back(states ? node.stateId : node.physicalId);
  }
  std::sort(ids.begin(), ids.end());
  return ids;
}

inline void checkCanonicalPartition(InfomapWrapper& im, const std::vector<std::vector<unsigned int>>& expectedPartition, bool states = false)
{
  CHECK(canonicalPartition(im.getModules(1, states)) == expectedPartition);
}

inline void checkApproxCodelength(double actual, double expected, double epsilon = 1e-10)
{
  CHECK(actual == doctest::Approx(expected).epsilon(epsilon));
}

inline void checkRunSanity(InfomapWrapper& im)
{
  CHECK(std::isfinite(im.codelength()));
  CHECK(std::isfinite(im.getIndexCodelength()));
  CHECK(im.codelength() >= 0.0);
  CHECK(im.getIndexCodelength() >= 0.0);
  CHECK(im.numLevels() >= 1);

  const auto useStates = im.network().haveMemoryInput();
  const auto modules = im.getModules(1, useStates);
  CHECK(modules.size() == im.numLeafNodes());

  auto coveredIds = sortedLeafIds(im, useStates);
  std::vector<unsigned int> partitionIds;
  for (const auto& module : canonicalPartition(modules)) {
    partitionIds.insert(partitionIds.end(), module.begin(), module.end());
  }
  std::sort(partitionIds.begin(), partitionIds.end());
  CHECK(partitionIds == coveredIds);
}

} // namespace test
} // namespace infomap

#endif // INFOMAP_TEST_UTILS_H_
