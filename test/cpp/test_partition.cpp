#include "vendor/doctest.h"

#include "Infomap.h"
#include "io/InfomapError.h"
#include "io/Output.h"

#include "TestUtils.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>
#include <set>
#include <iostream>
#include <sstream>
#include <tuple>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace {

using infomap::InfomapWrapper;
using infomap::InfoNode;

using EdgeKey = std::pair<unsigned int, unsigned int>;

std::vector<unsigned int> childStateIds(const InfoNode& node)
{
  std::vector<unsigned int> ids;
  for (const auto& child : node.children()) {
    ids.push_back(child.stateId);
  }
  return ids;
}

std::map<EdgeKey, double> aggregatedInterModuleFlow(std::vector<InfoNode*>& nodes, bool undirected)
{
  std::map<EdgeKey, double> flows;
  for (auto* node : nodes) {
    for (auto* edge : node->outEdges()) {
      auto module1 = node->index;
      auto module2 = edge->target->index;
      if (module1 == module2) {
        continue;
      }
      if (undirected && module1 > module2) {
        std::swap(module1, module2);
      }
      flows[{ module1, module2 }] += edge->data.flow;
    }
  }
  return flows;
}

std::map<EdgeKey, double> aggregatedModuleFlow(InfoNode& root, bool undirected)
{
  std::map<EdgeKey, double> flows;
  for (auto& module : root.children()) {
    for (auto* edge : module.outEdges()) {
      auto module1 = module.index;
      auto module2 = edge->target->index;
      if (module1 == module2) {
        continue;
      }
      if (undirected && module1 > module2) {
        std::swap(module1, module2);
      }
      flows[{ module1, module2 }] += edge->data.flow;
    }
  }
  return flows;
}

struct FixedPartitionResult {
  double codelength;
  double indexCodelength;
};

FixedPartitionResult evaluateFixedPartition(const std::string& extraFlags, bool columnar, bool selfLink = false, bool metadata = false)
{
  std::string flags = "--seed 123 --num-trials 1 --silent --no-infomap";
  if (!extraFlags.empty())
    flags += " " + extraFlags;
  if (columnar)
    flags += " --columnar";

  InfomapWrapper im(flags);
  infomap::test::addEdgeFixtureLinks(im, "graphs/twotriangles_unweighted.edges");
  if (selfLink)
    im.addLink(1, 1, 10.0);
  if (metadata)
    im.initMetaData(infomap::test::fixturePath("meta/twotriangles.meta"));
  im.setInitialPartition({ { 1, 1 }, { 2, 1 }, { 3, 1 }, { 4, 2 }, { 5, 2 }, { 6, 2 } });
  im.run();

  CHECK(std::isfinite(im.codelength()));
  CHECK(std::isfinite(im.getIndexCodelength()));
  CHECK(im.codelength() >= 0.0);
  CHECK(im.getIndexCodelength() >= -1e-12);
  infomap::test::checkCanonicalPartition(im, { { 1, 2, 3 }, { 4, 5, 6 } });
  return { im.codelength(), im.getIndexCodelength() };
}

FixedPartitionResult evaluateFixedStatePartition(bool columnar)
{
  std::string flags = "--seed 123 --num-trials 1 --silent --no-infomap";
  if (columnar)
    flags += " --columnar";

  InfomapWrapper im(flags);
  infomap::test::readNetworkFixture(im, "states.net");
  im.setInitialPartition({ { 1, 1 }, { 2, 1 }, { 3, 1 }, { 4, 2 }, { 5, 2 }, { 6, 2 } });
  im.run();

  CHECK(std::isfinite(im.codelength()));
  CHECK(std::isfinite(im.getIndexCodelength()));
  infomap::test::checkCanonicalPartition(im, { { 1, 2, 3 }, { 4, 5, 6 } }, true);
  return { im.codelength(), im.getIndexCodelength() };
}

// Fixed-partition codelength on the co-located state fixtures. `network` is
// states_shared_physical.net (two states of one physical node in one module) or
// states_distinct_physical.net (the same links, no shared physical node).
double evaluateColocatedStatePartition(const char* network, bool columnar, bool metadata)
{
  std::string flags = "--seed 123 --num-trials 1 --silent --no-infomap";
  if (columnar)
    flags += " --columnar";
  if (metadata)
    flags += " --meta-data " + infomap::test::fixturePath("meta/states_crossing.meta");

  InfomapWrapper im(flags);
  infomap::test::readNetworkFixture(im, network);
  im.setInitialPartition({ { 1, 1 }, { 2, 1 }, { 3, 1 }, { 4, 2 }, { 5, 2 }, { 6, 2 } });
  im.run();

  CHECK(std::isfinite(im.codelength()));
  infomap::test::checkCanonicalPartition(im, { { 1, 2, 3 }, { 4, 5, 6 } }, true);
  return im.codelength();
}

TEST_CASE("Columnar composes the metadata and physical-node codebooks [fast][core][partition][columnar][meta]")
{
  // The two codebooks are independent, so each one's contribution must be the
  // same whether or not the other is present. Asserted as two differences rather
  // than as pinned totals: a difference stays valid if the metadata term is ever
  // rescaled, and it states the property the fix is about instead of a number
  // that happens to fall out of it.
  //
  // Before #1012 addColumnarCorrections attached MemCorrection in an `else`
  // branch of the metadata one, so --meta-data on higher-order input dropped the
  // physical-node codebook: the first difference below was 0 instead of
  // -0.330578512396694.
  const double sharedMeta = evaluateColocatedStatePartition("states_shared_physical.net", true, true);
  const double distinctMeta = evaluateColocatedStatePartition("states_distinct_physical.net", true, true);
  const double sharedPlain = evaluateColocatedStatePartition("states_shared_physical.net", true, false);
  const double distinctPlain = evaluateColocatedStatePartition("states_distinct_physical.net", true, false);

  // The physical-node codebook saves the same amount with and without metadata.
  infomap::test::checkApproxCodelength(sharedMeta - distinctMeta, sharedPlain - distinctPlain);
  infomap::test::checkApproxCodelength(sharedPlain - distinctPlain, -0.330578512396694);

  // The metadata term costs the same amount with and without the aggregation.
  infomap::test::checkApproxCodelength(sharedMeta - sharedPlain, distinctMeta - distinctPlain);
  infomap::test::checkApproxCodelength(distinctMeta - distinctPlain, 0.915516344471709);
}

TEST_CASE("Columnar reports its own composed codelength across trials [fast][core][partition][columnar][meta]")
{
  // restoreBestResult re-materializes the winning trial's tree through the OO
  // objective, which under --meta-data is metadata-only and therefore disagrees
  // with the composed value the columnar core actually optimized. The block is
  // guarded by m_trialsRun > 1, so the disagreement is invisible at one trial:
  // before #1012 broadened that guard from nonRedundant to columnarSearch, a
  // 2-trial run printed 2.247219447 to the console and wrote 2.577797959367095 to
  // the .tree/.json files. codelength() reads the same value the files do.
  auto search = [](unsigned int numTrials) {
    InfomapWrapper im("--seed 123 --num-trials " + std::to_string(numTrials) + " --silent --two-level --columnar --meta-data " + infomap::test::fixturePath("meta/states_crossing.meta"));
    infomap::test::readNetworkFixture(im, "states_shared_physical.net");
    im.run();
    infomap::test::checkCanonicalPartition(im, { { 1, 2, 3 }, { 4, 5, 6 } }, true);
    return im.codelength();
  };

  const double oneTrial = search(1);
  infomap::test::checkApproxCodelength(oneTrial, 2.247219446970401);
  // Same partition, same objective: more trials must not change the number that
  // is reported and written out.
  infomap::test::checkApproxCodelength(search(2), oneTrial);
  infomap::test::checkApproxCodelength(search(10), oneTrial);
}

TEST_CASE("Meta-data on higher-order input diverges between OO and columnar by design [fast][core][partition][columnar-differential][meta]")
{
  // The documented exception to the rule the rest of this family enforces.
  //
  // Metadata and the physical-node codebook are independent codebooks over the
  // same leaves, and the columnar core scores both because corrections are
  // summed. The OO engine cannot: MetaMapEquation and MemMapEquation are sibling
  // `final` classes with different DeltaFlowDataTypes, so
  // InfomapOptimizer<Objective> holds exactly one, and initOptimizer picks
  // metadata. Composing them there means a new objective class, which is a
  // separate feature (#1012).
  //
  // So this is a deliberate divergence, not a tolerance to be loosened: -C
  // returns a lower codelength here because it is scoring a strictly richer
  // objective on the same partition. Both values are pinned so that a change to
  // either engine has to come here and say which one moved.
  const double oo = evaluateColocatedStatePartition("states_shared_physical.net", false, true);
  const double columnar = evaluateColocatedStatePartition("states_shared_physical.net", true, true);

  infomap::test::checkApproxCodelength(oo, 2.577797959367095);
  infomap::test::checkApproxCodelength(columnar, 2.247219446970401);
  CHECK(columnar < oo);

  // The divergence needs BOTH conditions. Without metadata the two engines score
  // the same objective, and with metadata but no co-located states (states.net
  // splits its shared physical node across the modules) the physical-node
  // codebook saves nothing, so they agree there too -- which is why the existing
  // meta+state coverage never saw this.
  infomap::test::checkApproxCodelength(evaluateColocatedStatePartition("states_shared_physical.net", true, false),
                                       evaluateColocatedStatePartition("states_shared_physical.net", false, false));
  const auto ooSplit = evaluateFixedStatePartition(false);
  const auto columnarSplit = evaluateFixedStatePartition(true);
  infomap::test::checkApproxCodelength(columnarSplit.codelength, ooSplit.codelength);
}

