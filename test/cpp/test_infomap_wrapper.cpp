#include "vendor/doctest.h"

#include "Infomap.h"

#include "TestUtils.h"

#include <stdexcept>

namespace {

using infomap::InfomapWrapper;

TEST_CASE("Infomap partitions the unweighted two-triangle fixture into two modules [fast][core]")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  infomap::test::addEdgeFixtureLinks(im, "graphs/twotriangles_unweighted.edges");

  im.run();

  CHECK(im.numTopModules() == 2);
  CHECK(im.numLevels() == 2);
  CHECK(infomap::test::canonicalPartition(im.getModules()) == std::vector<std::vector<unsigned int>>{{1, 2, 3}, {4, 5, 6}});
}

TEST_CASE("Infomap partitions the weighted two-triangle fixture into two modules [fast][core]")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  infomap::test::addEdgeFixtureLinks(im, "graphs/twotriangles_weighted.edges");

  im.run();

  CHECK(im.numTopModules() == 2);
  CHECK(im.numLevels() == 2);
  CHECK(infomap::test::canonicalPartition(im.getModules()) == std::vector<std::vector<unsigned int>>{{1, 2, 3}, {4, 5, 6}});
}

TEST_CASE("Recorded teleportation stays stable across trial counts for the directed fixture [fast][core]")
{
  auto runFixture = [](unsigned int numTrials, std::vector<std::vector<unsigned int>>& partition, double& codelength, double& indexCodelength) {
    InfomapWrapper im(infomap::test::defaultFlags("--directed --recorded-teleportation --num-trials " + std::to_string(numTrials)));
    infomap::test::addEdgeFixtureLinks(im, "graphs/recorded_teleportation_directed.edges");
    im.run();
    partition = infomap::test::canonicalPartition(im.getModules());
    codelength = im.codelength();
    indexCodelength = im.getIndexCodelength();
  };

  std::vector<std::vector<unsigned int>> oneTrialPartition;
  std::vector<std::vector<unsigned int>> twoTrialPartition;
  double oneTrialCodelength = 0.0;
  double twoTrialCodelength = 0.0;
  double oneTrialIndexCodelength = 0.0;
  double twoTrialIndexCodelength = 0.0;

  runFixture(1, oneTrialPartition, oneTrialCodelength, oneTrialIndexCodelength);
  runFixture(2, twoTrialPartition, twoTrialCodelength, twoTrialIndexCodelength);

  CHECK(oneTrialPartition == std::vector<std::vector<unsigned int>>{{1, 2, 3}, {4, 5, 6}});
  CHECK(oneTrialPartition == twoTrialPartition);
  CHECK(oneTrialCodelength == doctest::Approx(twoTrialCodelength));
  CHECK(oneTrialIndexCodelength == doctest::Approx(twoTrialIndexCodelength));
}

TEST_CASE("Precomputed flow rejects first-order input without vertex flows [fast][core]")
{
  InfomapWrapper im(infomap::test::defaultFlags("--flow-model precomputed"));
  im.readInputData(infomap::test::repoPath("examples/networks/twotriangles.net"));

  CHECK_THROWS_WITH_AS(
      im.run(),
      "Missing node flow in input data. Should be passed as a third field under a *Vertices section.",
      std::runtime_error);
}

TEST_CASE("Precomputed flow rejects higher-order input without state flows [fast][core]")
{
  InfomapWrapper im(infomap::test::defaultFlags("--flow-model precomputed"));
  im.readInputData(infomap::test::repoPath("examples/networks/states.net"));

  CHECK_THROWS_WITH_AS(
      im.run(),
      "Missing node flow in input data. Should be passed as a third field under a *States section.",
      std::runtime_error);
}

TEST_CASE("Precomputed flow fixture remains runnable in C++ tests [fast][core]")
{
  InfomapWrapper im(infomap::test::defaultFlags("--flow-model precomputed"));
  im.readInputData(infomap::test::repoPath("test/fixtures/networks/twotriangles_flow.net"));

  im.run();

  CHECK(im.numTopModules() == 2);
  CHECK(im.numLevels() == 2);
  CHECK(im.codelength() == doctest::Approx(1.889659695224));
  CHECK(infomap::test::canonicalPartition(im.getModules()) == std::vector<std::vector<unsigned int>>{{1, 2, 3}, {4, 5, 6}});
}

TEST_CASE("Higher-order module queries require state ids [fast][core]")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  im.readInputData(infomap::test::repoPath("test/fixtures/networks/states.net"));

  im.run();

  CHECK(im.numTopModules() == 2);
  CHECK(im.numLevels() == 2);
  CHECK(infomap::test::canonicalPartition(im.getModules(1, true)) == std::vector<std::vector<unsigned int>>{{1, 2, 3}, {4, 5, 6}});
  CHECK_THROWS_WITH_AS(im.getModules(false), "Cannot get modules on higher-order network without states.", std::runtime_error);
}

TEST_CASE("Multilayer links can be clustered without crashing [fast][crash]")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  im.addMultilayerLink(2, 1, 1, 2, 1.0);
  im.addMultilayerLink(1, 2, 2, 1, 1.0);

  im.run();

  CHECK(im.network().haveMemoryInput());
  CHECK(im.network().numNodes() == 2);
  CHECK(im.numTopModules() == 1);
  CHECK(im.numLevels() == 2);
}

} // namespace
