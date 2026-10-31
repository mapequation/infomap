#include "TestUtils.h"
#include "core/ColumnarHierarchy.h"

#include <cmath>
#include <tuple>
#include <vector>

// refineLayerWithinGrandparent keeps or undoes each grandparent on its own (#1387).
// That rests on one property of the stack codelength: re-partitioning the insides
// of a grandparent changes only its own codebook term and its child modules'
// terms, so the per-grandparent changes add up to the change of the whole layer.

namespace infomap {
namespace test {

namespace {

  // 64 leaves in a planted three-level hierarchy: 16 cliques of 4, grouped 4 by 4
  // into 4 groups, grouped 2 by 2 into 2 super groups. Symmetric links, weaker the
  // higher the level where their ends first meet.
  ColumnarLevel plantedLeaves()
  {
    const int n = 64;
    std::vector<std::tuple<int, int, double>> links;
    for (int a = 0; a < n; ++a)
      for (int b = a + 1; b < n; ++b) {
        double w = 0.0;
        if (a / 4 == b / 4)
          w = 1.0;
        else if (a / 16 == b / 16 && (a + b) % 3 == 0)
          w = 0.2;
        else if (a / 32 == b / 32 && (a + b) % 7 == 0)
          w = 0.05;
        else if ((a + b) % 31 == 0)
          w = 0.01;
        if (w > 0.0) {
          links.emplace_back(a, b, w);
          links.emplace_back(b, a, w);
        }
      }
    double total = 0.0;
    for (const auto& l : links)
      total += std::get<2>(l);

    ColumnarLevel leaf;
    leaf.n = n;
    leaf.flow.assign(n, 0.0);
    leaf.enter.assign(n, 0.0);
    leaf.exit.assign(n, 0.0);
    leaf.outStart.assign(n + 1, 0);
    leaf.inStart.assign(n + 1, 0);
    for (const auto& l : links) {
      ++leaf.outStart[std::get<0>(l) + 1];
      ++leaf.inStart[std::get<1>(l) + 1];
    }
    for (int i = 0; i < n; ++i) {
      leaf.outStart[i + 1] += leaf.outStart[i];
      leaf.inStart[i + 1] += leaf.inStart[i];
    }
    leaf.outTarget.assign(links.size(), 0);
    leaf.outFlow.assign(links.size(), 0.0);
    leaf.inTarget.assign(links.size(), 0);
    leaf.inFlow.assign(links.size(), 0.0);
    std::vector<int> outPos(leaf.outStart.begin(), leaf.outStart.end() - 1);
    std::vector<int> inPos(leaf.inStart.begin(), leaf.inStart.end() - 1);
    for (const auto& l : links) {
      const int s = std::get<0>(l), t = std::get<1>(l);
      const double f = std::get<2>(l) / total;
      const int o = outPos[s]++, in = inPos[t]++;
      leaf.outTarget[o] = t;
      leaf.outFlow[o] = f;
      leaf.inTarget[in] = s;
      leaf.inFlow[in] = f;
      leaf.flow[s] += f;
      leaf.exit[s] += f;
      leaf.enter[t] += f;
    }
    return leaf;
  }

  // The planted hierarchy with level 1 scrambled inside each group: the 4 level-1
  // modules of a group each take one leaf from every clique of the group, so a
  // refine within the groups (the grandparents of the leaves) has work to do.
  void setScrambledStack(ColumnarStack& stack)
  {
    std::vector<int> leafModule(64);
    for (int i = 0; i < 64; ++i)
      leafModule[i] = (i / 16) * 4 + i % 4;
    stack.setFlat(leafModule, 16);
    stack.pushLevel({ 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3 }, 4);
    stack.pushLevel({ 0, 0, 1, 1 }, 2);
  }