TEST_CASE("Fixed partitions have identical OO and columnar codelengths [fast][core][partition][columnar-differential]")
{
  // Exception: --meta-data on higher-order input. See the divergence test above
  // -- the columnar core composes the metadata and physical-node codebooks and
  // the OO objective dispatch selects one of them. The `--meta-data-rate 2` case
  // below is on a first-order network, where there is no second codebook, so it
  // still belongs to this family.
  struct TestCase {
    const char* flags;
    bool selfLink;
    bool metadata;
  };
  const TestCase cases[] = {
    { "", false, false },
    { "--directed", false, false },
    { "--directed --recorded-teleportation", false, false },
    { "--markov-time 2", false, false },
    { "--variable-markov-time", false, false },
    { "--regularized", false, false },
    { "--entropy-corrected", false, false },
    { "--meta-data-rate 2", false, true },
    { "--directed --recorded-teleportation", true, false },
  };

  for (const auto& testCase : cases) {
    INFO("flags: " << testCase.flags);
    CAPTURE(testCase.selfLink);
    CAPTURE(testCase.metadata);
    const auto oo = evaluateFixedPartition(testCase.flags, false, testCase.selfLink, testCase.metadata);
    const auto columnar = evaluateFixedPartition(testCase.flags, true, testCase.selfLink, testCase.metadata);
    infomap::test::checkApproxCodelength(columnar.codelength, oo.codelength);
    infomap::test::checkApproxCodelength(columnar.indexCodelength, oo.indexCodelength);
  }

  const auto ooState = evaluateFixedStatePartition(false);
  const auto columnarState = evaluateFixedStatePartition(true);
  infomap::test::checkApproxCodelength(columnarState.codelength, ooState.codelength);
  infomap::test::checkApproxCodelength(columnarState.indexCodelength, ooState.indexCodelength);
}

TEST_CASE("Two-level search finds the same optimum on OO and columnar engines [fast][core][partition][columnar-differential]")
{
  auto runTwoLevel = [](bool columnar) {
    std::string flags = "--seed 123 --num-trials 5 --silent --two-level";
    if (columnar)
      flags += " --columnar";
    InfomapWrapper im(flags);
    infomap::test::addEdgeFixtureLinks(im, "graphs/twotriangles_unweighted.edges");
    im.run();
    CHECK(im.maxTreeDepth() == 2); // --two-level must not build a deeper hierarchy
    infomap::test::checkCanonicalPartition(im, { { 1, 2, 3 }, { 4, 5, 6 } });
    return im.codelength();
  };

  const double oo = runTwoLevel(false);
  const double columnar = runTwoLevel(true);
  infomap::test::checkApproxCodelength(columnar, oo);
}

//! Sum InfoNode::codelength over every module including the root, i.e. the quantity
//! calcCodelengthOnTree(root(), true) returns. Leaves carry no codelength of their own.
double sumModuleCodelengths(const InfoNode& node)
{
  if (node.isLeaf()) {
    return 0.0;
  }
  double sum = node.codelength;
  for (const auto& child : node.children()) {
    sum += sumModuleCodelengths(child);
  }
  return sum;
}

unsigned int numScoredModules(const InfoNode& node)
{
  if (node.isLeaf()) {
    return 0;
  }
  unsigned int count = node.codelength > 0.0 ? 1 : 0;
  for (const auto& child : node.children()) {
    count += numScoredModules(child);
  }
  return count;
}

TEST_CASE("Cluster-data clu fixture initializes a two-level partition [fast][core][partition][columnar-contract]")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  im.readInputData(infomap::test::repoPath("examples/networks/twotriangles.net"));
  im.initNetwork(im.network());

  im.initPartition(infomap::test::clusterFixturePath("twotriangles_two_modules.clu"), false, &im.network());

  CHECK(im.numLeafNodes() == 6);
  CHECK(im.numTopModules() == 2);
  CHECK(im.numLevels() == 2);
  infomap::test::checkCanonicalPartition(im, { { 1, 2, 3 }, { 4, 5, 6 } });

  im.run();

  infomap::test::checkRunSanity(im);
  infomap::test::checkCanonicalPartition(im, { { 1, 2, 3 }, { 4, 5, 6 } });
}

TEST_CASE("Tree cluster-data fixture initializes a multi-level tree [fast][core][partition][columnar-contract]")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  im.readInputData(infomap::test::repoPath("examples/networks/twotriangles.net"));
  im.initNetwork(im.network());

  im.initPartition(infomap::test::clusterFixturePath("twotriangles_three_level.tree"), false, &im.network());

  CHECK(im.numLeafNodes() == 6);
  CHECK(im.numTopModules() == 2);
  CHECK(im.numLevels() == 3);
  const auto modules = im.getMultilevelModules(false);
  CHECK(modules.size() == 6);
  CHECK(modules.at(1).size() == 2);
  CHECK(modules.at(2).size() == 2);
  CHECK(modules.at(3).size() == 2);
  CHECK(modules.at(4).size() == 2);
  CHECK(modules.at(5).size() == 2);
  CHECK(modules.at(6).size() == 2);
  CHECK(modules.at(1).at(0) == modules.at(2).at(0));
  CHECK(modules.at(2).at(0) == modules.at(3).at(0));
  CHECK(modules.at(4).at(0) == modules.at(5).at(0));
  CHECK(modules.at(5).at(0) == modules.at(6).at(0));
  CHECK(modules.at(1).at(0) != modules.at(4).at(0));
  CHECK(modules.at(1).at(1) == modules.at(2).at(1));
  CHECK(modules.at(2).at(1) == modules.at(3).at(1));
  CHECK(modules.at(4).at(1) == modules.at(5).at(1));
  CHECK(modules.at(5).at(1) == modules.at(6).at(1));

  im.run();

  infomap::test::checkRunSanity(im);
}

TEST_CASE("A physical traversal leaves the engine's tree untouched [fast][core][partition][tree]")
{
  // InfomapIteratorPhysical stores InfoNode *copies* of live leaves, and InfoNode's copy
  // constructor takes the sibling pointers verbatim while ~InfoNode unlinks through them.
  // Destroying those copies therefore wrote into the engine's own nodes on a traversal
  // documented as read-only -- and into freed memory if a position outlived the engine
  // (#900).
  auto im = infomap::test::makeRunningInfomap([&](InfomapWrapper& infomap) {
    infomap.readInputData(infomap::test::repoPath("test/fixtures/networks/states.net"));
  });

  struct Links {
    const infomap::InfoNode* previous;
    const infomap::InfoNode* next;
    const infomap::InfoNode* parent;
    const infomap::InfoNode* firstChild;
  };
  std::map<unsigned int, Links> before;
  for (auto it = im->iterLeafNodes(); !it.isEnd(); ++it) {
    const auto& node = *it;
    before[node.stateId] = { node.previous, node.next, node.parent, node.firstChild };
  }
  REQUIRE(before.size() == 6);

  {
    // A full physical walk, and positions kept past the end of it.
    std::vector<infomap::InfomapIterator> kept;
    for (auto it = im->iterTreePhysical(); !it.isEnd(); ++it)
      kept.push_back(it.copy());
    CHECK(kept.size() > 0);
    // Every kept position still resolves, and resolves to what it did when it was taken,
    // rather than pointing into storage the source iterator dropped when it left the leaf
    // module. Reading the values matters: a dangling pointer is still non-null, so only
    // the sanitizer or the value catches it.
    std::vector<unsigned int> keptPhysicalIds;
    for (const auto& position : kept) {
      REQUIRE(position.current() != nullptr);
      if (position.current()->isLeaf())
        keptPhysicalIds.push_back(position.current()->physicalId);
    }
    CHECK(keptPhysicalIds == std::vector<unsigned int> { 1, 2, 3, 1, 4, 5 });
  }

  for (auto it = im->iterLeafNodes(); !it.isEnd(); ++it) {
    const auto& node = *it;
    const auto& links = before.at(node.stateId);
    CHECK(node.previous == links.previous);
    CHECK(node.next == links.next);
    CHECK(node.parent == links.parent);
    CHECK(node.firstChild == links.firstChild);
  }
}

