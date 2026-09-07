/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef COLUMNAR_MAP_EQUATION_H_
#define COLUMNAR_MAP_EQUATION_H_

#include "ColumnarLevel.h"
#include "ColumnarTuning.h" // teleIndexRate, read by unitIndexRate below

#include <functional>
#include <string>
#include <vector>
#include <cstddef>
#include <cstdlib>
#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace infomap {

class InfoNode;
class ColumnarTwoLevel;

// Defined in ColumnarObjectiveScore.cpp, where both the stack scorers and
// ColumnarTwoLevel::buildStackTerms live. Only ever named as a return type here,
// so the incomplete declaration is enough and this header stays free of it.
namespace columnar {
  struct StackTerms;

  /**
   * Per-node decomposition of the stack codelength, in stack coordinates.
   *
   * moduleTerm[k][m] is what module m at stack level k is charged (level 1 =
   * the leaf modules .. topLevel = the top modules); rootTerm is the root's own
   * charge. Slot moduleTerm[0] is the leaf level and stays empty: a leaf owns no
   * codebook. The entries sum to hierarchicalCodelengthFromStack().
   *
   * This exists so the reported decomposition comes from the SAME objective as
   * the reported total. The console "Levels" table and -o json modules[].codelength
   * used to read InfoNode::codelength, which only calcCodelengthOnTree ever wrote —
   * i.e. the object-oriented base map equation — so under --non-redundant the table
   * summed L while the headline number was L* (netsci -C --non-redundant -N10:
   * table Total 4.103756 against Best codelength 3.892209764).
   */
  struct StackBreakdown {
    double rootTerm = 0.0;
    std::vector<std::vector<double>> moduleTerm;

    double total() const
    {
      double sum = rootTerm;
      for (const auto& level : moduleTerm)
        for (double term : level)
          sum += term;
      return sum;
    }
  };
} // namespace columnar

/**
 * A composable correction on top of the base columnar map equation.
 *
 * The new objective structure (replacing the OO inheritance hierarchy) is a
 * base map equation plus a set of independent corrections whose contributions
 * sum. This lets objectives combine freely (bias + metadata + lossy at once),
 * which the single-inheritance OO objectives cannot.
 *
 * For now a correction contributes an additive term to the hierarchical
 * codelength (used for reporting-consistency and to make the up/down sweep
 * objective-aware). Move-loop hooks (per-candidate delta + incremental update
 * for fully objective-aware search) will be added as a second step.
 */
class ColumnarCorrection {
public:
  virtual ~ColumnarCorrection() = default;
  // Additive contribution to the total hierarchical codelength of the core's
  // current stacked hierarchy. Reads the partition through the core's public
  // accessors. Base objective = no corrections = exactly zero.
  //
  // `breakdown`, when non-null, must also receive this contribution split over
  // the nodes it is charged to, so the reported per-module decomposition adds up
  // to the reported total (see columnar::StackBreakdown). Accumulate into the
  // SAME expressions the return value sums, so the split is exact rather than a
  // second, independently-rounded computation. A correction with no natural
  // per-node split leaves the default: the whole scalar on the root. The
  // parameter is null on every search-path call, so nothing is allocated there.
  virtual double hierarchicalCorrection(const ColumnarTwoLevel& core, columnar::StackBreakdown* breakdown = nullptr) const = 0;

  // --- Optional move-loop hooks (leaf-level two-level search) ---------------
  // Corrections that shape the leaf partition (Meta/Lossy/Mem) participate in
  // the leaf move loop so the search — not just the gating — is objective-aware.
  // Structural corrections (Bias) leave this false and stay gating-only.
  // The contract is O(1) per candidate: a move of leaf `unit` touches only the
  // two modules, so the delta and update read/write per-module state keyed by
  // the unit's own attribute (Mem: its physical node; Meta: its category).
  virtual bool participatesInMoveLoop() const { return false; }
  // (Re)build per-module correction state for the given leaf->module partition.
  // Returns this correction's contribution to the current codelength.
  virtual double initMoveLoop(const std::vector<int>& /*leafModule*/, int /*numModules*/) { return 0.0; }
  // Change in this correction's contribution from moving `leaf` oldMod -> newMod.
  virtual double moveDelta(int /*leaf*/, int /*oldMod*/, int /*newMod*/) const { return 0.0; }
  // Commit the move to the per-module state.
  virtual void applyMove(int /*leaf*/, int /*oldMod*/, int /*newMod*/) {}

  // Append move-target modules the search should also consider for `leaf`,
  // beyond its edge neighborhood. The base move loop only proposes modules of
  // edge-connected neighbors; Mem uses this to propose modules that already hold
  // a co-physical state node (same physical id), so the search can find the
  // flow-disconnected merges that collapse the physical codebook — the merges
  // the correction rewards but the edge-based loop would never generate. The
  // leaf's current module may be included; the caller skips it. Default none.
  virtual void proposeMoveTargets(int /*leaf*/, std::vector<int>& /*out*/) const {}

  // --- Module-level (aggregated-unit) participation --------------------------
  // The aggregation passes of the two-level search move aggregated UNITS (a
  // unit = the set of leaves consolidated into one previous-pass module). A
  // correction that maintains per-unit attribute aggregates participates in
  // those passes too, making the aggregation trajectory itself objective-aware.
  // Without this the module passes optimize the base objective only, so (e.g.)
  // the memory objective's aggregation stops far too fine and the whole
  // coarsening burden falls on the later gated merges (issue #834).
  // setUnits rebuilds the per-unit aggregates for the given leaf -> unit map;
  // afterwards initMoveLoop / moveDelta / applyMove / proposeMoveTargets are
  // indexed by UNIT id. resetUnitsToLeaves restores leaf indexing (the
  // construction state). A unit move delta costs O(distinct attributes in the
  // unit) rather than O(1).
  virtual bool participatesInModuleMoves() const { return false; }
  virtual void setUnits(const std::vector<int>& /*leafToUnit*/, int /*numUnits*/) {}
  virtual void resetUnitsToLeaves() {}

  // --- Leaf-module merge hooks (mem-aware coarsening) -----------------------
  // The leaf-module merge operator folds one leaf module into another to coarsen
  // the partition where the correction rewards it (Mem: combine two modules'
  // physical codebooks). It reuses the move-loop per-module state built by
  // initMoveLoop (so modules here are leaf-module ids), and needs the change in
  // this correction's contribution from merging module A into module B, plus a
  // commit. Base objective / structural corrections leave these zero/no-op.
  virtual double mergeDelta(int /*moduleA*/, int /*moduleB*/) const { return 0.0; }
  virtual void applyMerge(int /*moduleA*/, int /*moduleB*/) {}
  // Append leaf modules sharing an attribute with `module` (Mem: a physical
  // node) as extra merge candidates beyond the edge-connected set. Redundant on
  // undirected clustering (co-attribute modules are edge-adjacent there) but
  // relevant on directed; the merge operator consults it only when the
  // co-physical tuning mode is on (COL_COMERGE). Default none.
  virtual void proposeMergePartners(int /*module*/, std::vector<int>& /*out*/) const {}

