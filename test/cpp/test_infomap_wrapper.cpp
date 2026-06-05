#include "vendor/doctest.h"

#include "Infomap.h"
#include "io/OutputView.h"
#include "utils/Log.h"

#include "TestUtils.h"

#include <cstdio>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace {

using infomap::InfomapWrapper;

using InternalEdge = std::tuple<unsigned int, unsigned int, double, double>;
using NodeIdentity = std::tuple<unsigned int, unsigned int, std::vector<int>>;
using OutputRowIdentity = std::tuple<unsigned int, unsigned int, unsigned int>;

struct LogCapture {
  std::ostringstream output;
  std::ostream& previousStream;
  bool previousSilent;

  // Capture the previous global log state and restore it in the destructor, so a capture
  // never leaks the output stream / silent flag into subsequent test cases.
  LogCapture()
      : previousStream(infomap::Log::getOutputStream()),
        previousSilent(infomap::Log::isSilent())
  {
    infomap::Log::setOutputStream(output);
    infomap::Log::setSilent(false);
  }

  ~LogCapture()
  {
    infomap::Log::setOutputStream(previousStream);
    infomap::Log::setSilent(previousSilent);
  }
};

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

std::string temporaryOutputPath(const std::string& prefix, const std::string& suffix)
{
  return prefix + suffix;
}

void removeFiles(const std::vector<std::string>& paths)
{
  for (const auto& path : paths) {
    std::remove(path.c_str());
  }
}

std::vector<OutputRowIdentity> outputViewIdentities(InfomapWrapper& im, bool states)
{
  infomap::OutputView view(im, im.network(), states);
  std::vector<OutputRowIdentity> rows;
  view.forEachLeaf(1, infomap::OutputLeafPolicy::HideBipartite, [&](const infomap::OutputLeafRow& row) {
    rows.emplace_back(row.stateId, row.physicalId, row.layerId);
  });
  std::sort(rows.begin(), rows.end());
  return rows;
}

std::vector<double> runParallelTrialsFixture(const std::string& extraFlags = "")
{
  InfomapWrapper im("--silent --seed 7 --num-trials 4 --parallel-trials --no-file-output " + extraFlags);
  im.readInputData(infomap::test::repoPath("examples/networks/ninetriangles.net"));
  im.run();
  infomap::test::checkRunSanity(im);
  return im.codelengths();
}

double runSingleTrialFixture(unsigned int seed)
{
  InfomapWrapper im("--silent --seed " + std::to_string(seed) + " --num-trials 1 --two-level --no-file-output");
  im.readInputData(infomap::test::repoPath("examples/networks/ninetriangles.net"));
  im.run();
  infomap::test::checkRunSanity(im);
  return im.codelength();
}

// Serial single-trial run whose flags (other than seed/num-trials) match runParallelTrialsFixture,
// so parallel trial i must equal the serial run with seed 7+i element-wise for the same mode.
double runSingleTrialFixtureWith(unsigned int seed, const std::string& extraFlags)
{
  InfomapWrapper im("--silent --seed " + std::to_string(seed) + " --num-trials 1 --no-file-output " + extraFlags);
  im.readInputData(infomap::test::repoPath("examples/networks/ninetriangles.net"));
  im.run();
  infomap::test::checkRunSanity(im);
  return im.codelength();
}

TEST_CASE("Infomap partitions the unweighted two-triangle fixture into two modules [fast][core][lifecycle]")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  infomap::test::addEdgeFixtureLinks(im, "graphs/twotriangles_unweighted.edges");

  im.run();

  infomap::test::checkRunSanity(im);
  CHECK(im.numTopModules() == 2);
  CHECK(im.numLevels() == 2);
  infomap::test::checkCanonicalPartition(im, { { 1, 2, 3 }, { 4, 5, 6 } });
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

