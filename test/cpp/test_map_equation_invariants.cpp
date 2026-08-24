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

namespace {

  // The reporting invariant: the per-level table the summary prints, which sums
  // InfoNode::codelength level by level, must add up to the codelength the run
  // reports. On the columnar path those per-node values used to come from a
  // different objective than the total -- the object-oriented base map equation,
  // so under --non-redundant the table showed L while the headline was L* -- or
  // from nowhere at all when initTree took its flat shortcut, which scores no
  // node and left every entry at 0.000000.
  void checkPerLevelTableSumsToCodelength(InfomapWrapper& im, const std::string& what)
  {
    std::vector<detail::PerLevelStat> perLevel;
    aggregatePerLevelCodelength(im.root(), perLevel);
    double sum = 0.0;
    for (const auto& level : perLevel)
      sum += level.codelength();
    INFO(what << ": per-level total=" << sum << " reported=" << im.codelength());
    CHECK(sum == doctest::Approx(im.codelength()).epsilon(1e-9));
    CHECK(sum > 0.0);
  }

  void runAndCheckPerLevelTable(const std::string& flags, const std::string& network)
  {
    InfomapWrapper im(defaultFlags(flags));
    im.readInputData(repoPath(network));
    im.run();
    checkPerLevelTableSumsToCodelength(im, flags + " on " + network);
  }

} // namespace

TEST_CASE("Reporting: the per-level table totals the reported codelength [fast][core][mapeq][columnar-contract]")
{
  // Both engines, both objectives, flat and hierarchical winners. --two-level is the
  // arm that used to print a table of zeros under --columnar: the search value comes
  // off the columnar stack and initTree's flat shortcut scores no InfoNode.
  //
  // Single-trial (defaultFlags() already is) on purpose. With --num-trials > 1 and a best trial that is not the
  // last, restoreBestResult re-materializes the winner through initTree and the flat
  // shortcut leaves the tree unscored again -- for the OBJECT-ORIENTED engine
  // (--two-level --num-trials 3 on ninetriangles gives 0.936 against a codelength of
  // 3.518). That is the shared half of #1002, fixed in initTree on master; the columnar
  // half is re-stamped on the restore path and has its own multi-trial case below. The
  // console table is unaffected either way because it is captured from the live tree of
  // the winning trial.
  for (const char* network : { "examples/networks/ninetriangles.net", "examples/networks/states.net" }) {
    for (const char* flags : { "",
                               "--two-level",
                               "--columnar",
                               "--columnar --two-level",
                               "--non-redundant",
                               "--non-redundant --two-level" }) {
      runAndCheckPerLevelTable(flags, network);
    }
  }
}

TEST_CASE("Reporting: the per-level table survives the multi-trial restore [fast][core][mapeq][columnar-contract]")
{
  // The restore path, which the single-trial case above cannot reach. When the best
  // trial is not the last executed, restoreBestResult re-materializes the winner
  // through initTree and rewrites the output file -- and initTree writes
  // InfoNode::codelength only through calcCodelengthOnTree (the base map equation),
  // or not at all when it takes its flat shortcut. Measured on these exact
  // configurations before the re-stamp: ninetriangles --non-redundant --num-trials 5
  // reported 3.078067323 with a table summing 3.385833 (the base L of the same tree),
  // and states.net --columnar --num-trials 5 reported 2.011405238 with a table summing
  // 0.066667 -- a leftover from the LAST trial's tree, which is worse than the 0.0 it
  // used to be: a plausible number instead of an obviously broken one.
  //
  // Columnar only: the object-oriented half of the same defect is a master-side
  // initTree fix, and asserting it here would fail for a reason this branch does not
  // own.
  for (const char* network : { "examples/networks/ninetriangles.net", "examples/networks/states.net" }) {
    for (const char* flags : { "--columnar --num-trials 5",
                               "--columnar --two-level --num-trials 5",
                               "--non-redundant --num-trials 5",
                               "--non-redundant --two-level --num-trials 5" }) {
      runAndCheckPerLevelTable(flags, network);
    }
  }
}

