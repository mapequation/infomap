/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef COLUMNARHIERARCHY_H_
#define COLUMNARHIERARCHY_H_

#include "ColumnarMapEquation.h"
#include "ColumnarObjective.h"
#include "ColumnarStack.h"

#include <limits>
#include <map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace infomap {

class InfoNode;

/**
 * The stacked-hierarchy search of the columnar engine.
 *
 * Two roles, two classes. ColumnarTwoLevel is the search: the leaf network, the
 * flow model, the objective, the move loop and the two-level search, and every
 * sub-search is one of its own over a slice. This class owns the stacked
 * hierarchy (ColumnarStack) and everything that changes it: the operators
 * (refines, regroup, merge, splits, retune, up-build) and the drivers that
 * sequence them (optimizeColumnar, optimizeFlexible, optimizeTwoLevelStack,
 * optimizeFromSeed, the deep repairs). It reads the problem through its root
 * search and runs the move loop through the search's public building block
 * (ColumnarTwoLevel::moveUnits / moveLeaves).
 *
 * The operators are public building blocks: a new search function combines
 * them with the stack edits and checkpoints of ColumnarStack and the gate below,
 * without becoming a member of either class.
 */
class ColumnarHierarchy {
public:
  using Level = ColumnarLevel;

  // The stack is bound to the root search's leaf network and flow model, which
  // are members of this object: a copy or move would leave it pointing at the
  // original's.
  ColumnarHierarchy() = default;
  ColumnarHierarchy(const ColumnarHierarchy&) = delete;
  ColumnarHierarchy& operator=(const ColumnarHierarchy&) = delete;

  // --- The problem ------------------------------------------------------------
  // The root search: configure it (corrections, interrupt callback, recorded
  // teleportation, pass-1 reuse) through here.
  ColumnarTwoLevel& search() { return m_search; }
  const ColumnarTwoLevel& search() const { return m_search; }
  int numLeaves() const { return m_search.numLeaves(); }

  // Build the leaf level through the root search (see ColumnarTwoLevel) and bind
  // the stack to it.
  void buildFromLeaves(const std::vector<InfoNode*>& leafNodes, bool undirected, unsigned long seed, bool recordedTeleport = false)
  {
    m_search.buildFromLeaves(leafNodes, undirected, seed, recordedTeleport);
    m_stack.bind(m_search.leafLevel(), m_search.flow());
  }
  void buildFromBorrowedLevel(const Level& level, const ColumnarBoundaryFlow& flow, unsigned long seed, double exitNetworkFlow = 0.0)
  {
    m_search.buildFromBorrowedLevel(level, flow, seed, exitNetworkFlow);
    m_stack.bind(m_search.leafLevel(), m_search.flow());
  }

  // The stacked hierarchy the search holds: the seeded or optimized partition.
  // Walking leafModule + parent gives a leaf's whole ancestor chain, which is how
  // InfomapBase maps stack modules onto the materialized InfoNode tree.
  const ColumnarStack& stack() const { return m_stack; }
  ColumnarStack& stack() { return m_stack; }

  // Two-level engine entry (the `--columnar --two-level` search): run the full
  // two-level optimize and materialize it as a two-level stack (leaves + one
  // module level) in m_stack, so hierarchicalCodelengthFromStack,
  // the module coarsening loop and toNodePaths all operate on it like any other
  // hierarchy. Returns the (correction-augmented) two-level codelength.
  double optimizeTwoLevelStack();

  // Seeded leaf fine-tune across the current module level of a two-level
  // stack, gated on the true stack codelength (revert if not improving).
  // Used by optimizeTwoLevelStack to interleave leaf tuning with the
  // correction-driven module merges. Returns whether it improved (updates L).
  bool retuneLeavesWithinModules(double& L);

  // Complete the flat two-level pipeline from a converged aggregation
  // partition (the flat-first probe's output, optimizeTwoLevel(0, false)):
  // materialize the stack, run the deferred leaf fine-tune to convergence,
  // then the merge <-> retune interleave of optimizeTwoLevelStack. Returns
  // the flat codelength; leaves the flat stack in m_stack.
  double completeFlatFromAggregation(std::vector<int> aggTop, int aggK);

  // Deep repair of a two-level stack (#889): the split operator interleaved
  // with the seeded leaf fine-tune and the merge until the trio stops
  // improving. This is the expensive discovery phase — the engine wiring
  // runs it ONCE on the winning trial (seeded from its tree), not inside
  // every trial. Every step is gated on the true stack codelength, so the
  // result is never worse than the seed. Returns the repaired codelength.
  // externalSeed: the stack is a partition this engine did not build (a
  // --cluster-data warm start), so the split and the merge run for every
  // objective; see splitTopModules.
  double deepRepairTwoLevelStack(bool externalSeed = false);

