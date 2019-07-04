#ifndef INFO_NODE_WITH_DATA_H_
#define INFO_NODE_WITH_DATA_H_

#include "NodeBase.h"
#include "FlowData.h"

namespace infomap {

template<typename FlowDataType>
class Node : public NodeBase
{
public:

	using NodeBaseType = Node<FlowDataType>;

	FlowDataType data;

	Node() : NodeBase() {};

	Node(const FlowDataType& flowData)
	: NodeBase(), data(flowData) {};

	// For first order nodes, physicalId equals stateId
	Node(const FlowDataType& flowData, unsigned int stateId)
	: NodeBase(stateId, stateId), data(flowData) {};

	Node(const FlowDataType& flowData, unsigned int stateId, unsigned int physicalId)
	: NodeBase(stateId, physicalId), data(flowData) {};

	Node(const FlowDataType& flowData, unsigned int stateId, unsigned int physicalId, unsigned int layerId)
	: NodeBase(stateId, physicalId, layerId), data(flowData) {};

	Node(const Node& other)
	:	NodeBase(other), data(other.data)
	{}


	virtual ~Node() {}

	Node& operator=(const Node& other)
	{
		NodeBase::operator=(other);
		data = other.data;
		return *this;
	}

	// ---------------------------- Data ----------------------------

	NodeBaseType& getNode(NodeBase& other) {
		return static_cast<NodeBaseType&>(other);
	}

	virtual void copyData(NodeBase& other) {
		data = getNode(other).data;
	}

	virtual void resetFlow() {
		data.resetFlow();
	}
	virtual void setFlow(double flow) {
		data.setFlow(flow);
	}
	virtual void setFlow(unsigned int flow) {
		data.setFlow(flow);
	}
	virtual void addFlow(double flow) {
		data.addFlow(flow);
	}
	virtual void addFlow(unsigned int flow) {
		data.addFlow(flow);
	}
	virtual void setEnterFlow(double flow) {
		data.setEnterFlow(flow);
	}
	virtual void setExitFlow(double flow) {
		data.setExitFlow(flow);
	}
	virtual void setEnterExitFlow(unsigned int flow) {
		data.setEnterExitFlow(flow);
	}
	virtual void addEnterFlow(double flow) {
		data.addEnterFlow(flow);
	}
	virtual void addExitFlow(double flow) {
		data.addExitFlow(flow);
	}
	virtual void addEnterExitFlow(unsigned int flow) {
		data.addEnterExitFlow(flow);
	}
	virtual double getFlow() const {
		return data.getFlow();
	}
	virtual double getEnterFlow() const {
		return data.getEnterFlow();
	}
	virtual double getExitFlow() const {
		return data.getExitFlow();
	}
	virtual unsigned int getFlowInt() const {
		return data.getFlowInt();
	}
	virtual unsigned int getEnterExitFlow() const {
		return data.getEnterExitFlow();
	}

	virtual FlowData getFlowData() const {
		return data.getFlowData();
	};

};

}

#endif /* INFO_NODE_WITH_DATA_H_ */
