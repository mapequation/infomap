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

namespace infomap {

class InfoNode;

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

  // Hierarchical codelength from the stacked levels/assignments in m_hier*.
  double hierarchicalCodelengthFromStack() const;

  bool m_undirected = false;
  unsigned long m_seed = 123;
  double m_exitNetworkFlow = 0.0; // flow leaving this (sub-)network; 0 if closed

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

} // namespace infomap

#endif // COLUMNAR_MAP_EQUATION_H_