TEST_CASE("A physical tree that cannot express the partition says so [fast][core][partition][parser]")
{
  // A physical .tree has one row per (module, physical node) and no state ids, so when a
  // physical node's states sit in different modules the file cannot say which state went
  // where. The reader pairs ascending state ids against file order, which is a guess: on
  // examples/networks/multilayer.net the partition comes back at 3.71206 bits against the
  // 2.01141 that was written. The guess stays -- the information is not in the file -- but
  // it is no longer silent (#908).
  const std::string physicalTree = "physical_roundtrip.tree";
  const std::string statesTree = "physical_roundtrip_states.tree";
  std::remove(physicalTree.c_str());
  std::remove(statesTree.c_str());

  {
    InfomapWrapper writer(infomap::test::defaultFlags("--seed 1"));
    writer.readInputData(infomap::test::repoPath("examples/networks/multilayer.net"));
    writer.run();
    infomap::writeTree(writer, writer.network(), physicalTree, false);
    infomap::writeTree(writer, writer.network(), statesTree, true);
  }

  auto warningsWhenReading = [](const std::string& clusterFile) {
    std::ostringstream captured;
    {
      infomap::test::ScopedLogCapture capture(captured);
      // Not defaultFlags: it carries --silent, which mutes the very warning under test.
      InfomapWrapper reader("--seed 123 --num-trials 1 --no-file-output --no-infomap --cluster-data " + clusterFile);
      reader.readInputData(infomap::test::repoPath("examples/networks/multilayer.net"));
      reader.run();
    }
    return captured.str();
  };

  const auto physicalOutput = warningsWhenReading(physicalTree);
  CHECK(physicalOutput.find("split across modules") != std::string::npos);
  // Singular, since exactly one physical node is split here.
  CHECK(physicalOutput.find("1 physical node has its") != std::string::npos);

  // The states tree carries the state ids, so it round-trips and must stay quiet.
  const auto statesOutput = warningsWhenReading(statesTree);
  CHECK(statesOutput.find("split across modules") == std::string::npos);

  std::remove(physicalTree.c_str());
  std::remove(statesTree.c_str());
}

TEST_CASE("Mixed-depth cluster data is rejected instead of crashing [fast][core][partition][parser]")
{
  // A module with both a leaf and a sub-module as children used to segfault: the
  // per-level aggregation read the first child, concluded "these are modules", and then
  // recursed into the leaf, dereferencing its null firstChild. Reordering the same
  // partition's rows gave the same tree two different codelengths instead (#898).
  InfomapWrapper im(infomap::test::defaultFlags());
  im.readInputData(infomap::test::repoPath("examples/networks/twotriangles.net"));
  im.initNetwork(im.network());

  try {
    im.initPartition(infomap::test::clusterFixturePath("twotriangles_mixed_depth.tree"), false, &im.network());
    FAIL("expected mixed-depth cluster data to be rejected");
  } catch (const infomap::InfomapError& e) {
    CHECK(e.code() == infomap::ExitCode::InputError);
    const std::string message(e.what());
    CHECK(message.find("mixes depths") != std::string::npos);
    // The offending module has to be named, or the user cannot find it in the file.
    CHECK(message.find("module 1") != std::string::npos);
  }
}

TEST_CASE("A two-level search from a deeper tree keeps its top level [fast][core][partition]")
{
  // --two-level with a deeper cluster tree reported codelength 0 on a tree that still
  // had sub-modules in it, or aborted with "fineTune() called but numLevels != 2" once
  // the tuning loop was allowed to run (#898). Both are gone: the supplied tree's top
  // level becomes the starting partition, which is what --two-level means.
  InfomapWrapper im(infomap::test::defaultFlags("--two-level"));
  im.readInputData(infomap::test::repoPath("examples/networks/twotriangles.net"));
  im.initNetwork(im.network());

  im.initPartition(infomap::test::clusterFixturePath("twotriangles_three_level.tree"), false, &im.network());

  CHECK(im.numLevels() == 2);
  CHECK(im.maxTreeDepth() == 2);
  CHECK(im.numTopModules() == 2);

  im.run();

  infomap::test::checkRunSanity(im);
  CHECK(im.codelength() > 0.0);
  CHECK(im.maxTreeDepth() == 2);
}

TEST_CASE("removeSubModules flattens a ragged tree whose shallow branch comes first [fast][core][partition][tree]")
{
  // numLevels() follows the firstChild chain only, so with the shallow branch first it
  // reports 2 for a three-level tree and the old `while (numLevels() > 2)` loop did
  // nothing at all -- leaving sub-modules in place for calcCodelengthOnTree to score as
  // if they were not there (#898).
  InfomapWrapper im(infomap::test::defaultFlags());
  im.readInputData(infomap::test::repoPath("examples/networks/twotriangles.net"));
  im.initNetwork(im.network());
  im.initPartition(infomap::test::clusterFixturePath("twotriangles_ragged_branches.tree"), false, &im.network());

  REQUIRE(im.maxTreeDepth() == 3);
  // The shortcut this test exists for, pinned so the fixture keeps exercising it.
  REQUIRE(im.numLevels() == 2);

  im.removeSubModules(true);

  CHECK(im.maxTreeDepth() == 2);
  unsigned int numModules = 0;
  for (auto& module : im.root()) {
    ++numModules;
    for (auto& child : module) {
      CHECK(child.isLeaf());
    }
  }
  CHECK(numModules == 2);
}

TEST_CASE("An unknown node id in cluster data leaves the partition alone [fast][core][partition][parser]")
{
  // std::map::operator[] inserted the unknown id with a default index of 0, so it
  // reassigned the *first* leaf node to the stray module and marked it as covered,
  // which also silenced the note about nodes genuinely missing an assignment (#898).
  // Compared as a grouping rather than as raw module indices, which get renumbered:
  // nodes are visited in state-id order and each new parent gets the next group number,
  // so two runs agree exactly when they put the same nodes together.
  auto groupingFrom = [](const std::string& clusterFile) {
    InfomapWrapper im(infomap::test::defaultFlags("--no-infomap"));
    im.readInputData(infomap::test::repoPath("examples/networks/twotriangles.net"));
    im.initNetwork(im.network());
    im.initPartition(infomap::test::clusterFixturePath(clusterFile), false, &im.network());

    std::map<unsigned int, const infomap::InfoNode*> parentByStateId;
    for (auto it = im.iterLeafNodes(); !it.isEnd(); ++it) {
      const auto& node = *it;
      parentByStateId[node.stateId] = node.parent;
    }

    std::map<const infomap::InfoNode*, unsigned int> groupOfParent;
    std::map<unsigned int, unsigned int> grouping;
    for (const auto& it : parentByStateId) {
      const auto inserted = groupOfParent.emplace(it.second, static_cast<unsigned int>(groupOfParent.size()));
      grouping[it.first] = inserted.first->second;
    }
    return grouping;
  };

  const auto clean = groupingFrom("twotriangles_two_modules.clu");
  const auto withUnknownId = groupingFrom("twotriangles_two_modules_unknown_id.clu");

  REQUIRE(clean.size() == 6);
  // Two groups of three, and the first leaf is not pulled out into the stray module.
  CHECK(clean.at(1) == clean.at(2));
  CHECK(clean.at(1) == clean.at(3));
  CHECK(clean.at(1) != clean.at(4));
  CHECK(withUnknownId == clean);
}

TEST_CASE("Tree cluster-data reinit and rerun stay stable on the same instance [fast][core][partition][lifecycle][columnar-contract]")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  im.readInputData(infomap::test::repoPath("examples/networks/twotriangles.net"));

  auto runTreePartition = [&]() {
    im.initNetwork(im.network());
    im.initPartition(infomap::test::clusterFixturePath("twotriangles_three_level.tree"), false, &im.network());

    CHECK(im.numLeafNodes() == 6);
    CHECK(im.numTopModules() == 2);
    CHECK(im.numLevels() == 3);

    const auto modules = im.getMultilevelModules(false);
    CHECK(modules.size() == 6);
    CHECK(modules.at(1).size() == 2);
    CHECK(modules.at(2).size() == 2);
    CHECK(modules.at(3).size() == 2);
    CHECK(modules.at(4).size() == 2);
    CHECK(modules.at(5).size() == 2);
    CHECK(modules.at(6).size() == 2);

    im.run();
    infomap::test::checkRunSanity(im);
  };

  runTreePartition();
  const auto firstModules = im.getMultilevelModules(false);
  const auto firstCodelength = im.codelength();
  const auto firstIndexCodelength = im.getIndexCodelength();
  const auto firstNumLevels = im.numLevels();

  runTreePartition();

  CHECK(im.getMultilevelModules(false) == firstModules);
  CHECK(im.codelength() == doctest::Approx(firstCodelength));
  CHECK(im.getIndexCodelength() == doctest::Approx(firstIndexCodelength));
  CHECK(im.numLevels() == firstNumLevels);
}

