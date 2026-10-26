#include "TestUtils.h"
#include "core/ColumnarStack.h"

#include <cstdint>
#include <tuple>
#include <vector>

// ColumnarStack, the columnar search's stacked hierarchy, as a building block of
// its own: the edits keep every level the aggregate of the level under it, and
// checkpoints undo any sequence of edits, nested or not, including edits that
// change the number of levels.

namespace infomap {
namespace test {

namespace {

  // Two directed 4-cycles with chords, joined by one link each way: leaves 0-3 and
  // 4-7. Only what aggregate reads is filled in: flow and the out-edge CSR.
  ColumnarLevel twoGroupLeaves()
  {
    const std::vector<std::tuple<int, int, double>> links = {
      { 0, 1, 0.05 }, { 1, 2, 0.05 }, { 2, 3, 0.05 }, { 3, 0, 0.05 }, { 0, 2, 0.04 }, { 1, 3, 0.04 },
      { 4, 5, 0.05 }, { 5, 6, 0.05 }, { 6, 7, 0.05 }, { 7, 4, 0.05 }, { 4, 6, 0.04 }, { 5, 7, 0.04 },
      { 3, 4, 0.01 }, { 7, 0, 0.01 },
    };
    ColumnarLevel leaf;
    leaf.n = 8;
    leaf.flow.assign(8, 0.125);
    leaf.enter.assign(8, 0.0);
    leaf.exit.assign(8, 0.0);
    leaf.outStart.assign(9, 0);
    for (const auto& l : links)
      ++leaf.outStart[std::get<0>(l) + 1];
    for (int i = 0; i < 8; ++i)
      leaf.outStart[i + 1] += leaf.outStart[i];
    leaf.outTarget.assign(links.size(), 0);
    leaf.outFlow.assign(links.size(), 0.0);
    std::vector<int> pos(leaf.outStart.begin(), leaf.outStart.end() - 1);
    for (const auto& l : links) {
      const int at = pos[std::get<0>(l)]++;
      leaf.outTarget[at] = std::get<1>(l);
      leaf.outFlow[at] = std::get<2>(l);
      leaf.exit[std::get<0>(l)] += std::get<2>(l);
      leaf.enter[std::get<1>(l)] += std::get<2>(l);
    }
    leaf.inStart.assign(9, 0);
    return leaf;
  }

  bool sameLevel(const ColumnarLevel& a, const ColumnarLevel& b)
  {
    return a.n == b.n && a.flow == b.flow && a.enter == b.enter && a.exit == b.exit && a.outStart == b.outStart
        && a.outTarget == b.outTarget && a.outFlow == b.outFlow;
  }

  // Same shape, assignments and module levels.
  bool sameStack(const ColumnarStack& a, const ColumnarStack& b)
  {
    if (a.numLevels() != b.numLevels())
      return false;
    for (int k = 1; k < a.numLevels(); ++k)
      if (!sameLevel(a.level(k), b.level(k)) || a.assign(k - 1) != b.assign(k - 1))
        return false;
    return true;
  }

