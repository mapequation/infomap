/*
 * infomapIterators.h
 *
 *  Created on: 2 mar 2015
 *      Author: Daniel
 */

#ifndef MODULES_CLUSTERING_CLUSTERING_INFOMAPITERATORS_H_
#define MODULES_CLUSTERING_CLUSTERING_INFOMAPITERATORS_H_

#include "treeIterators.h"
#include <deque>
#include <limits>

namespace infomap {


/**
 * Pre processing depth first iterator that explores sub-Infomap instances
 * Note:
 * This iterator presupposes that the next pointer of a node can't reach a node with a different parent.
 */
template <typename NodePointerType>
class InfomapClusterIterator : public DepthFirstIteratorBase<NodePointerType>
{
protected:
	typedef DepthFirstIteratorBase<NodePointerType>		Base;

	unsigned int m_moduleIndex = 0;
	int m_moduleIndexLevel = -1;

	using Base::m_depth;
	using Base::m_current;

public:

	InfomapClusterIterator() : Base() {}

	explicit
	InfomapClusterIterator(const NodePointerType& nodePointer, int moduleIndexLevel = -1)
	:	Base(nodePointer),
		m_moduleIndexLevel(moduleIndexLevel)
	{}

	InfomapClusterIterator(const InfomapClusterIterator& other)
	:	Base(other),
		m_moduleIndex(other.m_moduleIndex),
		m_moduleIndexLevel(other.m_moduleIndexLevel)
	{}
	
	virtual void init()
	{
		moveToInfomapRootIfExist();
	}

	void moveToInfomapRootIfExist()
	{
		if (m_current != nullptr) {
			NodePointerType infomapRoot = m_current->getInfomapRoot();
			if (infomapRoot != nullptr)
			{
				m_current = infomapRoot;
			}
		}
	}

	InfomapClusterIterator& operator= (const InfomapClusterIterator& other)
	{
		Base::operator=(other);
		m_moduleIndex = other.m_moduleIndex;
		m_moduleIndexLevel = other.m_moduleIndexLevel;
		return *this;
	}

	InfomapClusterIterator&
	operator++()
	{
		NodePointerType curr = Base::m_current;

		if(curr->firstChild != nullptr)
		{
			curr = curr->firstChild;
			++m_depth;
		}
		else
		{
			// Current node is a leaf
			// Presupposes that the next pointer can't reach out from the current parent.
			tryNext:
			while(curr->next == nullptr)
			{
				if (curr->parent != nullptr)
				{
					curr = curr->parent;
					--m_depth;
					if(curr == Base::m_root) // Check if back to beginning
					{
						m_current = nullptr;
						return *this;
					}
					if (m_moduleIndexLevel < 0) {
						 if (curr->isLeafModule()) // TODO: Generalize to -2 for second level to bottom
							 ++m_moduleIndex;
					}
					else if (static_cast<unsigned int>(m_moduleIndexLevel) == m_depth)
						++m_moduleIndex;
				}
				else
				{
					NodePointerType infomapOwner = curr->owner;
					if (infomapOwner != nullptr)
					{
						curr = infomapOwner;
						if(curr == Base::m_root) // Check if back to beginning
						{
							m_current = nullptr;
							return *this;
						}
						goto tryNext;
					}
					else // null also if no children in first place
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

	InfomapClusterIterator
	operator++(int)
	{
		InfomapClusterIterator copy(*this);
		++(*this);
		return copy;
	}

	InfomapClusterIterator
	next()
	{
		InfomapClusterIterator copy(*this);
		return ++copy;
	}

	InfomapClusterIterator&
	stepForward()
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
 * Note:
 * This iterator presupposes that the next pointer of a node can't reach a node with a different parent.
 */
template <typename NodePointerType>
class InfomapDepthFirstIterator : public DepthFirstIteratorBase<NodePointerType>
{
protected:
	typedef DepthFirstIteratorBase<NodePointerType>		Base;

	std::deque<unsigned int> m_path; // The child index path to current node
	unsigned int m_moduleIndex = 0;
	int m_moduleIndexLevel = -1;

	using Base::m_depth;
	using Base::m_current;

public:

	InfomapDepthFirstIterator() : Base() {}

	explicit
	InfomapDepthFirstIterator(const NodePointerType& nodePointer, int moduleIndexLevel = -1)
	:	Base(nodePointer),
		m_moduleIndexLevel(moduleIndexLevel)
	{}

	InfomapDepthFirstIterator(const InfomapDepthFirstIterator& other)
	:	Base(other),
		m_path(other.m_path),
		m_moduleIndex(other.m_moduleIndex),
		m_moduleIndexLevel(other.m_moduleIndexLevel)
	{}

	InfomapDepthFirstIterator& operator= (const InfomapDepthFirstIterator& other)
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
			if (infomapRoot != nullptr)
			{
				m_current = infomapRoot;
			}
		}
	}

	InfomapDepthFirstIterator&
	operator++()
	{
		NodePointerType curr = Base::m_current;

		if(curr->firstChild != nullptr)
		{
			curr = curr->firstChild;
			++m_depth;
			m_path.push_back(0);
		}
		else
		{
			// Current node is a leaf
			// Presupposes that the next pointer can't reach out from the current parent.
			tryNext:
			while(curr->next == nullptr)
			{
				if (curr->parent != nullptr)
				{
					curr = curr->parent;
					--m_depth;
					m_path.pop_back();
					if(curr == Base::m_root) // Check if back to beginning
					{
						m_current = nullptr;
						return *this;
					}
					if (m_moduleIndexLevel < 0) {
						 if (curr->isLeafModule()) // TODO: Generalize to -2 for second level to bottom
							 ++m_moduleIndex;
					}
					else if (static_cast<unsigned int>(m_moduleIndexLevel) == m_depth)
						++m_moduleIndex;
				}
				else
				{
					NodePointerType infomapOwner = curr->owner;
					if (infomapOwner != nullptr)
					{
						curr = infomapOwner;
						if(curr == Base::m_root) // Check if back to beginning
						{
							m_current = nullptr;
							return *this;
						}
						goto tryNext;
					}
					else // null also if no children in first place
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

	InfomapDepthFirstIterator
	operator++(int)
	{
		InfomapDepthFirstIterator copy(*this);
		++(*this);
		return copy;
	}

	InfomapDepthFirstIterator
	next()
	{
		InfomapDepthFirstIterator copy(*this);
		return ++copy;
	}

	InfomapDepthFirstIterator&
	stepForward()
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

}

#endif /* MODULES_CLUSTERING_CLUSTERING_INFOMAPITERATORS_H_ */