  // Restrict this correction to a subset of leaves for an in-context leaf refine
  // (the bottom-level re-partition within a first-order parent). `globalLeafIds`
  // lists the sub-problem's leaves by global id in the sub-problem's local order,
  // so the returned correction is indexed by local leaf id. Leaf-shaping
  // corrections (Meta/Mem) return a sliced copy; structural ones return nullptr
  // and stay out of the refine, which then optimizes the base objective there.
  // Because a bottom module lives entirely within one parent, per-parent slicing
  // is exact — a physical node's / category's per-module term never couples
  // across parents.
  virtual std::unique_ptr<ColumnarCorrection> sliceForLeaves(const std::vector<int>& /*globalLeafIds*/) const { return nullptr; }
};

/**
 * Columnar (structure-of-arrays) representation of a hierarchical partition
 * under the *base* map equation.
 *
 * Phase-1a scaffolding for the hierarchical-search rethink: this is the
 * foundation of the leaner core we are validating before building optimization
 * on it. Its first job is a correctness gate — reproduce the codelength that
 * the object-oriented tree reports (InfomapBase::calcCodelengthOnTree) from a
 * columnar aggregation of leaf flow and leaf adjacency. Only once that parity
 * holds do we trust the structure enough to move the move-loop, super-search,
 * and interior tuning onto it.
 *
 * Scope: base objective only (no memory / metadata / biased / lossy /
 * regularized), and no recorded teleportation — the two real test networks
 * (netscicoauthor2010 undirected, science2001 directed without recorded
 * teleportation) both fall in this scope.
 */
class ColumnarMapEquation {
public:
  // Build a columnar mirror of the current InfoNode tree rooted at `root`,
  // ingesting leaf flow and the leaf out-adjacency. `undirected` selects the
  // enter/exit aggregation rule so it matches InfomapBase::isUndirectedClustering().
  void buildFromTree(const InfoNode& root, const std::vector<InfoNode*>& leafNodes, bool undirected);

  // Total hierarchical codelength: the sum of the per-module map-equation term
  // over every internal node including the root. Mirrors calcCodelengthOnTree.
  double hierarchicalCodelength() const;

  unsigned int numModules() const { return static_cast<unsigned int>(m_modFlow.size()); }
  unsigned int numLevels() const;

private:
  // Recompute module flow and enter/exit flow from leaf flow + adjacency.
  void aggregate(const std::vector<InfoNode*>& leafNodes);
  // Per-module map-equation contribution (leaf-module vs module-of-modules).
  double moduleTerm(int m) const;

  bool m_undirected = false;

  // Leaves: ids [0, nLeaves)
  std::vector<double> m_leafFlow;
  std::vector<int> m_leafParentMod; // module id of each leaf's parent

  // Modules (internal nodes incl. root): ids [0, nModules)
  std::vector<double> m_modFlow;
  std::vector<double> m_modEnter;
  std::vector<double> m_modExit;
  std::vector<int> m_modParent; // parent module id, -1 for root
  std::vector<int> m_modDepth; // root = 0
  std::vector<char> m_modLeafModule; // 1 if its children are leaves

  // Children as CSR. If m_modLeafModule[m], entries index m_leafFlow; else they
  // index the module arrays.
  std::vector<int> m_childStart; // size nModules + 1
  std::vector<int> m_childList;
};

/**
 * Columnar two-level optimizer for the base map equation (M1b).
 *
 * A faithful port of the object-oriented core loop (InfomapOptimizer::
 * optimizeActiveNetwork / tryMoveEachNodeIntoBestModule + consolidateModules +
 * findTopModulesRepeatedly) onto structure-of-arrays working state, so the
 * hierarchical search can later be moved off the OO tree. Base objective only,
 * no recorded teleportation. Not bit-identical to the OO path (independent RNG
 * stream); the goal is quality parity on the two-level codelength.
 */
class ColumnarTwoLevel {
public:
  // One aggregation level's working state (units = leaves, then modules, ...).
  // Defined at namespace scope in ColumnarLevel.h so a header that only holds a
  // leaf level — InfomapBase, which owns the native columnar leaf input — does
  // not have to include this one.
  using Level = ColumnarLevel;

  // Add a composable objective correction (ownership transferred). No
  // corrections = base map equation. Corrections sum into the codelength.
  void addCorrection(std::unique_ptr<ColumnarCorrection> correction)
  {
    m_corrections.push_back(std::move(correction));
  }

  void setInterruptCallback(std::function<void()> callback)
  {
    m_interruptCallback = std::move(callback);
  }

  // --- Public read accessors for corrections (partition shape + leaf data) ---
  // Number of tree levels currently in the stacked hierarchy (0 = leaves).
  unsigned int hierNumLevels() const { return static_cast<unsigned int>(m_hierLevels.size()); }
  // Unit count at a stacked level (level 0 = leaves, 1.. = module levels).
  int hierLevelSize(int level) const { return hierLevel(level).n; }
  int numLeaves() const { return m_nLeaves; }
  // The leaf's current bottom (level-1) module id.
  int hierLeafModule(int leaf) const { return m_hierAssign[0][leaf]; }
  // The level-(k+1) parent of unit `unit` at stacked level k (0 = leaves). Walking
  // hierLeafModule + hierUnitParent gives a leaf's whole ancestor chain, which is
  // how InfomapBase maps stack modules onto the materialized InfoNode tree.
  int hierUnitParent(int level, int unit) const { return m_hierAssign[level][unit]; }
  double leafFlow(int leaf) const { return m_leafFlow[leaf]; }

  // Per level-1 module: the rate at which the ACTIVE base objective's leaf-module
  // term consumes that module's F_m = sum_{leaf in m} plogp(leafFlow).
  //
  // Only corrections that SUBSTITUTE one F for another need this — today just
  // MemCorrection, which replaces the state-node F by the physical-node F. An
  // additive correction carrying its own rate (Meta, Bias, Preferred) composes
  // with either objective unweighted and must not call this.
  //
  // Returns EMPTY when the rate is uniformly 1, which is the base map equation's
  // whole leaf-module level — so the caller keeps its unweighted arithmetic
  // verbatim rather than multiplying by a vector of ones (the summation order is
  // part of the reported number). Non-empty only under L*, where the rate is
  // nrLeafCodebookRate(flow, enter, exit) >= 1 and differs per module.
  std::vector<double> leafCodebookRates() const;

  // Build the leaf level (flow, enter/exit, out+in CSR) from the leaf network.
  void buildFromLeaves(const std::vector<InfoNode*>& leafNodes, bool undirected, unsigned long seed);

