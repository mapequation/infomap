#include "vendor/doctest.h"

#include "Infomap.h"

#include "TestUtils.h"

#include <set>
#include <tuple>
#include <vector>

namespace {

using infomap::InfomapWrapper;

using InternalEdge = std::tuple<unsigned int, unsigned int, double, double>;
using NodeIdentity = std::tuple<unsigned int, unsigned int, std::vector<int>>;

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

std::vector<NodeIdentity> nodeIdentitiesForModule(infomap::InfoNode& module)
{
  std::vector<NodeIdentity> identities;
  for (auto& node : module) {
    identities.emplace_back(node.stateId, node.physicalId, node.metaData);
  }
  std::sort(identities.begin(), identities.end());
  return identities;
}

std::vector<std::vector<unsigned int>> canonicalSubInfomapPartition(infomap::InfomapBase& subInfomap, bool states)
{
  std::map<unsigned int, unsigned int> modules;
  unsigned int moduleIndex = 0;
  for (auto& topModule : subInfomap.root()) {
    if (topModule.isLeaf()) {
      modules[states ? topModule.stateId : topModule.physicalId] = moduleIndex;
    } else {
      for (auto& leaf : topModule) {
        modules[states ? leaf.stateId : leaf.physicalId] = moduleIndex;
      }
    }
    ++moduleIndex;
  }
  return infomap::test::canonicalPartition(modules);
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

TEST_CASE("readInputData accumulate mode stays stable across multiple runs on the same instance [fast][core][lifecycle][parser]")
{
  InfomapWrapper im(infomap::test::defaultFlags());

  infomap::test::readNetworkFixture(im, "accumulate_a.net", false);
  CHECK(im.network().numNodes() == 2);
  CHECK(im.network().numLinks() == 1);

  im.run();
  infomap::test::checkRunSanity(im);
  CHECK(im.numTopModules() == 1);
  infomap::test::checkCanonicalPartition(im, {{1, 2}});

  infomap::test::readNetworkFixture(im, "accumulate_b.net", true);
  CHECK(im.network().numNodes() == 4);
  CHECK(im.network().numLinks() == 2);

  im.run();
  infomap::test::checkRunSanity(im);
  CHECK(im.numTopModules() == 2);
  infomap::test::checkCanonicalPartition(im, {{1, 2}, {3, 4}});

  infomap::test::readNetworkFixture(im, "accumulate_b.net", false);
  CHECK(im.network().numNodes() == 2);
  CHECK(im.network().numLinks() == 1);

  im.run();
  infomap::test::checkRunSanity(im);
  CHECK(im.numTopModules() == 1);
  infomap::test::checkCanonicalPartition(im, {{3, 4}});
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

TEST_CASE("Higher-order input reruns deterministically on the same instance [fast][core][lifecycle][crash]")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  infomap::test::readNetworkFixture(im, "states.net");

  im.run();
  infomap::test::checkRunSanity(im);

  const auto firstPartition = infomap::test::canonicalPartition(im.getModules(1, true));
  const auto firstCodelength = im.codelength();
  const auto firstIndexCodelength = im.getIndexCodelength();

  im.run();
  infomap::test::checkRunSanity(im);

  CHECK(infomap::test::canonicalPartition(im.getModules(1, true)) == firstPartition);
  CHECK(im.codelength() == doctest::Approx(firstCodelength));
  CHECK(im.getIndexCodelength() == doctest::Approx(firstIndexCodelength));
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

TEST_CASE("Invalid multilayer input failure does not poison later valid multilayer init on the same instance [fast][core][lifecycle][parser]")
{
  InfomapWrapper im(infomap::test::defaultFlags());

  CHECK_THROWS_AS(infomap::test::readNetworkFixture(im, "invalid_multilayer.net"), std::runtime_error);

  infomap::test::readNetworkFixture(im, "multilayer.net");
  CHECK(im.network().haveMemoryInput());
  CHECK(im.network().numPhysicalNodes() == 5);

  im.run();

  infomap::test::checkRunSanity(im);
  CHECK(im.network().haveMemoryInput());
  CHECK(im.getModules(1, true).size() == im.numLeafNodes());
}

TEST_CASE("File-backed multilayer input reruns deterministically on the same instance [fast][core][lifecycle][crash]")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  infomap::test::readNetworkFixture(im, "multilayer.net");

  im.run();
  infomap::test::checkRunSanity(im);
  CHECK(im.network().haveMemoryInput());
  CHECK(im.network().numPhysicalNodes() == 5);

  const auto firstPartition = infomap::test::canonicalPartition(im.getModules(1, true));
  const auto firstCodelength = im.codelength();
  const auto firstIndexCodelength = im.getIndexCodelength();

  im.run();
  infomap::test::checkRunSanity(im);
  CHECK(im.network().haveMemoryInput());
  CHECK(im.network().numPhysicalNodes() == 5);

  CHECK(infomap::test::canonicalPartition(im.getModules(1, true)) == firstPartition);
  CHECK(im.codelength() == doctest::Approx(firstCodelength));
  CHECK(im.getIndexCodelength() == doctest::Approx(firstIndexCodelength));
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

TEST_CASE("Subnetwork reuse and dispose stay stable on the same parent module [fast][core][lifecycle][subnetwork]")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  im.readInputData(infomap::test::repoPath("examples/networks/twotriangles.net"));
  im.run();

  infomap::test::checkRunSanity(im);
  REQUIRE(im.numTopModules() == 2);

  auto& module = *im.root().firstChild;
  const auto originalEdges = internalEdgesForModule(module);
  std::vector<unsigned int> expectedLeafIds;
  for (const auto& node : module) {
    expectedLeafIds.push_back(node.stateId);
  }
  std::sort(expectedLeafIds.begin(), expectedLeafIds.end());

  auto runSubnetwork = [&]() -> std::tuple<std::vector<std::vector<unsigned int>>, double, double> {
    auto& subInfomap = im.getSubInfomap(module).initNetwork(module);
    REQUIRE(module.getInfomapRoot() == &subInfomap.root());
    REQUIRE(subInfomap.root().owner == &module);
    CHECK(internalEdgesForModule(subInfomap.root()) == originalEdges);

    subInfomap.run();
    CHECK(std::isfinite(subInfomap.codelength()));
    CHECK(std::isfinite(subInfomap.getIndexCodelength()));
    CHECK(subInfomap.codelength() >= -1e-12);
    CHECK(subInfomap.getIndexCodelength() >= -1e-12);
    CHECK(subInfomap.numLevels() >= 1);
    CHECK(subInfomap.numLeafNodes() == module.childDegree());

    std::map<unsigned int, unsigned int> modules;
    unsigned int moduleIndex = 0;
    for (auto& topModule : subInfomap.root()) {
      if (topModule.isLeaf()) {
        modules[topModule.stateId] = moduleIndex;
      } else {
        for (auto& leaf : topModule) {
          modules[leaf.stateId] = moduleIndex;
        }
      }
      ++moduleIndex;
    }

    std::vector<unsigned int> coveredIds;
    for (const auto& moduleEntry : modules) {
      coveredIds.push_back(moduleEntry.first);
    }
    CHECK(coveredIds == expectedLeafIds);

    return {infomap::test::canonicalPartition(modules), subInfomap.codelength(), subInfomap.getIndexCodelength()};
  };

  const auto firstRun = runSubnetwork();
  const auto secondRun = runSubnetwork();
  const auto& firstPartition = std::get<0>(firstRun);
  const auto firstCodelength = std::get<1>(firstRun);
  const auto firstIndexCodelength = std::get<2>(firstRun);
  const auto& secondPartition = std::get<0>(secondRun);
  const auto secondCodelength = std::get<1>(secondRun);
  const auto secondIndexCodelength = std::get<2>(secondRun);

  CHECK(secondPartition == firstPartition);
  CHECK(secondCodelength == doctest::Approx(firstCodelength));
  CHECK(secondIndexCodelength == doctest::Approx(firstIndexCodelength));

  CHECK(module.disposeInfomap());
  CHECK(module.getInfomapRoot() == nullptr);
}

TEST_CASE("Higher-order subnetwork rebuild preserves state identities and internal edges [fast][core][lifecycle][subnetwork]")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  infomap::test::readNetworkFixture(im, "states.net");
  im.run();

  infomap::test::checkRunSanity(im);
  REQUIRE(im.numTopModules() == 2);

  auto& module = *im.root().firstChild;
  REQUIRE(module.childDegree() >= 2);

  const auto originalEdges = internalEdgesForModule(module);
  const auto originalIdentities = nodeIdentitiesForModule(module);

  auto runSubnetwork = [&]() -> std::tuple<std::vector<std::vector<unsigned int>>, double, double> {
    auto& subInfomap = im.getSubInfomap(module).initNetwork(module);
    REQUIRE(subInfomap.root().owner == &module);
    REQUIRE(subInfomap.numLeafNodes() == module.childDegree());
    CHECK(internalEdgesForModule(subInfomap.root()) == originalEdges);
    CHECK(nodeIdentitiesForModule(subInfomap.root()) == originalIdentities);

    subInfomap.run();
    CHECK(std::isfinite(subInfomap.codelength()));
    CHECK(std::isfinite(subInfomap.getIndexCodelength()));
    CHECK(subInfomap.codelength() >= -1e-12);
    CHECK(subInfomap.getIndexCodelength() >= -1e-12);
    CHECK(subInfomap.numLevels() >= 1);

    return {canonicalSubInfomapPartition(subInfomap, true), subInfomap.codelength(), subInfomap.getIndexCodelength()};
  };

  const auto firstRun = runSubnetwork();
  const auto secondRun = runSubnetwork();

  CHECK(std::get<0>(secondRun) == std::get<0>(firstRun));
  CHECK(std::get<1>(secondRun) == doctest::Approx(std::get<1>(firstRun)));
  CHECK(std::get<2>(secondRun) == doctest::Approx(std::get<2>(firstRun)));
}

TEST_CASE("Metadata-bearing subnetwork rebuild preserves leaf metadata [fast][core][lifecycle][subnetwork][parser]")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  im.readInputData(infomap::test::repoPath("examples/networks/twotriangles.net"));
  im.initMetaData(infomap::test::fixturePath("meta/twotriangles.meta"));
  im.run();

