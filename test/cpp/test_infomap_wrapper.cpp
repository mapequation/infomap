#include "vendor/doctest.h"

#include "Infomap.h"
#include "io/OutputView.h"
#include "utils/Log.h"

#include "TestUtils.h"

#include <atomic>
#include <cstdio>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
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
    identities.emplace_back(node.stateId, node.physicalId, node.metaData());
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
  InfomapWrapper im(infomap::test::withTestEngine("--silent --seed 7 --num-trials 4 --parallel-trials --no-file-output " + extraFlags));
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
  InfomapWrapper im(infomap::test::withTestEngine("--silent --seed " + std::to_string(seed) + " --num-trials 1 --no-file-output " + extraFlags));
  im.readInputData(infomap::test::repoPath("examples/networks/ninetriangles.net"));
  im.run();
  infomap::test::checkRunSanity(im);
  return im.codelength();
}

// Interrupt-handler callbacks (signature `bool(*)(void*)`): return true to
// request cooperative cancellation of the current run.
bool alwaysInterrupt(void*) { return true; }
bool neverInterrupt(void*) { return false; }

// Counts how many times the owner-thread checkpoint polled the handler.
bool countPolls(void* data)
{
  ++*static_cast<int*>(data);
  return false;
}

// Returns false for the first *(int*)data polls, then true — lets the run get
// past the entry checkpoint so a later poll point is exercised.
bool interruptAfter(void* data)
{
  auto* remaining = static_cast<int*>(data);
  if (*remaining <= 0)
    return true;
  --*remaining;
  return false;
}

// `clusters` weakly-linked 4-cliques — big enough to build a hierarchy and spawn
// OpenMP tasks, so the in-task cancellation paths are actually exercised.
void buildClusteredNetwork(InfomapWrapper& im, unsigned int clusters)
{
  for (unsigned int c = 0; c < clusters; ++c) {
    const unsigned int b = c * 4 + 1;
    im.addLink(b, b + 1);
    im.addLink(b, b + 2);
    im.addLink(b, b + 3);
    im.addLink(b + 1, b + 2);
    im.addLink(b + 1, b + 3);
    im.addLink(b + 2, b + 3);
    if (c > 0)
      im.addLink(b, b - 4);
  }
}

TEST_CASE("Run throws InterruptionError when the handler requests cancellation [fast][core][lifecycle][crash][columnar-contract]")
{
  InfomapWrapper im(infomap::test::withTestEngine("--silent --seed 1 --num-trials 1 --no-file-output"));
  im.readInputData(infomap::test::repoPath("examples/networks/ninetriangles.net"));
  im.setInterruptHandler(&alwaysInterrupt);

  CHECK_THROWS_AS(im.run(), infomap::InterruptionError);
}

TEST_CASE("The in-optimizer cancellation checkpoint is reached during a run [fast][core][lifecycle][columnar-contract]")
{
  InfomapWrapper im(infomap::test::withTestEngine("--silent --seed 1 --num-trials 1 --no-file-output"));
  im.readInputData(infomap::test::repoPath("examples/networks/ninetriangles.net"));
  int polls = 0;
  im.setInterruptHandler(&countPolls, &polls);

  im.run();

  infomap::test::checkRunSanity(im);
  // A single-trial run has exactly two non-optimizer checkpoints (run() entry +
  // per-trial loop); > 2 proves the in-optimizer poll fires. Guards against
  // dropping it (count would fall to 2, leaving mid-run cancellation untested).
  CHECK(polls > 2);
}

TEST_CASE("A non-cancelling handler produces the same result as a no-handler run [fast][core][lifecycle][columnar-contract]")
{
  // Reference: no handler installed at all.
  InfomapWrapper ref(infomap::test::withTestEngine("--silent --seed 1 --num-trials 1 --no-file-output"));
  ref.readInputData(infomap::test::repoPath("examples/networks/ninetriangles.net"));
  ref.run();

  InfomapWrapper im(infomap::test::withTestEngine("--silent --seed 1 --num-trials 1 --no-file-output"));
  im.readInputData(infomap::test::repoPath("examples/networks/ninetriangles.net"));
  im.setInterruptHandler(&neverInterrupt);
  im.run();

  infomap::test::checkRunSanity(im);
  // Installing a (non-cancelling) handler must not change the objective or the
  // partition — the poll points are inert on the no-cancel path (F5).
  CHECK(infomap::test::canonicalPartition(im.getModules()) == infomap::test::canonicalPartition(ref.getModules()));
  CHECK(im.codelength() == doctest::Approx(ref.codelength()));
}

