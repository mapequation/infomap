/* ------------------------------------------------------------------------

 Infomap software package for multi-level network clustering

 * Copyright (c) 2013
 * For license and authors, see README.txt or http://www.mapequation.org

------------------------------------------------------------------------ */

#ifndef FLOWDATA_H_
#define FLOWDATA_H_
#include <ostream>

class FlowUndirected
{
public:
	FlowUndirected(double flow = 1.0, double teleportWeight = 1.0) :
		flow(flow),
		exitFlow(0.0),
		enterFlow(exitFlow)
	{}
	FlowUndirected(const FlowUndirected& other) :
		flow(other.flow),
		exitFlow(other.exitFlow),
		enterFlow(exitFlow)
	{}
	FlowUndirected& operator=(const FlowUndirected& other)
	{
		flow = other.flow;
		exitFlow = other.exitFlow;
		return *this;
	}

	double flow;
	double exitFlow;
	double& enterFlow;

	friend std::ostream& operator<<(std::ostream& out, const FlowUndirected& data)
	{
		return out << "flow: " << data.flow << ", exit: " << data.exitFlow;
	}
};

class FlowDirected
{
public:
	FlowDirected(double flow = 1.0, double teleportWeight = 1.0) :
		flow(flow),
		exitFlow(0.0),
		enterFlow(exitFlow)
	{}
	FlowDirected(const FlowDirected& other) :
		flow(other.flow),
		exitFlow(other.exitFlow),
		enterFlow(exitFlow)
	{}
	FlowDirected& operator=(const FlowDirected& other)
	{
		flow = other.flow;
		exitFlow = other.exitFlow;
		return *this;
	}

	double flow;
	double exitFlow;
	double& enterFlow;

	friend std::ostream& operator<<(std::ostream& out, const FlowDirected& data)
	{
		return out << "flow: " << data.flow << ", exit: " << data.exitFlow;
	}
};

class FlowDirectedWithTeleportation
{
public:
	FlowDirectedWithTeleportation(double flow = 1.0, double teleportWeight = 1.0) :
		flow(flow),
		exitFlow(0.0),
		enterFlow(exitFlow),
		teleportWeight(teleportWeight),
		danglingFlow(0.0),
		teleportSourceFlow(0.0)
	{}
	FlowDirectedWithTeleportation(const FlowDirectedWithTeleportation& other) :
		flow(other.flow),
		exitFlow(other.exitFlow),
		enterFlow(exitFlow),
		teleportWeight(other.teleportWeight),
		danglingFlow(other.danglingFlow),
		teleportSourceFlow(other.teleportSourceFlow)
	{}
	FlowDirectedWithTeleportation& operator=(const FlowDirectedWithTeleportation& other)
	{
		flow = other.flow;
		exitFlow = other.exitFlow;
		teleportWeight = other.teleportWeight;
		danglingFlow = other.danglingFlow;
		teleportSourceFlow = other.teleportSourceFlow;
		return *this;
	}

	double flow;
	double exitFlow;
	double& enterFlow;
	double teleportWeight;
	double danglingFlow;
	double teleportSourceFlow; // Equals flow when the latter isn't transformed to enter flow

	friend std::ostream& operator<<(std::ostream& out, const FlowDirectedWithTeleportation& data)
	{
//		return out << "flow: " << data.flow << ", exit: " << data.exitFlow;
		return out << "flow: " << data.flow << ", exit: " << data.exitFlow <<
				", enter: " << data.enterFlow << ", teleWeight: " << data.teleportWeight <<
				", danglingFlow: " << data.danglingFlow;
	}
};

class FlowDirectedNonDetailedBalance
{
public:
	FlowDirectedNonDetailedBalance(double flow = 1.0, double teleportWeight = 1.0) :
		flow(flow),
		enterFlow(0.0),
		exitFlow(0.0)
	{}
	FlowDirectedNonDetailedBalance(const FlowDirectedNonDetailedBalance& other) :
		flow(other.flow),
		enterFlow(other.enterFlow),
		exitFlow(other.exitFlow)
	{}
	FlowDirectedNonDetailedBalance& operator=(const FlowDirectedNonDetailedBalance& other)
	{
		flow = other.flow;
		enterFlow = other.enterFlow;
		exitFlow = other.exitFlow;
		return *this;
	}