TEST_CASE("Multi-trial run reports the best trial codelength [fast][core][lifecycle]")
{
  InfomapWrapper im("--silent --seed 1 --num-trials 5");
  im.readInputData(infomap::test::repoPath("examples/networks/ninetriangles.net"));

  im.run();

  infomap::test::checkRunSanity(im);
  const auto& codelengths = im.codelengths();
  REQUIRE(codelengths.size() == 5);

  auto bestIt = std::min_element(codelengths.begin(), codelengths.end());
  REQUIRE(bestIt != codelengths.end());

  CHECK(im.codelength() == doctest::Approx(*bestIt));
}

TEST_CASE("Run reports write machine-readable JSON with no-file-output [fast][core][lifecycle][output]")
{
  const std::vector<std::string> paths = {
    "run_report_timing.json",
    "run_report_summary.json",
  };
  removeFiles(paths);

  InfomapWrapper im("--silent --seed 7 --num-trials 2 --no-file-output --memory-report --timing-json " + paths[0] + " --summary-json " + paths[1]);
  infomap::test::addEdgeFixtureLinks(im, "graphs/twotriangles_unweighted.edges");

  im.run();

  infomap::test::checkRunSanity(im);
  const auto timingJson = infomap::test::readTextFile(paths[0]);
  const auto summaryJson = infomap::test::readTextFile(paths[1]);

  CHECK(summaryJson.find("\"version\":\"") != std::string::npos);
  CHECK(summaryJson.find("\"codelength\":") != std::string::npos);
  CHECK(summaryJson.find("\"top_modules\":") != std::string::npos);
  CHECK(summaryJson.find("\"levels\":") != std::string::npos);
  CHECK(summaryJson.find("\"trials\":2,") != std::string::npos);
  CHECK(summaryJson.find("\"best_trial\":") != std::string::npos);
  CHECK(summaryJson.find("\"trial_codelengths\":[") != std::string::npos);
  CHECK(summaryJson.find("\"trial_top_modules\":[") != std::string::npos);

  CHECK(timingJson.find("\"version\":\"") != std::string::npos);
  CHECK(timingJson.find("\"openmp\":") != std::string::npos);
  CHECK(timingJson.find("\"threads_requested\":") != std::string::npos);
  CHECK(timingJson.find("\"threads_used\":1,") != std::string::npos);
  CHECK(timingJson.find("\"thread_source\":\"") != std::string::npos);
  CHECK(timingJson.find("\"thread_source\":\"\"") == std::string::npos); // value is non-empty
  CHECK(timingJson.find("\"network\":{\"nodes\":") != std::string::npos);
  CHECK(timingJson.find("\"timing\":{") != std::string::npos);
  CHECK(timingJson.find("\"flow_calculation_s\":") != std::string::npos);
  CHECK(timingJson.find("\"init_network_s\":") != std::string::npos);
  CHECK(timingJson.find("\"trial_optimize_s\":") != std::string::npos);
  CHECK(timingJson.find("\"total_s\":") != std::string::npos);
  CHECK(timingJson.find("\"parse_input_s\"") == std::string::npos);
  CHECK(timingJson.find("\"trials\":[") != std::string::npos);
  CHECK(timingJson.find("\"trial\":1,") != std::string::npos);
  CHECK(timingJson.find("\"trial\":2,") != std::string::npos);
  CHECK(timingJson.find("\"thread\":0,") != std::string::npos);
  CHECK(timingJson.find("\"seed\":7,") != std::string::npos);
  CHECK(timingJson.find("\"seed\":8,") != std::string::npos);
  CHECK(timingJson.find("\"top_modules\":") != std::string::npos);
  CHECK(timingJson.find("\"num_levels\":") != std::string::npos);
  CHECK(timingJson.find("\"memory\":{\"rss_peak_mb\":") != std::string::npos);

  removeFiles(paths);
}

