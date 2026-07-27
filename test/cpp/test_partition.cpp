#include "vendor/doctest.h"

#include "Infomap.h"
#include "io/InfomapError.h"

#include "TestUtils.h"

#include <algorithm>
#include <map>
#include <set>
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

TEST_CASE("Cluster-data clu fixture initializes a two-level partition [fast][core][partition]")
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

TEST_CASE("Tree cluster-data fixture initializes a multi-level tree [fast][core][partition]")
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

TEST_CASE("Tree cluster-data reinit and rerun stay stable on the same instance [fast][core][partition][lifecycle]")
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

TEST_CASE("Soft cluster-data can be optimized away when it is suboptimal [fast][core][partition]")
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

TEST_CASE("Hard cluster-data preserves the imposed coarse partition [fast][core][partition]")
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

TEST_CASE("Embedded JSON path seeds the initial partition [fast][core][partition][json]")
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

TEST_CASE("Hard cluster-data reinit and rerun stay stable on the same instance [fast][core][partition][lifecycle]")
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
TEST_CASE("Hard cluster-data rerun preserves leaf metadata on the same instance [fast][core][partition][lifecycle][parser]")
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

TEST_CASE("Tree round-trip reproduces codelength on a regular network [fast][core][partition][parser][lifecycle]")
{
  auto baseline = infomap::test::makeRunningInfomap(
      [](InfomapWrapper& im) { im.readInputData(infomap::test::repoPath("examples/networks/ninetriangles.net")); },
      "--num-trials 5");

  writeAndCheckRoundTrip(*baseline, infomap::test::repoPath("examples/networks/ninetriangles.net"), false, "ninetriangles");
}

TEST_CASE("Tree round-trip reproduces codelength on a higher-order (states) network [fast][core][partition][parser][lifecycle]")
{
  auto baseline = infomap::test::makeRunningInfomap(
      [](InfomapWrapper& im) { im.readInputData(infomap::test::repoPath("examples/networks/states.net")); });

  // Physical-merged tree (column 4 = physical id).
  writeAndCheckRoundTrip(*baseline, infomap::test::repoPath("examples/networks/states.net"), false, "states_physical");
  // State-level tree (column 4 = state id, column 5 = physical id).
  writeAndCheckRoundTrip(*baseline, infomap::test::repoPath("examples/networks/states.net"), true, "states_state");
}

TEST_CASE("Tree round-trip reproduces codelength on a multilayer network [fast][core][partition][parser][lifecycle]")
{
  auto baseline = infomap::test::makeRunningInfomap(
      [](InfomapWrapper& im) { im.readInputData(infomap::test::repoPath("examples/networks/multilayer.net")); });

  // State-level tree carries layer ids; physical-merged tree on multilayer is
  // ambiguous when the same physical id appears in multiple layers, so we only
  // exercise the recoverable variant.
  writeAndCheckRoundTrip(*baseline, infomap::test::repoPath("examples/networks/multilayer.net"), true, "multilayer_state");
}

TEST_CASE("Tree cluster-data tolerates repeated reinit on the same higher-order instance [fast][core][partition][lifecycle]")
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

TEST_CASE("No-infomap tree cluster-data reruns preserve loaded codelength [fast][core][partition][lifecycle]")
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
TEST_CASE("numNonTrivialTopModules is zero when all nodes are in one module [fast][core][partition]")
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
    // Config::seedToRandomNumberGenerator is unsigned long but every route into
    // the RNG takes unsigned int, so the engine only sees the low 32 bits. The
    // narrowing has to stay uniform, or the seed this run reports would name a
    // trial it cannot reproduce. The base is picked so the run straddles 2^32:
    // trial 2 lands exactly on the wrap and narrows to engine seed 0.
    constexpr unsigned long wideBase = 4294967294ul; // 2^32 - 2
    const auto wide = runCycle(seedAndTrials(wideBase, numTrials));
    REQUIRE(wide.size() == numTrials);
    REQUIRE(std::set<double>(wide.begin(), wide.end()).size() > 1);

    for (unsigned int i = 0; i < numTrials; ++i) {
      const auto standalone = runCycle(seedAndTrials(wideBase + i, 1));
      REQUIRE(standalone.size() == 1);
      CHECK(standalone[0] == doctest::Approx(wide[i]));
    }

    // The aliasing this width mismatch implies, pinned so it stays deliberate:
    // seeds congruent mod 2^32 are the same run.
    const auto low = runCycle(seedAndTrials(1, 1));
    const auto aliased = runCycle(seedAndTrials(1ul + (1ul << 32), 1));
    REQUIRE(low.size() == 1);
    REQUIRE(aliased.size() == 1);
    CHECK(aliased[0] == doctest::Approx(low[0]));
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
