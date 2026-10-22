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

#include <vector>

namespace infomap {

/**
 * Owns how a set of units' boundary (enter/exit) flow is computed, aggregated
 * and sliced in the columnar core: the flow model's configuration (undirected
 * link flow, recorded teleportation and its global total), the aggregation of a
 * level under a unit -> group assignment, the slicing of a unit subset into its
 * own sub-network, and the recorded-teleport flow between units and modules that
 * a move or merge subtracts like link flow (TeleportSums).
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

  // Enter-flow transform: node flow := enter for every unit. A unit's enter is its
  // index-codebook use rate q (teleporting in included, #1038), which is what the
  // objective charges for its codeword in a module-of-modules codebook. enter/exit,
  // teleport aggregates and edges are carried through untouched.
  static void setIndexRateAsFlow(Level& level) { level.flow = level.enter; }

  /**
   * Live per-module teleport sums of a search over one level's units, so that a
   * move or merge can subtract the recorded-teleport flow between the parts from
   * the TOTAL enter/exit exactly as it subtracts their link flow. With module sums
   * T_M = sum t, W_M = sum w, the teleport flow between unit u and module M (both
   * directions) is t_u W_M + T_M w_u. Every query returns 0, and every update is a
   * no-op, when the flow model does not record teleportation, so search code never
   * branches on it.
   */
  class TeleportSums {
  public:
    // Every unit of `units` in its own module.
    void reset(const ColumnarBoundaryFlow& flow, const Level& units)
    {
      m_on = flow.recordedTeleport() && !units.teleFlow.empty();
      if (!m_on)
        return;
      m_flow = units.teleFlow;
      m_weight = units.teleWeight;
    }

    // Teleport flow between unit u and module M, u not in M.
    double between(const Level& units, int u, int M) const
    {
      if (!m_on)
        return 0.0;
      return units.teleFlow[u] * m_weight[M] + m_flow[M] * units.teleWeight[u];
    }

    // Teleport flow between unit u and the rest of its own module A.
    double betweenOwn(const Level& units, int u, int A) const
    {
      if (!m_on)
        return 0.0;
      const double tu = units.teleFlow[u], wu = units.teleWeight[u];
      return tu * (m_weight[A] - wu) + (m_flow[A] - tu) * wu;
    }

    void move(const Level& units, int u, int from, int to)
    {
      if (!m_on)
        return;
      m_flow[from] -= units.teleFlow[u];
      m_weight[from] -= units.teleWeight[u];
      m_flow[to] += units.teleFlow[u];
      m_weight[to] += units.teleWeight[u];
    }

    // Module M's teleport flow / weight sums (0 when not recording).
    double flowOf(int M) const { return m_on ? m_flow[M] : 0.0; }
    double weightOf(int M) const { return m_on ? m_weight[M] : 0.0; }

    // Teleport flow between modules a and b.
    double betweenModules(int a, int b) const
    {
      if (!m_on)
        return 0.0;
      return m_flow[a] * m_weight[b] + m_flow[b] * m_weight[a];
    }

    // Fold module a into module b.
    void merge(int a, int b)
    {
      if (!m_on)
        return;
      m_flow[b] += m_flow[a];
      m_weight[b] += m_weight[a];
      m_flow[a] = 0.0;
      m_weight[a] = 0.0;
    }

  private:
    bool m_on = false;
    std::vector<double> m_flow, m_weight; // per module: sum of member teleport flow / weight
  };

  // Aggregate a base level under a unit->group assignment (K groups): group
  // flow = sum of member flow, group enter/exit = crossing link flow plus, under
  // recorded teleportation, the group's moduleTeleEnter/Exit (the only place the
  // closed form is evaluated), plus the group-group CSR. A pure function of
  // (base, assign) and the flow model.
  Level aggregate(const Level& base, const std::vector<int>& assign, int K) const;

  // The internal sub-network over the base units `units` (local id j = position
  // in `units`): per-unit flow/enter/exit/teleport carried from base, and only
  // the edges with both ends in `units`, out-edges in base out-edge order and
  // in-edges placed by a running cursor. indexRateAsFlow applies the enter-flow
  // transform (an interior unit's codeword usage is its enter rate, see
  // setIndexRateAsFlow) instead of carrying base flow. `loc` is an all -1 scratch
  // vector over base units (global -> local id), restored before returning.
  Level slice(const Level& base, const std::vector<int>& units, std::vector<int>& loc, bool indexRateAsFlow) const;

private:
  bool m_undirected = false;
  // Recorded teleportation (regularized / --recorded-teleportation): the walker
  // records teleport steps, so a module's enter/exit gains a dense all-to-all
  // teleport term the link-crossing aggregation can't see. Per-unit teleport
  // flow/weight are threaded through every Level (they sum under aggregation);
  // aggregate folds the closed form into group enter/exit, and searches subtract
  // the between-part teleport flow via TeleportSums. m_totalTeleFlow is the
  // GLOBAL total (constant across sub-networks), so sub-optimizers inherit it via
  // buildFromLevel rather than recomputing from their local units.
  bool m_recordedTeleport = false;
  double m_totalTeleFlow = 0.0; // GLOBAL sum of leaf teleport flow (root teleport flow)
};

} // namespace infomap

#endif // COLUMNARBOUNDARYFLOW_H_
