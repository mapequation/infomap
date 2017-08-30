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


#include "StateNetwork.h"
#include "../utils/FlowCalculator.h"
#include "../utils/Log.h"

namespace infomap {

std::pair<StateNetwork::NodeMap::iterator, bool> StateNetwork::addStateNode(StateNode node)
{
	// return m_nodes.emplace(node.id, node);
	return m_nodes.insert(StateNetwork::NodeMap::value_type(node.id, node));
}

std::pair<StateNetwork::NodeMap::iterator, bool> StateNetwork::addStateNode(unsigned int id, unsigned int physId)
{
	return addStateNode(StateNode(id, physId));
}

std::pair<StateNetwork::NodeMap::iterator, bool> StateNetwork::addNode(unsigned int id)
{
	return addStateNode(StateNode(id));
}

std::pair<StateNetwork::NodeMap::iterator, bool> StateNetwork::addNode(unsigned int id, double weight)
{
	auto ret = addNode(id);
	auto& node = ret.first->second;
	node.weight = weight;
	return ret;
}

std::pair<StateNetwork::NodeMap::iterator, bool> StateNetwork::addNode(unsigned int id, std::string name)
{
	m_names[id] = name;
	return addNode(id);
}

std::pair<StateNetwork::NodeMap::iterator, bool> StateNetwork::addNode(unsigned int id, std::string name, double weight)
{
	m_names[id] = name;
	return addNode(id, weight);
}

StateNetwork::PhysNode& StateNetwork::addPhysicalNode(unsigned int physId)
{
	auto& physNode = m_physNodes[physId];
	return physNode;
}

StateNetwork::PhysNode& StateNetwork::addPhysicalNode(unsigned int physId, double weight)
{
	auto& physNode = m_physNodes[physId];
	physNode.weight = weight;
	return physNode;
}

StateNetwork::PhysNode& StateNetwork::addPhysicalNode(unsigned int physId, const std::string& name)
{
	auto& physNode = m_physNodes[physId];
	physNode.name = name;
	return physNode;
}

StateNetwork::PhysNode& StateNetwork::addPhysicalNode(unsigned int physId, double weight, const std::string& name)
{
	auto& physNode = m_physNodes[physId];
	physNode.weight = weight;
	physNode.name = name;
	return physNode;
}


std::pair<std::map<unsigned int, std::string>::iterator, bool> StateNetwork::addName(unsigned int id, std::string name)
{
	// m_names[id] = name;
	return m_names.insert(std::make_pair(id, name));
}

bool StateNetwork::addLink(unsigned int sourceId, unsigned int targetId, double weight)
{
	if (sourceId == targetId) {
		++m_numSelfLinksFound;
		if (!m_config.includeSelfLinks) {
			return false;
		}
		m_sumSelfLinkWeight += weight;
	}
	// const auto& sourceNode = m_nodes.emplace(sourceId, StateNode(sourceId)).first;
	// const auto& targetNode = m_nodes.emplace(targetId, StateNode(targetId)).first;
	// m_nodes.emplace(sourceId, StateNode(sourceId));
	// m_nodes.emplace(targetId, StateNode(targetId));
	addNode(sourceId);
	addNode(targetId);

	++m_numLinks;
	m_sumLinkWeight += weight;

	bool addedNewLink = true;

	// Log() << "Add link " << sourceId << " -> " << targetId << ", weight: " << weight << "\n";
	// Aggregate link weights if they are definied more than once
	auto& outLinks = m_nodeLinkMap[sourceId];
	if (outLinks.empty()) {
		outLinks[targetId] = weight;
		// Log() << "  -> new source link!\n";
	}
	else {
		// auto& ret = outLinks.emplace(targetNode, weight);
		// if (!ret.second)
			// auto& linkData = ret.first;
		auto& linkData = outLinks[targetId];
		++linkData.count;
		if (linkData.count != 1) {
			// Log() << "  -> existing weight: " << linkData.weight;
			linkData.weight += weight;
			// Log() << " -> weight: " << linkData.weight << "\n";
			// Log() << "    -> count: " << linkData.count << ", weight: " << linkData.weight << "\n";
			++m_numAggregatedLinks;
			--m_numLinks;
			addedNewLink = false;
		}
		else {
			// Log() << "  -> new target link!\n";
			linkData.weight = weight;
		}
	}
	// printNetwork();
	return addedNewLink;
	// auto& ret = m_nodeLinkMap.emplace(sourceId, std::make_pair(StateNode(), LinkData()));
	// if (!ret.second) {
	// 	ret.first[targetId] = weight;
	// }
	// else {
	// 	auto& ret2 = ret.first.emplace(targetId, weight);
	// 	if (!ret2.second) {
	// 		ret2.first += weight;
	// 		++m_numAggregatedLinks;
	// 		--m_numLinks;
	// 		return false;
	// 	}
	// }
	// return true;

	// LinkMap::iterator firstIt = m_links.lower_bound(sourceId);
	// if (firstIt != m_links.end() && firstIt->first == sourceId) // First linkEnd already exists, check second linkEnd
	// {
	// 	std::pair<std::map<unsigned int, double>::iterator, bool> ret2 = firstIt->second.insert(std::make_pair(targetId, weight));
	// 	if (!ret2.second)
	// 	{
	// 		ret2.first->second += weight;
	// 		++m_numAggregatedLinks;
	// 		--m_numLinks;
	// 		return false;
	// 	}
	// }
	// else
	// {
	// 	m_links.insert(firstIt, std::make_pair(sourceId, std::map<unsigned int, double>()))->second.insert(std::make_pair(targetId, weight));
	// }

	// return true;
}

void StateNetwork::calculateFlow()
{
	FlowCalculator flowCalculator;
	flowCalculator.calculateFlow(*this, m_config);
}

void StateNetwork::printNetwork() const
{
	Log() << "----------------\n";
	Log() << "Current network:\n";
	for (auto& linkIt : m_nodeLinkMap) {
		for (auto& subIt : linkIt.second)
		{
			Log() << "  " << linkIt.first.id << " -> " << subIt.first.id << ", weight: " << subIt.second.weight << "\n";
		}
	}
	Log() << "----------------\n";
}

}
