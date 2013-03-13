/* ------------------------------------------------------------------------

 Infomap software package for multi-level network clustering

 * Copyright (c) 2013
 * For license and authors, see README.txt or http://www.mapequation.org

------------------------------------------------------------------------ */

#ifndef EDGE_H_
#define EDGE_H_

struct EdgeData
{
public:
	EdgeData() :
		weight(0.0),
		flow(0.0)
	{}

	EdgeData(double weight) :
		weight (weight),
		flow(weight)
	{}

	EdgeData(double weight, double flow) :
		weight (weight),
		flow(flow)
	{}

	double weight;
	double flow;
};

template <typename node_type>
class Edge
{
public:
	Edge(node_type& source, node_type& target, double weight, double flow)
	:	source(source),
	 	target(target),
	 	data(weight, flow)
	{}
	Edge(Edge const & edge)
	:	source(edge.source),
	 	target(edge.target),
	 	data(edge.data)
	{}

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

#endif /* EDGE_H_ */