  // Build the leaf level directly from columnar arrays (used to feed the
  // enter-flow super-network, or a sub-network within a module, into a nested
  // optimizer). exitNetworkFlow is the flow leaving the (sub-)network — 0 for
  // the whole closed network, the module's exit flow for an in-context refine.
  // recordedTeleport/globalTotalTeleFlow propagate the GLOBAL teleport context so
  // sub-optimizers fold in the same all-to-all teleport term (the level already
  // carries per-unit teleFlow/teleWeight); globalTotalTeleFlow is the whole
  // network's teleport flow, NOT the sub-network's local sum.
  // Takes the level by value so a caller with a dead local can hand over its
  // storage (std::move) instead of paying a copy of the whole CSR.
  void buildFromLevel(Level level, bool undirected, unsigned long seed, double exitNetworkFlow = 0.0, bool recordedTeleport = false, double globalTotalTeleFlow = 0.0);

  // Same, but the leaf level stays owned by the caller and is only read through
  // (no copy). Used for the native columnar leaf input, which InfomapBase builds
  // once from the network and reuses across trials: the leaf CSR is the single
  // largest allocation in a run, so each trial borrows it instead of duplicating
  // it. The caller MUST keep `level` alive, and unchanged, for this optimizer's
  // whole lifetime — the leaf level is immutable during a search.
  void buildFromBorrowedLevel(const Level& level, bool undirected, unsigned long seed, double exitNetworkFlow = 0.0, bool recordedTeleport = false, double globalTotalTeleFlow = 0.0);

  // Run repeated aggregation (Louvain-style) and return the two-level codelength
  // of the resulting leaves -> top-modules partition. maxAggPasses > 0 stops the
  // aggregation early (finer "building block" bottom); doFineTune toggles the
  // level-0 fine-tune-to-convergence.
  // pass1Seed (optional): start pass 1 from this unit->module assignment via
  // seedAssignment (singletons, then deterministic placement into the given
  // module) instead of from singletons, so the greedy move loop improves an
  // existing partition rather than re-deriving one. Used by the hierarchical
  // refinements' partial seeding (see buildPartialSeed).
  double optimizeTwoLevel(unsigned int maxAggPasses = 0, bool doFineTune = true, const std::vector<int>* pass1Seed = nullptr);

  // Two-level engine entry (the `--columnar --two-level` search): run the full
  // two-level optimize and materialize it as a two-level stack (leaves + one
  // module level) in m_hierLevels/m_hierAssign, so hierarchicalCodelengthFromStack,
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
  // the flat codelength; leaves the flat stack in m_hierLevels/m_hierAssign.
  double completeFlatFromAggregation(std::vector<int> aggTop, int aggK);

  // Deep repair of a two-level stack (#889): the split operator interleaved
  // with the seeded leaf fine-tune and the merge until the trio stops
  // improving. This is the expensive discovery phase — the engine wiring
  // runs it ONCE on the winning trial (seeded from its tree), not inside
  // every trial. Every step is gated on the true stack codelength, so the
  // result is never worse than the seed. Returns the repaired codelength.
  double deepRepairTwoLevelStack();

  // Once-per-run repair of a DEEP (multi-level) winner, the hierarchical
  // analog of deepRepairTwoLevelStack: the hierarchical split operator
  // (splitLevelModules) interleaved with the module coarsening, forced on
  // regardless of COL_HSPLIT and shaped by COL_HSPLIT_WINNER. Returns the
  // repaired hierarchical codelength (never worse than the seed).
  double deepRepairHierarchicalStack();

  // Whether the once-per-run hierarchical repair is enabled (COL_HSPLIT_WINNER).
  static bool hierarchicalWinnerRepairEnabled();

  // Whether this optimizer's regroup arm escalated to the full finest-granularity
  // ladder — the search's own pathology signal (a correction-driven group basin
  // the converged partition could not reach pairwise). The winner repair reads
  // it to decide whether a single-trial run pays for fresh split discovery.
  bool regroupEscalated() const { return m_regroupEscalated; }

  // Allow the split operators' expensive fresh from-singletons sub-clustering
  // (piece source 3). Default on; the winner repair turns it off for a
  // single-trial run whose trial never escalated — the cheap sources still run,
  // and on a healthy network they are what the repair would have used anyway.
  void setFreshDiscovery(bool on) { m_freshDiscovery = on; }

  // Whether any attached correction can participate in module-level moves
  // (Mem/Meta) — the gate for the aggregation trajectory repair and the
  // split operator; false on base networks.
  bool hasModuleMoveCorrections() const
  {
    for (const auto& c : m_corrections)
      if (c->participatesInMoveLoop() && c->participatesInModuleMoves())
        return true;
    return false;
  }

  // Run the two-level optimize, then grow the hierarchy upward with the
  // enter-flow super-search (M1c). bottomBlockLimit > 0 seeds a finer bottom
  // level. Returns the hierarchical codelength of the multi-level partition.
  double optimizeHierarchical(unsigned int bottomBlockLimit = 0);

  // Grow the hierarchy from an already-built bottom two-level (m_leaf0/m_leafTop
  // with bottomK modules) via the enter-flow super-search, using m_superAggLimit.
  // Does not touch the bottom, so optimizeColumnar builds the bottom once and
  // reuses it across up-merge strategies. Returns hierarchical codelength.
  double buildHierarchyFromBottom(int bottomK);

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

  // Refine the current built hierarchy (m_hier*) in place: interior-layer
  // refinement (re-partition each interior layer within its grandparent, up/down
  // to convergence) followed by module coarsening. `startL` is the hierarchical
  // codelength of the incoming stack (used as the accept/revert baseline);
  // sweepLimit caps the sweeps (0 = until convergence). Returns the refined
  // codelength. Split out of optimizeConverge so optimizeColumnar can screen
  // several up-build strategies cheaply and refine only the winner.
  double refineHierarchy(double startL, unsigned int sweepLimit = 0);

  // Top-level columnar engine entry (the `--columnar` search): build the
  // hierarchy at a small set of up-merge strategies, screen them by post-build
  // codelength, then run the interior-layer refinement to convergence on the
  // best one only, leaving its stacked hierarchy in the members ready to
  // materialize into an InfoNode tree. sweepLimit caps the refinement sweeps
  // (0 = until convergence; wired to --tune-iteration-limit / -T).
  double optimizeColumnar(unsigned int bottomBlockLimit = 1, unsigned int sweepLimit = 0);

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

  // Materialize the best hierarchy (m_hier*) as one module-path per leaf, in the
  // shape InfomapBase::initTree expects: coarsest-first (path[0] = top module),
  // 1-based module ids, plus a trailing leaf-rank slot (unused by initTree but
  // keeps path length = module levels + 1). `leafNodes` supplies leaf stateIds
  // and must be the same vector passed to buildFromLeaves.
  std::vector<std::pair<unsigned int, std::vector<unsigned int>>>
  toNodePaths(const std::vector<InfoNode*>& leafNodes) const;

