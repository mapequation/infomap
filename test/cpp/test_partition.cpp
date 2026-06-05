#include "vendor/doctest.h"

#include "Infomap.h"

#include "TestUtils.h"

#include <map>
#include <set>
#include <sstream>
#include <tuple>
#include <vector>

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
  const auto numLevels = infomap::printPerLevelCodelength(im.root(), output, true);
  const auto text = output.str();

  CHECK(numLevels >= 2);
  CHECK(text.find("Levels") != std::string::npos);
  CHECK(text.find("Level  Modules  Leaves  Avg degree  Module bits  Leaf bits  Total bits") != std::string::npos);
  CHECK(text.find("Total") != std::string::npos);
  CHECK(text.find("Per level number of modules") == std::string::npos);
  CHECK(text.find("2.700302") != std::string::npos);
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
      CHECK_FALSE(leaf->metaData.empty());
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
    // sharding mode is active because --trial-results is set
    InfomapWrapper im("--silent --seed 99 --num-trials 1 --trial-offset " + std::to_string(offset)
                      + " --trial-results /tmp/infomap_reseed_probe.json --no-final-output --no-file-output");
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

} // namespace