  // Once-per-run repair of a DEEP (multi-level) winner, the hierarchical
  // analog of deepRepairTwoLevelStack: the hierarchical split operator
  // (splitLevelModules) interleaved with the module coarsening, forced on
  // regardless of COL_HSPLIT and shaped by COL_HSPLIT_WINNER. Returns the
  // repaired hierarchical codelength (never worse than the seed).
  double deepRepairHierarchicalStack();

  // Whether the once-per-run hierarchical repair is enabled (COL_HSPLIT_WINNER).
  static bool hierarchicalWinnerRepairEnabled();

  // Allow the split operators' expensive fresh from-singletons sub-clustering
  // (piece source 3). Default on; the winner repair turns it off for a
  // single-trial run whose trial never escalated — the cheap sources still run,
  // and on a healthy network they are what the repair would have used anyway.
  void setFreshDiscovery(bool on) { m_freshDiscovery = on; }

  // Run the two-level optimize, then grow the hierarchy upward with the
  // enter-flow super-search (M1c). bottomBlockLimit > 0 seeds a finer bottom
  // level. Returns the hierarchical codelength of the multi-level partition.
  double optimizeHierarchical(unsigned int bottomBlockLimit = 0, unsigned int superAggLimit = 0);

  // Grow the hierarchy from a bottom leaf -> module assignment (bottomK modules)
  // via the enter-flow super-search; superAggLimit > 0 makes it conservative (see
  // optimizeConverge). Replaces the stack;
  // the bottom is an argument, so optimizeColumnar builds the bottom once and
  // reuses it across up-merge strategies. Returns hierarchical codelength.
  double buildHierarchyFromBottom(std::vector<int> bottom, int bottomK, unsigned int superAggLimit = 0);

  // M2: build the hierarchy, then refine to convergence. Returns hierarchical
  // codelength. Uses a fine bottom by default so the interior tune has room.
  // sweepLimit caps the module-coarsening sweeps (0 = until convergence),
  // mirroring the sweep cap of the converge search (--tune-iteration-limit).
  double optimizeFlexible(unsigned int bottomBlockLimit = 1, unsigned int sweepLimit = 0);

  // M3: build the hierarchy, then run the up/down convergence sweep — refine
  // *every* interior layer within its grandparent, iterating across layers
  // until the hierarchical codelength stops improving. Returns hierarchical
  // codelength. Generalizes optimizeFlexible (which tunes only the bottom).
  // superAggLimit > 0 makes the up-build conservative (that many aggregation
  // passes per super-level instead of a full two-level merge), yielding more,
  // finer levels for the sweep to tune — the "don't over-merge on the way up"
  // building-block idea. 0 = full super-merge (as optimizeFlexible).
  // sweepLimit caps the number of up/down tuning sweeps (0 = until convergence).
  double optimizeConverge(unsigned int bottomBlockLimit = 1, unsigned int superAggLimit = 0, unsigned int sweepLimit = 0);

  // Refine the current built hierarchy (m_stack) in place: interior-layer
  // refinement (re-partition each interior layer within its grandparent, up/down
  // to convergence) followed by module coarsening. `startL` is the hierarchical
  // codelength of the incoming stack (used as the accept/revert baseline);
  // sweepLimit caps the sweeps (0 = until convergence). Returns the refined
  // codelength. Split out of optimizeConverge so optimizeColumnar can screen
  // several up-build strategies cheaply and refine only the winner. A stack
  // still above `abandonAboveL` after the interior sweeps is returned without
  // the coarsening (see setAbandonUnrecoveredBuild).
  // externalSeed as in deepRepairTwoLevelStack, for the module coarsening.
  double refineHierarchy(double startL, unsigned int sweepLimit = 0, double abandonAboveL = std::numeric_limits<double>::infinity(), bool externalSeed = false);

  // Top-level columnar engine entry (the `--columnar` search): build the
  // hierarchy at a small set of up-merge strategies, screen them by post-build
  // codelength, then run the interior-layer refinement to convergence on the
  // best one only, leaving its stacked hierarchy in the members ready to
  // materialize into an InfoNode tree. sweepLimit caps the refinement sweeps
  // (0 = until convergence; wired to --tune-iteration-limit / -T).
  double optimizeColumnar(unsigned int bottomBlockLimit = 1, unsigned int sweepLimit = 0);

  // Terminal ragged pass: dissolve every intermediate module-of-modules whose
  // index codebook no longer pays for itself (its children move up into its
  // parent), best-first over the whole tree, exact per-node delta on the base
  // module-of-modules term plus the entropy-bias free-parameter term. The stack
  // cannot hold a ragged tree, so the surviving partition is emitted as ragged
  // per-leaf paths (toNodePaths returns them, and hasDissolvedResult() flags that
  // the caller must score/stamp the materialized tree rather than the stack). No
  // change to leaf modules, so mem/meta/lossy/regularized are untouched; disabled
  // under the |K - K_pref| bias (see ColumnarCorrection::blocksLevelDissolve).
  // Returns the (never-greater) codelength of the dissolved tree from the pass's own
  // accounting -- exact for every objective but L*, whose module-of-modules term this
  // pass does not price -- or the stack's own codelength when nothing dissolves.
  // With gainOnly it prices the pass and nothing else: no score of the stack, no
  // ragged result, and it returns the (non-positive) change in codelength instead.
  double dissolveUnprofitableLevels(bool gainOnly = false);

