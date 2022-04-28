/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Eriksson, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_AGPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef INFOMAP_ITERATOR_H_
#define INFOMAP_ITERATOR_H_

#include "../InfoNode.h"
#include <deque>
#include <map>
#include <cmath>

namespace infomap {

/**
 * Pre processing depth first iterator that explores sub-Infomap instances
 * Note:
 * This iterator presupposes that the next pointer of a node can't reach a node with a different parent.
 */
struct InfomapIterator {
protected:
  InfoNode* m_root = nullptr;
  InfoNode* m_current = nullptr;
  int m_moduleIndexLevel = -1;
  unsigned int m_moduleIndex = 0;
  std::deque<unsigned int> m_path; // The tree path to current node (indexing starting from one!)
  unsigned int m_depth = 0;

public:
  InfomapIterator() = default;

  InfomapIterator(InfoNode* nodePointer, int moduleIndexLevel = -1)
      : m_root(nodePointer),
        m_current(nodePointer),
        m_moduleIndexLevel(moduleIndexLevel)
  {
  }

  InfomapIterator(const InfomapIterator& other)
      : m_root(other.m_root),
        m_current(other.m_current),
        m_moduleIndexLevel(other.m_moduleIndexLevel),
        m_moduleIndex(other.m_moduleIndex),
        m_path(other.m_path),
        m_depth(other.m_depth)
  {
  }

  virtual ~InfomapIterator() = default;

  virtual InfomapIterator& operator=(const InfomapIterator& other)
  {
    m_root = other.m_root;
    m_current = other.m_current;
    m_moduleIndexLevel = other.m_moduleIndexLevel;
    m_moduleIndex = other.m_moduleIndex;
    m_path = other.m_path;
    m_depth = other.m_depth;
    return *this;
  }

  InfoNode* current()
  {
    return m_current;
  }

  const InfoNode* current() const
  {
    return m_current;
  }

  InfoNode& operator*()
  {
    return *m_current;
  }

  const InfoNode& operator*() const
  {
    return *m_current;
  }

  InfoNode* operator->()
  {
    return m_current;
  }

  const InfoNode* operator->() const
  {
    return m_current;
  }

  bool operator==(const InfomapIterator& other) const
  {
    return m_current == other.m_current;
  }

  bool operator!=(const InfomapIterator& other) const
  {
    return m_current != other.m_current;
  }

  virtual InfomapIterator& operator++();

  virtual InfomapIterator operator++(int)
  {
    InfomapIterator copy(*this);
    ++(*this);
    return copy;
  }

  virtual InfomapIterator& stepForward()
  {
    ++(*this);
    return *this;
  }

  const std::deque<unsigned int>& path() const
  {
    return m_path;
  }

  unsigned int moduleIndex() const
  {
    return m_moduleIndex;
  }

  unsigned int moduleId() const
  {
    return m_moduleIndex + 1;
  }

  unsigned int childIndex() const
  {
    return m_path.empty() ? 0 : m_path.back() - 1;
  }

  unsigned int depth() const
  {
    return m_depth;
  }

  double modularCentrality() const
  {
    if (m_current->parent == nullptr) {
      // The root node has no modular centrality
      return 0.0;
    }

    const auto p_m = m_current->parent->data.flow;
    const auto p_u = m_current->data.flow;
    const auto p_diff = p_m - p_u;

    if (p_diff > 0.0) {
      return -p_diff * std::log2(p_diff / p_m);
    }

    return 0.0;
  }

  bool isEnd() const
  {
    return m_current == nullptr;
  }
};

struct InfomapModuleIterator : public InfomapIterator {
public:
  InfomapModuleIterator() : InfomapIterator() { }

  InfomapModuleIterator(InfoNode* nodePointer, int moduleIndexLevel = -1)
      : InfomapIterator(nodePointer, moduleIndexLevel) { }

  InfomapModuleIterator(const InfomapModuleIterator& other)
      : InfomapIterator(other) { }

  InfomapModuleIterator& operator=(const InfomapModuleIterator& other)
  {
    InfomapIterator::operator=(other);
    return *this;
  }

  virtual InfomapIterator& operator++();

  virtual InfomapIterator operator++(int)
  {
    InfomapModuleIterator copy(*this);
    ++(*this);
    return copy;
  }

  using InfomapIterator::childIndex;
  using InfomapIterator::current;
  using InfomapIterator::depth;
  using InfomapIterator::modularCentrality;
  using InfomapIterator::path;
};

struct InfomapLeafModuleIterator : public InfomapIterator {
public:
  InfomapLeafModuleIterator() : InfomapIterator() { }

  InfomapLeafModuleIterator(InfoNode* nodePointer, int moduleIndexLevel = -1)
      : InfomapIterator(nodePointer, moduleIndexLevel)
  {
    init();
  }

  InfomapLeafModuleIterator(const InfomapLeafModuleIterator& other)
      : InfomapIterator(other)
  {
    init();
  }

  InfomapLeafModuleIterator& operator=(const InfomapLeafModuleIterator& other)
  {
    InfomapIterator::operator=(other);
    return *this;
  }

  /**
   * Iterate to first leaf module
   */
  void init();

  virtual InfomapIterator& operator++();

  virtual InfomapIterator operator++(int)
  {
    InfomapLeafModuleIterator copy(*this);
    ++(*this);
    return copy;
  }

  using InfomapIterator::childIndex;
  using InfomapIterator::current;
  using InfomapIterator::depth;
  using InfomapIterator::modularCentrality;
  using InfomapIterator::path;
};

struct InfomapLeafIterator : public InfomapIterator {
public:
  InfomapLeafIterator() : InfomapIterator() { }

  InfomapLeafIterator(InfoNode* nodePointer, int moduleIndexLevel = -1)
      : InfomapIterator(nodePointer, moduleIndexLevel)
  {
    init();
  }

