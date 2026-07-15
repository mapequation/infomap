/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef COLUMNAR_MAP_EQUATION_H_
#define COLUMNAR_MAP_EQUATION_H_

#include <vector>
#include <cstddef>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace infomap {

class InfoNode;
class ColumnarTwoLevel;

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
  virtual double hierarchicalCorrection(const ColumnarTwoLevel& core) const = 0;

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
  virtual double initMoveLoop(const std::vector<int>& leafModule, int numModules) { return 0.0; }
  // Change in this correction's contribution from moving `leaf` oldMod -> newMod.
  virtual double moveDelta(int leaf, int oldMod, int newMod) const { return 0.0; }
  // Commit the move to the per-module state.
  virtual void applyMove(int leaf, int oldMod, int newMod) {}

  // Append move-target modules the search should also consider for `leaf`,
  // beyond its edge neighborhood. The base move loop only proposes modules of
  // edge-connected neighbors; Mem uses this to propose modules that already hold
  // a co-physical state node (same physical id), so the search can find the
  // flow-disconnected merges that collapse the physical codebook — the merges
  // the correction rewards but the edge-based loop would never generate. The
  // leaf's current module may be included; the caller skips it. Default none.
  virtual void proposeMoveTargets(int leaf, std::vector<int>& out) const {}

  // --- Leaf-module merge hooks (mem-aware coarsening) -----------------------
  // The leaf-module merge operator folds one leaf module into another to coarsen
  // the partition where the correction rewards it (Mem: combine two modules'
  // physical codebooks). It reuses the move-loop per-module state built by
  // initMoveLoop (so modules here are leaf-module ids), and needs the change in
  // this correction's contribution from merging module A into module B, plus a
  // commit. Base objective / structural corrections leave these zero/no-op.
  virtual double mergeDelta(int moduleA, int moduleB) const { return 0.0; }
  virtual void applyMerge(int moduleA, int moduleB) {}

  // Restrict this correction to a subset of leaves for an in-context leaf refine
  // (the bottom-level re-partition within a first-order parent). `globalLeafIds`
  // lists the sub-problem's leaves by global id in the sub-problem's local order,
  // so the returned correction is indexed by local leaf id. Leaf-shaping
  // corrections (Meta/Mem) return a sliced copy; structural ones return nullptr
  // and stay out of the refine, which then optimizes the base objective there.
  // Because a bottom module lives entirely within one parent, per-parent slicing
  // is exact — a physical node's / category's per-module term never couples
  // across parents.
  virtual std::unique_ptr<ColumnarCorrection> sliceForLeaves(const std::vector<int>& globalLeafIds) const { return nullptr; }
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
  struct Level {
    int n = 0;
    std::vector<double> flow, enter, exit;
    std::vector<int> outStart, outTarget;
    std::vector<double> outFlow;
    std::vector<int> inStart, inTarget;
    std::vector<double> inFlow;
  };

  // Add a composable objective correction (ownership transferred). No
  // corrections = base map equation. Corrections sum into the codelength.
  void addCorrection(std::unique_ptr<ColumnarCorrection> correction)
  {
    m_corrections.push_back(std::move(correction));
  }

  // --- Public read accessors for corrections (partition shape + leaf data) ---
  // Number of tree levels currently in the stacked hierarchy (0 = leaves).
  unsigned int hierNumLevels() const { return static_cast<unsigned int>(m_hierLevels.size()); }
  // Unit count at a stacked level (level 0 = leaves, 1.. = module levels).
  int hierLevelSize(int level) const { return m_hierLevels[level].n; }
  int numLeaves() const { return m_nLeaves; }
  // The leaf's current bottom (level-1) module id.
  int hierLeafModule(int leaf) const { return m_hierAssign[0][leaf]; }
  double leafFlow(int leaf) const { return m_leafFlow[leaf]; }

  // Build the leaf level (flow, enter/exit, out+in CSR) from the leaf network.
  void buildFromLeaves(const std::vector<InfoNode*>& leafNodes, bool undirected, unsigned long seed);

  // Build the leaf level directly from columnar arrays (used to feed the
  // enter-flow super-network, or a sub-network within a module, into a nested
  // optimizer). exitNetworkFlow is the flow leaving the (sub-)network — 0 for
  // the whole closed network, the module's exit flow for an in-context refine.
  void buildFromLevel(const Level& level, bool undirected, unsigned long seed, double exitNetworkFlow = 0.0);

  // Run repeated aggregation (Louvain-style) and return the two-level codelength
  // of the resulting leaves -> top-modules partition. maxAggPasses > 0 stops the
  // aggregation early (finer "building block" bottom); doFineTune toggles the
  // level-0 fine-tune-to-convergence.
  double optimizeTwoLevel(unsigned int maxAggPasses = 0, bool doFineTune = true);

  // Run the two-level optimize, then grow the hierarchy upward with the
  // enter-flow super-search (M1c). bottomBlockLimit > 0 seeds a finer bottom
  // level. Returns the hierarchical codelength of the multi-level partition.
  double optimizeHierarchical(unsigned int bottomBlockLimit = 0);

  // M2: build the hierarchy, then refine to convergence. Returns hierarchical
  // codelength. Uses a fine bottom by default so the interior tune has room.
  double optimizeFlexible(unsigned int bottomBlockLimit = 1);

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

  // Top-level columnar engine entry (the `--columnar` search): run the up/down
  // sweep at a small set of up-merge settings and keep the best partition,
  // leaving its stacked hierarchy in the members ready to materialize into an
  // InfoNode tree. sweepLimit caps the tuning sweeps per strategy (0 = until
  // convergence; wired to --tune-iteration-limit).
  double optimizeColumnar(unsigned int bottomBlockLimit = 1, unsigned int sweepLimit = 0);

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

