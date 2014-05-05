/**********************************************************************************

 Infomap software package for multi-level network clustering

 Copyright (c) 2013 Daniel Edler, Martin Rosvall

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


#include "MemNetwork.h"
#include "../utils/FileURI.h"
#include "../io/convert.h"
#include "../io/SafeFile.h"
#include "../utils/Logger.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdio>
using std::make_pair;

void MemNetwork::readInputData()
{
	std::string filename = m_config.networkFile;
	if (m_config.inputFormat == "3gram")
		parseTrigram(filename);
	else
	{
		Network::readInputData();
		simulateMemoryFromOrdinaryNetwork();
	}
}

void MemNetwork::parseTrigram(std::string filename)
{
	RELEASE_OUT("Parsing directed trigram from file '" << filename << "'... " << std::flush);
	string line;
	string buf;
	SafeInFile input(filename.c_str());

	std::getline(input,line);
	if (!input.good())
		throw FileFormatError("Can't read first line of pajek file.");

	unsigned int numNodes = 0;
	std::istringstream ss;
	ss.str(line);
	ss >> buf;
	if(buf == "*Vertices" || buf == "*vertices" || buf == "*VERTICES") {
		if (!(ss >> numNodes))
			throw BadConversionError(io::Str() << "Can't parse an integer after \"" << buf <<
					"\" as the number of nodes.");
	}
	else {
		throw FileFormatError("The first line (to lower cases) doesn't match *vertices.");
	}

	if (numNodes == 0)
		throw FileFormatError("The number of vertices cannot be zero.");

	unsigned int specifiedNumNodes = numNodes;
	bool checkNodeLimit = m_config.nodeLimit > 0;
	if (checkNodeLimit)
		numNodes = m_config.nodeLimit;

	m_numNodes = numNodes;
	m_nodeNames.resize(numNodes);
	m_nodeWeights.assign(numNodes, 1.0);
	m_sumNodeWeights = 0.0;

	// Read node names, assuming order 1, 2, 3, ...
	for (unsigned int i = 0; i < numNodes; ++i)
	{
		unsigned int id = 0;
		if (!(input >> id) || id != static_cast<unsigned int>(i+1))
		{
			throw BadConversionError(io::Str() << "Couldn't parse node number " << (i+1) << " from line " << (i+2) << ".");
		}
		std::getline(input,line);
		unsigned int nameStart = line.find_first_of("\"");
		unsigned int nameEnd = line.find_last_of("\"");
		string name;
		double nodeWeight = 1.0;
		if(nameStart < nameEnd) {
			name = string(line.begin() + nameStart + 1, line.begin() + nameEnd);
			line = line.substr(nameEnd + 1);
			ss.clear();
			ss.str(line);
		}
		else {
			ss.clear();
			ss.str(line);
			ss >> buf; // Take away the index from the stream
			ss >> name; // Extract the next token as the name assuming no spaces
		}
		ss >> nodeWeight; // Extract the next token as node weight. If failed, the old value (1.0) is kept.
		m_sumNodeWeights += nodeWeight;
		m_nodeWeights[i] = nodeWeight;
		//		m_nodeMap.insert(make_pair(name, i));
		m_nodeNames[i] = name;
	}

	if (m_config.nodeLimit > 0 && numNodes < specifiedNumNodes)
	{
		unsigned int surplus = specifiedNumNodes - numNodes;
		for (unsigned int i = 0; i < surplus; ++i)
			std::getline(input, line);
	}

	// Read the number of links in the network
	getline(input, line);
	ss.clear();
	ss.str(line);
	ss >> buf;
	if(buf != "*Arcs" && buf != "*arcs" && buf != "*3grams") {
		throw FileFormatError("The first line (to lower cases) after the nodes doesn't match *arcs or *3grams.");
	}

	unsigned int maxLinkEnd = 0;
	m_totalLinkWeight = 0.0;

	// Read links in format "from through to weight", for example "1 2 3 2" (all integers) and each undirected link only ones (weight is optional).
	while(!std::getline(input, line).fail())
	{
		if (line.length() == 0)
			continue;

		unsigned int n1, n2, n3;
		double weight;
		parseM2Link(line, n1, n2, n3, weight);

		addM2Link(n1, n2, n2, n3, weight);

		if (n2 != n3 || m_config.includeSelfLinks)
			insertLink(n2, n3, weight);

//		if(m_config.includeSelfLinks)
//		{
//			m_m2Nodes[M2Node(n1,n2)] += linkWeight;
//			m_m2Nodes[M2Node(n2,n3)] += 0.0;
//
//			m_m2Links[make_pair(M2Node(n1,n2),M2Node(n2,n3))] += linkWeight;
//			m_totM2LinkWeight += linkWeight;
//
//			m_totalLinkWeight += linkWeight;
//
//			if (n2 == n3) {
//				++m_numSelfLinks;
//				m_totalSelfLinkWeight += linkWeight;
//				if (n1 == n2) {
//					++m_numMemorySelfLinks;
//					m_totalMemorySelfLinkWeight += linkWeight;
//				}
//			}
//			insertLink(n2, n3, linkWeight);
//		}
//		else if (n2 != n3)
//		{
//			m_totalLinkWeight += linkWeight;
//			insertLink(n2, n3, linkWeight);
//
//			if(n1 != n2)
//			{
//				m_m2Nodes[M2Node(n1,n2)] += linkWeight;
//				m_m2Nodes[M2Node(n2,n3)] += 0.0;
//
//				m_m2Links[make_pair(M2Node(n1,n2),M2Node(n2,n3))] += linkWeight;
//				m_totM2LinkWeight += linkWeight;
//			}
//			else
//			{
//				m_m2Nodes[M2Node(n2,n3)] += linkWeight;
//			}
//		}
	}

	unsigned int zeroMinusOne = 0;
	--zeroMinusOne;
	if (maxLinkEnd == zeroMinusOne)
		throw InputDomainError(io::Str() << "Integer overflow, be sure to use zero-based node numbering if the node numbers start from zero.");
	if (maxLinkEnd >= numNodes)
		throw InputDomainError(io::Str() << "At least one link is defined with node numbers that exceeds the number of nodes.");

	if (m_links.size() == 0)
		throw InputDomainError(io::Str() << "No links could be found!");



	unsigned int M2nodeNr = 0;
	for(map<M2Node,double>::iterator it = m_m2Nodes.begin(); it != m_m2Nodes.end(); ++it)
	{
		m_m2NodeMap[it->first] = M2nodeNr;
		M2nodeNr++;
	}

//	set<int> M1nodes;
	m_m2NodeWeights.resize(m_m2Nodes.size());
	m_totM2NodeWeight = 0.0;

	M2nodeNr = 0;
	for(map<M2Node,double>::iterator it = m_m2Nodes.begin(); it != m_m2Nodes.end(); ++it)
	{
//		M1nodes.insert(it->first.second);
		m_m2NodeWeights[M2nodeNr] += it->second;
		m_totM2NodeWeight += it->second;
		++M2nodeNr;
	}



	//	unsigned int sumEdgesFound = m_links.size() + m_numSelfLinks + numDoubleLinks + numSkippedEdges;
	std::cout << "done! Found " << specifiedNumNodes << " nodes and " << m_numLinks << " links. ";
	std::cout << "Generated " << m_m2Nodes.size() << " memory nodes and " << m_m2Links.size() << " memory links. ";
	//	std::cout << "Average node weight: " << (m_sumNodeWeights / numNodes) << ". ";
	if (m_config.nodeLimit > 0)
		std::cout << "Limiting network to " << numNodes << " nodes and " << m_links.size() << " links. ";
	if(m_numAggregatedLinks > 0)
		std::cout << m_numAggregatedLinks << " links was aggregated to existing links. ";
	if (m_numSelfLinks > 0 && !m_config.includeSelfLinks)
		std::cout << m_numSelfLinks << " self-links was ignored. ";
	std::cout << std::endl;

}

void MemNetwork::simulateMemoryFromOrdinaryNetwork()
{
	std::cout << "Simulating memory from ordinary network by chaining pair of overlapping links to trigrams... " << std::flush;

	// Reset some data from ordinary network
	m_totalLinkWeight = 0.0;
	m_numSelfLinks = 0.0;
	m_totalSelfLinkWeight = 0.0;

	if (m_config.originallyUndirected)
	{
		std::cout << "(inflating undirected network... " << std::flush;
		LinkMap oldNetwork;
		oldNetwork.swap(m_links);
		for (LinkMap::const_iterator linkIt(oldNetwork.begin()); linkIt != oldNetwork.end(); ++linkIt)
		{
			unsigned int linkEnd1 = linkIt->first;
			const std::map<unsigned int, double>& subLinks = linkIt->second;
			for (std::map<unsigned int, double>::const_iterator subIt(subLinks.begin()); subIt != subLinks.end(); ++subIt)
			{
				unsigned int linkEnd2 = subIt->first;
				double linkWeight = subIt->second;
				// Add link to both directions
				insertLink(linkEnd1, linkEnd2, linkWeight);
				insertLink(linkEnd2, linkEnd1, linkWeight);
			}
		}

		// Dispose old network from memory
		LinkMap().swap(oldNetwork);
		std::cout << "done!) " << std::flush;
	}

	for (LinkMap::const_iterator linkIt(m_links.begin()); linkIt != m_links.end(); ++linkIt)
	{
		unsigned int n1 = linkIt->first;
		const std::map<unsigned int, double>& subLinks = linkIt->second;
		for (std::map<unsigned int, double>::const_iterator subIt(subLinks.begin()); subIt != subLinks.end(); ++subIt)
		{
			unsigned int n2 = subIt->first;
			double firstLinkWeight = subIt->second;

			// Create trigrams with all links that start with the end node of current link
			LinkMap::iterator secondLinkIt = m_links.find(n2);
			if (secondLinkIt != m_links.end())
			{
				std::map<unsigned int, double>& secondLinkSubMap = secondLinkIt->second;
				for (std::map<unsigned int, double>::const_iterator secondSubIt(secondLinkSubMap.begin()); secondSubIt != secondLinkSubMap.end(); ++secondSubIt)
				{
					unsigned int n3 = secondSubIt->first;
					double linkWeight = secondSubIt->second;

					addM2Link(n1, n2, n2, n3, linkWeight, firstLinkWeight / secondLinkSubMap.size(), 0.0);

//					if(m_config.includeSelfLinks)
//					{
//						// Set teleportation weight on first memory node (normalized to not multiply physical weight)
//						m_m2Nodes[M2Node(n1,n2)] += firstLinkWeight / secondLinkSubMap.size();
//						m_m2Nodes[M2Node(n2,n3)] += 0.0;
//
//						m_m2Links[make_pair(M2Node(n1,n2),M2Node(n2,n3))] += linkWeight;
//						m_totM2LinkWeight += linkWeight;
//
//						m_totalLinkWeight += linkWeight;
//
//						if (n2 == n3) {
//							++m_numSelfLinks;
//							m_totalSelfLinkWeight += linkWeight;
//							if (n1 == n2) {
//								++m_numMemorySelfLinks;
//								m_totalMemorySelfLinkWeight += linkWeight;
//							}
//						}
//					}
//					else if (n2 != n3)
//					{
//						m_totalLinkWeight += linkWeight;
//
//						if(n1 != n2)
//						{
//							// Set teleportation weight on first memory node (normalized to not multiply physical weight)
//							m_m2Nodes[M2Node(n1,n2)] += firstLinkWeight / secondLinkSubMap.size();
//							m_m2Nodes[M2Node(n2,n3)] += 0.0;
//
//							m_m2Links[make_pair(M2Node(n1,n2),M2Node(n2,n3))] += linkWeight;
//							m_totM2LinkWeight += linkWeight;
//						}
//						else
//						{
//							// First memory node is a self-link, create the second
//							m_m2Nodes[M2Node(n2,n3)] += linkWeight;
//						}
//					}
				}
			}
			else
			{
				// No chainable link found, create a dangling memory node (or remove need for existence in MemFlowNetwork?)
				addM2Node(n1, n2, firstLinkWeight);
			}
		}
	}

	unsigned int M2nodeNr = 0;
	for(map<M2Node,double>::iterator it = m_m2Nodes.begin(); it != m_m2Nodes.end(); ++it)
	{
		m_m2NodeMap[it->first] = M2nodeNr;
		M2nodeNr++;
	}

//	set<int> M1nodes;
	m_m2NodeWeights.resize(m_m2Nodes.size());
	m_totM2NodeWeight = 0.0;

	M2nodeNr = 0;
	for(map<M2Node,double>::iterator it = m_m2Nodes.begin(); it != m_m2Nodes.end(); ++it)
	{
		m_m2NodeWeights[M2nodeNr] += it->second;
		m_totM2NodeWeight += it->second;
		++M2nodeNr;
	}

	std::cout << "generated " << m_m2Nodes.size() << " memory nodes and " << m_m2Links.size() << " memory links. " << std::endl;
}

void MemNetwork::parseM2Link(const std::string& line, unsigned int& n1, unsigned int& n2, unsigned int& n3, double& weight)
{
	m_extractor.clear();
	m_extractor.str(line);
	if (!(m_extractor >> n1 >> n2 >> n3))
		throw FileFormatError(io::Str() << "Can't parse link data from line '" << line << "'");
	(m_extractor >> weight) || (weight = 1.0);

	n1 -= m_indexOffset;
	n2 -= m_indexOffset;
	n3 -= m_indexOffset;
}

void MemNetwork::parseM2Link(char line[], unsigned int& n1, unsigned int& n2, unsigned int& n3, double& weight)
{
	char *cptr;
	cptr = strtok(line, " \t"); // Get first non-whitespace character position
	if (cptr == NULL)
		throw FileFormatError(io::Str() << "Can't parse link data from line '" << line << "'");
	n1 = atoi(cptr); // get first connected node
	cptr = strtok(NULL, " \t"); // Get second non-whitespace character position
	if (cptr == NULL)
		throw FileFormatError(io::Str() << "Can't parse link data from line '" << line << "'");
	n2 = atoi(cptr); // get second connected node
	cptr = strtok(NULL, " \t"); // Get third non-whitespace character position
	if (cptr == NULL)
		throw FileFormatError(io::Str() << "Can't parse link data from line '" << line << "'");
	n3 = atoi(cptr); // get the third connected node
	cptr = strtok(NULL, " \t"); // Get fourth non-whitespace character position
	if (cptr != NULL)
		weight = atof(cptr); // get the link weight
	else
		weight = 1.0;

	n1 -= m_indexOffset;
	n2 -= m_indexOffset;
	n3 -= m_indexOffset;
}

bool MemNetwork::addM2Link(unsigned int n1PriorState, unsigned int n1, unsigned int n2PriorState, unsigned int n2, double weight, double firstM2NodeWeight, double secondM2NodeWeight)
{
	++m_numM2LinksFound;

	if (m_config.nodeLimit > 0 && (n1 >= m_config.nodeLimit || n2 >= m_config.nodeLimit))
		return false;

	m_maxNodeIndex = std::max(m_maxNodeIndex, std::max(n1, n2));
	m_minNodeIndex = std::min(m_minNodeIndex, std::min(n1, n2));

	if(m_config.includeSelfLinks)
	{
		insertM2Link(n1PriorState, n1, n2PriorState, n2, weight);
		addM2Node(n1PriorState, n1, firstM2NodeWeight);
		addM2Node(n2PriorState, n2, secondM2NodeWeight);
	}
	else if (n1 != n2)
	{
		if(n1PriorState != n1)
		{
			insertM2Link(n1PriorState, n1, n2PriorState, n2, weight);
			addM2Node(n1PriorState, n1, firstM2NodeWeight);
			addM2Node(n2PriorState, n2, secondM2NodeWeight);
		}
		else
			addM2Node(n2PriorState, n2, weight);
	}

	return true;
}

bool MemNetwork::insertM2Link(unsigned int n1PriorState, unsigned int n1, unsigned int n2PriorState, unsigned int n2, double weight)
{
	M2Node m1(n1PriorState, n1);
	M2Node m2(n2PriorState, n2);

	++m_numM2Links;
	m_totM2LinkWeight += weight;

	// Aggregate link weights if they are definied more than once
	M2LinkMap::iterator firstIt = m_m2Links.lower_bound(m1);
	if (firstIt != m_m2Links.end() && firstIt->first == m1) // First node already exists, check second node
	{
		std::pair<std::map<M2Node, double>::iterator, bool> ret2 = firstIt->second.insert(std::make_pair(m2, weight));
		if (!ret2.second)
		{
			ret2.first->second += weight;
			++m_numAggregatedM2Links;
			--m_numM2Links;
			return false;
		}
	}
	else
	{
		m_m2Links.insert(firstIt, std::make_pair(m1, std::map<M2Node, double>()))->second.insert(std::make_pair(m2, weight));
	}

	return true;
}

bool MemNetwork::insertM2Link(M2LinkMap::iterator firstM2Node, unsigned int n2PriorState, unsigned int n2, double weight)
{
	m_totM2LinkWeight += weight;
	std::pair<std::map<M2Node, double>::iterator, bool> ret2 = firstM2Node->second.insert(std::make_pair(M2Node(n2PriorState, n2), weight));
	if (!ret2.second)
	{
		ret2.first->second += weight;
		++m_numAggregatedM2Links;
		return false;
	}
	else
	{
		++m_numM2Links;
		return true;
	}
}

