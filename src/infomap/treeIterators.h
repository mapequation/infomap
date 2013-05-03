/**********************************************************************************

 Infomap software package for multi-level network clustering

 Copyright (c) 2013 Daniel Edler, Martin Rosvall
 
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
#include "../utils/Logger.h"
using std::iterator_traits;

/**
 * Base iterator to forward typedefs from the template type.
 * (It uses partial template specialization to give correct semantics both for pointer and non-pointer types.)
 */
template <typename NodePointerType, typename iterator_tag = std::forward_iterator_tag>
struct node_iterator_base
{
//	typedef typename iterator_traits<NodePointerType>::iterator_category	iterator_category; //random_access_iterator_tag
//	typedef std::forward_iterator_tag										iterator_category;
	typedef iterator_tag													iterator_category;
	typedef typename iterator_traits<NodePointerType>::value_type  			value_type;
	typedef typename iterator_traits<NodePointerType>::difference_type		difference_type;
	typedef typename iterator_traits<NodePointerType>::reference			reference;
	typedef typename iterator_traits<NodePointerType>::pointer				pointer;
};

/**
 * Sibling iterator.
 */
template <typename NodePointerType> // pointer or const pointer
//class SiblingIterator : public node_iterator_base<NodePointerType, std::bidirectional_iterator_tag>
class SiblingIterator // Inherited typedefs aren't recognized for some reason?!
{
public:
	typedef SiblingIterator<NodePointerType>								self_type;
	typedef std::bidirectional_iterator_tag									iterator_category;
	typedef typename iterator_traits<NodePointerType>::value_type  			value_type;
	typedef typename iterator_traits<NodePointerType>::difference_type		difference_type;
	typedef typename iterator_traits<NodePointerType>::reference			reference;
	typedef typename iterator_traits<NodePointerType>::pointer				pointer;

	SiblingIterator()
	:	m_current(NodePointerType())
	{}

	explicit
	SiblingIterator(const NodePointerType& nodePointer)
	:	m_current(nodePointer)
	{}

	SiblingIterator(const SiblingIterator& other)
	:	m_current(other.m_current)
	{}

	SiblingIterator & operator= (const SiblingIterator& other)
	{
		m_current = other.m_current;
		return *this;
	}

	pointer base() const
	{ return m_current; }

	reference
	operator*() const
	{ return *m_current; }

	pointer
	operator->() const
	{ return m_current; }


	SiblingIterator&
	operator++()
	{
		ASSERT(m_current != 0);
		m_current = m_current->next;
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
		ASSERT(m_current != 0);
		m_current = m_current->previous;
		return *this;
	}

	SiblingIterator
	operator--(int)
	{
		SiblingIterator copy(*this);
		--(*this);
		return copy;
	}

	bool operator==(const self_type& rhs) const
	{
		return m_current == rhs.m_current;
	}

	bool operator!=(const self_type& rhs) const
	{
		return !(m_current == rhs.m_current);
	}

private:
	NodePointerType m_current;
};

/**
 * Depth-First-Search pre-order iterator
 *
 * Note:
 * This iterator presupposes that the next pointer of a node can't reach a node with a different parent.
 */
template <typename NodePointerType> // NodeBase* or const NodeBase*
class DFSPreOrderIterator
{
public:
	typedef DFSPreOrderIterator<NodePointerType>							self_type;

	// Use iterator_traits<...> to forward correct typedefs.
	//(It uses 'partial template specialization' to give correct semantics both for pointer and non-pointer types.)
//	typedef typename iterator_traits<NodePointerType>::iterator_category	iterator_category; //random_access_iterator_tag
	typedef std::forward_iterator_tag										iterator_category;
	typedef typename iterator_traits<NodePointerType>::value_type  			value_type;
	typedef typename iterator_traits<NodePointerType>::difference_type		difference_type;
	typedef typename iterator_traits<NodePointerType>::reference			reference;
	typedef typename iterator_traits<NodePointerType>::pointer				pointer;

	//	DFSPreOrderIterator();
	//	DFSPreOrderIterator(node_ptr_type nodePointer);

	DFSPreOrderIterator()
	:	m_root(NodePointerType()),
		m_current(m_root),
	 	m_depth(0)
	{}

