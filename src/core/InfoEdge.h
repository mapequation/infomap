/*
 * InfoEdge.h
 *
 *  Created on: 26 feb 2015
 *      Author: Daniel
 */

#ifndef _INFOEDGE_H_
#define _INFOEDGE_H_

namespace infomap {

struct EdgeData {
public:
  EdgeData(double weight, double flow) : weight(weight), flow(flow) { }

  double weight = 0.0;
  double flow = 0.0;
};

template <typename node_type>
class Edge {
public:
  Edge(node_type& source, node_type& target, double weight, double flow)
      : source(source),
        target(target),
        data(weight, flow) {}

  Edge(const Edge& edge)
      : source(edge.source),
        target(edge.target),
        data(edge.data) {}

  node_type& other(node_type& node) const
  {
    return (node == source) ? target : source;
  }

  bool isSelfPointing() const
  {
    return source == target;
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


#endif /* _INFOEDGE_H_ */
