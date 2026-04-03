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

TEST_CASE("Recorded teleportation serial leaf optimization matches pointer and CSR backends [fast][core][flow][lifecycle]")
{
  const auto flags = infomap::test::defaultFlags("--directed --recorded-teleportation");

  InfomapWrapper pointerIm(flags);
  infomap::test::readNetworkFixture(pointerIm, "teleport_directed.net");
  pointerIm.initNetwork(pointerIm.network());
  pointerIm.setActiveNetworkFromLeafs();
  pointerIm.m_csrMaterialization.reset();
  CHECK_FALSE(pointerIm.csrBackend().available());
  pointerIm.initPartition();

  InfomapWrapper csrIm(flags);
  infomap::test::readNetworkFixture(csrIm, "teleport_directed.net");
  csrIm.initNetwork(csrIm.network());
  csrIm.setActiveNetworkFromLeafs();
  REQUIRE(csrIm.csrBackend().available());
  csrIm.initPartition();

  const auto pointerLoops = pointerIm.optimizeActiveNetwork();
  const auto csrLoops = csrIm.optimizeActiveNetwork();

  CHECK(csrLoops == pointerLoops);
  CHECK(infomap::test::canonicalPartition(csrIm.getModules()) == infomap::test::canonicalPartition(pointerIm.getModules()));
  CHECK(csrIm.codelength() == doctest::Approx(pointerIm.codelength()));
  CHECK(csrIm.getIndexCodelength() == doctest::Approx(pointerIm.getIndexCodelength()));
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
}

TEST_CASE("Inner parallelization remains runnable and numerically sane on the directed fixture [fast][core][flow][openmp]")
{
  const auto result = runDirectedFixture("--inner-parallelization");

  CHECK(result.numTopModules >= 1);
  CHECK(result.numTopModules <= 6);
  CHECK(result.partition.size() == result.numTopModules);
  CHECK(result.codelength >= result.indexCodelength);
}

TEST_CASE("Inner parallelization keeps the active graph pointer-backed [fast][core][flow][openmp][lifecycle]")
{
  InfomapWrapper im(infomap::test::defaultFlags("--directed --inner-parallelization"));
  infomap::test::readNetworkFixture(im, "teleport_directed.net");
  im.initNetwork(im.network());
  im.setActiveNetworkFromLeafs();

  CHECK_FALSE(im.csrBackend().available());
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