	explicit
	DFSPreOrderIterator(const NodePointerType& nodePointer)
	:	m_root(nodePointer),
		m_current(nodePointer),
	 	m_depth(0)
	{}

	DFSPreOrderIterator(const DFSPreOrderIterator& other)
	:	m_root(other.m_root),
		m_current(other.m_current),
	 	m_depth(other.m_depth)
	{}

	DFSPreOrderIterator & operator= (const DFSPreOrderIterator& other)
	{
		m_root = other.m_root;
		m_current = other.m_current;
		m_depth = other.m_depth;
		return *this;
	}

	pointer base() const
	{ return m_current; }

	// Forward iterator requirements
	reference
	operator*() const
	{ return *m_current; }

	pointer
	operator->() const
	{ return m_current; }

	DFSPreOrderIterator&
	operator++()
	{
		ASSERT(m_current!=0);
		if(m_current->firstChild != 0)
		{
			m_current = m_current->firstChild;
			++m_depth;
		}
		else
		{
			// The last condition should never be true if the first is not!
//			while(m_current->next == 0 || m_current->next->parent != m_current->parent)
			// Now presupposes that the next pointer can't reach out from the current parent.
			while(m_current->next == 0)
			{
				m_current = m_current->parent;
				if(m_current == m_root || m_current == 0) // 0 if no children in first place
				{
					m_current = 0;
					return *this;
				}
				--m_depth;
			}
			m_current = m_current->next;
		}
		return *this;
	}

	DFSPreOrderIterator
	operator++(int)
	{
		DFSPreOrderIterator copy(*this);
		++(*this);
		return copy;
	}

	DFSPreOrderIterator
	next()
	{
		DFSPreOrderIterator copy(*this);
		return ++copy;
	}

	unsigned int depth() const
	{
		return m_depth;
	}

	bool operator==(const self_type& rhs) const
	{
		return m_current == rhs.m_current;
	}

	bool operator!=(const self_type& rhs) const
	{
		return !(m_current == rhs.m_current);
	}

private:
	NodePointerType m_root;
	NodePointerType m_current;
	unsigned int m_depth;
};



template <typename NodePointerType> // NodeBase* or const NodeBase*
class LeafNodeIterator
{
public:
	typedef LeafNodeIterator<NodePointerType>							self_type;

	// Use iterator_traits<...> to forward correct typedefs. (It uses 'partial template specialization' to give correct semantics both for pointer and non-pointer types.)
	//	typedef typename iterator_traits<NodePointerType>::iterator_category	iterator_category; //random_access_iterator_tag
	typedef std::forward_iterator_tag										iterator_category;
	typedef typename iterator_traits<NodePointerType>::value_type  			value_type;
	typedef typename iterator_traits<NodePointerType>::difference_type		difference_type;
	typedef typename iterator_traits<NodePointerType>::reference			reference;
	typedef typename iterator_traits<NodePointerType>::pointer				pointer;

	//	DFSPreOrderIterator();
	//	DFSPreOrderIterator(node_ptr_type nodePointer);

	LeafNodeIterator()
	:	m_current(NodePointerType()),
	 	m_depth(0)
	{}

	explicit
	LeafNodeIterator(const NodePointerType& nodePointer)
	:	m_current(nodePointer),
	 	m_depth(0)
	{
		if (m_current != 0)
		{
			while(m_current->firstChild != 0)
			{
				m_current = m_current->firstChild;
				++m_depth;
			}
		}
	}



	LeafNodeIterator(const LeafNodeIterator& other)
	:	m_current(other.m_current),
	 	m_depth(other.m_depth)
	{}

	LeafNodeIterator & operator= (const LeafNodeIterator& other)
	{
		m_current = other.m_current;
		m_depth = other.m_depth;
		return *this;
	}

	pointer base() const
	{ return m_current; }

	// Forward iterator requirements
	reference
	operator*() const
	{ return *m_current; }

	pointer
	operator->() const
	{ return m_current; }

