/*******************************************************************************
  Infomap software package for multi-level network clustering

  Copyright (c) 2013, 2014 Daniel Edler, Martin Rosvall

  For more information, see <http://www.mapequation.org>

  This file is part of Infomap software package.

  Infomap software package is free software: you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Infomap software package is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with Infomap software package.  If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/

#ifndef _INFOMAPITERATORS_H_
#define _INFOMAPITERATORS_H_

#include "treeIterators.h"
#include <deque>
#include <limits>

namespace infomap {

/**
 * Child iterator.
 */
template <typename NodePointerType> // pointer or const pointer
class InfomapChildIterator {
  using iterator_category = std::bidirectional_iterator_tag;
  using value_type = typename iterator_traits<NodePointerType>::value_type;
  using difference_type = typename iterator_traits<NodePointerType>::difference_type;
  using reference = typename iterator_traits<NodePointerType>::reference;
  using pointer = typename iterator_traits<NodePointerType>::pointer;

protected:
  NodePointerType m_root = nullptr;
  NodePointerType m_current = nullptr;

public:
  InfomapChildIterator() = default;

  explicit InfomapChildIterator(const NodePointerType& nodePointer)
      : m_root(nodePointer), m_current(nodePointer) { init(); }

  InfomapChildIterator(const InfomapChildIterator& other)
      : m_root(other.m_root), m_current(other.m_current) { }

  InfomapChildIterator& operator=(const InfomapChildIterator& other)
  {
    m_root = other.m_root;
    m_current = other.m_current;
    return *this;
  }

  void init()
  {
    if (m_root != nullptr) {
      NodePointerType infomapRoot = m_root->getInfomapRoot();
      if (infomapRoot != nullptr) {
        m_root = infomapRoot;
      }
    }
    m_current = m_root == nullptr ? nullptr : m_root->firstChild;
  }

  pointer current() const { return m_current; }

  reference operator*() const { return *m_current; }

  pointer operator->() const { return m_current; }

  bool operator==(const InfomapChildIterator& rhs) const { return m_current == rhs.m_current; }

  bool operator!=(const InfomapChildIterator& rhs) const { return m_current != rhs.m_current; }

  bool isEnd() const { return m_current == nullptr; }

  InfomapChildIterator& operator++()
  {
    m_current = m_current->next;
    if (m_current != nullptr && m_current->parent != m_root) {
      m_current = nullptr;
    }
    return *this;
  }

  InfomapChildIterator operator++(int)
  {
    InfomapChildIterator copy(*this);
    ++(*this);
    return copy;
  }

  InfomapChildIterator& operator--()
  {
    m_current = m_current->previous;
    if (m_current != nullptr && m_current->parent != m_root) {
      m_current = nullptr;
    }
    return *this;
  }

  InfomapChildIterator operator--(int)
  {
    InfomapChildIterator copy(*this);
    --(*this);
    return copy;
  }
};

/**
 * Pre processing depth first iterator that explores sub-Infomap instances
 * Note: This iterator presupposes that the next pointer of a node can't reach a node with a different parent.
 */
template <typename NodePointerType>
class InfomapClusterIterator : public DepthFirstIteratorBase<NodePointerType> {
protected:
  using Base = DepthFirstIteratorBase<NodePointerType>;

  unsigned int m_moduleIndex = 0;
  int m_moduleIndexLevel = -1;

  using Base::m_current;
  using Base::m_depth;

public:
  InfomapClusterIterator() : Base() { }

  explicit InfomapClusterIterator(const NodePointerType& nodePointer, int moduleIndexLevel = -1)
      : Base(nodePointer), m_moduleIndexLevel(moduleIndexLevel)
  {
    init();
  }

  InfomapClusterIterator(const InfomapClusterIterator& other)
      : Base(other), m_moduleIndex(other.m_moduleIndex), m_moduleIndexLevel(other.m_moduleIndexLevel) { }

  virtual void init() { moveToInfomapRootIfExist(); }

  void moveToInfomapRootIfExist()
  {
    if (m_current != nullptr) {
      NodePointerType infomapRoot = m_current->getInfomapRoot();
      if (infomapRoot != nullptr) {
        m_current = infomapRoot;
      }
    }
  }

  InfomapClusterIterator& operator=(const InfomapClusterIterator& other)
  {
    Base::operator=(other);
    m_moduleIndex = other.m_moduleIndex;
    m_moduleIndexLevel = other.m_moduleIndexLevel;
    return *this;
  }

