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


#ifndef NODEFACTORY_H_
#define NODEFACTORY_H_
#include <string>
#include "Node.h"
#include "flowData.h"

#ifdef NS_INFOMAP
namespace infomap
{
#endif

class NodeFactoryBase
{
public:
	virtual ~NodeFactoryBase() {}

	virtual NodeBase* createNode(std::string, double flow, double teleWeight = 1.0) const = 0;
	virtual NodeBase* createNode(const NodeBase&) const = 0;
};


template <typename FlowType>
class NodeFactory : public NodeFactoryBase
{
	typedef Node<FlowType> 			node_type;
	typedef const Node<FlowType>	const_node_type;
public:
	NodeBase* createNode(std::string name, double flow, double teleWeight) const
	{
		return new node_type(name, flow, teleWeight);
	}
	NodeBase* createNode(const NodeBase& node) const
	{
		return new node_type(static_cast<const_node_type&>(node));
	}
};

template <typename FlowType>
class MemNodeFactory : public NodeFactoryBase
{
	typedef MemNode<FlowType> 			node_type;
	typedef const MemNode<FlowType>		const_node_type;
public:
	NodeBase* createNode(std::string name, double flow, double teleWeight) const
	{
		return new node_type(name, flow, teleWeight);
	}
	NodeBase* createNode(const NodeBase& node) const
	{
		return new node_type(static_cast<const_node_type&>(node));
	}
};

#ifdef NS_INFOMAP
}
#endif

#endif /* NODEFACTORY_H_ */
