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


#ifndef STATENETWORK_H_
#define STATENETWORK_H_

#include <string>
#include <map>
#include <vector>
#include <utility>
#include "../io/Config.h"
// #include "../utils/FlowCalculator.h"

namespace infomap {

class StateNetwork
{
public:

	struct StateNode
	{
		unsigned int id = 0;
		unsigned int physicalId = 0;
		double weight = 1.0;
		double flow = 0.0;
		double enterFlow = 0.0;
		double exitFlow = 0.0;
		
		StateNode() {}
		StateNode(unsigned int id) :
			id(id), physicalId(id)
		{}
		StateNode(unsigned int id, unsigned int physicalId) :
			id(id), physicalId(physicalId)
		{}
		StateNode(const StateNode& other) :
			id(other.id),
			physicalId(other.physicalId),
			weight(other.weight),
			flow(other.flow),
			enterFlow(other.enterFlow),
			exitFlow(other.exitFlow)
		{}
		StateNode& operator=(const StateNode& other)
		{
			id = other.id;
			physicalId = other.physicalId;
			weight = other.weight;
			flow = other.flow;
			enterFlow = other.enterFlow;
			exitFlow = other.exitFlow;
			return *this;
		}

		bool operator ==(const StateNode& rhs) const
		{ return id == rhs.id; }

		bool operator !=(const StateNode& rhs) const
		{ return id != rhs.id; }

		bool operator <(const StateNode& rhs) const
		{ return id < rhs.id; }

	};

	struct PhysNode
	{
		unsigned int physId = 0;
		double weight = 1.0;
		std::string name = "";
		PhysNode() {}
		PhysNode(unsigned int physId) : physId(physId) {}
		PhysNode(unsigned int physId, double weight) : physId(physId), weight(weight) {}
		PhysNode(double weight) : weight(weight) {}
		PhysNode(unsigned int physId, std::string name) : physId(physId), name(name) {}
		PhysNode(std::string name) : name(name) {}
	};

	struct LinkData
	{
		double weight = 1.0;
		double flow = 0.0;
		unsigned int count = 0;

		LinkData(double weight = 1.0) :
			weight(weight)
		{}
		LinkData(const LinkData& other) :
			weight(other.weight),
			flow(other.flow)
		{}
		LinkData& operator=(const LinkData& other)
		{
			weight = other.weight;
			flow = other.flow;
			return *this;
		}
		LinkData& operator+=(double weight)
		{
			weight += weight;
			return *this;
		}
	};

	struct StateLink
	{
		StateLink(unsigned int sourceIndex = 0, unsigned int targetIndex = 0, double weight = 0.0) :
			source(sourceIndex),
			target(targetIndex),
			weight(weight),
			flow(weight)
		{}
		StateLink(const StateLink& other) :
			source(other.source),
			target(other.target),
			weight(other.weight),
			flow(other.flow)
		{}
		unsigned int source;
		unsigned int target;
		double weight;
		double flow;
	};

	using NodeMap = std::map<unsigned int, StateNode>;
	// typedef Network::StateLinkMap										StateLinkMap;
	typedef std::vector<StateLink>										LinkVec;
	typedef std::map<unsigned int, std::map<unsigned int, double> >	LinkMap;
	typedef std::map<StateNode, std::map<StateNode, LinkData> >	NodeLinkMap;

protected:
	friend class FlowCalculator;
	// Config
	Config m_config;
	// Network
	bool m_directed = true;
	// LinkMap m_links;
	NodeMap m_nodes;
	NodeLinkMap m_nodeLinkMap;
	unsigned int m_numNodesFound = 0;
	unsigned int m_numStateNodesFound = 0;
	unsigned int m_numLinksFound = 0;
	unsigned int m_numLinks = 0;
	unsigned int m_numSelfLinksFound = 0;
	double m_sumLinkWeight = 0.0;
	double m_sumSelfLinkWeight = 0.0;
	unsigned int m_numAggregatedLinks = 0;
	// Attributes
	std::map<unsigned int, std::string> m_names;
	std::map<unsigned int, PhysNode> m_physNodes;

	// Infomap
	// InfoNode m_root;

public:

	StateNetwork()
	:	m_config(Config()) {}
	StateNetwork(const Config& config)
	:	m_config(config) {}
	virtual ~StateNetwork() {}
	
	// Config
	void setConfig(const Config& config) { m_config = config; }
	const Config& getConfig() { return m_config; }

	// Mutators
	std::pair<NodeMap::iterator, bool> addStateNode(StateNode node);
	std::pair<NodeMap::iterator, bool> addStateNode(unsigned int id, unsigned int physId);
	std::pair<NodeMap::iterator, bool> addNode(unsigned int id);
	std::pair<NodeMap::iterator, bool> addNode(unsigned int id, std::string name);
	std::pair<NodeMap::iterator, bool> addNode(unsigned int id, double weight);
	std::pair<NodeMap::iterator, bool> addNode(unsigned int id, std::string, double weight);
	PhysNode& addPhysicalNode(unsigned int physId);
	PhysNode& addPhysicalNode(unsigned int physId, double weight);
	PhysNode& addPhysicalNode(unsigned int physId, const std::string& name);
	PhysNode& addPhysicalNode(unsigned int physId, double weight, const std::string& name);
	std::pair<std::map<unsigned int, std::string>::iterator, bool> addName(unsigned int id, std::string);
	bool addLink(unsigned int sourceId, unsigned int targetId, double weight = 1.0);
	
	void calculateFlow();

	// Getters
	const NodeMap& nodes() const { return m_nodes; }
	unsigned int numNodes() const { return m_nodes.size(); }
	unsigned int numPhysicalNodes() const { return m_physNodes.size(); }
	const NodeLinkMap& nodeLinkMap() const { return m_nodeLinkMap; }
	// const LinkMap& links() const { return m_links; }
	unsigned int numLinks() const { return m_numLinks; }
	double sumLinkWeight() const { return m_sumLinkWeight; }
	double sumSelfLinkWeight() const { return m_sumSelfLinkWeight; }

	bool isDirected() const { return m_config.directed; }

	void printNetwork() const;


};

}

#endif /* STATENETWORK_H_ */