TEST_CASE("requestInterrupt() aborts the run even when the handler returns false [fast][core][lifecycle][crash][columnar-contract]")
{
  InfomapWrapper im(infomap::test::withTestEngine("--silent --seed 1 --num-trials 1 --no-file-output"));
  im.readInputData(infomap::test::repoPath("examples/networks/ninetriangles.net"));
  // The handler never returns true; it sets the cancel flag directly, proving
  // the thread-safe flag path is observed independently of the callback return.
  im.setInterruptHandler(
      [](void* data) {
        static_cast<InfomapWrapper*>(data)->requestInterrupt();
        return false;
      },
      &im);

  CHECK_THROWS_AS(im.run(), infomap::InterruptionError);
}

// Cancel inside optimization, clear the handler, rerun, return the result.
// We do NOT compare against a *fresh* instance: the RNG is seeded once at
// construction (not per run()), so a reused instance legitimately differs from a
// fresh one here — that is RNG continuation, not corruption.
std::pair<std::vector<std::vector<unsigned int>>, double> interruptThenRecover()
{
  InfomapWrapper im(infomap::test::withTestEngine("--silent --seed 1 --num-trials 1 --no-file-output"));
  im.readInputData(infomap::test::repoPath("examples/networks/ninetriangles.net"));
  int polls = 3; // cancel inside the optimization phase
  im.setInterruptHandler(&interruptAfter, &polls);
  CHECK_THROWS_AS(im.run(), infomap::InterruptionError);

  im.clearInterruptHandler();
  im.run();
  infomap::test::checkRunSanity(im);
  return { infomap::test::canonicalPartition(im.getModules()), im.codelength() };
}

TEST_CASE("An interrupted instance recovers to a sane, deterministic result [fast][core][lifecycle][crash][columnar-contract]")
{
  // Identical interrupt+clear+rerun on two instances must give identical results:
  // no corrupt/nondeterministic state left behind, flag reset (F4).
  const auto a = interruptThenRecover();
  const auto b = interruptThenRecover();

  CHECK(a.first == b.first);
  CHECK(a.second == doctest::Approx(b.second));
  CHECK(a.first.size() > 1);
}

// Cancelling from another thread mid-run must never let an exception escape an
// OpenMP region (std::terminate), and must surface as InterruptionError, not a
// "Parallel trial failed" / wrong-type error (F3). Outcome-robust: completing
// (canceller lost the race) is fine too; only terminate or wrong-type fail.
TEST_CASE("Cancelling a --parallel-trials run never terminates and surfaces InterruptionError [fast][core][lifecycle][crash]")
{
  InfomapWrapper im("--silent --seed 1 --num-trials 200 --parallel-trials --no-file-output");
  buildClusteredNetwork(im, 800);

  std::atomic<bool> started { false };
  std::atomic<bool> done { false };
  // The handler only signals that the run is underway (never cancels), so the
  // canceller waits until then before flipping the shared flag — maximising the
  // chance the flag is set while the parallel-trial workers are running.
  im.setInterruptHandler([](void* d) { static_cast<std::atomic<bool>*>(d)->store(true); return false; }, &started);
  std::thread canceller([&] {
    while (!started.load() && !done.load())
      std::this_thread::yield();
    while (!done.load()) {
      im.requestInterrupt();
      std::this_thread::yield();
    }
  });

  std::string outcome;
  try {
    im.run();
    outcome = "completed";
  } catch (const infomap::InterruptionError&) {
    outcome = "interrupted";
  } catch (const std::exception& e) {
    outcome = std::string("wrong-type: ") + e.what();
  }
  done.store(true);
  canceller.join();

  // Completing (the canceller lost the race) or InterruptionError are both fine;
  // a wrong-type exception fails, and a std::terminate would crash before here.
  INFO("run outcome: " << outcome);
  CHECK(outcome.rfind("wrong-type:", 0) != 0);
}