private:
  // Move-loop machinery operating on the current level `m_lvl`.
  void initPartition();
  // Seed the partition of `m_lvl` at a given unit->module assignment (the shared
  // primitive behind fine/coarse/interior tuning), maintaining exact aggregates.
  void seedAssignment(const std::vector<int>& assign);
  // Forced move of one unit to a target module, updating aggregates + terms.
  void moveUnit(int u, int newMod);
  unsigned int moveLoop();
  // Build the next level from the current module assignment; returns unit count.
  int consolidateToNextLevel();

  // Aggregate a base level under a unit->group assignment (K groups): group
  // flow = sum of member flow, group enter/exit = crossing flow, plus the
  // group-group CSR. A pure function of (base, assign).
  static Level aggregateLevel(const Level& base, const std::vector<int>& assign, int K, bool undirected);

  // Refine each level-2 module's leaves as their own in-context two-level
  // problem, replacing the bottom level. Returns true if any module changed.
  bool refineBottomWithinParents();

  // Generalized within-grandparent refine (M3): re-partition layer-k units into
  // new layer-(k+1) modules, each constrained to stay within its layer-(k+2)
  // grandparent (the grandparent's exit is the sub-network exitNetworkFlow).
  // Rebuilds m_hierAssign[k], m_hierAssign[k+1], m_hierLevels[k+1]; layers k+2
  // and above are untouched. Requires k in [0, top-2]. refineBottomWithinParents
  // is the k == 0 special case. Returns false if no grandparent layer exists.
  bool refineLayerWithinGrandparent(int k);

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

  // Total correction contribution for a leaf->module assignment (numModules
  // groups), summed over the move-loop-participating corrections. Used so the
  // two-level optimizer selects partitions by the augmented objective (base +
  // correction) rather than base alone. Re-seeds each correction's move-loop
  // state as a side effect (harmless: the next move loop re-inits it).
  double augmentedCorrectionFor(const std::vector<int>& leafAssign, int numModules);

  // Hierarchical codelength from the stacked levels/assignments in m_hier*.
  double hierarchicalCodelengthFromStack() const;

  bool m_undirected = false;
  unsigned long m_seed = 123;
  double m_exitNetworkFlow = 0.0; // flow leaving this (sub-)network; 0 if closed
  unsigned int m_superAggLimit = 0; // >0: conservative up-build (passes/super-level)
  bool m_leafMoveLoop = false; // true while moveLoop units are leaves (corrections active)
  bool m_seededPhase = false; // true when the move loop starts from an existing partition (fine-tune/refine)
  double m_lastCorrection = 0.0; // correction total of the last leaf move loop (0 if none)
  std::vector<std::unique_ptr<ColumnarCorrection>> m_corrections; // objective add-ons

  // Sum of the composable corrections' contributions to the hierarchical
  // codelength (0 for the base objective, i.e. no corrections).
  double objectiveCorrection() const;

  Level m_leaf0; // immutable leaf network
  Level m_lvl;
  std::vector<int> m_module; // unit -> module id
  std::vector<double> m_mFlow, m_mEnter, m_mExit;
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
  double m_leafNodeFlow_log_nodeFlow = 0.0; // over leaves, constant
  unsigned int m_numTopModules = 0;

  // Stacked hierarchy after optimizeHierarchical: m_hierLevels[0] = leaves,
  // [1] = first module level, ... ; m_hierAssign[k] maps a level-k unit to its
  // level-(k+1) parent.
  std::vector<Level> m_hierLevels;
  std::vector<std::vector<int>> m_hierAssign;
};

