/**********************************************************************************

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

**********************************************************************************/


#ifndef TREEITERATORS_H_
#define TREEITERATORS_H_

#include <cstddef>
#include <iterator>

namespace infomap {

#ifndef ASSERT
#include <cassert>
#define ASSERT( x ) assert(x)
#endif

using std::iterator_traits;

/**
 * Base node iterator.
 */
template <typename NodePointerType, typename iterator_tag = std::bidirectional_iterator_tag>
struct node_iterator_base
{
//	typedef typename iterator_traits<NodePointerType>::iterator_category	iterator_category; //random_access_iterator_tag
//	typedef std::forward_iterator_tag										iterator_category;
	typedef iterator_tag													iterator_category;
	typedef typename iterator_traits<NodePointerType>::value_type  			value_type;
	typedef typename iterator_traits<NodePointerType>::difference_type		difference_type;
	typedef typename iterator_traits<NodePointerType>::reference			reference;
	typedef typename iterator_traits<NodePointerType>::pointer				pointer;

	node_iterator_base()
	:	m_current(nullptr)
	{
		init();
	}

	explicit
	node_iterator_base(const NodePointerType& nodePointer)
	:	m_current(nodePointer)
	{
		init();
	}

	node_iterator_base(const node_iterator_base& other)
	:	m_current(other.m_current)
	{}

	node_iterator_base & operator= (const node_iterator_base& other)
	{
		m_current = other.m_current;
		return *this;
	}

	virtual ~node_iterator_base() {}

	virtual void init() {}

	pointer base() const
	{ return m_current; }

	reference
	operator*() const
	{ return *m_current; }

	pointer
	operator->() const
	{ return m_current; }

	bool operator==(const node_iterator_base& rhs) const
	{
		return m_current == rhs.m_current;
	}

	bool operator!=(const node_iterator_base& rhs) const
	{
		return !(m_current == rhs.m_current);
	}

	bool isEnd() const
	{
		return m_current == nullptr;
	}

protected:
	NodePointerType m_current;
};

template <typename NodePointerType>
class DepthFirstIteratorBase : public node_iterator_base<NodePointerType>
{
	typedef node_iterator_base<NodePointerType>		Base;
public:

	DepthFirstIteratorBase()
	:	Base(),
		m_root(nullptr),
		m_depth(0)
	{}

	explicit
	DepthFirstIteratorBase(const NodePointerType& nodePointer)
	:	Base(nodePointer),
		m_root(nodePointer),
		m_depth(0)
	{}

	DepthFirstIteratorBase(const DepthFirstIteratorBase& other)
	:	Base(other),
		m_root(other.m_root),
		m_depth(other.m_depth)
	{}

	DepthFirstIteratorBase & operator= (const DepthFirstIteratorBase& other)
	{
		Base::operator=(other);
		m_root = other.m_root;
		m_depth = other.m_depth;
		return *this;
	}

	unsigned int depth() const
	{
		return m_depth;
	}

protected:
	NodePointerType m_root;
	unsigned int m_depth;
	using Base::m_current;
};


/**
 * Pre processing depth first iterator
 * Note:
 * This iterator presupposes that the next pointer of a node can't reach a node with a different parent.
 */
template <typename NodePointerType, bool pre_t = true>
class DepthFirstIterator : public DepthFirstIteratorBase<NodePointerType>
{
	typedef DepthFirstIteratorBase<NodePointerType>		Base;
public:

	DepthFirstIterator() : Base() {}

	explicit
	DepthFirstIterator(const NodePointerType& nodePointer) : Base(nodePointer) {}

	DepthFirstIterator(const DepthFirstIterator& other) : Base(other) {}

	DepthFirstIterator& operator= (const DepthFirstIterator& other)
	{
		Base::operator=(other);
		return *this;
	}

	DepthFirstIterator&
	operator++()
	{
		NodePointerType curr = Base::m_current;
		if(curr->firstChild != nullptr)
		{
			curr = curr->firstChild;
			++Base::m_depth;
		}
		else
		{
			// Presupposes that the next pointer can't reach out from the current parent.
			while(curr->next == nullptr)
			{
				curr = curr->parent;
				--Base::m_depth;
				if(curr == Base::m_root || curr == nullptr) // 0 if no children in first place
				{
					Base::m_current = nullptr;
					return *this;
				}
			}
			curr = curr->next;
		}
		Base::m_current = curr;
		return *this;
	}

	DepthFirstIterator
	operator++(int)
	{
		DepthFirstIterator copy(*this);
		++(*this);
		return copy;
	}

	DepthFirstIterator
	next()
	{
		DepthFirstIterator copy(*this);
		return ++copy;
	}
};


/**
 * Post processing depth first iterator
 * Note:
 * This iterator presupposes that the next pointer of a node can't reach a node with a different parent.
 */
template <typename NodePointerType>
class DepthFirstIterator<NodePointerType, false> : public DepthFirstIteratorBase<NodePointerType>
{
	typedef DepthFirstIteratorBase<NodePointerType>		Base;
public:

	DepthFirstIterator() : Base() {}

	explicit
	DepthFirstIterator(const NodePointerType& nodePointer) : Base(nodePointer) {}

	DepthFirstIterator(const DepthFirstIterator& other) : Base(other) {}

	DepthFirstIterator& operator= (const DepthFirstIterator& other)
	{
		Base::operator=(other);
		return *this;
	}

	virtual void init()
	{
		if (Base::m_current != nullptr)
		{
			while(Base::m_current->firstChild != nullptr)
			{
				Base::m_current = Base::m_current->firstChild;
				++Base::m_depth;
			}
		}
	}