  // Whether the last search produced a ragged result via dissolveUnprofitableLevels.
  bool hasDissolvedResult() const { return !m_dissolvedPaths.empty(); }
  // Discard a dissolve proposal so toNodePaths emits the rectangular stack again --
  // the caller's never-worse revert when the ragged tree did not lower the objective.
  void clearDissolvedPaths()
  {
    m_dissolvedPaths.clear();
    m_dissolveOffset.clear();
    m_dissolveOrder.clear();
    m_dissolveTerm.clear();
  }
  // The dissolve's module-tree, for splicing the materialized rectangular tree in
  // place and stamping it (InfomapBase::spliceDissolvedModules): node id 0 is the
  // root and dissolveNodeId(k, m) is stack module m at level k. dissolvedOrder() lists
  // the dissolved ids in the order they were applied, each into its then-current
  // parent, so replaying it on any tree with the same shape gives the same result.
  // dissolvedNodeTerms()[id] is a kept node's codelength on the dissolved tree (NaN
  // for a dissolved node), including every correction's per-node share.
  int dissolveNodeId(int level, int module) const { return m_dissolveOffset[level] + module; }
  int dissolveNumNodes() const { return static_cast<int>(m_dissolveTerm.size()); }
  const std::vector<int>& dissolvedOrder() const { return m_dissolveOrder; }
  const std::vector<double>& dissolvedNodeTerms() const { return m_dissolveTerm; }

  // Flat-first trial (#889, hierarchical half): the hierarchical searches build
  // the bottom of the hierarchy with the full two-level pipeline
  // (optimizeTwoLevelStack) instead of the fine building blocks, and keep the
  // flat stack itself as a gated candidate against the built + refined
  // hierarchy. The enter-flow up-build cannot reach flat-optimum partitions on
  // networks whose optimum is (near-)flat with many modules; seeding the bottom
  // with the flat optimum imports the two-level wins. The engine alternates
  // this across trials (even-numbered trials flat-first, so -N1 is unchanged)
  // and best-of-N picks per network between the two search directions.
  void setFlatFirstBottom(bool on) { m_flatFirstBottom = on; }
  // Whether a hierarchical build whose unrefined codelength is already worse than
  // one module is abandoned instead of refined (#1041, F56). Refinement cannot
  // pull such a build into the right basin -- it only squeezes it under the
  // one-level bound (om2 `-d --regularized -N1`: build 9.85 -> refined 7.49 while
  // the two-level search reaches 6.95; om5 `-d -N1` 7.81 against 6.87) -- so the
  // trial is left above one-level for InfomapBase's fallback, and the run-level
  // rescue answers with the two-level search when every trial ended there. At
  // -N10 this is bit-identical on every benchmark row and removes the doomed
  // trials' refinement (malaria -16%, air30k -11%, om4 -24% in seconds). The
  // caller enables it exactly where its one-level fallback applies -- not under
  // the preferred-modules bias, whose unrefined builds all look far above
  // one-level and whose fallback is off -- and only in runs with more than one
  // trial, where the flat-first sibling supplies the flat answer for free; the
  // lone trial of a -N1 run is refined as before (F56 addendum).
  void setAbandonDoomedBuild(bool on) { m_abandonDoomedBuild = on; }
  // The lone-trial counterpart (F57): a doomed build is refined, but only its
  // leaf-near half -- the interior sweeps (-F: the bottom re-partition) -- before
  // the verdict. That half is what separates the two regimes: on malaria / air30k
  // / air30k reg `-N1` it takes the build well under one-level (malaria 8.49 ->
  // 7.52 against 8.22), on the overlapping family it barely moves it (om5
  // `-d --regularized` 9.24 -> 9.11 against 7.99). A build still above one-level
  // there is left for the fallback and the run-level rescue instead of paying the
  // module coarsening, which never brought one under (om5 8.80, om4 8.33).
  void setAbandonUnrecoveredBuild(bool on) { m_abandonUnrecoveredBuild = on; }

  // Materialize the best hierarchy (m_stack) as one module-path per leaf, in the
  // shape InfomapBase::initTree expects: coarsest-first (path[0] = top module),
  // 1-based module ids, plus a trailing leaf-rank slot (unused by initTree but
  // keeps path length = module levels + 1). `leafNodes` supplies leaf stateIds
  // and must be the same vector passed to buildFromLeaves.
  std::vector<std::pair<unsigned int, std::vector<unsigned int>>>
  toNodePaths(const std::vector<InfoNode*>& leafNodes) const;

