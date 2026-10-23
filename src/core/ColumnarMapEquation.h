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
#include "ColumnarBoundaryFlow.h"
#include "ColumnarStack.h"

#include <cmath>
#include <functional>
#include <limits>
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

namespace columnar {
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
 * What a correction reads when it prices a stack: the stack's shape, and the
 * rate at which the active base objective charges each leaf module's leaf
 * codewords. A correction prices whatever stack it is shown -- the search's own
 * or a candidate it never installs -- so it reads neither the optimizer nor its
 * live stack.
 */
class ColumnarStackView {
public:
  ColumnarStackView(const ColumnarStack& stack, bool nonRedundant, double exitNetworkFlow)
      : m_stack(stack), m_nonRedundant(nonRedundant), m_exitNetworkFlow(exitNetworkFlow) {}

  const ColumnarStack& stack() const { return m_stack; }
  // Number of tree levels in the stack (0 = leaves).
  unsigned int hierNumLevels() const { return static_cast<unsigned int>(m_stack.numLevels()); }
  // Unit count at a stack level (level 0 = leaves, 1.. = module levels).
  int hierLevelSize(int level) const { return m_stack.size(level); }
  int numLeaves() const { return m_stack.size(0); }
  // The leaf's bottom (level-1) module id.
  int hierLeafModule(int leaf) const { return m_stack.leafModule(leaf); }
  // The level-(k+1) parent of unit `unit` at stack level k (0 = leaves).
  int hierUnitParent(int level, int unit) const { return m_stack.parent(level, unit); }

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

private:
  const ColumnarStack& m_stack;
  bool m_nonRedundant;
  double m_exitNetworkFlow;
};

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
  // Additive contribution to the total hierarchical codelength of the stack the
  // view shows. Base objective = no corrections = exactly zero.
  //
  // `breakdown`, when non-null, must also receive this contribution split over
  // the nodes it is charged to, so the reported per-module decomposition adds up
  // to the reported total (see columnar::StackBreakdown). Accumulate into the
  // SAME expressions the return value sums, so the split is exact rather than a
  // second, independently-rounded computation. A correction with no natural
  // per-node split leaves the default: the whole scalar on the root. The
  // parameter is null on every search-path call, so nothing is allocated there.
  virtual double hierarchicalCorrection(const ColumnarStackView& core, columnar::StackBreakdown* breakdown = nullptr) const = 0;

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

  // --- Level-dissolve hooks (terminal ragged pass, dissolveUnprofitableLevels) ---
  // The dissolve pass removes an intermediate module-of-modules whose index
  // codebook no longer pays for itself, lifting its children into its parent.
  // It touches ONLY module-of-modules terms, which are the base map equation for
  // every objective (a correction's own cost lives on leaf modules, which the
  // pass never moves) -- so a correction contributes to a dissolve gain only if
  // its cost depends on the SET of internal codebooks. Two cases, both default
  // to the no-op:
  //  * blocksLevelDissolve(): the |K - K_pref| bias is a tree-level scalar a
  //    local per-node delta cannot see, so it disables the pass (as the OO pass
  //    guards on preferredNumberOfModules). The entropy bias does NOT block: its
  //    cost is a per-codebook free-parameter count, which is exactly local.
  //  * dissolveGainPerInternalNode(): bits saved by removing one internal
  //    codebook -- non-zero only for the entropy bias (one free parameter fewer).
  virtual bool blocksLevelDissolve() const { return false; }
  virtual double dissolveGainPerInternalNode() const { return 0.0; }
  //  * dissolveNodeCharge(): this correction's share on a kept module-of-modules or
  //    the root with `numChildren` children, as the breakdown charges it -- the stamp
  //    the pass writes to the materialized tree without a re-score. `hasExitCodeword`
  //    is false for the root and for a module that is its parent's only child all the
  //    way up. Non-zero only for the entropy bias: one parameter per child, one less
  //    without an exit codeword (BiasedEntropyCorrection::hierarchicalCorrection).
  virtual double dissolveNodeCharge(int /*numChildren*/, bool /*hasExitCodeword*/) const { return 0.0; }

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
 * The search of the columnar engine: the problem (leaf network, flow model,
 * objective corrections), the move loop and the two-level search built on it.
 *
 * The two-level search started as a port of the object-oriented core loop
 * (InfomapOptimizer::optimizeActiveNetwork / tryMoveEachNodeIntoBestModule +
 * consolidateModules + findTopModulesRepeatedly) onto structure-of-arrays working
 * state; it has since grown its own regroup ladder, trajectory repair and pass-1
 * reuse. Not bit-identical to the OO path (independent RNG stream).
 *
 * Every sub-search of the hierarchy is one of these over a slice. The stacked
 * hierarchy and its operators live in ColumnarHierarchy, which owns a root
 * ColumnarTwoLevel as its problem and runs the move loop on any level through
 * moveUnits / moveLeaves.
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

