/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef COLUMNARSTACK_H_
#define COLUMNARSTACK_H_

#include "ColumnarLevel.h"
#include "ColumnarBoundaryFlow.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace infomap {

/**
 * The stacked hierarchy of the columnar search: a rectangular multi-level
 * partition of a leaf network (every leaf at the same depth).
 *
 * Level 0 is the leaf network and levels 1..topLevel() are module levels, each
 * a ColumnarLevel built by ColumnarBoundaryFlow::aggregate. assign(k) maps a
 * level-k unit to its level-(k+1) module. The leaf network is bound, not
 * owned: it is the largest object in a run, so level 0 is read through the
 * binding and never copied into the stack or into any copy of it.
 *
 * The stack is a value. Copying it copies the module levels and assignments
 * (never the leaf network), which is how a search keeps a whole candidate
 * partition. Every write goes through the edits below, which keep the levels
 * the aggregates of the levels under them. A module level's aggregates depend
 * only on the leaf set of each of its modules, so an edit that regroups units
 * within their parents leaves the levels above it valid; an edit that moves
 * units across parents re-aggregates them with reaggregateFrom.
 *
 * Checkpoints make any sequence of edits tentative at no copying cost: while
 * one is open, the first write to a slot moves the slot's old value into a
 * journal, and rollback moves it back. Checkpoints nest, so a tentative step
 * can contain gated steps of its own. Edits that change the number of levels
 * save the whole stack once instead (module levels only).
 */
class ColumnarStack {
public:
  using Level = ColumnarLevel;

  ColumnarStack() = default;
  ColumnarStack(const ColumnarStack& other);
  ColumnarStack& operator=(const ColumnarStack& other);
  ColumnarStack(ColumnarStack&& other) noexcept;
  ColumnarStack& operator=(ColumnarStack&& other) noexcept;
  ~ColumnarStack() = default;

  // The leaf network (level 0) and the flow model every level is aggregated
  // with. Both must outlive the stack and every copy of it.
  void bind(const Level& leaf, const ColumnarBoundaryFlow& flow)
  {
    m_leaf = &leaf;
    m_flow = &flow;
    m_version = m_leafVersion = nextVersion();
  }

  // Names the stack's content: every edit takes a fresh id, a rollback restores
  // the id its checkpoint saw, and a copy keeps its source's. Ids are unique
  // across stacks, so equal ids mean equal content and a scorer can cache by it.
  std::uint64_t version() const { return m_version; }
  // The same for the leaf partition alone (the leaf -> level-1 assignment, and so
  // level 1): it changes only with assign(0) or the stack's shape.
  std::uint64_t leafPartitionVersion() const { return m_leafVersion; }

  // --- Shape ---------------------------------------------------------------
  // Levels including the leaves; 0 until a partition is built.
  int numLevels() const { return static_cast<int>(m_levels.size()); }
  // Index of the top module level (numLevels() - 1).
  int topLevel() const { return numLevels() - 1; }
  // Whether at least one module level exists.
  bool hasModules() const { return m_levels.size() >= 2; }
  const Level& level(int k) const { return k == 0 ? *m_leaf : m_levels[static_cast<std::size_t>(k)]; }
  int size(int k) const { return level(k).n; }
  // Level-k unit -> level-(k+1) module, for k = 0 .. topLevel() - 1.
  const std::vector<int>& assign(int k) const { return m_assign[static_cast<std::size_t>(k)]; }
  int parent(int k, int unit) const { return m_assign[static_cast<std::size_t>(k)][static_cast<std::size_t>(unit)]; }
  int leafModule(int leaf) const { return m_assign[0][static_cast<std::size_t>(leaf)]; }
  int numTopModules() const { return hasModules() ? m_levels.back().n : 0; }