  unsigned int numTopModules() const { return m_numTopModules; }
  // Number of tree levels (leaves + module levels) after optimizeHierarchical.
  unsigned int numHierLevels() const { return static_cast<unsigned int>(m_hierLevels.size()); }
  // Final leaf -> top-module assignment (compacted ids), one entry per leaf.
  const std::vector<int>& leafTopModule() const { return m_leafTop; }

  // Seed the stacked hierarchy (m_hierLevels / m_hierAssign) from a given
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

  // Enable recorded-teleportation codebook terms (regularized flow). Must be set
  // before hierarchicalCodelengthFromStack; per-leaf teleport flow/weight come
  // from buildFromLeaves. No effect on the base/mem/meta objectives.
  void setRecordedTeleportation(bool on) { m_recordedTeleport = on; }

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

  // Hierarchical codelength (base map equation + active corrections) from the
  // stacked levels/assignments in m_hier* — the seeded or optimized partition.
  double hierarchicalCodelengthFromStack() const;

  // The same codelength, plus its per-node decomposition under the SAME
  // objective. Reporting only (the search never asks for the breakdown, so it
  // pays no allocation): the console per-level table and -o json
  // modules[].codelength are stamped from this.
  double codelengthBreakdownFromStack(columnar::StackBreakdown& breakdown) const;

  // Codelength of the all-in-one-module partition under the ACTIVE objective,
  // corrections included. Not the same thing as InfomapBase::getOneLevelCodelength(),
  // which is calcCodelength on a tree with ZERO modules: under --entropy-corrected
  // the two differ by exactly multiplier/(2*totalDegree) (ninetriangles 4.918622452
  // vs 4.925032709), and the collapse the one-level fallback performs installs one
  // module, so this is the value that partition actually costs. Restores the
  // current stack before returning, so it can be asked at any point in the search.
  double oneLevelCodelength();

private:
  void pollInterrupt() const
  {
    if (m_interruptCallback)
      m_interruptCallback();
  }

  // Move-loop machinery operating on the current level `m_lvl`.
  void initPartition();
  // Seed the partition of `m_lvl` at a given unit->module assignment (the shared
  // primitive behind fine/coarse/interior tuning), maintaining exact aggregates.
  void seedAssignment(const std::vector<int>& assign);
  // Recompute the running plogp terms + codelength from the module aggregates
  // (one O(K) pass). Pairs with m_deferTerms for deterministic placements.
  void rebuildRunningTerms();
  // Forced move of one unit to a target module, updating aggregates + terms.
  void moveUnit(int u, int newMod);
  // sweepCap > 0 caps the loop at that many sweeps instead of kCoreLoopLimit;
  // used by the regroup ladder's leaf-granularity candidate test, where the
  // point is to establish that a candidate wins, not to converge it.
  unsigned int moveLoop(unsigned int sweepCap = 0);
  // Build the next level from the current module assignment; returns unit count.
  int consolidateToNextLevel();

  // Remove / add module m's contribution to the running plogp terms + m_enterFlow,
  // using its effective (teleport-inclusive) enter/exit. With recorded
  // teleportation off these reduce to the link enter/exit (base behaviour).
  void removeModuleTerms(int m);
  void addModuleTerms(int m);

  // Recorded-teleport-aware move delta: change in the augmented base codelength
  // from moving unit u (link enter/exit/flow cur*, teleport tfu/twu) out of module
  // A into module B, with deltaOld/deltaNew the link flow between u and A/B. This
  // is the unhoisted reference form (the readable counterpart of the free
  // hoistOldSideTele + deltaCodelengthMovingNodeTeleHoisted the move loop uses
  // when m_recordedTeleport is on); kept for clarity like deltaCodelengthMovingNode.
  double deltaCodelengthMovingNodeTele(double curEnter, double curExit, double curFlow, double tfu, double twu, int A, int B, double deltaOld, double deltaNew) const;

  // Aggregate a base level under a unit->group assignment (K groups): group
  // flow = sum of member flow, group enter/exit = crossing flow, plus the
  // group-group CSR. A pure function of (base, assign).
  static Level aggregateLevel(const Level& base, const std::vector<int>& assign, int K, bool undirected);

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
  int subClusterLeaves(const std::vector<int>& S, double parentExit, std::vector<int>& loc, std::vector<int>& localAssign, bool fineTune = true, const std::vector<int>* leafModule = nullptr);

  // Generalized in-context two-level of one parent's children S at an arbitrary
  // stack level (subClusterLeaves is the level-0 case). `base` is the level the
  // children live on; `interior` applies the enter-flow transform (an interior
  // unit's codeword usage is its index rate q, exactly as the up-build's
  // setIndexRateAsFlow and refineLayerWithinGrandparent's k > 0 branch);
  // `sliceCorrections` slices the leaf-shaping corrections onto S (only
  // meaningful when the children ARE leaves). `loc` is an all -1 scratch vector
  // over base units, restored before returning. `unitModule` is the partial-seed
  // source (see subClusterLeaves' leafModule); interior callers pass nullptr,
  // which is the from-singletons default.
  int subClusterUnits(const Level& base, bool interior, bool sliceCorrections, const std::vector<int>& S, double parentExit, std::vector<int>& loc, std::vector<int>& localAssign, bool fineTune, const std::vector<int>* unitModule);

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
  // control. Returns false when partial seeding does not apply.
  bool buildPartialSeed(const Level& sub, const std::vector<int>& S, const std::vector<int>& assign, int layer, std::vector<int>& seed) const;

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
  int splitTopModules(double& L, bool allowSingletons);

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
  // Rebuilds m_hierAssign[k], m_hierAssign[k+1], m_hierLevels[k+1]; layers k+2
  // and above are untouched. Requires k in [0, top-2]. refineBottomWithinParents
  // is the k == 0 special case. Returns false if no grandparent layer exists.
  bool refineLayerWithinGrandparent(int k);

  // Re-partition the top module grouping within the root (the whole network is
  // the grandparent, exit flow 0). The up/down sweep refines layer k only within
  // a layer-(k+2) grandparent, so the topmost grouping — which has no grandparent
  // above it — is otherwise locked from the initial build. This closes that hole
  // in the general tuning: after a leaf-module merge coarsens the bottom, it lets
  // the super-structure regroup, and iterating merge <-> top-refine relaxes the
  // same-parent merge restriction over sweeps. Enter-flow transform (grouping
  // modules). Rebuilds m_hierAssign[top-1] and m_hierLevels[top]. Returns false
  // if there is no sub-top layer to regroup.
  bool refineTopLayer();

  // Module-level coarsening to convergence: interleave the leaf-module merge
  // (mergeLeafModulesWithinParents) and the gated top regroup (refineTopLayer),
  // each accepted only if it lowers the true hierarchical codelength (else
  // reverted). `L` is the accept/revert baseline, updated in place; maxSweeps
  // caps the interleave. Shared by the converge refinement (refineHierarchy) and
  // the fast (-F) search so both coarsen the memory/metadata/lossy objectives the
  // same way (without it -F skips the coarsening those objectives require).
  void coarsenModules(double& L, int maxSweeps);

