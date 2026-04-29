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

namespace {

class DetachedChildChain {
public:
  DetachedChildChain() = default;

  DetachedChildChain(InfoNode* first, InfoNode* last) noexcept
      : m_first(first),
        m_last(last) { }

  DetachedChildChain(const DetachedChildChain&) = delete;
  DetachedChildChain& operator=(const DetachedChildChain&) = delete;

  DetachedChildChain(DetachedChildChain&& other) noexcept
      : m_first(other.m_first),
        m_last(other.m_last)
  {
    other.m_first = nullptr;
    other.m_last = nullptr;
  }

  DetachedChildChain& operator=(DetachedChildChain&& other) noexcept
  {
    if (this != &other) {
      reset();
      m_first = other.m_first;
      m_last = other.m_last;
      other.m_first = nullptr;
      other.m_last = nullptr;
    }
    return *this;
  }

  ~DetachedChildChain() noexcept
  {
    reset();
  }

  InfoNode* first() const noexcept { return m_first; }

  InfoNode* last() const noexcept { return m_last; }

  bool empty() const noexcept { return m_first == nullptr; }

  void release() noexcept
  {
    m_first = nullptr;
    m_last = nullptr;
  }

  void reset() noexcept
  {
    InfoNode* node = m_first;
    m_first = nullptr;
    m_last = nullptr;
    while (node != nullptr) {
      InfoNode* next = node->next;
      delete node;
      node = next;
    }
  }

private:
  InfoNode* m_first = nullptr;
  InfoNode* m_last = nullptr;
};

void spliceChildrenIntoParentBeforeDelete(InfoNode& node) noexcept
{
  InfoNode* parent = node.parent;
  InfoNode* previous = node.previous;
  InfoNode* next = node.next;
  InfoNode* firstChild = node.firstChild;
  InfoNode* lastChild = node.lastChild;

  unsigned int childCount = 0;
  for (InfoNode* child = firstChild; child != nullptr; child = child->next) {
    child->parent = parent;
    ++childCount;
  }

  parent->setChildDegree(parent->childDegree() + childCount - 1); // -1 as this node is deleted.

  firstChild->previous = previous;
  lastChild->next = next;

  if (parent->firstChild == &node) {
    parent->firstChild = firstChild;
  } else {
    previous->next = firstChild;
  }

  if (parent->lastChild == &node) {
    parent->lastChild = lastChild;
  } else {
    next->previous = lastChild;
  }

  // Detach before self-delete so the destructor does not delete the reparented
  // child chain or reconnect sibling links that were already spliced.
  node.firstChild = nullptr;
  node.lastChild = nullptr;
  node.next = nullptr;
  node.previous = nullptr;
  node.parent = nullptr;
  node.setChildDegree(0);

}

} // namespace

void InfomapBaseDeleter::operator()(InfomapBase* infomap) const noexcept
{
  delete infomap;
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
  m_outEdges.clear();
  m_inEdges.clear();
  m_infomap.reset();
  copyDetachedValueStateFrom(other);
  return *this;
}

InfoNode::~InfoNode() noexcept
{
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

void InfoNode::addChild(InfoNode* child) noexcept
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

InfoNode& InfoNode::addChild(std::unique_ptr<InfoNode> child) noexcept
{
  auto* childPtr = child.get();
  addChild(childPtr);
  child.release();
  return *childPtr;
}

void InfoNode::releaseChildren() noexcept
{
  DetachedChildChain detached(firstChild, lastChild);
  firstChild = nullptr;
  lastChild = nullptr;
  m_childDegree = 0;
  detached.release();
}

InfoNode& InfoNode::replaceChildrenWithOneNode()
{
  if (childDegree() == 1)
    return *firstChild;
  if (firstChild == nullptr)
    throw std::logic_error("replaceChildrenWithOneNode called on a node without any children.");
  if (firstChild->firstChild == nullptr)
    throw std::logic_error("replaceChildrenWithOneNode called on a node without any grandchildren.");
  auto middleNode = std::make_unique<InfoNode>();
  auto* middleNodePtr = middleNode.get();
  InfoNode::child_iterator nodeIt = begin_child();
  unsigned int numOriginalChildrenLeft = m_childDegree;
  auto d0 = m_childDegree;
  do {
    InfoNode* n = nodeIt.current();
    ++nodeIt;
    middleNodePtr->addChild(n);
  } while (--numOriginalChildrenLeft != 0);

  auto detachedChildren = DetachedChildChain(firstChild, lastChild);
  firstChild = nullptr;
  lastChild = nullptr;
  m_childDegree = 0;
  addChild(std::move(middleNode));
  detachedChildren.release();
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
    numChildrenReplaced += n->replaceWithChildren();
  } while (--numOriginalChildrenLeft != 0);
  return numChildrenReplaced;
}

unsigned int InfoNode::replaceWithChildren() noexcept
{
  if (isLeaf() || isRoot())
    return 0;

  spliceChildrenIntoParentBeforeDelete(*this);
  delete this;
  return 1;
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
    n->replaceWithChildrenDebug();
  } while (--numOriginalChildrenLeft != 0);
}

void InfoNode::replaceWithChildrenDebug() noexcept
{
  if (isLeaf() || isRoot())
    return;

  spliceChildrenIntoParentBeforeDelete(*this);
  delete this;
}

void InfoNode::remove(bool removeChildren) noexcept
{
  firstChild = removeChildren ? nullptr : firstChild;
  delete this;
}

void InfoNode::deleteChildren() noexcept
{
  DetachedChildChain activeChildren(firstChild, lastChild);
  firstChild = nullptr;
  lastChild = nullptr;
  DetachedChildChain collapsedChildren(collapsedFirstChild, collapsedLastChild);
  collapsedFirstChild = nullptr;
  collapsedLastChild = nullptr;
  m_childDegree = 0;
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
    auto detachedChildren = DetachedChildChain(firstChild, lastChild);
    firstChild = nullptr;
    lastChild = nullptr;
    m_childDegree = 0;
    for (auto node : nodes) {
      addChild(node);
    }
    detachedChildren.release();
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
  auto activeChildren = DetachedChildChain(firstChild, lastChild);
  unsigned int numCollapsedChildren = childDegree();
  firstChild = nullptr;
  lastChild = nullptr;
  m_childDegree = 0;
  collapsedFirstChild = activeChildren.first();
  collapsedLastChild = activeChildren.last();
  activeChildren.release();
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
