#include "TestUtils.h"

#include <cmath>
#include <memory>

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
                                          << " reference=" << im.getReferenceOneLevelCodelength()
                                          << " raw=" << im.getOneLevelCodelength()
                                          << " codelength=" << im.codelength());
    // The partition under test *is* the one-module partition, so the reference and
    // the reported codelength are the same number and the saving is zero. Asserted
    // on the *reference* accessor, which is what the summary prints and what
    // getRelativeCodelengthSavings divides by; the raw field stays the bare root's
    // value because it also scales partition()'s tuning-termination threshold.
    checkApproxCodelength(im.getReferenceOneLevelCodelength(), im.codelength());
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
// never sees, and #830 stays open for that half.
TEST_CASE("MapEquation invariant: entropy-corrected tracked == recompute (repro #830) [core][mapeq][entropy-corrected]")
{
  InfomapWrapper im(defaultFlags("--two-level --entropy-corrected"));
  im.readInputData(networkFixturePath("lossy_benchmark.net"));
  im.run();
  checkTrackedMatchesRecompute(im);
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

// Follow-ups (not reproduced here): #831 (two-level initPartition/consolidate
// reentrancy corrupting the recomputed codelength after a prior multi-level
// trial) and #837 (hierarchical super-step degrading a two-level memory
// partition via a wrong super-index consolidation value) are the same
// tracked-vs-recompute drift class but need dedicated multi-trial / hierarchical
// setups to isolate. Add targeted should_fail cases when those are pinned down.

} // namespace test
} // namespace infomap

