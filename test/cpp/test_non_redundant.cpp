#include "TestUtils.h"
#include "io/Config.h"

// The non-redundant map equation L* is a runtime flag (--non-redundant), not a build
// feature, so these tests are always compiled. Golden code lengths were computed
// independently from the three-codebook definition of L* (see the project notebook
// "Non-redundant map equation - expanded form.ipynb") using the exact flow statistics
// Infomap reports, so a match here checks the C++ implementation against the maths,
// not against itself.

namespace infomap {
namespace test {

namespace {

Config nonRedundantConfig(const std::string& extraFlags)
{
  return Config("input.net --silent --no-file-output " + extraFlags, true);
}

// Score a fixed partition of a fixture network under a given objective (--no-infomap).
double scoreFixedPartition(const std::string& network, const std::string& cluster, const std::string& flags)
{
  InfomapWrapper im(defaultFlags(flags + " --no-infomap --cluster-data " + clusterFixturePath(cluster)));
  im.readInputData(networkFixturePath(network));
  im.run();
  return im.codelength();
}

} // namespace

TEST_CASE("NonRedundant: --non-redundant parses and, unlike --lossy, does not force two-level [fast][core][non-redundant]")
{
  const auto conf = nonRedundantConfig("--non-redundant");
  CHECK(conf.nonRedundant);
  CHECK_FALSE(conf.twoLevel); // hierarchical search is supported

  // Directed flow is supported (enter and exit rates enter the codebooks separately).
  const auto directedConf = nonRedundantConfig("--non-redundant --directed");
  CHECK(directedConf.nonRedundant);
}

TEST_CASE("NonRedundant: accepts directed, meta data, regularized and recorded teleportation [fast][core][non-redundant]")
{
  // These all compose with the non-redundant objective and must parse without error.
  // Regularization / recorded teleportation only change the flow estimates; intra-module
  // teleport is not a module-boundary crossing, so the leave-one-out exit codebook stays valid.
  for (const auto* flags : { "--non-redundant --directed",
                             "--non-redundant --meta-data foo.txt",
                             "--non-redundant --recorded-teleportation",
                             "--non-redundant --regularized",
                             "--non-redundant --non-redundant-exact" }) {
    CHECK_NOTHROW(nonRedundantConfig(flags));
  }
}

TEST_CASE("NonRedundant: hierarchical search defaults to the keep-super-structure mode [fast][core][non-redundant]")
{
  // --non-redundant defaults fastHierarchicalSolution to 1 (keep the bottom-up super-module
  // structure and refine downward) instead of the standard-L default 0 (remove sub modules and
  // rebuild each top module top-down in isolation). The rebuild suits standard L but lands in
  // worse L* minima, because L*'s leave-one-out exit codebook couples siblings. Explicit -F/-FF
  // and --two-level are respected; standard L keeps its own default 0.
  CHECK(nonRedundantConfig("--non-redundant").fastHierarchicalSolution == 1);
  CHECK(nonRedundantConfig("--non-redundant -F").fastHierarchicalSolution == 1);
  CHECK(nonRedundantConfig("--non-redundant -F -F").fastHierarchicalSolution == 2);
  CHECK(nonRedundantConfig("--non-redundant --non-redundant-exact").fastHierarchicalSolution == 1);
  CHECK(Config("input.net --silent --no-file-output", true).fastHierarchicalSolution == 0);
}

TEST_CASE("NonRedundant: rejects memory input detected only after parsing [fast][core][non-redundant]")
{
  // Config validation runs before the input file is parsed; state input flips
  // haveMemory() afterwards, so the optimizer dispatch must re-validate rather than
  // silently fall through to MemMapEquation (which would ignore --non-redundant).
  InfomapWrapper im(defaultFlags("--non-redundant"));
  im.readInputData(networkFixturePath("states.net"));
  CHECK_THROWS_AS(im.run(), std::runtime_error);
}

TEST_CASE("NonRedundant: two-level code length on a fixed partition, undirected, differs from standard L [fast][core][non-redundant]")
{
  const double Lstar = scoreFixedPartition("twotriangles_flow.net", "twotriangles_two_modules.clu", "--non-redundant --two-level");
  const double Lstd = scoreFixedPartition("twotriangles_flow.net", "twotriangles_two_modules.clu", "--two-level");

  CHECK(Lstar == doctest::Approx(1.983611049901).epsilon(1e-9));
  CHECK(Lstd == doctest::Approx(2.114170945008).epsilon(1e-9));
  // A genuinely different objective (here the non-redundant code is the shorter one).
  CHECK(Lstar < Lstd);
}

TEST_CASE("NonRedundant: two-level code length on a fixed partition, directed [fast][core][non-redundant]")
{
  // Directed: enter != exit rates per module; L* uses each separately.
  const double Lstar = scoreFixedPartition("twotriangles_flow.net", "twotriangles_two_modules.clu", "--non-redundant --two-level --directed");
  CHECK(Lstar == doctest::Approx(1.791821407552).epsilon(1e-9));
}

TEST_CASE("NonRedundant: composes additively with meta data [fast][core][non-redundant]")
{
  // Meta data is supported: MetaMapEquation runs the base non-redundant codebook and adds
  // an orthogonal meta-data term, so L = L*(non-redundant) + meta-data rate * meta term.
  const double base = scoreFixedPartition("twotriangles_flow.net", "twotriangles_two_modules.clu", "--non-redundant --two-level");

  InfomapWrapper meta(defaultFlags("--non-redundant --two-level --no-infomap"
                                   " --cluster-data " + clusterFixturePath("twotriangles_two_modules.clu")
                                   + " --meta-data " + fixturePath("meta/twotriangles.meta")));
  meta.readInputData(networkFixturePath("twotriangles_flow.net"));
  meta.run();

  CHECK(base == doctest::Approx(1.983611049901).epsilon(1e-9));
  CHECK(meta.codelength() == doctest::Approx(2.975663740930).epsilon(1e-6));
  // The flow-codebook part is unchanged; meta data only adds a positive term.
  CHECK(meta.codelength() > base);
}

TEST_CASE("NonRedundant: applies to regularized (Bayesian-prior) flow unchanged [fast][core][non-redundant]")
{
  // The Bayesian prior changes only the flow estimates; L* consumes them through the same
  // enter/exit/flow statistics, so --non-redundant --regularized just scores L* on the
  // regularized flow. Golden value computed independently from the regularized flow statistics.
  const double Lstar = scoreFixedPartition("twotriangles_flow.net", "twotriangles_two_modules.clu",
                                           "--non-redundant --regularized");
  CHECK(std::isfinite(Lstar));
  CHECK(Lstar == doctest::Approx(2.356088414).epsilon(1e-6));
}

TEST_CASE("NonRedundant: a single module reduces to the node entropy (equals standard L) [fast][core][non-redundant]")
{
  // With one module the enter and exit codebooks vanish and L* = H(p_alpha).
  const double Lstar = scoreFixedPartition("twotriangles_flow.net", "twotriangles_single_module.clu", "--non-redundant --two-level");
  const double Lstd = scoreFixedPartition("twotriangles_flow.net", "twotriangles_single_module.clu", "--two-level");
  CHECK(Lstar == doctest::Approx(2.470950594455).epsilon(1e-9));
  CHECK(Lstar == doctest::Approx(Lstd).epsilon(1e-9));
}

TEST_CASE("NonRedundant: hierarchical search finds a multilevel partition no worse than two-level [fast][core][non-redundant]")
{
  InfomapWrapper hier(defaultFlags("--non-redundant --num-trials 40 --seed 1"));
  hier.readInputData(networkFixturePath("unbalanced_hierarchy.net"));
  hier.run();

  CHECK(hier.numLevels() >= 3);
  CHECK(hier.codelength() == doctest::Approx(2.049467514).epsilon(1e-6));

  InfomapWrapper two(defaultFlags("--non-redundant --two-level --num-trials 40 --seed 1"));
  two.readInputData(networkFixturePath("unbalanced_hierarchy.net"));
  two.run();
  // The exactly re-assembled hierarchical L* must never exceed the two-level L*.
  CHECK(hier.codelength() <= two.codelength() + 1e-9);
}

TEST_CASE("NonRedundant: directed hierarchical search is sound [fast][core][non-redundant]")
{
  InfomapWrapper hier(defaultFlags("--non-redundant --directed --num-trials 40 --seed 1"));
  hier.readInputData(networkFixturePath("unbalanced_hierarchy.net"));
  hier.run();

  CHECK(hier.numLevels() >= 3);
  // Default keep-super-structure mode (fastHierarchicalSolution 1). The old top-down-rebuild
  // default reached 3.116556331 here; keeping the super structure lands a lower L*.
  CHECK(hier.codelength() == doctest::Approx(3.114446503).epsilon(1e-6));
}

TEST_CASE("NonRedundant: adaptive series exit delta matches the exact O(m) sweep [fast][core][non-redundant]")
{
  // By default the search is driven by the O(1) adaptive power-series approximation of the
  // leave-one-out exit term; --non-redundant-exact selects the exact O(m) sweep instead
  // (the only path that still exercises the exact delta after the approximation became the
  // default). On a network with a genuine module hierarchy the two drive the greedy search
  // to the same optimum: the series error ~ (max q_enter / D)^(K+1) is negligible while
  // modules are many, and the reported L* is recomputed exactly for both. (On tiny, few-
  // module networks the paths may settle in marginally different local optima -- expected,
  // and why the exact sweep stays cheap and available there.)
  InfomapWrapper approx(defaultFlags("--non-redundant --num-trials 40 --seed 1"));
  approx.readInputData(networkFixturePath("unbalanced_hierarchy.net"));
  approx.run();

  InfomapWrapper exact(defaultFlags("--non-redundant --non-redundant-exact --num-trials 40 --seed 1"));
  exact.readInputData(networkFixturePath("unbalanced_hierarchy.net"));
  exact.run();

  CHECK(exact.codelength() == doctest::Approx(2.049467514).epsilon(1e-6));
  CHECK(approx.codelength() == doctest::Approx(exact.codelength()).epsilon(1e-9));
  CHECK(approx.numLevels() == exact.numLevels());
}

TEST_CASE("NonRedundant: --no-infomap reproduces the code length of an optimized multilevel tree [fast][core][non-redundant]")
{
  // Scoring an externally supplied multilevel partition must reproduce the value the
  // optimizer computed for it (exercises calcCodelengthOnTree on a fixed hierarchy).
  InfomapWrapper hier(defaultFlags("--non-redundant --num-trials 40 --seed 1"));
  hier.readInputData(networkFixturePath("unbalanced_hierarchy.net"));
  hier.run();
  const double optimized = hier.codelength();
  const unsigned int levels = hier.numLevels();

  const std::string treePath = "/tmp/infomap_nr_roundtrip.tree";
  std::remove(treePath.c_str());
  hier.writeTree(treePath);

  InfomapWrapper rescored(defaultFlags("--non-redundant --no-infomap --cluster-data " + treePath));
  rescored.readInputData(networkFixturePath("unbalanced_hierarchy.net"));
  rescored.run();

  CHECK(rescored.numLevels() == levels);
  CHECK(rescored.codelength() == doctest::Approx(optimized).epsilon(1e-9));
  std::remove(treePath.c_str());
}

TEST_CASE("NonRedundant: isolated super-level step (fast-hierarchical) stays sound [fast][core][non-redundant]")
{
  // -F -F keeps the super levels found by upward coarse-graining and skips the
  // downward recursion (fastHierarchicalSolution >= 2), isolating the super step.
  // Its exactly re-assembled L* must not exceed the two-level L*.
  InfomapWrapper super(defaultFlags("--non-redundant -F -F --num-trials 40 --seed 1"));
  super.readInputData(networkFixturePath("unbalanced_hierarchy.net"));
  super.run();

  InfomapWrapper two(defaultFlags("--non-redundant --two-level --num-trials 40 --seed 1"));
  two.readInputData(networkFixturePath("unbalanced_hierarchy.net"));
  two.run();

  CHECK(super.numLevels() >= 2);
  CHECK(super.codelength() <= two.codelength() + 1e-9);
}

} // namespace test
} // namespace infomap
