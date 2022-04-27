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

#include "InfoNode.h"
#include "InfomapCore.h"
#include <algorithm>

namespace infomap {

InfoNode::~InfoNode()
{
  if (m_infomap != nullptr) {
    delete m_infomap;
  }

  deleteChildren();

  if (next != nullptr)
    next->previous = previous;
  if (previous != nullptr)
    previous->next = next;
  if (parent != nullptr) {
    if (parent->firstChild == this)
      parent->firstChild = next;
    if (parent->lastChild == this)
      parent->lastChild = previous;
  }

  // Delete outgoing edges.
  // TODO: Renders ingoing edges invalid. Assume or assert that all nodes on the same level are deleted?
  for (edge_iterator outEdgeIt(begin_outEdge());
       outEdgeIt != end_outEdge();
       ++outEdgeIt) {
    delete *outEdgeIt;
  }
}

InfomapBase& InfoNode::setInfomap(InfomapBase* infomap)
{
  disposeInfomap();
  m_infomap = infomap;
  if (infomap == nullptr)
    throw std::logic_error("InfoNode::setInfomap(...) called with null infomap");
  return *m_infomap;
}

InfomapBase& InfoNode::getInfomap()
{
  if (m_infomap == nullptr)
    throw std::logic_error("InfoNode::getInfomap() called but infomap is null");
  return *m_infomap;
}

InfoNode* InfoNode::getInfomapRoot()
{
  return m_infomap != nullptr ? &m_infomap->root() : nullptr;
}

InfoNode const* InfoNode::getInfomapRoot() const
{
  return m_infomap != nullptr ? &m_infomap->root() : nullptr;
}

bool InfoNode::disposeInfomap()
{
  if (m_infomap != nullptr) {
    delete m_infomap;
    m_infomap = nullptr;
    return true;
  }
  return false;
}

bool InfoNode::isLeaf() const
{
  return firstChild == nullptr;
}

bool InfoNode::isLeafModule() const
{
  // TODO: Safe to assume all children are leaves if first child is leaf?
  return m_infomap == nullptr && firstChild != nullptr && firstChild->firstChild == nullptr;
}

bool InfoNode::isRoot() const
{
  return parent == nullptr;
}

unsigned int InfoNode::depth() const
{
  unsigned int depth = 0;
  InfoNode* n = parent;
  while (n != nullptr) {
    ++depth;
    n = n->parent;
  }
  return depth;
}

unsigned int InfoNode::firstDepthBelow() const
{
  unsigned int depthBelow = 0;
  InfoNode* child = firstChild;
  while (child != nullptr) {
    ++depthBelow;
    child = child->firstChild;
  }
  return depthBelow;
}

unsigned int InfoNode::childIndex() const
{
  unsigned int childIndex = 0;
  const InfoNode* n(this);
  while (n->previous) {
    n = n->previous;
    ++childIndex;
  }
  return childIndex;
}

std::vector<unsigned int> InfoNode::calculatePath() const
{
  const InfoNode* current = this;
  std::vector<unsigned int> path;
  while (current->parent != nullptr) {
    path.push_back(current->childIndex() + 1);
    current = current->parent;
    if (current->owner != nullptr) {
      current = current->owner;
    }
  }
  std::reverse(path.begin(), path.end());
  return path;
}

unsigned int InfoNode::childDegree() const
{
  return m_childDegree;
}

unsigned int InfoNode::infomapChildDegree() const
{
  return m_infomap == nullptr ? childDegree() : m_infomap->root().childDegree();
}

void InfoNode::addChild(InfoNode* child)
{
  if (firstChild == nullptr) {
    child->previous = nullptr;
    firstChild = child;
  } else {
    child->previous = lastChild;
    lastChild->next = child;
  }
  lastChild = child;
  child->next = nullptr;
  child->parent = this;
  ++m_childDegree;
}

void InfoNode::releaseChildren()
{
  firstChild = nullptr;
  lastChild = nullptr;
  m_childDegree = 0;
}

InfoNode& InfoNode::replaceChildrenWithOneNode()
{
  if (childDegree() == 1)
    return *firstChild;
  if (firstChild == nullptr)
    throw std::logic_error("replaceChildrenWithOneNode called on a node without any children.");
  if (firstChild->firstChild == nullptr)
    throw std::logic_error("replaceChildrenWithOneNode called on a node without any grandchildren.");
  InfoNode* middleNode = new InfoNode();
  InfoNode::child_iterator nodeIt = begin_child();
  unsigned int numOriginalChildrenLeft = m_childDegree;
  auto d0 = m_childDegree;
  do {
    InfoNode* n = nodeIt.current();
    ++nodeIt;
    middleNode->addChild(n);
  } while (--numOriginalChildrenLeft != 0);

  releaseChildren();
  addChild(middleNode);
  auto d1 = middleNode->replaceChildrenWithGrandChildren();
  if (d1 != d0)
    throw std::logic_error("replaceChildrenWithOneNode replaced different number of children as having before");
  return *middleNode;
}

unsigned int InfoNode::replaceChildrenWithGrandChildren()
{
  if (firstChild == nullptr)
    return 0;
  InfoNode::child_iterator nodeIt = begin_child();
  unsigned int numOriginalChildrenLeft = m_childDegree;
  unsigned int numChildrenReplaced = 0;
  do {
    InfoNode* n = nodeIt.current();
    ++nodeIt;
    numChildrenReplaced += n->replaceWithChildren();
  } while (--numOriginalChildrenLeft != 0);
  return numChildrenReplaced;
}

unsigned int InfoNode::replaceWithChildren()
{
  if (isLeaf() || isRoot())
    return 0;

  // Re-parent children
  unsigned int deltaChildDegree = 0;
  InfoNode* child = firstChild;
  do {
    child->parent = parent;
    child = child->next;
    ++deltaChildDegree;
  } while (child != nullptr);
  parent->m_childDegree += deltaChildDegree - 1; // -1 as this node is deleted

  firstChild->previous = previous;
  lastChild->next = next;

  if (parent->firstChild == this) {
    parent->firstChild = firstChild;
  } else {
    previous->next = firstChild;
  }

  if (parent->lastChild == this) {
    parent->lastChild = lastChild;
  } else {
    next->previous = lastChild;
  }

  // Release connected nodes before delete, otherwise children are deleted and neighbours are reconnected.
  firstChild = nullptr;
  lastChild = nullptr;
  next = nullptr;
  previous = nullptr;
  parent = nullptr;
  delete this;
  return 1;
}

void InfoNode::replaceChildrenWithGrandChildrenDebug()
{
  if (firstChild == nullptr)
    return;
  InfoNode::child_iterator nodeIt = begin_child();
  unsigned int numOriginalChildrenLeft = m_childDegree;
  do {
    InfoNode* n = nodeIt.current();
    ++nodeIt;
    n->replaceWithChildrenDebug();
  } while (--numOriginalChildrenLeft != 0);
}

void InfoNode::replaceWithChildrenDebug()
{
  if (isLeaf() || isRoot())
    return;

  // Re-parent children
  unsigned int deltaChildDegree = 0;
  InfoNode* child = firstChild;
  do {
    child->parent = parent;
    child = child->next;
    ++deltaChildDegree;
  } while (child != 0);
  parent->m_childDegree += deltaChildDegree - 1; // -1 as this node is deleted

  if (parent->firstChild == this) {
    parent->firstChild = firstChild;
  } else {
    previous->next = firstChild;
    firstChild->previous = previous;
  }

  if (parent->lastChild == this) {
    parent->lastChild = lastChild;
  } else {
    next->previous = lastChild;
    lastChild->next = next;
  }

  // Release connected nodes before delete, otherwise children are deleted and neighbours are reconnected.
  firstChild = nullptr;
  lastChild = nullptr;
  next = nullptr;
  previous = nullptr;
  parent = nullptr;

  delete this;
}

void InfoNode::remove(bool removeChildren)
{
  firstChild = removeChildren ? nullptr : firstChild;
  delete this;
}

void InfoNode::deleteChildren()
{
  if (firstChild == nullptr)
    return;

  child_iterator nodeIt = begin_child();
  do {
    InfoNode* n = nodeIt.current();
    ++nodeIt;
    delete n;
  } while (nodeIt.current() != nullptr);

  firstChild = nullptr;
  lastChild = nullptr;
  m_childDegree = 0;
}

void InfoNode::calcChildDegree()
{
  m_childrenChanged = false;
  if (firstChild == nullptr)
    m_childDegree = 0;
  else if (firstChild == lastChild)
    m_childDegree = 1;
  else {
    m_childDegree = 0;
    for (auto& child : children()) {
      (void)child;
      ++m_childDegree;
    }
  }
}

void InfoNode::setChildDegree(unsigned int value)
{
  m_childDegree = value;
  m_childrenChanged = false;
}

void InfoNode::setNumLeafNodes(unsigned int value)
{
  m_numLeafMembers = value;
}

void InfoNode::initClean()
{
  releaseChildren();
  previous = next = parent = nullptr;

  physicalNodes.clear();
}

void InfoNode::sortChildrenOnFlow(bool recurse)
{
  if (childDegree() == 0)
    return;
  std::vector<InfoNode*> nodes(childDegree());
  double lastFlow = 1.0;
  bool isSorted = true;
  unsigned int i = 0;
  for (InfoNode& child : children()) {
    if (child.data.flow > lastFlow) {
      isSorted = false;
    }
    nodes[i] = &child;
    lastFlow = child.data.flow;
    ++i;
  }
  if (!isSorted) {
    std::sort(nodes.begin(), nodes.end(), [](const InfoNode* a, const InfoNode* b) {
      return b->data.flow < a->data.flow;
    });
    releaseChildren();
    for (auto node : nodes) {
      addChild(node);
    }
  }
  if (recurse) {
    for (InfoNode& child : children()) {
      auto newRoot = child.getInfomapRoot();
      InfoNode& node = newRoot ? *newRoot : child;
      node.sortChildrenOnFlow(recurse);
    }
  }
}

unsigned int InfoNode::collapseChildren()
{
  std::swap(collapsedFirstChild, firstChild);
  std::swap(collapsedLastChild, lastChild);
  unsigned int numCollapsedChildren = childDegree();
  releaseChildren();
  return numCollapsedChildren;
}

unsigned int InfoNode::expandChildren()
{
  bool haveCollapsedChildren = collapsedFirstChild != nullptr;
  if (haveCollapsedChildren) {
    if (firstChild != nullptr || lastChild != nullptr)
      throw std::logic_error("Expand collapsed children called on a node that already has children.");
    std::swap(collapsedFirstChild, firstChild);
    std::swap(collapsedLastChild, lastChild);
    calcChildDegree();
    return childDegree();
  }
  return 0;
}

} // namespace infomap