	LeafNodeIterator&
	operator++()
	{
		ASSERT(m_current!=0);
		while(m_current->next == 0 || m_current->next->parent != m_current->parent)
		{
			m_current = m_current->parent;
			--m_depth;
			if(m_current == 0)
				return *this;
		}

		m_current = m_current->next;

		if (m_current != 0)
		{
			while(m_current->firstChild != 0)
			{
				m_current = m_current->firstChild;
				++m_depth;
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

	unsigned int depth() const
	{
		return m_depth;
	}

	bool operator==(const self_type& rhs) const
	{
		return m_current == rhs.m_current;
	}

	bool operator!=(const self_type& rhs) const
	{
		return !(m_current == rhs.m_current);
	}

private:
	NodePointerType m_current;
	unsigned int m_depth;
};


//
//inline bool
//operator==(const DFSPreOrderIterator& a, const DFSPreOrderIterator& b)
//	{ return a.base() == b.base(); }
//
//inline bool
//operator<(const DFSPreOrderIterator& a, const DFSPreOrderIterator& b)
//	{ return a.base() < b.base(); }
//
//inline bool
//operator!=(const DFSPreOrderIterator& a, const DFSPreOrderIterator& b)
//	{ return !(a.base() == b.base()); }
//
//inline bool
//operator>(const DFSPreOrderIterator& a, const DFSPreOrderIterator& b)
//	{ return b.base() < a.base(); }
//
//inline bool
//operator<=(const DFSPreOrderIterator& a, const DFSPreOrderIterator& b)
//	{ return !(b.base() < a.base()); }
//
//inline bool
//operator>=(const DFSPreOrderIterator& a, const DFSPreOrderIterator& b)
//	{ return !(a.base() < b.base()); }


//DFSPreOrderIterator::DFSPreOrderIterator()
//:	m_current(value_type())
//{
//}
//
//DFSPreOrderIterator::DFSPreOrderIterator(node_ptr_type nodePointer)
//:	m_current(nodePointer)
//{
//}



//
//using std::iterator_traits;
// using std::iterator;
// template<typename _Iterator, typename _Container>
//   class __normal_iterator
//   {
//   protected:
//     _Iterator _M_current;
//
//   public:
//     typedef typename iterator_traits<_Iterator>::iterator_category
//                                                            iterator_category;
//     typedef typename iterator_traits<_Iterator>::value_type  value_type;
//     typedef typename iterator_traits<_Iterator>::difference_type
//                                                            difference_type;
//     typedef typename iterator_traits<_Iterator>::reference reference;
//     typedef typename iterator_traits<_Iterator>::pointer   pointer;
//
//     __normal_iterator() : _M_current(_Iterator()) { }
//
//     explicit
//     __normal_iterator(const _Iterator& __i) : _M_current(__i) { }
//
//     // Allow iterator to const_iterator conversion
//     template<typename _Iter>
//       __normal_iterator(const __normal_iterator<_Iter,
//			  typename __enable_if<
//     	       (std::__are_same<_Iter, typename _Container::pointer>::__value),
//		      _Container>::__type>& __i)
//       : _M_current(__i.base()) { }
//
//     // Forward iterator requirements
//     reference
//     operator*() const
//     { return *_M_current; }
//
//     pointer
//     operator->() const
//     { return _M_current; }
//
//     __normal_iterator&
//     operator++()
//     {
//	++_M_current;
//	return *this;
//     }
//
//     __normal_iterator
//     operator++(int)
//     { return __normal_iterator(_M_current++); }
//
//     // Bidirectional iterator requirements
//     __normal_iterator&
//     operator--()
//     {
//	--_M_current;
//	return *this;
//     }
//
//     __normal_iterator
//     operator--(int)
//     { return __normal_iterator(_M_current--); }
//
//     // Random access iterator requirements
//     reference
//     operator[](const difference_type& __n) const
//     { return _M_current[__n]; }
//
//     __normal_iterator&
//     operator+=(const difference_type& __n)
//     { _M_current += __n; return *this; }
//
//     __normal_iterator
//     operator+(const difference_type& __n) const
//     { return __normal_iterator(_M_current + __n); }
//
//     __normal_iterator&
//     operator-=(const difference_type& __n)
//     { _M_current -= __n; return *this; }
//
//     __normal_iterator
//     operator-(const difference_type& __n) const
//     { return __normal_iterator(_M_current - __n); }
//
//     const _Iterator&
//     base() const
//     { return _M_current; }
//   };
//
//


//// stl_list.h
///**
//   *  @brief A list::iterator.
//   *
//   *  @if maint
//   *  All the functions are op overloads.
//   *  @endif
//  */
//  template<typename _Tp>
//    struct DepthFirstPreOrderIterator
//    {
//      typedef DepthFirstPreOrderIterator<_Tp>                _Self;
//      typedef _List_node<_Tp>                    _Node;
//
//      typedef ptrdiff_t                          difference_type;
//      typedef std::bidirectional_iterator_tag    iterator_category;
//      typedef _Tp                                value_type;
//      typedef _Tp*                               pointer;
//      typedef _Tp&                               reference;
//
//      _List_iterator()
//      : _M_node() { }
//
//      explicit
//      _List_iterator(_List_node_base* __x)
//      : _M_node(__x) { }
//
//      // Must downcast from List_node_base to _List_node to get to _M_data.
//      reference
//      operator*() const
//      { return static_cast<_Node*>(_M_node)->_M_data; }
//
//      pointer
//      operator->() const
//      { return &static_cast<_Node*>(_M_node)->_M_data; }
//
//      _Self&
//      operator++()
//      {
//	_M_node = _M_node->_M_next;
//	return *this;
//      }
//
//      _Self
//      operator++(int)
//      {
//	_Self __tmp = *this;
//	_M_node = _M_node->_M_next;
//	return __tmp;
//      }
//
//      _Self&
//      operator--()
//      {
//	_M_node = _M_node->_M_prev;
//	return *this;
//      }
//
//      _Self
//      operator--(int)
//      {
//	_Self __tmp = *this;
//	_M_node = _M_node->_M_prev;
//	return __tmp;
//      }
//
//      bool
//      operator==(const _Self& __x) const
//      { return _M_node == __x._M_node; }
//
//      bool
//      operator!=(const _Self& __x) const
//      { return _M_node != __x._M_node; }
//
//      // The only member points to the %list element.
//      _List_node_base* _M_node;
//    };
//
//  /**
//   *  @brief A list::const_iterator.
//   *
//   *  @if maint
//   *  All the functions are op overloads.
//   *  @endif
//  */
//  template<typename _Tp>
//    struct _List_const_iterator
//    {
//      typedef _List_const_iterator<_Tp>          _Self;
//      typedef const _List_node<_Tp>              _Node;
//      typedef _List_iterator<_Tp>                iterator;
//
//      typedef ptrdiff_t                          difference_type;
//      typedef std::bidirectional_iterator_tag    iterator_category;
//      typedef _Tp                                value_type;
//      typedef const _Tp*                         pointer;
//      typedef const _Tp&                         reference;
//
//      _List_const_iterator()
//      : _M_node() { }
//
//      explicit
//      _List_const_iterator(const _List_node_base* __x)
//      : _M_node(__x) { }
//
//      _List_const_iterator(const iterator& __x)
//      : _M_node(__x._M_node) { }
//
//      // Must downcast from List_node_base to _List_node to get to
//      // _M_data.
//      reference
//      operator*() const
//      { return static_cast<_Node*>(_M_node)->_M_data; }
//
//      pointer
//      operator->() const
//      { return &static_cast<_Node*>(_M_node)->_M_data; }
//
//      _Self&
//      operator++()
//      {
//	_M_node = _M_node->_M_next;
//	return *this;
//      }
//
//      _Self
//      operator++(int)
//      {
//	_Self __tmp = *this;
//	_M_node = _M_node->_M_next;
//	return __tmp;
//      }
//
//      _Self&
//      operator--()
//      {
//	_M_node = _M_node->_M_prev;
//	return *this;
//      }
//
//      _Self
//      operator--(int)
//      {
//	_Self __tmp = *this;
//	_M_node = _M_node->_M_prev;
//	return __tmp;
//      }
//
//      bool
//      operator==(const _Self& __x) const
//      { return _M_node == __x._M_node; }
//
//      bool
//      operator!=(const _Self& __x) const
//      { return _M_node != __x._M_node; }
//
//      // The only member points to the %list element.
//      const _List_node_base* _M_node;
//    };
//
//  template<typename _Val>
//    inline bool
//    operator==(const _List_iterator<_Val>& __x,
//	       const _List_const_iterator<_Val>& __y)
//    { return __x._M_node == __y._M_node; }
//
//  template<typename _Val>
//    inline bool
//    operator!=(const _List_iterator<_Val>& __x,
//               const _List_const_iterator<_Val>& __y)
//    { return __x._M_node != __y._M_node; }




#endif /* TREEITERATORS_H_ */
