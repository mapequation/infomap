/*
 * InfomapIterator.h
 *
 */

#ifndef INFOMAP_ITERATOR_H_
#define INFOMAP_ITERATOR_H_

#include <deque>
#include <map>
#include "Node.h"
#include "FlowData.h"

namespace infomap {

// class NodeBase;


/**
 * Pre processing depth first iterator that explores sub-Infomap instances
 * Note:
 * This iterator presupposes that the next pointer of a node can't reach a node with a different parent.
 */
class InfomapIterator
{
protected:
	NodeBase* m_root = nullptr;
	NodeBase* m_current = nullptr;
	int m_moduleIndexLevel = -1;
	unsigned int m_moduleIndex = 0;
	std::deque<unsigned int> m_path; // The child index path to current node
	unsigned int m_depth = 0;

public:

	InfomapIterator() {}

	InfomapIterator(NodeBase* nodePointer, int moduleIndexLevel = -1)
	:	m_root(nodePointer),
		m_current(nodePointer),
		m_moduleIndexLevel(moduleIndexLevel)
	{}

	InfomapIterator(const InfomapIterator& other)
	:	m_root(other.m_root),
		m_current(other.m_current),
		m_moduleIndexLevel(other.m_moduleIndexLevel),
		m_moduleIndex(other.m_moduleIndex),
		m_path(other.m_path),
		m_depth(other.m_depth)
	{}

	virtual ~InfomapIterator() {}

	InfomapIterator& operator= (const InfomapIterator& other)
	{
		m_root = other.m_root;
		m_current = other.m_current;
		m_moduleIndexLevel = other.m_moduleIndexLevel;
		m_moduleIndex = other.m_moduleIndex;
		m_path = other.m_path;
		m_depth = other.m_depth;
		return *this;
	}

	NodeBase* current()
	{
		return m_current;
	}

	const NodeBase* current() const
	{
		return m_current;
	}

	NodeBase& operator*()
	{
		return *m_current;
	}

	const NodeBase& operator*() const
	{
		return *m_current;
	}

	NodeBase* operator->()
	{
		return m_current;
	}

	const NodeBase* operator->() const
	{
		return m_current;
	}

	// NodeBase& operator*();

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

	unsigned int childIndex() const
	{
		return m_path.empty() ? 0 : m_path.back();
	}

	unsigned int depth() const
	{
		return m_depth;
	}

	bool isEnd() const
	{
		return m_current == nullptr;
	}
};

class InfomapModuleIterator : public InfomapIterator
{
public:
	InfomapModuleIterator() : InfomapIterator() {}

	InfomapModuleIterator(NodeBase* nodePointer, int moduleIndexLevel = -1)
	:	InfomapIterator(nodePointer, moduleIndexLevel)
	{}

	InfomapModuleIterator(const InfomapModuleIterator& other)
	:	InfomapIterator(other)
	{}
	
	InfomapModuleIterator& operator= (const InfomapModuleIterator& other)
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

};

class InfomapLeafModuleIterator : public InfomapIterator
{
public:
	InfomapLeafModuleIterator() : InfomapIterator() {}

	InfomapLeafModuleIterator(NodeBase* nodePointer, int moduleIndexLevel = -1)
	:	InfomapIterator(nodePointer, moduleIndexLevel)
	{ init(); }

	InfomapLeafModuleIterator(const InfomapLeafModuleIterator& other)
	:	InfomapIterator(other)
	{ init(); }
	
	InfomapLeafModuleIterator& operator= (const InfomapLeafModuleIterator& other)
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

};

class InfomapLeafIterator : public InfomapIterator
{
public:
	InfomapLeafIterator() : InfomapIterator() {}

	InfomapLeafIterator(NodeBase* nodePointer, int moduleIndexLevel = -1)
	:	InfomapIterator(nodePointer, moduleIndexLevel)
	{ init(); }