  // leaves -> 4 pairs -> 2 groups -> 1 top module.
  ColumnarStack threeLevelStack(const ColumnarLevel& leaf, const ColumnarBoundaryFlow& flow)
  {
    ColumnarStack stack;
    stack.bind(leaf, flow);
    stack.setFlat({ 0, 0, 1, 1, 2, 2, 3, 3 }, 4);
    stack.pushLevel({ 0, 0, 1, 1 }, 2);
    stack.pushLevel({ 0, 0 }, 1);
    return stack;
  }

} // namespace

TEST_CASE("ColumnarStack builds levels as aggregates of the level below")
{
  const ColumnarLevel leaf = twoGroupLeaves();
  const ColumnarBoundaryFlow flow = ColumnarBoundaryFlow::forLeaves(leaf, false, false);
  const ColumnarStack stack = threeLevelStack(leaf, flow);

  REQUIRE(stack.numLevels() == 4);
  CHECK(stack.topLevel() == 3);
  CHECK(&stack.level(0) == &leaf);
  CHECK(stack.size(1) == 4);
  CHECK(stack.size(2) == 2);
  CHECK(stack.numTopModules() == 1);
  CHECK(stack.leafModule(5) == 2);
  CHECK(stack.parent(1, 2) == 1);
  for (int k = 0; k < 3; ++k)
    CHECK(sameLevel(stack.level(k + 1), flow.aggregate(stack.level(k), stack.assign(k), stack.size(k + 1))));
  // The two groups exchange exactly the two joining links.
  CHECK(stack.level(2).exit[0] == doctest::Approx(0.01));
}

TEST_CASE("ColumnarStack reads a rectangular tree from leaf paths and refuses a ragged one")
{
  const ColumnarLevel leaf = twoGroupLeaves();
  const ColumnarBoundaryFlow flow = ColumnarBoundaryFlow::forLeaves(leaf, false, false);
  ColumnarStack stack;
  stack.bind(leaf, flow);
  // Coarsest first, ids not compact.
  const std::vector<std::vector<int>> paths = { { 7, 3 }, { 7, 3 }, { 7, 9 }, { 7, 9 }, { 2, 3 }, { 2, 3 }, { 2, 5 }, { 2, 5 } };
  REQUIRE(stack.setFromLeafPaths(paths));
  REQUIRE(stack.numLevels() == 3);
  CHECK(stack.size(1) == 4);
  CHECK(stack.size(2) == 2);
  CHECK(stack.leafModule(0) == stack.leafModule(1));
  CHECK(stack.leafModule(1) != stack.leafModule(2));
  CHECK(stack.parent(1, stack.leafModule(0)) == stack.parent(1, stack.leafModule(3)));
  CHECK(stack.parent(1, stack.leafModule(0)) != stack.parent(1, stack.leafModule(4)));

  const ColumnarStack before = stack;
  std::vector<std::vector<int>> ragged = paths;
  ragged[3] = { 7 };
  CHECK_FALSE(stack.setFromLeafPaths(ragged));
  CHECK(sameStack(stack, before));
}

TEST_CASE("ColumnarStack checkpoints undo slot edits exactly and keep committed ones")
{
  const ColumnarLevel leaf = twoGroupLeaves();
  const ColumnarBoundaryFlow flow = ColumnarBoundaryFlow::forLeaves(leaf, false, false);
  ColumnarStack stack = threeLevelStack(leaf, flow);
  const ColumnarStack original = stack;

  // Move leaf 3 to the second group: a regroup across grandparents, so the levels
  // above need re-aggregating.
  ColumnarStack::Checkpoint checkpoint = stack.checkpoint();
  stack.setModules(0, { 0, 0, 1, 2, 2, 2, 3, 3 }, 4);
  stack.reaggregateFrom(1);
  CHECK(stack.leafModule(3) == 2);
  CHECK(sameLevel(stack.level(2), flow.aggregate(stack.level(1), stack.assign(1), 2)));
  stack.rollback(checkpoint);
  CHECK(sameStack(stack, original));
  CHECK_FALSE(stack.hasOpenCheckpoint());

  checkpoint = stack.checkpoint();
  stack.setModules(1, { 0, 1, 1, 1 }, 2);
  stack.commit(checkpoint);
  CHECK(stack.parent(1, 1) == 1);
  CHECK_FALSE(sameStack(stack, original));
}

TEST_CASE("ColumnarStack checkpoints nest")
{
  const ColumnarLevel leaf = twoGroupLeaves();
  const ColumnarBoundaryFlow flow = ColumnarBoundaryFlow::forLeaves(leaf, false, false);
  ColumnarStack stack = threeLevelStack(leaf, flow);
  const ColumnarStack original = stack;

  const ColumnarStack::Checkpoint outer = stack.checkpoint();
  stack.setModules(0, { 0, 1, 1, 1, 2, 2, 3, 3 }, 4);
  const ColumnarStack afterOuterEdit = stack;

  // An inner step that is rolled back leaves the outer edit in place.
  ColumnarStack::Checkpoint inner = stack.checkpoint();
  stack.setParents(1, { 0, 1, 1, 1 });
  stack.reaggregateFrom(1);
  stack.setModules(0, { 0, 0, 0, 1, 2, 2, 3, 3 }, 4);
  stack.rollback(inner);
  CHECK(sameStack(stack, afterOuterEdit));

  // An inner step that is committed is still undone by the outer rollback.
  inner = stack.checkpoint();
  stack.setModules(0, { 0, 0, 0, 0, 1, 1, 1, 1 }, 4);
  stack.setParents(1, { 0, 1, 1, 1 });
  stack.reaggregateFrom(1);
  stack.commit(inner);
  CHECK(stack.hasOpenCheckpoint());
  stack.rollback(outer);
  CHECK(sameStack(stack, original));
}

TEST_CASE("ColumnarStack checkpoints undo edits that change the number of levels")
{
  const ColumnarLevel leaf = twoGroupLeaves();
  const ColumnarBoundaryFlow flow = ColumnarBoundaryFlow::forLeaves(leaf, false, false);
  ColumnarStack stack = threeLevelStack(leaf, flow);
  const ColumnarStack original = stack;

  // A slot edit, a level insertion and removal, and a slot edit in the new shape,
  // all undone by one rollback.
  const ColumnarStack::Checkpoint outer = stack.checkpoint();
  stack.setModules(1, { 0, 1, 1, 1 }, 2);
  const ColumnarStack::Checkpoint inner = stack.checkpoint();
  stack.insertLevel(0, { 0, 0, 1, 1, 2, 2, 3, 4 }, 5, { 0, 1, 2, 3, 3 });
  CHECK(stack.numLevels() == 5);
  stack.removeLevel(2);
  CHECK(stack.numLevels() == 4);
  stack.setParents(2, { 0, 0 });
  stack.commit(inner);
  stack.pushLevel({ 0 }, 1);
  CHECK(stack.numLevels() == 5);
  stack.rollback(outer);
  CHECK(sameStack(stack, original));
}

TEST_CASE("ColumnarStack inserts and removes a level without changing the levels around it")
{
  const ColumnarLevel leaf = twoGroupLeaves();
  const ColumnarBoundaryFlow flow = ColumnarBoundaryFlow::forLeaves(leaf, false, false);
  ColumnarStack stack = threeLevelStack(leaf, flow);
  const ColumnarStack original = stack;

  // Split the bottom into the 8 leaves' own modules, under the old pairs.
  stack.insertLevel(0, { 0, 1, 2, 3, 4, 5, 6, 7 }, 8, { 0, 0, 1, 1, 2, 2, 3, 3 });
  REQUIRE(stack.numLevels() == 5);
  CHECK(stack.size(1) == 8);
  CHECK(sameLevel(stack.level(2), original.level(1)));
  CHECK(sameLevel(stack.level(3), original.level(2)));

  // Removing it composes the assignments back: every leaf joins its old pair.
  stack.removeLevel(1);
  CHECK(stack.assign(0) == original.assign(0));
  CHECK(sameStack(stack, original));

  // Removing the top makes the groups the top modules.
  stack.removeLevel(3);
  CHECK(stack.numLevels() == 3);
  CHECK(stack.numTopModules() == 2);
}

TEST_CASE("ColumnarStack copies are independent values with closed checkpoints")
{
  const ColumnarLevel leaf = twoGroupLeaves();
  const ColumnarBoundaryFlow flow = ColumnarBoundaryFlow::forLeaves(leaf, false, false);
  ColumnarStack stack = threeLevelStack(leaf, flow);

  const ColumnarStack::Checkpoint checkpoint = stack.checkpoint();
  stack.setModules(1, { 0, 1, 1, 1 }, 2);
  ColumnarStack copy = stack;
  CHECK_FALSE(copy.hasOpenCheckpoint());
  CHECK(sameStack(copy, stack));
  stack.rollback(checkpoint);
  CHECK(copy.parent(1, 1) == 1);
  CHECK(stack.parent(1, 1) == 0);
  CHECK(&copy.level(0) == &leaf);
}

TEST_CASE("ColumnarStack versions name its content: equal ids mean equal stacks")
{
  const ColumnarLevel leaf = twoGroupLeaves();
  const ColumnarBoundaryFlow flow = ColumnarBoundaryFlow::forLeaves(leaf, false, false);
  ColumnarStack stack = threeLevelStack(leaf, flow);
  const std::uint64_t built = stack.version();

  // An edit takes a fresh id; a rollback restores the id its checkpoint saw.
  const ColumnarStack::Checkpoint outer = stack.checkpoint();
  stack.setModules(1, { 0, 1, 1, 1 }, 2);
  const std::uint64_t edited = stack.version();
  CHECK(edited != built);
  const ColumnarStack::Checkpoint inner = stack.checkpoint();
  stack.pushLevel({ 0, 0 }, 1);
  CHECK(stack.version() != edited);
  stack.rollback(inner);
  CHECK(stack.version() == edited);
  stack.rollback(outer);
  CHECK(stack.version() == built);

  // A copy keeps the id; an edit of either one never reuses an id the other has
  // had, so two stacks with different content never share one.
  ColumnarStack copy = stack;
  CHECK(copy.version() == built);
  stack.setModules(1, { 0, 1, 1, 1 }, 2);
  copy.setModules(1, { 0, 0, 0, 1 }, 2);
  CHECK(stack.version() != copy.version());
  CHECK(stack.version() != edited);
  CHECK(copy.version() != edited);

  // A commit keeps the edit's id.
  const ColumnarStack::Checkpoint kept = stack.checkpoint();
  stack.setParents(1, { 0, 0 });
  const std::uint64_t parentsSet = stack.version();
  stack.commit(kept);
  CHECK(stack.version() == parentsSet);
}

} // namespace test
} // namespace infomap
