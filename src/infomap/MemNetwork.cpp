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

			m_totalLinkWeight += linkWeight;

			if (n2 == n3) {
				++m_numSelfLinks;
				m_totalSelfLinkWeight += linkWeight;
				if (n1 == n2) {
					++m_numMemorySelfLinks;
					m_totalMemorySelfLinkWeight += linkWeight;
				}
			}
			insertLink(n2, n3, linkWeight);
		}
		else if (n2 != n3)
		{
			m_totalLinkWeight += linkWeight;
			insertLink(n2, n3, linkWeight);

			if(n1 != n2)
			{
				m_m2Nodes[M2Node(n1,n2)] += linkWeight;
				m_m2Nodes[M2Node(n2,n3)] += 0.0;

				m_m2Links[make_pair(M2Node(n1,n2),M2Node(n2,n3))] += linkWeight;
				m_totM2LinkWeight += linkWeight;
			}
			else
			{
				m_m2Nodes[M2Node(n2,n3)] += linkWeight;
			}
		}
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


					if(m_config.includeSelfLinks)
					{
						// Set teleportation weight on first memory node (normalized to not multiply physical weight)
						m_m2Nodes[M2Node(n1,n2)] += firstLinkWeight / secondLinkSubMap.size();
						m_m2Nodes[M2Node(n2,n3)] += 0.0;

						m_m2Links[make_pair(M2Node(n1,n2),M2Node(n2,n3))] += linkWeight;
						m_totM2LinkWeight += linkWeight;

						m_totalLinkWeight += linkWeight;

						if (n2 == n3) {
							++m_numSelfLinks;
							m_totalSelfLinkWeight += linkWeight;
							if (n1 == n2) {
								++m_numMemorySelfLinks;
								m_totalMemorySelfLinkWeight += linkWeight;
							}
						}
					}
					else if (n2 != n3)
					{
						m_totalLinkWeight += linkWeight;

						if(n1 != n2)
						{
							// Set teleportation weight on first memory node (normalized to not multiply physical weight)
							m_m2Nodes[M2Node(n1,n2)] += firstLinkWeight / secondLinkSubMap.size();
							m_m2Nodes[M2Node(n2,n3)] += 0.0;

							m_m2Links[make_pair(M2Node(n1,n2),M2Node(n2,n3))] += linkWeight;
							m_totM2LinkWeight += linkWeight;
						}
						else
						{
							// First memory node is a self-link, create the second
							m_m2Nodes[M2Node(n2,n3)] += linkWeight;
						}
					}
				}
			}
			else
			{
				// No chainable link found, create a dangling memory node (or remove need for existence in MemFlowNetwork?)
				m_m2Nodes[M2Node(n1,n2)] += firstLinkWeight;
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


void MultiplexNetwork::readInputData()
{
	if (m_config.inputFormat == "multiplex")
		parseMultiplexNetwork(m_config.networkFile);
	else
		throw ImplementationError("Multiplex network only supports single multiplex data input for now.");
}

void MultiplexNetwork::parseMultiplexNetwork(std::string filename)
{
	RELEASE_OUT("Parsing multiplex network from file '" << filename << "'... " << std::flush);

	SafeInFile input(filename.c_str());

	string line;
	bool intra = true;
	unsigned int numIntraLinksFound = 0;
	unsigned int numInterLinksFound = 0;

	// Read links in format "from to weight", for example "1 3 2" (all integers) and each undirected link only ones (weight is optional).
	while(!std::getline(input, line).fail())
	{
		if (line.length() == 0 || line[0] == '#')
			continue;
		if (line == "*Intra" || line == "*intra") {
			intra = true;
			continue;
		}
		if (line == "*Inter" || line == "*inter") {
			intra = false;
			continue;
		}

		if (intra)
		{
			unsigned int level, n1, n2;
			double weight;

			parseIntraLink(line, level, n1, n2, weight);

			level -= m_indexOffset;

			while (m_networks.size() < level + 1)
				m_networks.push_back(Network(m_config));

			m_networks[level].addLink(n1, n2, weight);

			++numIntraLinksFound;
		}
		else
		{
			unsigned int nodeIndex, level1, level2;
			double weight;

			parseInterLink(line, nodeIndex, level1, level2, weight);

			nodeIndex -= m_indexOffset;
			level1 -= m_indexOffset;
			level2 -= m_indexOffset;

			m_interLinks[InterLinkKey(nodeIndex, level1, level2)] += weight;

			++numInterLinksFound;
		}
	}

	if (m_networks.size() < 2)
		throw InputDomainError("Need at least two layers of network data for multiplex network.");

	std::cout << "done! Found " << numIntraLinksFound << " intra-network links in " << m_networks.size() << " layers with " <<
			numInterLinksFound << " inter-network links\n";

	// Finalize and check each network layer
	for (unsigned int layerIndex = 0; layerIndex < m_networks.size(); ++layerIndex)
	{
		std::cout << "Layer " << (layerIndex + 1) << ": " << std::flush;
		m_networks[layerIndex].finalizeAndCheckNetwork();
		m_networks[layerIndex].printParsingResult();
	}

	generateMemoryNetwork();
}

void MultiplexNetwork::generateMemoryNetwork()
{
	// Check maximum number of nodes
	unsigned int maxNumNodes = m_networks[0].numNodes();
	bool differentNodeCount = false;
	for (unsigned int layerIndex = 0; layerIndex < m_networks.size(); ++layerIndex)
	{
		unsigned int numNodesInLayer = m_networks[layerIndex].numNodes();
		if (numNodesInLayer != maxNumNodes)
			differentNodeCount = true;
		maxNumNodes = std::max(maxNumNodes, numNodesInLayer);

		// Take node names from networks if exist
		if (m_nodeNames.empty() && !m_networks[layerIndex].nodeNames().empty())
			m_networks[layerIndex].swapNodeNames(m_nodeNames);
	}

	m_numNodes = maxNumNodes;

	if (differentNodeCount)
	{
		std::cout << "Adjusting for equal number of nodes:\n";
		for (unsigned int layerIndex = 0; layerIndex < m_networks.size(); ++layerIndex)
		{
			if (m_networks[layerIndex].numNodes() != maxNumNodes)
			{
				std::cout << "  Layer " << (layerIndex + 1) << ": " <<
						m_networks[layerIndex].numNodes() << " -> " << maxNumNodes << " nodes." << std::endl;
				m_networks[layerIndex].finalizeAndCheckNetwork(maxNumNodes);
			}
		}
	}

	std::cout << "Generating memory network... " << std::flush;

	// First generate memory links from intra links (from ordinary links within each network)
	for (unsigned int layerIndex = 0; layerIndex < m_networks.size(); ++layerIndex)
	{
		const LinkMap& linkMap = m_networks[layerIndex].linkMap();
		for (LinkMap::const_iterator linkIt(linkMap.begin()); linkIt != linkMap.end(); ++linkIt)
		{
			unsigned int n1 = linkIt->first;
			const std::map<unsigned int, double>& subLinks = linkIt->second;
			for (std::map<unsigned int, double>::const_iterator subIt(subLinks.begin()); subIt != subLinks.end(); ++subIt)
			{
				unsigned int n2 = subIt->first;
				double linkWeight = subIt->second;

				m_m2Links[make_pair(M2Node(layerIndex, n1), M2Node(layerIndex, n2))] += linkWeight;
				m_totM2LinkWeight += linkWeight;

				if (m_config.includeSelfLinks || n1 != n2)
				{
					m_m2Nodes[M2Node(layerIndex, n1)] += linkWeight; // -> total weighted out-degree
				}
				m_m2Nodes[M2Node(layerIndex, n2)] += 0.0;
			}
		}
	}

	// Then generate memory links from inter links (links between nodes in different layers)
	for (std::map<InterLinkKey, double>::const_iterator interIt(m_interLinks.begin()); interIt != m_interLinks.end(); ++interIt)
	{
		const InterLinkKey& interLink = interIt->first;
		unsigned int layer1 = interLink.layer1;
		unsigned int layer2 = interLink.layer2;
		unsigned int nodeIndex = interLink.nodeIndex;
		double linkWeight = interIt->second;
		if (layer1 != layer2)
		{
			//TODO: Rescale with self-layer weight if possible
			m_m2Links[make_pair(M2Node(layer1, nodeIndex), M2Node(layer2, nodeIndex))] += linkWeight;
			m_totM2LinkWeight += linkWeight;
			// Create corresponding memory nodes if not exist
			m_m2Nodes[M2Node(layer1, nodeIndex)] += 0.0;
			m_m2Nodes[M2Node(layer2, nodeIndex)] += 0.0;
		}
	}

	unsigned int M2nodeNr = 0;
	for(map<M2Node,double>::iterator it = m_m2Nodes.begin(); it != m_m2Nodes.end(); ++it)
	{
		m_m2NodeMap[it->first] = M2nodeNr;
		M2nodeNr++;
	}

	m_m2NodeWeights.resize(m_m2Nodes.size());
	m_totM2NodeWeight = 0.0;

	M2nodeNr = 0;
	for(map<M2Node,double>::iterator it = m_m2Nodes.begin(); it != m_m2Nodes.end(); ++it)
	{
		m_m2NodeWeights[M2nodeNr] += it->second;
		m_totM2NodeWeight += it->second;
		++M2nodeNr;
	}

	std::cout << "done! Generated " << m_m2Nodes.size() << " memory nodes and " << m_m2Links.size() << " memory links. ";
	std::cout << std::endl;
}

void MultiplexNetwork::parseIntraLink(const std::string& line, unsigned int& level, unsigned int& n1, unsigned int& n2, double& weight)
{
	m_extractor.clear();
	m_extractor.str(line);
	if (!(m_extractor >> level >> n1 >> n2))
		throw FileFormatError(io::Str() << "Can't parse multiplex intra link data from line '" << line << "'");
	(m_extractor >> weight) || (weight = 1.0);
}

void MultiplexNetwork::parseInterLink(const std::string& line, unsigned int& node, unsigned int& level1, unsigned int& level2, double& weight)
{
	m_extractor.clear();
	m_extractor.str(line);
	if (!(m_extractor >> node >> level1 >> level2))
		throw FileFormatError(io::Str() << "Can't parse multiplex intra link data from line '" << line << "'");
	(m_extractor >> weight) || (weight = 1.0);
}