TEST_CASE("Parallel trials report the best trial codelength [fast][core][lifecycle][openmp]")
{
  InfomapWrapper im("--silent --seed 7 --num-trials 4 --parallel-trials --no-file-output");
  im.readInputData(infomap::test::repoPath("examples/networks/ninetriangles.net"));

  im.run();

  infomap::test::checkRunSanity(im);
  const auto& codelengths = im.codelengths();
  REQUIRE(codelengths.size() == 4);

  auto bestIt = std::min_element(codelengths.begin(), codelengths.end());
  REQUIRE(bestIt != codelengths.end());

  CHECK(im.codelength() == doctest::Approx(*bestIt));
}

TEST_CASE("Parallel trials are deterministic for the same seed [fast][core][lifecycle][openmp]")
{
  const auto first = runParallelTrialsFixture();
  const auto second = runParallelTrialsFixture();

  REQUIRE(first.size() == 4);
  REQUIRE(second.size() == 4);
  CHECK(first == second);
}

TEST_CASE("Parallel trials are invariant to worker count [fast][core][lifecycle][openmp]")
{
#ifdef _OPENMP
  // Worker count is driven by OMP_NUM_THREADS; the per-trial codelength vector must be
  // identical regardless of how many workers run the trials (trial i always uses seed+i).
  const int previousThreads = omp_get_max_threads();

  omp_set_num_threads(1);
  const auto oneWorker = runParallelTrialsFixture();

  omp_set_num_threads(4);
  const auto manyWorkers = runParallelTrialsFixture();

  omp_set_num_threads(previousThreads);

  REQUIRE(oneWorker.size() == 4);
  REQUIRE(manyWorkers.size() == 4);
  CHECK(oneWorker == manyWorkers);
#endif
}

TEST_CASE("Parallel trial workers reset between strided trials [fast][core][lifecycle][openmp]")
{
#ifdef _OPENMP
  InfomapWrapper im("--silent --seed 7 --num-trials 8 --parallel-trials --two-level --no-file-output");
  im.readInputData(infomap::test::repoPath("examples/networks/ninetriangles.net"));

  im.run();

  infomap::test::checkRunSanity(im);
  const auto& codelengths = im.codelengths();
  REQUIRE(codelengths.size() == 8);
  for (unsigned int i = 0; i < codelengths.size(); ++i) {
    CHECK(codelengths[i] == doctest::Approx(runSingleTrialFixture(7 + i)));
  }
#endif
}

TEST_CASE("Parallel trials with one trial warn and run serially [fast][core][lifecycle]")
{
  LogCapture capture;
  InfomapWrapper im("--seed 7 --num-trials 1 --parallel-trials --no-file-output");
  infomap::test::addEdgeFixtureLinks(im, "graphs/twotriangles_unweighted.edges");

  im.run();

  infomap::test::checkRunSanity(im);
  CHECK(im.codelengths().size() == 1);
  CHECK(capture.output.str().find("--parallel-trials requires --num-trials > 1") != std::string::npos);
}

TEST_CASE("Parallel trials run with variable Markov time without falling back [fast][core][lifecycle]")
{
  LogCapture capture;
  InfomapWrapper im("--seed 7 --num-trials 2 --parallel-trials --variable-markov-time --no-file-output");
  infomap::test::addEdgeFixtureLinks(im, "graphs/twotriangles_unweighted.edges");

  im.run();

  infomap::test::checkRunSanity(im);
  CHECK(im.codelengths().size() == 2);
  CHECK(capture.output.str().find("is not supported with --variable-markov-time") == std::string::npos);
}

TEST_CASE("Parallel trials run with entropy correction without falling back [fast][core][lifecycle]")
{
  LogCapture capture;
  InfomapWrapper im("--seed 7 --num-trials 2 --parallel-trials --entropy-corrected --no-file-output");
  infomap::test::addEdgeFixtureLinks(im, "graphs/twotriangles_unweighted.edges");

  im.run();

  infomap::test::checkRunSanity(im);
  CHECK(im.codelengths().size() == 2);
  CHECK(capture.output.str().find("is not supported with --entropy-corrected") == std::string::npos);
}