namespace {

  // An Erdos-Renyi graph with no module structure, from a hand-rolled LCG so the
  // instance is identical on every platform (the columnar one-level fallback has to
  // actually fire for the test below to mean anything, and that depends on the
  // instance). n=80, p=0.2 gives 610 links and a search result worse than the
  // all-in-one-module partition.
  void addErdosRenyiLinks(InfomapWrapper& im, unsigned int n, double p)
  {
    unsigned long long x = 12345;
    for (unsigned int i = 1; i <= n; ++i) {
      for (unsigned int j = i + 1; j <= n; ++j) {
        x = (1103515245ULL * x + 12345ULL) % 2147483648ULL;
        if (static_cast<double>(x) / 2147483648.0 < p)
          im.addLink(i, j, 1.0);
      }
    }
  }

} // namespace

TEST_CASE("Columnar: the one-level fallback prices the partition it installs [fast][core][mapeq]")
{
  // Collapsing to one module used to report getOneLevelCodelength(), which is
  // calcCodelength on a tree with ZERO modules -- while the collapse installs ONE.
  // The core's own one-module codelength is what it costs now.
  //
  // Under --entropy-corrected the two prices are equal, and that equality is the
  // point rather than an accident: both describe the walk with a single codebook
  // over the same N node codewords. The zero-module tree has the root codebook and
  // nothing else; the one-module tree adds an index codebook with a single codeword
  // (no free parameter) around a module that holds all the flow and so has no exit
  // codeword either. N - 1 free parameters both ways.
  InfomapWrapper im(defaultFlags("--columnar --entropy-corrected"));
  addErdosRenyiLinks(im, 80, 0.2);
  im.run();

  REQUIRE(im.numTopModules() == 1); // the fallback fired
  CHECK(im.codelength() == doctest::Approx(im.getOneLevelCodelength()).epsilon(1e-9));
  checkPerLevelTableSumsToCodelength(im, "columnar one-level fallback");

  // The base map equation charges nothing per node, so there the two coincide and
  // nothing about the fallback moves.
  InfomapWrapper plain(defaultFlags("--columnar"));
  addErdosRenyiLinks(plain, 80, 0.2);
  plain.run();
  REQUIRE(plain.numTopModules() == 1);
  CHECK(plain.codelength() == doctest::Approx(plain.getOneLevelCodelength()).epsilon(1e-12));
}

TEST_CASE("Reporting: a fixed flat partition is scored on the columnar path too [fast][core][mapeq]")
{
  // --no-infomap with a flat partition reaches initPartition directly, never
  // initTree, so nothing on the columnar path had ever written InfoNode::codelength:
  // the table was 0.000000 throughout next to a real codelength.
  InfomapWrapper im(defaultFlags("--columnar --no-infomap --two-level"));
  addEdgeFixtureLinks(im, "graphs/twotriangles_unweighted.edges");
  im.setInitialPartition({ { 1, 1 }, { 2, 1 }, { 3, 1 }, { 4, 2 }, { 5, 2 }, { 6, 2 } });
  im.run();

  checkPerLevelTableSumsToCodelength(im, "columnar --no-infomap flat partition");
  CHECK(im.root().codelength > 0.0);
  for (const auto& module : im.root())
    CHECK(module.codelength > 0.0);
}

// Follow-ups (not reproduced here): #831 (two-level initPartition/consolidate
// reentrancy corrupting the recomputed codelength after a prior multi-level
// trial) and #837 (hierarchical super-step degrading a two-level memory
// partition via a wrong super-index consolidation value) are the same
// tracked-vs-recompute drift class but need dedicated multi-trial / hierarchical
// setups to isolate. Add targeted should_fail cases when those are pinned down.

} // namespace test
} // namespace infomap

