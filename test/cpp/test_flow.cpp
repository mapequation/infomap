#include "vendor/doctest.h"

#include "Infomap.h"

#include "TestUtils.h"

#include <cstdio>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace {

using infomap::InfomapWrapper;

struct FlowRunResult {
  std::map<unsigned int, unsigned int> modules;
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

  auto modules = im.getModules();
  infomap::test::checkRunSanity(im);
  return {
      modules,
      infomap::test::canonicalPartition(modules),
      im.codelength(),
      im.getIndexCodelength(),
      im.numTopModules(),
  };
}

void checkInnerParallelPartitionCodelength(const std::string& extraFlags, const std::string& clusterPath)
{
  const auto result = runDirectedFixture("--inner-parallelization " + extraFlags);
  {
    std::ofstream out(clusterPath.c_str());
    out << "# node_id module\n";
    for (const auto& module : result.modules) {
      out << module.first << " " << module.second << "\n";
    }
  }

  InfomapWrapper check(infomap::test::defaultFlags("--directed --no-infomap --cluster-data " + clusterPath + " " + extraFlags));
  infomap::test::readNetworkFixture(check, "teleport_directed.net");
  check.run();
  std::remove(clusterPath.c_str());

  CHECK(check.codelength() == doctest::Approx(result.codelength));
  CHECK(check.getIndexCodelength() == doctest::Approx(result.indexCodelength));
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

TEST_CASE("Inner parallelization codelength matches its emitted partition [fast][core][flow][openmp]")
{
#ifdef _OPENMP
  omp_set_num_threads(8);
  CHECK(omp_get_max_threads() > 1);
#endif

  checkInnerParallelPartitionCodelength("", "inner_parallel_partition_check.clu");
}

TEST_CASE("Inner parallelization codelength matches recorded-teleportation partition [fast][core][flow][openmp]")
{
#ifdef _OPENMP
  omp_set_num_threads(8);
  CHECK(omp_get_max_threads() > 1);
#endif

  checkInnerParallelPartitionCodelength("--recorded-teleportation", "inner_parallel_recorded_partition_check.clu");
}

TEST_CASE("Inner parallelization is deterministic for a fixed seed and thread count [fast][core][flow][openmp]")
{
#ifdef _OPENMP
  omp_set_num_threads(8);
  CHECK(omp_get_max_threads() > 1);
#endif

  const auto first = runDirectedFixture("--inner-parallelization");
  const auto second = runDirectedFixture("--inner-parallelization");

  CHECK(first.partition == second.partition);
  CHECK(first.codelength == doctest::Approx(second.codelength));
  CHECK(first.indexCodelength == doctest::Approx(second.indexCodelength));
}

TEST_CASE("Inner parallelization with meta data falls back to stable serial optimization [fast][core][flow][openmp]")
{
#ifdef _OPENMP
  omp_set_num_threads(8);
  CHECK(omp_get_max_threads() > 1);
#endif

  InfomapWrapper im(infomap::test::defaultFlags(
      "--inner-parallelization --meta-data " + infomap::test::fixturePath("meta/states.meta") + " --meta-data-rate 2"));
  infomap::test::readNetworkFixture(im, "states.net");

  im.run();

  infomap::test::checkRunSanity(im);
  CHECK(im.codelength() > 0.0);
  CHECK(im.codelength() >= im.getIndexCodelength());
  CHECK(im.getMetaCodelength() >= 0.0);
  for (auto* leaf : im.leafNodes()) {
    CHECK_FALSE(leaf->metaData.empty());
    CHECK(leaf->metaData[0] != -1);
  }
}

TEST_CASE("Inner parallelization with memory input falls back to stable serial optimization [fast][core][flow][openmp]")
{
#ifdef _OPENMP
  omp_set_num_threads(8);
  CHECK(omp_get_max_threads() > 1);
#endif

  InfomapWrapper im(infomap::test::defaultFlags("--inner-parallelization"));
  infomap::test::readNetworkFixture(im, "states.net");

  im.run();

  infomap::test::checkRunSanity(im);
  CHECK(im.codelength() > 0.0);
  CHECK(im.codelength() >= im.getIndexCodelength());
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
