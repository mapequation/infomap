/*
 * FlowData.h
 */

#ifndef SRC_CLUSTERING_FLOWDATA_H_
#define SRC_CLUSTERING_FLOWDATA_H_

#include <ostream>

namespace infomap {

struct FlowData
{
	double flow;
	double enterFlow;
	double exitFlow;
	int moduleSize;

	FlowData(double flow = 1.0, double enterFlow = 0.0, double exitFlow = 0.0) :
		flow(flow),
		enterFlow(enterFlow),
		exitFlow(exitFlow),
		moduleSize(0)
	{}
	FlowData(const FlowData& other) :
		flow(other.flow),
		enterFlow(other.enterFlow),
		exitFlow(other.exitFlow),
		moduleSize(other.moduleSize)
	{}
	FlowData& operator=(const FlowData& other)
	{
		flow = other.flow;
		enterFlow = other.enterFlow;
		exitFlow = other.exitFlow;
		moduleSize = other.moduleSize;
		return *this;
	}

	FlowData& operator+=(const FlowData& other)
	{
		flow += other.flow;
		enterFlow += other.enterFlow;
		exitFlow += other.exitFlow;
		moduleSize += other.moduleSize;
		return *this;
	}

	FlowData& operator-=(const FlowData& other)
	{
		flow -= other.flow;
		enterFlow -= other.enterFlow;
		exitFlow -= other.exitFlow;
		moduleSize -= other.moduleSize;
		return *this;
	}

	void resetFlow() { flow = 0; }

	void setFlow(double f) { flow = f; }
	void setFlow(unsigned int f) {}

	void addFlow(double f) { flow += f; }
	void addFlow(unsigned int f) {}

	void setEnterFlow(double f) { enterFlow = f; }
	void setExitFlow(double f) { exitFlow = f; }
	void setEnterExitFlow(unsigned int f) {}

	void addEnterFlow(double f) { enterFlow += f; }
	void addExitFlow(double f) { exitFlow += f; }
	void addEnterExitFlow(unsigned int f) {}

	void setModuleSize(int f) { moduleSize = f; }
	void addModuleSize(int f) { moduleSize += f; }

	double getFlow() const { return flow; }
	double getEnterFlow() const { return enterFlow; }
	double getExitFlow() const { return exitFlow; }
	unsigned int getFlowInt() const { return 0; }
	unsigned int getEnterExitFlow() const { return 0; }
	int getModuleSize() const { return moduleSize; }

	FlowData getFlowData() const { return *this; }

	friend std::ostream& operator<<(std::ostream& out, const FlowData& data)
	{
		return out << "flow: " << data.flow << ", enter: " << data.enterFlow << ", exit: " << data.exitFlow;
	}
};

struct FlowDataInt
{
	unsigned int flow;
	unsigned int enterExitFlow;
	int moduleSize;

	FlowDataInt(unsigned int flow = 0) :
		flow(flow),
		enterExitFlow(0),
		moduleSize(0)
	{}
	FlowDataInt(const FlowDataInt& other) :
		flow(other.flow),
		enterExitFlow(other.enterExitFlow),
		moduleSize(other.moduleSize)
	{}
	FlowDataInt& operator=(const FlowDataInt& other)
	{
		flow = other.flow;
		enterExitFlow = other.enterExitFlow;
		moduleSize = other.moduleSize;
		return *this;
	}

	FlowDataInt& operator+=(const FlowDataInt& other)
	{
		flow += other.flow;
		enterExitFlow += other.enterExitFlow;
		moduleSize += other.moduleSize;
		return *this;
	}

	FlowDataInt& operator-=(const FlowDataInt& other)
	{
		flow -= other.flow;
		enterExitFlow -= other.enterExitFlow;
		moduleSize -= other.moduleSize;
		return *this;
	}

	void resetFlow() { flow = 0; }

	void setFlow(double f) { }
	void setFlow(unsigned int f) { flow = f; }

	void addFlow(double f) {}
	void addFlow(unsigned int f) { flow += f; }

	void setEnterFlow(double f) { }
	void setExitFlow(double f) { }
	void setEnterExitFlow(unsigned int f) { enterExitFlow = f; }

	void addEnterFlow(double f) { }
	void addExitFlow(double f) { }
	void addEnterExitFlow(unsigned int f) { enterExitFlow += f; }

	void setModuleSize(int f) { moduleSize = f; }
	void addModuleSize(int f) { moduleSize += f; }

	double getFlow() const { return flow; }
	double getEnterFlow() const { return enterExitFlow; }
	double getExitFlow() const { return enterExitFlow; }
	unsigned int getFlowInt() const { return flow; }
	unsigned int getEnterExitFlow() const { return enterExitFlow; }
	int getModuleSize() const { return moduleSize; }
	FlowData getFlowData() const { return FlowData(flow, enterExitFlow, enterExitFlow); }

	friend std::ostream& operator<<(std::ostream& out, const FlowDataInt& data)
	{
		return out << "flow: " << data.flow << ", enter/exit: " << data.enterExitFlow;
	}
};


struct DeltaFlow
{
	unsigned int module = 0;
	double deltaExit = 0.0;
	double deltaEnter = 0.0;
	unsigned int count = 0;

	virtual ~DeltaFlow() = default;