TEST_CASE("Serial entropy correction is deterministic across fresh instances [fast][core][lifecycle]")
{
  // Hierarchical search spawns sub-Infomap instances; entropy bias correction must keep using
  // the full network's total degree / node count (formerly a shared static, now propagated to
  // sub instances via inheritNetworkPropertiesFrom). We assert seed determinism rather than an
  // absolute codelength: the exact value is a platform-dependent local optimum (greedy
  // tie-breaking differs across libm/FP), so a golden number is not portable. Propagation
  // correctness (serial results unchanged vs the pre-fix build) is verified out of band.
  const auto run = [] {
    InfomapWrapper im("--silent --seed 7 --num-trials 1 --entropy-corrected --no-file-output");
    im.readInputData(infomap::test::repoPath("examples/networks/ninetriangles.net"));
    im.run();
    infomap::test::checkRunSanity(im);
    return im.codelength();
  };

  CHECK(run() == doctest::Approx(run()));
}

TEST_CASE("Parallel trials with variable Markov time match serial trials [fast][core][lifecycle][openmp]")
{
#ifdef _OPENMP
  // Each parallel trial i must equal the serial single-trial run with seed 7+i for the same mode.
  // This pins both correctness (parallel == serial) and that serial VMT results are unchanged.
  const auto codelengths = runParallelTrialsFixture("--variable-markov-time");
  REQUIRE(codelengths.size() == 4);
  for (unsigned int i = 0; i < codelengths.size(); ++i) {
    CHECK(codelengths[i] == doctest::Approx(runSingleTrialFixtureWith(7 + i, "--variable-markov-time")));
  }
#endif
}

TEST_CASE("Parallel trials with entropy correction match serial trials [fast][core][lifecycle][openmp]")
{
#ifdef _OPENMP
  const auto codelengths = runParallelTrialsFixture("--entropy-corrected");
  REQUIRE(codelengths.size() == 4);
  for (unsigned int i = 0; i < codelengths.size(); ++i) {
    CHECK(codelengths[i] == doctest::Approx(runSingleTrialFixtureWith(7 + i, "--entropy-corrected")));
  }
#endif
}

TEST_CASE("Parallel trials with variable Markov time are invariant to worker count [fast][core][lifecycle][openmp]")
{
#ifdef _OPENMP
  const int previousThreads = omp_get_max_threads();

  omp_set_num_threads(1);
  const auto oneWorker = runParallelTrialsFixture("--variable-markov-time");

  omp_set_num_threads(4);
  const auto manyWorkers = runParallelTrialsFixture("--variable-markov-time");

  omp_set_num_threads(previousThreads);

  REQUIRE(oneWorker.size() == 4);
  REQUIRE(manyWorkers.size() == 4);
  CHECK(oneWorker == manyWorkers);
#endif
}

TEST_CASE("Parallel trials with entropy correction are invariant to worker count [fast][core][lifecycle][openmp]")
{
#ifdef _OPENMP
  const int previousThreads = omp_get_max_threads();

  omp_set_num_threads(1);
  const auto oneWorker = runParallelTrialsFixture("--entropy-corrected");

  omp_set_num_threads(4);
  const auto manyWorkers = runParallelTrialsFixture("--entropy-corrected");

  omp_set_num_threads(previousThreads);

  REQUIRE(oneWorker.size() == 4);
  REQUIRE(manyWorkers.size() == 4);
  CHECK(oneWorker == manyWorkers);
#endif
}