TEST_CASE("Cancelling a recursive run from another thread never terminates [fast][core][lifecycle][crash]")
{
  InfomapWrapper im("--silent --seed 1 --num-trials 1 --no-file-output");
  buildClusteredNetwork(im, 1200);

  std::atomic<bool> started { false };
  std::atomic<bool> done { false };
  im.setInterruptHandler([](void* d) { static_cast<std::atomic<bool>*>(d)->store(true); return false; }, &started);
  std::thread canceller([&] {
    while (!started.load() && !done.load())
      std::this_thread::yield();
    while (!done.load()) {
      im.requestInterrupt();
      std::this_thread::yield();
    }
  });

  std::string outcome;
  try {
    im.run();
    outcome = "completed";
  } catch (const infomap::InterruptionError&) {
    outcome = "interrupted";
  } catch (const std::exception& e) {
    outcome = std::string("wrong-type: ") + e.what();
  }
  done.store(true);
  canceller.join();

  // Completing (the canceller lost the race) or InterruptionError are both fine;
  // a wrong-type exception fails, and a std::terminate would crash before here.
  INFO("run outcome: " << outcome);
  CHECK(outcome.rfind("wrong-type:", 0) != 0);
}

TEST_CASE("Infomap partitions the unweighted two-triangle fixture into two modules [fast][core][lifecycle][columnar-contract]")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  infomap::test::addEdgeFixtureLinks(im, "graphs/twotriangles_unweighted.edges");

  im.run();

  infomap::test::checkRunSanity(im);
  CHECK(im.numTopModules() == 2);
  CHECK(im.numLevels() == 2);
  infomap::test::checkCanonicalPartition(im, { { 1, 2, 3 }, { 4, 5, 6 } });
}

TEST_CASE("Infomap can rerun the same multi-trial instance safely [fast][core][lifecycle][crash][columnar-contract]")
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

// A copy-constructible, seedable UniformRandomBitGenerator (a small LCG) that
// records every seed value it receives and counts every draw — stands in for a
// host RNG (igraph etc.) to exercise the pluggable-engine seam (issue #411).
struct TrackingEngine {
  using result_type = infomap::RandGen::result_type;

  struct Stats {
    mutable std::mutex mutex;
    std::vector<unsigned int> seedValues;
    std::atomic<unsigned long> drawCount { 0 };

    void recordSeed(unsigned int seedValue)
    {
      std::lock_guard<std::mutex> lock(mutex);
      seedValues.push_back(seedValue);
    }

    std::vector<unsigned int> snapshot() const
    {
      std::lock_guard<std::mutex> lock(mutex);
      return seedValues;
    }
  };

  std::shared_ptr<Stats> stats;
  result_type state = 0;

  explicit TrackingEngine(std::shared_ptr<Stats> sharedStats) : stats(std::move(sharedStats)) {}

  void seed(unsigned int seedValue)
  {
    state = seedValue;
    stats->recordSeed(seedValue);
  }

  static constexpr result_type min() { return 0; }
  static constexpr result_type max() { return std::numeric_limits<result_type>::max(); }

  result_type operator()()
  {
    ++stats->drawCount;
    state = state * 1664525u + 1013904223u;
    return state;
  }
};