	InfomapLeafIterator(const InfomapLeafIterator& other)
	:	InfomapIterator(other)
	{ init(); }
	
	InfomapLeafIterator& operator= (const InfomapLeafIterator& other)
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

};

/**
 * Iterate over the whole tree, collecting physical nodes within same leaf modules
 * Note: The physical nodes are created when entering the parent module and removed
 * when leaving the module. The tree will not be modified.
 */
class InfomapIteratorPhysical : public InfomapIterator
{
protected:
	using PhysNode = Node<FlowData>;
	std::map<unsigned int, PhysNode> m_physNodes;
	std::map<unsigned int, PhysNode>::iterator m_physIter;
	InfomapIterator m_oldIter;

public:
	InfomapIteratorPhysical() : InfomapIterator() {}

	InfomapIteratorPhysical(NodeBase* nodePointer, int moduleIndexLevel = -1)
	:	InfomapIterator(nodePointer, moduleIndexLevel)
	{}

	InfomapIteratorPhysical(const InfomapIteratorPhysical& other)
	:	InfomapIterator(other),
		m_physNodes(other.m_physNodes),
		m_physIter(other.m_physIter),
		m_oldIter(other.m_oldIter)
	{}

	InfomapIteratorPhysical(const InfomapIterator& other)
	:	InfomapIterator(other)
	{}
	
	InfomapIteratorPhysical& operator= (const InfomapIteratorPhysical& other)
	{
		InfomapIterator::operator=(other);
		m_physNodes = other.m_physNodes;
		m_physIter = other.m_physIter;
		m_oldIter = other.m_oldIter;
		return *this;
	}
	
	InfomapIteratorPhysical& operator= (const InfomapIterator& other)
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

};

/**
 * Iterate over all physical leaf nodes, joining physical nodes within same leaf modules
 * Note: The physical nodes are created when entering the parent module and removed
 * when leaving the module. The tree will not be modified.
 */
class InfomapLeafIteratorPhysical : public InfomapIteratorPhysical
{
public:
	InfomapLeafIteratorPhysical()
	: InfomapIteratorPhysical()
	{}

	InfomapLeafIteratorPhysical(NodeBase* nodePointer, int moduleIndexLevel = -1)
	:	InfomapIteratorPhysical(nodePointer, moduleIndexLevel)
	{ init(); }

	InfomapLeafIteratorPhysical(const InfomapLeafIteratorPhysical& other)
	:	InfomapIteratorPhysical(other)
	{ init(); }
	
	InfomapLeafIteratorPhysical& operator= (const InfomapLeafIteratorPhysical& other)
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

};

/**
 * Iterate parent by parent until it is nullptr,
 * moving up through possible sub infomap instances
 * on the way
 */
class InfomapParentIterator
{
protected:
	// NodeBase* m_root = nullptr;
	NodeBase* m_current = nullptr;
	// int m_moduleIndexLevel = -1;
	// unsigned int m_moduleIndex = 0;
	// std::deque<unsigned int> m_path; // The child index path to current node
	// unsigned int m_depth = 0;

public:

	InfomapParentIterator() {}

	InfomapParentIterator(NodeBase* nodePointer)
	: m_current(nodePointer)
	{}

	InfomapParentIterator(const InfomapParentIterator& other)
	:	m_current(other.m_current)
	{}

	virtual ~InfomapParentIterator() {}

	InfomapParentIterator& operator= (const InfomapParentIterator& other)
	{
		m_current = other.m_current;
		return *this;
	}

	NodeBase* current()
	{
		return m_current;
	}

	const NodeBase* current() const
	{
		return m_current;
	}

	NodeBase& operator*()
	{
		return *m_current;
	}

	const NodeBase& operator*() const
	{
		return *m_current;
	}

	NodeBase* operator->()
	{
		return m_current;
	}

	const NodeBase* operator->() const
	{
		return m_current;
	}

	// NodeBase& operator*();

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


}

#endif /* INFOMAP_ITERATOR_H_ */