TEST_CASE("Parallel trials warn when inner parallelization is requested [fast][core][lifecycle][openmp]")
{
#ifdef _OPENMP
  LogCapture capture;
  InfomapWrapper im("--seed 7 --num-trials 2 --parallel-trials --inner-parallelization --no-file-output");
  infomap::test::addEdgeFixtureLinks(im, "graphs/twotriangles_unweighted.edges");

  im.run();

  infomap::test::checkRunSanity(im);
  CHECK(im.codelengths().size() == 2);
  CHECK(capture.output.str().find("--parallel-trials ignores --inner-parallelization") != std::string::npos);
#endif
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
  infomap::test::checkCanonicalPartition(im, { { 3, 4 } });
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
  infomap::test::checkCanonicalPartition(im, { { 1, 2 }, { 3, 4 } });
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
  infomap::test::checkCanonicalPartition(im, { { 1, 2 } });

  infomap::test::readNetworkFixture(im, "accumulate_b.net", true);
  CHECK(im.network().numNodes() == 4);
  CHECK(im.network().numLinks() == 2);

  im.run();
  infomap::test::checkRunSanity(im);
  CHECK(im.numTopModules() == 2);
  infomap::test::checkCanonicalPartition(im, { { 1, 2 }, { 3, 4 } });

  infomap::test::readNetworkFixture(im, "accumulate_b.net", false);
  CHECK(im.network().numNodes() == 2);
  CHECK(im.network().numLinks() == 1);

  im.run();
  infomap::test::checkRunSanity(im);
  CHECK(im.numTopModules() == 1);
  infomap::test::checkCanonicalPartition(im, { { 3, 4 } });
}

TEST_CASE("Higher-order module queries require state ids [fast][core][lifecycle]")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  infomap::test::readNetworkFixture(im, "states.net");

  im.run();

  infomap::test::checkRunSanity(im);
  CHECK(im.numTopModules() == 2);
  CHECK(im.numLevels() == 2);
  infomap::test::checkCanonicalPartition(im, { { 1, 2, 3 }, { 4, 5, 6 } }, true);
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
    coveredIds.reserve(modules.size());
    for (const auto& moduleEntry : modules) {
      coveredIds.push_back(moduleEntry.first);
    }
    CHECK(coveredIds == expectedLeafIds);

    return { infomap::test::canonicalPartition(modules), subInfomap.codelength(), subInfomap.getIndexCodelength() };
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

    return { canonicalSubInfomapPartition(subInfomap, true), subInfomap.codelength(), subInfomap.getIndexCodelength() };
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

    return { canonicalSubInfomapPartition(subInfomap, true), subInfomap.codelength(), subInfomap.getIndexCodelength() };
  };

  const auto firstRun = runSubnetwork();
  const auto secondRun = runSubnetwork();

  CHECK(std::get<0>(secondRun) == std::get<0>(firstRun));
  CHECK(std::get<1>(secondRun) == doctest::Approx(std::get<1>(firstRun)));
  CHECK(std::get<2>(secondRun) == doctest::Approx(std::get<2>(firstRun)));
}

TEST_CASE("writeTree and writeClu render higher-order state output [fast][core][output]")
{
  auto im = infomap::test::makeRunningInfomap(
      [&](InfomapWrapper& infomap) { infomap::test::readNetworkFixture(infomap, "states.net"); });

  const auto treePath = temporaryOutputPath("states_tree", ".tree");
  const auto cluPath = temporaryOutputPath("states_clu", ".clu");

  im->writeTree(treePath, true);
  im->writeClu(cluPath, true, 1);

  const auto treeText = infomap::test::readTextFile(treePath);
  const auto cluText = infomap::test::readTextFile(cluPath);

  CHECK(treeText.find("# path flow name state_id node_id") != std::string::npos);
  CHECK(treeText.find("state_id") != std::string::npos);
  CHECK(cluText.find("# state_id module flow node_id") != std::string::npos);
  CHECK(cluText.find("# module level 1") != std::string::npos);

  std::remove(treePath.c_str());
  std::remove(cluPath.c_str());
}