TEST_CASE("A two-level partition re-initialised after a multi-level one scores the leaves [fast][core][partition][lifecycle]")
{
  // #831: the objective's network-level terms (the base map equation's
  // nodeFlow_log_nodeFlow) describe whichever network was active when they were last
  // initialised. The multi-level branch of initTree leaves them initialised for the
  // *module* network, and the two-level route did not restore them, so re-initialising a
  // two-level partition on the same instance scored the leaves against the module
  // network's terms -- off by one whole one-level codelength. On politicalblogs that came
  // out negative and was written to the .tree file while the console reported the correct
  // value. Pinned as: the same partition must score the same on a reused instance as on a
  // fresh one.
  auto twoLevelCodelength = [](InfomapWrapper& im) {
    im.initPartition(infomap::test::clusterFixturePath("twotriangles_two_modules.clu"), false, &im.network());
    return im.codelength();
  };
  // --no-infomap so the flow is calculated (the codelength is 0 without it) while no
  // search runs, keeping each instance's only module state the one this test gives it.
  auto makeInstance = [] {
    return infomap::test::makeRunningInfomap(
        [](InfomapWrapper& im) { im.readInputData(infomap::test::repoPath("examples/networks/twotriangles.net")); },
        "--no-infomap");
  };

  auto fresh = makeInstance();
  const auto expected = twoLevelCodelength(*fresh);
  REQUIRE(expected > 0.0);

  auto reused = makeInstance();
  // A multi-level partition first -- this is what left the terms behind.
  reused->initPartition(infomap::test::clusterFixturePath("twotriangles_three_level.tree"), false, &reused->network());
  REQUIRE(reused->numLevels() == 3);

  CHECK(twoLevelCodelength(*reused) == doctest::Approx(expected));
  CHECK(reused->getIndexCodelength() == doctest::Approx(fresh->getIndexCodelength()));
}

TEST_CASE("A flat tree materialization scores its modules [fast][core][partition][lifecycle]")
{
  // #1002: initTree's `maxDepth == 2 || twoLevel` shortcut returned straight out of
  // initPartition, which updates the objective's aggregate bookkeeping but writes no
  // InfoNode::codelength. Only the deep branch scored the tree, so a flat materialization
  // left every module holding whatever it held before. Here that is 0 on a fresh
  // instance; on a reused one it is a stale value from an earlier, deeper partition.
  // --no-infomap keeps the search out of it: with a search running, partition() rescores
  // the tree at the end and the gap is invisible.
  auto im = infomap::test::makeRunningInfomap(
      [](InfomapWrapper& wrapper) { wrapper.readInputData(infomap::test::repoPath("examples/networks/twotriangles.net")); },
      "--no-infomap --two-level");

  // A three-level tree under --two-level takes the shortcut through its second
  // predicate, keeping only each leaf's top-level module.
  im->initPartition(infomap::test::clusterFixturePath("twotriangles_three_level.tree"), false, &im->network());
  REQUIRE(im->numLevels() == 2);
  REQUIRE(im->codelength() > 0.0);

  // Root plus the two modules; the leaves have no codelength of their own.
  CHECK(numScoredModules(im->root()) == 3);
  CHECK(sumModuleCodelengths(im->root()) == doctest::Approx(im->codelength()));
}

TEST_CASE("A restored flat best-of-N winner keeps its module codelengths [fast][core][partition][lifecycle]")
{
  // #1002, as users met it: restoreBestResult re-materializes the winning tree through
  // initTree and immediately rewrites the output file, so when the winner was flat every
  // modules[].codelength in the JSON output came out 0 -- while the same run at
  // --num-trials 1 wrote real values, because nothing restored. ninetriangles under
  // --two-level reaches the same codelength in every trial, so trial 1 stays the best and
  // the restore always fires.
  InfomapWrapper im("--seed 123 --num-trials 10 --silent --two-level --no-file-output");
  im.readInputData(infomap::test::repoPath("examples/networks/ninetriangles.net"));
  im.run();

  REQUIRE(im.numLevels() == 2);
  // Root plus nine module nodes.
  CHECK(numScoredModules(im.root()) == 10);
  CHECK(sumModuleCodelengths(im.root()) == doctest::Approx(im.codelength()));
}

TEST_CASE("Pretty per-level codelength renders a structured levels table [fast][core][partition][output]")
{
  InfomapWrapper im(infomap::test::defaultFlags("--directed -0 --no-file-output --pretty"));
  im.readInputData(infomap::test::repoPath("examples/networks/modular_wd.net"));
  im.run();

  std::ostringstream output;
  const auto numLevels = infomap::printPerLevelCodelength(im.root(), output);
  const auto text = output.str();

  CHECK(numLevels >= 2);
  CHECK(text.find("Levels") != std::string::npos);
  CHECK(text.find("Level  Modules  Leaves  Avg children  Module bits  Leaf bits  Total bits") != std::string::npos);
  CHECK(text.find("Total") != std::string::npos);
  CHECK(text.find("Per level number of modules") == std::string::npos);
  CHECK(text.find("2.700302") != std::string::npos);
}

TEST_CASE("Per-level aggregation survives a module with both kinds of child [fast][core][partition][output]")
{
  // Cluster data can no longer deliver this shape -- it is rejected at intake -- but the
  // aggregation is a shared reporting path and used to read only the first child to
  // decide whether a module holds leaves. A leaf sibling *after* a sub-module was then
  // recursed into and its null firstChild dereferenced (SIGSEGV), while a leaf *before*
  // one returned early and dropped every remaining sub-module from the table. Built by
  // hand so the guarantee is pinned even though no input can reach it (#898).
  InfoNode root;
  auto* module = new InfoNode({}, 100);
  auto* leafBeside = new InfoNode({}, 1);
  auto* subModule = new InfoNode({}, 200);
  auto* nestedLeafA = new InfoNode({}, 2);
  auto* nestedLeafB = new InfoNode({}, 3);

  root.addChild(module);
  // Sub-module first, then the leaf: the order that used to crash.
  module->addChild(subModule);
  module->addChild(leafBeside);
  subModule->addChild(nestedLeafA);
  subModule->addChild(nestedLeafB);

  std::ostringstream output;
  unsigned int numLevels = 0;
  CHECK_NOTHROW(numLevels = infomap::printPerLevelCodelength(root, output));

  // Three levels of nodes, and every leaf accounted for: one beside the sub-module and
  // two inside it.
  CHECK(numLevels == 3);
  std::vector<infomap::detail::PerLevelStat> stats;
  infomap::aggregatePerLevelCodelength(root, stats);
  unsigned int totalLeaves = 0;
  for (const auto& stat : stats)
    totalLeaves += stat.numLeafNodes;
  CHECK(totalLeaves == 3);

  root.releaseChildren();
  module->releaseChildren();
  subModule->releaseChildren();
  delete nestedLeafA;
  delete nestedLeafB;
  delete subModule;
  delete leafBeside;
  delete module;
}

TEST_CASE("InfoNode hierarchy mutations preserve parentage and child order [fast][core][partition][tree]")
{
  InfoNode root;

  auto* childA = new InfoNode({}, 10);
  auto* childB = new InfoNode({}, 20);
  auto* childC = new InfoNode({}, 30);
  root.addChild(childA);
  root.addChild(childB);
  root.addChild(childC);

  CHECK(root.childDegree() == 3);
  CHECK(childStateIds(root) == std::vector<unsigned int> { 10, 20, 30 });

  CHECK(root.collapseChildren() == 3);
  CHECK(root.childDegree() == 0);
  CHECK(root.firstChild == nullptr);
  CHECK(root.lastChild == nullptr);

  CHECK(root.expandChildren() == 3);
  CHECK(root.childDegree() == 3);
  CHECK(childStateIds(root) == std::vector<unsigned int> { 10, 20, 30 });
  CHECK(childA->parent == &root);
  CHECK(childB->parent == &root);
  CHECK(childC->parent == &root);

  root.releaseChildren();
  CHECK(root.childDegree() == 0);
  CHECK(root.firstChild == nullptr);
  CHECK(root.lastChild == nullptr);

  delete childA;
  delete childB;
  delete childC;

  auto* rebuiltA = new InfoNode({}, 40);
  auto* rebuiltB = new InfoNode({}, 50);
  root.addChild(rebuiltA);
  root.addChild(rebuiltB);

  CHECK(root.childDegree() == 2);
  CHECK(childStateIds(root) == std::vector<unsigned int> { 40, 50 });
}

TEST_CASE("InfoNode releaseChildren detaches active children without deleting them [fast][core][partition][tree][ownership]")
{
  InfoNode root;
  auto* childA = new InfoNode({}, 10);
  auto* childB = new InfoNode({}, 20);
  root.addChild(childA);
  root.addChild(childB);

  root.releaseChildren();

  CHECK(root.childDegree() == 0);
  CHECK(root.firstChild == nullptr);
  CHECK(root.lastChild == nullptr);
  CHECK(childA->stateId == 10);
  CHECK(childB->stateId == 20);
  CHECK(childA->parent == &root);
  CHECK(childB->parent == &root);
  CHECK(childA->next == childB);
  CHECK(childB->previous == childA);

  delete childA;
  delete childB;
}

