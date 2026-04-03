#include "vendor/doctest.h"

#include "Infomap.h"

#include "TestUtils.h"

#include <stdexcept>

namespace {

using infomap::InfomapWrapper;

TEST_CASE("Infomap partitions the two-triangle graph into two modules")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  im.addLink(1, 2);
  im.addLink(2, 3);
  im.addLink(3, 1);
  im.addLink(3, 4);
  im.addLink(4, 5);
  im.addLink(5, 6);
  im.addLink(6, 4);

  im.run();

  CHECK(im.numTopModules() == 2);
  CHECK(im.numLevels() == 2);
  CHECK(infomap::test::canonicalPartition(im.getModules()) == std::vector<std::vector<unsigned int>>{{1, 2, 3}, {4, 5, 6}});
}

TEST_CASE("Precomputed flow rejects first-order input without vertex flows")
{
  InfomapWrapper im(infomap::test::defaultFlags("--flow-model precomputed"));
  im.readInputData(infomap::test::repoPath("examples/networks/twotriangles.net"));

  CHECK_THROWS_WITH_AS(
      im.run(),
      "Missing node flow in input data. Should be passed as a third field under a *Vertices section.",
      std::runtime_error);
}

TEST_CASE("Precomputed flow rejects higher-order input without state flows")
{
  InfomapWrapper im(infomap::test::defaultFlags("--flow-model precomputed"));
  im.readInputData(infomap::test::repoPath("examples/networks/states.net"));

  CHECK_THROWS_WITH_AS(
      im.run(),
      "Missing node flow in input data. Should be passed as a third field under a *States section.",
      std::runtime_error);
}

TEST_CASE("Precomputed flow fixture remains runnable in C++ tests")
{
  InfomapWrapper im(infomap::test::defaultFlags("--flow-model precomputed"));
  im.readInputData(infomap::test::repoPath("test/fixtures/twotriangles_flow.net"));

  im.run();

  CHECK(im.numTopModules() == 2);
  CHECK(im.numLevels() == 2);
  CHECK(im.codelength() == doctest::Approx(1.889659695224));
  CHECK(infomap::test::canonicalPartition(im.getModules()) == std::vector<std::vector<unsigned int>>{{1, 2, 3}, {4, 5, 6}});
}

TEST_CASE("Higher-order module queries require state ids")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  im.readInputData(infomap::test::repoPath("examples/networks/states.net"));

  im.run();

  CHECK(im.numTopModules() == 2);
  CHECK(im.numLevels() == 2);
  CHECK(infomap::test::canonicalPartition(im.getModules(1, true)) == std::vector<std::vector<unsigned int>>{{1, 2, 3}, {4, 5, 6}});
  CHECK_THROWS_WITH_AS(im.getModules(false), "Cannot get modules on higher-order network without states.", std::runtime_error);
}

TEST_CASE("Multilayer links can be clustered without crashing")
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