  // Mem-aware leaf-module coarsening: merge leaf modules (level-0 -> 1) within
  // their shared level-2 parent when it lowers the augmented objective. The base
  // map equation opposes merging well-separated modules, but a leaf-shaping
  // correction (Mem) rewards folding co-attribute flow together (a smaller
  // physical codebook), and the balanced mem optimum lives at coarser leaf
  // modules than the base optimum. Single-leaf moves can't cross that barrier
  // (uphill before the codebook payoff); an atomic module merge can. Super
  // levels stay first-order. No-op for the base objective (no correction => a
  // merge only raises base => never accepted). Returns true if anything merged.
  bool mergeLeafModulesWithinParents();

  // Add leaf-shaping corrections, sliced to the given subset of leaves, onto a
  // sub-optimizer so an in-context bottom refine optimizes the augmented
  // objective (base + Meta/Mem) rather than base alone. No-op for the base
  // objective and for structural corrections. Only valid where the refined
  // units are leaves (k == 0) — interior levels stay first-order (base).
  void addSlicedLeafCorrections(ColumnarTwoLevel& subOpt, const std::vector<int>& globalLeafIds) const;

  bool m_undirected = false;
  unsigned long m_seed = 123;
  double m_exitNetworkFlow = 0.0; // flow leaving this (sub-)network; 0 if closed
  unsigned int m_superAggLimit = 0; // >0: conservative up-build (passes/super-level)
  bool m_flatFirstBottom = false; // build the bottom with the full two-level pipeline (see setFlatFirstBottom)
  // True while m_hierLevels' bottom (leaf -> level-1) is the converged two-level
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
  // True inside optimizeFromSeed: the two operators the engine skips on the base
  // objective -- the from-singletons split (splitTopModules) and the module merge
  // (mergeLeafModulesWithinParents) -- run there for every objective.
  //
  // Both of their usual gates rest on the same premise: that the partition being
  // operated on came out of THIS engine's aggregation, which already settled every
  // module-level move, so the only overshoot left to repair is the kind a correction
  // introduces. An externally supplied partition has no such provenance. Its modules
  // can need subdividing (an over-coarse cluster file) or merging (an over-fine one)
  // under the plain map equation, and both operators compute a full base-objective
  // delta and are gated on the true stack codelength, so running them is sound
  // wherever it is useful -- it just never pays on an engine-built partition, which is
  // what the gates keep free.
  bool m_externalSeed = false;
  // True inside the once-per-run hierarchical repair: coarsenModules then reads
  // COL_HSPLIT_WINNER instead of COL_HSPLIT for the split interleave.
  bool m_forceHSplit = false;
  bool m_deferTerms = false; // deterministic placement: moveUnit skips running-term (plogp) maintenance; rebuildRunningTerms() restores them
  bool m_noEmptyTargets = false; // purify-only move loop: no empty-module candidates (regroup ladder polish)
  bool m_leafMoveLoop = false; // true while moveLoop units are leaves (corrections active)
  bool m_moduleCorrActive = false; // true while module-move-capable corrections participate in a module-level move loop
  bool m_seededPhase = false; // true when the move loop starts from an existing partition (fine-tune/refine)
  // Which interior-refine sweep is running (refineHierarchy). Sweep 0 re-derives
  // every layer from singletons; partial seeding can be restricted to the later
  // sweeps, where the layer already holds a re-derived partition worth locking.
  int m_refineSweep = 0;
  double m_lastCorrection = 0.0; // correction total of the last move loop's active corrections (0 if none)
  std::vector<std::unique_ptr<ColumnarCorrection>> m_corrections; // objective add-ons
  std::function<void()> m_interruptCallback;

  // Sum of the composable corrections' contributions to the hierarchical
  // codelength (0 for the base objective, i.e. no corrections). `breakdown`, when
  // non-null, also collects each correction's per-node split.
  double objectiveCorrection(columnar::StackBreakdown* breakdown = nullptr) const;

  // The levels, assignments and teleport-augmented boundary rates a stack scoring
  // reads, resolved once. The single source of truth for the recorded-teleportation
  // augmentation: hierarchicalCodelengthFromStack and leafCodebookRates both go
  // through it, so no consumer can re-derive a module's enter/exit differently.
  columnar::StackTerms buildStackTerms() const;

  // --- The leaf network and the active level ---
  // Both are read-only for the whole search: nothing writes into a level's
  // columns after it is built (aggregation produces a *new* level). The leaf
  // level costs 24 B per link, so it exists exactly once per run and everything
  // that needs it points at it rather than copying it:
  //
  //   leaf0()  the immutable leaf network — m_leaf0Owned when this optimizer
  //            built it (buildFromLeaves / buildFromLevel), or the caller's
  //            level when it was borrowed (buildFromBorrowedLevel).
  //   lvl()    the level the move loop is currently working on — aliases the
  //            leaf network while the units are leaves, and m_lvlOwned once
  //            aggregation has produced module levels.
  //
  // Before this indirection each of these was a full copy of the leaf CSR, and a
  // trial held up to four of them at once (the caller's input, m_leaf0, m_lvl and
  // m_hierLevels[0], plus the saved-stack copies).
  Level m_leaf0Owned;
  const Level* m_leaf0Ptr = nullptr;
  Level m_lvlOwned;
  const Level* m_lvlPtr = nullptr;

  // Shared tail of the three build entries: point the leaf network at `leaf`
  // (owned or borrowed) and derive the per-leaf context from it.
  void initLeafContext(const Level* leaf, bool undirected, unsigned long seed, double exitNetworkFlow, bool recordedTeleport, double globalTotalTeleFlow);

  const Level& leaf0() const { return *m_leaf0Ptr; }
  const Level& lvl() const { return *m_lvlPtr; }
  // Make the leaf network the active level (no copy).
  void activateLeafLevel() { m_lvlPtr = m_leaf0Ptr; }
  // Take ownership of a derived (aggregated / sub-) level as the active one.
  void activateOwnedLevel(Level&& level)
  {
    m_lvlOwned = std::move(level);
    m_lvlPtr = &m_lvlOwned;
  }
  // Copy a caller-owned level in as the active one; free when it is the leaf
  // network, which is aliased instead.
  void activateLevelCopy(const Level& level)
  {
    if (&level == m_leaf0Ptr) {
      activateLeafLevel();
      return;
    }
    m_lvlOwned = level;
    m_lvlPtr = &m_lvlOwned;
  }
  // Re-activate a level a previous activateLevelCopy already installed, without
  // copying it again. Valid only while m_lvlOwned still holds that level, i.e.
  // after an excursion that moved m_lvlPtr alone (activateLeafLevel).
  void reactivateLevelCopy(const Level& level)
  {
    if (&level == m_leaf0Ptr)
      activateLeafLevel();
    else
      m_lvlPtr = &m_lvlOwned;
  }

