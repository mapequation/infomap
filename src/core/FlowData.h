/*
 * FlowData.h
 *
 *  Created on: 24 feb 2015
 *      Author: Daniel
 */

#ifndef SRC_CLUSTERING_FLOWDATA_H_
#define SRC_CLUSTERING_FLOWDATA_H_

#include <ostream>

namespace infomap {

struct FlowData
{
	FlowData(double flow = 1.0) :
		flow(flow),
		enterFlow(0.0),
		exitFlow(0.0)
	{}
	FlowData(const FlowData& other) :
		flow(other.flow),
		enterFlow(other.enterFlow),
		exitFlow(other.exitFlow)
	{}
	FlowData& operator=(const FlowData& other)
	{
		flow = other.flow;
		enterFlow = other.enterFlow;
		exitFlow = other.exitFlow;
		return *this;
	}

	double flow;
	double enterFlow;
	double exitFlow;

	FlowData& operator+=(const FlowData& other)
	{
		flow += other.flow;
		enterFlow += other.enterFlow;
		exitFlow += other.exitFlow;
		return *this;
	}

	FlowData& operator-=(const FlowData& other)
	{
		flow -= other.flow;
		enterFlow -= other.enterFlow;
		exitFlow -= other.exitFlow;
		return *this;
	}

	friend std::ostream& operator<<(std::ostream& out, const FlowData& data)
	{
		return out << "flow: " << data.flow << ", enter: " << data.enterFlow << ", exit: " << data.exitFlow;
	}
};


struct DeltaFlow
{
	DeltaFlow()
	:	module(0),
		deltaExit(0.0),
		deltaEnter(0.0),
		count(0) {}

	DeltaFlow(unsigned int module, double deltaExit, double deltaEnter)
	:	module(module),
		deltaExit(deltaExit),
		deltaEnter(deltaEnter),
		count(0) {}

	DeltaFlow(const DeltaFlow& other) // Copy constructor
	:	module(other.module),
		deltaExit(other.deltaExit),
		deltaEnter(other.deltaEnter),
		count(other.count) {}

	// DeltaFlow& operator=(DeltaFlow other) // Assignment operator (copy-and-swap idiom)
	// {
	// 	swap(*this, other);
	// 	return *this;
	// }
		
	DeltaFlow& operator=(const DeltaFlow& other) // Assignment operator
	{
		module = other.module;
		deltaExit = other.deltaExit;
		deltaEnter = other.deltaEnter;
		count = other.count;
		return *this;
	}

	DeltaFlow& operator +=(const DeltaFlow& other)
	{
		module = other.module;
		deltaExit += other.deltaExit;
		deltaEnter += other.deltaEnter;
		++count;
		return *this;
	}

	// bool operator==(const DeltaFlow& other)
	// {
	// 	return module == other.module;
	// }

	void reset()
	{
		module = 0;
		deltaExit = 0.0;
		deltaEnter = 0.0;
		count = 0;
	}

	friend void swap(DeltaFlow& first, DeltaFlow& second)
	{
		std::swap(first.module, second.module);
		std::swap(first.deltaExit, second.deltaExit);
		std::swap(first.deltaEnter, second.deltaEnter);
		std::swap(first.count, second.count);
	}

	friend std::ostream& operator<<(std::ostream& out, const DeltaFlow& data)
	{
		return out << "module: " << data.module << ", deltaEnter: " << data.deltaEnter << ", deltaExit: " << data.deltaExit << ", count: " << data.count;
	}

	unsigned int module = 0;
	double deltaExit = 0.0;
	double deltaEnter = 0.0;
	unsigned int count = 0;
};

struct MemDeltaFlow : DeltaFlow
{
	MemDeltaFlow()
	:	DeltaFlow(),
		sumDeltaPlogpPhysFlow(0.0),
		sumPlogpPhysFlow(0.0) {}

	MemDeltaFlow(unsigned int module, double deltaExit, double deltaEnter, double sumDeltaPlogpPhysFlow = 0.0, double sumPlogpPhysFlow = 0.0)
	:	DeltaFlow(module, deltaExit, deltaEnter),
		sumDeltaPlogpPhysFlow(sumDeltaPlogpPhysFlow),
		sumPlogpPhysFlow(sumPlogpPhysFlow) {}

	MemDeltaFlow(const MemDeltaFlow& other) // Copy constructor
	:	DeltaFlow(other),
		sumDeltaPlogpPhysFlow(other.sumDeltaPlogpPhysFlow),
		sumPlogpPhysFlow(other.sumPlogpPhysFlow) {}

	MemDeltaFlow& operator=(MemDeltaFlow other) // Assignment operator (copy-and-swap idiom)
	{
		swap(*this, other);
		return *this;
	}

	bool operator==(const MemDeltaFlow& other)
	{
		return module == other.module;
	}

	void reset()
	{
		DeltaFlow::reset();
		sumDeltaPlogpPhysFlow = 0.0;
		sumPlogpPhysFlow = 0.0;
	}

	friend void swap(MemDeltaFlow& first, MemDeltaFlow& second)
	{
		std::swap(first.module, second.module);
		std::swap(first.deltaExit, second.deltaExit);
		std::swap(first.deltaEnter, second.deltaEnter);
		std::swap(first.count, second.count);
		std::swap(first.sumDeltaPlogpPhysFlow, second.sumDeltaPlogpPhysFlow);
		std::swap(first.sumPlogpPhysFlow, second.sumPlogpPhysFlow);
	}

	double sumDeltaPlogpPhysFlow = 0.0;
	double sumPlogpPhysFlow = 0.0;
};


struct PhysData
{
	PhysData(unsigned int physNodeIndex, double sumFlowFromM2Node = 0.0)
	: physNodeIndex(physNodeIndex), sumFlowFromM2Node(sumFlowFromM2Node)
	{}
	PhysData(const PhysData& other) : physNodeIndex(other.physNodeIndex), sumFlowFromM2Node(other.sumFlowFromM2Node) {}
	unsigned int physNodeIndex;
	double sumFlowFromM2Node; // The amount of flow from the memory node in this physical node
};


}



#endif /* SRC_CLUSTERING_FLOWDATA_H_ */