  InfomapClusterIterator& operator++()
  {
    NodePointerType curr = Base::m_current;

    if (curr->firstChild != nullptr) {
      curr = curr->firstChild;
      ++m_depth;
    } else {
    // Current node is a leaf
    // Presupposes that the next pointer can't reach out from the current parent.
    tryNext:
      while (curr->next == nullptr) {
        if (curr->parent != nullptr) {
          curr = curr->parent;
          --m_depth;
          if (curr == Base::m_root) // Check if back to beginning
          {
            m_current = nullptr;
            return *this;
          }
          if (m_moduleIndexLevel < 0) {
            if (curr->isLeafModule()) // TODO: Generalize to -2 for second level to bottom
              ++m_moduleIndex;
          } else if (static_cast<unsigned int>(m_moduleIndexLevel) == m_depth)
            ++m_moduleIndex;
        } else {
          NodePointerType infomapOwner = curr->owner;
          if (infomapOwner != nullptr) {
            curr = infomapOwner;
            if (curr == Base::m_root) // Check if back to beginning
            {
              m_current = nullptr;
              return *this;
            }
            goto tryNext;
          } else // null also if no children in first place
          {
            m_current = nullptr;
            return *this;
          }
        }
      }
      curr = curr->next;
    }
    m_current = curr;
    moveToInfomapRootIfExist();
    return *this;
  }

  InfomapClusterIterator operator++(int)
  {
    InfomapClusterIterator copy(*this);
    ++(*this);
    return copy;
  }

  InfomapClusterIterator next()
  {
    InfomapClusterIterator copy(*this);
    return ++copy;
  }

  InfomapClusterIterator& stepForward()
  {
    ++(*this);
    return *this;
  }

  unsigned int moduleIndex() const
  {
    return m_moduleIndex;
  }
};

/**
 * Pre processing depth first iterator that explores sub-Infomap instances
 * Note: This iterator presupposes that the next pointer of a node can't reach a node with a different parent.
 */
template <typename NodePointerType>
class InfomapDepthFirstIterator : public DepthFirstIteratorBase<NodePointerType> {
protected:
  typedef DepthFirstIteratorBase<NodePointerType> Base;

  std::deque<unsigned int> m_path; // The child index path to current node
  unsigned int m_moduleIndex = 0;
  int m_moduleIndexLevel = -1;

  using Base::m_current;
  using Base::m_depth;

public:
  InfomapDepthFirstIterator() : Base() { }

  explicit InfomapDepthFirstIterator(const NodePointerType& nodePointer, int moduleIndexLevel = -1)
      : Base(nodePointer),
        m_moduleIndexLevel(moduleIndexLevel)
  {
    init();
  }

  InfomapDepthFirstIterator(const InfomapDepthFirstIterator& other)
      : Base(other),
        m_path(other.m_path),
        m_moduleIndex(other.m_moduleIndex),
        m_moduleIndexLevel(other.m_moduleIndexLevel)
  {
  }

  InfomapDepthFirstIterator& operator=(const InfomapDepthFirstIterator& other)
  {
    Base::operator=(other);
    m_path = other.m_path;
    m_moduleIndex = other.m_moduleIndex;
    m_moduleIndexLevel = other.m_moduleIndexLevel;
    return *this;
  }

  virtual void init()
  {
    moveToInfomapRootIfExist();
  }

  void moveToInfomapRootIfExist()
  {
    if (m_current != nullptr) {
      NodePointerType infomapRoot = m_current->getInfomapRoot();
      if (infomapRoot != nullptr) {
        m_current = infomapRoot;
      }
    }
  }

  InfomapDepthFirstIterator& operator++()
  {
    NodePointerType curr = Base::m_current;

    if (curr->firstChild != nullptr) {
      curr = curr->firstChild;
      ++m_depth;
      m_path.push_back(0);
    } else {
    // Current node is a leaf
    // Presupposes that the next pointer can't reach out from the current parent.
    tryNext:
      while (curr->next == nullptr) {
        if (curr->parent != nullptr) {
          curr = curr->parent;
          --m_depth;
          m_path.pop_back();
          if (curr == Base::m_root) // Check if back to beginning
          {
            m_current = nullptr;
            return *this;
          }
          if (m_moduleIndexLevel < 0) {
            if (curr->isLeafModule()) // TODO: Generalize to -2 for second level to bottom
              ++m_moduleIndex;
          } else if (static_cast<unsigned int>(m_moduleIndexLevel) == m_depth)
            ++m_moduleIndex;
        } else {
          NodePointerType infomapOwner = curr->owner;
          if (infomapOwner != nullptr) {
            curr = infomapOwner;
            if (curr == Base::m_root) // Check if back to beginning
            {
              m_current = nullptr;
              return *this;
            }
            goto tryNext;
          } else // null also if no children in first place
          {
            m_current = nullptr;
            return *this;
          }
        }
      }
      curr = curr->next;
      ++m_path.back();
    }
    m_current = curr;
    moveToInfomapRootIfExist();
    return *this;
  }

  InfomapDepthFirstIterator operator++(int)
  {
    InfomapDepthFirstIterator copy(*this);
    ++(*this);
    return copy;
  }

  InfomapDepthFirstIterator next()
  {
    InfomapDepthFirstIterator copy(*this);
    return ++copy;
  }

  InfomapDepthFirstIterator& stepForward()
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
};

} // namespace infomap

#endif /* _INFOMAPITERATORS_H_ */