TEST_CASE("A custom RNG engine drives the run and reseeding stays deterministic [fast][core][lifecycle][rng]")
{
  auto stats = std::make_shared<TrackingEngine::Stats>();
  InfomapWrapper im(infomap::test::defaultFlags());
  im.setRandomEngine(TrackingEngine(stats));
  im.readInputData(infomap::test::repoPath("examples/networks/ninetriangles.net"));

  im.run();
  infomap::test::checkRunSanity(im);

  // A positive draw count proves the injected engine supplies the draws
  // themselves — a regression where only seed() is forwarded (draws falling
  // back to the default mt19937) would still pass the determinism checks below.
  const auto drawsAfterFirstRun = stats->drawCount.load();
  CHECK(drawsAfterFirstRun > 0);

  const auto firstPartition = infomap::test::canonicalPartition(im.getModules());
  const auto firstCodelength = im.codelength();

  const auto seedsBeforeReseed = stats->snapshot().size();
  im.reseed(123);
  // reseed() forwards exactly one seed() call to the injected engine.
  REQUIRE(stats->snapshot().size() == seedsBeforeReseed + 1);
  im.run();
  infomap::test::checkRunSanity(im);

  // Same injected engine + same seed -> identical result; the seam reseeds the
  // injected engine (not the default mt19937).
  const auto seedValues = stats->snapshot();
  CHECK(infomap::test::canonicalPartition(im.getModules()) == firstPartition);
  CHECK(im.codelength() == doctest::Approx(firstCodelength));
  REQUIRE(seedValues.size() >= 2);
  CHECK(seedValues[0] == 123u);
  CHECK(seedValues[1] == 123u);
  CHECK(stats->drawCount.load() > drawsAfterFirstRun);
}

TEST_CASE("setConfig preserves the injected RNG engine and applies the new seed [fast][core][lifecycle][rng]")
{
  auto stats = std::make_shared<TrackingEngine::Stats>();
  InfomapWrapper im(infomap::test::defaultFlags());
  im.setRandomEngine(TrackingEngine(stats));

  infomap::Config conf(im.getConfig());
  conf.seedToRandomNumberGenerator = 321;
  im.setConfig(conf);

  // The engine survives setConfig (would be dropped by a plain `*this = conf`)
  // and is reseeded with the new seed.
  const auto seedValues = stats->snapshot();
  REQUIRE(seedValues.size() == 2);
  CHECK(seedValues[0] == 123u);
  CHECK(seedValues[1] == 321u);

  infomap::test::addEdgeFixtureLinks(im, "graphs/twotriangles_unweighted.edges");
  im.run();
  infomap::test::checkRunSanity(im);
}

TEST_CASE("Sub-Infomap creation inherits the injected RNG engine [fast][core][lifecycle][subnetwork][rng]")
{
  auto stats = std::make_shared<TrackingEngine::Stats>();
  InfomapWrapper im(infomap::test::defaultFlags());
  im.setRandomEngine(TrackingEngine(stats));
  im.readInputData(infomap::test::repoPath("examples/networks/twotriangles.net"));
  im.run();

  infomap::test::checkRunSanity(im);
  REQUIRE(im.numTopModules() == 2);

  const auto seedsBeforeSub = stats->snapshot().size();

  auto& module = *im.root().firstChild;
  auto& subInfomap = im.getSubInfomap(module).initNetwork(module);

  // Inheritance is proven by the extra recorded seed: a fresh default-engine
  // sub-Infomap would record none. Sub-Infomaps seed from the config seed.
  const auto seedValues = stats->snapshot();
  REQUIRE(seedValues.size() == seedsBeforeSub + 1);
  CHECK(seedValues.back() == im.getConfig().seedToRandomNumberGenerator);

  subInfomap.run();
  CHECK(std::isfinite(subInfomap.codelength()));
  CHECK(subInfomap.codelength() >= -1e-12);
}

// An igraph-shaped engine: only seed() + randInt(min, max), no
// UniformRandomBitGenerator surface at all. That this type compiles at the
// setRandomEngine() call site proves bounded-draw engines never have to supply
// raw bits — the host's own bounded mapping (igraph_rng_get_integer et al.) is
// used as-is, so results stay platform-stable for the host. (#411)
struct BoundedTrackingEngine {
  std::shared_ptr<TrackingEngine::Stats> stats;
  unsigned int state = 0;

  explicit BoundedTrackingEngine(std::shared_ptr<TrackingEngine::Stats> sharedStats) : stats(std::move(sharedStats)) {}

  void seed(unsigned int seedValue)
  {
    state = seedValue;
    stats->recordSeed(seedValue);
  }

  unsigned int randInt(unsigned int min, unsigned int max)
  {
    ++stats->drawCount;
    state = state * 1664525u + 1013904223u;
    return min + state % (max - min + 1u);
  }
};