TEST_CASE("Reinitializing a network clears collapsed root children [fast][core][partition][tree]")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  im.readInputData(infomap::test::repoPath("examples/networks/twotriangles.net"));
  im.initNetwork(im.network());

  CHECK(im.root().childDegree() == 6);
  CHECK(im.root().collapseChildren() == 6);
  CHECK(im.root().childDegree() == 0);

  im.initNetwork(im.network());

  CHECK(im.root().childDegree() == 6);
  CHECK(im.root().collapsedFirstChild == nullptr);
  CHECK(im.root().collapsedLastChild == nullptr);
}

TEST_CASE("InfoNode deleteChildren also clears collapsed children [fast][core][partition][tree]")
{
  InfoNode root;
  root.addChild(new InfoNode({}, 10));
  root.addChild(new InfoNode({}, 20));

  CHECK(root.collapseChildren() == 2);
  CHECK(root.childDegree() == 0);
  CHECK(root.firstChild == nullptr);
  CHECK(root.collapsedFirstChild != nullptr);

  root.deleteChildren();

  CHECK(root.childDegree() == 0);
  CHECK(root.firstChild == nullptr);
  CHECK(root.lastChild == nullptr);
  CHECK(root.collapsedFirstChild == nullptr);
  CHECK(root.collapsedLastChild == nullptr);
}

TEST_CASE("InfoNode deleteChildren clears both active and collapsed child chains [fast][core][partition][tree][ownership]")
{
  InfoNode root;
  root.addChild(new InfoNode({}, 10));
  root.addChild(new InfoNode({}, 20));

  CHECK(root.collapseChildren() == 2);
  root.addChild(new InfoNode({}, 30));

  root.deleteChildren();

  CHECK(root.childDegree() == 0);
  CHECK(root.firstChild == nullptr);
  CHECK(root.lastChild == nullptr);
  CHECK(root.collapsedFirstChild == nullptr);
  CHECK(root.collapsedLastChild == nullptr);
}

TEST_CASE("InfoNode initClean clears inherited collapsed children on clones [fast][core][partition][tree]")
{
  InfoNode source;
  source.addChild(new InfoNode({}, 10));
  source.addChild(new InfoNode({}, 20));

  CHECK(source.collapseChildren() == 2);
  CHECK(source.childDegree() == 0);
  CHECK(source.collapsedFirstChild != nullptr);
  CHECK(source.collapsedLastChild != nullptr);

  InfoNode clone(source);
  clone.initClean();

  CHECK(clone.childDegree() == 0);
  CHECK(clone.firstChild == nullptr);
  CHECK(clone.lastChild == nullptr);
  CHECK(clone.collapsedFirstChild == nullptr);
  CHECK(clone.collapsedLastChild == nullptr);

  source.deleteChildren();
}

TEST_CASE("InfoNode replace mutations preserve flattened tree structure [fast][core][partition][tree]")
{
  InfoNode root;
  auto* moduleA = new InfoNode({}, 100);
  auto* moduleB = new InfoNode({}, 200);
  root.addChild(moduleA);
  root.addChild(moduleB);
  moduleA->addChild(new InfoNode({}, 1));
  moduleA->addChild(new InfoNode({}, 2));
  moduleB->addChild(new InfoNode({}, 3));
  moduleB->addChild(new InfoNode({}, 4));

  CHECK(root.childDegree() == 2);
  CHECK(moduleA->childDegree() == 2);
  CHECK(moduleB->childDegree() == 2);

  CHECK(moduleA->replaceWithChildren() == 1);
  CHECK(root.childDegree() == 3);
  CHECK(childStateIds(root) == std::vector<unsigned int> { 1, 2, 200 });
  CHECK(root.firstChild->parent == &root);
  CHECK(root.firstChild->next->parent == &root);
  CHECK(root.lastChild == moduleB);

  CHECK(root.replaceChildrenWithGrandChildren() == 1);
  CHECK(root.childDegree() == 4);
  CHECK(childStateIds(root) == std::vector<unsigned int> { 1, 2, 3, 4 });
  for (const auto& child : root.children()) {
    CHECK(child.parent == &root);
    CHECK(child.isLeaf());
  }
}

TEST_CASE("InfoNode replaceWithChildren reparents a middle child chain before deleting the module [fast][core][partition][tree][ownership]")
{
  InfoNode root;
  auto* before = new InfoNode({}, 10);
  auto* module = new InfoNode({}, 20);
  auto* after = new InfoNode({}, 30);
  root.addChild(before);
  root.addChild(module);
  root.addChild(after);
  module->addChild(new InfoNode({}, 1));
  module->addChild(new InfoNode({}, 2));

  CHECK(module->replaceWithChildren() == 1);

  CHECK(childStateIds(root) == std::vector<unsigned int> { 10, 1, 2, 30 });
  CHECK(root.firstChild == before);
  CHECK(root.lastChild == after);
  CHECK(before->next->stateId == 1);
  CHECK(before->next->parent == &root);
  CHECK(before->next->next->stateId == 2);
  CHECK(before->next->next->parent == &root);
  CHECK(before->next->next->next == after);
  CHECK(after->previous->stateId == 2);
}

TEST_CASE("InfoNode remove documents current child-chain ownership semantics [fast][core][partition][tree][ownership]")
{
  InfoNode deleteRoot;
  auto* deletingParent = new InfoNode({}, 10);
  deletingParent->addChild(new InfoNode({}, 11));
  deleteRoot.addChild(deletingParent);

  deletingParent->remove(false);

  CHECK(deleteRoot.firstChild == nullptr);
  CHECK(deleteRoot.lastChild == nullptr);

  InfoNode detachRoot;
  auto* detachingParent = new InfoNode({}, 20);
  auto* detachedChild = new InfoNode({}, 21);
  detachingParent->addChild(detachedChild);
  detachRoot.addChild(detachingParent);

  detachingParent->remove(true);

  CHECK(detachRoot.firstChild == nullptr);
  CHECK(detachRoot.lastChild == nullptr);
  CHECK(detachedChild->stateId == 21);

  detachedChild->parent = nullptr;
  detachedChild->previous = nullptr;
  detachedChild->next = nullptr;
  delete detachedChild;
}

TEST_CASE("InfoNode owns outgoing edges while incoming edges are non-owning references [fast][core][partition][tree][ownership]")
{
  auto* source = new InfoNode({}, 10);
  InfoNode target({}, 20);
  source->addOutEdge(target, 1.0, 0.5);

  CHECK(source->outDegree() == 1);
  CHECK(target.inDegree() == 1);

  delete source;

  CHECK(target.inDegree() == 1);
}

TEST_CASE("Soft cluster-data can be optimized away when it is suboptimal [fast][core][partition][columnar-contract]")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  im.readInputData(infomap::test::repoPath("examples/networks/twotriangles.net"));
  im.initNetwork(im.network());

  im.initPartition(infomap::test::clusterFixturePath("twotriangles_single_module.clu"), false, &im.network());

  CHECK(im.numLeafNodes() == 6);
  CHECK(im.numTopModules() == 1);

  im.run();

  infomap::test::checkRunSanity(im);
  CHECK(im.numTopModules() == 2);
  infomap::test::checkCanonicalPartition(im, { { 1, 2, 3 }, { 4, 5, 6 } });
}

TEST_CASE("Hard cluster-data preserves the imposed coarse partition [fast][core][partition][columnar-contract]")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  im.readInputData(infomap::test::repoPath("examples/networks/twotriangles.net"));
  im.initNetwork(im.network());

  im.initPartition(infomap::test::clusterFixturePath("twotriangles_two_modules.clu"), true, &im.network());

  CHECK(im.numLeafNodes() == 2);
  CHECK(im.numTopModules() == 2);
  CHECK_FALSE(im.haveModules());

  im.run();

  infomap::test::checkRunSanity(im);
  CHECK(im.numLeafNodes() == 6);
  CHECK(im.numTopModules() == 2);
  infomap::test::checkCanonicalPartition(im, { { 1, 2, 3 }, { 4, 5, 6 } });
}

TEST_CASE("Hard cluster-data loaded during run preserves the imposed partition [fast][core][partition][columnar-contract]")
{
  InfomapWrapper im(infomap::test::defaultFlags("--cluster-data " + infomap::test::clusterFixturePath("twotriangles_two_modules.clu")));
  im.clusterDataIsHard = true;
  im.readInputData(infomap::test::repoPath("examples/networks/twotriangles.net"));

  im.run();

  infomap::test::checkRunSanity(im);
  CHECK(im.numLeafNodes() == 6);
  CHECK(im.numTopModules() == 2);
  infomap::test::checkCanonicalPartition(im, { { 1, 2, 3 }, { 4, 5, 6 } });
}

