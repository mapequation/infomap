/*
 * InfoEdge.h
 *
 *  Created on: 26 feb 2015
 *      Author: Daniel
 */

#ifndef MODULES_CLUSTERING_CLUSTERING_INFOEDGE_H_
#define MODULES_CLUSTERING_CLUSTERING_INFOEDGE_H_

namespace infomap {

struct EdgeData {
public:
  EdgeData() : weight(0.0), flow(0.0) {}

  EdgeData(double weight) : weight(weight), flow(weight) {}

  EdgeData(double weight, double flow) : weight(weight), flow(flow) {}

  double weight;
  double flow;
};

template <typename node_type>
class Edge {
public:
  Edge(node_type& source, node_type& target, double weight, double flow)
      : source(source),
        target(target),
        data(weight, flow) {}

  Edge(Edge const& edge)
      : source(edge.source),
        target(edge.target),
        data(edge.data) {}

  node_type& other(node_type& node)
  {
    return (node == source) ? target : source;
  }

  bool isSelfPointing()
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


#endif /* MODULES_CLUSTERING_CLUSTERING_INFOEDGE_H_ */
