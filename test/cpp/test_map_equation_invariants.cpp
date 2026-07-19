#include "TestUtils.h"

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
  InfomapWrapper im(defaultFlags("--two-level --meta-data " + repoPath("test/fixtures/meta/states.meta")));
  im.readInputData(networkFixturePath("states.net"));
  im.run();
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

// Regression for #830: under --entropy-corrected the tracked codelength drifts
// from a fresh recompute (observed tracked 2.9815 vs recompute 2.9911). Marked
// should_fail so it stays green while the bug is present and turns RED the
// moment #830 is fixed -- at which point drop the decorator so it becomes a
// normal passing invariant.
TEST_CASE("MapEquation invariant: entropy-corrected tracked == recompute (repro #830) [core][mapeq][entropy-corrected]"
          * doctest::should_fail())
{
  InfomapWrapper im(defaultFlags("--two-level --entropy-corrected"));
  im.readInputData(networkFixturePath("lossy_benchmark.net"));
  im.run();
  checkTrackedMatchesRecompute(im);
}

// Follow-ups (not reproduced here): #831 (two-level initPartition/consolidate
// reentrancy corrupting the recomputed codelength after a prior multi-level
// trial) and #837 (hierarchical super-step degrading a two-level memory
// partition via a wrong super-index consolidation value) are the same
// tracked-vs-recompute drift class but need dedicated multi-trial / hierarchical
// setups to isolate. Add targeted should_fail cases when those are pinned down.

} // namespace test
} // namespace infomap
