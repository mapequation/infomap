#include "vendor/doctest.h"

#include "Infomap.h"

#include "TestUtils.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace {

using infomap::InfomapWrapper;

struct FlowRunResult {
  std::vector<std::vector<unsigned int>> partition;
  double codelength = 0.0;
  double indexCodelength = 0.0;
  unsigned int numTopModules = 0;
};

FlowRunResult runDirectedFixture(const std::string& extraFlags)
{
  InfomapWrapper im(infomap::test::defaultFlags("--directed " + extraFlags));
  infomap::test::readNetworkFixture(im, "teleport_directed.net");

  im.run();

  infomap::test::checkRunSanity(im);
  return {
      infomap::test::canonicalPartition(im.getModules()),
      im.codelength(),
      im.getIndexCodelength(),
      im.numTopModules(),
  };
}

TEST_CASE("Recorded teleportation stays stable across trial counts for the directed fixture [fast][core][flow]")
{
  const auto oneTrial = runDirectedFixture("--recorded-teleportation --num-trials 1");
  const auto twoTrials = runDirectedFixture("--recorded-teleportation --num-trials 2");

  CHECK(oneTrial.partition == std::vector<std::vector<unsigned int>>{{1, 2, 3}, {4, 5, 6}});
  CHECK(oneTrial.partition == twoTrials.partition);
  CHECK(oneTrial.codelength == doctest::Approx(twoTrials.codelength));
  CHECK(oneTrial.indexCodelength == doctest::Approx(twoTrials.indexCodelength));
}

TEST_CASE("Directed to-nodes teleportation keeps the expected coarse partition [fast][core][flow]")
{
  const auto result = runDirectedFixture("--to-nodes");

  CHECK(result.numTopModules == 2);
  CHECK(result.partition == std::vector<std::vector<unsigned int>>{{1, 2, 3}, {4, 5, 6}});
}

TEST_CASE("Directed regularization remains numerically sane on the teleportation fixture [fast][core][flow]")
{
  const auto result = runDirectedFixture("--regularized");

  CHECK(result.numTopModules == 1);
  CHECK(result.partition == std::vector<std::vector<unsigned int>>{{1, 2, 3, 4, 5, 6}});
  CHECK(result.codelength > result.indexCodelength);
  infomap::test::checkApproxCodelength(result.codelength, 2.575842872);
}

TEST_CASE("Undirected regularization remains stable on the two-triangles fixture [fast][core][flow]")
{
  InfomapWrapper im(infomap::test::defaultFlags("--regularized"));
  im.readInputData(infomap::test::repoPath("examples/networks/twotriangles.net"));

  im.run();

  infomap::test::checkRunSanity(im);
  CHECK(im.numTopModules() == 1);
  infomap::test::checkCanonicalPartition(im, {{1, 2, 3, 4, 5, 6}});
  infomap::test::checkApproxCodelength(im.codelength(), 2.575767408, 1e-9);
}

TEST_CASE("Directed regularized codelength matches analytic multi-level tree [fast][core][flow]")
{
  InfomapWrapper im(infomap::test::defaultFlags(
      "--directed --regularized --no-infomap --cluster-data "
      + infomap::test::clusterFixturePath("regularized_four_pairs_three_level.tree")));
  im.addLink(1, 2);
  im.addLink(2, 1);
  im.addLink(3, 4);
  im.addLink(4, 3);
  im.addLink(5, 6);
  im.addLink(6, 5);
  im.addLink(7, 8);
  im.addLink(8, 7);
  im.run();

  CHECK(im.numLevels() == 3);
  infomap::test::checkApproxCodelength(im.codelength(), 4.051270346886);
}

TEST_CASE("Regularized multilayer flow supports non-dense matchable state ids [fast][core][flow]")
{
  InfomapWrapper im(infomap::test::defaultFlags(
      "--directed --regularized --matchable-multilayer-ids 2 --no-infomap --two-level"));
  im.addMultilayerIntraLink(1, 10, 20, 1.0);
  im.addMultilayerIntraLink(2, 10, 20, 1.0);
  im.addMultilayerInterLink(1, 10, 2, 1.0);
  im.addMultilayerInterLink(2, 10, 1, 1.0);

  CHECK_NOTHROW(im.run());
  infomap::test::checkRunSanity(im);
}

TEST_CASE("Directed regularized codelength matches analytic multi-level tree [fast][core][flow]")
{
  InfomapWrapper im(infomap::test::defaultFlags(
      "--directed --regularized --no-infomap --cluster-data "
      + infomap::test::clusterFixturePath("regularized_four_pairs_three_level.tree")));
  im.addLink(1, 2);
  im.addLink(2, 1);
  im.addLink(3, 4);
  im.addLink(4, 3);
  im.addLink(5, 6);
  im.addLink(6, 5);
  im.addLink(7, 8);
  im.addLink(8, 7);
  im.run();

  CHECK(im.numLevels() == 3);
  infomap::test::checkApproxCodelength(im.codelength(), 4.051270346886);
}

TEST_CASE("Inner parallelization remains runnable and numerically sane on the directed fixture [fast][core][flow][openmp]")
{
  const auto result = runDirectedFixture("--inner-parallelization");

  CHECK(result.numTopModules >= 1);
  CHECK(result.numTopModules <= 6);
  CHECK(result.partition.size() == result.numTopModules);
  CHECK(result.codelength >= result.indexCodelength);
}

TEST_CASE("Precomputed flow rejects first-order input without vertex flows [fast][core][flow][parser]")
{
  InfomapWrapper im(infomap::test::defaultFlags("--flow-model precomputed"));
  im.readInputData(infomap::test::repoPath("examples/networks/twotriangles.net"));

  CHECK_THROWS_WITH_AS(
      im.run(),
      "Missing node flow in input data. Should be passed as a third field under a *Vertices section.",
      std::runtime_error);
}

TEST_CASE("Precomputed flow rejects higher-order input without state flows [fast][core][flow][parser]")
{
  InfomapWrapper im(infomap::test::defaultFlags("--flow-model precomputed"));
  infomap::test::readNetworkFixture(im, "states.net");

  CHECK_THROWS_WITH_AS(
      im.run(),
      "Missing node flow in input data. Should be passed as a third field under a *States section.",
      std::runtime_error);
}

TEST_CASE("Precomputed flow fixture remains runnable in C++ tests [fast][core][flow]")
{
  InfomapWrapper im(infomap::test::defaultFlags("--flow-model precomputed"));
  infomap::test::readNetworkFixture(im, "twotriangles_flow.net");

  im.run();

  infomap::test::checkRunSanity(im);
  CHECK(im.numTopModules() == 2);
  CHECK(im.numLevels() == 2);
  infomap::test::checkApproxCodelength(im.codelength(), 1.889659695224);
  infomap::test::checkCanonicalPartition(im, {{1, 2, 3}, {4, 5, 6}});
}

} // namespace
