/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef COLUMNARBOUNDARYFLOW_H_
#define COLUMNARBOUNDARYFLOW_H_

#include "ColumnarLevel.h"
#include "ColumnarTuning.h" // teleIndexRate, read by unitIndexRate below

#include <vector>

namespace infomap {

/**
 * Owns how a set of units' boundary (enter/exit) flow is computed, aggregated
 * and sliced in the columnar core: the flow model's configuration (undirected
 * link flow, recorded teleportation and its global total), the aggregation of a
 * level under a unit -> group assignment, the slicing of a unit subset into its
 * own sub-network, and the per-module recorded-teleport enter/exit terms.
 *
 * ColumnarTwoLevel holds one of these and hands it on to every sub-optimizer
 * (buildFromLevel), so a sub-network sees the same flow model as its parent.
 */
class ColumnarBoundaryFlow {
public:
  using Level = ColumnarLevel;

  void setUndirected(bool undirected) { m_undirected = undirected; }
  void setRecordedTeleport(bool on) { m_recordedTeleport = on; }
  void setTotalTeleFlow(double totalTeleFlow) { m_totalTeleFlow = totalTeleFlow; }

  bool undirected() const { return m_undirected; }
  bool recordedTeleport() const { return m_recordedTeleport; }
  double totalTeleFlow() const { return m_totalTeleFlow; }

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

  // Aggregate a base level under a unit->group assignment (K groups): group
  // flow = sum of member flow, group enter/exit = crossing flow, plus the
  // group-group CSR. A pure function of (base, assign) and the undirected flag.
  Level aggregate(const Level& base, const std::vector<int>& assign, int K) const;

  // The internal sub-network over the base units `units` (local id j = position
  // in `units`): per-unit flow/enter/exit/teleport carried from base, and only
  // the edges with both ends in `units`, out-edges in base out-edge order and
  // in-edges placed by a running cursor. indexRateAsFlow applies the enter-flow
  // transform (an interior unit's codeword usage is its index rate q, see
  // unitIndexRate) instead of carrying base flow. `loc` is an all -1 scratch
  // vector over base units (global -> local id), restored before returning.
  Level slice(const Level& base, const std::vector<int>& units, std::vector<int>& loc, bool indexRateAsFlow) const;

  /**
   * Reduce a leaf level's stored enter/exit to the link-only crossings the
   * ColumnarLevel contract requires, under recorded teleportation.
   *
   * FlowCalculator stores a node's enter/exit as its TOTAL boundary rates — the
   * object-oriented convention since the regularized map equation (#181): with
   * teleportation recorded they already hold the node's own teleport enter
   * (T - t) * w and exit t * (1 - w). The columnar core adds the teleport half
   * per module instead (moduleTeleEnter/Exit, from the aggregated t and w), so
   * reading the stored rates as link flow counted every member's teleportation a
   * second time in every module rate the move loop and the aggregation passes
   * saw (#1042: the search steered by an objective 0.5 bits off on jazz, 0.65 on
   * om5). The stack scorer never saw it — it rebuilds module boundaries from the
   * link CSR — so every reported codelength was right; only the search was not.
   *
   * Call once, on the leaf level, and only when teleportation is recorded: in
   * every other flow model the stored rates are link-only already.
   */
  static void stripLeafTeleportRates(Level& leaf);

private:
  bool m_undirected = false;
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
  double m_totalTeleFlow = 0.0; // GLOBAL sum of leaf teleport flow (root teleport flow)
};

} // namespace infomap

#endif // COLUMNARBOUNDARYFLOW_H_