  // Number of tree levels (leaves + module levels) after optimizeHierarchical.
  unsigned int numHierLevels() const { return static_cast<unsigned int>(m_stack.numLevels()); }

  // Seed the stacked hierarchy (m_stack) from a given
  // partition, so hierarchicalCodelengthFromStack() evaluates it on the columnar
  // structure and the optimizer can resume from it. Each leafPaths[i] is that
  // leaf's module ids coarsest-first (path[0] = top module .. path.back() =
  // finest/leaf module); ids need not be compacted (they are hashed per level).
  // buildFromLeaves must have run first. Returns false (leaving the stack
  // untouched) for a ragged tree (leaves at differing depths).
  //
  // The rectangular contract stays strict on purpose: it is what keeps the base
  // scorer honest, since the base map equation is NOT invariant under inserting a
  // pass-through level (ninetriangles rect 3.38583082 -> 3.97958082 with one such
  // level above every leaf module). L* IS invariant — a single-child parent's enter
  // codebook is e*(plogp(e)-plogp(e))/e == 0 and its child's exit term has numerator
  // plogp(x)-0-plogp(x) == 0 — so under --non-redundant the caller rectangularizes a
  // ragged tree by padding the short paths, and the empty path of a top-level leaf
  // (InfomapBase::padLeafPathsToUniformDepth), instead of relaxing this guard.
  // Everything else falls back to the object-oriented tree.
  bool seedHierarchyFromLeafPaths(const std::vector<std::vector<int>>& leafPaths);

  // Continue the search from the partition currently seeded into the stack
  // (seedHierarchyFromLeafPaths must have run): the columnar analogue of the
  // object-oriented warm start from a soft --cluster-data partition (#824).
  //
  // Two steps. (1) POLISH the seed at its own granularity -- a flat seed through the
  // two-level interleave (from-singletons split, seeded leaf re-tune, module merge), a
  // deep one through the interior-layer refinement. The split is the operator that
  // matters for an EXTERNAL seed: it subdivides a supplied module the engine would
  // never have built, the job the object-oriented coarseTune does with a sub-Infomap
  // per module, and it is a no-op on a partition the engine produced itself (which is
  // why the per-trial pipeline leaves it out). (2) unless `flat` (--two-level),
  // REBUILD the hierarchy above the polished bottom with the ordinary up-build and
  // refine it, keeping whichever of the two is better -- because neither operator in
  // (1) can add or remove a level, so a seed whose shape is wrong for the objective
  // otherwise traps the search at its own depth.
  //
  // sweepLimit caps the refinement sweeps as elsewhere (0 = until convergence).
  // Returns the codelength of the partition left in the stack, or +infinity when
  // nothing was seeded.
  double optimizeFromSeed(bool flat, unsigned int sweepLimit = 0);

  // Use the non-redundant map equation L* as the base objective: the exit codebook
  // of a module excludes the module just left (leave-one-out over siblings) and the
  // first visit after entering uses a separate enter codebook. Changes the two-level
  // move-loop objective (leaf level) and hierarchicalCodelengthFromStack (all levels).
  void setNonRedundant(bool on) { m_nonRedundant = on; }
  // With L*, drive the leaf move loop with the exact O(m) leave-one-out exit sweep
  // instead of the default O(1) adaptive power-series delta (validation).
  void setNonRedundantExact(bool on) { m_nrExact = on; }

  // Stop the interior-layer refinement early once a whole sweep improves the
  // hierarchical codelength by less than this fraction of the post-build
  // codelength (the diminishing-returns knee). 0 = off (grind to full
  // convergence). Wired from --tune-iteration-relative-threshold.
  void setMinRelativeTuneImprovement(double frac) { m_minRelTuneImprovement = frac; }

  // Hierarchical codelength (base map equation + active corrections) of the
  // stack the search holds — the seeded or optimized partition.
  double hierarchicalCodelengthFromStack() const { return m_scoreCache ? scoredStack().total : scoreStack(m_stack); }

  // The same codelength, plus its per-node decomposition under the SAME
  // objective. Reporting only (the search never asks for the breakdown, so it
  // pays no allocation): the console per-level table and -o json
  // modules[].codelength are stamped from this.
  double codelengthBreakdownFromStack(columnar::StackBreakdown& breakdown) const { return scoreStack(m_stack, &breakdown); }

  // Codelength of ANY stack bound to this optimizer's leaf network, under its
  // objective (base map equation + corrections), without installing it: a
  // candidate partition is priced before the search decides to keep it.
  // `breakdown`, when non-null, also receives the per-node decomposition.
  double scoreStack(const ColumnarStack& stack, columnar::StackBreakdown* breakdown = nullptr) const;

