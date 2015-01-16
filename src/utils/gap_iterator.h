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


#ifndef GAP_ITERATOR_H_
#define GAP_ITERATOR_H_

#include <iterator>

#ifdef NS_INFOMAP
namespace infomap
{
#endif

//using std::iterator; // Can inherit from std::iterator to automatically forward typedefs
using std::iterator_traits;

template<typename _Iterator>
class gap_iterator
{
public:
	typedef _Iterator												iterator_type;
	typedef typename iterator_traits<_Iterator>::iterator_category	iterator_category;
	typedef typename iterator_traits<_Iterator>::value_type  		value_type;
	typedef typename iterator_traits<_Iterator>::difference_type	difference_type;
	typedef typename iterator_traits<_Iterator>::reference 			reference;
	typedef typename iterator_traits<_Iterator>::pointer   			pointer;

	gap_iterator(iterator_type first,
			iterator_type firstEnd,
			iterator_type second) :
				m_first(first),
				m_firstEnd(firstEnd),
				m_second(second),
				m_isFirst(m_first != m_firstEnd),
				m_current(m_isFirst ? m_first : m_second)
	{ }

	gap_iterator(iterator_type secondEnd) :
		m_first(secondEnd),
		m_firstEnd(secondEnd),
		m_second(secondEnd),
		m_isFirst(false),
		m_current(secondEnd)
	{ }

	/**
	 *  Copy constructor.
	 */
	gap_iterator(const gap_iterator& other) :
		m_first(other.m_first),
		m_firstEnd(other.m_firstEnd),
		m_second(other.m_second),
		m_isFirst(other.m_isFirst),
		m_current(other.m_current)
	{ }

	reference
	operator*() const
	{ return *m_current; }

	pointer
	operator->() const
	{ return m_current.operator->(); }

	gap_iterator&
	operator++()
	{
		++m_current;
		if (m_current == m_firstEnd)
		{
			m_current = m_second;
			m_isFirst = false;
		}
		return *this;
	}

	gap_iterator&
	operator--()
	{
		if (m_current == m_second)
		{
			m_current = m_firstEnd;
			m_isFirst = true;
		}
		--m_current;
		return *this;
	}

	const _Iterator&
	base() const
	{ return m_current; }

	bool isFirst() const
	{ return m_isFirst; }

private:
	iterator_type m_first;
	iterator_type m_firstEnd;
	iterator_type m_second;
	bool m_isFirst;
	_Iterator m_current;
};

// copy from stl_iterator.h
/**
 * Notice:
 * The compare functions need to check that it compares elements from the same
 * iterator, because on the STL iterator, the dummy end element can be equal to
 * a real element on another iterator. It actually happened that the end element
 * on the second iterator was equal to the begin element on the first even though
 * both iterator had length 2, thus this iterator behaved as there was no elements
 * to iterate over!
 */
template<typename _Iterator>
inline bool
operator==(const gap_iterator<_Iterator>& __x,
		const gap_iterator<_Iterator>& __y)
{ return !(__x.isFirst() ^ __y.isFirst()) && (__x.base() == __y.base()); }

template<typename _Iterator>
inline bool
operator<(const gap_iterator<_Iterator>& __x,
		const gap_iterator<_Iterator>& __y)
{ return (__x.isFirst() && !__y.isFirst()) || (__y.base() < __x.base()); }

template<typename _Iterator>
inline bool
operator!=(const gap_iterator<_Iterator>& __x,
		const gap_iterator<_Iterator>& __y)
{ return !(__x == __y); }

template<typename _Iterator>
inline bool
operator>(const gap_iterator<_Iterator>& __x,
		const gap_iterator<_Iterator>& __y)
{ return __y < __x; }

template<typename _Iterator>
inline bool
operator<=(const gap_iterator<_Iterator>& __x,
		const gap_iterator<_Iterator>& __y)
{ return !(__y < __x); }

template<typename _Iterator>
inline bool
operator>=(const gap_iterator<_Iterator>& __x,
		const gap_iterator<_Iterator>& __y)
{ return !(__x < __y); }

#ifdef NS_INFOMAP
}
#endif

#endif /* GAP_ITERATOR_H_ */
