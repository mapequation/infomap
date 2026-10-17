#include "TestUtils.h"
#include "io/InfomapError.h"

#include <cmath>
#include <memory>
#include <utility>
#include <vector>

// Invariant tests for the map-equation objective family.
//
// After a search, the optimizer's *tracked* codelength (getCodelength(), updated
// incrementally by updateCodelengthOnMovingNode as nodes move) must equal a
// *fresh recompute* of the resulting partition (calcCodelengthOnTree, which
// recomputes every module from its children rather than returning any running
// value). The search loop relies on this equality; when it drifts, the reported
// codelength is wrong even if the partition is fine -- the class of OO-engine
// codelength-consistency bugs (#830/#831/#837).
//
// The objective is chosen from the input/flags (see InfomapBase::initOptimizer):
//   ordinary network      -> BiasedMapEquation (the default)
//   memory/state network  -> MemMapEquation
//   --meta-data           -> MetaMapEquation
//   memory + regularized  -> RegularizedMultilayerMapEquation (feature-gated)
// so each TEST_CASE picks its objective purely through configuration.
//
// getCodelength()/calcCodelengthOnTree are private on InfomapBase; this TU is
// compiled with -fno-access-control (see CMakeLists.txt) to reach them.

namespace infomap {
namespace test {

namespace {

// Two-level invariant: the incrementally tracked codelength must match a fresh
// recompute of the resulting tree.
void checkTrackedMatchesRecompute(InfomapWrapper& im)
{
  const double tracked = im.getCodelength();
  const double recomputed = im.calcCodelengthOnTree(im.root(), true);
  INFO("tracked=" << tracked << " recomputed=" << recomputed);
  checkApproxCodelength(tracked, recomputed);
}

} // namespace

TEST_CASE("MapEquation invariant: BiasedMapEquation (ordinary), tracked == recompute [fast][core][mapeq][biased]")
{
  InfomapWrapper im(defaultFlags("--two-level"));
  im.readInputData(networkFixturePath("lossy_benchmark.net"));
  im.run();
  checkTrackedMatchesRecompute(im);
}

// The same invariant with a module-count preference in play. The existing biased
// case above passes at the default --preferred-number-of-modules 0, where the
// |K - K_pref| term is identically zero, so it never covered the term itself.
//
// #1021: biasedCost is one scalar for the whole partition, so calcCodelength has
// no per-node share to return and calcCodelengthOnTree's sum lost the term
// completely. The tracked side kept it, so the two disagreed by |K - K_pref| --
// unbounded in K_pref, and 7 bits on a six-node fixture.
TEST_CASE("MapEquation invariant: BiasedMapEquation with a module preference, tracked == recompute [fast][core][mapeq][biased]")
{
  for (const unsigned int preference : { 1u, 3u, 40u }) {
    InfomapWrapper im(defaultFlags("--two-level --preferred-number-of-modules " + std::to_string(preference)));
    im.readInputData(networkFixturePath("lossy_benchmark.net"));
    im.run();
    INFO("--preferred-number-of-modules " << preference << ", found " << im.numTopModules());
    // The invariant is vacuous where the search satisfies the preference exactly,
    // since |K - K_pref| is then zero and the term under test never appears. At
    // least one of these has to leave a residual, or the case proves nothing --
    // 40 exceeds what this fixture can be split into.
    if (preference == 40u)
      REQUIRE(im.numTopModules() != preference);
    checkTrackedMatchesRecompute(im);
  }
}

// The one-level reference has to price the same objective as the partition it is
// compared against. m_oneLevelCodelength was calcCodelength on the bare root -- a
// tree with no module level at all -- so a partition-level term was missing from it
// while the reported codelength carried one. Scoring the one-module partition then
// gave `Relative savings` of -84% against a reference that is that very partition,
// and the one-level collapse installs this value verbatim on a tree that does have
// one module (#1020).
//
// The entropy-corrected half of that issue is no longer reachable: #1033 gave a
// module that cannot be left no exit codeword, which made the lone module's
// correction match the bare root's. --preferred-number-of-modules is what still
// showed it, and its cost is exactly |K - K_pref| so the expected values follow.
TEST_CASE("The one-level reference prices the same objective as a one-module partition [fast][core][mapeq][biased]")
{
  for (const unsigned int preference : { 0u, 1u, 5u, 9u }) {
    const auto flags = preference == 0u
        ? std::string("--two-level --no-infomap --cluster-data ")
        : "--two-level --preferred-number-of-modules " + std::to_string(preference) + " --no-infomap --cluster-data ";
    InfomapWrapper im(defaultFlags(flags + clusterFixturePath("twotriangles_single_module.clu")));
    im.readInputData(repoPath("test/fixtures/graphs/twotriangles_unweighted.edges"));
    im.run();

    REQUIRE(im.numTopModules() == 1);
    INFO("--preferred-number-of-modules " << preference
                                          << " one-level=" << im.getOneLevelCodelength()
                                          << " codelength=" << im.codelength());
    // The partition under test *is* the one-module partition, so the reference and
    // the reported codelength are the same number and the saving is zero.
    checkApproxCodelength(im.getOneLevelCodelength(), im.codelength());
    CHECK(im.getRelativeCodelengthSavings() == doctest::Approx(0.0).epsilon(1e-9));
  }
}

// The user-visible half of #1021, and the shape the issue reported: scoring a
// tracked partition with --no-infomap never moved, whatever K_pref was, because
// the reported number on that path is calcCodelengthOnTree's sum. The partition
// here has two modules, so the term is exactly |2 - K_pref| and the expected
// values follow from the unbiased run rather than from a hardcoded constant.
TEST_CASE("A module preference reaches a scored partition, not only the search [fast][core][mapeq][biased]")
{
  const auto scoredCodelength = [](const std::string& extraFlags) {
    InfomapWrapper im(defaultFlags(
        "--two-level --no-infomap --cluster-data "
        + clusterFixturePath("twotriangles_two_modules.clu") + " " + extraFlags));
    im.readInputData(repoPath("test/fixtures/graphs/twotriangles_unweighted.edges"));
    im.run();
    REQUIRE(im.numTopModules() == 2);
    return im.codelength();
  };

  const double unbiased = scoredCodelength("");

  for (const unsigned int preference : { 1u, 2u, 5u, 9u }) {
    const double expected = unbiased + std::abs(2.0 - static_cast<double>(preference));
    const double scored = scoredCodelength("--preferred-number-of-modules " + std::to_string(preference));
    INFO("K_pref=" << preference << " scored=" << scored << " expected=" << expected);
    checkApproxCodelength(scored, expected);
  }
}

// The same term through the recursive search. recursivePartition rebuilt the total
// from the top modules' terms, which do not hold the module-count term, so a deep
// tree with K != K_pref top modules was reported |K - K_pref| short: 3.579251676 for
// ninetriangles with -s 3 on macOS, against 4.579251676 for the same tree scored
// with --no-infomap. Seeds reach different trees on different standard libraries
// (std::uniform_int_distribution is implementation-defined), so this sweeps seeds
// and requires that some run reaches the case.
TEST_CASE("A module preference reaches a deep tree from the recursive search [fast][core][mapeq][biased]")
{
  const std::vector<std::string> networks {
    networkFixturePath("unbalanced_hierarchy.net"),
    repoPath("examples/networks/ninetriangles.net"),
  };
  unsigned int numDeepTreesOffPreference = 0;
  for (const auto& network : networks) {
    for (unsigned int seed = 1; seed <= 20; ++seed) {
      InfomapWrapper im(defaultFlags("--preferred-number-of-modules 2 --seed " + std::to_string(seed)));
      im.readInputData(network);
      im.run();
      if (im.numLevels() > 2 && im.numTopModules() != 2)
        ++numDeepTreesOffPreference;

      const double reported = im.codelength();
      const double recomputed = im.calcCodelengthOnTree(im.root(), true);
      INFO(network << " seed " << seed << ": " << im.numLevels() << " levels, " << im.numTopModules() << " top modules");
      INFO("reported=" << reported << " recomputed=" << recomputed);
      checkApproxCodelength(reported, recomputed);
    }
  }
  REQUIRE(numDeepTreesOffPreference > 0);
}

TEST_CASE("MapEquation invariant: MemMapEquation (memory), tracked == recompute [fast][core][mapeq][mem]")
{
  InfomapWrapper im(defaultFlags("--two-level"));
  im.readInputData(networkFixturePath("states.net"));
  im.run();
  checkTrackedMatchesRecompute(im);
}

TEST_CASE("MapEquation invariant: MetaMapEquation (meta-data), tracked == recompute [fast][core][mapeq][meta]")
{
  // states_crossing.meta on purpose, not states.meta: the latter's categories
  // (1,1,1,2,2,2) coincide exactly with the optimal partition of states.net, so
  // metaCodelength is 0 at every initPartition and every per-move deltaMetaL is
  // 0. Both sides of the equality then equal the plain memory value and the case
  // is numerically identical to the MemMapEquation one above -- any bug in the
  // incremental tracking of the meta term, which is the only thing this case adds,
  // would pass unnoticed.
  InfomapWrapper im(defaultFlags("--two-level --meta-data " + repoPath("test/fixtures/meta/states_crossing.meta")));
  im.readInputData(networkFixturePath("states.net"));
  im.run();

  // Guard against silently degenerating again: without a meta term there is
  // nothing for the meta objective to track incrementally, so the invariant below
  // would hold for the wrong reason.
  const double metaCodelength = im.getMetaCodelength();
  INFO("metaCodelength=" << metaCodelength);
  CHECK(metaCodelength > 0.0);

  checkTrackedMatchesRecompute(im);
}

// Regression for #1138: coarse-tune splits each module into sub-modules and
// consolidates them, and by then InfomapOptimizer has overwritten each
// sub-module's index with its former module's index. MetaMapEquation looked the
// sub-modules' metadata up by that index, so they received each other's
// collections. The tune loop runs coarse-tune under --two-level too, so the case
// above reaches it, but no module of states.net splits and the two indices never
// diverge there. Here they do: before the fix the tracked meta term fell to 0 and
// the run reported 3.4965 against 5.0757 recomputed from the leaves.
TEST_CASE("MapEquation invariant: MetaMapEquation through coarse-tune, tracked == recompute (repro #1138) [fast][core][mapeq][meta]")
{
  InfomapWrapper im(defaultFlags("--two-level --meta-data " + repoPath("test/fixtures/meta/ninetriangles_crossing.meta")));
  im.readInputData(repoPath("examples/networks/ninetriangles.net"));
  im.run();

  const double metaCodelength = im.getMetaCodelength();
  INFO("metaCodelength=" << metaCodelength);
  CHECK(metaCodelength > 0.0);

  checkTrackedMatchesRecompute(im);
}

#if INFOMAP_FEATURE_REGULARIZED_MULTILAYER
TEST_CASE("MapEquation invariant: RegularizedMultilayerMapEquation, tracked == recompute [fast][core][mapeq][regularized]")
{
  // Regularized multilayer flow requires *Intra/*Inter input (not *Multilayer).
  InfomapWrapper im(defaultFlags("--two-level --regularized"));
  im.readInputData(networkFixturePath("intra_inter.net"));
  im.run();
  checkTrackedMatchesRecompute(im);
}
#endif

// Regression for #830: the tracked codelength used to drift from a fresh
// recompute under --entropy-corrected (before this fix: tracked 2.9815 against
// recompute 2.9911), because the recompute charged the root index codebook
// childDegree free parameters where it has childDegree - 1 -- an index codebook
// has no exit codeword. Deeper trees are not covered: there the recompute also
// charges the intermediate module-of-modules codebooks that the tracked side
// never sees. The reported codelength of a deep tree is covered by #1176 below.
TEST_CASE("MapEquation invariant: entropy-corrected tracked == recompute (repro #830) [core][mapeq][entropy-corrected]")
{
  InfomapWrapper im(defaultFlags("--two-level --entropy-corrected"));
  im.readInputData(networkFixturePath("lossy_benchmark.net"));
  im.run();
  checkTrackedMatchesRecompute(im);
}

// The recursive search builds a module of modules from a sub-Infomap, whose root
// has no exit codeword, and used to keep that root's term. In the main tree the
// module does have one, so under --entropy-corrected each module of modules below
// the root was one free parameter short, in the reported codelength and in the
// per-level table (#1176: 2.372190815 against 2.391846060 for unbalanced_hierarchy
// with -s 4 on macOS). Swept over seeds, as the module-preference case above.
TEST_CASE("Entropy correction charges every codebook of a deep tree (repro #1176) [fast][core][mapeq][entropy-corrected]")
{
  const std::vector<std::pair<std::string, std::string>> inputs {
    { networkFixturePath("unbalanced_hierarchy.net"), "--entropy-corrected --preferred-number-of-modules 2" },
    { repoPath("examples/networks/modular_w.net"), "--entropy-corrected -d" },
  };
  unsigned int numDeepTrees = 0;
  for (const auto& input : inputs) {
    for (unsigned int seed = 1; seed <= 20; ++seed) {
      InfomapWrapper im(defaultFlags(input.second + " --seed " + std::to_string(seed)));
      im.readInputData(input.first);
      im.run();
      if (im.numLevels() > 2)
        ++numDeepTrees;

      // The per-level table reads the node terms; the module-count term belongs to no node.
      double sumOfNodeTerms = im.calcTreeCodelengthCost(im.numTopModules());
      for (const auto& node : im.root().infomapTree()) {
        if (!node.isLeaf())
          sumOfNodeTerms += node.codelength;
      }
      const double reported = im.codelength();
      const double recomputed = im.calcCodelengthOnTree(im.root(), true);
      INFO(input.first << " " << input.second << " seed " << seed << ": " << im.numLevels() << " levels, " << im.numTopModules() << " top modules");
      INFO("reported=" << reported << " node terms=" << sumOfNodeTerms << " recomputed=" << recomputed);
      checkApproxCodelength(reported, recomputed);
      checkApproxCodelength(sumOfNodeTerms, recomputed);
    }
  }
  REQUIRE(numDeepTrees > 0);
}

// The size of the correction, on a partition pinned from a file so no search
// enters the comparison. twotriangles has N = 6 nodes and total degree D = 14,
// and the codebooks of an m-module partition hold m - 1 + N free parameters:
// m - 1 in the index codebook and, per module, its nodes plus an exit codeword
// less the codebook's own normalisation. Miller-Madow charges (K - 1) / (2n)
// nats per codebook and the map equation is in bits, so each parameter costs
// 1 / (2 D ln2) bits -- the ln2 is what makes the corrected codelength of a
// fixed partition stay flat as links are removed.
TEST_CASE("Entropy correction charges m - 1 + N parameters at 1/(2 D ln2) bits [fast][core][mapeq][entropy-corrected]")
{
  const auto codelengthOf = [](const std::string& flags) {
    InfomapWrapper im(defaultFlags("--two-level --no-infomap --cluster-data "
                                   + clusterFixturePath("twotriangles_two_modules.clu") + " " + flags));
    im.readInputData(repoPath("examples/networks/twotriangles.net"));
    im.run();
    return im.codelength();
  };

  const double correction = codelengthOf("--entropy-corrected") - codelengthOf("");
  const double expected = (2 - 1 + 6) / (2 * 14.0 * std::log(2.0));
  INFO("correction=" << correction << " expected=" << expected);
  checkApproxCodelength(correction, expected);
}

// A module holding the whole network cannot be exited, so its codebook has no
// exit codeword and the partition has one free parameter less: N - 1, not N.
// Unlike an unobserved node or an unobserved boundary link -- which the declared
// alphabet keeps charging, since a sample that misses them does not make them
// impossible -- this codeword cannot occur in any sample. The index codebook of
// a single module is likewise never used, and both sides of the equality have to
// agree about that.
TEST_CASE("A single module covering the network has no exit codeword [fast][core][mapeq][entropy-corrected]")
{
  const auto run = [](const std::string& flags) {
    auto im = std::make_unique<InfomapWrapper>(defaultFlags("--two-level --no-infomap --cluster-data "
                                                            + clusterFixturePath("twotriangles_single_module.clu") + " " + flags));
    im->readInputData(repoPath("examples/networks/twotriangles.net"));
    im->run();
    return im;
  };

  const auto plain = run("");
  const auto corrected = run("--entropy-corrected");

  const double correction = corrected->codelength() - plain->codelength();
  const double expected = (6 - 1) / (2 * 14.0 * std::log(2.0));
  INFO("correction=" << correction << " expected=" << expected);
  checkApproxCodelength(correction, expected);

  checkTrackedMatchesRecompute(*corrected);
}

// The entropy-bias correction must reach the move loop, not only the reported
// codelength: its delta was gated behind preferredNumModules, so under the
// default --preferred-number-of-modules 0 the search optimized the uncorrected
// map equation and only the printed number changed (#904). Asserted as an
// effect on the returned partition, since that is what a user sees.
TEST_CASE("Entropy correction steers the search, not just the report [core][mapeq][entropy-corrected]")
{
  const auto partitionOf = [](const std::string& flags) {
    InfomapWrapper im(defaultFlags("--two-level -N 1 --seed 42 " + flags));
    im.readInputData(repoPath("examples/networks/ninetriangles.net"));
    im.run();
    // Canonicalized, i.e. compared up to relabeling: module ids are not stable
    // labels, so comparing the raw {node -> module} maps would let a pure
    // renumbering read as a changed clustering and pass this test for the wrong
    // reason.
    return canonicalPartition(im.getModules(1, false));
  };

  const auto uncorrected = partitionOf("");
  // At the documented default strength the correction term is ~1e-4 bits here,
  // far too small to move a node; strength 10 makes it decisive.
  const auto corrected = partitionOf("--entropy-corrected --entropy-correction-strength 10");

  CHECK(uncorrected != corrected);
}

// The super-level Infomap is built without memory, which means from a default Config, so its
// objective ran with the entropy correction off and no module-count preference however the run was
// configured (#904). findHierarchicalSuperModules compares that codelength straight against the
// main objective's index codelength, so an uncorrected super solution could read as an improvement,
// be consolidated, and leave the trial describing the network in more bits than the two-level
// partition it had already found and thrown away (measured on this input: 3.7885 against 3.7421).
//
// Asserted as the user-visible invariant rather than on the internal comparison: the hierarchical
// search starts from the two-level solution and only ever adds levels that shorten the description,
// so it must not come back with a worse codelength than a two-level run of the same trial. Note
// that --preferred-number-of-levels deliberately breaks this invariant (#308 lets a depth
// preference accept a longer description), which is why no depth preference is set here.
TEST_CASE("Super level never leaves the trial worse than its own two-level solution [core][mapeq][entropy-corrected]")
{
  const auto codelengthOf = [](const std::string& flags) {
    InfomapWrapper im(defaultFlags("--seed 3 " + flags));
    im.readInputData(repoPath("examples/networks/ninetriangles.net"));
    im.run();
    return im.codelength();
  };

  const double twoLevel = codelengthOf("--two-level --entropy-corrected");
  const double hierarchical = codelengthOf("--entropy-corrected");

  INFO("two-level=" << twoLevel << " hierarchical=" << hierarchical);
  CHECK(hierarchical <= twoLevel + 1e-12);

  // Guard against the invariant holding for the wrong reason: it is trivially true if the
  // correction is so weak here that both runs return the same solution anyway. Stated as a
  // magnitude rather than an ordering on purpose -- the two values come from different objectives
  // and possibly different partitions, so neither direction is guaranteed a priori, while a gap
  // this size can only come from an active correction (it is 35/156 = 0.2244 bits here, the
  // correction term for the nine-module partition both runs find).
  const double uncorrected = codelengthOf("--two-level");
  INFO("uncorrected two-level=" << uncorrected);
  CHECK(std::abs(twoLevel - uncorrected) > 0.1);
}

// The tree has no self-links, but a node whose only out-link is a self-link is not dangling:
// the flow follows the self-link, so the node teleports at rate alpha like any other and the
// teleport flow sums to alpha. Without the self-link the node is dangling in both places (#1182).
TEST_CASE("Recorded teleportation does not take a node with only a self-link for dangling [fast][core][mapeq]")
{
  for (const bool selfLink : { true, false }) {
    INFO("selfLink=" << selfLink);
    InfomapWrapper im(defaultFlags("--directed --recorded-teleportation --no-infomap"));
    for (const auto& link : std::vector<std::pair<unsigned int, unsigned int>> { { 1, 2 }, { 2, 3 }, { 3, 1 }, { 3, 4 }, { 4, 5 }, { 5, 6 }, { 6, 4 }, { 6, 7 } })
      im.addLink(link.first, link.second);
    if (selfLink)
      im.addLink(7, 7);
    im.setInitialPartition({ { 1, 1 }, { 2, 1 }, { 3, 1 }, { 4, 2 }, { 5, 2 }, { 6, 2 }, { 7, 2 } });
    im.run();

    double sumTeleportFlow = 0.0;
    for (const auto* leaf : im.leafNodes())
      sumTeleportFlow += leaf->data.teleportFlow;
    if (selfLink) {
      CHECK(sumTeleportFlow == doctest::Approx(0.15).epsilon(1e-12));
      checkApproxCodelength(im.codelength(), 2.033045821, 1e-9);
    } else {
      CHECK(sumTeleportFlow > 0.15);
      checkApproxCodelength(im.codelength(), 2.637456030, 1e-9);
    }
  }
}

#if INFOMAP_CHECK_INVARIANTS
// The flow invariants are compiled into debug, sanitizer and fuzz builds and run after
// each phase (#1171). They hold on a real run, and they catch a module whose exit flow
// no longer balances against its children's.
TEST_CASE("Flow invariants hold after a run and catch an unbalanced module [fast][core][mapeq]")
{
  InfomapWrapper im(defaultFlags());
  im.readInputData(repoPath("examples/networks/ninetriangles.net"));
  im.run();
  CHECK_NOTHROW(im.checkInvariants(im.root(), "a test"));

  auto& module = *im.root().firstChild;
  module.data.exitFlow += 0.01;
  CHECK_THROWS_AS(im.checkInvariants(im.root(), "a test"), InfomapError);
}

// Enter and exit flow scale with the Markov time, so they are checked relative to it. At
// t = 1e7 the rounding of the boundary rates alone exceeds an absolute 1e-9.
TEST_CASE("Flow invariants accept the boundary rates of a large Markov time [fast][core][mapeq]")
{
  // --directed is left out: at this Markov time the run's own negative-module-flow guard, which
  // has an absolute tolerance, already refuses it.
  for (const std::string flags : { "--flow-model rawdir", "" }) {
    CAPTURE(flags);
    InfomapWrapper im(defaultFlags("--markov-time 1e7 -N 1 --seed 1 " + flags));
    im.readInputData(repoPath("examples/networks/modular_wd.net"));
    CHECK_NOTHROW(im.run());
  }
}

// Whether the leaf flow must sum to 1 is decided per run. A link-free run after a linked one
// on the same instance has zero flow, which is its correct result, not a violation.
TEST_CASE("Flow invariants accept a link-free run on an instance that ran a linked network [fast][core][mapeq]")
{
  InfomapWrapper im(defaultFlags("-N 1 --seed 7"));
  im.addLink(1, 2);
  im.addLink(2, 3);
  im.addLink(3, 1);
  CHECK_NOTHROW(im.run());
  CHECK(im.m_flowSumsToOne);

  im.network().clear();
  im.addNode(1);
  im.addNode(2);
  CHECK_NOTHROW(im.run());
  CHECK_FALSE(im.m_flowSumsToOne);
  CHECK(im.numLeafNodes() == 2);
}
#endif

// Open bugs found by the fuzz_run target (#1171), stated as what should happen and
// marked should_fail, so each turns red when its bug is fixed. Release builds refuse
// some through the negative-flow guard and report a codelength for the others; builds
// with the invariants refuse all of them. When one is fixed, drop its marker here and
// its entry in isKnownFinding() in test/fuzz/fuzz_run.cpp.
namespace {

void checkLeafFlowBalances(InfomapWrapper& im)
{
  CHECK_NOTHROW(im.run());
  double sumEnter = 0.0;
  double sumExit = 0.0;
  for (const auto& node : im.network().nodes()) {
    sumEnter += node.second.enterFlow;
    sumExit += node.second.exitFlow;
  }
  INFO("leaf enter flow " << sumEnter << ", exit flow " << sumExit);
  CHECK(sumExit == doctest::Approx(sumEnter));
}

} // namespace

// Markov time scales the link flow but not the node exit flow that the regularized
// path sets, so above 1 a module's internal link flow outweighs its nodes' exit flow
// and the module is left with negative enter and exit flow. --variable-markov-time
// does the same wherever it scales a node's links up.
TEST_CASE("Fuzz finding: --regularized with --markov-time above 1 keeps module flow non-negative (#1191) [fast][core][mapeq]" * doctest::should_fail())
{
  InfomapWrapper im(defaultFlags("--regularized --markov-time 2"));
  im.addLink(1, 2);
  CHECK_NOTHROW(im.run());
}

TEST_CASE("Fuzz finding: --regularized with --variable-markov-time keeps module flow non-negative (#1191) [fast][core][mapeq]" * doctest::should_fail())
{
  InfomapWrapper im(defaultFlags("--directed --regularized --variable-markov-time"));
  im.readInputData(repoPath("examples/networks/modular_w.net"));
  CHECK_NOTHROW(im.run());
}

// The recorded-teleportation enter and exit terms assume the directed model's
// teleporting flow. Under undirdir, outdirdir, rawdir and precomputed flow they come on
// top of link flow that already sums to 1, so the leaves exit at a higher rate than they are entered. On
// a single link a release run reports codelength 2 against a one-level codelength of 0.
TEST_CASE("Fuzz finding: --recorded-teleportation balances leaf flow under undirdir (#1188) [fast][core][mapeq]" * doctest::should_fail())
{
  InfomapWrapper im(defaultFlags("--flow-model undirdir --recorded-teleportation"));
  im.addLink(1, 2);
  checkLeafFlowBalances(im);
}

TEST_CASE("Fuzz finding: --recorded-teleportation balances leaf flow under outdirdir (#1188) [fast][core][mapeq]" * doctest::should_fail())
{
  InfomapWrapper im(defaultFlags("--flow-model outdirdir --recorded-teleportation"));
  im.addLink(1, 2);
  checkLeafFlowBalances(im);
}

TEST_CASE("Fuzz finding: --recorded-teleportation balances leaf flow under rawdir (#1188) [fast][core][mapeq]" * doctest::should_fail())
{
  InfomapWrapper im(defaultFlags("--flow-model rawdir --recorded-teleportation"));
  im.addLink(1, 2);
  checkLeafFlowBalances(im);
}

TEST_CASE("Fuzz finding: --recorded-teleportation balances leaf flow under precomputed flow (#1188) [fast][core][mapeq]" * doctest::should_fail())
{
  InfomapWrapper im(defaultFlags("--flow-model precomputed --recorded-teleportation"));
  im.readInputData(networkFixturePath("twotriangles_flow.net"));
  checkLeafFlowBalances(im);
}

// Recorded teleportation over a node flow that is not the coded walk's own (undirdir,
// outdirdir or rawdir under --regularized, or --use-node-weights-as-flow): the finished tree can hold
// a module of modules whose enter and exit flow do not balance against its children's.
// A release run then reports a worse tree than the recursive search found, e.g.
// ninetriangles.net with --recorded-teleportation --use-node-weights-as-flow --seed 46
// (libc++): 4.32 bits, against 3.64 after the recursive step. It needs particular
// search paths, and the seeds that hit differ between standard libraries, so sweep them.
TEST_CASE("Fuzz finding: recorded teleportation over a foreign node flow balances module flow in the finished tree (#1189) [fast][core][mapeq]" * doctest::should_fail())
{
  const auto moduleFlowBalances = [](InfomapWrapper& im) {
    try {
      im.run();
    } catch (const InfomapError&) {
      return false; // the invariants, in builds that have them
    }
    for (auto& node : im.root().infomapTree()) {
      if (node.isLeaf() || node.isRoot())
        continue;
      double sumEnter = 0.0;
      double sumExit = 0.0;
      for (const InfoNode& child : node) {
        sumEnter += child.data.enterFlow;
        sumExit += child.data.exitFlow;
      }
      if (std::abs((sumExit - node.data.exitFlow) - (sumEnter - node.data.enterFlow)) > 1e-9)
        return false;
    }
    return true;
  };

  unsigned int numUnbalanced = 0;
  for (unsigned int seed = 1; seed <= 200; ++seed) {
    InfomapWrapper im(defaultFlags("--recorded-teleportation --use-node-weights-as-flow --seed " + std::to_string(seed)));
    im.readInputData(repoPath("examples/networks/ninetriangles.net"));
    if (!moduleFlowBalances(im))
      ++numUnbalanced;
  }
  for (unsigned int seed = 1; seed <= 100; ++seed) {
    InfomapWrapper im(defaultFlags("--flow-model outdirdir --regularized --core-loop-limit 1 --seed " + std::to_string(seed)));
    for (const auto& link : std::vector<std::pair<unsigned int, unsigned int>> { { 0, 2 }, { 1, 3 }, { 2, 3 }, { 4, 5 }, { 5, 6 }, { 7, 9 }, { 3, 7 } })
      im.addLink(link.first, link.second);
    im.addLink(5, 1, 0.8);
    if (!moduleFlowBalances(im))
      ++numUnbalanced;
  }
  CHECK(numUnbalanced == 0);
}

// --variable-markov-time scales each link by the largest node flow over the local one.
// With --use-node-weights-as-flow an isolated node can hold that largest flow, and the
// run is refused with negative enter flow on a linked node.
TEST_CASE("Fuzz finding: --variable-markov-time with --use-node-weights-as-flow keeps flow non-negative (#1191) [fast][core][mapeq]" * doctest::should_fail())
{
  InfomapWrapper im(defaultFlags("--variable-markov-time --use-node-weights-as-flow"));
  im.addNode(3);
  im.addLink(1, 2);
  CHECK_NOTHROW(im.run());
}

// The relax-to-self relay in the flow calculation treats any link between two states of
// one physical node as a layer switch and relays its flow onward, on state networks
// too. Under --regularized that extra link flow leaves a module with negative enter flow.
TEST_CASE("Fuzz finding: --regularized with --multilayer-relax-to-self keeps module flow non-negative on states (#1191) [fast][core][mapeq]" * doctest::should_fail())
{
  InfomapWrapper im(defaultFlags("--regularized --multilayer-relax-to-self"));
  im.addStateNode(1, 1);
  im.addStateNode(2, 2);
  im.addStateNode(3, 3);
  im.addStateNode(4, 2);
  im.addLink(2, 3, 1.0);
  im.addLink(4, 2, 0.2);
  CHECK_NOTHROW(im.run());
}

// Only the *Bipartite section is checked for links that cross the start id, so feature
// nodes can still be linked to each other. Directed PageRank with
// --bipartite-teleportation then diverges to a total node flow of 1.7e27. A release run
// warns about it, searches with codelengths of that size, and is refused by the
// negative-flow guard at the end.
TEST_CASE("Fuzz finding: --bipartite-teleportation keeps directed flow normalized (#1190) [fast][core][mapeq]" * doctest::should_fail())
{
  InfomapWrapper im(defaultFlags("--directed --bipartite-teleportation -N 1 --seed 1"));
  im.setBipartiteStartId(4);
  im.addLink(2, 4, 1.0);
  im.addLink(4, 6, 3.0);
  im.addLink(6, 7, 8.0);
  im.addLink(6, 8, 5.0);
  im.addLink(8, 10, 2.0);
  im.addLink(7, 11, 3.0);
  im.addLink(10, 33, 1.0);
  im.addLink(33, 35, 1.0);
  im.addLink(35, 36, 1.0);
  im.addLink(36, 10, 0.5);
  CHECK_NOTHROW(im.run());
  double sumFlow = 0.0;
  for (const auto& node : im.network().nodes())
    sumFlow += node.second.flow;
  CHECK(sumFlow == doctest::Approx(1.0));
}

// --use-node-weights-as-flow replaces the node flow after the link flow is computed,
// and the regularized path takes enter and exit flow from the node flow. Where a node's
// weight gives it less flow than its links carry, enter flow goes negative, on the node
// itself or on the module that holds it.
TEST_CASE("Fuzz finding: --regularized with --use-node-weights-as-flow keeps flow non-negative (#1191) [fast][core][mapeq]" * doctest::should_fail())
{
  InfomapWrapper im(defaultFlags("--regularized --use-node-weights-as-flow"));
  // Unit node weights give each node a third of the flow; the links give node 7 a twentieth.
  im.addLink(36, 10, 0.5);
  im.addLink(7, 10, 0.05);
  CHECK_NOTHROW(im.run());
}

// Follow-ups (not reproduced here): #831 (two-level initPartition/consolidate
// reentrancy corrupting the recomputed codelength after a prior multi-level
// trial) and #837 (hierarchical super-step degrading a two-level memory
// partition via a wrong super-index consolidation value) are the same
// tracked-vs-recompute drift class but need dedicated multi-trial / hierarchical
// setups to isolate. Add targeted should_fail cases when those are pinned down.

} // namespace test
} // namespace infomap