TEST_CASE("Embedded JSON path seeds the initial partition [fast][core][partition][json][columnar-contract]")
{
  // The embedded nodes[].path assigns three modules {1,2}, {3,4}, {5,6}.
  // With --no-infomap the result is exactly that imposed initial partition,
  // proving the embedded path fed initPartition during the trial.
  InfomapWrapper im(infomap::test::defaultFlags("--no-infomap"));
  im.readInputData(infomap::test::repoPath("test/fixtures/networks/json/twotriangles_paths.json"));

  im.run();

  CHECK(im.numTopModules() == 3);
  infomap::test::checkCanonicalPartition(im, { { 1, 2 }, { 3, 4 }, { 5, 6 } });
}

TEST_CASE("Hard cluster-data reinit and rerun stay stable on the same instance [fast][core][partition][lifecycle][columnar-contract]")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  im.readInputData(infomap::test::repoPath("examples/networks/twotriangles.net"));

  auto runHardPartition = [&]() {
    im.initNetwork(im.network());
    im.initPartition(infomap::test::clusterFixturePath("twotriangles_two_modules.clu"), true, &im.network());

    CHECK(im.numLeafNodes() == 2);
    CHECK(im.numTopModules() == 2);
    CHECK_FALSE(im.haveModules());

    im.run();

    infomap::test::checkRunSanity(im);
    CHECK(im.numLeafNodes() == 6);
    CHECK(im.numTopModules() == 2);
    infomap::test::checkCanonicalPartition(im, { { 1, 2, 3 }, { 4, 5, 6 } });
  };

  runHardPartition();
  const auto firstPartition = infomap::test::canonicalPartition(im.getModules());
  const auto firstCodelength = im.codelength();
  const auto firstIndexCodelength = im.getIndexCodelength();

  runHardPartition();

  CHECK(infomap::test::canonicalPartition(im.getModules()) == firstPartition);
  CHECK(im.codelength() == doctest::Approx(firstCodelength));
  CHECK(im.getIndexCodelength() == doctest::Approx(firstIndexCodelength));
}
TEST_CASE("Hard cluster-data rerun preserves leaf metadata on the same instance [fast][core][partition][lifecycle][parser][columnar-contract]")
{
  InfomapWrapper im(infomap::test::defaultFlags("--meta-data-rate 2"));
  im.readInputData(infomap::test::repoPath("examples/networks/twotriangles.net"));
  im.initMetaData(infomap::test::fixturePath("meta/twotriangles.meta"));

  auto runHardPartition = [&]() {
    im.initNetwork(im.network());
    im.initPartition(infomap::test::clusterFixturePath("twotriangles_two_modules.clu"), true, &im.network());

    CHECK(im.numLeafNodes() == 2);
    CHECK(im.numTopModules() == 2);
    CHECK_FALSE(im.haveModules());

    im.run();

    infomap::test::checkRunSanity(im);
    CHECK(im.numLeafNodes() == 6);
    for (auto* leaf : im.leafNodes()) {
      CHECK_FALSE(leaf->metaData().empty());
    }
  };

  runHardPartition();
  const auto firstPartition = infomap::test::canonicalPartition(im.getModules());
  const auto firstCodelength = im.codelength();
  const auto firstIndexCodelength = im.getIndexCodelength();

  runHardPartition();

  CHECK(infomap::test::canonicalPartition(im.getModules()) == firstPartition);
  CHECK(im.codelength() == doctest::Approx(firstCodelength));
  CHECK(im.getIndexCodelength() == doctest::Approx(firstIndexCodelength));
}
TEST_CASE("Consolidate modules preserves aggregated inter-module flow [fast][core][partition][tree]")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  im.readInputData(infomap::test::repoPath("examples/networks/twotriangles.net"));
  im.initNetwork(im.network());
  im.setActiveNetworkFromLeafs();
  im.initPartition();

  std::vector<unsigned int> modules { 0, 0, 0, 1, 1, 1 };
  im.moveActiveNodesToPredefinedModules(modules);

  const auto expectedFlows = aggregatedInterModuleFlow(im.activeNetwork(), im.isUndirectedClustering());
  REQUIRE(expectedFlows.size() == 1);

  im.consolidateModules(false);

  CHECK(im.numTopModules() == 2);
  CHECK(im.root().childDegree() == 2);
  CHECK(aggregatedModuleFlow(im.root(), im.isUndirectedClustering()) == expectedFlows);
}

TEST_CASE("Invalid cluster-data fixtures fail deterministically [fast][core][partition][parser]")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  im.readInputData(infomap::test::repoPath("examples/networks/twotriangles.net"));
  im.initNetwork(im.network());

  CHECK_THROWS_WITH_AS(
      im.initPartition(infomap::test::clusterFixturePath("invalid_missing_module.clu"), false, &im.network()),
      "Couldn't parse node key and cluster id from line '1'",
      std::runtime_error);
  CHECK_THROWS_WITH_AS(
      im.initPartition(infomap::test::clusterFixturePath("invalid_zero_path.tree"), false, &im.network()),
      "There is a '0' in the tree path, lowest allowed integer is 1.",
      std::runtime_error);
}

TEST_CASE("Invalid cluster-data failure does not poison later valid init on the same instance [fast][core][partition][parser][lifecycle]")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  im.readInputData(infomap::test::repoPath("examples/networks/twotriangles.net"));

  im.initNetwork(im.network());
  CHECK_THROWS_WITH_AS(
      im.initPartition(infomap::test::clusterFixturePath("invalid_missing_module.clu"), false, &im.network()),
      "Couldn't parse node key and cluster id from line '1'",
      std::runtime_error);

  im.initNetwork(im.network());
  im.initPartition(infomap::test::clusterFixturePath("twotriangles_two_modules.clu"), false, &im.network());

  CHECK(im.numLeafNodes() == 6);
  CHECK(im.numTopModules() == 2);
  CHECK(im.numLevels() == 2);

  im.run();

  infomap::test::checkRunSanity(im);
  infomap::test::checkCanonicalPartition(im, { { 1, 2, 3 }, { 4, 5, 6 } });
}

TEST_CASE("Invalid tree cluster-data failure does not poison later valid tree init on the same instance [fast][core][partition][parser][lifecycle]")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  im.readInputData(infomap::test::repoPath("examples/networks/twotriangles.net"));

  im.initNetwork(im.network());
  CHECK_THROWS_WITH_AS(
      im.initPartition(infomap::test::clusterFixturePath("invalid_zero_path.tree"), false, &im.network()),
      "There is a '0' in the tree path, lowest allowed integer is 1.",
      std::runtime_error);

  im.initNetwork(im.network());
  im.initPartition(infomap::test::clusterFixturePath("twotriangles_three_level.tree"), false, &im.network());

  CHECK(im.numLeafNodes() == 6);
  CHECK(im.numTopModules() == 2);
  CHECK(im.numLevels() == 3);
  CHECK(im.getMultilevelModules(false).size() == 6);

  im.run();

  infomap::test::checkRunSanity(im);
}

namespace {

  std::string scratchTreePath(const std::string& tag)
  {
    return std::string("infomap_test_issue247_") + tag + ".tree";
  }

  void writeAndCheckRoundTrip(InfomapWrapper& source, const std::string& networkPath, bool states, const std::string& tag)
  {
    const auto treePath = scratchTreePath(tag);
    source.writeTree(treePath, states);
    const auto expectedCodelength = source.codelength();
    const auto expectedIndexCodelength = source.getIndexCodelength();

    // --no-infomap: load tree and verify codelength matches the source.
    InfomapWrapper noInfomap(infomap::test::defaultFlags("--no-infomap --cluster-data " + treePath));
    noInfomap.readInputData(networkPath);
    noInfomap.run();
    infomap::test::checkApproxCodelength(noInfomap.codelength(), expectedCodelength);
    infomap::test::checkApproxCodelength(noInfomap.getIndexCodelength(), expectedIndexCodelength);
    infomap::test::checkRunSanity(noInfomap);

    // Continuing optimization: load tree and run; codelength must not regress
    // and the lifecycle must not crash or leak.
    InfomapWrapper continued(infomap::test::defaultFlags("--cluster-data " + treePath));
    continued.readInputData(networkPath);
    continued.run();
    CHECK(continued.codelength() <= expectedCodelength + 1e-9);
    infomap::test::checkRunSanity(continued);

    std::remove(treePath.c_str());
  }

} // namespace

TEST_CASE("Tree with rows that mix physical and state id columns fails deterministically [fast][core][partition][parser]")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  im.readInputData(infomap::test::repoPath("examples/networks/twotriangles.net"));
  im.initNetwork(im.network());

  CHECK_THROWS_WITH_AS(
      im.initPartition(infomap::test::clusterFixturePath("mixed_id_columns.tree"), false, &im.network()),
      "Mixed state and physical tree ids are not supported in line '1:2 0.5 \"2\" 2'.",
      std::runtime_error);
}

