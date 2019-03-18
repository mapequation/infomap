/*
 * NodeBase.cpp
 *
 *  Created on: 19 feb 2015
 *      Author: Daniel
 */
#include "NodeBase.h"
#include "InfomapCore.h"
#include <algorithm>

namespace infomap {


NodeBase::~NodeBase()
{
	if (m_infomap != nullptr) {
		delete m_infomap;
	}

	deleteChildren();

	if (next != 0)
		next->previous = previous;
	if (previous != 0)
		previous->next = next;
	if (parent != 0)
	{
		if (parent->firstChild == this)
			parent->firstChild = next;
		if (parent->lastChild == this)
			parent->lastChild = previous;
	}

	// Delete outgoing edges.
	// TODO: Renders ingoing edges invalid. Assume or assert that all nodes on the same level are deleted?
	for (edge_iterator outEdgeIt(begin_outEdge());
			outEdgeIt != end_outEdge(); ++outEdgeIt)
	{
		delete *outEdgeIt;
	}

}


// InfomapBase& NodeBase::getInfomap(bool reset) {
// 	if (!m_infomap || reset)
// 		m_infomap = std::unique_ptr<InfomapBase>(new M1Infomap());
// 	return *m_infomap;
// }

// InfomapBase& NodeBase::getMemInfomap(bool reset) {
// 	if (!m_infomap || reset)
// 		m_infomap = std::unique_ptr<InfomapBase>(new M2Infomap());
// 	return *m_infomap;
// }

// NodeBase* NodeBase::getInfomapRoot() {
// 	return m_infomap? &m_infomap->root() : nullptr;
// }

// bool NodeBase::disposeInfomap()
// {
// 	if (m_infomap) {
// 		m_infomap.reset();
// 		return true;
// 	}
// 	return false;
// }

// InfomapBase& NodeBase::getInfomap(bool reset) {
// 	if (!m_infomap || reset || m_infomap->isHigherOrder()) {
// 		disposeInfomap();
// 		m_infomap = new M1Infomap();
// 	}
// 	return *m_infomap;
// }

// InfomapBase& NodeBase::getMemInfomap(bool reset) {
// 	if (!m_infomap || reset || !m_infomap->isHigherOrder()) {
// 		disposeInfomap();
// 		m_infomap = new M2Infomap();
// 	}
// 	return *m_infomap;
// }

InfomapBase& NodeBase::setInfomap(InfomapBase* infomap) {
	disposeInfomap();
	m_infomap = infomap;
	if (infomap == nullptr)
		throw InternalOrderError("NodeBase::setInfomap(...) called with null infomap");
	return *m_infomap;
}

InfomapBase& NodeBase::getInfomap() {
	if (m_infomap == nullptr)
		throw InternalOrderError("NodeBase::getInfomap() called but infomap is null");
	return *m_infomap;
}

NodeBase* NodeBase::getInfomapRoot() {
	return m_infomap != nullptr? &m_infomap->root() : nullptr;
}

NodeBase const* NodeBase::getInfomapRoot() const {
	return m_infomap != nullptr? &m_infomap->root() : nullptr;
}

bool NodeBase::disposeInfomap()
{
	if (m_infomap != nullptr) {
		delete m_infomap;
		m_infomap = nullptr;
		return true;
	}
	return false;
}

bool NodeBase::isLeaf() const
{
	return firstChild == nullptr;
}

bool NodeBase::isLeafModule() const
{
	//TODO: Safe to assume all children are leaves if first child is leaf?
	return m_infomap == nullptr && firstChild != nullptr && firstChild->firstChild == nullptr;
}

bool NodeBase::isRoot() const
{
	return parent == nullptr;
}

unsigned int NodeBase::depth() const
{
	unsigned int depth = 0;
	NodeBase* n = parent;
	while (n != nullptr) {
		++depth;
		n = n->parent;
	}
	return depth;
}

unsigned int NodeBase::firstDepthBelow() const
{
	unsigned int depthBelow = 0;
	NodeBase* child = firstChild;
	while (child != nullptr) {
		++depthBelow;
		child = child->firstChild;
	}
	return depthBelow;
}

unsigned int NodeBase::childIndex() const
{
	unsigned int childIndex = 0;
	const NodeBase* n(this);
	while (n->previous) {
		n = n->previous;
		++childIndex;
	}
	return childIndex;
}

std::vector<unsigned int> NodeBase::calculatePath() const
{
	const NodeBase* current = this;
	std::vector<unsigned int> path;
	while (current->parent != nullptr) {
		path.push_back(current->childIndex());
		current = current->parent;
		if (current->owner != nullptr) {
			current = current->owner;
		}
	}
	std::reverse(path.begin(), path.end());
	return path;
}

unsigned int NodeBase::childDegree() const
{
//	if (!m_childrenChanged)
		return m_childDegree;
//	calcChildDegree();
//	return m_childDegree;
}

unsigned int NodeBase::infomapChildDegree() const
{
	return m_infomap == nullptr ? childDegree() : m_infomap->root().childDegree();
}

void NodeBase::addChild(NodeBase* child)
{
//	m_childrenChanged = true;
	if (firstChild == 0)
	{
//		DEBUG_OUT("\t\t-->Add node " << *child << " as FIRST CHILD to parent " << *this << std::endl);
		child->previous = nullptr;
		firstChild = child;
	}
	else
	{
//		DEBUG_OUT("\t\t-->Add node " << *child << " as LAST CHILD to parent " << *this << std::endl);
		child->previous = lastChild;
		lastChild->next = child;
	}
	lastChild = child;
	child->next = nullptr;
	child->parent = this;
	++m_childDegree;
}

void NodeBase::releaseChildren()
{
	firstChild = nullptr;
	lastChild = nullptr;
	m_childDegree = 0;
}

NodeBase& NodeBase::replaceChildrenWithOneNode()
{
	if (childDegree() == 1)
		return *firstChild;
	if (firstChild == nullptr)
		throw InternalOrderError("replaceChildrenWithOneNode called on a node without any children.");
	if (firstChild->firstChild == nullptr)
		throw InternalOrderError("replaceChildrenWithOneNode called on a node without any grandchildren.");
	NodeBase* middleNode = new NodeBase();
	NodeBase::child_iterator nodeIt = begin_child();
	unsigned int numOriginalChildrenLeft = m_childDegree;
	auto d0 = m_childDegree;
	do
	{
		NodeBase* n = nodeIt.current();
		++nodeIt;
		middleNode->addChild(n);
	}
	while (--numOriginalChildrenLeft != 0);
	releaseChildren();
	addChild(middleNode);
	auto d1 = middleNode->replaceChildrenWithGrandChildren();
	if (d1 != d0)
		throw InternalOrderError("replaceChildrenWithOneNode replaced different number of children as having before");
	return *middleNode;
}

unsigned int NodeBase::replaceChildrenWithGrandChildren()
{
	if (firstChild == nullptr)
		return 0;
	NodeBase::child_iterator nodeIt = begin_child();
	unsigned int numOriginalChildrenLeft = m_childDegree;
	unsigned int numChildrenReplaced = 0;
	do
	{
		NodeBase* n = nodeIt.current();
		++nodeIt;
		numChildrenReplaced += n->replaceWithChildren();
	}
	while (--numOriginalChildrenLeft != 0);
	return numChildrenReplaced;
}

unsigned int NodeBase::replaceWithChildren()
{
	if (isLeaf() || isRoot())
		return 0;

	// Reparent children
	unsigned int deltaChildDegree = 0;
	NodeBase* child = firstChild;
	do
	{
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

void NodeBase::replaceChildrenWithGrandChildrenDebug()
{
	if (firstChild == 0)
		return;
	NodeBase::child_iterator nodeIt = begin_child();
	unsigned int numOriginalChildrenLeft = m_childDegree;
	do
	{
		NodeBase* n = nodeIt.current();
		++nodeIt;
		n->replaceWithChildrenDebug();
	}
	while (--numOriginalChildrenLeft != 0);
}


void NodeBase::replaceWithChildrenDebug()
{
	if (isLeaf() || isRoot())
		return;


	// Reparent children
	unsigned int deltaChildDegree = 0;
	NodeBase* child = firstChild;
	do
	{
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

// void NodeBase::storeModulesIn(NodeBase& otherRoot)
// {
// 	// Move root links to otherRoot
// 	otherRoot.firstChild = firstChild;
// 	otherRoot.lastChild = lastChild;
// 	otherRoot.m_childDegree = m_childDegree;
// 	firstChild->parent = &otherRoot;
// 	lastChild->parent = &otherRoot;
	
// 	// Link directly to leaf nodes in curren node instead
// 	// Reparent leaf nodes within same infomap instance
// 	NodeBase* leaf = this;
// 	// Walk down to leaf
//TODO: Use leaf module iteartor and clone each leaf node to
// otherRoot and reparent leaf node to current node.
// 	while (leaf->firstChild != nullptr)
// 	{
// 		leaf = leaf->firstChild;
// 	}
// 	unsigned int numLeafNodes = 0;
// 	// Walk horisontally through all siblings and copy leaf nodes (without edges)
// 	while (leaf != nullptr) {
// 		++numLeafNodes;
// 		leaf->parent = this;
// 		leaf = leaf->next;
// 	}
// 	m_childDegree = numLeafNodes;
// }

void NodeBase::remove(bool removeChildren)
{
	firstChild = removeChildren ? 0 : firstChild;
	delete this;
}

void NodeBase::deleteChildren()
{
	if (firstChild == nullptr)
		return;

	child_iterator nodeIt = begin_child();
	do
	{
		NodeBase* n = nodeIt.current();
		++nodeIt;
		delete n;
	}
	while (nodeIt.current() != nullptr);

	firstChild = nullptr;
	lastChild = nullptr;
	m_childDegree = 0;
}

void NodeBase::calcChildDegree()
{
	m_childrenChanged = false;
	if (firstChild == nullptr)
		m_childDegree = 0;
	else if (firstChild == lastChild)
		m_childDegree = 1;
	else
	{
		m_childDegree = 0;
		for (auto child : children()) {
			++m_childDegree;
		}
	}
}

void NodeBase::setChildDegree(unsigned int value)
{
	m_childDegree = value;
	m_childrenChanged = false;
}

void NodeBase::setNumLeafNodes(unsigned int value)
{
	m_numLeafMembers = value;
}

void NodeBase::initClean()
{
	releaseChildren();
	previous = next = parent = nullptr;

	physicalNodes.clear();
}

unsigned int NodeBase::collapseChildren()
{
	std::swap(collapsedFirstChild, firstChild);
	std::swap(collapsedLastChild, lastChild);
	unsigned int numCollapsedChildren = childDegree();
	releaseChildren();
	return numCollapsedChildren;
}

unsigned int NodeBase::expandChildren()
{
	bool haveCollapsedChildren = collapsedFirstChild != nullptr;
	if (haveCollapsedChildren) {
		if (firstChild != nullptr || lastChild != nullptr)
			throw InternalOrderError("Expand collapsed children called on a node that already has children.");
		std::swap(collapsedFirstChild, firstChild);
		std::swap(collapsedLastChild, lastChild);
		calcChildDegree();
		return childDegree();
	}
	return 0;
}

}
