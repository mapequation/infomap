#include "vendor/doctest.h"

#include "Infomap.h"

#include "TestUtils.h"

#include <set>
#include <tuple>
#include <vector>

namespace {

using infomap::InfomapWrapper;

using InternalEdge = std::tuple<unsigned int, unsigned int, double, double>;

std::multiset<InternalEdge> internalEdgesForModule(infomap::InfoNode& module)
{
  std::multiset<InternalEdge> edges;
  for (auto& node : module) {
    for (auto* edge : node.outEdges()) {
      if (edge->target->parent == &module) {
        edges.emplace(edge->source->stateId, edge->target->stateId, edge->data.weight, edge->data.flow);
      }
    }
  }
  return edges;
}

TEST_CASE("Infomap partitions the unweighted two-triangle fixture into two modules [fast][core][lifecycle]")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  infomap::test::addEdgeFixtureLinks(im, "graphs/twotriangles_unweighted.edges");

  im.run();

  infomap::test::checkRunSanity(im);
  CHECK(im.numTopModules() == 2);
  CHECK(im.numLevels() == 2);
  infomap::test::checkCanonicalPartition(im, {{1, 2, 3}, {4, 5, 6}});
}

TEST_CASE("Infomap can rerun the same multi-trial instance safely [fast][core][lifecycle][crash]")
{
  InfomapWrapper im(infomap::test::defaultFlags("--num-trials 2"));
  infomap::test::addEdgeFixtureLinks(im, "graphs/recorded_teleportation_directed.edges");

  im.run();
  infomap::test::checkRunSanity(im);

  const auto firstPartition = infomap::test::canonicalPartition(im.getModules());
  const auto firstCodelength = im.codelength();
  const auto firstIndexCodelength = im.getIndexCodelength();

  im.run();
  infomap::test::checkRunSanity(im);

  CHECK(infomap::test::canonicalPartition(im.getModules()) == firstPartition);
  CHECK(im.codelength() == doctest::Approx(firstCodelength));
  CHECK(im.getIndexCodelength() == doctest::Approx(firstIndexCodelength));
}

TEST_CASE("Infomap reruns ninetriangles deterministically on the same instance [fast][core][lifecycle][crash]")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  im.readInputData(infomap::test::repoPath("examples/networks/ninetriangles.net"));

  im.run();
  infomap::test::checkRunSanity(im);

  const auto firstPartition = infomap::test::canonicalPartition(im.getModules());
  const auto firstCodelength = im.codelength();
  const auto firstIndexCodelength = im.getIndexCodelength();

  im.run();
  infomap::test::checkRunSanity(im);

  CHECK(infomap::test::canonicalPartition(im.getModules()) == firstPartition);
  CHECK(im.codelength() == doctest::Approx(firstCodelength));
  CHECK(im.getIndexCodelength() == doctest::Approx(firstIndexCodelength));
}

TEST_CASE("readInputData accumulate=false replaces the previous network [fast][core][lifecycle][parser]")
{
  InfomapWrapper im(infomap::test::defaultFlags());

  infomap::test::readNetworkFixture(im, "accumulate_a.net", false);
  infomap::test::readNetworkFixture(im, "accumulate_b.net", false);

  CHECK(im.network().numNodes() == 2);
  CHECK(im.network().numLinks() == 1);

  im.run();

  infomap::test::checkRunSanity(im);
  CHECK(im.numTopModules() == 1);
  infomap::test::checkCanonicalPartition(im, {{3, 4}});
}

TEST_CASE("readInputData accumulate=true appends first-order fixtures [fast][core][lifecycle][parser]")
{
  InfomapWrapper im(infomap::test::defaultFlags());

  infomap::test::readNetworkFixture(im, "accumulate_a.net", false);
  infomap::test::readNetworkFixture(im, "accumulate_b.net", true);

  CHECK(im.network().numNodes() == 4);
  CHECK(im.network().numLinks() == 2);

  im.run();

  infomap::test::checkRunSanity(im);
  CHECK(im.numTopModules() == 2);
  infomap::test::checkCanonicalPartition(im, {{1, 2}, {3, 4}});
}

TEST_CASE("Higher-order module queries require state ids [fast][core][lifecycle]")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  infomap::test::readNetworkFixture(im, "states.net");

  im.run();

  infomap::test::checkRunSanity(im);
  CHECK(im.numTopModules() == 2);
  CHECK(im.numLevels() == 2);
  infomap::test::checkCanonicalPartition(im, {{1, 2, 3}, {4, 5, 6}}, true);
  CHECK_THROWS_WITH_AS(im.getModules(false), "Cannot get modules on higher-order network without states.", std::runtime_error);
}

TEST_CASE("File-backed multilayer input clusters as a higher-order network [fast][core][lifecycle][parser]")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  infomap::test::readNetworkFixture(im, "multilayer.net");

  CHECK(im.network().haveMemoryInput());
  CHECK(im.network().numPhysicalNodes() == 5);

  im.run();

  infomap::test::checkRunSanity(im);
  CHECK(im.network().haveMemoryInput());
  CHECK(im.getModules(1, true).size() == im.numLeafNodes());
}

TEST_CASE("Subnetwork generation preserves parent indices and clones internal edges [fast][core][lifecycle][subnetwork]")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  im.readInputData(infomap::test::repoPath("examples/networks/twotriangles.net"));
  im.run();

  infomap::test::checkRunSanity(im);
  REQUIRE(im.numTopModules() == 2);

  auto& module = *im.root().firstChild;
  REQUIRE(module.childDegree() == 3);

  const auto originalEdges = internalEdgesForModule(module);
  std::vector<unsigned int> sentinelIndices;
  unsigned int childIndex = 0;
  for (auto& node : module) {
    node.index = 100 + childIndex;
    sentinelIndices.push_back(node.index);
    ++childIndex;
  }

  auto& subInfomap = im.getSubInfomap(module).initNetwork(module);

  REQUIRE(subInfomap.numLeafNodes() == module.childDegree());
  CHECK(subInfomap.root().owner == &module);
  CHECK(internalEdgesForModule(subInfomap.root()) == originalEdges);

  childIndex = 0;
  for (auto& node : module) {
    CHECK(node.index == sentinelIndices[childIndex]);
    ++childIndex;
  }
}

} // namespace
