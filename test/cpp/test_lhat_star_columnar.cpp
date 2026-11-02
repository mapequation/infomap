#include "TestUtils.h"
#include "core/ColumnarObjective.h"
#include "io/Config.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

// The generative map equation Lhat* on the columnar (non-recursive) engine: what the
// non-redundant code COSTS on the walk, where L* (test_non_redundant_columnar.cpp)
// sums the same codebooks' entropies. It is the default score of --non-redundant;
// --non-redundant-score entropy selects L*.
//
// Closed form, with Z_a = p_a + q_a and, for a tree node v, E_v = sum_{c in ch(v)} q_c
// and D_v = q_v + E_v (q_root = 0 for a closed network):
//
//   Lhat* = H(p)
//         + sum_{leaf modules a} [ p_a lg Z_a + q_a lg(p_a / q_a) ]
//         + sum_{internal v}     [ sum_{c in ch(v)} q_c lg((D_v - q_c)/q_c)
//                                  + q_v lg(E_v / q_v) ]
//
// Every golden below is computed independently in Python, from the closed form in
// output/primer/rungs_models.py (Lhat_hier_plogp), which is itself verified against
// the explicit hierarchical kernel T_hier to 9e-16. The engine agrees with it to
// 2.7e-15 over the fixtures here and over a 3-level 12-node network, a ragged tree
// and weighted karate.
//
// Two structural properties the tests pin rather than the arithmetic:
//  - one module gives exactly H(p), under every flow model, because every module
//    term vanishes there. That is flow-model-independent, so it checks the directed
//    and rawdir paths without needing a second reference implementation.
//  - a single-child (pass-through) level costs exactly 0, as it does under L*, so
//    padLeafPathsToUniformDepth's padding is free under this score too.