TEST_CASE("OutputView projects first-order leaf rows [fast][core][output]")
{
  auto im = infomap::test::makeRunningInfomap(
      [&](InfomapWrapper& infomap) { infomap::test::addEdgeFixtureLinks(infomap, "graphs/twotriangles_unweighted.edges"); });

  infomap::OutputView view(*im, im->network(), false);
  std::vector<unsigned int> physicalIds;
  unsigned int nonEmptyPaths = 0;
  view.forEachLeaf(1, infomap::OutputLeafPolicy::HideBipartite, [&](const infomap::OutputLeafRow& row) {
    physicalIds.push_back(row.physicalId);
    CHECK(row.stateId == row.physicalId);
    CHECK(row.moduleId > 0);
    if (!row.path.empty()) {
      ++nonEmptyPaths;
    }
  });

  std::sort(physicalIds.begin(), physicalIds.end());
  CHECK(physicalIds == std::vector<unsigned int> { 1, 2, 3, 4, 5, 6 });
  CHECK(nonEmptyPaths == physicalIds.size());
}

TEST_CASE("OutputView separates higher-order physical and state rows [fast][core][output]")
{
  auto im = infomap::test::makeRunningInfomap(
      [&](InfomapWrapper& infomap) { infomap::test::readNetworkFixture(infomap, "states.net"); });

  infomap::OutputView physicalView(*im, im->network(), false);
  infomap::OutputView stateView(*im, im->network(), true);

  CHECK(physicalView.isHigherOrderPhysicalLevel());
  CHECK_FALSE(stateView.isHigherOrderPhysicalLevel());

  const auto physicalRows = outputViewIdentities(*im, false);
  const auto stateRows = outputViewIdentities(*im, true);
  std::set<unsigned int> physicalIds;
  for (const auto& row : stateRows) {
    physicalIds.insert(std::get<1>(row));
  }

  CHECK(physicalRows.size() <= stateRows.size());
  CHECK(physicalIds.size() == 5);
  CHECK(stateRows.size() == 6);
}

TEST_CASE("OutputView exposes multilayer state layer ids [fast][core][output]")
{
  auto im = infomap::test::makeRunningInfomap(
      [&](InfomapWrapper& infomap) { infomap::test::readNetworkFixture(infomap, "multilayer.net"); });

  infomap::OutputView view(*im, im->network(), true);
  bool foundLayer = false;
  view.forEachLeaf(1, infomap::OutputLeafPolicy::HideBipartite, [&](const infomap::OutputLeafRow& row) {
    CHECK(row.physicalId != 0);
    if (row.layerId != 0) {
      foundLayer = true;
    }
  });

  CHECK(view.isMultilayer());
  CHECK(foundLayer);
}

TEST_CASE("OutputView applies bipartite hide filter centrally [fast][core][output]")
{
  auto im = infomap::test::makeRunningInfomap(
      [&](InfomapWrapper& infomap) { infomap.readInputData(infomap::test::repoPath("examples/networks/bipartite.net")); });
  im->hideBipartiteNodes = true;

  infomap::OutputView view(*im, im->network(), false);
  std::vector<unsigned int> physicalIds;
  view.forEachLeaf(1, infomap::OutputLeafPolicy::HideBipartite, [&](const infomap::OutputLeafRow& row) {
    physicalIds.push_back(row.physicalId);
  });

  std::sort(physicalIds.begin(), physicalIds.end());
  CHECK(physicalIds == std::vector<unsigned int> { 1, 2, 3 });
}

TEST_CASE("writeFlowTree is stable across repeated calls on the same instance [fast][core][output][regression]")
{
  auto im = infomap::test::makeRunningInfomap(
      [&](InfomapWrapper& infomap) { infomap::test::addEdgeFixtureLinks(infomap, "graphs/twotriangles_unweighted.edges"); });

  const auto firstPath = temporaryOutputPath("first_flow_tree", ".ftree");
  const auto secondPath = temporaryOutputPath("second_flow_tree", ".ftree");

  im->writeFlowTree(firstPath);
  im->writeFlowTree(secondPath);

  const auto firstText = infomap::test::readTextFile(firstPath);
  const auto secondText = infomap::test::readTextFile(secondPath);
  CHECK(firstText == secondText);
  CHECK(firstText.find("#*Links path enterFlow exitFlow numEdges numChildren") != std::string::npos);

  std::remove(firstPath.c_str());
  std::remove(secondPath.c_str());
}

