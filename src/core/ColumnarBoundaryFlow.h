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

#include <cstddef>
#include <unordered_map>
#include <vector>

namespace infomap {

/**
 * Owns how a set of units' boundary (enter/exit) flow is computed, aggregated
 * and sliced in the columnar core: the flow model's configuration (undirected
 * link flow, recorded teleportation -- global or per layer -- and its totals),
 * the aggregation of a level under a unit -> group assignment, the slicing of a
 * unit subset into its own sub-network, and the recorded-teleport flow between
 * units and modules that a move or merge subtracts like link flow (TeleportSums).
 *
 * ColumnarTwoLevel holds one of these and hands it on to every sub-optimizer
 * (buildFromLevel), so a sub-network sees the same flow model as its parent.
 */
class ColumnarBoundaryFlow {
public:
  using Level = ColumnarLevel;

  // How teleportation is recorded in the boundary rates: not at all; globally
  // (regularized / --recorded-teleportation: a walker teleports anywhere, onto
  // teleWeight); or per layer (the regularized multilayer prior, #828: a walker
  // teleports within its own layer, onto the layer's teleport weights).
  enum class TeleportKind { none,
                            global,
                            layered };

  // The flow model of a leaf level: global teleport total = sum of leaf teleport
  // flow, and the layered kind whenever the level carries per-layer teleport
  // data (the regularized multilayer prior applies whether or not global
  // teleportation is recorded, as in InfomapBase::aggregateFlowValuesFromLeafToRoot).
  static ColumnarBoundaryFlow forLeaves(const Level& leaf, bool undirected, bool recordedTeleport);

  void setUndirected(bool undirected) { m_undirected = undirected; }
  // Global recording on/off; the layered kind is a property of the input and stays.
  void setRecordedTeleport(bool on)
  {
    if (m_kind != TeleportKind::layered)
      m_kind = on ? TeleportKind::global : TeleportKind::none;
  }
  void setTotalTeleFlow(double totalTeleFlow) { m_totalTeleFlow = totalTeleFlow; }

  bool undirected() const { return m_undirected; }
  TeleportKind teleportKind() const { return m_kind; }
  double totalTeleFlow() const { return m_totalTeleFlow; }
  int numLayers() const { return static_cast<int>(m_layerTeleFlow.size()); }

  // A module's recorded-teleport enter/exit from its aggregated teleport flow tf
  // and weight tw (see InfomapBase::aggregateFlowValuesFromLeafToRoot): a walker
  // teleports out with the fraction not landing back (exit), and teleports in from
  // the rest of the network onto this module's teleport weight (enter).
  double moduleTeleEnter(double tf, double tw) const { return (m_totalTeleFlow - tf) * tw; }
  double moduleTeleExit(double tf, double tw) const { return tf * (1.0 - tw); }
  // The same within layer l, against the layer's totals T_l, W_l: a module's
  // teleport enter/exit is the sum of these over its layers.
  double layerTeleEnter(int l, double tf, double tw) const { return (m_layerTeleFlow[l] - tf) * tw; }
  double layerTeleExit(int l, double tf, double tw) const { return tf * (m_layerTeleWeight[l] - tw); }