  int numLeaves() const { return m_nLeaves; }

  // Build the leaf level (flow, enter/exit, out+in CSR) from the leaf network.
  // recordedTeleport: the leaves' stored enter/exit are total rates, their own
  // teleport enter/exit included, and are kept as such (see ColumnarLevel).
  void buildFromLeaves(const std::vector<InfoNode*>& leafNodes, bool undirected, unsigned long seed, bool recordedTeleport = false);

  // Build the leaf level directly from columnar arrays (used to feed the
  // enter-flow super-network, or a sub-network within a module, into a nested
  // optimizer). exitNetworkFlow is the flow leaving the (sub-)network — 0 for
  // the whole closed network, the module's exit flow for an in-context refine.
  // `flow` is the parent's whole flow model (undirected, teleport kind and its
  // GLOBAL totals), so sub-optimizers fold in the same teleport terms as the
  // parent (the level already carries the per-unit teleport sums); the totals
  // are the whole network's, NOT the sub-network's local sums.
  // Takes the level by value so a caller with a dead local can hand over its
  // storage (std::move) instead of paying a copy of the whole CSR.
  void buildFromLevel(Level level, const ColumnarBoundaryFlow& flow, unsigned long seed, double exitNetworkFlow = 0.0);

  // Same, but the leaf level stays owned by the caller and is only read through
  // (no copy). Used for the native columnar leaf input, which InfomapBase builds
  // once from the network and reuses across trials: the leaf CSR is the single
  // largest allocation in a run, so each trial borrows it instead of duplicating
  // it. The caller MUST keep `level` alive, and unchanged, for this optimizer's
  // whole lifetime — the leaf level is immutable during a search.
  void buildFromBorrowedLevel(const Level& level, const ColumnarBoundaryFlow& flow, unsigned long seed, double exitNetworkFlow = 0.0);

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
  // optimizeTwoLevel's fine-tune: re-optimize the leaves of m_leafTop to
  // convergence, seeded at it; returns the tuned codelength (bestCodelength if
  // no sweep improved on it).
  double fineTuneLeafTop(double bestCodelength);
  // Share of the leaf flow in the largest module of a leaf -> module assignment.
  double largestModuleFlowShare(const std::vector<int>& leafModule, int numModules) const;

  // Pass-1 reuse (#1120). The lone trial of a -N1 hierarchical run keeps what its
  // leaf pass left (setKeepPass1State); when that trial collapses to one module,
  // InfomapBase's run-level rescue runs the two-level search on the SAME seed and
  // leaf network, whose pass 1 is that identical sweep, and adopts the kept state
  // instead of repeating it (adoptPass1State: only from a search on the same seed
  // and leaf count; returns whether it adopted). The next optimizeTwoLevel then
  // starts at the first aggregation pass, bit-identical to a fresh run.
  void setKeepPass1State(bool on) { m_keepPass1State = on; }

  bool adoptPass1State(ColumnarTwoLevel& from);

  // --- The move loop as a building block -------------------------------------
  // One run of this optimizer's move loop over an arbitrary level, seeded at a
  // unit -> module assignment: for search functions that re-sort units the
  // two-level search does not own, such as the pieces of a split or re-seeded
  // leaves. `level` becomes the active level (activeLevel()) until the next search
  // on this optimizer. `leafToUnit`, when given, is the leaf -> unit map of
  // `level`; the module-move-capable corrections then score the units through it
  // (setUnits) and are reset to leaves afterwards. Without it, the corrections stay
  // out of the loop. Returns the unit -> module assignment the loop settled on;
  // module ids are not compacted.
  const std::vector<int>& moveUnits(Level&& level, const std::vector<int>& seed, const std::vector<int>* leafToUnit);
  // The same on the leaf network itself, with every leaf-shaping correction in
  // the loop.
  const std::vector<int>& moveLeaves(const std::vector<int>& seed);
  // The level the last move loop ran on.
  const Level& activeLevel() const { return lvl(); }

