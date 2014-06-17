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

void MultiplexNetwork::readInputData(std::string filename)
{
	if (filename.empty())
		filename = m_config.networkFile;
	if (m_config.inputFormat == "multiplex")
		parseMultiplexNetwork(m_config.networkFile);
	else if (m_config.additionalInput.size() > 0)
		parseMultipleNetworks();
	else
		throw ImplementationError("No multiplex identified.");
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

	bool simulateInterLayerLinks = m_config.multiplexAggregationRate >= 0.0 || numInterLinksFound == 0;
	if (simulateInterLayerLinks)
		generateMemoryNetworkWithSimulatedInterLayerLinks();
	else
		generateMemoryNetworkWithInterLayerLinksFromData();

	finalizeAndCheckNetwork();

	printParsingResult(false);
}

void MultiplexNetwork::parseMultipleNetworks()
{
	std::vector<std::string> networkFilenames;
	networkFilenames.push_back(m_config.networkFile);
	for (unsigned int i = 0; i < m_config.additionalInput.size(); ++i)
		networkFilenames.push_back(m_config.additionalInput[i]);

	for (unsigned int i = 0; i < networkFilenames.size(); ++i)
	{
		m_networks.push_back(Network(m_config));
		std::cout << "[Network layer " << (i + 1) << " from file '" << networkFilenames[i] << "']:\n";
		m_networks[i].readInputData(networkFilenames[i]);
	}

	adjustForDifferentNumberOfNodes();

	unsigned int numInterLinksFound = 0; //TODO: Assume last additional data is inter-layer data if #additionalInputs > 1 && multiplexAggregationRate < 0

	std::cout << "Generating memory network... " << std::flush;

	bool simulateInterLayerLinks = m_config.multiplexAggregationRate >= 0.0 || numInterLinksFound == 0;
	if (simulateInterLayerLinks)
		generateMemoryNetworkWithSimulatedInterLayerLinks();
	else
		generateMemoryNetworkWithInterLayerLinksFromData();

	finalizeAndCheckNetwork();

	printParsingResult(false);
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
	std::cout << "Generating memory network with inter layer links from data... " << std::flush;
	// First generate memory links from intra links (from ordinary links within each network)
	std::vector<std::vector<double> > sumOutWeights(m_networks.size());

	for (unsigned int layerIndex = 0; layerIndex < m_networks.size(); ++layerIndex)
	{
		sumOutWeights[layerIndex].assign(m_numNodes, 0.0);
		const LinkMap& linkMap = m_networks[layerIndex].linkMap();
		for (LinkMap::const_iterator linkIt(linkMap.begin()); linkIt != linkMap.end(); ++linkIt)
		{
			unsigned int n1 = linkIt->first;
			const std::map<unsigned int, double>& subLinks = linkIt->second;
			for (std::map<unsigned int, double>::const_iterator subIt(subLinks.begin()); subIt != subLinks.end(); ++subIt)
			{
				unsigned int n2 = subIt->first;
				double linkWeight = subIt->second;

				sumOutWeights[layerIndex][n1] += linkWeight;

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


	// Extract the self-layer links to be able to scale the inter-layer links correctly. Use the sum of intra out weights as default.
	std::vector<std::vector<double> > selfLayerWeights(sumOutWeights);
	for (std::map<M2Node, InterLinkMap>::const_iterator m2NodeIt(m_interLinks.begin()); m2NodeIt != m_interLinks.end(); ++m2NodeIt)
	{
		const M2Node& m2Node = m2NodeIt->first;
		unsigned int layer1 = m2Node.priorState;
		unsigned int nodeIndex = m2Node.physIndex;
		const InterLinkMap& interLinkMap = m2NodeIt->second;
		for (InterLinkMap::const_iterator interLinkIt(interLinkMap.begin()); interLinkIt != interLinkMap.end(); ++interLinkIt)
		{
			unsigned int layer2 = interLinkIt->first;
			double linkWeight = interLinkIt->second;
			if (layer2 == layer1)
			{
				selfLayerWeights[layer1][nodeIndex] = linkWeight;
			}
		}
	}

	// Then generate memory links from inter links (links between nodes in different layers)
	for (std::map<M2Node, InterLinkMap>::const_iterator m2NodeIt(m_interLinks.begin()); m2NodeIt != m_interLinks.end(); ++m2NodeIt)
	{
		const M2Node& m2Node = m2NodeIt->first;
		M2LinkMap::iterator m2SourceIt = m_m2Links.lower_bound(m2Node);
		if (m2SourceIt == m_m2Links.end() || m2SourceIt->first != m2Node)
			m2SourceIt = m_m2Links.insert(m2SourceIt, std::make_pair(m2Node, std::map<M2Node, double>())); // TODO: Use C++11 for optimized insertion with hint from lower_bound
		unsigned int layer1 = m2Node.priorState;
		unsigned int nodeIndex = m2Node.physIndex;
		const InterLinkMap& interLinkMap = m2NodeIt->second;
		for (InterLinkMap::const_iterator interLinkIt(interLinkMap.begin()); interLinkIt != interLinkMap.end(); ++interLinkIt)
		{
			unsigned int layer2 = interLinkIt->first;
			if (layer2 != layer1)
			{
				double interLinkWeight = interLinkIt->second;
	//			//Rescale with self-layer weight if possible
				double scaledInterLinkWeight = sumOutWeights[layer1][nodeIndex] / selfLayerWeights[layer1][nodeIndex] * interLinkWeight;

				// Switch to same physical node within other layer
				bool nonPhysicalSwitch = false;
				if (nonPhysicalSwitch)
				{
					insertM2Link(m2SourceIt, layer2, nodeIndex, scaledInterLinkWeight);
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
						for (std::map<unsigned int, double>::const_iterator interIntraIt(otherLayerOutLinks.begin()); interIntraIt != otherLayerOutLinks.end(); ++interIntraIt)
						{
							unsigned int otherLayerTargetNodeIndex = interIntraIt->first;
							double otherLayerLinkWeight = interIntraIt->second;

							double interIntraLinkWeight = scaledInterLinkWeight * otherLayerLinkWeight / sumOutWeights[layer2][nodeIndex];
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
	double aggregationRate = m_config.multiplexAggregationRate < 0 ? 0.15 : m_config.multiplexAggregationRate; //TODO: Set default in config and use separate bool

	std::cout << "Generating memory network with aggregation rate " << aggregationRate << "... " << std::flush;

	// First generate memory links from intra links (from ordinary links within each network)
	for (unsigned int nodeIndex = 0; nodeIndex < m_numNodes; ++nodeIndex)
	{
		double sumOutLinkWeightAllLayers = 0.0;
		for (unsigned int i = 0; i< m_networks.size(); ++i)
			sumOutLinkWeightAllLayers += m_networks[i].sumLinkOutWeight()[nodeIndex];

//		std::cout << "\n==== Node " << nodeIndex << " ====";
		for (unsigned int layer1 = 0; layer1 < m_networks.size(); ++layer1)
		{
//			std::cout << "\n---- 1st layer: " << layer1;
			M2Node m2Source(layer1, nodeIndex);

			const LinkMap& layer1LinkMap = m_networks[layer1].linkMap();
			LinkMap::const_iterator layer1OutLinksIt = layer1LinkMap.find(nodeIndex);

			if (layer1OutLinksIt == layer1LinkMap.end())
			{
//				std::cout << " ---> SKIPPING DANGLING NODE " << m2Source;
				continue;
			}

			double sumOutLinkWeightLayer1 = m_networks[layer1].sumLinkOutWeight()[nodeIndex];

//			M2LinkMap::iterator m2SourceIt = m_m2Links.lower_bound(m2Source);
//			if (m2SourceIt == m_m2Links.end() || m2SourceIt->first != m2Source)
//				m2SourceIt = m_m2Links.insert(m2SourceIt, std::make_pair(m2Source, std::map<M2Node, double>())); // TODO: Use C++11 for optimized insertion with hint from lower_bound

			for (unsigned int layer2 = 0; layer2 < m_networks.size(); ++layer2)
			{
				// Distribute inter-link to the outgoing intra-links of the node in the inter-linked layer
//				std::cout << "\n---- 2nd layer: " << layer2;
				bool isIntra = layer2 == layer1;
				LinkMap::const_iterator layer2OutLinksIt = layer1OutLinksIt;
				if (!isIntra)
				{
					const LinkMap& layer2LinkMap = m_networks[layer2].linkMap();
					layer2OutLinksIt = layer2LinkMap.find(nodeIndex);
					if (layer2OutLinksIt == layer2LinkMap.end())
					{
//						std::cout << "\n  No mirror to node " << m2Source << " on layer " << layer2;
						continue;
					}
				}

				const std::map<unsigned int, double>& layer2OutLinks = layer2OutLinksIt->second;

				for (std::map<unsigned int, double>::const_iterator outLinkIt(layer2OutLinks.begin()); outLinkIt != layer2OutLinks.end(); ++outLinkIt)
				{
					unsigned int n2 = outLinkIt->first;
					double linkWeight = outLinkIt->second;

					double intraLinkWeight = isIntra ? linkWeight : 0.0;

					double aggregatedLinkWeight = aggregationRate * linkWeight / sumOutLinkWeightAllLayers  + (1.0 - aggregationRate) * intraLinkWeight / sumOutLinkWeightLayer1;
//					std::cout << "\n  Creating link " << M2Node(layer1, nodeIndex) << " -> " << M2Node(layer2, n2) << " with weight: " << aggregatedLinkWeight << "  ";
					insertM2Link(layer1, nodeIndex, layer2, n2, aggregatedLinkWeight);
//					insertM2Link(m2SourceIt, layer2, n2, aggregatedLinkWeight);

					if (m_config.includeSelfLinks || n2 != nodeIndex)
						addM2Node(layer1, nodeIndex, intraLinkWeight);
					addM2Node(layer2, n2, 0.0);
				}
			}
		}
	}
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