TEST_CASE("Tree round-trip reproduces codelength on a regular network [fast][core][partition][parser][lifecycle][columnar-contract]")
{
  auto baseline = infomap::test::makeRunningInfomap(
      [](InfomapWrapper& im) { im.readInputData(infomap::test::repoPath("examples/networks/ninetriangles.net")); },
      "--num-trials 5");

  writeAndCheckRoundTrip(*baseline, infomap::test::repoPath("examples/networks/ninetriangles.net"), false, "ninetriangles");
}

TEST_CASE("Tree round-trip reproduces codelength on a higher-order (states) network [fast][core][partition][parser][lifecycle][columnar-contract]")
{
  auto baseline = infomap::test::makeRunningInfomap(
      [](InfomapWrapper& im) { im.readInputData(infomap::test::repoPath("examples/networks/states.net")); });

  // Physical-merged tree (column 4 = physical id).
  writeAndCheckRoundTrip(*baseline, infomap::test::repoPath("examples/networks/states.net"), false, "states_physical");
  // State-level tree (column 4 = state id, column 5 = physical id).
  writeAndCheckRoundTrip(*baseline, infomap::test::repoPath("examples/networks/states.net"), true, "states_state");
}

TEST_CASE("Tree round-trip reproduces codelength on a multilayer network [fast][core][partition][parser][lifecycle][columnar-contract]")
{
  auto baseline = infomap::test::makeRunningInfomap(
      [](InfomapWrapper& im) { im.readInputData(infomap::test::repoPath("examples/networks/multilayer.net")); });

  // State-level tree carries layer ids; physical-merged tree on multilayer is
  // ambiguous when the same physical id appears in multiple layers, so we only
  // exercise the recoverable variant.
  writeAndCheckRoundTrip(*baseline, infomap::test::repoPath("examples/networks/multilayer.net"), true, "multilayer_state");
}

TEST_CASE("Tree cluster-data tolerates repeated reinit on the same higher-order instance [fast][core][partition][lifecycle][columnar-contract]")
{
  auto baseline = infomap::test::makeRunningInfomap(
      [](InfomapWrapper& im) { im.readInputData(infomap::test::repoPath("examples/networks/states.net")); });

  const auto treePath = scratchTreePath("states_lifecycle_state");
  baseline->writeTree(treePath, true);

  InfomapWrapper im(infomap::test::defaultFlags("--num-trials 5 --cluster-data " + treePath));
  im.readInputData(infomap::test::repoPath("examples/networks/states.net"));
  im.run();

  infomap::test::checkRunSanity(im);
  const auto firstCodelength = im.codelength();

  im.run();
  infomap::test::checkRunSanity(im);
  CHECK(im.codelength() == doctest::Approx(firstCodelength));

  std::remove(treePath.c_str());
}

TEST_CASE("No-infomap tree cluster-data reruns preserve loaded codelength [fast][core][partition][lifecycle][columnar-contract]")
{
  auto baseline = infomap::test::makeRunningInfomap(
      [](InfomapWrapper& im) { im.readInputData(infomap::test::repoPath("examples/networks/ninetriangles.net")); },
      "--num-trials 5");

  const auto treePath = scratchTreePath("ninetriangles_no_infomap_lifecycle");
  baseline->writeTree(treePath);
  const auto expectedCodelength = baseline->codelength();
  const auto expectedIndexCodelength = baseline->getIndexCodelength();

  InfomapWrapper im(infomap::test::defaultFlags("--no-infomap --cluster-data " + treePath));
  im.readInputData(infomap::test::repoPath("examples/networks/ninetriangles.net"));
  im.run();

  infomap::test::checkRunSanity(im);
  infomap::test::checkApproxCodelength(im.codelength(), expectedCodelength);
  infomap::test::checkApproxCodelength(im.getIndexCodelength(), expectedIndexCodelength);

  im.run();
  infomap::test::checkRunSanity(im);
  infomap::test::checkApproxCodelength(im.codelength(), expectedCodelength);
  infomap::test::checkApproxCodelength(im.getIndexCodelength(), expectedIndexCodelength);

  std::remove(treePath.c_str());
}

// Invariant: whenever Infomap ends up with a single top module, numNonTrivialTopModules()
// must be 0. Before the fix in partition(), the one-level fallback path (triggered when
// the found codelength is worse than the one-level codelength) left m_numNonTrivialTopModules
// at its pre-fallback value instead of recalculating it.
TEST_CASE("numNonTrivialTopModules is zero when all nodes are in one module [fast][core][partition][columnar-contract]")
{
  InfomapWrapper im(infomap::test::defaultFlags());
  // Complete graph K5 — no community structure, one-level codelength is optimal.
  // The optimizer converges directly to one module without needing the codelength-comparison
  // fallback, so this test covers the invariant rather than the specific fallback code path.
  im.addLink(1, 2);
  im.addLink(1, 3);
  im.addLink(1, 4);
  im.addLink(1, 5);
  im.addLink(2, 3);
  im.addLink(2, 4);
  im.addLink(2, 5);
  im.addLink(3, 4);
  im.addLink(3, 5);
  im.addLink(4, 5);
  im.run();

  CHECK(im.numTopModules() == 1);
  CHECK(im.numNonTrivialTopModules() == 0);
  CHECK(im.codelength() == doctest::Approx(im.getOneLevelCodelength()));
  infomap::test::checkRunSanity(im);
  infomap::test::checkCanonicalPartition(im, { { 1, 2, 3, 4, 5 } });
}

// #827: --preferred-number-of-modules is wired into the columnar engine as the
// |K - K_pref| move-loop bias (PreferredModulesCorrection), the columnar
// analogue of BiasedMapEquation. On the OO path this flag was dead under
// --columnar. Here we verify the columnar two-level search is steered to the
// requested module count, and that the flag-off run is unaffected.
TEST_CASE("Columnar --preferred-number-of-modules steers the two-level module count [fast][core][partition][columnar]")
{
  // Four 5-cliques in a ring joined by single bridge edges: the unbiased
  // two-level optimum is the four natural cliques.
  auto build = [](InfomapWrapper& im) {
    constexpr int C = 4, S = 5;
    auto nid = [](int c, int i) { return c * S + i + 1; };
    for (int c = 0; c < C; ++c)
      for (int i = 0; i < S; ++i)
        for (int j = i + 1; j < S; ++j)
          im.addLink(nid(c, i), nid(c, j));
    for (int c = 0; c < C; ++c)
      im.addLink(nid(c, 0), nid((c + 1) % C, 0));
  };
  auto topModules = [&](const std::string& extra) {
    InfomapWrapper im("--seed 123 --num-trials 1 --silent --two-level --columnar " + extra);
    build(im);
    im.run();
    return im.numTopModules();
  };

  CHECK(topModules("") == 4); // unbiased: the four natural cliques
  CHECK(topModules("--preferred-number-of-modules 1") == 1); // merge past structure
  CHECK(topModules("--preferred-number-of-modules 2") == 2);
  CHECK(topModules("--preferred-number-of-modules 3") == 3);
  CHECK(topModules("--preferred-number-of-modules 6") == 6); // split the cliques
}

TEST_CASE("--num-threads bounds parallel-trials workers without changing the result [fast][core][threads]")
{
  // Parallel trials reseed per trial (seed = base + trialIndex), so the result is
  // invariant to the worker count. --num-threads only bounds how many workers run.
  // Therefore a 1-thread budget and a 4-thread budget must produce the same best
  // result, which confirms omp_set_num_threads(B) propagates into the worker pool
  // without altering the per-trial RNG sequence.
  const char* baseFlags = "--parallel-trials --num-trials 4 --seed 123 --no-file-output --silent";
  InfomapWrapper oneThread(std::string("--num-threads 1 ") + baseFlags);
  InfomapWrapper fourThreads(std::string("--num-threads 4 ") + baseFlags);
  for (InfomapWrapper* im : { &oneThread, &fourThreads }) {
    im->addLink(0, 1);
    im->addLink(1, 2);
    im->addLink(2, 0);
    im->addLink(2, 3);
    im->addLink(3, 4);
    im->addLink(4, 5);
    im->addLink(5, 3);
    im->run();
  }
  CHECK(oneThread.codelength() == doctest::Approx(fourThreads.codelength()));
  CHECK(oneThread.numTopModules() == fourThreads.numTopModules());
}

// Light smoke test for the offset path: both offset-0 and offset-1 should
// converge to a valid positive codelength on a small network.
// Full split-then-merge determinism (sharded+merged == single run) is
// verified in Phase F.
TEST_CASE("--trial-offset produces a valid codelength [fast][core][partition][sharding]")
{
  const char* baseFlags = "--seed 123 --num-trials 1 --silent --no-file-output";
  for (const char* flags : { baseFlags, "--seed 123 --num-trials 1 --silent --no-file-output --trial-offset 1" }) {
    InfomapWrapper im(flags);
    im.addLink(0, 1);
    im.addLink(1, 2);
    im.addLink(2, 0);
    im.addLink(2, 3);
    im.addLink(3, 4);
    im.addLink(4, 5);
    im.addLink(5, 3);
    im.run();
    CHECK(im.codelength() > 0.0);
    infomap::test::checkRunSanity(im);
  }
}