  std::vector<int> m_module; // unit -> module id
  std::vector<double> m_mFlow, m_mEnter, m_mExit;
  // Per-module recorded-teleport aggregates (link-independent), tracked only when
  // m_recordedTeleport. The module's effective enter/exit fold in a teleport term
  // (moduleTeleEnter/Exit) built from these; see effEnter/effExit in the .cpp.
  std::vector<double> m_mTeleFlow, m_mTeleWeight;
  std::vector<int> m_mMembers;
  std::vector<int> m_emptyModules;

  // Running base-map-equation terms for the current active level.
  double m_enterFlow = 0.0;
  double m_enter_log_enter = 0.0;
  double m_exit_log_exit = 0.0;
  double m_flow_log_flow = 0.0;
  double m_enterFlow_log_enterFlow = 0.0;
  double m_nodeFlow_log_nodeFlow = 0.0; // over the current level's units
  double m_codelength = 0.0;

  // Leaf-level bookkeeping across aggregation.
  int m_nLeaves = 0;
  std::vector<double> m_leafFlow;
  std::vector<int> m_leafTop; // leaf -> current top-unit id
  std::vector<int> m_leafBlocks; // leaf -> pass-1 building block (see splitTopModules)
  std::vector<int> m_lastSinglesPieces; // leaf -> piece of the last fresh from-singletons derivation
  bool m_freshSinglesProductive = true; // last fresh derivation's recombine improved (gates further fresh derives)
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
  double m_leafNodeFlow_log_nodeFlow = 0.0; // over leaves, constant
  unsigned int m_numTopModules = 0;

  // Recorded teleportation (regularized / --recorded-teleportation): the walker
  // records teleport steps, so a module's enter/exit gains a dense all-to-all
  // teleport term the link-crossing aggregation can't see. Per-unit teleport
  // flow/weight are threaded through every Level (they sum under aggregation) and
  // folded into the effective module enter/exit in initPartition, the move loop,
  // the merge operator, and hierarchicalCodelengthFromStack, exactly as
  // InfomapBase::aggregateFlowValuesFromLeafToRoot does. m_totalTeleFlow is the
  // GLOBAL total (constant across sub-networks), so sub-optimizers inherit it via
  // buildFromLevel rather than recomputing from their local units.
  bool m_recordedTeleport = false;
  // Non-redundant map equation L* (see setNonRedundant). m_nrExact selects the exact
  // O(m) exit sweep over the O(K) power-series delta in the leaf move loop.
  bool m_nonRedundant = false;
  bool m_nrExact = false;
  // Regroup arm state: whether this optimizer escalated to the full ladder
  // (see regroupEscalated), whether fresh split discovery is allowed (see
  // setFreshDiscovery), and the ROOT problem's leaf count — sub-optimizers
  // inherit it (subClusterUnits) so the arm's fraction-of-root gate is
  // scale-free rather than an absolute size cutoff.
  bool m_regroupEscalated = false;
  bool m_freshDiscovery = true;
  int m_rootLeaves = 0;
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
  double m_totalTeleFlow = 0.0; // GLOBAL sum of leaf teleport flow (root teleport flow)
  // A module's recorded-teleport enter/exit from its aggregated teleport flow tf
  // and weight tw (see InfomapBase::aggregateFlowValuesFromLeafToRoot): a walker
  // teleports out with the fraction not landing back (exit), and teleports in from
  // the rest of the network onto this module's teleport weight (enter).
  double moduleTeleEnter(double tf, double tw) const { return (m_totalTeleFlow - tf) * tw; }
  double moduleTeleExit(double tf, double tw) const { return tf * (1.0 - tw); }

  // A unit's index-codebook use rate q = e + (T - t) * w: the rate at which a
  // walker enters it, across a link or by teleporting in from outside. This is
  // what the objective charges the index codebook for the unit, and therefore
  // what the enter-flow transform must hand a sub-search as its node flow --
  // `enter` alone is the link part only (see teleIndexRate, #1038). Equal to
  // `enter` whenever recorded teleportation is off.
  double unitIndexRate(const Level& level, int i) const
  {
    if (!m_recordedTeleport || level.teleWeight.empty() || !columnar::teleIndexRate())
      return level.linkEnter[i];
    return level.linkEnter[i] + moduleTeleEnter(level.teleFlow[i], level.teleWeight[i]);
  }

  // In-place enter-flow transform: node flow := unitIndexRate for every unit.
  // enter/exit/teleport aggregates and edges are carried through untouched, so
  // the sub-search's own group-level enter stays exact.
  void setIndexRateAsFlow(Level& level) const
  {
    level.flow = level.linkEnter;
    if (!m_recordedTeleport || level.teleWeight.empty() || !columnar::teleIndexRate())
      return;
    for (int i = 0; i < level.n; ++i)
      level.flow[i] += moduleTeleEnter(level.teleFlow[i], level.teleWeight[i]);
  }
  std::vector<double> m_leafTeleFlow;
  std::vector<double> m_leafTeleWeight;

  // Stacked hierarchy after optimizeHierarchical: level 0 = leaves, [1] = first
  // module level, ... ; m_hierAssign[k] maps a level-k unit to its level-(k+1)
  // parent.
  //
  // Level 0 is always the leaf network, and nothing ever writes to it (every
  // rebuild targets index >= 1), so slot 0 stays an empty placeholder and reads
  // go through hierLevel(), which routes level 0 to leaf0(). That keeps indices
  // and size() unchanged while making the stack — and every save/restore copy of
  // it — free of the leaf CSR.
  std::vector<Level> m_hierLevels;
  const Level& hierLevel(int k) const { return k == 0 ? leaf0() : m_hierLevels[k]; }
  std::vector<std::vector<int>> m_hierAssign;
};

/**
 * Biased objective: the entropy bias correction. Miller-Madow charges one free
 * parameter per codebook codeword beyond the first, at mult/(2*D*ln2) bits each
 * -- (K-1)/(2n) is in nats, the map equation is in bits. calcCodelengthOnTree
 * adds childDegree parameters for every internal node incl. the root, which sums
 * over the tree to the non-root node count (= sum of level sizes incl leaves),
 * less one parameter for every codebook with no exit codeword: the root, and any
 * module that is an only child all the way up to it. A structural correction
 * (all levels), no per-node state.
 */
class BiasedEntropyCorrection final : public ColumnarCorrection {
public:
  BiasedEntropyCorrection(double multiplier, double totalDegree)
      : m_multiplier(multiplier), m_totalDegree(totalDegree > 0.0 ? totalDegree : 1.0) {}
  double hierarchicalCorrection(const ColumnarTwoLevel& core, columnar::StackBreakdown* breakdown = nullptr) const override;

private:
  double m_multiplier;
  double m_totalDegree;
};

