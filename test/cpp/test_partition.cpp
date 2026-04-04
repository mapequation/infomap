#include "vendor/doctest.h"

#include "Infomap.h"

#include "TestUtils.h"

#include <map>
#include <set>
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
      flows[{module1, module2}] += edge->data.flow;
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
      flows[{module1, module2}] += edge->data.flow;
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
  infomap::test::checkCanonicalPartition(im, {{1, 2, 3}, {4, 5, 6}});

  im.run();

  infomap::test::checkRunSanity(im);
  infomap::test::checkCanonicalPartition(im, {{1, 2, 3}, {4, 5, 6}});
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
  CHECK(childStateIds(root) == std::vector<unsigned int>{10, 20, 30});

  CHECK(root.collapseChildren() == 3);
  CHECK(root.childDegree() == 0);
  CHECK(root.firstChild == nullptr);
  CHECK(root.lastChild == nullptr);

  CHECK(root.expandChildren() == 3);
  CHECK(root.childDegree() == 3);
  CHECK(childStateIds(root) == std::vector<unsigned int>{10, 20, 30});
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
  CHECK(childStateIds(root) == std::vector<unsigned int>{40, 50});
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
  CHECK(childStateIds(root) == std::vector<unsigned int>{1, 2, 200});
  CHECK(root.firstChild->parent == &root);
  CHECK(root.firstChild->next->parent == &root);
  CHECK(root.lastChild == moduleB);

  CHECK(root.replaceChildrenWithGrandChildren() == 1);
  CHECK(root.childDegree() == 4);
  CHECK(childStateIds(root) == std::vector<unsigned int>{1, 2, 3, 4});
  for (const auto& child : root.children()) {
    CHECK(child.parent == &root);
    CHECK(child.isLeaf());
  }
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
  infomap::test::checkCanonicalPartition(im, {{1, 2, 3}, {4, 5, 6}});
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
  infomap::test::checkCanonicalPartition(im, {{1, 2, 3}, {4, 5, 6}});
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
    infomap::test::checkCanonicalPartition(im, {{1, 2, 3}, {4, 5, 6}});
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

  std::vector<unsigned int> modules{0, 0, 0, 1, 1, 1};
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

} // namespace