  // Codelength of the all-in-one-module partition under the ACTIVE objective,
  // corrections included. Not the same thing as InfomapBase::getOneLevelCodelength(),
  // which is calcCodelength on a tree with ZERO modules: under --entropy-corrected
  // the two differ by exactly multiplier/(2*totalDegree) (ninetriangles 4.918622452
  // vs 4.925032709), and the collapse the one-level fallback performs installs one
  // module, so this is the value that partition actually costs. It prices that
  // partition as a stack of its own and leaves the search's stack untouched, so
  // it can be asked at any point in the search.
  double oneLevelCodelength() const;

  // --- Building blocks ----------------------------------------------------------
  // Run `step` (which returns whether it changed the stack) under a checkpoint
  // and keep it only if it lowers the stack codelength below L - kMinImprovement,
  // updating L; otherwise undo it. The accept/revert policy every gated operator
  // shares.
  template <class Step>
  bool gatedStep(double& L, Step&& step)
  {
    const ColumnarStack::Checkpoint checkpoint = m_stack.checkpoint();
    if (!step()) {
      m_stack.rollback(checkpoint);
      return false;
    }
    const double after = hierarchicalCodelengthFromStack();
    if (after < L - columnar::kMinImprovement) {
      m_stack.commit(checkpoint);
      L = after;
      return true;
    }
    m_stack.rollback(checkpoint);
    return false;
  }

  // Refine each level-2 module's leaves as their own in-context two-level
  // problem, replacing the bottom level. Returns true if any module changed.
  bool refineBottomWithinParents();

  // In-context objective-aware two-level of one parent's leaves S (the
  // parent's exit is the sub-network exit; corrections sliced to S): returns
  // the number of sub-modules, per-leaf local assignment in localAssign.
  // `loc` is an all -1 global-leaf scratch vector, restored before returning.
  // Shared by refineBottomWithinParents (fineTune = true) and splitTopModules
  // (fineTune = false: piece proposals need community granularity, not final
  // polish — the gated recombination and interleaved leaf re-tune do that).
  // leafModule (optional): the leaf -> current module map. When given and
  // partial seeding is on, the sub-optimize's pass 1 starts from a partial seed
  // of that partition instead of from singletons (see buildPartialSeed).
  // maxAggPasses: the sub-optimize's aggregation depth (0 = to convergence; 1 =
  // its pass-1 building blocks, splitTopModules on a one-module seed).
  int subClusterLeaves(const std::vector<int>& S, double parentExit, std::vector<int>& loc, std::vector<int>& localAssign, bool fineTune = true, const std::vector<int>* leafModule = nullptr, unsigned int maxAggPasses = 0);

  // Generalized in-context two-level of one parent's children S at an arbitrary
  // stack level (subClusterLeaves is the level-0 case). `base` is the level the
  // children live on; `interior` applies the enter-flow transform (an interior
  // unit's codeword usage is its enter rate q, exactly as the up-build's
  // setIndexRateAsFlow and refineLayerWithinGrandparent's k > 0 branch);
  // `sliceCorrections` slices the leaf-shaping corrections onto S (only
  // meaningful when the children ARE leaves). `loc` is an all -1 scratch vector
  // over base units, restored before returning. `unitModule` is the partial-seed
  // source (see subClusterLeaves' leafModule); interior callers pass nullptr,
  // which is the from-singletons default.
  int subClusterUnits(const Level& base, bool interior, bool sliceCorrections, const std::vector<int>& S, double parentExit, std::vector<int>& loc, std::vector<int>& localAssign, bool fineTune, const std::vector<int>* unitModule, unsigned int maxAggPasses = 0);

  // Split operator (#889), the subdivision half of a coarse-tune: subdivide
  // the top modules of a two-level stack into pieces — the retained pass-1
  // building blocks, the last fresh from-singletons derivation projected onto
  // the current partition, or (when the cheap sources don't improve and
  // allowSingletons is set) a fresh from-singletons sub-clustering within
  // each module (community granularity, so extracting a whole community from
  // an over-merged module is a single gated move) — and re-sort the pieces
  // with a seeded module-level move loop (module corrections active), gated
  // on the true stack codelength. A no-op without module-move-capable
  // corrections. Fresh derivations are memoized by leaf set and stay enabled
  // only while they keep improving; callers pass allowSingletons = false
  // until another operator has changed the partition since the last
  // rejection. Returns 0 = no improvement, 1 = improved via block pieces,
  // 2 = improved via singles pieces (updates L on improvement).
  //
  // externalSeed runs it for every objective, not only where module-move-capable
  // corrections drive the aggregation, and mergeLeafModulesWithinParents takes the
  // same flag. Both operators' usual gates rest on one premise: that the partition
  // came out of THIS engine's aggregation, which already settled every
  // module-level move, so the only overshoot left to repair is the kind a
  // correction introduces. An externally supplied partition has no such
  // provenance. Its modules can need subdividing (an over-coarse cluster file) or
  // merging (an over-fine one) under the plain map equation, and both operators
  // compute a full base-objective delta and are gated on the true stack
  // codelength, so running them is sound wherever it is useful -- it just never
  // pays on an engine-built partition, which is what the gates keep free.
  int splitTopModules(double& L, bool allowSingletons, bool externalSeed = false);