/**
 * Preferred-number-of-modules bias (--preferred-number-of-modules): the same
 * |K - K_pref| penalty (1 bit per module away from the target) that
 * BiasedMapEquation applies on the OO path, wired into the columnar two-level
 * move loop and merge so the search — not only the reported codelength — is
 * steered toward K_pref modules. K is the current non-empty leaf-module count,
 * tracked from the partition the move loop maintains (initMoveLoop seeds it,
 * applyMove/applyMerge keep it exact). Objective-agnostic: an additive
 * structural term that composes with base/meta/mem/lossy, like the entropy
 * bias. Applies at the top-level partition ONLY — OO does not propagate
 * preferredNumberOfModules to sub-Infomaps (cloneAsNonMain omits it), so
 * sliceForLeaves returns nullptr and the bias never recurses into sub-networks.
 */
class PreferredModulesCorrection final : public ColumnarCorrection {
public:
  explicit PreferredModulesCorrection(unsigned int preferredNumModules)
      : m_preferredNumModules(static_cast<int>(preferredNumModules)) {}
  double hierarchicalCorrection(const ColumnarTwoLevel& core, columnar::StackBreakdown* breakdown = nullptr) const override;

  // The same penalty for a caller that has a leaf-module count but no stack to
  // attach a correction to: InfomapBase::evaluateColumnarPartition's fallback to
  // the object-oriented tree, where this is the one correction with no OO
  // counterpart to reproduce it.
  static double costOf(int numModules, unsigned int preferredNumModules)
  {
    return std::abs(numModules - static_cast<int>(preferredNumModules));
  }

  bool participatesInMoveLoop() const override { return true; }
  double initMoveLoop(const std::vector<int>& leafModule, int numModules) override;
  double moveDelta(int leaf, int oldMod, int newMod) const override;
  void applyMove(int leaf, int oldMod, int newMod) override;
  double mergeDelta(int moduleA, int moduleB) const override;
  void applyMerge(int moduleA, int moduleB) override;
  // Top-level only (see class note): never slice into sub-networks.
  std::unique_ptr<ColumnarCorrection> sliceForLeaves(const std::vector<int>&) const override { return nullptr; }

private:
  // |K - K_pref| with unit weight (1 bit per module), matching
  // BiasedMapEquation::calcNumModuleCost.
  double cost(int numModules) const { return costOf(numModules, static_cast<unsigned int>(m_preferredNumModules)); }

  int m_preferredNumModules;
  std::vector<int> m_moduleMembers; // per module: member count
  int m_numNonEmpty = 0; // K = number of non-empty modules
};

/**
 * Metadata objective: per leaf-module, metaDataRate * F_m * H_m, where H_m is
 * the flow-weighted entropy of the module's metadata categories (matching
 * MetaCollection::calculateEntropy). Applies only at the leaf-module level
 * (MetaMapEquation delegates module-of-modules to base). Owns per-leaf
 * category + flow-weight state (single metadata dimension for now).
 */
class MetaCorrection final : public ColumnarCorrection {
public:
  // leafWeight[i] is the flow weight of leaf i (leaf flow when weighting by
  // flow, else a uniform weight); leafCategory[i] its metadata category.
  MetaCorrection(std::vector<int> leafCategory, std::vector<double> leafWeight, double metaDataRate)
      : m_leafCategory(std::move(leafCategory)), m_leafWeight(std::move(leafWeight)), m_metaDataRate(metaDataRate) {}
  double hierarchicalCorrection(const ColumnarTwoLevel& core, columnar::StackBreakdown* breakdown = nullptr) const override;

  bool participatesInMoveLoop() const override { return true; }
  double initMoveLoop(const std::vector<int>& leafModule, int numModules) override;
  double moveDelta(int leaf, int oldMod, int newMod) const override;
  void applyMove(int leaf, int oldMod, int newMod) override;
  double mergeDelta(int moduleA, int moduleB) const override;
  void applyMerge(int moduleA, int moduleB) override;
  std::unique_ptr<ColumnarCorrection> sliceForLeaves(const std::vector<int>& globalLeafIds) const override;

  bool participatesInModuleMoves() const override { return true; }
  void setUnits(const std::vector<int>& leafToUnit, int numUnits) override;
  void resetUnitsToLeaves() override;

private:
  // Per leaf-module contribution: F_m * H_m == plogp(F_m) - sum_c plogp(f_{m,c}).
  // metaTerm() applies the metaDataRate scale and sums over modules.
  double moduleCategoryFlow(int module, int category) const;

  std::vector<int> m_leafCategory; // per leaf: metadata category id
  std::vector<double> m_leafWeight; // per leaf: meta flow weight

  double m_metaDataRate;

  // Move-loop state (per module): total weight F_m and category->weight map.
  std::vector<double> m_moduleFlow; // F_m
  std::vector<std::unordered_map<int, double>> m_moduleCatFlow; // f_{m,c}

  // Per-(leaf, current-module) delta cache: the old-module delta and plogp(w)
  // are identical for every candidate the move loop probes for the same leaf
  // (mirrors MemCorrection).
  mutable int m_cacheUnit = -1;
  mutable int m_cacheOldMod = -1;
  mutable double m_cacheDOld = 0.0;
  mutable double m_cachePlogpW = 0.0;

  // Aggregated-unit mode (module-level passes): per-unit category -> weight
  // aggregates + per-unit total weight. Empty = units are leaves (the leaf
  // arrays are used directly).
  std::vector<std::vector<std::pair<int, double>>> m_unitCats;
  std::vector<double> m_unitWeight;
};

/**
 * Memory objective (state / higher-order networks): the leaf-module codebook is
 * over PHYSICAL nodes, not state nodes — state nodes of the same physical node
 * in the same module share a codeword (their flows sum).
 *
 * This is a SUBSTITUTION, not an additive term, and that distinction is the whole
 * subtlety. Both base objectives read the leaf flows of a level-1 module only
 * through F_m = sum_{leaf in m} plogp(leafFlow), linearly; the physical codebook
 * is the same objective with F_m^state replaced by F_m^phys = sum_phys
 * plogp(physFlow). So the correction is  sum_m rate_m * (F_m^state - F_m^phys),
 * where rate_m is the rate at which the ACTIVE objective consumes F_m — and the
 * two objectives do not agree on it:
 *
 *  - Base map equation: the T-normalized module term collapses to
 *    plogp(T) - plogp(qExit) - F_m (the module-flow*log(T) parts cancel between
 *    the state and physical versions), so rate_m == 1 for every module and the
 *    whole correction telescopes into two global sums,
 *    C_state - sum_{module,phys} plogp(physFlow), with C_state = sum_leaf
 *    plogp(stateFlow) a constant.
 *  - L* (--non-redundant): the module codebook is SPLIT into an enter codebook
 *    normalized by moduleFlow and a within codebook normalized by
 *    T = moduleFlow + qExit, and F_m is charged against both. rate_m is then
 *    nrLeafCodebookRate() = 1 + qEnter*qExit/(flow*(flow+qExit)) >= 1, varying per
 *    module, and the flat coefficient-1 correction under-subtracts a non-positive
 *    quantity (plogp is superadditive under splitting, so F^state <= F^phys) —
 *    i.e. it reports L* too HIGH, which broke invariance under exactly lumpable
 *    state duplication (#1009).
 *
 * hierarchicalCorrection therefore asks the core for leafCodebookRates(): empty
 * under the base objective, where it keeps the two-global-sums form verbatim, and
 * per-module under L*. Leaf-module level only (module-of-modules delegates to
 * base; neither objective reads leaf flows above level 1). Same O(1) move-loop
 * hook as Meta with attribute = physical node id — the case the hook is designed
 * for; those deltas stay base-flavoured on purpose (see --non-redundant-exact).
 */