  // A unit's exit without its own teleport exit: its link crossings alone. The
  // sub-searches that re-cluster inside a parent are handed this as their exit
  // codeword, which under-prices leaving the parent under recorded teleportation
  // (#1372). Kept on purpose: passing the total exit is a search change with its
  // own cost in deep repair, to be measured on its own.
  double linkExit(const Level& level, int i) const
  {
    if (m_kind == TeleportKind::layered && !level.layerStart.empty()) {
      double exit = level.exit[i];
      for (int k = level.layerStart[i]; k < level.layerStart[i + 1]; ++k)
        exit -= layerTeleExit(level.layerId[k], level.layerTeleFlow[k], level.layerTeleWeight[k]);
      return exit;
    }
    if (m_kind != TeleportKind::global || level.teleFlow.empty())
      return level.exit[i];
    return level.exit[i] - moduleTeleExit(level.teleFlow[i], level.teleWeight[i]);
  }

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
   * directions) is t_u W_M + T_M w_u; per layer it is that sum over the layers u
   * has (one for a leaf). Every query returns 0, and every update is a no-op, when
   * the flow model does not record teleportation, so search code never branches
   * on it. The move loop, the hottest caller, still asks kind() once and compiles
   * one body per kind, calling the *As<Kind> forms (moveLoopWith).
   *
   * Layered sums live in a dense module x layer table up to 2^20 cells (the real
   * multilayer networks benchmarked here stay well below it at singletons), else
   * in a hash keyed (module, layer). A unit query iterates the unit's layers (one
   * for a leaf, so O(1) per candidate); a module-module query (merge pass)
   * iterates all layers, which are few wherever per-layer teleportation applies.
   */
  class TeleportSums {
  public:
    TeleportKind kind() const { return m_kind; }
    bool on() const { return m_kind != TeleportKind::none; }

    // Every unit of `units` in its own module.
    void reset(const ColumnarBoundaryFlow& flow, const Level& units);

    // Teleport flow between unit u and module M, u not in M.
    template <TeleportKind K>
    double betweenAs(const Level& units, int u, int M) const
    {
      if constexpr (K == TeleportKind::global) {
        return units.teleFlow[u] * m_weight[M] + m_flow[M] * units.teleWeight[u];
      } else {
        double sum = 0.0;
        for (int k = units.layerStart[u]; k < units.layerStart[u + 1]; ++k) {
          const Cell c = cell(M, units.layerId[k]);
          sum += units.layerTeleFlow[k] * c.w + c.t * units.layerTeleWeight[k];
        }
        return sum;
      }
    }

    // Teleport flow between unit u and the rest of its own module A.
    template <TeleportKind K>
    double betweenOwnAs(const Level& units, int u, int A) const
    {
      if constexpr (K == TeleportKind::global) {
        const double tu = units.teleFlow[u], wu = units.teleWeight[u];
        return tu * (m_weight[A] - wu) + (m_flow[A] - tu) * wu;
      } else {
        double sum = 0.0;
        for (int k = units.layerStart[u]; k < units.layerStart[u + 1]; ++k) {
          const Cell c = cell(A, units.layerId[k]);
          const double tu = units.layerTeleFlow[k], wu = units.layerTeleWeight[k];
          sum += tu * (c.w - wu) + (c.t - tu) * wu;
        }
        return sum;
      }
    }

    template <TeleportKind K>
    void moveAs(const Level& units, int u, int from, int to)
    {
      if constexpr (K == TeleportKind::global) {
        m_flow[from] -= units.teleFlow[u];
        m_weight[from] -= units.teleWeight[u];
        m_flow[to] += units.teleFlow[u];
        m_weight[to] += units.teleWeight[u];
      } else {
        for (int k = units.layerStart[u]; k < units.layerStart[u + 1]; ++k) {
          const int l = units.layerId[k];
          add(from, l, -units.layerTeleFlow[k], -units.layerTeleWeight[k]);
          add(to, l, units.layerTeleFlow[k], units.layerTeleWeight[k]);
        }
      }
    }

    double between(const Level& units, int u, int M) const
    {
      switch (m_kind) {
      case TeleportKind::global:
        return betweenAs<TeleportKind::global>(units, u, M);
      case TeleportKind::layered:
        return betweenAs<TeleportKind::layered>(units, u, M);
      default:
        return 0.0;
      }
    }
    double betweenOwn(const Level& units, int u, int A) const
    {
      switch (m_kind) {
      case TeleportKind::global:
        return betweenOwnAs<TeleportKind::global>(units, u, A);
      case TeleportKind::layered:
        return betweenOwnAs<TeleportKind::layered>(units, u, A);
      default:
        return 0.0;
      }
    }
    void move(const Level& units, int u, int from, int to)
    {
      if (m_kind == TeleportKind::global)
        moveAs<TeleportKind::global>(units, u, from, to);
      else if (m_kind == TeleportKind::layered)
        moveAs<TeleportKind::layered>(units, u, from, to);
    }

    // Module M's global teleport flow / weight sums (0 unless recording globally).
    double flowOf(int M) const { return m_kind == TeleportKind::global ? m_flow[M] : 0.0; }
    double weightOf(int M) const { return m_kind == TeleportKind::global ? m_weight[M] : 0.0; }

    // Teleport flow between modules a and b.
    double betweenModules(int a, int b) const;

    // Fold module a into module b.
    void merge(int a, int b);

  private:
    struct Cell {
      double t = 0.0, w = 0.0;
    };
    Cell cell(int M, int l) const
    {
      if (m_dense)
        return { m_flow[static_cast<std::size_t>(M) * m_numLayers + l], m_weight[static_cast<std::size_t>(M) * m_numLayers + l] };
      const auto it = m_cells.find(static_cast<long long>(M) * m_numLayers + l);
      return it == m_cells.end() ? Cell {} : it->second;
    }
    void add(int M, int l, double t, double w)
    {
      if (m_dense) {
        m_flow[static_cast<std::size_t>(M) * m_numLayers + l] += t;
        m_weight[static_cast<std::size_t>(M) * m_numLayers + l] += w;
        return;
      }
      Cell& c = m_cells[static_cast<long long>(M) * m_numLayers + l];
      c.t += t;
      c.w += w;
    }

    TeleportKind m_kind = TeleportKind::none;
    // global: per module, sum of member teleport flow / weight. layered + dense:
    // the same per (module, layer), at M * m_numLayers + l.
    std::vector<double> m_flow, m_weight;
    int m_numLayers = 0;
    bool m_dense = true;
    std::unordered_map<long long, Cell> m_cells; // layered, not dense
  };

  // Aggregate a base level under a unit->group assignment (K groups): group
  // flow = sum of member flow, group enter/exit = crossing link flow plus, under
  // recorded teleportation, the group's moduleTeleEnter/Exit or per-layer
  // layerTeleEnter/Exit (the only place the closed form is evaluated), plus the
  // group-group CSR. A pure function of (base, assign) and the flow model.
  Level aggregate(const Level& base, const std::vector<int>& assign, int K) const;

  // The per-layer teleport CSR of `out` (K groups) from base's under `assign`:
  // each group's layers with their summed flow / weight. Clears it when base
  // carries none. Shared by aggregate and the search's consolidation.
  void aggregateLayers(const Level& base, const std::vector<int>& assign, int K, Level& out) const;

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
  // flow/weight (per layer when layered) are threaded through every Level (they
  // sum under aggregation); aggregate folds the closed form into group
  // enter/exit, and searches subtract the between-part teleport flow via
  // TeleportSums. The totals are GLOBAL (constant across sub-networks), so
  // sub-optimizers inherit this whole object via buildFromLevel rather than
  // recomputing from their local units.
  TeleportKind m_kind = TeleportKind::none;
  double m_totalTeleFlow = 0.0; // GLOBAL sum of leaf teleport flow (root teleport flow)
  // layered: per compact layer id, the whole network's T_l / W_l.
  std::vector<double> m_layerTeleFlow, m_layerTeleWeight;
};

} // namespace infomap

#endif // COLUMNARBOUNDARYFLOW_H_