  // Hierarchical split operator (experimental, env COL_HSPLIT): the analog of
  // splitTopModules for a stacked hierarchy. Splits the level-(k+1) modules by
  // partitioning their level-k children into pieces and re-sorting the pieces
  // with a seeded move loop over the piece-aggregated level-k network (the
  // enter-flow transform for interior levels), gated on the true stack
  // codelength. Because the move loop may place a piece in any module —
  // including an empty one — this is group-split AND cross-parent relocation;
  // when a grandparent layer exists the new modules inherit the grandparent of
  // the (flow-)dominant module their pieces came from, and every level above
  // k+1 is re-aggregated before the gate. Piece sources, cheapest first: the
  // pass-1 leaf blocks and the last derivation intersected with the current
  // modules, then a fresh from-singletons sub-clustering of each module's
  // children. Returns 0 = no improvement, else the source index (updates L).
  int splitLevelModules(int k, double& L, bool allowSingletons);

  // Generalized within-grandparent refine (M3): re-partition layer-k units into
  // new layer-(k+1) modules, each constrained to stay within its layer-(k+2)
  // grandparent (the grandparent's exit is the sub-network exitNetworkFlow).
  // Rebuilds assign(k), assign(k+1) and level k+1 of the stack; layers k+2
  // and above are untouched. Requires k in [0, top-2]. refineBottomWithinParents
  // is the k == 0 special case. Each grandparent's new partition is kept or
  // undone on its own (#1387): a grandparent's enter and exit do not change with
  // its insides, so the layer's codelength change is the sum of per-grandparent
  // changes, and only the grandparents whose change lowers the codelength take
  // their new modules. Returns false if no grandparent layer exists or none
  // improves.
  // sweep: which up/down sweep of refineHierarchy asks (0 = the first, which
  // re-derives every layer from singletons; partial seeding can be restricted to
  // the later sweeps, where the layer already holds a re-derived partition worth
  // locking).
  bool refineLayerWithinGrandparent(int k, int sweep = 0);

  // Re-partition the top module grouping within the root (the whole network is
  // the grandparent, exit flow 0). The up/down sweep refines layer k only within
  // a layer-(k+2) grandparent, so the topmost grouping — which has no grandparent
  // above it — is otherwise locked from the initial build. This closes that hole
  // in the general tuning: after a leaf-module merge coarsens the bottom, it lets
  // the super-structure regroup, and iterating merge <-> top-refine relaxes the
  // same-parent merge restriction over sweeps. Enter-flow transform (grouping
  // modules). Rebuilds the stack's assign(top-1) and top level. Returns false
  // if there is no sub-top layer to regroup.
  bool refineTopLayer();

  // Module-level coarsening to convergence: interleave the leaf-module merge
  // (mergeLeafModulesWithinParents) and the gated top regroup (refineTopLayer),
  // each accepted only if it lowers the true hierarchical codelength (else
  // reverted). `L` is the accept/revert baseline, updated in place; maxSweeps
  // caps the interleave. Shared by the converge refinement (refineHierarchy) and
  // the fast (-F) search so both coarsen the memory/metadata/lossy objectives the
  // same way (without it -F skips the coarsening those objectives require).
  // winnerSplit: the once-per-run hierarchical repair, which reads COL_HSPLIT_WINNER
  // instead of COL_HSPLIT for the split interleave. externalSeed as in
  // splitTopModules, for the merge.
  void coarsenModules(double& L, int maxSweeps, bool winnerSplit = false, bool externalSeed = false);

