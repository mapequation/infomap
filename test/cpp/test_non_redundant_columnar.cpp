#include "TestUtils.h"
#include "core/ColumnarObjective.h"
#include "io/Config.h"

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

} // namespace test
} // namespace infomap