	double flow;
	double enterFlow;
	double exitFlow;
//	double selfLinkFlow;

	friend std::ostream& operator<<(std::ostream& out, const FlowDirectedNonDetailedBalance& data)
	{
		return out << "flow: " << data.flow << ", enter: " << data.enterFlow << ", exit: " << data.exitFlow;
	}
};

class FlowDirectedNonDetailedBalanceWithTeleportation
{
public:
	FlowDirectedNonDetailedBalanceWithTeleportation(double flow = 1.0, double teleportWeight = 1.0) :
		flow(flow),
		enterFlow(0.0),
		exitFlow(0.0),
		teleportWeight(teleportWeight),
		danglingFlow(0.0)
	{}
	FlowDirectedNonDetailedBalanceWithTeleportation(const FlowDirectedNonDetailedBalanceWithTeleportation& other) :
		flow(other.flow),
		enterFlow(other.enterFlow),
		exitFlow(other.exitFlow),
		teleportWeight(other.teleportWeight),
		danglingFlow(other.danglingFlow)
	{}
	FlowDirectedNonDetailedBalanceWithTeleportation& operator=(const FlowDirectedNonDetailedBalanceWithTeleportation& other)
	{
		flow = other.flow;
		enterFlow = other.enterFlow;
		exitFlow = other.exitFlow;
		teleportWeight = other.teleportWeight;
		danglingFlow = other.danglingFlow;
		return *this;
	}

	double flow;
	double enterFlow;
	double exitFlow;
	double teleportWeight;
	double danglingFlow;
//	double selfLinkFlow;

	friend std::ostream& operator<<(std::ostream& out, const FlowDirectedNonDetailedBalanceWithTeleportation& data)
	{
		return out << "flow: " << data.flow << ", enter: " << data.enterFlow << ", exit: " << data.exitFlow;
	}
};

/**
 * Dummy class to be able to return flow data from specialized structures to non-templated contexts
 */
class FlowDummy
{
public:
	FlowDummy() :
		flow(0.0),
		enterFlow(0.0),
		exitFlow(0.0),
		teleportWeight(0.0),
		danglingFlow(0.0)
	{}

	FlowDummy(const FlowDummy& other) :
		flow(other.flow),
		enterFlow(other.enterFlow),
		exitFlow(other.exitFlow),
		teleportWeight(other.teleportWeight),
		danglingFlow(other.danglingFlow)
	{}

	FlowDummy(const FlowUndirected& other) :
		flow(other.flow),
		enterFlow(other.enterFlow),
		exitFlow(other.exitFlow),
		teleportWeight(0.0),
		danglingFlow(0.0)
	{}

	FlowDummy(const FlowDirected& other) :
		flow(other.flow),
		enterFlow(other.enterFlow),
		exitFlow(other.exitFlow),
		teleportWeight(0.0),
		danglingFlow(0.0)
	{}

	FlowDummy(const FlowDirectedWithTeleportation& other) :
		flow(other.flow),
		enterFlow(other.enterFlow),
		exitFlow(other.exitFlow),
		teleportWeight(other.teleportWeight),
		danglingFlow(other.danglingFlow)
	{}

	FlowDummy(const FlowDirectedNonDetailedBalance& other) :
		flow(other.flow),
		enterFlow(other.enterFlow),
		exitFlow(other.exitFlow),
		teleportWeight(0.0),
		danglingFlow(0.0)
	{}

	FlowDummy(const FlowDirectedNonDetailedBalanceWithTeleportation& other) :
		flow(other.flow),
		enterFlow(other.enterFlow),
		exitFlow(other.exitFlow),
		teleportWeight(other.teleportWeight),
		danglingFlow(other.danglingFlow)
	{}

	FlowDummy& operator=(const FlowDummy& other)
	{
		flow = other.flow;
		enterFlow = other.enterFlow;
		exitFlow = other.exitFlow;
		teleportWeight = other.teleportWeight;
		danglingFlow = other.danglingFlow;
		return *this;
	}

	double flow;
	double enterFlow;
	double exitFlow;
	double teleportWeight;
	double danglingFlow;

	friend std::ostream& operator<<(std::ostream& out, const FlowDummy& data)
	{
		return out << "flow: " << data.flow << ", enter: " << data.enterFlow << ", exit: " << data.exitFlow;
	}
};



#endif /* FLOWDATA_H_ */