TEST_CASE("A bounded-draw engine without a bit generator drives the run [fast][core][lifecycle][rng]")
{
  auto stats = std::make_shared<TrackingEngine::Stats>();
  InfomapWrapper im(infomap::test::defaultFlags());
  im.setRandomEngine(BoundedTrackingEngine(stats));
  im.readInputData(infomap::test::repoPath("examples/networks/ninetriangles.net"));

  im.run();
  infomap::test::checkRunSanity(im);

  const auto drawsAfterFirstRun = stats->drawCount.load();
  CHECK(drawsAfterFirstRun > 0);

  const auto firstPartition = infomap::test::canonicalPartition(im.getModules());
  const auto firstCodelength = im.codelength();

  im.reseed(123);
  im.run();
  infomap::test::checkRunSanity(im);

  // Same bounded engine + same seed -> identical result; every draw went
  // through the engine's own bounded mapping.
  CHECK(infomap::test::canonicalPartition(im.getModules()) == firstPartition);
  CHECK(im.codelength() == doctest::Approx(firstCodelength));
  CHECK(stats->drawCount.load() > drawsAfterFirstRun);
}

TEST_CASE("Multi-trial run reports the best trial codelength [fast][core][lifecycle][columnar-contract]")
{
  InfomapWrapper im(infomap::test::withTestEngine("--silent --seed 1 --num-trials 5"));
  im.readInputData(infomap::test::repoPath("examples/networks/ninetriangles.net"));

  im.run();

  infomap::test::checkRunSanity(im);
  const auto& codelengths = im.codelengths();
  REQUIRE(codelengths.size() == 5);

  auto bestIt = std::min_element(codelengths.begin(), codelengths.end());
  REQUIRE(bestIt != codelengths.end());

  CHECK(im.codelength() == doctest::Approx(*bestIt));
}

TEST_CASE("Converge stops trials on a codelength plateau within the cap [fast][core][lifecycle][columnar-contract]")
{
  const unsigned int cap = 30;
  InfomapWrapper im(infomap::test::withTestEngine("--silent --seed 1 --converge --num-trials " + std::to_string(cap)));
  im.readInputData(infomap::test::repoPath("examples/networks/ninetriangles.net"));

  im.run();

  infomap::test::checkRunSanity(im);
  const auto& codelengths = im.codelengths();
  // Ran at least the floor, never exceeded the cap, and the best is reported.
  // Local copy avoids odr-use of the constexpr member (C++14: no out-of-line def).
  const unsigned int minTrials = infomap::Config::convergeMinTrials;
  CHECK(codelengths.size() >= minTrials);
  CHECK(codelengths.size() <= cap);

  auto bestIt = std::min_element(codelengths.begin(), codelengths.end());
  REQUIRE(bestIt != codelengths.end());
  CHECK(im.codelength() == doctest::Approx(*bestIt));
}

TEST_CASE("Converge is deterministic for a given input and seed [fast][core][lifecycle][columnar-contract]")
{
  auto runOnce = [] {
    InfomapWrapper im(infomap::test::withTestEngine("--silent --seed 1 --converge --num-trials 30"));
    im.readInputData(infomap::test::repoPath("examples/networks/ninetriangles.net"));
    im.run();
    return std::make_pair(im.codelengths().size(), im.codelength());
  };

  const auto first = runOnce();
  const auto second = runOnce();
  CHECK(first.first == second.first); // same trial count
  CHECK(first.second == doctest::Approx(second.second)); // same best codelength
}

