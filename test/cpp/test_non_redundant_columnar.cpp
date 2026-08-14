#include "TestUtils.h"
#include "io/Config.h"

#include <cstdio>

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

TEST_CASE("NonRedundant/columnar: no input or objective is excluded [fast][core][non-redundant]")
{
  // L* constrains which walk *steps* are possible — no immediate re-entry into the
  // module just left, no immediate exit from the one just entered — which says nothing
  // about which codebook a step is coded in. Higher-order dynamics and every
  // composable correction therefore stay available.
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
  accepts("--non-redundant --lossy");
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

TEST_CASE("NonRedundant/columnar: a correction adds the same term under L* as under L [fast][core][non-redundant]")
{
  // This is what "L* supports every correction" actually asserts, and it is a claim
  // about where corrections enter rather than about L*: they are additive terms that
  // ColumnarTwoLevel::objectiveCorrection() sums on top of whichever base objective is
  // selected. On a FIXED partition, therefore, the term a correction contributes must be
  // identical under both bases — nothing in it can depend on the codebook structure the
  // base objective chose. Both arms run on the columnar engine so the comparison isolates
  // the base objective and nothing else.
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

} // namespace test
} // namespace infomap