  // Mem-aware leaf-module coarsening: merge leaf modules (level-0 -> 1) within
  // their shared level-2 parent when it lowers the augmented objective. The base
  // map equation opposes merging well-separated modules, but a leaf-shaping
  // correction (Mem) rewards folding co-attribute flow together (a smaller
  // physical codebook), and the balanced mem optimum lives at coarser leaf
  // modules than the base optimum. Single-leaf moves can't cross that barrier
  // (uphill before the codebook payoff); an atomic module merge can. Super
  // levels stay first-order. No-op for the base objective (no correction => a
  // merge only raises base => never accepted). Returns true if anything merged.
  bool mergeLeafModulesWithinParents(bool externalSeed = false);

private:
  // The current stack scored with its breakdown, once per stack version (see
  // ColumnarStack::version), while m_scoreCache is on: refineHierarchy's sweeps,
  // where the per-grandparent refine reads the breakdown before and after its
  // layer and the gate reads the total after it. Two slots, so the stack before
  // a step survives scoring the stack after it.
  struct ScoredStack {
    std::uint64_t version = 0;
    double total = 0.0;
    columnar::StackBreakdown breakdown;
  };
  const ScoredStack& scoredStack() const;
  mutable ScoredStack m_scored[2];
  mutable int m_scoredLast = 0;
  bool m_scoreCache = false;
  // While m_scoreCache is on, each correction that reads only the leaf partition
  // (ColumnarCorrection::readsLeafPartitionOnly) is scored once per leaf partition
  // version: a refine above the leaf layer leaves it as it was.
  struct CorrectionScore {
    const ColumnarCorrection* correction = nullptr;
    std::uint64_t leafVersion = 0;
    double value = 0.0;
    bool charged = false; // level1/root hold its breakdown charges
    std::vector<double> level1;
    double root = 0.0;
  };
  mutable std::vector<CorrectionScore> m_correctionScores;
  // Per layer k: the content of every grandparent as the last recorded refine of
  // layer k left it (every refine of the leaf layer, re-sweeps of interior layers),
  // for the next re-sweep to skip the grandparents nothing has touched since (see
  // refineLayerWithinGrandparent). Cleared when refineHierarchy starts.
  std::vector<std::unordered_set<std::uint64_t>> m_resweptContent;
  // Per layer-k unit, a hash of its leaf set.
  std::vector<std::uint64_t> unitHashes(int k) const;

  const Level& leaf0() const { return m_search.leafLevel(); }
  int nLeaves() const { return m_search.numLeaves(); }
  std::vector<std::unique_ptr<ColumnarCorrection>>& corrections() { return m_search.corrections(); }
  const std::vector<std::unique_ptr<ColumnarCorrection>>& corrections() const { return m_search.corrections(); }
  void pollInterrupt() const { m_search.pollInterrupt(); }

  // Partial seed for one sub-optimize over the units S of a single parent /
  // grandparent (partial seeding). The sub-optimize's default is to
  // re-derive S from singletons: full discovery, but it also discards the parts
  // of the partition that were never in doubt. Seeding it fully is the opposite
  // failure — the greedy loop reproduces the input and the gate reverts. This
  // builds the middle: LOCK the units their current module (assign[S[j]]) holds
  // most tightly, RELEASE the loosest fraction as fresh singletons the move
  // loop must re-place. Locked modules are compacted to 0..K-1 and each
  // released unit gets a fresh id K, K+1, ... (compacting over the locked units
  // only is required — compacting all modules first overflows the sub-network's
  // module id space at high release fractions). `layer` only keys the random
  // control; `sweep` is the refine sweep asking (see refineLayerWithinGrandparent).
  // Returns false when partial seeding does not apply.
  bool buildPartialSeed(const Level& sub, const std::vector<int>& S, const std::vector<int>& assign, int layer, int sweep, std::vector<int>& seed) const;

  // Add leaf-shaping corrections, sliced to the given subset of leaves, onto a
  // sub-optimizer so an in-context bottom refine optimizes the augmented
  // objective (base + Meta/Mem) rather than base alone. No-op for the base
  // objective and for structural corrections. Only valid where the refined
  // units are leaves (k == 0) — interior levels stay first-order (base).
  void addSlicedLeafCorrections(ColumnarTwoLevel& subOpt, const std::vector<int>& globalLeafIds) const;

  // Corrections that charge only the top modules (#1068): the top-module count
  // they steer to (0: none), and their copies onto a search whose partition
  // becomes the top level (ColumnarCorrection::topLevelCopy).
  int targetTopModules() const;
  void addTopLevelCorrections(ColumnarTwoLevel& opt) const;
  // The hierarchical search's bottom (leaf -> module, returns its module count):
  // the two-level search without the top-level-only corrections, unless that bottom
  // is already coarser than their target, when the top has to be found at the
  // leaves -- the biased leaf search from the unbiased bottom and from singletons,
  // the better of the two (#1068).
  int buildBottom(unsigned int bottomBlockLimit, std::vector<int>& bottom);

  // Sum of the composable corrections' contributions to the codelength of
  // `stack` (0 for the base objective, i.e. no corrections). `breakdown`, when
  // non-null, also collects each correction's per-node split.
  double objectiveCorrection(const ColumnarStack& stack, columnar::StackBreakdown* breakdown = nullptr) const;

  // The view a correction prices `stack` through, under this optimizer's base
  // objective.
  ColumnarStackView stackView(const ColumnarStack& stack) const { return { stack, m_nonRedundant, m_search.exitNetworkFlow() }; }

