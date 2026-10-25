/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "ColumnarStack.h"

#include <cassert>
#include <unordered_map>
#include <utility>

namespace infomap {

ColumnarStack::ColumnarStack(const ColumnarStack& other)
    : m_leaf(other.m_leaf), m_flow(other.m_flow), m_levels(other.m_levels), m_assign(other.m_assign) {}

ColumnarStack& ColumnarStack::operator=(const ColumnarStack& other)
{
  assert(!hasOpenCheckpoint() && "replace a stack only outside its checkpoints");
  if (this == &other)
    return *this;
  m_leaf = other.m_leaf;
  m_flow = other.m_flow;
  m_levels = other.m_levels;
  m_assign = other.m_assign;
  return *this;
}

ColumnarStack::ColumnarStack(ColumnarStack&& other) noexcept
    : m_leaf(other.m_leaf), m_flow(other.m_flow), m_levels(std::move(other.m_levels)), m_assign(std::move(other.m_assign)) {}

ColumnarStack& ColumnarStack::operator=(ColumnarStack&& other) noexcept
{
  assert(!hasOpenCheckpoint() && "replace a stack only outside its checkpoints");
  m_leaf = other.m_leaf;
  m_flow = other.m_flow;
  m_levels = std::move(other.m_levels);
  m_assign = std::move(other.m_assign);
  return *this;
}

void ColumnarStack::clear()
{
  saveWhole();
  m_levels.clear();
  m_assign.clear();
  resetSavedFlags();
}

void ColumnarStack::setFlat(std::vector<int> leafModule, int K)
{
  saveWhole();
  Level modules = m_flow->aggregate(*m_leaf, leafModule, K);
  m_levels.clear();
  m_levels.emplace_back(); // level 0 is the leaf network, read through m_leaf
  m_levels.push_back(std::move(modules));
  m_assign.clear();
  m_assign.push_back(std::move(leafModule));
  resetSavedFlags();
}

bool ColumnarStack::setFromLeafPaths(const std::vector<std::vector<int>>& leafPaths)
{
  const int nLeaves = m_leaf->n;
  if (static_cast<int>(leafPaths.size()) != nLeaves)
    return false;

  // Require a rectangular tree (every leaf the same number of module levels).
  // Ragged trees don't map onto the strict-level stack; the caller falls back.
  int depth = -1;
  for (const auto& p : leafPaths) {
    const int d = static_cast<int>(p.size());
    if (d < 1)
      return false;
    if (depth == -1)
      depth = d;
    else if (d != depth)
      return false;
  }

  // Compact a module id per leaf at each stack level. Stack level j (1 = finest
  // .. depth = top) groups leaves that share the path prefix path[0 .. depth-j]:
  // the finest module needs the whole path to match, the top module only path[0].
  // Coarsest level first, so that a level-j module is the pair (its level-(j+1)
  // module, path[depth-j]) and one 64-bit key stands in for the whole prefix. Ids
  // are handed out in first-seen leaf order, exactly as the prefix map did, so the
  // stack this builds is the same one. That map, keyed by a std::vector copy of the
  // prefix per leaf per level, was about a third of the seed on web-NotreDame (325k
  // leaves x 6 levels), and the seed is paid by every -c warm start, every
  // evaluateColumnarPartition and the terminal winner passes.
  std::vector<std::vector<int>> levelId(depth + 2); // levelId[j][leaf], j = 1..depth
  std::vector<int> levelK(depth + 2, 0);
  levelId[depth + 1].assign(nLeaves, 0); // the root, above the top level
  std::unordered_map<unsigned long long, int> ids;
  ids.reserve(static_cast<std::size_t>(nLeaves));
  for (int j = depth; j >= 1; --j) {
    const int element = depth - j; // the path element that splits a level-(j+1) module into level-j ones
    ids.clear();
    levelId[j].resize(nLeaves);
    for (int i = 0; i < nLeaves; ++i) {
      const unsigned long long key = (static_cast<unsigned long long>(static_cast<unsigned int>(levelId[j + 1][i])) << 32)
          | static_cast<unsigned long long>(static_cast<unsigned int>(leafPaths[i][element]));
      const auto res = ids.emplace(key, static_cast<int>(ids.size()));
      levelId[j][i] = res.first->second;
    }
    levelK[j] = static_cast<int>(ids.size());
  }

  // Build the stack bottom-up via aggregate: level 0 = leaves, level 1 = finest
  // modules (leaf assignment levelId[1]), each coarser level from the level-k ->
  // level-(k+1) module map derived from the per-leaf level ids.
  saveWhole();
  m_levels.clear();
  m_assign.clear();
  m_levels.emplace_back(); // level 0 is the leaf network, read through m_leaf
  m_assign.push_back(levelId[1]);
  Level cur = m_flow->aggregate(*m_leaf, levelId[1], levelK[1]);
  m_levels.push_back(cur);
  for (int k = 1; k < depth; ++k) {
    // level-k module (columnar level k, my j=k) -> level-(k+1) module (j=k+1).
    std::vector<int> assign(levelK[k], -1);
    for (int i = 0; i < nLeaves; ++i)
      assign[levelId[k][i]] = levelId[k + 1][i];
    m_assign.push_back(assign);
    cur = m_flow->aggregate(cur, assign, levelK[k + 1]);
    m_levels.push_back(cur);
  }
  resetSavedFlags();
  return true;
}

void ColumnarStack::pushLevel(std::vector<int> topAssign, int K)
{
  assert(hasModules());
  saveWhole();
  Level top = m_flow->aggregate(level(topLevel()), topAssign, K);
  m_assign.push_back(std::move(topAssign));
  m_levels.push_back(std::move(top));
  resetSavedFlags();
}

void ColumnarStack::insertLevel(int k, std::vector<int> assign, int K, std::vector<int> parents)
{
  assert(k >= 0 && k < topLevel());
  assert(static_cast<int>(parents.size()) == K);
  saveWhole();
  Level mid = m_flow->aggregate(level(k), assign, K);
  m_assign[static_cast<std::size_t>(k)] = std::move(assign);
  m_assign.insert(m_assign.begin() + (k + 1), std::move(parents));
  m_levels.insert(m_levels.begin() + (k + 1), std::move(mid));
  resetSavedFlags();
}

void ColumnarStack::removeLevel(int k)
{
  assert(k >= 1 && k <= topLevel() && topLevel() >= 2);
  saveWhole();
  if (k < topLevel()) {
    // The children of a level-k module join its parent.
    std::vector<int>& below = m_assign[static_cast<std::size_t>(k - 1)];
    const std::vector<int>& through = m_assign[static_cast<std::size_t>(k)];
    for (int& m : below)
      m = through[static_cast<std::size_t>(m)];
    m_assign.erase(m_assign.begin() + k);
  } else {
    // The top goes, and with it the assignment into it.
    m_assign.pop_back();
  }
  m_levels.erase(m_levels.begin() + k);
  resetSavedFlags();
}

void ColumnarStack::setModules(int k, std::vector<int> assign, int K)
{
  assert(k >= 0 && k < topLevel());
  assert(k + 1 == topLevel() || K == size(k + 1));
  saveAssign(k);
  m_assign[static_cast<std::size_t>(k)] = std::move(assign);
  Level modules = m_flow->aggregate(level(k), m_assign[static_cast<std::size_t>(k)], K);
  saveLevel(k + 1);
  m_levels[static_cast<std::size_t>(k) + 1] = std::move(modules);
}

void ColumnarStack::setModules(int k, std::vector<int> assign, int K, std::vector<int> parents)
{
  assert(k >= 0 && k + 1 < topLevel());
  assert(static_cast<int>(parents.size()) == K);
  saveAssign(k);
  m_assign[static_cast<std::size_t>(k)] = std::move(assign);
  Level modules = m_flow->aggregate(level(k), m_assign[static_cast<std::size_t>(k)], K);
  saveLevel(k + 1);
  m_levels[static_cast<std::size_t>(k) + 1] = std::move(modules);
  saveAssign(k + 1);
  m_assign[static_cast<std::size_t>(k) + 1] = std::move(parents);
}

void ColumnarStack::setParents(int k, std::vector<int> parents)
{
  assert(k >= 0 && k < topLevel());
  saveAssign(k);
  m_assign[static_cast<std::size_t>(k)] = std::move(parents);
}

void ColumnarStack::reaggregateFrom(int k)
{
  for (int j = k; j < topLevel(); ++j) {
    Level modules = m_flow->aggregate(level(j), m_assign[static_cast<std::size_t>(j)], size(j + 1));
    saveLevel(j + 1);
    m_levels[static_cast<std::size_t>(j) + 1] = std::move(modules);
  }
}

ColumnarStack::Checkpoint ColumnarStack::checkpoint()
{
  if (m_journal.marks.empty())
    resetSavedFlags();
  m_journal.marks.push_back(m_journal.entries.size());
  Checkpoint checkpoint;
  checkpoint.m_mark = m_journal.marks.back();
  checkpoint.m_depth = m_journal.marks.size();
  return checkpoint;
}

void ColumnarStack::commit(const Checkpoint& checkpoint)
{
  assert(checkpoint.m_depth == m_journal.marks.size() && "close checkpoints innermost first");
  (void)checkpoint;
  m_journal.marks.pop_back();
  // An enclosing checkpoint still needs the saved values: its rollback restores
  // them. With none left open they are dropped.
  if (m_journal.marks.empty()) {
    m_journal.entries.clear();
    m_journal.wholeSavedAt = 0;
  }
}

void ColumnarStack::rollback(const Checkpoint& checkpoint)
{
  assert(checkpoint.m_depth == m_journal.marks.size() && "close checkpoints innermost first");
  // Newest first, so each slot ends at the value it had when it was first saved
  // and a whole-stack save restores the shape the earlier slot saves refer to.
  while (m_journal.entries.size() > checkpoint.m_mark) {
    Entry& e = m_journal.entries.back();
    switch (e.kind) {
    case Entry::Kind::level:
      m_levels[static_cast<std::size_t>(e.slot)] = std::move(e.level);
      break;
    case Entry::Kind::assign:
      m_assign[static_cast<std::size_t>(e.slot)] = std::move(e.assign);
      break;
    case Entry::Kind::whole:
      m_levels = std::move(e.levels);
      m_assign = std::move(e.assigns);
      break;
    }
    m_journal.entries.pop_back();
  }
  m_journal.marks.pop_back();
  // Saving a slot twice is harmless (rollback restores the older value last), so
  // forget which slots were saved rather than tracking what survived.
  resetSavedFlags();
  m_journal.wholeSavedAt = 0;
}

void ColumnarStack::saveLevel(int k)
{
  if (!saving() || savedSince(m_journal.wholeSavedAt))
    return;
  auto& savedAt = m_journal.levelSavedAt;
  if (savedAt.size() <= static_cast<std::size_t>(k))
    savedAt.resize(static_cast<std::size_t>(k) + 1, 0);
  if (savedSince(savedAt[static_cast<std::size_t>(k)]))
    return;
  Entry e;
  e.kind = Entry::Kind::level;
  e.slot = k;
  e.level = std::move(m_levels[static_cast<std::size_t>(k)]);
  m_journal.entries.push_back(std::move(e));
  savedAt[static_cast<std::size_t>(k)] = m_journal.entries.size();
}

void ColumnarStack::saveAssign(int k)
{
  if (!saving() || savedSince(m_journal.wholeSavedAt))
    return;
  auto& savedAt = m_journal.assignSavedAt;
  if (savedAt.size() <= static_cast<std::size_t>(k))
    savedAt.resize(static_cast<std::size_t>(k) + 1, 0);
  if (savedSince(savedAt[static_cast<std::size_t>(k)]))
    return;
  Entry e;
  e.kind = Entry::Kind::assign;
  e.slot = k;
  e.assign = std::move(m_assign[static_cast<std::size_t>(k)]);
  m_journal.entries.push_back(std::move(e));
  savedAt[static_cast<std::size_t>(k)] = m_journal.entries.size();
}

void ColumnarStack::saveWhole()
{
  if (!saving() || savedSince(m_journal.wholeSavedAt))
    return;
  // A copy of the current state: slots saved earlier under this checkpoint keep
  // their own entries, which rollback applies after this one.
  Entry e;
  e.kind = Entry::Kind::whole;
  e.levels = m_levels;
  e.assigns = m_assign;
  m_journal.entries.push_back(std::move(e));
  m_journal.wholeSavedAt = m_journal.entries.size();
}

void ColumnarStack::resetSavedFlags()
{
  // Slot indices refer to the current shape; after a change of shape, or a
  // rollback, every slot is saved afresh on its next write.
  m_journal.levelSavedAt.assign(m_levels.size(), 0);
  m_journal.assignSavedAt.assign(m_assign.size(), 0);
}

} // namespace infomap
