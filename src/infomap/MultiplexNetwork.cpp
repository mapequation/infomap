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


#include "MultiplexNetwork.h"
#include "../utils/FileURI.h"
#include "../io/convert.h"
#include "../io/SafeFile.h"
#include "../utils/Logger.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdio>

using std::make_pair;

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

			while (m_networks.size() < level + 1)
				m_networks.push_back(Network(m_config));

			m_networks[level].addLink(n1, n2, weight);

			++numIntraLinksFound;
		}
		else
		{
			unsigned int nodeIndex, layer1, layer2;
			double weight;

			parseInterLink(line, nodeIndex, layer1, layer2, weight);

			m_interLinks[M2Node(layer1, nodeIndex)][layer2] += weight;

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

	adjustForDifferentNumberOfNodes();

	std::cout << "Generating memory network... " << std::flush;

	bool useInterLayerData = numInterLinksFound > 0;
	if (useInterLayerData)
		generateMemoryNetworkWithInterLayerLinksFromData();
	else
		generateMemoryNetworkWithSimulatedInterLayerLinks();

	finalizeAndCheckNetwork();

	printParsingResult();
}

void MultiplexNetwork::adjustForDifferentNumberOfNodes()
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

}

void MultiplexNetwork::generateMemoryNetworkWithInterLayerLinksFromData()
{
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

				insertM2Link(layerIndex, n1, layerIndex, n2, linkWeight);

//				std::cout << "\nGenerating memory link (" << layerIndex << "," << n1 << ") -> (" << layerIndex << "," << n2 << ") with weight " << linkWeight;

				if (m_config.includeSelfLinks || n1 != n2)
				{
//					std::cout << "\n  -> Generating state node (" << layerIndex << "," << n1 << ") with weight!";
					addM2Node(layerIndex, n1, linkWeight); // -> total weighted out-degree
				}
//				std::cout << "\n  -> Generating state node (" << layerIndex << "," << n2 << ")";
				addM2Node(layerIndex, n2, 0.0);
			}
		}
	}

	std::cout << "connecting layers... " << std::flush;

	// Then generate memory links from inter links (links between nodes in different layers)
	for (std::map<M2Node, InterLinkMap>::const_iterator m2NodeIt(m_interLinks.begin()); m2NodeIt != m_interLinks.end(); ++m2NodeIt)
	{
		const M2Node& m2Node = m2NodeIt->first;
		M2LinkMap::iterator m2SourceIt = m_m2Links.lower_bound(m2Node);
		if (m2SourceIt == m_m2Links.end() || m2SourceIt->first != m2Node)
			m2SourceIt = m_m2Links.insert(--m2SourceIt, std::make_pair(m2Node, std::map<M2Node, double>())); // Insertion optimized if position hint preceeds the element to insert (C++98)
		unsigned int layer1 = m2Node.priorState;
		unsigned int nodeIndex = m2Node.physIndex;
		const InterLinkMap& interLinkMap = m2NodeIt->second;
		for (InterLinkMap::const_iterator interLinkIt(interLinkMap.begin()); interLinkIt != interLinkMap.end(); ++interLinkIt)
		{
			unsigned int layer2 = interLinkIt->first;
			double linkWeight = interLinkIt->second;
			if (layer2 != layer1)
			{
				// Switch to same physical node within other layer
	//			//TODO: Rescale with self-layer weight if possible
				bool nonPhysicalSwitch = false;
				if (nonPhysicalSwitch)
				{
					insertM2Link(m2SourceIt, layer2, nodeIndex, linkWeight);
					addM2Node(layer1, nodeIndex, 0.0);
					addM2Node(layer2, nodeIndex, 0.0);
				}
				else
				{
					const LinkMap& otherLayerLinks = m_networks[layer2].linkMap();
					// Distribute inter-link to the outgoing intra-links of the node in the inter-linked layer
					LinkMap::const_iterator otherLayerOutLinkIt = otherLayerLinks.find(nodeIndex);

					if (otherLayerOutLinkIt != otherLayerLinks.end())
					{
						const std::map<unsigned int, double>& otherLayerOutLinks = otherLayerOutLinkIt->second;
						double sumLinkWeightOtherLayer = 0.0;
						for (std::map<unsigned int, double>::const_iterator interIntraIt(otherLayerOutLinks.begin()); interIntraIt != otherLayerOutLinks.end(); ++interIntraIt)
						{
							sumLinkWeightOtherLayer += interIntraIt->second;
						}

						for (std::map<unsigned int, double>::const_iterator interIntraIt(otherLayerOutLinks.begin()); interIntraIt != otherLayerOutLinks.end(); ++interIntraIt)
						{
							unsigned int otherLayerTargetNodeIndex = interIntraIt->first;
							double otherLayerLinkWeight = interIntraIt->second;

							double interIntraLinkWeight = linkWeight * otherLayerLinkWeight / sumLinkWeightOtherLayer;
							insertM2Link(m2SourceIt, layer2, otherLayerTargetNodeIndex, interIntraLinkWeight);

							addM2Node(layer1, nodeIndex, 0.0);
							addM2Node(layer2, otherLayerTargetNodeIndex, 0.0);
						}
					}
				}
			}
		}
	}
}

