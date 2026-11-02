#include "TestUtils.h"
#include "core/ColumnarObjective.h"
#include "io/Config.h"

#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

// The generative map equation Lhat* on the columnar (non-recursive) engine, which is
// what --non-redundant computes: the ARITHMETIC and its goldens. The structural
// contracts of the flag -- parsing, composition, ragged and state trees -- are in
// test_non_redundant_columnar.cpp.
//
// Closed form, with Z_a = p_a + q_a^ex, E_v = sum_{c in ch(v)} q_c^en and
// D_v = q_v^ex + E_v (q_root = 0 for a closed network), H(p) over the IN-flow:
//
//   Lhat* = H(p)
//         + sum_{leaf a}     [ (p_a - q_a^en + q_a^ex) lg Z_a - q_a^ex lg q_a^ex
//                              + q_a^en lg p_a ]
//         + sum_{internal v} [ sum_{c in ch(v)} ( q_c^ex lg(D_v - q_c^en)
//                                                 - q_c^en lg q_c^en )
//                              - q_v^ex lg q_v^ex + q_v^en lg E_v ]
//
// The enter and exit rates agree exactly when the boundary rates account for every
// step the visit rate counts -- undirected flow, and directed flow with recorded
// teleportation -- and the leaf term then collapses to p_a lg Z_a + q_a lg(p_a/q_a).
// They come apart under unrecorded teleportation (-d, outdirdir, undirdir) and under
// rawdir, which is where writing p_a there cost up to 0.103 bits.
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

TEST_CASE("Lhat*/columnar: --non-redundant selects this objective and offers no other [fast][core][non-redundant]")
{
  const auto def = lhatConfig("--non-redundant");
  CHECK(def.nonRedundant);
  CHECK(def.columnarSearch);

  // There is no score selector: --non-redundant means this objective and nothing else.
  // Pinned so that reintroducing one is a deliberate act rather than a silent widening.
  CHECK_THROWS(lhatConfig("--non-redundant --non-redundant-score entropy"));
}

