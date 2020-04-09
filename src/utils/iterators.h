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

#endif //ITERATORS_H_