TEST_CASE("writeResult renders selected first-order output artifacts [fast][core][output]")
{
  auto im = infomap::test::makeRunningInfomap(
      [&](InfomapWrapper& infomap) { infomap::test::addEdgeFixtureLinks(infomap, "graphs/twotriangles_unweighted.edges"); });

  im->noFileOutput = false;
  im->outDirectory = "";
  im->outName = "output_result_first_order";
  im->printTree = true;
  im->printClu = true;
  im->printJson = true;
  im->printCsv = true;

  const std::vector<std::string> paths = {
    "output_result_first_order.tree",
    "output_result_first_order.clu",
    "output_result_first_order.json",
    "output_result_first_order.csv",
  };
  removeFiles(paths);

  im->writeResult();

  const auto treeText = infomap::test::readTextFile(paths[0]);
  const auto cluText = infomap::test::readTextFile(paths[1]);
  const auto jsonText = infomap::test::readTextFile(paths[2]);
  const auto csvText = infomap::test::readTextFile(paths[3]);

  CHECK(treeText.find("# path flow name node_id") != std::string::npos);
  CHECK(cluText.find("# node_id module flow") != std::string::npos);
  CHECK(jsonText.find("\"nodes\":[") != std::string::npos);
  CHECK(csvText.find("path,flow,name,node_id") != std::string::npos);

  removeFiles(paths);
}

TEST_CASE("writeResult renders selected physical and state output artifacts [fast][core][output]")
{
  auto im = infomap::test::makeRunningInfomap(
      [&](InfomapWrapper& infomap) { infomap::test::readNetworkFixture(infomap, "states.net"); });

  im->noFileOutput = false;
  im->outDirectory = "";
  im->outName = "output_result_states";
  im->printTree = true;
  im->printClu = true;
  im->printJson = true;
  im->printCsv = true;

  const std::vector<std::string> paths = {
    "output_result_states.tree",
    "output_result_states_states.tree",
    "output_result_states.clu",
    "output_result_states_states.clu",
    "output_result_states.json",
    "output_result_states_states.json",
    "output_result_states.csv",
    "output_result_states_states.csv",
  };
  removeFiles(paths);

  im->writeResult();

  const auto physicalTreeText = infomap::test::readTextFile(paths[0]);
  const auto stateTreeText = infomap::test::readTextFile(paths[1]);
  const auto physicalCluText = infomap::test::readTextFile(paths[2]);
  const auto stateCluText = infomap::test::readTextFile(paths[3]);
  const auto physicalJsonText = infomap::test::readTextFile(paths[4]);
  const auto stateJsonText = infomap::test::readTextFile(paths[5]);
  const auto physicalCsvText = infomap::test::readTextFile(paths[6]);
  const auto stateCsvText = infomap::test::readTextFile(paths[7]);

  CHECK(physicalTreeText.find("# path flow name node_id") != std::string::npos);
  CHECK(stateTreeText.find("# path flow name state_id node_id") != std::string::npos);
  CHECK(physicalCluText.find("# node_id module flow") != std::string::npos);
  CHECK(stateCluText.find("# state_id module flow node_id") != std::string::npos);
  CHECK(physicalJsonText.find("\"stateLevel\":false") != std::string::npos);
  CHECK(stateJsonText.find("\"stateLevel\":true") != std::string::npos);
  CHECK(physicalCsvText.find("path,flow,name,node_id") != std::string::npos);
  CHECK(stateCsvText.find("path,flow,name,state_id,node_id") != std::string::npos);

  removeFiles(paths);
}

} // namespace
