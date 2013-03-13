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

class NodeFactory
{
public:
	virtual ~NodeFactory() {}

	virtual NodeBase* createNode(std::string, double nodeWeight) = 0;
	virtual NodeBase* createNode(const NodeBase&) = 0;
};


template <typename FlowType>
class NodeFactoryTyped : public NodeFactory
{
	typedef Node<FlowType> 			node_type;
	typedef const Node<FlowType>	const_node_type;
public:
	NodeBase* createNode(std::string name, double nodeWeight)
	{
		return new node_type(name, nodeWeight);
	}
	NodeBase* createNode(const NodeBase& node)
	{
		return new node_type(static_cast<const_node_type&>(node));
	}
};

#endif /* NODEFACTORY_H_ */
