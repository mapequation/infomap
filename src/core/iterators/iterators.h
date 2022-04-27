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

#ifndef ITERATORS_H_
#define ITERATORS_H_

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

#endif // ITERATORS_H_
