#include "vendor/doctest.h"

#include "Infomap.h"

#include "TestUtils.h"

#include <vector>

namespace {

using infomap::InfomapWrapper;

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

} // namespace
