/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Eriksson, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_AGPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef ITER_WRAPPER_H_
#define ITER_WRAPPER_H_

namespace infomap {

template <typename Iter>
class IterWrapper {
  Iter m_begin, m_end;

public:
  IterWrapper(Iter begin, Iter end) : m_begin(begin), m_end(end)
  {
  }

  template <typename Container>
  IterWrapper(Container& container) : m_begin(container.begin()), m_end(container.end())
  {
  }

  Iter begin()
  {
    return m_begin;
  };

  Iter end()
  {
    return m_end;
  };
};

} // namespace infomap

#endif // ITER_WRAPPER_H_