  // --- The problem, for the search functions built on this optimizer ----------
  // ColumnarHierarchy reads its leaf network, flow model, objective and
  // configuration here: this optimizer is the problem's owner, and every
  // sub-search is a ColumnarTwoLevel of its own over a slice.
  const Level& leafLevel() const { return leaf0(); }
  const ColumnarBoundaryFlow& flow() const { return m_flow; }
  unsigned long seed() const { return m_seed; }
  double exitNetworkFlow() const { return m_exitNetworkFlow; }
  // The objective corrections. Mutable because the merge pass and the move loop
  // keep their incremental state in them.
  std::vector<std::unique_ptr<ColumnarCorrection>>& corrections() { return m_corrections; }
  const std::vector<std::unique_ptr<ColumnarCorrection>>& corrections() const { return m_corrections; }
  const std::function<void()>& interruptCallback() const { return m_interruptCallback; }
  void pollInterrupt() const
  {
    if (m_interruptCallback)
      m_interruptCallback();
  }
  // The ROOT problem's leaf count, inherited by sub-optimizers so the regroup
  // arm's fraction-of-root gate is scale-free rather than an absolute cutoff.
  int rootLeaves() const { return m_rootLeaves; }
  void setRootLeaves(int n) { m_rootLeaves = n; }
  // The pass-1 building blocks of the last optimizeTwoLevel (leaf -> block).
  const std::vector<int>& leafBlocks() const { return m_leafBlocks; }

  // Whether this optimizer's regroup arm escalated to the full finest-granularity
  // ladder — the search's own pathology signal (a correction-driven group basin
  // the converged partition could not reach pairwise). The winner repair reads
  // it to decide whether a single-trial run pays for fresh split discovery.
  bool regroupEscalated() const { return m_regroupEscalated; }

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

  unsigned int numTopModules() const { return m_numTopModules; }
  // Final leaf -> top-module assignment (compacted ids), one entry per leaf.
  const std::vector<int>& leafTopModule() const { return m_leafTop; }

  // Enable recorded-teleportation codebook terms (regularized flow). Must be set
  // before hierarchicalCodelengthFromStack; per-leaf teleport flow/weight come
  // from buildFromLeaves. No effect on the base/mem/meta objectives.
  void setRecordedTeleportation(bool on) { m_flow.setRecordedTeleport(on); }

private:
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
  // The move loop's body, compiled once per teleport kind: without recorded
  // teleportation the teleport flow between a unit and a module is identically
  // zero, so that instantiation carries no teleport code in the per-candidate
  // path, and the global and per-layer kinds each inline their own sums.
  template <ColumnarBoundaryFlow::TeleportKind Tele>
  unsigned int moveLoopWith(unsigned int sweepCap);
  // Build the next level from the current module assignment; returns unit count.
  int consolidateToNextLevel();

  // Remove / add module m's contribution to the running plogp terms + m_enterFlow,
  // from its total (teleport-inclusive) enter/exit.
  void removeModuleTerms(int m);
  void addModuleTerms(int m);

  // Flow model (undirected, recorded teleportation) and the aggregation /
  // slicing / teleport-term helpers built on it.
  ColumnarBoundaryFlow m_flow;
  unsigned long m_seed = 123;
  double m_exitNetworkFlow = 0.0; // flow leaving this (sub-)network; 0 if closed
  bool m_deferTerms = false; // deterministic placement: moveUnit skips running-term (plogp) maintenance; rebuildRunningTerms() restores them
  bool m_noEmptyTargets = false; // purify-only move loop: no empty-module candidates (regroup ladder polish)
  bool m_leafMoveLoop = false; // true while moveLoop units are leaves (corrections active)
  bool m_moduleCorrActive = false; // true while module-move-capable corrections participate in a module-level move loop
  bool m_seededPhase = false; // true when the move loop starts from an existing partition (fine-tune/refine)
  double m_lastCorrection = 0.0; // correction total of the last move loop's active corrections (0 if none)
  std::vector<std::unique_ptr<ColumnarCorrection>> m_corrections; // objective add-ons
  std::function<void()> m_interruptCallback;

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
  // the stack's level 0, plus the saved-stack copies).
  Level m_leaf0Owned;
  const Level* m_leaf0Ptr = nullptr;
  Level m_lvlOwned;
  const Level* m_lvlPtr = nullptr;