TEST_CASE("Sharding-mode serial reseed makes trial i reproducible by global index [fast][core][merge]")
{
  auto runTrialAt = [](unsigned int offset) {
    // Sharding mode is active because --trial-offset > 0, which triggers the
    // per-trial reseed (seed = base + global index). No files are written, so
    // the test is portable and needs no cleanup.
    InfomapWrapper im("--silent --seed 99 --num-trials 1 --trial-offset " + std::to_string(offset)
                      + " --no-file-output");
    im.addLink(0, 1);
    im.addLink(1, 2);
    im.addLink(2, 0);
    im.addLink(2, 3);
    im.addLink(3, 4);
    im.addLink(4, 5);
    im.addLink(5, 3);
    im.run();
    return im.codelength();
  };
  // Two single-trial shards at the SAME global index must give identical codelength
  CHECK(runTrialAt(2) == doctest::Approx(runTrialAt(2)));
  // ...and the path runs without error at a nonzero offset.
  CHECK(runTrialAt(5) > 0.0);
}

// The per-trial seed contract (#905): trial i of a run is the trial with seed
// base + trialOffset + i, whichever mode produced it, and nothing that only
// affects output may change which trials run.
//
// These need a network whose trials actually differ. A plain cycle has many
// near-degenerate partitions -- where the module boundaries fall depends on the
// random visit order -- so its per-trial codelengths spread out. The shipped
// fixtures all converge to the same optimum on every trial and would satisfy
// every check below vacuously.
namespace seedcontract {

constexpr unsigned int numCycleNodes = 60;
constexpr unsigned int numTrials = 8;
constexpr unsigned int baseSeed = 3;

std::vector<double> runCycle(const std::string& flags)
{
  InfomapWrapper im("--silent --no-file-output " + flags);
  for (unsigned int i = 0; i < numCycleNodes; ++i) {
    im.addLink(i, (i + 1) % numCycleNodes);
  }
  im.run();
  return im.codelengths();
}

std::string seedAndTrials(unsigned long seed, unsigned int trials)
{
  return "--seed " + std::to_string(seed) + " --num-trials " + std::to_string(trials);
}

} // namespace seedcontract

TEST_CASE("Every trial is reproducible from the seed reported for it [fast][core][partition][sharding][merge]")
{
  using namespace seedcontract;

  const auto sequential = runCycle(seedAndTrials(baseSeed, numTrials));
  REQUIRE(sequential.size() == numTrials);
  // Precondition: the trials differ, so a broken seeding regime can be detected
  // at all. Without it every comparison below would pass on any implementation.
  REQUIRE(std::set<double>(sequential.begin(), sequential.end()).size() > 1);

  SUBCASE("a standalone run at seed base+i reproduces trial i")
  {
    // Before the fix the default serial path ran one continuous RNG stream
    // through all trials, so the seed reported for trial i named a seed that
    // trial never used.
    for (unsigned int i = 0; i < numTrials; ++i) {
      const auto standalone = runCycle(seedAndTrials(baseSeed + i, 1));
      REQUIRE(standalone.size() == 1);
      CHECK(standalone[0] == doctest::Approx(sequential[i]));
    }
  }

  SUBCASE("a shard at global index i reproduces trial i")
  {
    // Sharded serial trials used to reseed only the engine and leave
    // Config::seedToRandomNumberGenerator at the base seed, so every
    // sub-Infomap of the recursive partition kept seeding from the base.
    for (unsigned int i = 0; i < numTrials; ++i) {
      const auto shard = runCycle(seedAndTrials(baseSeed, 1) + " --trial-offset " + std::to_string(i));
      REQUIRE(shard.size() == 1);
      CHECK(shard[0] == doctest::Approx(sequential[i]));
    }
  }

  SUBCASE("--parallel-trials runs the same trials as the sequential path")
  {
    const auto parallel = runCycle(seedAndTrials(baseSeed, numTrials) + " --parallel-trials");
    REQUIRE(parallel.size() == numTrials);
    for (unsigned int i = 0; i < numTrials; ++i) {
      CHECK(parallel[i] == doctest::Approx(sequential[i]));
    }
  }

  SUBCASE("shards partition the sequential run's trials")
  {
    // The equivalence interfaces/python/source/workflows/hpc.md promises: any
    // partition of [0, N) reproduces the trials of a single --num-trials N run.
    std::vector<double> merged;
    for (unsigned int offset = 0; offset < numTrials; offset += 2) {
      const auto shard = runCycle(seedAndTrials(baseSeed, 2) + " --trial-offset " + std::to_string(offset));
      REQUIRE(shard.size() == 2);
      merged.insert(merged.end(), shard.begin(), shard.end());
    }
    REQUIRE(merged.size() == numTrials);
    for (unsigned int i = 0; i < numTrials; ++i) {
      CHECK(merged[i] == doctest::Approx(sequential[i]));
    }
  }

  SUBCASE("the contract survives a base seed wider than the engine")
  {
    // Every route into the RNG takes unsigned int (BasicRandom's constructor,
    // Random::seed from reseed and from setNonMainConfig), so the engine only
    // ever sees the low 32 bits of Config::seedToRandomNumberGenerator. Where
    // that field is wider, the narrowing has to stay uniform, or the seed a run
    // reports would name a trial it cannot reproduce.
    //
    // Guarded because the field is `unsigned long`, whose width is not fixed:
    // 64-bit on LP64 (Linux, macOS), 32-bit on LLP64 (Windows). On Windows it is
    // exactly the engine's width, no seed above UINT_MAX is representable, and
    // there is nothing here to test -- running it anyway wraps the base and asks
    // for `--seed 0`, which the parser rejects (min is 1).
    if (sizeof(unsigned long) > sizeof(unsigned int)) {
      // Derived rather than written as a literal or a shift: `1ul << 32` is
      // undefined where unsigned long is 32 bits, even on the branch not taken.
      const unsigned long twoPow32 = static_cast<unsigned long>(std::numeric_limits<unsigned int>::max()) + 1ul;
      // Straddles the wrap: trial 2 lands on 2^32 and narrows to engine seed 0.
      const unsigned long wideBase = twoPow32 - 2ul;

      const auto wide = runCycle(seedAndTrials(wideBase, numTrials));
      REQUIRE(wide.size() == numTrials);
      REQUIRE(std::set<double>(wide.begin(), wide.end()).size() > 1);

      for (unsigned int i = 0; i < numTrials; ++i) {
        const auto standalone = runCycle(seedAndTrials(wideBase + i, 1));
        REQUIRE(standalone.size() == 1);
        CHECK(standalone[0] == doctest::Approx(wide[i]));
      }

      // The aliasing the width mismatch implies, pinned so it stays deliberate:
      // seeds congruent mod 2^32 are the same run.
      const auto low = runCycle(seedAndTrials(1ul, 1));
      const auto aliased = runCycle(seedAndTrials(1ul + twoPow32, 1));
      REQUIRE(low.size() == 1);
      REQUIRE(aliased.size() == 1);
      CHECK(aliased[0] == doctest::Approx(low[0]));
    }
  }
}

TEST_CASE("Hierarchical partition is invariant to the OpenMP thread count [fast][core][partition][threads]")
{
  // The recursive partition runs sub-modules as parallel tasks. Results must
  // not depend on task scheduling: sub-Infomaps re-seed from the config seed,
  // so every module's partition is execution-order independent, and per-module
  // statistics are aggregated in a fixed order. Lock that guarantee in by
  // comparing a multi-level run at 1 thread against one at several threads.
  auto runHierarchical = [&]() {
    // ninetriangles yields a three-level solution, giving the recursive
    // phase real hierarchy to check.
    InfomapWrapper im("--seed 1 --num-trials 1 --silent");
    im.readInputData(infomap::test::repoPath("examples/networks/ninetriangles.net"));
    im.run();
    infomap::test::checkRunSanity(im);
    // Precondition: multi-level, so the recursive phase actually ran and the
    // invariance comparison below is meaningful.
    REQUIRE(im.maxTreeDepth() > 2);
    return std::make_tuple(im.getMultilevelModules(false), im.codelength(), im.getIndexCodelength());
  };

#ifdef _OPENMP
  const int previousMaxThreads = omp_get_max_threads();
  omp_set_num_threads(1);
#endif
  const auto serialResult = runHierarchical();

#ifdef _OPENMP
  omp_set_num_threads(4); // Any count >= 2 exercises the task-graph scheduling
#endif
  const auto parallelResult = runHierarchical();

#ifdef _OPENMP
  omp_set_num_threads(previousMaxThreads);
#endif

  CHECK(std::get<0>(parallelResult) == std::get<0>(serialResult));
  // Exact equality on purpose: the task graph preserves floating-point
  // aggregation order, so the codelengths must match bitwise.
  CHECK(std::get<1>(parallelResult) == std::get<1>(serialResult));
  CHECK(std::get<2>(parallelResult) == std::get<2>(serialResult));
}

} // namespace
