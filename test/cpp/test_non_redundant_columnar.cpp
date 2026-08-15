#include "TestUtils.h"
#include "core/ColumnarObjective.h"
#include "io/Config.h"

#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

// The non-redundant map equation L* on the columnar (non-recursive) engine. L* is a
// base-objective variant of ColumnarTwoLevel (not a composable correction): it
// restructures the enter/within/exit codebooks at every level, the exit codebook
// using a leave-one-out normalizer over sibling modules. --non-redundant implies
// --columnar (the only L* implementation on this branch).
//
// The fixed-partition goldens below are computed independently from the three-codebook
// definition of L* (see "Non-redundant map equation - expanded form.ipynb") and match
// the object-oriented non-redundant implementation to full print precision, so they
// check the columnar hierarchicalCodelengthFromStack against the maths.

namespace infomap {
namespace test {

namespace {

Config nonRedundantConfig(const std::string& extraFlags)
{
  return Config("input.net --silent --no-file-output " + extraFlags, true);
}

// Score a fixed partition of a fixture network under a given objective (--no-infomap),
// which routes through evaluateColumnarPartition -> hierarchicalCodelengthFromStack.
double scoreFixedPartition(const std::string& network, const std::string& cluster, const std::string& flags)
{
  InfomapWrapper im(defaultFlags(flags + " --no-infomap --cluster-data " + clusterFixturePath(cluster)));
  im.readInputData(networkFixturePath(network));
  im.run();
  return im.codelength();
}

struct RingLink {
  unsigned int source;
  unsigned int target;
  double weight;
};

// k triangles in a ring: triangle i on nodes 3i+1, 3i+2, 3i+3, its last node
// linked on to the next triangle's first. k = 2 is the canonical two-module
// network; larger k gives the multi-module case.
std::vector<RingLink> ringOfTriangles(unsigned int k)
{
  std::vector<RingLink> links;
  for (unsigned int i = 0; i < k; ++i) {
    const unsigned int a = 3 * i + 1;
    links.push_back({ a, a + 1, 1.0 });
    links.push_back({ a + 1, a + 2, 1.0 });
    links.push_back({ a + 2, a, 1.0 });
    links.push_back({ a + 2, 3 * ((i + 1) % k) + 1, 1.0 });
  }
  return links;
}

// Score the one-triangle-per-module partition of ringOfTriangles(k), either on the
// physical network (copies == 1) or on an exactly lumpable STATE duplication of it:
// every physical node becomes `copies` state nodes and every link's weight is spread
// evenly over the copies x copies state pairs.
//
// That construction leaves the dynamics alone. Each state of u carries 1/copies of
// u's flow and u's conditional next-step law, so the state walk projects exactly
// onto the walk on the physical network, and the lifted partition keeps every
// physical node's states together in one module. Nothing describable has changed, so
// the codelength must not change either -- under L* exactly as under L.
double scoreRingOfTriangles(unsigned int k, unsigned int copies, const std::string& flags)
{
  const unsigned int n = 3 * k;
  const auto links = ringOfTriangles(k);
  InfomapWrapper im(defaultFlags(flags + " --two-level --no-infomap"));
  std::map<unsigned int, unsigned int> partition;
  if (copies == 1) {
    for (const auto& link : links)
      im.addLink(link.source, link.target, link.weight);
    for (unsigned int u = 1; u <= n; ++u)
      partition[u] = (u - 1) / 3 + 1;
  } else {
    for (unsigned int u = 1; u <= n; ++u) {
      for (unsigned int c = 0; c < copies; ++c) {
        im.addStateNode(u + c * n, u);
        partition[u + c * n] = (u - 1) / 3 + 1;
      }
    }
    const double share = 1.0 / static_cast<double>(copies * copies);
    for (const auto& link : links) {
      for (unsigned int cs = 0; cs < copies; ++cs) {
        for (unsigned int ct = 0; ct < copies; ++ct)
          im.addLink(link.source + cs * n, link.target + ct * n, link.weight * share);
      }
    }
  }
  im.setInitialPartition(partition);
  im.run();
  return im.codelength();
}

} // namespace

TEST_CASE("NonRedundant/columnar: --non-redundant parses and implies --columnar [fast][core][non-redundant]")
{
  const auto conf = nonRedundantConfig("--non-redundant");
  CHECK(conf.nonRedundant);
  CHECK(conf.columnarSearch); // L* runs on the columnar engine

  const auto exactConf = nonRedundantConfig("--non-redundant --non-redundant-exact");
  CHECK(exactConf.nonRedundant);
  CHECK(exactConf.nonRedundantExact);

  const auto directedConf = nonRedundantConfig("--non-redundant --directed");
  CHECK(directedConf.nonRedundant);
}

TEST_CASE("NonRedundant/columnar: no input is excluded and every objective but lossy composes [fast][core][non-redundant]")
{
  // L* constrains which walk *steps* are possible — no immediate re-entry into the
  // module just left, no immediate exit from the one just entered — which says nothing
  // about which codebook a step is coded in. Higher-order dynamics and the composable
  // corrections therefore stay available.
  auto accepts = [](const std::string& flags) {
    const auto conf = nonRedundantConfig(flags);
    CHECK(conf.nonRedundant);
    CHECK(conf.columnarSearch);
  };
  accepts("--non-redundant --meta-data foo.txt");
  accepts("--non-redundant --entropy-corrected");
  accepts("--non-redundant --preferred-number-of-modules 5");
  accepts("--non-redundant --directed --regularized");
  accepts("--non-redundant --multilayer-relax-rate 0.15");
#if INFOMAP_FEATURE_LOSSY_MAP_EQUATION
  // The exception, and the reason this case is not called "no objective is excluded"
  // any more: the lossy noise credit hands back sum_leaf plogp(f) at coefficient 1,
  // which is the base objective's rate for it, not L*'s (#1011, F38). Both flag
  // orders, so the check cannot become order-dependent.
  CHECK_THROWS_AS(nonRedundantConfig("--non-redundant --lossy"), std::runtime_error);
  CHECK_THROWS_AS(nonRedundantConfig("--lossy --non-redundant"), std::runtime_error);
#endif
}

TEST_CASE("NonRedundant/columnar: higher-order input runs under L* [fast][core][non-redundant]")
{
  // Config-level checks cannot cover this one: stateInput/multilayerInput are set by
  // configureNetworkMode() when the network is READ, never by an option, so the only way
  // to know higher-order input is accepted is to run it. Both of these carry the physical
  // codebook (MemCorrection) on top of the L* base.
  for (const char* net : { "states_flow.net", "multilayer.net" }) {
    InfomapWrapper im(defaultFlags("--non-redundant --num-trials 1"));
    im.readInputData(networkFixturePath(net));
    im.run();
    CHECK(std::isfinite(im.codelength()));
    CHECK(im.codelength() > 0.0);
  }
}

TEST_CASE("NonRedundant/columnar: an ADDITIVE correction adds the same term under L* as under L [fast][core][non-redundant]")
{
  // What "L* supports every correction" asserts for the corrections that ADD a term
  // carrying their own rate. Metadata is one: its term is metaDataRate * sum_m F_m*H_m,
  // a separate codebook consulted once per unit of node-visit flow in the module, and L*
  // changes which codebook names a node, not how often a node is visited. So on a FIXED
  // partition the metadata term is identical under both bases. Same for the entropy bias
  // (a node count) and the preferred module count.
  //
  // It does NOT generalize to every correction, and reading it as if it did is what let
  // #1009 through: MemCorrection SUBSTITUTES the physical-node sum plogp(flow) for the
  // state-node one inside the base objective's own leaf-module term, so it inherits that
  // term's coefficient — 1 under the base map equation, >= 1 and per-module under L*.
  // The invariance test below is the one that pins that down.
  //
  // Both arms run on the columnar engine so the comparison isolates the base objective
  // and nothing else.
  const std::string net = "twotriangles_flow.net";
  const std::string clu = "twotriangles_two_modules.clu";
  const std::string meta = " --meta-data " + fixturePath("meta/twotriangles.meta");

  const double L = scoreFixedPartition(net, clu, "--columnar --two-level");
  const double Lmeta = scoreFixedPartition(net, clu, "--columnar --two-level" + meta);
  const double Lstar = scoreFixedPartition(net, clu, "--non-redundant --two-level");
  const double LstarMeta = scoreFixedPartition(net, clu, "--non-redundant --two-level" + meta);

  CHECK(Lmeta != doctest::Approx(L)); // the correction has to bite for this to mean anything
  CHECK(LstarMeta - Lstar == doctest::Approx(Lmeta - L).epsilon(1e-9));
  // ... and adding a correction must not perturb the L* base itself.
  CHECK(Lstar == doctest::Approx(1.983611049901).epsilon(1e-9));
}

TEST_CASE("NonRedundant/columnar: two-level L* on a fixed partition, undirected, differs from standard L [fast][core][non-redundant]")
{
  const double Lstar = scoreFixedPartition("twotriangles_flow.net", "twotriangles_two_modules.clu", "--non-redundant --two-level");
  const double Lstd = scoreFixedPartition("twotriangles_flow.net", "twotriangles_two_modules.clu", "--two-level");

  CHECK(Lstar == doctest::Approx(1.983611049901).epsilon(1e-9));
  CHECK(Lstar < Lstd);
}

TEST_CASE("NonRedundant/columnar: two-level L* on a fixed partition, directed [fast][core][non-redundant]")
{
  const double Lstar = scoreFixedPartition("twotriangles_flow.net", "twotriangles_two_modules.clu", "--non-redundant --two-level --directed");
  CHECK(Lstar == doctest::Approx(1.791821407552).epsilon(1e-9));
}

TEST_CASE("NonRedundant/columnar: one module reduces to the node entropy (== standard L) [fast][core][non-redundant]")
{
  const double Lstar = scoreFixedPartition("twotriangles_flow.net", "twotriangles_single_module.clu", "--non-redundant --two-level");
  const double Lstd = scoreFixedPartition("twotriangles_flow.net", "twotriangles_single_module.clu", "--two-level");
  CHECK(Lstar == doctest::Approx(2.470950594455).epsilon(1e-9));
  CHECK(Lstar == doctest::Approx(Lstd).epsilon(1e-9));
}

TEST_CASE("NonRedundant/columnar: hierarchical L* on a multilevel fixed partition (exercises the super-module codebooks) [fast][core][non-redundant]")
{
  // A three-level tree whose middle level is trivial (one sub-module per top module)
  // must reduce to the two-level L* of the same leaf partition: a trivial interior
  // level contributes a zero enter codebook and zero-Z exit codebooks.
  const double Lstar3 = scoreFixedPartition("twotriangles_flow.net", "twotriangles_three_level.tree", "--non-redundant");
  CHECK(Lstar3 == doctest::Approx(1.983611049901).epsilon(1e-9));
}

TEST_CASE("NonRedundant/columnar: L* is invariant under exactly lumpable state duplication [fast][core][non-redundant]")
{
  // The regression test for #1009. Splitting a physical node into states that are
  // indistinguishable to the walker, with the partition lifted so no physical node is
  // split across modules, describes the same process with the same modules — so both
  // objectives must report the same codelength. L always did; L* did not, because
  // MemCorrection substituted the physical-node sum plogp(flow) for the state-node one
  // at coefficient 1 (right for the base map equation) while L*'s leaf-module term
  // consumes it at nrLeafCodebookRate() >= 1. The substituted quantity is non-positive,
  // so the error was always positive and grew with the number of physical nodes holding
  // several states in one module: +0.050 bits here undirected, +0.026 directed.
  //
  // Undirected, PageRank-directed and rawdir all run: the rate is 1 + qEnter*qExit /
  // (flow*(flow+qExit)), so it is only the undirected case that has qEnter == qExit.
  for (const std::string& flow : { std::string(""), std::string(" --directed"), std::string(" --flow-model rawdir") }) {
    for (unsigned int k : { 2u, 5u }) { // two modules, then multi-module
      CAPTURE(flow);
      CAPTURE(k);
      // Control: the base objective was already invariant, so a failure here means the
      // duplication was built wrong, not that the objective is wrong.
      const double L = scoreRingOfTriangles(k, 1, "--columnar" + flow);
      const double Ldup = scoreRingOfTriangles(k, 2, "--columnar" + flow);
      CHECK(Ldup == doctest::Approx(L).epsilon(1e-12));

      const double Lstar = scoreRingOfTriangles(k, 1, "--non-redundant" + flow);
      const double LstarDup = scoreRingOfTriangles(k, 2, "--non-redundant" + flow);
      CHECK(LstarDup == doctest::Approx(Lstar).epsilon(1e-12));
      CHECK(Lstar < L); // L* is the cheaper description of the same partition
    }
  }

  // Goldens on the physical arm, so the pair cannot pass by both sides moving together.
  CHECK(scoreRingOfTriangles(2, 1, "--non-redundant") == doctest::Approx(2.361270125569452).epsilon(1e-9));
  CHECK(scoreRingOfTriangles(5, 1, "--non-redundant") == doctest::Approx(2.861270125569453).epsilon(1e-9));
}

TEST_CASE("NonRedundant/columnar: nrLeafCodebookRate is the rate nrEnterWithin consumes F at [fast][core][non-redundant]")
{
  // The two must move together: MemCorrection weights its substitution by
  // nrLeafCodebookRate while the scorer charges F through nrEnterWithin, and nothing
  // else ties them. nrEnterWithin is affine in F, so the identity below is exact.
  using namespace infomap::columnar;
  const double flow[] = { 0.5, 0.25, 1.0, 1e-17 };
  const double enter[] = { 0.0, 0.05, 0.3 };
  const double exit[] = { 0.0, 0.05, 0.4 };
  for (double f : flow) {
    for (double e : enter) {
      for (double x : exit) {
        const double rate = nrLeafCodebookRate(f, e, x);
        const double dF = nrEnterWithin(f, e, x, 0.75) - nrEnterWithin(f, e, x, 0.25);
        CHECK(dF == doctest::Approx(-rate * 0.5).epsilon(1e-12));
        // >= 1 unless the module is degenerate (no flow) or has no boundary in one
        // direction, which is what makes the old flat coefficient 1 an under-charge.
        CHECK(rate >= (f < 1e-16 ? 0.0 : 1.0 - 1e-12));
      }
    }
  }
}

TEST_CASE("NonRedundant/columnar: hierarchical search produces a sound, self-consistent L* [fast][core][non-redundant]")
{
  InfomapWrapper hier(defaultFlags("--non-redundant --num-trials 5"));
  hier.readInputData(repoPath("examples/networks/ninetriangles.net"));
  hier.run();
  const double searched = hier.codelength();

  CHECK(std::isfinite(searched));
  CHECK(searched > 0.0);
  CHECK(hier.numLevels() >= 2); // found a hierarchy

  // Round-trip: re-scoring the searched tree under L* reproduces the search codelength
  // (the reported value is the columnar L* of the found partition).
  // Relative, like the other round-trip tests: Windows has no /tmp, and an absolute
  // POSIX path made this the only test that could not run there.
  const std::string treePath = "infomap_nr_columnar_roundtrip.tree";
  std::remove(treePath.c_str());
  hier.writeTree(treePath);

  InfomapWrapper rescored(defaultFlags("--non-redundant --no-infomap --cluster-data " + treePath));
  rescored.readInputData(repoPath("examples/networks/ninetriangles.net"));
  rescored.run();
  CHECK(rescored.codelength() == doctest::Approx(searched).epsilon(1e-9));
  std::remove(treePath.c_str());
}

TEST_CASE("NonRedundant/columnar: a ragged tree is scored under L*, not the base map equation [fast][core][non-redundant]")
{
  // A tree whose branches end at different depths does not fit the strict-level stack,
  // and the object-oriented fallback it used to take has no L* implementation at all --
  // so --non-redundant silently reported the BASE codelength, identical to the run
  // without the flag. Padding the short paths with a pass-through level makes the tree
  // rectangular at zero cost under L*, so the two now agree with the hand-padded
  // fixture instead.
  const double raggedLstar = scoreFixedPartition("twotriangles_flow.net", "twotriangles_ragged_branches.tree", "--non-redundant");
  const double paddedLstar = scoreFixedPartition("twotriangles_flow.net", "twotriangles_ragged_branches_padded.tree", "--non-redundant");
  const double raggedL = scoreFixedPartition("twotriangles_flow.net", "twotriangles_ragged_branches.tree", "");

  CHECK(raggedLstar == doctest::Approx(paddedLstar).epsilon(1e-12));
  CHECK(raggedLstar == doctest::Approx(2.142186380).epsilon(1e-9));
  CHECK(raggedLstar < raggedL); // it was bit-equal to raggedL (2.614170945) before
  CHECK(raggedL == doctest::Approx(2.614170945).epsilon(1e-9)); // the base value must not move
}

TEST_CASE("NonRedundant/columnar: a ragged STATE tree is scored under L* [fast][core][non-redundant]")
{
  // Same, with MemCorrection in play: the physical-node codebook substitution is
  // charged at the leaf-module rate, which differs between L and L*, so this is the
  // case where "just reuse the object-oriented value" is furthest from right.
  const double raggedLstar = scoreFixedPartition("states.net", "states_ragged_branches.tree", "--non-redundant");
  const double paddedLstar = scoreFixedPartition("states.net", "states_ragged_branches_padded.tree", "--non-redundant");
  const double raggedL = scoreFixedPartition("states.net", "states_ragged_branches.tree", "");

  CHECK(raggedLstar == doctest::Approx(paddedLstar).epsilon(1e-12));
  CHECK(raggedLstar == doctest::Approx(2.612197223).epsilon(1e-9));
  CHECK(raggedL == doctest::Approx(4.251629167).epsilon(1e-9));
}

TEST_CASE("NonRedundant/columnar: only L* is invariant under a pass-through level [fast][core][non-redundant]")
{
  // The theorem the padding rests on, pinned from both sides so that "simplifying" the
  // gate away would fail: inserting a single-child level costs exactly 0 under L* (its
  // enter codebook is e*(plogp(e)-plogp(e))/e and its child's exit term has numerator
  // plogp(x)-0-plogp(x)) and costs real bits under the base map equation.
  const double raggedL = scoreFixedPartition("twotriangles_flow.net", "twotriangles_ragged_branches.tree", "");
  const double paddedL = scoreFixedPartition("twotriangles_flow.net", "twotriangles_ragged_branches_padded.tree", "");
  CHECK(paddedL > raggedL + 1e-6);

  const double raggedLstar = scoreFixedPartition("twotriangles_flow.net", "twotriangles_ragged_branches.tree", "--non-redundant");
  const double paddedLstar = scoreFixedPartition("twotriangles_flow.net", "twotriangles_ragged_branches_padded.tree", "--non-redundant");
  CHECK(raggedLstar == doctest::Approx(paddedLstar).epsilon(1e-12));
}

TEST_CASE("NonRedundant/columnar: the ragged padding does not leak into the entropy bias [fast][core][non-redundant]")
{
  // --entropy-corrected is the one correction that counts NODES, so the phantom levels
  // the padding inserts would inflate it. The evaluation takes their charge back off
  // the total; this pins that it takes off exactly one node's worth (the ragged tree
  // needs one pass-through, above module 1).
  //
  // The per-node bias is derived from the fixture rather than hard-coded: the
  // single-module partition has 6 leaves + 1 module = 7 non-root nodes, so the gap the
  // flag opens there is 7 times the per-node term.
  const double oneModule = scoreFixedPartition("twotriangles_flow.net", "twotriangles_single_module.clu", "--non-redundant --two-level");
  const double oneModuleBiased = scoreFixedPartition("twotriangles_flow.net", "twotriangles_single_module.clu", "--non-redundant --two-level --entropy-corrected");
  const double biasPerNode = (oneModuleBiased - oneModule) / 7.0;
  CHECK(biasPerNode > 0.0);

  const double raggedBiased = scoreFixedPartition("twotriangles_flow.net", "twotriangles_ragged_branches.tree", "--non-redundant --entropy-corrected");
  const double paddedBiased = scoreFixedPartition("twotriangles_flow.net", "twotriangles_ragged_branches_padded.tree", "--non-redundant --entropy-corrected");
  CHECK(raggedBiased == doctest::Approx(paddedBiased - biasPerNode).epsilon(1e-9));
}

TEST_CASE("NonRedundant/columnar: a BARE top-level leaf is scored under L* too [fast][core][non-redundant]")
{
  // The most common ragged shape there is: Infomap's .tree format writes a module of
  // one node as a bare top-level leaf (`2 0.15 "A" 1`) and reads it back the same way,
  // so leafModulePathsFromTree hands the padding an EMPTY path -- no finest id to
  // repeat. Bailing there left --non-redundant reporting the base L, bit-identical to
  // the run without the flag (2.714170945 against a true L* of 2.187131226).
  //
  // A top-level leaf IS its own module, so it gets a synthetic module id and the same
  // padding as any short path. The three fixtures are three spellings of ONE partition
  // -- bare, explicit module, explicit module plus a pass-through -- so L* must give
  // all three the same value.
  const double bareLstar = scoreFixedPartition("twotriangles_flow.net", "twotriangles_top_level_leaf.tree", "--non-redundant");
  const double moduleLstar = scoreFixedPartition("twotriangles_flow.net", "twotriangles_top_level_leaf_module.tree", "--non-redundant");
  const double rectLstar = scoreFixedPartition("twotriangles_flow.net", "twotriangles_top_level_leaf_rect.tree", "--non-redundant");

  CHECK(bareLstar == doctest::Approx(moduleLstar).epsilon(1e-12));
  CHECK(bareLstar == doctest::Approx(rectLstar).epsilon(1e-12));
  CHECK(bareLstar == doctest::Approx(2.187131226).epsilon(1e-9));

  // The base map equation is NOT invariant under those spellings and must not move:
  // it charges each extra level, and it addresses a bare top-level leaf in the root's
  // own codebook rather than giving it a module codebook.
  const double bareL = scoreFixedPartition("twotriangles_flow.net", "twotriangles_top_level_leaf.tree", "");
  const double moduleL = scoreFixedPartition("twotriangles_flow.net", "twotriangles_top_level_leaf_module.tree", "");
  CHECK(bareL == doctest::Approx(2.714170945).epsilon(1e-9));
  CHECK(moduleL == doctest::Approx(3.014170945).epsilon(1e-9));
  CHECK(bareLstar < bareL - 1e-6); // it was bit-equal to bareL before

  // --entropy-corrected counts nodes, so the pass-through the padding stacks above A's
  // module is discounted -- but A's own module is not phantom and is counted.
  const double bareBiased = scoreFixedPartition("twotriangles_flow.net", "twotriangles_top_level_leaf.tree", "--non-redundant --entropy-corrected");
  const double moduleBiased = scoreFixedPartition("twotriangles_flow.net", "twotriangles_top_level_leaf_module.tree", "--non-redundant --entropy-corrected");
  const double rectBiased = scoreFixedPartition("twotriangles_flow.net", "twotriangles_top_level_leaf_rect.tree", "--non-redundant --entropy-corrected");
  CHECK(bareBiased == doctest::Approx(moduleBiased).epsilon(1e-9));
  CHECK(rectBiased > bareBiased + 1e-9); // the hand-written pass-through is a real node
}

TEST_CASE("NonRedundant/columnar: an all-top-level tree never becomes ragged in the first place [fast][core][non-redundant]")
{
  // REGRESSION GUARD, not evidence for the padding: this case passes unchanged on the
  // pre-change binary (both values 3.220279696 there too, no fallback line). It is here
  // to pin that the padding did not disturb the shape it does NOT handle.
  //
  // Every path in this fixture is one level deep, so initTree's `maxDepth == 2 ||
  // twoLevel` shortcut fires, initPartition turns every top-level id into a real
  // module, and padLeafPathsToUniformDepth is handed a rectangular two-level tree --
  // no empty path, nothing to pad, no ragged bail. The empty path the synthetic module
  // id exists for needs a file that ALSO carries a path deeper than 2; that is the BARE
  // top-level leaf case above, which is the one that fails on the pre-change binary.
  //
  // What this still checks: one partition (one module per node) written two ways, as a
  // depth-1 .tree and as a .clu, must score the same, and L* must undercut base L.
  const double flatTreeLstar = scoreFixedPartition("twotriangles_flow.net", "twotriangles_all_top_level.tree", "--non-redundant");
  const double cluLstar = scoreFixedPartition("twotriangles_flow.net", "twotriangles_singletons.clu", "--non-redundant");
  CHECK(flatTreeLstar == doctest::Approx(cluLstar).epsilon(1e-12));

  const double flatTreeL = scoreFixedPartition("twotriangles_flow.net", "twotriangles_all_top_level.tree", "");
  CHECK(flatTreeLstar < flatTreeL - 1e-6);
}

TEST_CASE("Columnar: the preferred-modules penalty survives the object-oriented fallback [fast][core][non-redundant]")
{
  // --preferred-number-of-modules is the only correction with no object-oriented
  // counterpart, so a tree that falls back to calcCodelengthOnTree used to lose the
  // whole |K - K_pref| penalty without a word. The base objective does not pad (padding
  // is not free for it), so the ragged tree still takes the fallback -- the penalty has
  // to be added there by hand.
  // --columnar explicitly: this is a columnar-engine contract, and the object-oriented
  // engine has no such penalty to check for.
  const double ragged = scoreFixedPartition("twotriangles_flow.net", "twotriangles_ragged_branches.tree", "--columnar");
  // The ragged fixture has three leaf modules: 1, 2:1 and 2:2.
  for (unsigned int preferred = 1; preferred <= 6; ++preferred) {
    const double biased = scoreFixedPartition("twotriangles_flow.net",
                                              "twotriangles_ragged_branches.tree",
                                              "--columnar --preferred-number-of-modules " + std::to_string(preferred));
    const double expectedPenalty = std::abs(3.0 - static_cast<double>(preferred));
    CHECK(biased == doctest::Approx(ragged + expectedPenalty).epsilon(1e-9));
  }
}

} // namespace test
} // namespace infomap