  // Codelength change of every layer-(k+2) module G between two stacks that
  // differ only inside the grandparents: G's own term plus its children's terms.
  std::vector<double> perGrandparentDelta(int k, const ColumnarStack& before, const columnar::StackBreakdown& b0, const ColumnarStack& after, const columnar::StackBreakdown& b1)
  {
    std::vector<double> delta(static_cast<std::size_t>(before.size(k + 2)), 0.0);
    for (int G = 0; G < before.size(k + 2); ++G)
      delta[G] = b1.moduleTerm[k + 2][G] - b0.moduleTerm[k + 2][G];
    for (int m = 0; m < after.size(k + 1); ++m)
      delta[after.parent(k + 1, m)] += b1.moduleTerm[k + 1][m];
    for (int m = 0; m < before.size(k + 1); ++m)
      delta[before.parent(k + 1, m)] -= b0.moduleTerm[k + 1][m];
    return delta;
  }

} // namespace

TEST_CASE("Columnar refine within grandparents: per-grandparent changes add up to the layer's change [fast][core][columnar-stack]")
{
  const ColumnarLevel leaf = plantedLeaves();
  const ColumnarBoundaryFlow flow = ColumnarBoundaryFlow::forLeaves(leaf, false, false);
  ColumnarHierarchy hier;
  hier.buildFromBorrowedLevel(leaf, flow, 123);
  setScrambledStack(hier.stack());

  for (int k = 0; k <= 1; ++k) {
    ColumnarStack& stack = hier.stack();
    columnar::StackBreakdown b0, b1;
    const double L0 = hier.scoreStack(stack, &b0);
    const ColumnarStack before = stack;

    // Re-partition layer k inside every grandparent: units alternate between two
    // new modules per grandparent.
    const int nU = stack.size(k);
    std::vector<int> assign(nU), parents;
    std::vector<int> firstOf(static_cast<std::size_t>(stack.size(k + 2)) * 2, -1);
    for (int u = 0; u < nU; ++u) {
      const int G = stack.parent(k + 1, stack.parent(k, u));
      int& id = firstOf[static_cast<std::size_t>(G) * 2 + u % 2];
      if (id < 0) {
        id = static_cast<int>(parents.size());
        parents.push_back(G);
      }
      assign[u] = id;
    }
    const ColumnarStack::Checkpoint cp = stack.checkpoint();
    stack.setModules(k, assign, static_cast<int>(parents.size()), parents);
    const double L1 = hier.scoreStack(stack, &b1);

    const std::vector<double> delta = perGrandparentDelta(k, before, b0, stack, b1);
    double sum = 0.0;
    for (double d : delta)
      sum += d;
    CHECK(std::abs(sum - (L1 - L0)) < 1e-12);
    CHECK(std::abs(b1.rootTerm - b0.rootTerm) < 1e-12);
    for (int level = k + 3; level <= stack.topLevel(); ++level)
      for (int m = 0; m < stack.size(level); ++m)
        CHECK(std::abs(b1.moduleTerm[level][m] - b0.moduleTerm[level][m]) < 1e-12);
    stack.rollback(cp);
  }
}

TEST_CASE("Columnar refine within grandparents: no grandparent ends worse [fast][core][columnar-stack]")
{
  const ColumnarLevel leaf = plantedLeaves();
  const ColumnarBoundaryFlow flow = ColumnarBoundaryFlow::forLeaves(leaf, false, false);
  ColumnarHierarchy hier;
  hier.buildFromBorrowedLevel(leaf, flow, 123);
  setScrambledStack(hier.stack());

  columnar::StackBreakdown b0, b1;
  double L = hier.scoreStack(hier.stack(), &b0);
  const double start = L;
  const ColumnarStack before = hier.stack();
  REQUIRE(hier.gatedStep(L, [&] { return hier.refineLayerWithinGrandparent(0); }));
  CHECK(L < start);
  hier.scoreStack(hier.stack(), &b1);
  for (double d : perGrandparentDelta(0, before, b0, hier.stack(), b1))
    CHECK(d <= columnar::kMinImprovement);
  // The scrambled groups are put back together: every clique is one module.
  for (int c = 0; c < 16; ++c)
    for (int j = 1; j < 4; ++j)
      CHECK(hier.stack().assign(0)[c * 4 + j] == hier.stack().assign(0)[c * 4]);
}

} // namespace test
} // namespace infomap
