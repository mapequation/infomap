/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

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
  IterWrapper(Iter begin, Iter end) : m_begin(begin), m_end(end) { }

  template <typename Container>
  IterWrapper(Container& container) : m_begin(container.begin()), m_end(container.end()) { }

  Iter begin() noexcept { return m_begin; };

  Iter end() noexcept { return m_end; };
};

/**
 * Concatenate two containers for sequential iteration
 *
 * Usage:
  std::vector<int> v1 = {1,2,3};
  std::vector<int> v2 = {4,5,6};

  std::cout << "All items:\n";
  for (auto& v : Concat(v1, v2)) {
    std::cout << v << " ";
  }
  std::cout << "Done!" << std::endl;
  # Outputs "1 2 3 4 5 6 Done!"
*/
template <typename Container>
class Concat {
public:
  using T = typename Container::value_type;
  using iterator = typename Container::iterator;

protected:
  Container& m_first;
  Container& m_second;

public:
  class IterConcat {
    using T = typename Container::value_type;
    using Iter = typename Container::iterator;
    Container* m_first = nullptr;
    Container* m_second = nullptr;
    iterator m_current;
    iterator m_end;
    bool m_isFirst = true;
    bool m_isEnd = false;

  private:
    IterConcat(Container* cont2) : m_second(cont2)
    {
      m_end = m_second->end();
      m_current = m_end;
      m_isEnd = true;
    }

  public:
    IterConcat(Container* cont1, Container* cont2) : m_first(cont1), m_second(cont2)
    {
      if (cont1->empty()) {
        switchIterator();
      } else {
        m_current = m_first->begin();
        m_end = m_first->end();
        if (m_current == m_end)
          m_isEnd = true;
      }
    }

    static IterConcat make_end(Container* cont2)
    {
      return IterConcat(cont2);
    }

    void switchIterator() noexcept
    {
      m_isFirst = false;
      m_current = m_second->begin();
      m_end = m_second->end();
      if (m_current == m_end)
        m_isEnd = true;
    }

    IterConcat& operator++() noexcept
    {
      ++m_current;
      if (m_current == m_end) {
        if (m_isFirst)
          switchIterator();
        else
          m_isEnd = true;
      }
      return *this;
    }

    IterConcat operator++(int) noexcept
    {
      IterConcat copy(*this);
      ++(*this);
      return std::move(copy);
    }

    T& operator*() noexcept { return *m_current; }

    const T& operator*() const noexcept { return *m_current; }

    T* operator->() noexcept { return m_current; }

    const T* operator->() const noexcept { return m_current; }

    bool operator!=(IterConcat& other) noexcept { return m_current != other.m_current; }
    bool isEnd() noexcept { return m_isEnd; }
  };

  Concat(Container& cont1, Container& cont2) : m_first(cont1), m_second(cont2) { }

  IterConcat begin() noexcept { return IterConcat(&m_first, &m_second); };
  IterConcat end() noexcept { return IterConcat::make_end(&m_second); };
};

} // namespace infomap

#endif // ITER_WRAPPER_H_