TEST_CASE("Lhat*/columnar: two-level Lhat* on a fixed partition, undirected [fast][core][non-redundant]")
{
  const double Lhat = scoreFixedPartition("twotriangles_flow.net", "twotriangles_two_modules.clu", "--non-redundant --two-level");
  CHECK(Lhat == doctest::Approx(1.999192930915).epsilon(1e-9));

  // The standard map equation on the same partition is a different number, so a golden
  // cannot pass by the dispatch falling through to the neighbouring objective.
  const double L = scoreFixedPartition("twotriangles_flow.net", "twotriangles_two_modules.clu", "--columnar --two-level");
  CHECK(L == doctest::Approx(2.114170945008).epsilon(1e-9));
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

TEST_CASE("Lhat*/columnar: a directed golden, from a flow this test knows by symmetry [fast][core][non-redundant]")
{
  // A directed 6-cycle split in half. Every node has one in- and one out-arc, so the
  // PageRank is uniform and every arc carries 1/6 of the flow whatever the teleportation
  // does -- which is what makes an INDEPENDENT directed golden possible here, where
  // reproducing Infomap's directed flow model in Python otherwise would be the test.
  //
  // p_i = 1/6, p_a = 1/2, q^en_a = q^ex_a = 1/6, Q = 1/3, so
  //   Lhat* = lg 6 + 2*[ (1/2) lg(1/2 + 1/6) - (1/6) lg(1/6) + (1/6) lg(1/2) ]
  //              + 2*[ (1/6) lg(1/3 - 1/6) - (1/6) lg(1/6) ]
  //         = 2.528320833574
  // This exercises the enter/exit-split terms on the directed path; the one-module case
  // below pins the degenerate end of the same path exactly.
  InfomapWrapper im(defaultFlags("--non-redundant --two-level --directed --no-infomap"));
  for (unsigned int i = 1; i <= 6; ++i)
    im.addLink(i, i % 6 + 1, 1.0);
  std::map<unsigned int, unsigned int> partition;
  for (unsigned int i = 1; i <= 6; ++i)
    partition[i] = i <= 3 ? 1 : 2;
  im.setInitialPartition(partition);
  im.run();
  CHECK(im.codelength() == doctest::Approx(2.528320833574).epsilon(1e-9));
}

TEST_CASE("Lhat*/columnar: rawdir, where qEnter and qExit genuinely differ [fast][core][non-redundant]")
{
  // The case every other golden here misses. Flux balance forces qEnter == qExit for
  // every module of a genuine stationary chain -- counting a module's internal steps
  // from the inside and from the outside gives moduleFlow - qExit and moduleFlow -
  // qEnter -- so undirected, PageRank-directed and recorded-teleportation runs cannot
  // tell the two rates apart. --flow-model rawdir can: the link weights ARE the flow and
  // the visit rate is the in-flow, with no stationarity assumed.
  //
  // That made the within codebook's weight invisible. It is how often the book is
  // OPENED, (moduleFlow - qEnter) + qExit, not moduleFlow; the two agree exactly when
  // qEnter == qExit. Writing moduleFlow cost up to 0.1 bits here and nothing anywhere
  // else, and every assertion in this file passed either way.
  //
  // The golden comes from the EXPLICIT kernel, not from the closed form the engine
  // implements: with pi_ij = w_ij / sum(w) and p = the in-flow, T is built entry by
  // entry from the model's definition (within p_j/Z_a; crossing
  // (qExit_a/Z_a)(qEnter_b/(Qen - qEnter_a))(p_j/p_b)), its rows checked to sum to 1,
  // and the value is -sum_ij pi_ij lg T_ij = 1.404184397997. So it would also catch a
  // closed form that failed to cancel a pairwise crossing rate, which no golden derived
  // from module-level statistics can.
  InfomapWrapper im(defaultFlags("--non-redundant --two-level --flow-model rawdir --no-infomap"));
  im.addLink(1, 2, 0.1);
  im.addLink(1, 3, 0.2);
  im.addLink(2, 3, 0.3);
  im.addLink(3, 4, 0.1);
  im.addLink(4, 5, 0.1);
  im.addLink(4, 6, 0.1);
  im.addLink(5, 6, 0.1);
  std::map<unsigned int, unsigned int> partition;
  for (unsigned int i = 1; i <= 6; ++i)
    partition[i] = i <= 3 ? 1 : 2;
  im.setInitialPartition(partition);
  im.run();
  CHECK(im.codelength() == doctest::Approx(1.404184397997).epsilon(1e-9));
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

  // Unit goldens with qEnter != qExit, which is the only regime where the within
  // book's weight is visible at all. The last two are one-sided (qExit == 0, then
  // qEnter == 0): there the gap against a moduleFlow-weighted term is largest, and
  // swapping the two arguments is unmistakable. Computed from the closed form in
  // output/primer/rungs_models.py (leaf_term), itself verified against that module's
  // explicit directed kernel.
  CHECK(lhatLeafModule(0.50, 0.10, 0.12, 0.90) == doctest::Approx(-0.991555894595).epsilon(1e-11));
  CHECK(lhatLeafModule(0.40, 0.17, 0.23, 0.75) == doctest::Approx(-0.793685184862).epsilon(1e-11));
  CHECK(lhatLeafModule(0.25, 0.05, 0.00, 0.40) == doctest::Approx(-0.900000000000).epsilon(1e-11));
  CHECK(lhatLeafModule(0.30, 0.00, 0.08, 0.55) == doctest::Approx(-0.788944401824).epsilon(1e-11));

  // Lhat* consumes F at the uniform rate 1 and nothing else reads it, so the leaf term
  // is affine in F with slope -1 exactly. That is what lets MemCorrection substitute
  // the physical-node F unweighted: MemCorrection needs no per-module rate at all.
  const double a = lhatLeafModule(0.4, 0.1, 0.12, -0.9);
  const double b = lhatLeafModule(0.4, 0.1, 0.12, -0.4);
  CHECK(a - b == doctest::Approx(0.5).epsilon(1e-15));
}

} // namespace test
} // namespace infomap
