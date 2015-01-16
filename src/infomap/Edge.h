/**********************************************************************************

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

**********************************************************************************/


#ifndef EDGE_H_
#define EDGE_H_

#include <ostream>

#ifdef NS_INFOMAP
namespace infomap
{
#endif

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

#ifdef NS_INFOMAP
}
#endif

#endif /* EDGE_H_ */