void MultiplexNetwork::generateMemoryNetworkWithSimulatedInterLayerLinks()
{
	// Simulate inter-layer links
	double aggregationRate = 1.0;

	std::cout << "Generating memory network with aggregation rate " << aggregationRate << "... " << std::flush;

	std::vector<double> physNodeWeights(m_numNodes, 0.0);

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
				double linkWeight = subIt->second;
				physNodeWeights[n1] += linkWeight;
			}
		}
	}

	for (unsigned int nodeIndex = 0; nodeIndex < m_numNodes; ++nodeIndex)
	{
		for (unsigned int layer1 = 0; layer1 < m_networks.size(); ++layer1)
		{
			for (unsigned int layer2 = 0; layer2 < m_networks.size(); ++layer2)
			{
				// Distribute inter-link to the outgoing intra-links of the node in the inter-linked layer
				const LinkMap& linkMap = m_networks[layer2].linkMap();
				LinkMap::const_iterator otherLayerLinkIt = linkMap.find(nodeIndex);

				if (otherLayerLinkIt != linkMap.end())
				{
					const std::map<unsigned int, double>& otherLayerIntraLinks = otherLayerLinkIt->second;
					double sumLinkWeightOtherLayer = 0.0;
					for (std::map<unsigned int, double>::const_iterator interIntraIt(otherLayerIntraLinks.begin()); interIntraIt != otherLayerIntraLinks.end(); ++interIntraIt)
					{
						sumLinkWeightOtherLayer += interIntraIt->second;
					}

					for (std::map<unsigned int, double>::const_iterator interIntraIt(otherLayerIntraLinks.begin()); interIntraIt != otherLayerIntraLinks.end(); ++interIntraIt)
					{
						unsigned int n2 = interIntraIt->first;
						double linkWeightOtherLayer = interIntraIt->second;

						double interIntraLinkWeight = aggregationRate * linkWeightOtherLayer / physNodeWeights[nodeIndex];
//						std::cout << "\nCreating link " << M2Node(layer1, nodeIndex) << " -> " << M2Node(layer2, n2) << " with weight: " << interIntraLinkWeight << "  ";
						insertM2Link(layer1, nodeIndex, layer2, n2, interIntraLinkWeight);

						addM2Node(layer1, nodeIndex, interIntraLinkWeight);
						addM2Node(layer2, n2, 0.0);
					}
				}
			}
		}
	}
}

void MultiplexNetwork::finalizeAndCheckNetwork()
{
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
}

void MultiplexNetwork::printParsingResult()
{
	std::cout << "done! Generated " << m_m2Nodes.size() << " memory nodes and " << m_numM2Links << " memory links. ";
	if (m_numAggregatedM2Links)
		std::cout << "Aggregated " << m_numAggregatedM2Links << " memory links. ";
	std::cout << std::endl;
}

void MultiplexNetwork::parseIntraLink(const std::string& line, unsigned int& level, unsigned int& n1, unsigned int& n2, double& weight)
{
	m_extractor.clear();
	m_extractor.str(line);
	if (!(m_extractor >> level >> n1 >> n2))
		throw FileFormatError(io::Str() << "Can't parse multiplex intra link data from line '" << line << "'");
	(m_extractor >> weight) || (weight = 1.0);
	level -= m_indexOffset;
	n1 -= m_indexOffset;
	n2 -= m_indexOffset;
}

void MultiplexNetwork::parseInterLink(const std::string& line, unsigned int& node, unsigned int& level1, unsigned int& level2, double& weight)
{
	m_extractor.clear();
	m_extractor.str(line);
	if (!(m_extractor >> node >> level1 >> level2))
		throw FileFormatError(io::Str() << "Can't parse multiplex intra link data from line '" << line << "'");
	(m_extractor >> weight) || (weight = 1.0);
	node -= m_indexOffset;
	level1 -= m_indexOffset;
	level2 -= m_indexOffset;
}