  // --- Building a stack (these change the number of levels) ------------------
  // No levels at all.
  void clear();
  // Leaves and one module level: leaf -> module, K modules.
  void setFlat(std::vector<int> leafModule, int K);
  // From one module path per leaf, coarsest first (path[0] = top module); ids
  // need not be compact. Every path must have the same length >= 1: returns
  // false, leaving the stack untouched, for a ragged tree or a count mismatch.
  bool setFromLeafPaths(const std::vector<std::vector<int>>& leafPaths);
  // A new top level over the current top: top unit -> new module, K modules.
  void pushLevel(std::vector<int> topAssign, int K);
  // A new module level between level k and level k+1 (0 <= k < topLevel()):
  // assign is level k -> new module (K of them), parents is new module ->
  // level-(k+1) module. The levels above keep their aggregates.
  void insertLevel(int k, std::vector<int> assign, int K, std::vector<int> parents);
  // Remove module level k (1 <= k <= topLevel(), with at least two module
  // levels): its children join its parent, or become top modules when k is the
  // top. The levels above keep their aggregates.
  void removeLevel(int k);

  // --- Editing a stack (the number of levels stays) --------------------------
  // Regroup level k into K modules at level k+1 (re-aggregated). When k+1 is
  // not the top, assign(k+1) must stay valid: K equals the old size, or the
  // caller passes the new modules' parents with the overload below.
  void setModules(int k, std::vector<int> assign, int K);
  // Same, with parents: new level-(k+1) module -> level-(k+2) module. The levels
  // above keep their aggregates, so every new module must stay inside the
  // grandparent its units had; otherwise call reaggregateFrom(k + 1).
  void setModules(int k, std::vector<int> assign, int K, std::vector<int> parents);
  // Replace assign(k) without re-aggregating anything; module counts unchanged.
  void setParents(int k, std::vector<int> parents);
  // Re-aggregate levels k+1 .. top from their assignments. A module left without
  // children is dropped and the rest keep their order, so a level's size stays its
  // module count (#1068). Returns the number of modules dropped.
  int reaggregateFrom(int k);

  // --- Checkpoints -----------------------------------------------------------
  class Checkpoint {
  public:
    Checkpoint() = default;

  private:
    friend class ColumnarStack;
    std::size_t m_mark = 0;
    std::size_t m_depth = 0;
    std::uint64_t m_version = 0;
    std::uint64_t m_leafVersion = 0;
  };
  // Open a checkpoint. Close it with commit or rollback, innermost first.
  Checkpoint checkpoint();
  // Keep every edit since the checkpoint.
  void commit(const Checkpoint& checkpoint);
  // Undo every edit since the checkpoint.
  void rollback(const Checkpoint& checkpoint);
  bool hasOpenCheckpoint() const { return !m_journal.marks.empty(); }

private:
  // One saved value: a level slot, an assignment slot, or the whole stack.
  struct Entry {
    enum class Kind : std::uint8_t { level,
                                     assign,
                                     whole };
    Kind kind = Kind::level;
    int slot = 0;
    Level level;
    std::vector<int> assign;
    std::vector<Level> levels;
    std::vector<std::vector<int>> assigns;
  };
  // A copy or move of a stack starts with a closed journal: checkpoints belong
  // to the object they were opened on.
  struct Journal {
    std::vector<Entry> entries;
    std::vector<std::size_t> marks; // entries.size() at each open checkpoint
    // Index + 1 of the entry that last saved a slot (0 = none); a slot is saved
    // again once a checkpoint opens after that entry.
    std::vector<std::size_t> levelSavedAt, assignSavedAt;
    std::size_t wholeSavedAt = 0;
  };

  bool saving() const { return !m_journal.marks.empty(); }
  bool savedSince(std::size_t savedAt) const { return savedAt > m_journal.marks.back(); }
  void saveLevel(int k);
  void saveAssign(int k);
  // Before an edit that changes the number of levels.
  void saveWhole();
  void resetSavedFlags();
  static std::uint64_t nextVersion();

  const Level* m_leaf = nullptr;
  const ColumnarBoundaryFlow* m_flow = nullptr;
  // m_levels[0] is an empty placeholder: level 0 is read from m_leaf.
  std::vector<Level> m_levels;
  std::vector<std::vector<int>> m_assign;
  Journal m_journal;
  std::uint64_t m_version = 0;
  std::uint64_t m_leafVersion = 0;
};

} // namespace infomap

#endif // COLUMNARSTACK_H_
