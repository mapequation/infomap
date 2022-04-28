/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Eriksson, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_AGPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef INFOEDGE_H_
#define INFOEDGE_H_

namespace infomap {

struct EdgeData {
public:
  EdgeData() = default;

  EdgeData(double weight, double flow) : weight(weight), flow(flow) { }

  double weight;
  double flow;
};

template <typename node_type>
class Edge {
public:
  Edge(node_type& source, node_type& target, double weight, double flow)
      : data(weight, flow),
        source(source),
        target(target) { }

  Edge(Edge const& edge)
      : data(edge.data),
        source(edge.source),
        target(edge.target) { }

  node_type& other(node_type& node)
  {
    return (node == source) ? target : source;
  }

  friend std::ostream& operator<<(std::ostream& out, const Edge& edge)
  {
    return out << "(" << edge.source << ") -> (" << edge.target << "), flow: "
               << edge.data.flow;
  }

  EdgeData data;
  node_type& source;
  node_type& target;
};

} // namespace infomap

#endif // INFOEDGE_H_
