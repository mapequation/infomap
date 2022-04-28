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
  EdgeData() : weight(0.0), flow(0.0) { }

  EdgeData(double weight, double flow) : weight(weight), flow(flow) { }

  double weight;
  double flow;
};

template <typename node_type>
class Edge {
public:
  Edge(node_type& source, node_type& target, double weight, double flow)
      : source(source),
        target(target),
        data(weight, flow) { }

  Edge(Edge const& edge)
      : source(edge.source),
        target(edge.target),
        data(edge.data) { }

  node_type& other(node_type& node)
  {
    return (node == source) ? target : source;
  }

  friend std::ostream& operator<<(std::ostream& out, const Edge& edge)
  {
    return out << "(" << edge.source << ") -> (" << edge.target << "), flow: "
               << edge.data.flow;
  }

  node_type& source;
  node_type& target;
  EdgeData data;
};

} // namespace infomap

#endif // INFOEDGE_H_
