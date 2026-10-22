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

void ColumnarBoundaryFlow::stripLeafTeleportRates(Level& leaf)
{
  double totalTele = 0.0;
  for (double tf : leaf.teleFlow)
    totalTele += tf;
  // max(0, .): a node with no links keeps exactly its teleport rates, which the
  // subtraction cancels only to rounding.
  for (int i = 0; i < leaf.n; ++i) {
    leaf.linkEnter[i] = std::max(0.0, leaf.linkEnter[i] - (totalTele - leaf.teleFlow[i]) * leaf.teleWeight[i]);
    leaf.linkExit[i] = std::max(0.0, leaf.linkExit[i] - leaf.teleFlow[i] * (1.0 - leaf.teleWeight[i]));
  }
}

ColumnarBoundaryFlow::Level ColumnarBoundaryFlow::aggregate(const Level& base, const std::vector<int>& assign, int K) const
{
  Level out;
  out.n = K;
  out.flow.assign(K, 0.0);
  out.linkEnter.assign(K, 0.0);
  out.linkExit.assign(K, 0.0);
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
        out.linkExit[ga] += half;
        out.linkEnter[ga] += half;
        out.linkEnter[gb] += half;
        out.linkExit[gb] += half;
      } else {
        out.linkExit[ga] += f;
        out.linkEnter[gb] += f;
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
  sub.linkEnter.resize(nP);
  sub.linkExit.resize(nP);
  sub.teleFlow.resize(nP);
  sub.teleWeight.resize(nP);
  std::vector<int> outDeg(nP, 0), inDeg(nP, 0);
  for (int j = 0; j < nP; ++j) {
    const int g = units[j];
    sub.flow[j] = indexRateAsFlow ? unitIndexRate(base, g) : base.flow[g];
    sub.linkEnter[j] = base.linkEnter[g];
    sub.linkExit[j] = base.linkExit[g];
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

  for (int j = 0; j < nP; ++j)
    loc[units[j]] = -1; // reset scratch
  return sub;
}

} // namespace infomap