namespace infomap {
namespace test {

namespace {

Config lhatConfig(const std::string& extraFlags)
{
  return Config("input.net --silent --no-file-output " + extraFlags, true);
}

double scoreFixedPartition(const std::string& network, const std::string& cluster, const std::string& flags)
{
  InfomapWrapper im(defaultFlags(flags + " --no-infomap --cluster-data " + clusterFixturePath(cluster)));
  im.readInputData(networkFixturePath(network));
  im.run();
  return im.codelength();
}

} // namespace

TEST_CASE("Lhat*/columnar: --non-redundant scores the code by its codelength unless asked otherwise [fast][core][non-redundant]")
{
  const auto def = lhatConfig("--non-redundant");
  CHECK(def.nonRedundant);
  CHECK(def.columnarSearch);
  CHECK(def.nonRedundantScore == "codelength");
  CHECK_FALSE(def.nonRedundantEntropyScore);

  // The option is read only with --non-redundant, but a misspelling is still a message
  // rather than a silent fall-through to the default.
  CHECK_THROWS_AS(lhatConfig("--non-redundant-score lstar"), std::runtime_error);
}

TEST_CASE("Lhat*/columnar: two-level Lhat* on a fixed partition, undirected [fast][core][non-redundant]")
{
  const double Lhat = scoreFixedPartition("twotriangles_flow.net", "twotriangles_two_modules.clu", "--non-redundant --two-level");
  CHECK(Lhat == doctest::Approx(1.999192930915).epsilon(1e-9));

  // The same code priced the other way, and the standard map equation, on the same
  // partition: three different numbers, so a golden cannot pass by the dispatch
  // falling through to a neighbouring objective.
  const double Lstar = scoreFixedPartition("twotriangles_flow.net", "twotriangles_two_modules.clu", "--non-redundant --non-redundant-score entropy --two-level");
  const double L = scoreFixedPartition("twotriangles_flow.net", "twotriangles_two_modules.clu", "--columnar --two-level");
  CHECK(Lstar == doctest::Approx(1.983611049901).epsilon(1e-9));
  CHECK(L == doctest::Approx(2.114170945008).epsilon(1e-9));
  CHECK(Lstar < Lhat); // the entropy of an unmatched code is below what it costs
  CHECK(Lhat < L);
}

TEST_CASE("Lhat*/columnar: singletons, where the enter and index books carry everything [fast][core][non-redundant]")
{
  const double Lhat = scoreFixedPartition("twotriangles_flow.net", "twotriangles_singletons.clu", "--non-redundant --two-level");
  CHECK(Lhat == doctest::Approx(3.151452828848).epsilon(1e-9));
}

TEST_CASE("Lhat*/columnar: one module is exactly the node entropy, under every flow model [fast][core][non-redundant]")
{
  // With one module q = 0, so every module term vanishes and Lhat* collapses to H(p)
  // -- which is also L and L*. Flow-model-independent, so this is the directed and
  // rawdir check as well: it needs no second reference implementation to be exact.
  for (const std::string& flow : { std::string(""), std::string(" --directed"), std::string(" --flow-model rawdir") }) {
    CAPTURE(flow);
    const double Lhat = scoreFixedPartition("twotriangles_flow.net", "twotriangles_single_module.clu", "--non-redundant --two-level" + flow);
    const double L = scoreFixedPartition("twotriangles_flow.net", "twotriangles_single_module.clu", "--columnar --two-level" + flow);
    CHECK(Lhat == doctest::Approx(L).epsilon(1e-12));
  }
  CHECK(scoreFixedPartition("twotriangles_flow.net", "twotriangles_single_module.clu", "--non-redundant --two-level")
        == doctest::Approx(2.470950594455).epsilon(1e-9));
}

TEST_CASE("Lhat*/columnar: directed Lhat* is a real third number, not a fallback [fast][core][non-redundant]")
{
  // The enter and exit rates of a module differ under PageRank-directed flow, and the
  // closed form carries them separately (q^en in the node-naming terms, q^ex in the
  // exit terms). Nothing here pins the arithmetic -- the one-module case above does
  // that exactly -- but a dispatch that fell through to L* or to L would be caught.
  const double Lhat = scoreFixedPartition("twotriangles_flow.net", "twotriangles_two_modules.clu", "--non-redundant --two-level --directed");
  const double Lstar = scoreFixedPartition("twotriangles_flow.net", "twotriangles_two_modules.clu", "--non-redundant --non-redundant-score entropy --two-level --directed");
  const double L = scoreFixedPartition("twotriangles_flow.net", "twotriangles_two_modules.clu", "--columnar --two-level --directed");
  CHECK(std::isfinite(Lhat));
  CHECK(Lhat > 0.0);
  CHECK(Lhat != doctest::Approx(Lstar).epsilon(1e-9));
  CHECK(Lhat != doctest::Approx(L).epsilon(1e-9));
}

TEST_CASE("Lhat*/columnar: hierarchical Lhat* on a multilevel fixed partition [fast][core][non-redundant]")
{
  // twotriangles_three_level.tree is the two-module partition with a single-child level
  // inserted under each module, so Lhat* must report the two-module value unchanged:
  // a pass-through node has D_v - q_c == q_v and E_v == q_v, and both its terms cancel.
  const double Lhat3 = scoreFixedPartition("twotriangles_flow.net", "twotriangles_three_level.tree", "--non-redundant");
  CHECK(Lhat3 == doctest::Approx(1.999192930915).epsilon(1e-9));
}

TEST_CASE("Lhat*/columnar: a ragged tree is scored under Lhat*, and the padding is free [fast][core][non-redundant]")
{
  const double ragged = scoreFixedPartition("twotriangles_flow.net", "twotriangles_ragged_branches.tree", "--non-redundant");
  const double padded = scoreFixedPartition("twotriangles_flow.net", "twotriangles_ragged_branches_padded.tree", "--non-redundant");
  CHECK(ragged == doctest::Approx(2.085466428681).epsilon(1e-9));
  CHECK(padded == doctest::Approx(ragged).epsilon(1e-12));

  // Not the base map equation's value for the same tree: #1012's failure mode was a
  // silent fallback to it.
  const double raggedL = scoreFixedPartition("twotriangles_flow.net", "twotriangles_ragged_branches.tree", "--columnar");
  CHECK(ragged != doctest::Approx(raggedL).epsilon(1e-9));
}

TEST_CASE("Lhat*/columnar: hierarchical search produces a sound, self-consistent Lhat* [fast][core][non-redundant]")
{
  InfomapWrapper hier(defaultFlags("--non-redundant --num-trials 5"));
  hier.readInputData(repoPath("examples/networks/ninetriangles.net"));
  hier.run();
  const double searched = hier.codelength();

  CHECK(std::isfinite(searched));
  CHECK(searched > 0.0);
  CHECK(hier.numLevels() >= 2);

  // Round-trip: re-scoring the written tree reproduces the search codelength, so the
  // number in the file header is the objective the search optimized. Relative path:
  // Windows has no /tmp.
  const std::string treePath = "infomap_lhat_columnar_roundtrip.tree";
  std::remove(treePath.c_str());
  hier.writeTree(treePath);

  InfomapWrapper rescored(defaultFlags("--non-redundant --no-infomap --cluster-data " + treePath));
  rescored.readInputData(repoPath("examples/networks/ninetriangles.net"));
  rescored.run();
  CHECK(rescored.codelength() == doctest::Approx(searched).epsilon(1e-9));
  std::remove(treePath.c_str());
}

TEST_CASE("Lhat*/columnar: the per-module decomposition is the reported total [fast][core][non-redundant]")
{
  // PR #1015 made the per-module codelengths in .json/.ftree L*-aware, so they sum to
  // the L* headline instead of to the base map equation's. The same has to hold for
  // Lhat*: every `total +=` in scoreStackLhat is paired with a charge() on the same
  // expression, including the leaf-module share of H(p) and the root's own two terms.
  // Checked in memory, not from the file, because -o json rounds the per-module numbers.
  for (const std::string& flags : { std::string("--non-redundant --num-trials 5"),
                                    std::string("--non-redundant --two-level --num-trials 5"),
                                    std::string("--non-redundant -d --num-trials 5") }) {
    CAPTURE(flags);
    InfomapWrapper im(defaultFlags(flags));
    im.readInputData(repoPath("examples/networks/ninetriangles.net"));
    im.run();
    double stamped = im.root().codelength;
    for (auto it = im.root().begin_tree(); !it.isEnd(); ++it)
      if (!it->isLeaf() && !it->isRoot())
        stamped += it->codelength;
    CHECK(stamped == doctest::Approx(im.codelength()).epsilon(1e-9));
  }
}

TEST_CASE("Lhat*/columnar: the closed-form node terms are the arithmetic the scorer uses [fast][core][non-redundant]")
{
  using namespace columnar;

  // A pass-through node costs exactly 0: one child, so E == qEnter(self) and
  // D - qEnter(child) == qExit(self). Pinned directly on the kernels, because the
  // padding gate in padLeafPathsToUniformDepth depends on it.
  const double qEn = 0.17, qEx = 0.23;
  const double E = qEn, D = qEx + qEn;
  CHECK(lhatChildTerm(qEn, qEx, D) + lhatNodeTerm(qEn, qEx, E) == doctest::Approx(0.0).epsilon(1e-15));

  // A leaf module with no boundary is its own closed network: the within book names a
  // node out of p_a alone, so the term is -F + p lg p, i.e. H(p) rescaled.
  CHECK(lhatLeafModule(1.0, 0.0, 0.0, -0.5) == doctest::Approx(0.5).epsilon(1e-15));

  // Lhat* consumes F at the uniform rate 1 and nothing else reads it, so the leaf term
  // is affine in F with slope -1 exactly. That is what lets MemCorrection substitute
  // the physical-node F unweighted (ColumnarStackView::leafCodebookRates returns empty).
  const double a = lhatLeafModule(0.4, 0.1, 0.12, -0.9);
  const double b = lhatLeafModule(0.4, 0.1, 0.12, -0.4);
  CHECK(a - b == doctest::Approx(0.5).epsilon(1e-15));
}

} // namespace test
} // namespace infomap
