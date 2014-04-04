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
using std::make_pair;

void MemNetwork::readFromFile(std::string filename)
{
	if (m_config.inputFormat == "3gram")
		parseTrigram(filename);
	else
		Network::readFromFile(filename);
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

	unsigned int numDoubleLinks = 0;
	unsigned int numEdgeLines = 0;
	unsigned int numSkippedEdges = 0;
	unsigned int maxLinkEnd = 0;
	m_totalLinkWeight = 0.0;

	// Read links in format "from through to weight", for example "1 2 3 2" (all integers) and each undirected link only ones (weight is optional).
	while(!std::getline(input, line).fail())
	{
		if (line.length() == 0)
			continue;
		++numEdgeLines;
		ss.clear();
		ss.str(line);
		unsigned int n1;
		if (!(ss >> n1))
		{
			throw BadConversionError(io::Str() << "Can't parse first integer of link number " <<
					numEdgeLines << " from line '" << line << "'");
		}
		unsigned int n2;
		if (!(ss >> n2))
			throw BadConversionError(io::Str() << "Can't parse second integer of link number " <<
					numEdgeLines << " from line '" << line << "'");

		unsigned int n3;
		if (!(ss >> n3))
			throw BadConversionError(io::Str() << "Can't parse third integer of link number " <<
					numEdgeLines << " from line '" << line << "'");

		if (checkNodeLimit && (n3 > numNodes || n2 > numNodes || n1 > numNodes))
		{
			++numSkippedEdges;
			continue;
		}

		double linkWeight;
		if (!(ss >> linkWeight))
			linkWeight = 1.0;

		--n1; // Node numbering starts from 1 in the .net format
		--n2;
		--n3;

		maxLinkEnd = std::max(maxLinkEnd, std::max(n1, std::max(n2, n3)));

		//		// Aggregate link weights if they are definied more than once
		//		LinkMap::iterator it = m_links.find(std::make_pair(n1, n2));
		//		if(it == m_links.end())
		//		{
		//			m_links.insert(std::make_pair(std::make_pair(n1, n2), linkWeight));
		//		}
		//		else
		//		{
		//			it->second += linkWeight;
		//			++numDoubleLinks;
		//			if (n2 == n1)
		//				--m_numSelfLinks;
		//		}


		if(m_config.includeSelfLinks)
		{
			m_m2Nodes[M2Node(n1,n2)] += linkWeight;
			m_m2Nodes[M2Node(n2,n3)] += 0.0;

			m_m2Links[make_pair(M2Node(n1,n2),M2Node(n2,n3))] += linkWeight;
			m_totM2LinkWeight += linkWeight;

			m_links[make_pair(n2,n3)] += linkWeight;
			m_totalLinkWeight += linkWeight;
		}
		else{
			if(n1 != n2 && n2 != n3)
			{
				m_m2Nodes[M2Node(n1,n2)] += linkWeight;
				m_m2Nodes[M2Node(n2,n3)] += 0.0;

				m_m2Links[make_pair(M2Node(n1,n2),M2Node(n2,n3))] += linkWeight;
				m_totM2LinkWeight += linkWeight;

				m_links[make_pair(n2,n3)] += linkWeight;
				m_totalLinkWeight += linkWeight;
			}
			else if(n2 != n3)
			{
				m_m2Nodes[M2Node(n2,n3)] += linkWeight;

				m_links[make_pair(n2,n3)] += linkWeight;
				m_totalLinkWeight += linkWeight;
			}
		}
	}

	unsigned int zeroMinusOne = 0;
	--zeroMinusOne;
	if (maxLinkEnd == zeroMinusOne)
		throw InputDomainError(io::Str() << "Integer overflow, be sure to use zero-based node numbering if the node numbers starts from zero.");
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
	std::cout << "done! Found " << specifiedNumNodes << " nodes and " << m_links.size() << " links. ";
	std::cout << "Generated " << m_m2Nodes.size() << " memory nodes and " << m_m2Links.size() << " memory links. ";
	//	std::cout << "Average node weight: " << (m_sumNodeWeights / numNodes) << ". ";
	if (m_config.nodeLimit > 0)
		std::cout << "Limiting network to " << numNodes << " nodes and " << m_links.size() << " links. ";
	if(numDoubleLinks > 0)
		std::cout << numDoubleLinks << " links was aggregated to existing links. ";
	if (m_numSelfLinks > 0 && !m_config.includeSelfLinks)
		std::cout << m_numSelfLinks << " self-links was ignored. ";
	std::cout << std::endl;

}

