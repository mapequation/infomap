/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "ColumnarBoundaryFlow.h"

#include <algorithm>
#include <unordered_map>

namespace infomap {

namespace {
  // Largest module x layer table TeleportSums keeps dense (cells, 16 B each);
  // beyond it the per-(module, layer) sums go to a hash.
  constexpr long long kDenseLayerCells = 1LL << 20;
} // namespace

ColumnarBoundaryFlow ColumnarBoundaryFlow::forLeaves(const Level& leaf, bool undirected, bool recordedTeleport)
{
  ColumnarBoundaryFlow flow;
  flow.setUndirected(undirected);
  flow.setRecordedTeleport(recordedTeleport);
  double totalTeleFlow = 0.0;
  for (double tf : leaf.teleFlow)
    totalTeleFlow += tf;
  flow.setTotalTeleFlow(totalTeleFlow);
  if (!leaf.layerStart.empty()) {
    flow.m_kind = TeleportKind::layered;
    int numLayers = 0;
    for (int l : leaf.layerId)
      numLayers = std::max(numLayers, l + 1);
    flow.m_layerTeleFlow.assign(numLayers, 0.0);
    flow.m_layerTeleWeight.assign(numLayers, 0.0);
    for (std::size_t k = 0; k < leaf.layerId.size(); ++k) {
      flow.m_layerTeleFlow[leaf.layerId[k]] += leaf.layerTeleFlow[k];
      flow.m_layerTeleWeight[leaf.layerId[k]] += leaf.layerTeleWeight[k];
    }
  }
  return flow;
}

void ColumnarBoundaryFlow::TeleportSums::reset(const ColumnarBoundaryFlow& flow, const Level& units)
{
  m_kind = flow.teleportKind();
  if (m_kind == TeleportKind::global && units.teleFlow.empty())
    m_kind = TeleportKind::none;
  if (m_kind == TeleportKind::layered && units.layerStart.empty())
    m_kind = TeleportKind::none;
  if (m_kind == TeleportKind::global) {
    m_flow = units.teleFlow;
    m_weight = units.teleWeight;
    return;
  }
  m_cells.clear();
  if (m_kind != TeleportKind::layered)
    return;
  m_numLayers = flow.numLayers();
  m_dense = static_cast<long long>(units.n) * m_numLayers <= kDenseLayerCells;
  if (m_dense) {
    m_flow.assign(static_cast<std::size_t>(units.n) * m_numLayers, 0.0);
    m_weight.assign(static_cast<std::size_t>(units.n) * m_numLayers, 0.0);
  } else {
    m_flow.clear();
    m_weight.clear();
    m_cells.reserve(units.layerId.size());
  }
  for (int u = 0; u < units.n; ++u)
    for (int k = units.layerStart[u]; k < units.layerStart[u + 1]; ++k)
      add(u, units.layerId[k], units.layerTeleFlow[k], units.layerTeleWeight[k]);
}

double ColumnarBoundaryFlow::TeleportSums::betweenModules(int a, int b) const
{
  if (m_kind == TeleportKind::global)
    return m_flow[a] * m_weight[b] + m_flow[b] * m_weight[a];
  if (m_kind != TeleportKind::layered)
    return 0.0;
  double sum = 0.0;
  for (int l = 0; l < m_numLayers; ++l) {
    const Cell ca = cell(a, l), cb = cell(b, l);
    sum += ca.t * cb.w + cb.t * ca.w;
  }
  return sum;
}

void ColumnarBoundaryFlow::TeleportSums::merge(int a, int b)
{
  if (m_kind == TeleportKind::global) {
    m_flow[b] += m_flow[a];
    m_weight[b] += m_weight[a];
    m_flow[a] = 0.0;
    m_weight[a] = 0.0;
    return;
  }
  if (m_kind != TeleportKind::layered)
    return;
  for (int l = 0; l < m_numLayers; ++l) {
    const Cell ca = cell(a, l);
    if (ca.t == 0.0 && ca.w == 0.0)
      continue;
    add(b, l, ca.t, ca.w);
    add(a, l, -ca.t, -ca.w);
  }
}

void ColumnarBoundaryFlow::aggregateLayers(const Level& base, const std::vector<int>& assign, int K, Level& out) const
{
  out.layerStart.clear();
  out.layerId.clear();
  out.layerTeleFlow.clear();
  out.layerTeleWeight.clear();
  if (base.layerStart.empty())
    return;
  // Members by group (counting sort, unit order kept), then each group's layers
  // gathered through a per-layer slot into its output range.
  std::vector<int> start(K + 1, 0);
  for (int i = 0; i < base.n; ++i)
    ++start[assign[i] + 1];
  for (int g = 0; g < K; ++g)
    start[g + 1] += start[g];
  std::vector<int> members(base.n);
  std::vector<int> pos(start.begin(), start.end() - 1);
  for (int i = 0; i < base.n; ++i)
    members[pos[assign[i]]++] = i;

  std::vector<int> slot(numLayers(), -1);
  out.layerStart.assign(K + 1, 0);
  out.layerId.reserve(base.layerId.size());
  out.layerTeleFlow.reserve(base.layerId.size());
  out.layerTeleWeight.reserve(base.layerId.size());
  for (int g = 0; g < K; ++g) {
    const int first = static_cast<int>(out.layerId.size());
    for (int m = start[g]; m < start[g + 1]; ++m) {
      const int i = members[m];
      for (int k = base.layerStart[i]; k < base.layerStart[i + 1]; ++k) {
        const int l = base.layerId[k];
        if (slot[l] < 0) {
          slot[l] = static_cast<int>(out.layerId.size());
          out.layerId.push_back(l);
          out.layerTeleFlow.push_back(0.0);
          out.layerTeleWeight.push_back(0.0);
        }
        out.layerTeleFlow[slot[l]] += base.layerTeleFlow[k];
        out.layerTeleWeight[slot[l]] += base.layerTeleWeight[k];
      }
    }
    out.layerStart[g + 1] = static_cast<int>(out.layerId.size());
    for (int k = first; k < out.layerStart[g + 1]; ++k)
      slot[out.layerId[k]] = -1;
  }
}

ColumnarBoundaryFlow::Level ColumnarBoundaryFlow::aggregate(const Level& base, const std::vector<int>& assign, int K) const
{
  Level out;
  out.n = K;
  out.flow.assign(K, 0.0);
  out.enter.assign(K, 0.0);
  out.exit.assign(K, 0.0);
  out.teleFlow.assign(K, 0.0);
  out.teleWeight.assign(K, 0.0);
  const bool haveTele = !base.teleFlow.empty();
  for (int i = 0; i < base.n; ++i) {
    out.flow[assign[i]] += base.flow[i];
    if (haveTele) {
      out.teleFlow[assign[i]] += base.teleFlow[i];
      out.teleWeight[assign[i]] += base.teleWeight[i];
    }
  }

  // Group-group edge flow + crossing enter/exit, from base out-edges.
  std::unordered_map<long long, double> edgeMap;
  edgeMap.reserve(base.outTarget.size());
  for (int a = 0; a < base.n; ++a) {
    const int ga = assign[a];
    for (int k = base.outStart[a]; k < base.outStart[a + 1]; ++k) {
      const int gb = assign[base.outTarget[k]];
      if (ga == gb)
        continue;
      const double f = base.outFlow[k];
      edgeMap[static_cast<long long>(ga) * K + gb] += f;
      if (m_undirected) {
        const double half = f / 2.0;
        out.exit[ga] += half;
        out.enter[ga] += half;
        out.enter[gb] += half;
        out.exit[gb] += half;
      } else {
        out.exit[ga] += f;
        out.enter[gb] += f;
      }
    }
  }

  // Total rates: add each group's teleport enter/exit from its aggregated sums.
  if (m_kind == TeleportKind::global && haveTele) {
    for (int g = 0; g < K; ++g) {
      out.enter[g] += moduleTeleEnter(out.teleFlow[g], out.teleWeight[g]);
      out.exit[g] += moduleTeleExit(out.teleFlow[g], out.teleWeight[g]);
    }
  }
  aggregateLayers(base, assign, K, out);
  if (m_kind == TeleportKind::layered && !out.layerStart.empty()) {
    for (int g = 0; g < K; ++g) {
      for (int k = out.layerStart[g]; k < out.layerStart[g + 1]; ++k) {
        out.enter[g] += layerTeleEnter(out.layerId[k], out.layerTeleFlow[k], out.layerTeleWeight[k]);
        out.exit[g] += layerTeleExit(out.layerId[k], out.layerTeleFlow[k], out.layerTeleWeight[k]);
      }
    }
  }

  std::vector<int> outDeg(K, 0), inDeg(K, 0);
  for (const auto& kv : edgeMap) {
    ++outDeg[static_cast<int>(kv.first / K)];
    ++inDeg[static_cast<int>(kv.first % K)];
  }
  out.outStart.assign(K + 1, 0);
  out.inStart.assign(K + 1, 0);
  for (int i = 0; i < K; ++i) {
    out.outStart[i + 1] = out.outStart[i] + outDeg[i];
    out.inStart[i + 1] = out.inStart[i] + inDeg[i];
  }
  out.outTarget.assign(out.outStart[K], 0);
  out.outFlow.assign(out.outStart[K], 0.0);
  out.inTarget.assign(out.inStart[K], 0);
  out.inFlow.assign(out.inStart[K], 0.0);
  std::vector<int> outPos(out.outStart.begin(), out.outStart.end() - 1);
  std::vector<int> inPos(out.inStart.begin(), out.inStart.end() - 1);
  for (const auto& kv : edgeMap) {
    const int ga = static_cast<int>(kv.first / K);
    const int gb = static_cast<int>(kv.first % K);
    const double f = kv.second;
    out.outTarget[outPos[ga]] = gb;
    out.outFlow[outPos[ga]] = f;
    ++outPos[ga];
    out.inTarget[inPos[gb]] = ga;
    out.inFlow[inPos[gb]] = f;
    ++inPos[gb];
  }
  return out;
}

ColumnarBoundaryFlow::Level ColumnarBoundaryFlow::slice(const Level& base, const std::vector<int>& units, std::vector<int>& loc, bool indexRateAsFlow) const
{
  const int nP = static_cast<int>(units.size());
  for (int j = 0; j < nP; ++j)
    loc[units[j]] = j;

  Level sub;
  sub.n = nP;
  sub.flow.resize(nP);
  sub.enter.resize(nP);
  sub.exit.resize(nP);
  sub.teleFlow.resize(nP);
  sub.teleWeight.resize(nP);
  std::vector<int> outDeg(nP, 0), inDeg(nP, 0);
  for (int j = 0; j < nP; ++j) {
    const int g = units[j];
    sub.flow[j] = indexRateAsFlow ? base.enter[g] : base.flow[g];
    sub.enter[j] = base.enter[g];
    sub.exit[j] = base.exit[g];
    sub.teleFlow[j] = base.teleFlow.empty() ? 0.0 : base.teleFlow[g];
    sub.teleWeight[j] = base.teleWeight.empty() ? 0.0 : base.teleWeight[g];
    for (int k = base.outStart[g]; k < base.outStart[g + 1]; ++k) {
      const int lt = loc[base.outTarget[k]];
      if (lt != -1) {
        ++outDeg[j];
        ++inDeg[lt];
      }
    }
  }
  sub.outStart.assign(nP + 1, 0);
  sub.inStart.assign(nP + 1, 0);
  for (int j = 0; j < nP; ++j) {
    sub.outStart[j + 1] = sub.outStart[j] + outDeg[j];
    sub.inStart[j + 1] = sub.inStart[j] + inDeg[j];
  }
  sub.outTarget.assign(sub.outStart[nP], 0);
  sub.outFlow.assign(sub.outStart[nP], 0.0);
  sub.inTarget.assign(sub.inStart[nP], 0);
  sub.inFlow.assign(sub.inStart[nP], 0.0);
  std::vector<int> op(sub.outStart.begin(), sub.outStart.end() - 1);
  std::vector<int> ip(sub.inStart.begin(), sub.inStart.end() - 1);
  for (int j = 0; j < nP; ++j) {
    const int g = units[j];
    for (int k = base.outStart[g]; k < base.outStart[g + 1]; ++k) {
      const int lt = loc[base.outTarget[k]];
      if (lt != -1) {
        const double f = base.outFlow[k];
        sub.outTarget[op[j]] = lt;
        sub.outFlow[op[j]] = f;
        ++op[j];
        sub.inTarget[ip[lt]] = j;
        sub.inFlow[ip[lt]] = f;
        ++ip[lt];
      }
    }
  }

  if (!base.layerStart.empty()) {
    sub.layerStart.assign(nP + 1, 0);
    for (int j = 0; j < nP; ++j) {
      const int g = units[j];
      for (int k = base.layerStart[g]; k < base.layerStart[g + 1]; ++k) {
        sub.layerId.push_back(base.layerId[k]);
        sub.layerTeleFlow.push_back(base.layerTeleFlow[k]);
        sub.layerTeleWeight.push_back(base.layerTeleWeight[k]);
      }
      sub.layerStart[j + 1] = static_cast<int>(sub.layerId.size());
    }
  }

  for (int j = 0; j < nP; ++j)
    loc[units[j]] = -1; // reset scratch
  return sub;
}

} // namespace infomap