  InfomapLeafIterator(const InfomapLeafIterator& other)
      : InfomapIterator(other)
  {
    init();
  }

  InfomapLeafIterator& operator=(const InfomapLeafIterator& other)
  {
    InfomapIterator::operator=(other);
    return *this;
  }

  /**
   * Iterate to first leaf node
   */
  void init();

  virtual InfomapIterator& operator++();

  virtual InfomapIterator operator++(int)
  {
    InfomapLeafIterator copy(*this);
    ++(*this);
    return copy;
  }

  using InfomapIterator::childIndex;
  using InfomapIterator::current;
  using InfomapIterator::depth;
  using InfomapIterator::modularCentrality;
  using InfomapIterator::path;
};

/**
 * Iterate over the whole tree, collecting physical nodes within same leaf modules
 * Note: The physical nodes are created when entering the parent module and removed
 * when leaving the module. The tree will not be modified.
 */
struct InfomapIteratorPhysical : public InfomapIterator {
protected:
  std::map<unsigned int, InfoNode> m_physNodes;
  std::map<unsigned int, InfoNode>::iterator m_physIter;
  InfomapIterator m_oldIter;

public:
  InfomapIteratorPhysical() : InfomapIterator() { }

  InfomapIteratorPhysical(InfoNode* nodePointer, int moduleIndexLevel = -1)
      : InfomapIterator(nodePointer, moduleIndexLevel) { }

  InfomapIteratorPhysical(const InfomapIteratorPhysical& other)
      : InfomapIterator(other),
        m_physNodes(other.m_physNodes),
        m_physIter(other.m_physIter),
        m_oldIter(other.m_oldIter) { }

  InfomapIteratorPhysical(const InfomapIterator& other)
      : InfomapIterator(other) { }

  InfomapIteratorPhysical& operator=(const InfomapIteratorPhysical& other)
  {
    InfomapIterator::operator=(other);
    m_physNodes = other.m_physNodes;
    m_physIter = other.m_physIter;
    m_oldIter = other.m_oldIter;
    return *this;
  }

  InfomapIteratorPhysical& operator=(const InfomapIterator& other)
  {
    InfomapIterator::operator=(other);
    return *this;
  }

  virtual InfomapIterator& operator++();

  virtual InfomapIterator operator++(int)
  {
    InfomapIteratorPhysical copy(*this);
    ++(*this);
    return copy;
  }

  using InfomapIterator::childIndex;
  using InfomapIterator::current;
  using InfomapIterator::depth;
  using InfomapIterator::modularCentrality;
  using InfomapIterator::path;
};

/**
 * Iterate over all physical leaf nodes, joining physical nodes within same leaf modules
 * Note: The physical nodes are created when entering the parent module and removed
 * when leaving the module. The tree will not be modified.
 */
struct InfomapLeafIteratorPhysical : public InfomapIteratorPhysical {
public:
  InfomapLeafIteratorPhysical()
      : InfomapIteratorPhysical() { }

  InfomapLeafIteratorPhysical(InfoNode* nodePointer, int moduleIndexLevel = -1)
      : InfomapIteratorPhysical(nodePointer, moduleIndexLevel)
  {
    init();
  }

  InfomapLeafIteratorPhysical(const InfomapLeafIteratorPhysical& other)
      : InfomapIteratorPhysical(other)
  {
    init();
  }

  InfomapLeafIteratorPhysical& operator=(const InfomapLeafIteratorPhysical& other)
  {
    InfomapIteratorPhysical::operator=(other);
    return *this;
  }

  /**
   * Iterate to first leaf node
   */
  void init();

  virtual InfomapIterator& operator++();

  virtual InfomapIterator operator++(int)
  {
    InfomapLeafIteratorPhysical copy(*this);
    ++(*this);
    return copy;
  }

  using InfomapIteratorPhysical::childIndex;
  using InfomapIteratorPhysical::current;
  using InfomapIteratorPhysical::depth;
  using InfomapIteratorPhysical::modularCentrality;
  using InfomapIteratorPhysical::path;
};

/**
 * Iterate parent by parent until it is nullptr,
 * moving up through possible sub infomap instances
 * on the way
 */
struct InfomapParentIterator {
protected:
  InfoNode* m_current = nullptr;

public:
  InfomapParentIterator() = default;

  InfomapParentIterator(InfoNode* nodePointer)
      : m_current(nodePointer) { }

  InfomapParentIterator(const InfomapParentIterator& other)
      : m_current(other.m_current) { }

  virtual ~InfomapParentIterator() = default;

  InfomapParentIterator& operator=(const InfomapParentIterator& other)
  {
    m_current = other.m_current;
    return *this;
  }

  InfoNode* current()
  {
    return m_current;
  }

  const InfoNode* current() const
  {
    return m_current;
  }

  InfoNode& operator*()
  {
    return *m_current;
  }

  const InfoNode& operator*() const
  {
    return *m_current;
  }

  InfoNode* operator->()
  {
    return m_current;
  }

  const InfoNode* operator->() const
  {
    return m_current;
  }

  bool operator==(const InfomapParentIterator& other) const
  {
    return m_current == other.m_current;
  }

  bool operator!=(const InfomapParentIterator& other) const
  {
    return m_current != other.m_current;
  }

  virtual InfomapParentIterator& operator++();

  virtual InfomapParentIterator operator++(int)
  {
    InfomapParentIterator copy(*this);
    ++(*this);
    return copy;
  }

  virtual InfomapParentIterator& stepForward()
  {
    ++(*this);
    return *this;
  }

  bool isEnd() const
  {
    return m_current == nullptr;
  }
};

} // namespace infomap

#endif // INFOMAP_ITERATOR_H_
