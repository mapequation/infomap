/* ------------------------------------------------------------------------

 Infomap software package for multi-level network clustering

 * Copyright (c) 2013
 * For license and authors, see README.txt or http://www.mapequation.org

------------------------------------------------------------------------ */

#ifndef NODEFACTORY_H_
#define NODEFACTORY_H_
#include <string>
#include "Node.h"
#include "flowData.h"

class NodeFactoryBase
{
public:
	virtual ~NodeFactoryBase() {}

	virtual NodeBase* createNode(std::string, double flow, double teleWeight = 1.0) = 0;
	virtual NodeBase* createNode(const NodeBase&) = 0;
};


template <typename FlowType>
class NodeFactory : public NodeFactoryBase
{
	typedef Node<FlowType> 			node_type;
	typedef const Node<FlowType>	const_node_type;
public:
	NodeBase* createNode(std::string name, double flow, double teleWeight)
	{
		return new node_type(name, flow, teleWeight);
	}
	NodeBase* createNode(const NodeBase& node)
	{
		return new node_type(static_cast<const_node_type&>(node));
	}
};

#endif /* NODEFACTORY_H_ */
