#include "vendor/doctest.h"

#include "Infomap.h"

#include "TestUtils.h"

#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <vector>

namespace {

using infomap::InfomapWrapper;
using infomap::FlowData;
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

std::vector<unsigned int> childChainStateIds(const InfoNode* firstChild)
{
  std::vector<unsigned int> ids;
  for (const auto* child = firstChild; child != nullptr; child = child->nextSibling()) {
    ids.push_back(child->stateId);
  }
  return ids;
}

void deleteDetachedChildChain(InfoNode* firstChild)
{
  while (firstChild != nullptr) {
    auto* next = firstChild->nextSibling();
    firstChild->clearDetachedLinks();
    delete firstChild;
    firstChild = next;
  }
}

void checkLinkedChildOrder(InfoNode& parent, const std::vector<unsigned int>& expectedStateIds)
{
  CHECK(childStateIds(parent) == expectedStateIds);
  CHECK(parent.childDegree() == expectedStateIds.size());

  InfoNode* previous = nullptr;
  InfoNode* child = parent.firstChildNode();
  std::size_t index = 0;
  while (child != nullptr) {
    REQUIRE(index < expectedStateIds.size());
    CHECK(child->stateId == expectedStateIds[index]);
    CHECK(child->parentNode() == &parent);
    CHECK(child->previousSibling() == previous);
    if (previous != nullptr) {
      CHECK(previous->nextSibling() == child);
    }
    previous = child;
    child = child->nextSibling();
    ++index;
  }

  CHECK(index == expectedStateIds.size());
  CHECK(parent.lastChildNode() == previous);
  if (previous != nullptr) {
    CHECK(previous->nextSibling() == nullptr);
  }
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
  CHECK(root.firstChildNode() == nullptr);
  CHECK(root.lastChildNode() == nullptr);

  CHECK(root.expandChildren() == 3);
  CHECK(root.childDegree() == 3);
  CHECK(childStateIds(root) == std::vector<unsigned int> { 10, 20, 30 });
  CHECK(childA->parentNode() == &root);
  CHECK(childB->parentNode() == &root);
  CHECK(childC->parentNode() == &root);

  root.releaseChildren();
  CHECK(root.childDegree() == 0);
  CHECK(root.firstChildNode() == nullptr);
  CHECK(root.lastChildNode() == nullptr);

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

TEST_CASE("InfoNode unique_ptr child handoff preserves raw child-chain semantics [fast][core][partition][tree][ownership]")
{
  InfoNode root;
  auto childA = std::make_unique<InfoNode>(FlowData {}, 10);
  auto childB = std::make_unique<InfoNode>(FlowData {}, 20);

  auto* childAPtr = childA.get();
  auto* childBPtr = childB.get();

  InfoNode& returnedA = root.addChild(std::move(childA));
  InfoNode& returnedB = root.addChild(std::move(childB));

  CHECK(&returnedA == childAPtr);
  CHECK(&returnedB == childBPtr);
  CHECK(childA == nullptr);
  CHECK(childB == nullptr);
  CHECK(root.childDegree() == 2);
  CHECK(root.firstChildNode() == childAPtr);
  CHECK(root.lastChildNode() == childBPtr);
  CHECK(childAPtr->parentNode() == &root);
  CHECK(childBPtr->parentNode() == &root);
  CHECK(childAPtr->previousSibling() == nullptr);
  CHECK(childAPtr->nextSibling() == childBPtr);
  CHECK(childBPtr->previousSibling() == childAPtr);
  CHECK(childBPtr->nextSibling() == nullptr);
  CHECK(childStateIds(root) == std::vector<unsigned int> { 10, 20 });

  childAPtr->addChild(std::make_unique<InfoNode>(FlowData {}, 11));
  CHECK(childAPtr->childDegree() == 1);
}

TEST_CASE("InfoNode raw addChild reparents child ownership between parents [fast][core][partition][tree][ownership]")
{
  InfoNode oldParent;
  InfoNode newParent;
  auto* child = &oldParent.addChild(std::make_unique<InfoNode>(FlowData {}, 10));
  oldParent.addChild(std::make_unique<InfoNode>(FlowData {}, 20));

  newParent.addChild(child);

  CHECK(childStateIds(oldParent) == std::vector<unsigned int> { 20 });
  CHECK(childStateIds(newParent) == std::vector<unsigned int> { 10 });
  CHECK(child->parentNode() == &newParent);
  CHECK(child->previousSibling() == nullptr);
  CHECK(child->nextSibling() == nullptr);

  oldParent.deleteChildren();

  CHECK(child->stateId == 10);
  CHECK(newParent.firstChildNode() == child);
  CHECK(newParent.lastChildNode() == child);
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
  CHECK(root.firstChildNode() == nullptr);
  CHECK(root.lastChildNode() == nullptr);
  CHECK(childA->stateId == 10);
  CHECK(childB->stateId == 20);
  CHECK(childA->parentNode() == &root);
  CHECK(childB->parentNode() == &root);
  CHECK(childA->nextSibling() == childB);
  CHECK(childB->previousSibling() == childA);

  delete childA;
  delete childB;
}

TEST_CASE("InfoNode reattaches released children under new unique_ptr ownership [fast][core][partition][tree][ownership]")
{
  InfoNode oldParent;
  InfoNode newParent;
  auto* childA = &oldParent.addChild(std::make_unique<InfoNode>(FlowData {}, 10));
  auto* childB = &oldParent.addChild(std::make_unique<InfoNode>(FlowData {}, 20));

  oldParent.releaseChildren();
  newParent.addChild(childA);
  newParent.addChild(childB);

  CHECK(oldParent.childDegree() == 0);
  CHECK(oldParent.firstChildNode() == nullptr);
  CHECK(oldParent.lastChildNode() == nullptr);
  checkLinkedChildOrder(newParent, { 10, 20 });
}

TEST_CASE("InfoNode detached child handle adopts children in linked-list order [fast][core][partition][tree][ownership]")
{
  InfoNode oldParent;
  InfoNode newParent;
  auto childA = std::make_unique<InfoNode>(FlowData {}, 10);
  auto childB = std::make_unique<InfoNode>(FlowData {}, 20);
  auto childC = std::make_unique<InfoNode>(FlowData {}, 30);
  childA->data.flow = 0.1;
  childB->data.flow = 0.7;
  childC->data.flow = 0.4;

  oldParent.addChild(std::move(childA));
  oldParent.addChild(std::move(childB));
  oldParent.addChild(std::move(childC));
  oldParent.sortChildrenOnFlow(false);

  auto detachedChildren = oldParent.detachChildren();

  CHECK(oldParent.childDegree() == 0);
  CHECK(oldParent.firstChildNode() == nullptr);
  CHECK(oldParent.lastChildNode() == nullptr);
  CHECK(detachedChildren.size() == 3);
  CHECK(childChainStateIds(detachedChildren.first()) == std::vector<unsigned int> { 20, 30, 10 });

  newParent.adoptChildren(std::move(detachedChildren));

  checkLinkedChildOrder(newParent, { 20, 30, 10 });
}

TEST_CASE("InfoNode detached child handle can move one child to another parent [fast][core][partition][tree][ownership]")
{
  InfoNode oldParent;
  InfoNode firstNewParent;
  InfoNode secondNewParent;
  auto* childA = &oldParent.addChild(std::make_unique<InfoNode>(FlowData {}, 10));
  auto* childB = &oldParent.addChild(std::make_unique<InfoNode>(FlowData {}, 20));
  auto* childC = &oldParent.addChild(std::make_unique<InfoNode>(FlowData {}, 30));

  auto detachedChildren = oldParent.detachChildren();
  firstNewParent.addChild(detachedChildren.take(childB));
  secondNewParent.adoptChildren(std::move(detachedChildren));

  checkLinkedChildOrder(firstNewParent, { 20 });
  checkLinkedChildOrder(secondNewParent, { 10, 30 });
  CHECK(childA->parentNode() == &secondNewParent);
  CHECK(childB->parentNode() == &firstNewParent);
  CHECK(childC->parentNode() == &secondNewParent);
}

TEST_CASE("InfoNode adoptChildren appends without reordering existing children [fast][core][partition][tree][ownership]")
{
  InfoNode oldParent;
  InfoNode newParent;
  auto existingA = std::make_unique<InfoNode>(FlowData {}, 10);
  auto existingB = std::make_unique<InfoNode>(FlowData {}, 20);
  existingA->data.flow = 0.1;
  existingB->data.flow = 0.7;
  newParent.addChild(std::move(existingA));
  newParent.addChild(std::move(existingB));
  newParent.sortChildrenOnFlow(false);

  oldParent.addChild(std::make_unique<InfoNode>(FlowData {}, 30));
  oldParent.addChild(std::make_unique<InfoNode>(FlowData {}, 40));

  newParent.adoptChildren(oldParent.detachChildren());

  checkLinkedChildOrder(newParent, { 20, 10, 30, 40 });
}

TEST_CASE("InfoNode detached child handle deletes dropped children [fast][core][partition][tree][ownership]")
{
  InfoNode oldParent;
  oldParent.addChild(std::make_unique<InfoNode>(FlowData {}, 10));
  oldParent.addChild(std::make_unique<InfoNode>(FlowData {}, 20));

  {
    auto detachedChildren = oldParent.detachChildren();
    CHECK(detachedChildren.size() == 2);
    CHECK(oldParent.childDegree() == 0);
  }

  CHECK(oldParent.childDegree() == 0);
  CHECK(oldParent.firstChildNode() == nullptr);
  CHECK(oldParent.lastChildNode() == nullptr);
  oldParent.deleteChildren();
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
  CHECK(im.root().collapsedFirstChildNode() == nullptr);
  CHECK(im.root().collapsedLastChildNode() == nullptr);
}

TEST_CASE("InfoNode deleteChildren also clears collapsed children [fast][core][partition][tree]")
{
  InfoNode root;
  root.addChild(std::make_unique<InfoNode>(FlowData {}, 10));
  root.addChild(std::make_unique<InfoNode>(FlowData {}, 20));

  CHECK(root.collapseChildren() == 2);
  CHECK(root.childDegree() == 0);
  CHECK(root.firstChildNode() == nullptr);
  CHECK(root.collapsedFirstChildNode() != nullptr);

  root.deleteChildren();

  CHECK(root.childDegree() == 0);
  CHECK(root.firstChildNode() == nullptr);
  CHECK(root.lastChildNode() == nullptr);
  CHECK(root.collapsedFirstChildNode() == nullptr);
  CHECK(root.collapsedLastChildNode() == nullptr);
}

TEST_CASE("InfoNode deleteChildren clears both active and collapsed child chains [fast][core][partition][tree][ownership]")
{
  InfoNode root;
  root.addChild(std::make_unique<InfoNode>(FlowData {}, 10));
  root.addChild(std::make_unique<InfoNode>(FlowData {}, 20));

  CHECK(root.collapseChildren() == 2);
  root.addChild(std::make_unique<InfoNode>(FlowData {}, 30));

  root.deleteChildren();

  CHECK(root.childDegree() == 0);
  CHECK(root.firstChildNode() == nullptr);
  CHECK(root.lastChildNode() == nullptr);
  CHECK(root.collapsedFirstChildNode() == nullptr);
  CHECK(root.collapsedLastChildNode() == nullptr);
}

TEST_CASE("InfoNode copy constructor creates detached shallow copies [fast][core][partition][tree][ownership]")
{
  InfoNode source;
  source.index = 7;
  source.stateId = 100;
  source.physicalId = 200;
  source.layerId = 3;
  source.metaData = { 1, 2 };
  source.codelength = 1.25;
  source.dirty = true;
  source.stateNodes = { 100, 101 };
  source.setNumLeafNodes(2);
  auto* sourceEdgeTarget = new InfoNode(FlowData {}, 99);
  source.addOutEdge(*sourceEdgeTarget, 1.0, 0.5);

  auto* activeChild = new InfoNode({}, 10);
  source.addChild(activeChild);
  source.addChild(std::make_unique<InfoNode>(FlowData {}, 20));

  CHECK(source.collapseChildren() == 2);
  CHECK(source.childDegree() == 0);
  CHECK(source.collapsedFirstChildNode() != nullptr);
  CHECK(source.collapsedLastChildNode() != nullptr);

  InfoNode clone(source);

  CHECK(clone.index == source.index);
  CHECK(clone.stateId == source.stateId);
  CHECK(clone.physicalId == source.physicalId);
  CHECK(clone.layerId == source.layerId);
  CHECK(clone.metaData == source.metaData);
  CHECK(clone.codelength == doctest::Approx(source.codelength));
  CHECK(clone.dirty == source.dirty);
  CHECK(clone.stateNodes == source.stateNodes);
  CHECK(clone.numLeafMembers() == source.numLeafMembers());
  CHECK(clone.childDegree() == 0);
  CHECK(clone.outDegree() == 0);
  CHECK(clone.inDegree() == 0);
  CHECK(clone.owner == nullptr);
  CHECK(clone.parentNode() == nullptr);
  CHECK(clone.previousSibling() == nullptr);
  CHECK(clone.nextSibling() == nullptr);
  CHECK(clone.firstChildNode() == nullptr);
  CHECK(clone.lastChildNode() == nullptr);
  CHECK(clone.collapsedFirstChildNode() == nullptr);
  CHECK(clone.collapsedLastChildNode() == nullptr);

  CHECK(source.collapsedFirstChildNode() == activeChild);
  CHECK(source.collapsedFirstChildNode()->nextSibling()->stateId == 20);

  delete sourceEdgeTarget;
  source.deleteChildren();
}

TEST_CASE("InfoNode copy assignment clears destination ownership and detaches from source [fast][core][partition][tree][ownership]")
{
  InfoNode source({}, 40);
  source.index = 12;
  source.codelength = 2.5;
  source.setNumLeafNodes(3);
  source.addChild(std::make_unique<InfoNode>(FlowData {}, 41));

  InfoNode destination({}, 10);
  destination.addChild(std::make_unique<InfoNode>(FlowData {}, 11));
  auto* destinationEdgeTarget = new InfoNode(FlowData {}, 12);
  destination.addOutEdge(*destinationEdgeTarget, 1.0, 0.5);

  auto* sourceChild = source.firstChildNode();
  destination = source;

  CHECK(destination.stateId == source.stateId);
  CHECK(destination.index == source.index);
  CHECK(destination.codelength == doctest::Approx(source.codelength));
  CHECK(destination.numLeafMembers() == source.numLeafMembers());
  CHECK(destination.childDegree() == 0);
  CHECK(destination.outDegree() == 0);
  CHECK(destination.inDegree() == 0);
  CHECK(destination.parentNode() == nullptr);
  CHECK(destination.previousSibling() == nullptr);
  CHECK(destination.nextSibling() == nullptr);
  CHECK(destination.firstChildNode() == nullptr);
  CHECK(destination.lastChildNode() == nullptr);
  CHECK(destination.collapsedFirstChildNode() == nullptr);
  CHECK(destination.collapsedLastChildNode() == nullptr);

  CHECK(source.firstChildNode() == sourceChild);
  CHECK(source.firstChildNode()->parentNode() == &source);

  delete destinationEdgeTarget;
}

TEST_CASE("InfoNode initClean remains an explicit reset helper [fast][core][partition][tree]")
{
  InfoNode parent;
  auto& source = parent.addChild(std::make_unique<InfoNode>());
  source.addChild(std::make_unique<InfoNode>(FlowData {}, 10));
  source.collapseChildren();

  source.initClean();

  CHECK(source.childDegree() == 0);
  CHECK(source.parentNode() == nullptr);
  CHECK(source.previousSibling() == nullptr);
  CHECK(source.nextSibling() == nullptr);
  CHECK(source.firstChildNode() == nullptr);
  CHECK(source.lastChildNode() == nullptr);
  CHECK(source.collapsedFirstChildNode() == nullptr);
  CHECK(source.collapsedLastChildNode() == nullptr);
}

TEST_CASE("InfoNode replaceChildrenWithOneNode preserves children through guarded detach [fast][core][partition][tree][ownership]")
{
  InfoNode root;
  auto* moduleA = new InfoNode({}, 100);
  auto* moduleB = new InfoNode({}, 200);
  root.addChild(moduleA);
  root.addChild(moduleB);
  moduleA->addChild(std::make_unique<InfoNode>(FlowData {}, 1));
  moduleA->addChild(std::make_unique<InfoNode>(FlowData {}, 2));
  moduleB->addChild(std::make_unique<InfoNode>(FlowData {}, 3));
  moduleB->addChild(std::make_unique<InfoNode>(FlowData {}, 4));

  InfoNode& middle = root.replaceChildrenWithOneNode();

  CHECK(root.childDegree() == 1);
  CHECK(root.firstChildNode() == &middle);
  CHECK(root.lastChildNode() == &middle);
  CHECK(middle.parentNode() == &root);
  CHECK(middle.previousSibling() == nullptr);
  CHECK(middle.nextSibling() == nullptr);
  CHECK(middle.childDegree() == 4);
  CHECK(childStateIds(middle) == std::vector<unsigned int> { 1, 2, 3, 4 });
  for (auto& child : middle.children()) {
    CHECK(child.parentNode() == &middle);
  }
}

TEST_CASE("InfoNode sortChildrenOnFlow preserves links through guarded detach [fast][core][partition][tree][ownership]")
{
  InfoNode root;
  auto childA = std::make_unique<InfoNode>(FlowData {}, 10);
  auto childB = std::make_unique<InfoNode>(FlowData {}, 20);
  auto childC = std::make_unique<InfoNode>(FlowData {}, 30);
  childA->data.flow = 0.1;
  childB->data.flow = 0.7;
  childC->data.flow = 0.4;

  root.addChild(std::move(childA));
  root.addChild(std::move(childB));
  root.addChild(std::move(childC));

  root.sortChildrenOnFlow(false);

  CHECK(root.childDegree() == 3);
  CHECK(childStateIds(root) == std::vector<unsigned int> { 20, 30, 10 });
  CHECK(root.firstChildNode()->previousSibling() == nullptr);
  CHECK(root.firstChildNode()->parentNode() == &root);
  CHECK(root.firstChildNode()->nextSibling()->parentNode() == &root);
  CHECK(root.lastChildNode()->parentNode() == &root);
  CHECK(root.lastChildNode()->nextSibling() == nullptr);
  CHECK(root.firstChildNode()->nextSibling()->previousSibling() == root.firstChildNode());
  CHECK(root.lastChildNode()->previousSibling() == root.firstChildNode()->nextSibling());
}

TEST_CASE("InfoNode replace mutations preserve flattened tree structure [fast][core][partition][tree]")
{
  InfoNode root;
  auto* moduleA = new InfoNode({}, 100);
  auto* moduleB = new InfoNode({}, 200);
  root.addChild(moduleA);
  root.addChild(moduleB);
  moduleA->addChild(std::make_unique<InfoNode>(FlowData {}, 1));
  moduleA->addChild(std::make_unique<InfoNode>(FlowData {}, 2));
  moduleB->addChild(std::make_unique<InfoNode>(FlowData {}, 3));
  moduleB->addChild(std::make_unique<InfoNode>(FlowData {}, 4));

  CHECK(root.childDegree() == 2);
  CHECK(moduleA->childDegree() == 2);
  CHECK(moduleB->childDegree() == 2);

  CHECK(moduleA->replaceWithChildren() == 1);
  CHECK(root.childDegree() == 3);
  CHECK(childStateIds(root) == std::vector<unsigned int> { 1, 2, 200 });
  CHECK(root.firstChildNode()->parentNode() == &root);
  CHECK(root.firstChildNode()->nextSibling()->parentNode() == &root);
  CHECK(root.lastChildNode() == moduleB);

  CHECK(root.replaceChildrenWithGrandChildren() == 1);
  CHECK(root.childDegree() == 4);
  CHECK(childStateIds(root) == std::vector<unsigned int> { 1, 2, 3, 4 });
  for (const auto& child : root.children()) {
    CHECK(child.parentNode() == &root);
    CHECK(child.isLeaf());
  }
}

TEST_CASE("InfoNode replaceWithChildren reparents a first child chain before deleting the module [fast][core][partition][tree][ownership]")
{
  InfoNode root;
  auto* module = new InfoNode({}, 20);
  auto* after = new InfoNode({}, 30);
  root.addChild(module);
  root.addChild(after);
  module->addChild(std::make_unique<InfoNode>(FlowData {}, 1));
  module->addChild(std::make_unique<InfoNode>(FlowData {}, 2));

  CHECK(module->replaceWithChildren() == 1);

  checkLinkedChildOrder(root, { 1, 2, 30 });
  CHECK(root.firstChildNode()->previousSibling() == nullptr);
  CHECK(root.lastChildNode() == after);
  CHECK(after->previousSibling()->stateId == 2);
}

TEST_CASE("InfoNode replaceWithChildren reparents a last child chain before deleting the module [fast][core][partition][tree][ownership]")
{
  InfoNode root;
  auto* before = new InfoNode({}, 10);
  auto* module = new InfoNode({}, 20);
  root.addChild(before);
  root.addChild(module);
  module->addChild(std::make_unique<InfoNode>(FlowData {}, 1));
  module->addChild(std::make_unique<InfoNode>(FlowData {}, 2));

  CHECK(module->replaceWithChildren() == 1);

  checkLinkedChildOrder(root, { 10, 1, 2 });
  CHECK(root.firstChildNode() == before);
  CHECK(root.lastChildNode()->stateId == 2);
  CHECK(root.lastChildNode()->nextSibling() == nullptr);
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
  module->addChild(std::make_unique<InfoNode>(FlowData {}, 1));
  module->addChild(std::make_unique<InfoNode>(FlowData {}, 2));

  CHECK(module->replaceWithChildren() == 1);

  checkLinkedChildOrder(root, { 10, 1, 2, 30 });
  CHECK(root.firstChildNode() == before);
  CHECK(root.lastChildNode() == after);
  CHECK(before->nextSibling()->stateId == 1);
  CHECK(before->nextSibling()->parentNode() == &root);
  CHECK(before->nextSibling()->nextSibling()->stateId == 2);
  CHECK(before->nextSibling()->nextSibling()->parentNode() == &root);
  CHECK(before->nextSibling()->nextSibling()->nextSibling() == after);
  CHECK(after->previousSibling()->stateId == 2);
}

TEST_CASE("InfoNode replaceChildWithChildren performs parent-owned reparenting [fast][core][partition][tree][ownership]")
{
  InfoNode root;
  auto* before = new InfoNode({}, 10);
  auto* module = new InfoNode({}, 20);
  auto* after = new InfoNode({}, 30);
  root.addChild(before);
  root.addChild(module);
  root.addChild(after);
  module->addChild(std::make_unique<InfoNode>(FlowData {}, 1));
  module->addChild(std::make_unique<InfoNode>(FlowData {}, 2));

  CHECK(root.replaceChildWithChildren(*module) == 1);

  checkLinkedChildOrder(root, { 10, 1, 2, 30 });
  CHECK(root.childDegree() == 4);
  CHECK(before->nextSibling()->stateId == 1);
  CHECK(after->previousSibling()->stateId == 2);
}

TEST_CASE("InfoNode replaceWithChildrenDebug uses the same reparent splice path [fast][core][partition][tree][ownership]")
{
  InfoNode root;
  auto* before = new InfoNode({}, 10);
  auto* module = new InfoNode({}, 20);
  auto* after = new InfoNode({}, 30);
  root.addChild(before);
  root.addChild(module);
  root.addChild(after);
  module->addChild(std::make_unique<InfoNode>(FlowData {}, 1));
  module->addChild(std::make_unique<InfoNode>(FlowData {}, 2));

  module->replaceWithChildrenDebug();

  checkLinkedChildOrder(root, { 10, 1, 2, 30 });
  CHECK(root.firstChildNode() == before);
  CHECK(root.lastChildNode() == after);
}

TEST_CASE("InfoNode remove documents current child-chain ownership semantics [fast][core][partition][tree][ownership]")
{
  InfoNode deleteRoot;
  auto* deletingParent = new InfoNode({}, 10);
  deletingParent->addChild(std::make_unique<InfoNode>(FlowData {}, 11));
  deletingParent->addChild(std::make_unique<InfoNode>(FlowData {}, 12));
  deleteRoot.addChild(deletingParent);

  deletingParent->remove(false);

  CHECK(deleteRoot.firstChildNode() == nullptr);
  CHECK(deleteRoot.lastChildNode() == nullptr);

  InfoNode detachRoot;
  auto* detachingParent = new InfoNode({}, 20);
  auto* detachedChild = new InfoNode({}, 21);
  auto* detachedSibling = new InfoNode({}, 22);
  detachingParent->addChild(detachedChild);
  detachingParent->addChild(detachedSibling);
  detachRoot.addChild(detachingParent);

  detachingParent->remove(true);

  CHECK(detachRoot.firstChildNode() == nullptr);
  CHECK(detachRoot.lastChildNode() == nullptr);
  CHECK(childChainStateIds(detachedChild) == std::vector<unsigned int> { 21, 22 });
  CHECK(detachedChild->previousSibling() == nullptr);
  CHECK(detachedChild->nextSibling() == detachedSibling);
  CHECK(detachedSibling->previousSibling() == detachedChild);
  CHECK(detachedSibling->nextSibling() == nullptr);

  deleteDetachedChildChain(detachedChild);
}

TEST_CASE("InfoNode remove true unlinks first and last child modules while detaching their children [fast][core][partition][tree][ownership]")
{
  InfoNode firstRoot;
  auto* firstModule = new InfoNode({}, 10);
  auto* firstSibling = new InfoNode({}, 20);
  auto* firstDetachedChild = new InfoNode({}, 11);
  firstModule->addChild(firstDetachedChild);
  firstRoot.addChild(firstModule);
  firstRoot.addChild(firstSibling);

  firstModule->remove(true);

  CHECK(childStateIds(firstRoot) == std::vector<unsigned int> { 20 });
  CHECK(firstRoot.firstChildNode() == firstSibling);
  CHECK(firstRoot.lastChildNode() == firstSibling);
  CHECK(firstSibling->previousSibling() == nullptr);
  CHECK(firstSibling->nextSibling() == nullptr);
  deleteDetachedChildChain(firstDetachedChild);

  InfoNode lastRoot;
  auto* lastSibling = new InfoNode({}, 30);
  auto* lastModule = new InfoNode({}, 40);
  auto* lastDetachedChild = new InfoNode({}, 41);
  lastModule->addChild(lastDetachedChild);
  lastRoot.addChild(lastSibling);
  lastRoot.addChild(lastModule);

  lastModule->remove(true);

  CHECK(childStateIds(lastRoot) == std::vector<unsigned int> { 30 });
  CHECK(lastRoot.firstChildNode() == lastSibling);
  CHECK(lastRoot.lastChildNode() == lastSibling);
  CHECK(lastSibling->previousSibling() == nullptr);
  CHECK(lastSibling->nextSibling() == nullptr);
  deleteDetachedChildChain(lastDetachedChild);
}

TEST_CASE("InfoNode removeChild unlinks parent and sibling pointers through owner API [fast][core][partition][tree][ownership]")
{
  InfoNode root;
  auto* first = new InfoNode({}, 10);
  auto* middle = new InfoNode({}, 20);
  auto* last = new InfoNode({}, 30);
  root.addChild(first);
  root.addChild(middle);
  root.addChild(last);

  root.removeChild(*middle, InfoNode::RemoveChildrenPolicy::DeleteSubtree);

  CHECK(childStateIds(root) == std::vector<unsigned int> { 10, 30 });
  CHECK(root.firstChildNode() == first);
  CHECK(root.lastChildNode() == last);
  CHECK(first->previousSibling() == nullptr);
  CHECK(first->nextSibling() == last);
  CHECK(last->previousSibling() == first);
  CHECK(last->nextSibling() == nullptr);
  CHECK(root.childDegree() == 2);
}

TEST_CASE("InfoNode removeChild unlinks first and last children through owner API [fast][core][partition][tree][ownership]")
{
  InfoNode firstRoot;
  auto* first = new InfoNode({}, 10);
  auto* second = new InfoNode({}, 20);
  firstRoot.addChild(first);
  firstRoot.addChild(second);

  firstRoot.removeChild(*first, InfoNode::RemoveChildrenPolicy::DeleteSubtree);

  CHECK(childStateIds(firstRoot) == std::vector<unsigned int> { 20 });
  CHECK(firstRoot.firstChildNode() == second);
  CHECK(firstRoot.lastChildNode() == second);
  CHECK(second->previousSibling() == nullptr);
  CHECK(second->nextSibling() == nullptr);
  CHECK(firstRoot.childDegree() == 1);

  InfoNode lastRoot;
  auto* beforeLast = new InfoNode({}, 30);
  auto* last = new InfoNode({}, 40);
  lastRoot.addChild(beforeLast);
  lastRoot.addChild(last);

  lastRoot.removeChild(*last, InfoNode::RemoveChildrenPolicy::DeleteSubtree);

  CHECK(childStateIds(lastRoot) == std::vector<unsigned int> { 30 });
  CHECK(lastRoot.firstChildNode() == beforeLast);
  CHECK(lastRoot.lastChildNode() == beforeLast);
  CHECK(beforeLast->previousSibling() == nullptr);
  CHECK(beforeLast->nextSibling() == nullptr);
  CHECK(lastRoot.childDegree() == 1);
}

TEST_CASE("InfoNode owns outgoing edges while incoming edges are non-owning references [fast][core][partition][tree][ownership]")
{
  auto* source = new InfoNode({}, 10);
  InfoNode target({}, 20);
  source->addOutEdge(target, 1.0, 0.5);

  CHECK(source->outDegree() == 1);
  CHECK(target.inDegree() == 1);
  CHECK(*source->outEdges().begin() == *target.inEdges().begin());

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

} // namespace