  ColumnarTwoLevel m_search;
  bool m_flatFirstBottom = false; // build the bottom with the full two-level pipeline (see setFlatFirstBottom)
  bool m_abandonDoomedBuild = false; // skip refining a build already worse than one-level (see setAbandonDoomedBuild)
  bool m_abandonUnrecoveredBuild = false; // stop refining a doomed build its interior sweeps left above one-level (see setAbandonUnrecoveredBuild)
  // True while the stack's bottom (leaf -> level-1) is the converged two-level
  // optimum produced by completeFlatFromAggregation, rather than a fine-blocks
  // or up-built bottom. The leaf partition is then already at the two-level
  // fixpoint (fine-tune to convergence + the merge <-> retune interleave), so
  // the leaf-layer re-derivation the hierarchical refinements do
  // (refineLayerWithinGrandparent(0) / refineBottomWithinParents, both
  // from-singletons within a grandparent's leaf set) is re-solving a problem
  // whose answer it already holds. Measured: exactly zero gain in 9 of 10
  // flat-bottom trials across air30k / regularized / malaria / pref-25, while
  // being the single most expensive pass in the trial (0.5-0.7s of a 1.0s
  // air30k trial); the tenth gained 0.24% once, which does not survive a seed
  // change. Skipped while set — but an accepted refine of an interior layer
  // above marks layer 0 dirty again, so the leaf re-derivation stays reachable
  // once the structure it nests in actually moves.
  bool m_bottomConverged = false;

  std::vector<int> m_lastSinglesPieces; // leaf -> piece of the last fresh from-singletons derivation
  bool m_freshSinglesProductive = true; // last fresh derivation paid its cost (gates further fresh derives)
  bool m_seedHasWholeNetworkModule = false; // deep repair's seed: >1 module, one of kWholeNetworkModuleShare of the leaves
  bool m_seedIsOneModule = false; // deep repair's seed is a single module (the one-level fallback): extract at block granularity
  // Sub-cluster memo for splitTopModules' from-singletons pieces: sorted leaf
  // set -> (K, per-leaf local assignment). A module's sub-clustering depends
  // only on its own leaf set, so results survive across interleave rounds.
  std::map<std::vector<int>, std::pair<int, std::vector<int>>> m_subClusterCache;
  // splitLevelModules (COL_HSPLIT) per-stack-level state: the last piece
  // derivation at level k (unit -> piece, reusable as a cheap source while the
  // level's unit count is unchanged) and whether it improved (gates the fresh
  // derivation, as m_freshSinglesProductive does at leaf level).
  std::vector<std::vector<int>> m_lastLevelPieces;
  std::vector<char> m_levelFreshProductive;

  // Non-redundant map equation L* (see setNonRedundant). m_nrExact selects the exact
  // O(m) exit sweep over the O(K) power-series delta in the leaf move loop.
  bool m_nonRedundant = false;
  bool m_nrExact = false;
  // Interior-refine early-stop knee (0 = off, grind to convergence): stop once a
  // whole up/down sweep's gain drops below this fraction of the post-build
  // codelength.
  //
  // 5e-3 is the DEFAULT; users who want the deeper refinement can ask for it
  // with --tune-iteration-relative-threshold (1e-3 and 0 both work, and the
  // columnar path honors an explicit value even when it equals the OO default).
  //
  // F23 lowered this to 1e-3 by default and F26 restored it, on a corrected
  // measurement: the deeper knee buys web-NotreDame -0.097% and powergrid
  // -0.054% but costs +20% and +23% CPU respectively, not the +8.7% F23
  // reported (that A/B ran under load, which inflated the 5e-3 arm's baseline
  // and compressed the delta -- on an idle machine it is 20.53s -> 24.69s).
  // The deeper knee IS on the codelength/CPU Pareto frontier -- at a matched
  // ~24.3s budget the old knee reaches only 5.5727 against 5.5674, and -N14
  // does not move it at all -- but a fifth more CPU for a tenth of a percent is
  // not the right DEFAULT, so it is a dial instead. Only bites on stacks with
  // more than one interior layer (refineSweeps is 1 otherwise), so
  // science2001/air30k/malaria and all of -F are unaffected either way.
  double m_minRelTuneImprovement = 5e-3;

  // The stacked hierarchy (see ColumnarStack), bound to leaf0() and m_flow by
  // initLeafContext. Its level 0 is the leaf network itself, so the stack -- and
  // every copy of it -- is free of the leaf CSR.
  ColumnarStack m_stack;

  // Set by dissolveUnprofitableLevels when it removed at least one level for some
  // branch: one ragged module-path per leaf (coarsest-first, no trailing leaf
  // slot), the partition the rectangular stack cannot represent. toNodePaths emits
  // these when present; every stack rebuild clears them (clearDissolvedPaths()).
  std::vector<std::vector<int>> m_dissolvedPaths;
  // The dissolve's module-tree, alongside m_dissolvedPaths (see dissolveNodeId).
  std::vector<int> m_dissolveOffset;
  std::vector<int> m_dissolveOrder;
  std::vector<double> m_dissolveTerm;
  // Allow the split operators' expensive fresh from-singletons sub-clustering
  // (see setFreshDiscovery).
  bool m_freshDiscovery = true;
};

} // namespace infomap

#endif // COLUMNARHIERARCHY_H_