  // Shared tail of the three build entries: point the leaf network at `leaf`
  // (owned or borrowed) and derive the per-leaf context from it.
  void initLeafContext(const Level* leaf, const ColumnarBoundaryFlow& flow, unsigned long seed, double exitNetworkFlow);

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
  // m_mEnter/m_mExit are TOTAL rates; m_tele supplies the recorded-teleport flow
  // between a unit and a module that a move subtracts alongside the link flow.
  ColumnarBoundaryFlow::TeleportSums m_tele;
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
  // What the leaf pass of optimizeTwoLevel left, before any aggregation: enough to
  // consolidate the next level exactly as the pass itself would (see adoptPass1State).
  struct Pass1State {
    std::vector<int> module;
    std::vector<double> mFlow, mEnter, mExit;
    ColumnarBoundaryFlow::TeleportSums tele;
    std::vector<int> mMembers;
    double codelength = 0.0;
  };
  bool m_keepPass1State = false;
  std::unique_ptr<Pass1State> m_pass1State; // kept by this search, for a later one to adopt
  std::unique_ptr<Pass1State> m_adoptedPass1; // adopted from another search; consumed by optimizeTwoLevel
  double m_leafNodeFlow_log_nodeFlow = 0.0; // over leaves, constant
  unsigned int m_numTopModules = 0;

  // Regroup arm state: whether this optimizer escalated to the full ladder
  // (see regroupEscalated), and the ROOT problem's leaf count — sub-optimizers
  // inherit it (see rootLeaves) so the arm's fraction-of-root gate is
  // scale-free rather than an absolute size cutoff.
  bool m_regroupEscalated = false;
  int m_rootLeaves = 0;
  std::vector<double> m_leafTeleFlow;
  std::vector<double> m_leafTeleWeight;
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
  double hierarchicalCorrection(const ColumnarStackView& core, columnar::StackBreakdown* breakdown = nullptr) const override;

  // Removing one internal codebook drops the free-parameter count by one; the
  // per-parameter price is m_multiplier / (2 * totalDegree * ln2), the same
  // constant hierarchicalCorrection charges. Matches the OO pass, which prices a
  // dissolve through BiasedMapEquation::calcCodelengthOnModuleOfModules and so
  // sheds exactly this one codebook's free-parameter charge.
  double dissolveGainPerInternalNode() const override
  {
    return m_multiplier / (2.0 * m_totalDegree * std::log(2.0));
  }
  double dissolveNodeCharge(int numChildren, bool hasExitCodeword) const override
  {
    return dissolveGainPerInternalNode() * (numChildren - (hasExitCodeword ? 0 : 1));
  }

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
  double hierarchicalCorrection(const ColumnarStackView& core, columnar::StackBreakdown* breakdown = nullptr) const override;

  // The same penalty for a caller that has a leaf-module count but no stack to
  // attach a correction to: InfomapBase::evaluateColumnarPartition's fallback to
  // the object-oriented tree, where this is the one correction with no OO
  // counterpart to reproduce it.
  static double costOf(int numModules, unsigned int preferredNumModules)
  {
    return std::abs(numModules - static_cast<int>(preferredNumModules));
  }

  // |K - K_pref| is a scalar over the whole partition, not a sum over codebooks,
  // so a local per-node dissolve gain cannot account for it -- disable the pass,
  // exactly as the OO pass returns early on preferredNumberOfModules > 0.
  bool blocksLevelDissolve() const override { return true; }

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
  double hierarchicalCorrection(const ColumnarStackView& core, columnar::StackBreakdown* breakdown = nullptr) const override;

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
 * hierarchicalCorrection therefore asks the stack view for leafCodebookRates(): empty
 * under the base objective, where it keeps the two-global-sums form verbatim, and
 * per-module under L*. Leaf-module level only (module-of-modules delegates to
 * base; neither objective reads leaf flows above level 1). Same O(1) move-loop
 * hook as Meta with attribute = physical node id — the case the hook is designed
 * for; those deltas stay base-flavoured on purpose (see --non-redundant-exact).
 */
class MemCorrection final : public ColumnarCorrection {
public:
  MemCorrection(std::vector<int> leafPhysical, std::vector<double> leafFlow);
  double hierarchicalCorrection(const ColumnarStackView& core, columnar::StackBreakdown* breakdown = nullptr) const override;

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
  double hierarchicalCorrection(const ColumnarStackView& core, columnar::StackBreakdown* breakdown = nullptr) const override;

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