  infomap::test::checkRunSanity(im);
  REQUIRE(im.numTopModules() == 2);

  auto& module = *im.root().firstChild;
  const auto originalEdges = internalEdgesForModule(module);
  const auto originalIdentities = nodeIdentitiesForModule(module);
  REQUIRE_FALSE(originalIdentities.empty());

  auto& subInfomap = im.getSubInfomap(module).initNetwork(module);
  REQUIRE(subInfomap.root().owner == &module);
  REQUIRE(subInfomap.numLeafNodes() == module.childDegree());
  CHECK(internalEdgesForModule(subInfomap.root()) == originalEdges);
  CHECK(nodeIdentitiesForModule(subInfomap.root()) == originalIdentities);

  subInfomap.run();
  CHECK(std::isfinite(subInfomap.codelength()));
  CHECK(std::isfinite(subInfomap.getIndexCodelength()));
  CHECK(subInfomap.codelength() >= -1e-12);
  CHECK(subInfomap.getIndexCodelength() >= -1e-12);
  CHECK(subInfomap.numLevels() >= 1);
}

TEST_CASE("Higher-order metadata-bearing subnetwork rebuild stays stable [fast][core][lifecycle][subnetwork][parser]")
{
  InfomapWrapper im(infomap::test::defaultFlags("--meta-data-rate 2"));
  infomap::test::readNetworkFixture(im, "states.net");
  im.initMetaData(infomap::test::fixturePath("meta/states.meta"));
  im.run();

  infomap::test::checkRunSanity(im);
  REQUIRE(im.numTopModules() == 2);

  auto& module = *im.root().firstChild;
  REQUIRE(module.childDegree() >= 2);

  const auto originalEdges = internalEdgesForModule(module);
  const auto originalIdentities = nodeIdentitiesForModule(module);
  REQUIRE_FALSE(originalIdentities.empty());
  for (const auto& identity : originalIdentities) {
    CHECK_FALSE(std::get<2>(identity).empty());
  }

  auto runSubnetwork = [&]() -> std::tuple<std::vector<std::vector<unsigned int>>, double, double> {
    auto& subInfomap = im.getSubInfomap(module).initNetwork(module);
    REQUIRE(subInfomap.root().owner == &module);
    REQUIRE(subInfomap.numLeafNodes() == module.childDegree());
    CHECK(internalEdgesForModule(subInfomap.root()) == originalEdges);
    CHECK(nodeIdentitiesForModule(subInfomap.root()) == originalIdentities);

    subInfomap.run();
    CHECK(std::isfinite(subInfomap.codelength()));
    CHECK(std::isfinite(subInfomap.getIndexCodelength()));
    CHECK(subInfomap.codelength() >= -1e-12);
    CHECK(subInfomap.getIndexCodelength() >= -1e-12);
    CHECK(subInfomap.numLevels() >= 1);

    return {canonicalSubInfomapPartition(subInfomap, true), subInfomap.codelength(), subInfomap.getIndexCodelength()};
  };

  const auto firstRun = runSubnetwork();
  const auto secondRun = runSubnetwork();

  CHECK(std::get<0>(secondRun) == std::get<0>(firstRun));
  CHECK(std::get<1>(secondRun) == doctest::Approx(std::get<1>(firstRun)));
  CHECK(std::get<2>(secondRun) == doctest::Approx(std::get<2>(firstRun)));
}

} // namespace
