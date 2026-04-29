/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "InfoNode.h"
#include "InfomapBase.h"
#include <algorithm>

namespace infomap {

void InfomapBaseDeleter::operator()(InfomapBase* infomap) const noexcept
{
  delete infomap;
}

std::unique_ptr<InfoNode> InfoNode::DetachedChildChain::take(InfoNode* child) noexcept
{
  for (auto it = m_children.begin(); it != m_children.end(); ++it) {
    if (it->get() == child) {
      InfoNode::unlinkFromChain(*child, m_first, m_last);
      auto ownedChild = std::move(*it);
      m_children.erase(it);
      if (m_childDegree > 0)
        --m_childDegree;
      child->parent = nullptr;
      return ownedChild;
    }
  }
  return nullptr;
}

bool InfoNode::chainContains(InfoNode* first, const InfoNode* node) noexcept
{
  for (auto* child = first; child != nullptr; child = child->next) {
    if (child == node)
      return true;
  }
  return false;
}

void InfoNode::unlinkFromChain(InfoNode& node, InfoNode*& first, InfoNode*& last) noexcept
{
  if (node.next != nullptr)
    node.next->previous = node.previous;
  if (node.previous != nullptr)
    node.previous->next = node.next;
  if (first == &node)
    first = node.next;
  if (last == &node)
    last = node.previous;

  node.previous = nullptr;
  node.next = nullptr;
}

void InfoNode::unlinkFromParentAndSiblings(InfoNode& node) noexcept
{
  if (node.parent != nullptr) {
    unlinkFromChain(node, node.parent->firstChild, node.parent->lastChild);
  } else {
    node.previous = nullptr;
    node.next = nullptr;
  }

  // Preserve current destructor/remove semantics: unlinking updates links but
  // does not update the parent's cached child degree.
  node.parent = nullptr;
}

InfoNode::InfoNode(const InfoNode& other)
{
  copyDetachedValueStateFrom(other);
}

InfoNode& InfoNode::operator=(const InfoNode& other)
{
  if (this == &other)
    return *this;

  deleteChildren();
  m_children.clear();
  m_collapsedChildren.clear();
  m_outEdges.clear();
  m_inEdges.clear();
  m_infomap.reset();
  copyDetachedValueStateFrom(other);
  return *this;
}

InfoNode::~InfoNode() noexcept
{
  deleteChildren();
  unlinkFromParentAndSiblings(*this);

  // Incoming edge pointers on other nodes become dangling non-owning back-references.
}

void InfoNode::copyDetachedValueStateFrom(const InfoNode& other)
{
  data = other.data;
  index = other.index;
  stateId = other.stateId;
  physicalId = other.physicalId;
  layerId = other.layerId;
  metaData = other.metaData;
  owner = nullptr;
  parent = nullptr;
  previous = nullptr;
  next = nullptr;
  firstChild = nullptr;
  lastChild = nullptr;
  collapsedFirstChild = nullptr;
  collapsedLastChild = nullptr;
  codelength = other.codelength;
  dirty = other.dirty;
  physicalNodes = other.physicalNodes;
  metaCollection = other.metaCollection;
  stateNodes = other.stateNodes;
  m_childDegree = 0;
  m_childrenChanged = false;
  m_numLeafMembers = other.m_numLeafMembers;
  m_children.clear();
  m_collapsedChildren.clear();
}

InfomapBase& InfoNode::setInfomap(InfomapBase* infomap)
{
  m_infomap.reset(infomap);
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

const InfomapBase& InfoNode::getInfomap() const
{
  if (m_infomap == nullptr)
    throw std::logic_error("InfoNode::getInfomap() called but infomap is null");
  return *m_infomap;
}

InfoNode* InfoNode::getInfomapRoot() noexcept
{
  return m_infomap != nullptr ? &m_infomap->root() : nullptr;
}

InfoNode const* InfoNode::getInfomapRoot() const noexcept
{
  return m_infomap != nullptr ? &m_infomap->root() : nullptr;
}

bool InfoNode::disposeInfomap() noexcept
{
  if (m_infomap != nullptr) {
    m_infomap.reset();
    return true;
  }
  return false;
}

unsigned int InfoNode::depth() const noexcept
{
  unsigned int depth = 0;
  InfoNode* n = parent;
  while (n != nullptr) {
    ++depth;
    n = n->parent;
  }
  return depth;
}

unsigned int InfoNode::firstDepthBelow() const noexcept
{
  unsigned int depthBelow = 0;
  InfoNode* child = firstChild;
  while (child != nullptr) {
    ++depthBelow;
    child = child->firstChild;
  }
  return depthBelow;
}

unsigned int InfoNode::childIndex() const noexcept
{
  unsigned int childIndex = 0;
  const InfoNode* n(this);
  while (n->previous) {
    n = n->previous;
    ++childIndex;
  }
  return childIndex;
}

std::vector<unsigned int> InfoNode::calculatePath() const noexcept
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

unsigned int InfoNode::infomapChildDegree() const noexcept
{
  return m_infomap == nullptr ? childDegree() : m_infomap->root().childDegree();
}

void InfoNode::appendLinkedChild(InfoNode* child) noexcept
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

void InfoNode::appendOwnedChild(std::unique_ptr<InfoNode> child) noexcept
{
  auto* childPtr = child.get();
  appendLinkedChild(childPtr);
  m_children.push_back(std::move(child));
}

std::unique_ptr<InfoNode> InfoNode::takeChildOwnership(InfoNode* child) noexcept
{
  auto takeFrom = [child](std::vector<std::unique_ptr<InfoNode>>& children) {
    for (auto it = children.begin(); it != children.end(); ++it) {
      if (it->get() == child) {
        auto ownedChild = std::move(*it);
        children.erase(it);
        return ownedChild;
      }
    }
    return std::unique_ptr<InfoNode>();
  };

  if (auto ownedChild = takeFrom(m_children))
    return ownedChild;
  return takeFrom(m_collapsedChildren);
}

void InfoNode::addChild(InfoNode* child) noexcept
{
  std::unique_ptr<InfoNode> ownedChild;
  if (child->parent != nullptr) {
    InfoNode* oldParent = child->parent;
    ownedChild = oldParent->takeChildOwnership(child);

    if (chainContains(oldParent->firstChild, child)) {
      unlinkFromChain(*child, oldParent->firstChild, oldParent->lastChild);
      if (oldParent->m_childDegree > 0)
        oldParent->setChildDegree(oldParent->m_childDegree - 1);
    } else if (chainContains(oldParent->collapsedFirstChild, child)) {
      unlinkFromChain(*child, oldParent->collapsedFirstChild, oldParent->collapsedLastChild);
    } else {
      child->previous = nullptr;
      child->next = nullptr;
    }
    child->parent = nullptr;
  }

  if (ownedChild == nullptr)
    ownedChild.reset(child);

  appendOwnedChild(std::move(ownedChild));
}

InfoNode& InfoNode::addChild(std::unique_ptr<InfoNode> child) noexcept
{
  auto* childPtr = child.get();
  appendOwnedChild(std::move(child));
  return *childPtr;
}

void InfoNode::releaseChildren() noexcept
{
  auto detachedChildren = detachChildren();
  for (auto& child : detachedChildren.m_children) {
    child.release();
  }
}

InfoNode::DetachedChildChain InfoNode::detachChildren() noexcept
{
  auto unorderedChildren = std::move(m_children);
  std::vector<std::unique_ptr<InfoNode>> orderedChildren;
  orderedChildren.reserve(unorderedChildren.size());

  auto takeFromUnorderedChildren = [&unorderedChildren](InfoNode* child) {
    for (auto it = unorderedChildren.begin(); it != unorderedChildren.end(); ++it) {
      if (it->get() == child) {
        auto ownedChild = std::move(*it);
        unorderedChildren.erase(it);
        return ownedChild;
      }
    }
    return std::unique_ptr<InfoNode>();
  };

  for (auto* child = firstChild; child != nullptr; child = child->next) {
    if (auto ownedChild = takeFromUnorderedChildren(child))
      orderedChildren.push_back(std::move(ownedChild));
  }

  for (auto& child : unorderedChildren) {
    if (child != nullptr)
      orderedChildren.push_back(std::move(child));
  }

  auto* detachedFirstChild = firstChild;
  auto* detachedLastChild = lastChild;
  auto detachedChildDegree = m_childDegree;

  firstChild = nullptr;
  lastChild = nullptr;
  m_childDegree = 0;

  return { std::move(orderedChildren), detachedFirstChild, detachedLastChild, detachedChildDegree };
}

void InfoNode::adoptChildren(DetachedChildChain children) noexcept
{
  for (auto& child : children.m_children) {
    child->previous = nullptr;
    child->next = nullptr;
    appendOwnedChild(std::move(child));
  }
}

void InfoNode::clearDetachedLinks() noexcept
{
  parent = nullptr;
  previous = nullptr;
  next = nullptr;
}

void InfoNode::setParentLink(InfoNode* parentNode) noexcept
{
  parent = parentNode;
}

unsigned int InfoNode::replaceChildWithChildren(InfoNode& child) noexcept
{
  if (child.parent != this || child.isLeaf())
    return 0;

  auto childIt = std::find_if(m_children.begin(), m_children.end(), [&child](const auto& ownedChild) {
    return ownedChild.get() == &child;
  });
  if (childIt == m_children.end())
    return 0;

  auto replacementChildren = child.detachChildren();
  if (replacementChildren.empty())
    return 0;

  auto* previousSibling = child.previous;
  auto* nextSibling = child.next;
  auto* replacementFirst = replacementChildren.first();
  auto* replacementLast = replacementChildren.last();
  const auto replacementCount = replacementChildren.size();

  for (auto* replacement = replacementFirst; replacement != nullptr; replacement = replacement->next) {
    replacement->parent = this;
  }

  replacementFirst->previous = previousSibling;
  replacementLast->next = nextSibling;

  if (firstChild == &child) {
    firstChild = replacementFirst;
  } else if (previousSibling != nullptr) {
    previousSibling->next = replacementFirst;
  }

  if (lastChild == &child) {
    lastChild = replacementLast;
  } else if (nextSibling != nullptr) {
    nextSibling->previous = replacementLast;
  }

  child.parent = nullptr;
  child.previous = nullptr;
  child.next = nullptr;
  child.setChildDegree(0);

  auto removedChild = std::move(*childIt);
  auto insertPos = m_children.erase(childIt);
  m_children.insert(
      insertPos,
      std::make_move_iterator(replacementChildren.m_children.begin()),
      std::make_move_iterator(replacementChildren.m_children.end()));

  setChildDegree(childDegree() + replacementCount - 1);
  return 1;
}

void InfoNode::removeChild(InfoNode& child, RemoveChildrenPolicy policy) noexcept
{
  if (child.parent != this)
    return;

  auto childIt = std::find_if(m_children.begin(), m_children.end(), [&child](const auto& ownedChild) {
    return ownedChild.get() == &child;
  });
  if (childIt == m_children.end())
    return;

  InfoNode::DetachedChildChain detachedChildren;
  if (policy == RemoveChildrenPolicy::DetachChildren) {
    detachedChildren = child.detachChildren();
    for (auto& detachedChild : detachedChildren.m_children) {
      detachedChild.release();
    }
  }

  unlinkFromChain(child, firstChild, lastChild);
  child.parent = nullptr;
  if (m_childDegree > 0)
    setChildDegree(m_childDegree - 1);

  m_children.erase(childIt);
}

InfoNode& InfoNode::replaceChildrenWithOneNode()
{
  if (childDegree() == 1)
    return *firstChild;
  if (firstChild == nullptr)
    throw std::logic_error("replaceChildrenWithOneNode called on a node without any children.");
  if (firstChild->firstChild == nullptr)
    throw std::logic_error("replaceChildrenWithOneNode called on a node without any grandchildren.");

  std::vector<InfoNode*> originalChildren;
  originalChildren.reserve(m_childDegree);
  for (auto* child = firstChild; child != nullptr; child = child->next) {
    originalChildren.push_back(child);
  }

  auto middleNode = std::make_unique<InfoNode>();
  auto* middleNodePtr = middleNode.get();
  auto d0 = m_childDegree;

  for (auto* child : originalChildren) {
    auto ownedChild = takeChildOwnership(child);
    if (ownedChild == nullptr)
      ownedChild.reset(child);
    child->parent = nullptr;
    child->previous = nullptr;
    child->next = nullptr;
    middleNodePtr->appendOwnedChild(std::move(ownedChild));
  }

  firstChild = nullptr;
  lastChild = nullptr;
  m_childDegree = 0;
  addChild(std::move(middleNode));
  auto d1 = middleNodePtr->replaceChildrenWithGrandChildren();
  if (d1 != d0)
    throw std::logic_error("replaceChildrenWithOneNode replaced different number of children as having before");
  return *middleNodePtr;
}

unsigned int InfoNode::replaceChildrenWithGrandChildren() noexcept
{
  if (firstChild == nullptr)
    return 0;
  InfoNode::child_iterator nodeIt = begin_child();
  unsigned int numOriginalChildrenLeft = m_childDegree;
  unsigned int numChildrenReplaced = 0;
  do {
    InfoNode* n = nodeIt.current();
    ++nodeIt;
    numChildrenReplaced += replaceChildWithChildren(*n);
  } while (--numOriginalChildrenLeft != 0);
  return numChildrenReplaced;
}

unsigned int InfoNode::replaceWithChildren() noexcept
{
  if (isLeaf() || isRoot())
    return 0;

  return parent->replaceChildWithChildren(*this);
}

void InfoNode::replaceChildrenWithGrandChildrenDebug() noexcept
{
  if (firstChild == nullptr)
    return;
  InfoNode::child_iterator nodeIt = begin_child();
  unsigned int numOriginalChildrenLeft = m_childDegree;
  do {
    InfoNode* n = nodeIt.current();
    ++nodeIt;
    replaceChildWithChildren(*n);
  } while (--numOriginalChildrenLeft != 0);
}

void InfoNode::replaceWithChildrenDebug() noexcept
{
  if (isLeaf() || isRoot())
    return;

  parent->replaceChildWithChildren(*this);
}

void InfoNode::remove(bool removeChildren) noexcept
{
  if (parent != nullptr) {
    parent->removeChild(*this, removeChildren ? RemoveChildrenPolicy::DetachChildren : RemoveChildrenPolicy::DeleteSubtree);
    return;
  }

  // Legacy detached-node compatibility. Internally, parent-owned nodes are
  // removed through removeChild().
  if (removeChildren) {
    for (auto& child : m_children) {
      child.release();
    }
    m_children.clear();
    firstChild = nullptr;
    lastChild = nullptr;
    setChildDegree(0);
  }
  std::unique_ptr<InfoNode> self(this);
}

void InfoNode::deleteChildren() noexcept
{
  firstChild = nullptr;
  lastChild = nullptr;
  collapsedFirstChild = nullptr;
  collapsedLastChild = nullptr;
  m_childDegree = 0;

  auto detachOwnedChildren = [](std::vector<std::unique_ptr<InfoNode>>& children) {
    for (auto& child : children) {
      child->parent = nullptr;
      child->previous = nullptr;
      child->next = nullptr;
    }
    children.clear();
  };

  detachOwnedChildren(m_children);
  detachOwnedChildren(m_collapsedChildren);
}

void InfoNode::calcChildDegree() noexcept
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

void InfoNode::setChildDegree(unsigned int value) noexcept
{
  m_childDegree = value;
  m_childrenChanged = false;
}

void InfoNode::initClean() noexcept
{
  releaseChildren();
  collapsedFirstChild = nullptr;
  collapsedLastChild = nullptr;
  previous = next = parent = nullptr;

  physicalNodes.clear();
}

void InfoNode::sortChildrenOnFlow(bool recurse) noexcept
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
    firstChild = nullptr;
    lastChild = nullptr;
    m_childDegree = 0;
    for (auto node : nodes) {
      appendLinkedChild(node);
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

unsigned int InfoNode::collapseChildren() noexcept
{
  unsigned int numCollapsedChildren = childDegree();
  auto* activeFirstChild = firstChild;
  auto* activeLastChild = lastChild;
  m_collapsedChildren = std::move(m_children);
  firstChild = nullptr;
  lastChild = nullptr;
  m_childDegree = 0;
  collapsedFirstChild = activeFirstChild;
  collapsedLastChild = activeLastChild;
  return numCollapsedChildren;
}

unsigned int InfoNode::expandChildren()
{
  bool haveCollapsedChildren = collapsedFirstChild != nullptr;
  if (haveCollapsedChildren) {
    if (firstChild != nullptr || lastChild != nullptr)
      throw std::logic_error("Expand collapsed children called on a node that already has children.");
    m_children = std::move(m_collapsedChildren);
    std::swap(collapsedFirstChild, firstChild);
    std::swap(collapsedLastChild, lastChild);
    calcChildDegree();
    return childDegree();
  }
  return 0;
}

} // namespace infomap