TEST_CASE("Run reports write machine-readable JSON with no-file-output [fast][core][lifecycle][output][columnar-contract]")
{
  const std::vector<std::string> paths = {
    "run_report_timing.json",
    "run_report_summary.json",
  };
  removeFiles(paths);

  InfomapWrapper im(infomap::test::withTestEngine("--silent --seed 7 --num-trials 2 --no-file-output --memory-report --timing-json " + paths[0] + " --summary-json " + paths[1]));
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

TEST_CASE("Parallel trials report the best trial codelength [fast][core][lifecycle][openmp][columnar-contract]")
{
  InfomapWrapper im(infomap::test::withTestEngine("--silent --seed 7 --num-trials 4 --parallel-trials --no-file-output"));
  im.readInputData(infomap::test::repoPath("examples/networks/ninetriangles.net"));

  im.run();

  infomap::test::checkRunSanity(im);
  const auto& codelengths = im.codelengths();
  REQUIRE(codelengths.size() == 4);

  auto bestIt = std::min_element(codelengths.begin(), codelengths.end());
  REQUIRE(bestIt != codelengths.end());

  CHECK(im.codelength() == doctest::Approx(*bestIt));
}

TEST_CASE("Parallel trials are deterministic for the same seed [fast][core][lifecycle][openmp][columnar-contract]")
{
  const auto first = runParallelTrialsFixture();
  const auto second = runParallelTrialsFixture();

  REQUIRE(first.size() == 4);
  REQUIRE(second.size() == 4);
  CHECK(first == second);
}

TEST_CASE("Parallel trials are invariant to worker count [fast][core][lifecycle][openmp][columnar-contract]")
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

TEST_CASE("Parallel trials with one trial warn and run serially [fast][core][lifecycle][columnar-contract]")
{
  LogCapture capture;
  InfomapWrapper im(infomap::test::withTestEngine("--seed 7 --num-trials 1 --parallel-trials --no-file-output"));
  infomap::test::addEdgeFixtureLinks(im, "graphs/twotriangles_unweighted.edges");

  im.run();

  infomap::test::checkRunSanity(im);
  CHECK(im.codelengths().size() == 1);
  CHECK(capture.output.str().find("--parallel-trials requires --num-trials > 1") != std::string::npos);
}

TEST_CASE("Parallel trials run with variable Markov time without falling back [fast][core][lifecycle][columnar-contract]")
{
  LogCapture capture;
  InfomapWrapper im(infomap::test::withTestEngine("--seed 7 --num-trials 2 --parallel-trials --variable-markov-time --no-file-output"));
  infomap::test::addEdgeFixtureLinks(im, "graphs/twotriangles_unweighted.edges");

  im.run();

  infomap::test::checkRunSanity(im);
  CHECK(im.codelengths().size() == 2);
  CHECK(capture.output.str().find("is not supported with --variable-markov-time") == std::string::npos);
}

TEST_CASE("Parallel trials run with entropy correction without falling back [fast][core][lifecycle][columnar-contract]")
{
  LogCapture capture;
  InfomapWrapper im(infomap::test::withTestEngine("--seed 7 --num-trials 2 --parallel-trials --entropy-corrected --no-file-output"));
  infomap::test::addEdgeFixtureLinks(im, "graphs/twotriangles_unweighted.edges");

  im.run();

  infomap::test::checkRunSanity(im);
  CHECK(im.codelengths().size() == 2);
  CHECK(capture.output.str().find("is not supported with --entropy-corrected") == std::string::npos);
}

TEST_CASE("Serial entropy correction is deterministic across fresh instances [fast][core][lifecycle][columnar-contract]")
{
  // Hierarchical search spawns sub-Infomap instances; entropy bias correction must keep using
  // the full network's total degree / node count (formerly a shared static, now propagated to
  // sub instances via inheritNetworkPropertiesFrom). We assert seed determinism rather than an
  // absolute codelength: the exact value is a platform-dependent local optimum (greedy
  // tie-breaking differs across libm/FP), so a golden number is not portable. Propagation
  // correctness (serial results unchanged vs the pre-fix build) is verified out of band.
  const auto run = [] {
    InfomapWrapper im(infomap::test::withTestEngine("--silent --seed 7 --num-trials 1 --entropy-corrected --no-file-output"));
    im.readInputData(infomap::test::repoPath("examples/networks/ninetriangles.net"));
    im.run();
    infomap::test::checkRunSanity(im);
    return im.codelength();
  };

  CHECK(run() == doctest::Approx(run()));
}

TEST_CASE("Parallel trials with variable Markov time match serial trials [fast][core][lifecycle][openmp][columnar-contract]")
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

TEST_CASE("Parallel trials with entropy correction match serial trials [fast][core][lifecycle][openmp][columnar-contract]")
{
#ifdef _OPENMP
  const auto codelengths = runParallelTrialsFixture("--entropy-corrected");
  REQUIRE(codelengths.size() == 4);
  for (unsigned int i = 0; i < codelengths.size(); ++i) {
    CHECK(codelengths[i] == doctest::Approx(runSingleTrialFixtureWith(7 + i, "--entropy-corrected")));
  }
#endif
}

TEST_CASE("Parallel trials with variable Markov time are invariant to worker count [fast][core][lifecycle][openmp][columnar-contract]")
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

TEST_CASE("Parallel trials with entropy correction are invariant to worker count [fast][core][lifecycle][openmp][columnar-contract]")
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

TEST_CASE("Infomap reruns ninetriangles deterministically on the same instance [fast][core][lifecycle][crash][columnar-contract]")
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

TEST_CASE("readInputData accumulate=false replaces the previous network [fast][core][lifecycle][parser][columnar-contract]")
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

TEST_CASE("readInputData accumulate=true appends first-order fixtures [fast][core][lifecycle][parser][columnar-contract]")
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

TEST_CASE("readInputData accumulate mode stays stable across multiple runs on the same instance [fast][core][lifecycle][parser][columnar-contract]")
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

TEST_CASE("Higher-order module queries require state ids [fast][core][lifecycle][columnar-contract]")
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

TEST_CASE("Higher-order input reruns deterministically on the same instance [fast][core][lifecycle][crash][columnar-contract]")
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

TEST_CASE("File-backed multilayer input clusters as a higher-order network [fast][core][lifecycle][parser][columnar-contract]")
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

TEST_CASE("getMultilayerStateId resolves physical (layer, node) to the generated state id [fast][core][multilayer][parser]")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  infomap::test::readNetworkFixture(im, "multilayer.net");

  const auto& map = im.network().layerNodeToStateId();
  REQUIRE(!map.empty());
  for (const auto& layerEntry : map) {
    const auto layerId = layerEntry.first;
    for (const auto& nodeEntry : layerEntry.second) {
      const auto nodeId = nodeEntry.first;
      const auto stateId = nodeEntry.second;
      CHECK(im.getMultilayerStateId(layerId, nodeId) == stateId);
    }
  }

  CHECK_THROWS_AS(im.getMultilayerStateId(99999, 99999), std::out_of_range);
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

TEST_CASE("writeTree and writeClu render higher-order state output [fast][core][output][columnar-contract]")
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

TEST_CASE("OutputView projects first-order leaf rows [fast][core][output][columnar-contract]")
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

TEST_CASE("OutputView separates higher-order physical and state rows [fast][core][output][columnar-contract]")
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

TEST_CASE("OutputView exposes multilayer state layer ids [fast][core][output][columnar-contract]")
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

TEST_CASE("OutputView applies bipartite hide filter centrally [fast][core][output][columnar-contract]")
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

TEST_CASE("writeFlowTree is stable across repeated calls on the same instance [fast][core][output][regression][columnar-contract]")
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

TEST_CASE("writeResult renders selected first-order output artifacts [fast][core][output][columnar-contract]")
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

TEST_CASE("writeResult renders selected physical and state output artifacts [fast][core][output][columnar-contract]")
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

// Repeated runs on the same instance free and re-allocate the whole node/edge
// tree from the per-instance ObjectPools. The point is that pool-slot reuse and
// teardown are sound across repeated runs: both runs must complete and yield a
// valid codelength, and (under sanitizers) without any use-after-free, with the
// empty-pool invariant holding at teardown. Exact codelength equality across
// re-runs is NOT asserted -- it is not a guaranteed invariant (e.g. parallel
// scheduling can break ties differently between runs).
TEST_CASE("InfomapWrapper reuses pooled nodes across repeated runs without leaking [fast][core][lifecycle][memory]")
{
  InfomapWrapper im("--two-level --silent --seed 7 --num-trials 1 --no-file-output");
  im.readInputData(infomap::test::repoPath("examples/networks/ninetriangles.net"));

  im.run();
  const double first = im.codelength();

  im.run(); // second run reclaims and reuses freed pool slots
  const double second = im.codelength();

  CHECK(std::isfinite(first));
  CHECK(first > 0.0);
  CHECK(std::isfinite(second));
  CHECK(second > 0.0);
}

} // namespace