	DeltaFlow() {}

	explicit DeltaFlow(unsigned int module, double deltaExit, double deltaEnter)
	:	module(module),
		deltaExit(deltaExit),
		deltaEnter(deltaEnter),
		count(0) {}

	DeltaFlow(const DeltaFlow&) = default;
    DeltaFlow& operator=(const DeltaFlow&) = default;
    DeltaFlow(DeltaFlow&&) = default;
    DeltaFlow& operator=(DeltaFlow&&) = default;

	DeltaFlow& operator+=(const DeltaFlow& other)
	{
		module = other.module;
		deltaExit += other.deltaExit;
		deltaEnter += other.deltaEnter;
		++count;
		return *this;
	}

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
};

struct DeltaFlowInt
{
	unsigned int module = 0;
	unsigned int deltaExit = 0;
	unsigned int deltaEnter = 0;
	unsigned int count = 0;

	virtual ~DeltaFlowInt() = default;

	DeltaFlowInt() {}

	explicit DeltaFlowInt(unsigned int module, unsigned int deltaExit, unsigned int deltaEnter)
	:	module(module),
		deltaExit(deltaExit),
		deltaEnter(deltaEnter),
		count(0) {}

	DeltaFlowInt(const DeltaFlowInt&) = default;
    DeltaFlowInt& operator=(const DeltaFlowInt&) = default;
    DeltaFlowInt(DeltaFlowInt&&) = default;
    DeltaFlowInt& operator=(DeltaFlowInt&&) = default;

	DeltaFlowInt& operator+=(const DeltaFlowInt& other)
	{
		module = other.module;
		deltaExit += other.deltaExit;
		deltaEnter += other.deltaEnter;
		++count;
		return *this;
	}

	void reset()
	{
		module = 0;
		deltaExit = 0;
		deltaEnter = 0;
		count = 0;
	}

	friend void swap(DeltaFlowInt& first, DeltaFlowInt& second)
	{
		std::swap(first.module, second.module);
		std::swap(first.deltaExit, second.deltaExit);
		std::swap(first.deltaEnter, second.deltaEnter);
		std::swap(first.count, second.count);
	}

	friend std::ostream& operator<<(std::ostream& out, const DeltaFlowInt& data)
	{
		return out << "module: " << data.module << ", deltaEnter: " << data.deltaEnter << ", deltaExit: " << data.deltaExit << ", count: " << data.count;
	}
};

struct MemDeltaFlow : DeltaFlow
{
	double sumDeltaPlogpPhysFlow = 0.0;
	double sumPlogpPhysFlow = 0.0;
	
	MemDeltaFlow() : DeltaFlow() {}

	explicit MemDeltaFlow(unsigned int module, double deltaExit, double deltaEnter, double sumDeltaPlogpPhysFlow = 0.0, double sumPlogpPhysFlow = 0.0)
	:	DeltaFlow(module, deltaExit, deltaEnter),
		sumDeltaPlogpPhysFlow(sumDeltaPlogpPhysFlow),
		sumPlogpPhysFlow(sumPlogpPhysFlow) {}

	MemDeltaFlow& operator+=(const MemDeltaFlow& other)
	{
		DeltaFlow::operator+=(other);
		sumDeltaPlogpPhysFlow += other.sumDeltaPlogpPhysFlow;
		sumPlogpPhysFlow += other.sumPlogpPhysFlow;
		return *this;
	}

	void reset()
	{
		DeltaFlow::reset();
		sumDeltaPlogpPhysFlow = 0.0;
		sumPlogpPhysFlow = 0.0;
	}

	friend void swap(MemDeltaFlow& first, MemDeltaFlow& second)
	{
        swap(static_cast<DeltaFlow&>(first), static_cast<DeltaFlow&>(second));
		std::swap(first.sumDeltaPlogpPhysFlow, second.sumDeltaPlogpPhysFlow);
		std::swap(first.sumPlogpPhysFlow, second.sumPlogpPhysFlow);
	}

	friend std::ostream& operator<<(std::ostream& out, const MemDeltaFlow& data)
	{
		return out << "module: " << data.module << ", deltaEnter: " << data.deltaEnter <<
		", deltaExit: " << data.deltaExit << ", count: " << data.count <<
		", sumDeltaPlogpPhysFlow: " << data.sumDeltaPlogpPhysFlow <<
		", sumPlogpPhysFlow: " << data.sumPlogpPhysFlow;
	}
};


struct PhysData
{
	PhysData(unsigned int physNodeIndex, double sumFlowFromM2Node = 0.0)
	: physNodeIndex(physNodeIndex), sumFlowFromM2Node(sumFlowFromM2Node)
	{}
	PhysData(const PhysData& other) : physNodeIndex(other.physNodeIndex), sumFlowFromM2Node(other.sumFlowFromM2Node) {}
	unsigned int physNodeIndex;
	double sumFlowFromM2Node; // The amount of flow from the memory node in this physical node
	
	friend std::ostream& operator<<(std::ostream& out, const PhysData& data)
	{
		return out << "physNodeIndex: " << data.physNodeIndex <<
		", sumFlowFromM2Node: " << data.sumFlowFromM2Node;
	}
};


}



#endif /* SRC_CLUSTERING_FLOWDATA_H_ */