/**
 * Biased objective: the entropy bias correction. calcCodelengthOnTree adds
 * mult*childDegree/(2*D) to every internal node incl. the root, which sums over
 * the tree to mult*(non-root node count)/(2*D) = mult*(sum of level sizes incl
 * leaves)/(2*D). A structural correction (all levels), no per-node state.
 */
class BiasedEntropyCorrection final : public ColumnarCorrection {
public:
  BiasedEntropyCorrection(double multiplier, double totalDegree)
      : m_multiplier(multiplier), m_totalDegree(totalDegree > 0.0 ? totalDegree : 1.0) {}
  double hierarchicalCorrection(const ColumnarTwoLevel& core) const override;

private:
  double m_multiplier;
  double m_totalDegree;
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
  double hierarchicalCorrection(const ColumnarTwoLevel& core) const override;

  bool participatesInMoveLoop() const override { return true; }
  double initMoveLoop(const std::vector<int>& leafModule, int numModules) override;
  double moveDelta(int leaf, int oldMod, int newMod) const override;
  void applyMove(int leaf, int oldMod, int newMod) override;
  double mergeDelta(int moduleA, int moduleB) const override;
  void applyMerge(int moduleA, int moduleB) override;
  std::unique_ptr<ColumnarCorrection> sliceForLeaves(const std::vector<int>& globalLeafIds) const override;

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
};

/**
 * Memory objective (state / higher-order networks): the leaf-module codebook is
 * over PHYSICAL nodes, not state nodes — state nodes of the same physical node
 * in the same module share a codeword (their flows sum). Working the
 * T-normalized module term out, the module-flow*log(T) parts cancel between the
 * state and physical versions, leaving a correction to the base (state-node)
 * codelength of  C_state - sum_{module,phys} plogp(physFlow),  where
 * C_state = sum_leaf plogp(stateFlow) is a constant and physFlow is the summed
 * state flow of a physical node within a module. Leaf-module level only
 * (module-of-modules delegates to base). Same O(1) move-loop hook as Meta with
 * attribute = physical node id — the case the hook is designed for.
 */
class MemCorrection final : public ColumnarCorrection {
public:
  MemCorrection(std::vector<int> leafPhysical, std::vector<double> leafFlow);
  double hierarchicalCorrection(const ColumnarTwoLevel& core) const override;

  bool participatesInMoveLoop() const override { return true; }
  double initMoveLoop(const std::vector<int>& leafModule, int numModules) override;
  double moveDelta(int leaf, int oldMod, int newMod) const override;
  void applyMove(int leaf, int oldMod, int newMod) override;
  void proposeMoveTargets(int leaf, std::vector<int>& out) const override;
  double mergeDelta(int moduleA, int moduleB) const override;
  void applyMerge(int moduleA, int moduleB) override;
  std::unique_ptr<ColumnarCorrection> sliceForLeaves(const std::vector<int>& globalLeafIds) const override;

private:
  double physFlow(int module, int physical) const;

  std::vector<int> m_leafPhysical; // per leaf (state node): physical node id
  std::vector<double> m_leafFlow; // per leaf: state flow
  double m_cState; // constant sum_leaf plogp(stateFlow)

  std::vector<std::unordered_map<int, double>> m_modulePhysFlow; // physFlow_{m,p}
  // Reverse index physical id -> modules currently holding a state node of that
  // physical node, so proposeMoveTargets can offer co-physical merge targets in
  // ~O(modules-per-physical) without scanning. Maintained alongside
  // m_modulePhysFlow in initMoveLoop/applyMove.
  std::unordered_map<int, std::unordered_set<int>> m_physModules;
};

} // namespace infomap

#endif // COLUMNAR_MAP_EQUATION_H_
