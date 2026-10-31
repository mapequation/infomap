/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

// ColumnarMapEquation: the read-only columnar mirror of an existing InfoNode
// tree, used by the `--columnar-check` parity gate to reproduce
// InfomapBase::calcCodelengthOnTree from a columnar aggregation. It optimizes
// nothing and shares no state with the search — hence its own translation unit.

#include "ColumnarMapEquation.h"
#include "InfoNode.h"
#include "InfoEdge.h"
#include "../utils/infomath.h"

#include <algorithm>
#include <numeric>
#include <unordered_map>
#include <vector>

namespace infomap {

void ColumnarMapEquation::buildFromTree(const InfoNode& root, const std::vector<InfoNode*>& leafNodes, bool undirected)
{
  m_undirected = undirected;
  const std::size_t nLeaves = leafNodes.size();

  m_leafFlow.assign(nLeaves, 0.0);
  std::unordered_map<const InfoNode*, int> leafId;
  leafId.reserve(nLeaves * 2);
  for (std::size_t i = 0; i < nLeaves; ++i) {
    m_leafFlow[i] = leafNodes[i]->data.flow;
    leafId[leafNodes[i]] = static_cast<int>(i);
  }

  // DFS over internal nodes (pre-order): assign module ids, depth, parent.
  // A node is always popped after its parent, so the parent id is known.
  m_modFlow.clear();
  m_modEnter.clear();
  m_modExit.clear();
  m_modParent.clear();
  m_modDepth.clear();
  m_modLeafModule.clear();
  std::vector<const InfoNode*> modNode;
  std::unordered_map<const InfoNode*, int> modId;

  std::vector<std::pair<const InfoNode*, int>> stack;
  stack.emplace_back(&root, 0);
  while (!stack.empty()) {
    const InfoNode* node = stack.back().first;
    const int depth = stack.back().second;
    stack.pop_back();

    const int id = static_cast<int>(m_modFlow.size());
    modId[node] = id;
    m_modFlow.push_back(0.0);
    m_modEnter.push_back(0.0);
    m_modExit.push_back(0.0);
    m_modDepth.push_back(depth);

    int par = -1;
    if (node->parent != nullptr) {
      auto it = modId.find(node->parent);
      if (it != modId.end())
        par = it->second;
    }
    m_modParent.push_back(par);
    m_modLeafModule.push_back(node->firstChild != nullptr && node->firstChild->isLeaf() ? 1 : 0);
    modNode.push_back(node);

    for (const auto& child : *node) {
      if (!child.isLeaf())
        stack.emplace_back(&child, depth + 1);
    }
  }
  const int nMod = static_cast<int>(m_modFlow.size());

  // Children as CSR (second pass: all ids now assigned).
  m_childStart.assign(nMod + 1, 0);
  for (int m = 0; m < nMod; ++m) {
    int cnt = 0;
    for (const auto& child : *modNode[m]) {
      (void)child;
      ++cnt;
    }
    m_childStart[m + 1] = m_childStart[m] + cnt;
  }
  m_childList.assign(static_cast<std::size_t>(m_childStart[nMod]), 0);
  for (int m = 0; m < nMod; ++m) {
    int pos = m_childStart[m];
    const bool leafMod = m_modLeafModule[m] != 0;
    for (const auto& child : *modNode[m]) {
      m_childList[pos++] = leafMod ? leafId.at(&child) : modId.at(&child);
    }
  }

  // Leaf -> parent module id.
  m_leafParentMod.assign(nLeaves, -1);
  for (std::size_t i = 0; i < nLeaves; ++i) {
    m_leafParentMod[i] = modId.at(leafNodes[i]->parent);
  }

  aggregate(leafNodes);
}

void ColumnarMapEquation::aggregate(const std::vector<InfoNode*>& leafNodes)
{
  const int nMod = static_cast<int>(m_modFlow.size());
  std::fill(m_modFlow.begin(), m_modFlow.end(), 0.0);
  std::fill(m_modEnter.begin(), m_modEnter.end(), 0.0);
  std::fill(m_modExit.begin(), m_modExit.end(), 0.0);

  // Module flow: deepest first so children are summed before their parents.
  std::vector<int> order(nMod);
  std::iota(order.begin(), order.end(), 0);
  std::sort(order.begin(), order.end(), [this](int a, int b) { return m_modDepth[a] > m_modDepth[b]; });
  for (int m : order) {
    const bool leafMod = m_modLeafModule[m] != 0;
    double f = 0.0;
    for (int k = m_childStart[m]; k < m_childStart[m + 1]; ++k) {
      const int c = m_childList[k];
      f += leafMod ? m_leafFlow[c] : m_modFlow[c];
    }
    m_modFlow[m] = f;
  }

  // Enter/exit flow: walk each leaf out-edge from the two endpoints' modules up
  // to their lowest common ancestor, matching aggregateFlowValuesFromLeafToRoot.
  const std::size_t nLeaves = leafNodes.size();
  std::unordered_map<const InfoNode*, int> leafId;
  leafId.reserve(nLeaves * 2);
  for (std::size_t i = 0; i < nLeaves; ++i)
    leafId[leafNodes[i]] = static_cast<int>(i);

  for (std::size_t i = 0; i < nLeaves; ++i) {
    InfoNode* s = leafNodes[i];
    for (InfoEdge* e : s->outEdges()) {
      auto tIt = leafId.find(e->target);
      if (tIt == leafId.end())
        continue; // target not a leaf in this set (shouldn't happen)
      int a = m_leafParentMod[i];
      int b = m_leafParentMod[tIt->second];
      if (a == b)
        continue; // intra-module edge: no contribution to module enter/exit

      const double f = e->data.flow;
      const double half = f / 2.0;
      while (m_modDepth[a] > m_modDepth[b]) {
        if (m_undirected) {
          m_modExit[a] += half;
          m_modEnter[a] += half;
        } else {
          m_modExit[a] += f;
        }
        a = m_modParent[a];
      }
      while (m_modDepth[b] > m_modDepth[a]) {
        if (m_undirected) {
          m_modEnter[b] += half;
          m_modExit[b] += half;
        } else {
          m_modEnter[b] += f;
        }
        b = m_modParent[b];
      }
      while (a != b) {
        if (m_undirected) {
          m_modExit[a] += half;
          m_modEnter[a] += half;
          m_modEnter[b] += half;
          m_modExit[b] += half;
        } else {
          m_modExit[a] += f;
          m_modEnter[b] += f;
        }
        a = m_modParent[a];
        b = m_modParent[b];
      }
    }
  }
}

double ColumnarMapEquation::moduleTerm(int m) const
{
  using infomath::plogp;
  if (m_modLeafModule[m] != 0) {
    // Module of leaf nodes: index codebook over child flow + module exit.
    const double T = m_modFlow[m] + m_modExit[m];
    if (T < 1e-16)
      return 0.0;
    double indexLength = 0.0;
    for (int k = m_childStart[m]; k < m_childStart[m + 1]; ++k)
      indexLength -= plogp(m_leafFlow[m_childList[k]] / T);
    indexLength -= plogp(m_modExit[m] / T);
    return indexLength * T;
  }
  // Module of modules: exit to coarser level or enter one of the children.
  if (m_modFlow[m] < 1e-16)
    return 0.0;
  double sumEnter = 0.0;
  double sumEnterLogEnter = 0.0;
  for (int k = m_childStart[m]; k < m_childStart[m + 1]; ++k) {
    const double en = m_modEnter[m_childList[k]];
    sumEnter += en;
    sumEnterLogEnter += plogp(en);
  }
  const double totalCodewordUse = m_modExit[m] + sumEnter;
  return plogp(totalCodewordUse) - sumEnterLogEnter - plogp(m_modExit[m]);
}

double ColumnarMapEquation::hierarchicalCodelength() const
{
  double total = 0.0;
  const int nMod = static_cast<int>(m_modFlow.size());
  for (int m = 0; m < nMod; ++m)
    total += moduleTerm(m);
  return total;
}

unsigned int ColumnarMapEquation::numLevels() const
{
  int maxDepth = 0;
  for (int d : m_modDepth)
    maxDepth = std::max(maxDepth, d);
  // Module depths run 0 (root) .. maxDepth (deepest leaf module); the leaves sit
  // one level below, so the tree depth (matching maxTreeDepth) is maxDepth + 1.
  return static_cast<unsigned int>(maxDepth + 1);
}

} // namespace infomap