	DepthFirstIterator&
	operator++()
	{
		// The root should be the last node
		if(Base::m_current == Base::m_root)
		{
			Base::m_current = nullptr;
			return *this;
		}

		NodePointerType curr = Base::m_current;
		if (curr->next != nullptr)
		{
			curr = curr->next;
			while (curr->firstChild != nullptr)
			{
				curr = curr->firstChild;
				++Base::m_depth;
			}
		}
		else
		{
			curr = curr->parent;
			--Base::m_depth;
		}

		Base::m_current = curr;

		return *this;
	}

	DepthFirstIterator
	operator++(int)
	{
		DepthFirstIterator copy(*this);
		++(*this);
		return copy;
	}

	DepthFirstIterator
	next()
	{
		DepthFirstIterator copy(*this);
		return ++copy;
	}
};


/**
 * Leaf node iterator
 */
template <typename NodePointerType>
class LeafNodeIterator : public DepthFirstIteratorBase<NodePointerType>
{
	typedef DepthFirstIteratorBase<NodePointerType>		Base;
public:

	LeafNodeIterator() : Base() {}

	explicit
	LeafNodeIterator(const NodePointerType& nodePointer) : Base(nodePointer) {}

	LeafNodeIterator(const LeafNodeIterator& other) : Base(other) {}

	LeafNodeIterator& operator= (const LeafNodeIterator& other)
	{
		Base::operator=(other);
		return *this;
	}

	virtual void init()
	{
		if (Base::m_current != nullptr)
		{
			while(Base::m_current->firstChild != nullptr)
			{
				Base::m_current = Base::m_current->firstChild;
				++Base::m_depth;
			}
		}
	}

	LeafNodeIterator&
	operator++()
	{
		ASSERT(Base::m_current != nullptr);
		while(Base::m_current->next == nullptr || Base::m_current->next->parent != Base::m_current->parent)
		{
			Base::m_current = Base::m_current->parent;
			--Base::m_depth;
			if(Base::m_current == nullptr)
				return *this;
		}

		Base::m_current = Base::m_current->next;

		if (Base::m_current != nullptr)
		{
			while(Base::m_current->firstChild != nullptr)
			{
				Base::m_current = Base::m_current->firstChild;
				++Base::m_depth;
			}
		}
		return *this;
	}

	LeafNodeIterator
	operator++(int)
	{
		LeafNodeIterator copy(*this);
		++(*this);
		return copy;
	}

	LeafNodeIterator
	next()
	{
		LeafNodeIterator copy(*this);
		return ++copy;
	}
};


/**
 * Leaf module iterator
 */
template <typename NodePointerType>
class LeafModuleIterator : public DepthFirstIteratorBase<NodePointerType>
{
	typedef DepthFirstIteratorBase<NodePointerType>		Base;
public:

	LeafModuleIterator() : Base() {}

	explicit
	LeafModuleIterator(const NodePointerType& nodePointer) : Base(nodePointer) {}

	LeafModuleIterator(const LeafModuleIterator& other) : Base(other) {}

	LeafModuleIterator& operator= (const LeafModuleIterator& other)
	{
		Base::operator=(other);
		init();
		return *this;
	}

	virtual void init()
	{
		if (Base::m_current != nullptr)
		{
			if (Base::m_current->firstChild == nullptr)
			{
				Base::m_current = nullptr; // End directly if no module
			}
			else
			{
				while(Base::m_current->firstChild->firstChild != nullptr)
				{
					Base::m_current = Base::m_current->firstChild;
					++Base::m_depth;
				}
			}
		}
	}

	LeafModuleIterator&
	operator++()
	{
		ASSERT(Base::m_current != nullptr);
		while(Base::m_current->next == nullptr || Base::m_current->next->parent != Base::m_current->parent)
		{
			Base::m_current = Base::m_current->parent;
			--Base::m_depth;
			if(Base::m_current == nullptr)
				return *this;
		}

		Base::m_current = Base::m_current->next;

		if (Base::m_current != nullptr)
		{
			if (Base::m_current->firstChild == nullptr)
			{
				Base::m_current = Base::m_current->parent;
			}
			else
			{
				while(Base::m_current->firstChild->firstChild != nullptr)
				{
					Base::m_current = Base::m_current->firstChild;
					++Base::m_depth;
				}
			}
		}
		return *this;
	}

	LeafModuleIterator
	operator++(int)
	{
		LeafModuleIterator copy(*this);
		++(*this);
		return copy;
	}

	LeafModuleIterator
	next()
	{
		LeafModuleIterator copy(*this);
		return ++copy;
	}
};




/**
 * Sibling iterator.
 */
template <typename NodePointerType> // pointer or const pointer
class SiblingIterator : public node_iterator_base<NodePointerType>
{
	typedef node_iterator_base<NodePointerType>								Base;
public:
	typedef SiblingIterator<NodePointerType>								self_type;

	SiblingIterator() : Base() {}

	explicit
	SiblingIterator(const NodePointerType& nodePointer) : Base(nodePointer) {}

	SiblingIterator(const SiblingIterator& other) : Base(other) {}

	SiblingIterator& operator= (const SiblingIterator& other)
	{
		Base::operator=(other);
		return *this;
	}

	SiblingIterator&
	operator++()
	{
		ASSERT(Base::m_current != nullptr);
		Base::m_current = Base::m_current->next;
		return *this;
	}

	SiblingIterator
	operator++(int)
	{
		SiblingIterator copy(*this);
		++(*this);
		return copy;
	}

	SiblingIterator&
	operator--()
	{
		ASSERT(Base::m_current != nullptr);
		Base::m_current = Base::m_current->previous;
		return *this;
	}

	SiblingIterator
	operator--(int)
	{
		SiblingIterator copy(*this);
		--(*this);
		return copy;
	}

};

}

#endif /* TREEITERATORS_H_ */