class MemCorrection final : public ColumnarCorrection {
public:
  MemCorrection(std::vector<int> leafPhysical, std::vector<double> leafFlow);
  double hierarchicalCorrection(const ColumnarTwoLevel& core, columnar::StackBreakdown* breakdown = nullptr) const override;

  bool participatesInMoveLoop() const override { return true; }
  double initMoveLoop(const std::vector<int>& leafModule, int numModules) override;
  double moveDelta(int leaf, int oldMod, int newMod) const override;
  void applyMove(int leaf, int oldMod, int newMod) override;
  void proposeMoveTargets(int leaf, std::vector<int>& out) const override;
  double mergeDelta(int moduleA, int moduleB) const override;
  void applyMerge(int moduleA, int moduleB) override;
  void proposeMergePartners(int module, std::vector<int>& out) const override;
  std::unique_ptr<ColumnarCorrection> sliceForLeaves(const std::vector<int>& globalLeafIds) const override;

  bool participatesInModuleMoves() const override { return true; }
  void setUnits(const std::vector<int>& leafToUnit, int numUnits) override;
  void resetUnitsToLeaves() override;

private:
  double physFlow(int module, int physical) const;

  std::vector<int> m_leafPhysical; // per leaf (state node): COMPACT physical id in [0, m_numPhys)
  std::vector<double> m_leafFlow; // per leaf: state flow
  double m_cState; // constant sum_leaf plogp(stateFlow)

  // Dense per-module physical-flow lookup (enabled in initMoveLoop when
  // numModules * numPhys is small): O(1) reads in the move loop's hot path.
  // The sparse maps stay authoritative for iteration (merge scan).
  bool m_dense = false;
  int m_numPhys = 0;
  std::vector<double> m_densePhysFlow;

  // Per-(unit, current-module) delta cache: the old-module term and plogp(f)
  // are identical for every candidate the move loop probes for the same unit.
  mutable int m_cacheUnit = -1;
  mutable int m_cacheOldMod = -1;
  mutable double m_cacheOldTerm = 0.0;
  mutable double m_cachePlogpF = 0.0;

  // Aggregated-unit mode (module-level passes): per-unit sparse
  // (physical -> flow) aggregates of COMPACT physical ids, ascending. Empty =
  // units are leaves (the leaf arrays are used directly).
  std::vector<std::vector<std::pair<int, double>>> m_unitPhys;

  std::vector<std::unordered_map<int, double>> m_modulePhysFlow; // physFlow_{m,p}
  // Reverse index physical id -> modules currently holding a state node of that
  // physical node, so proposeMoveTargets can offer co-physical merge targets in
  // ~O(modules-per-physical) without scanning. Maintained alongside
  // m_modulePhysFlow in initMoveLoop/applyMove.
  std::unordered_map<int, std::unordered_set<int>> m_physModules;
};

/**
 * Lossy objective (rate-distortion map equation): a leaf module whose naming
 * overhead (loss l_m = plogp(F_m) - sum_leaf plogp(f)) exceeds lambda times its
 * Markov entropy share H_m becomes a "noise" module coded by one shared visit
 * codeword. The objective is J = L_full - sum_m max(0, l_m - lambda*H_m), so the
 * additive correction to the base map equation is  -sum_m max(0, l_m - lambda*H_m),
 * a leaf-module-level term with the same per-module-aggregate move-loop/merge
 * pattern as Meta/Mem. Being additive, it composes with the other corrections
 * (e.g. bias + lossy). The class itself is objective-agnostic (plain flow/entropy
 * inputs); only the columnarPartition wiring is behind
 * INFOMAP_FEATURE_LOSSY_MAP_EQUATION.
 *
 * It does NOT compose with the non-redundant map equation L*, and --lossy
 * --non-redundant is rejected in Config (#1011). "l_m at coefficient 1" is the
 * base objective's rate for the naming cost sum_leaf plogp(f); L* charges it at
 * nrLeafCodebookRate >= 1, so the credit is short by (rate_m - 1) * l_m and the
 * gate compares the wrong quantity. Unlike MemCorrection (#1010), which only
 * needed the rate on the accounting, the multiplier here belongs inside the
 * max(0, .) — it moves the gate too. See F38 in
 * columnar_wip/columnar-rethink-notes.md for the derived form and for what is
 * still missing (the reporting layer, not the objective).
 */
class LossyCorrection final : public ColumnarCorrection {
public:
  // leafFlow[i] = leaf flow f_i; leafEntropy[i] = its Markov entropy share
  // (f_i * h_i, matching InfoNode::lossyEntropy). lambda = distortion price.
  LossyCorrection(std::vector<double> leafFlow, std::vector<double> leafEntropy, double lambda);
  double hierarchicalCorrection(const ColumnarTwoLevel& core, columnar::StackBreakdown* breakdown = nullptr) const override;

  bool participatesInMoveLoop() const override { return true; }
  double initMoveLoop(const std::vector<int>& leafModule, int numModules) override;
  double moveDelta(int leaf, int oldMod, int newMod) const override;
  void applyMove(int leaf, int oldMod, int newMod) override;
  double mergeDelta(int moduleA, int moduleB) const override;
  void applyMerge(int moduleA, int moduleB) override;
  std::unique_ptr<ColumnarCorrection> sliceForLeaves(const std::vector<int>& globalLeafIds) const override;

private:
  // Per-module noise cost c_m = max(0, plogp(F_m) - flf_m - lambda*H_m).
  double moduleCost(double flow, double flowLogFlow, double entropy) const;

  std::vector<double> m_leafFlow; // f_i
  std::vector<double> m_leafFlf; // plogp(f_i)
  std::vector<double> m_leafEntropy; // f_i * h_i
  double m_lambda;

  // Move-loop state (per module): F_m, flf_m = sum plogp(f), H_m.
  std::vector<double> m_moduleFlow;
  std::vector<double> m_moduleFlf;
  std::vector<double> m_moduleEntropy;
};

} // namespace infomap

#endif // COLUMNAR_MAP_EQUATION_H_
