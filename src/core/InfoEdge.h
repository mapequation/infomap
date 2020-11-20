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
  using FlowType = std::pair<double, double>;
public:
  EdgeData() : weight(0.0), flow({}) {}

  // ToDo[chris] how should the init look like? is it even used?
  //EdgeData(double weight) : weight(weight), flow(weight) {}

  EdgeData(double weight, FlowType flow) : weight(weight), flow(flow) {}

  double weight;
  FlowType flow{};
};

template <typename node_type>
class Edge {
  using FlowType = std::pair<double, double>;
public:
  Edge(node_type& source, node_type& target, double weight, FlowType flow)
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
