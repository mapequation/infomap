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

#include "../utils/infomath.h"

#ifdef NS_INFOMAP
namespace infomap
{
#endif

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

/**
 * Example of general multiplex network:
 *
 * *Vertices 4
 * 1 "Node 1"
 * 2 "Node 2"
 * 3 "Node 3"
 * 4 "Node 4"
 * *Multiplex
 * # layer node layer node [weight]
 * 1 1 1 2 2
 * 1 1 2 2 1
 * 1 2 1 1 1
 * 1 3 2 2 1
 * 2 2 1 3 1
 * 2 3 2 2 1
 * 2 4 2 1 2
 * 2 4 1 2 1
 *
 * The *Vertices section and the *Multiplex line can be omitted.
 * More compact sections can be defined for intra-network links
 * and inter-network links by adding *Intra and *Inter lines
 * like below:
 *
 * *Intra
 * # layer node node weight
 * 1 1 2 1
 * 1 2 1 1
 * 1 2 3 1
 * 1 3 2 1
 * 1 3 1 1
 * 1 1 3 1
 * 1 2 4 1
 * 1 4 2 1
 * 2 4 5 1
 * 2 5 4 1
 * 2 5 6 1
 * 2 6 5 1
 * 2 6 4 1
 * 2 4 6 1
 * 2 3 6 1
 * 2 6 3 1
 * *Inter
 * # layer node layer weight
 * 1 3 1 2
 * 1 3 2 1
 * 2 3 2 1
 * 2 3 1 1
 * 1 4 1 1
 * 1 4 2 1
 * 2 4 2 2
 * 2 4 1 1
 *
 * Note:
 * *Inter links define unrecorded movements:
 * Each inter-link (layer1 node1 layer2) is expanded to a set of
 * multiplex links {layer1 node1 layer2 node2} for all node2 that
 * node1 points to in the second layer.
 *
 */
void MultiplexNetwork::parseMultiplexNetwork(std::string filename)
{
	Log() << "Parsing multiplex network from file '" << filename << "'... " << std::flush;

	SafeInFile input(filename.c_str());

	// Assume general multiplex links
	string line = parseMultiplexLinks(input);

	while (line.length() > 0 && line[0] == '*')
	{
		std::string header = io::firstWord(line);
		if (header == "*Vertices" || header == "*vertices") {
			line = parseVertices(input, line);
		}
		else if (header == "*Intra" || header == "*intra") {
			line = parseIntraLinks(input);
		}
		else if (header == "*Inter" || header == "*inter") {
			line = parseInterLinks(input);
		}
		else if (header == "*Multiplex" || header == "*multiplex") {
			line = parseMultiplexLinks(input);
		}
		else
			throw FileFormatError(io::Str() << "Unrecognized header in multiplex network file: '" << line << "'.");
	}

	Log() << "done!\n";

	finalizeParser();

}

void MultiplexNetwork::finalizeParser(){
	if (!m_networks.empty())
		Log() << " --> Found " << m_numIntraLinksFound << " intra-network links in " << m_networks.size() << " layers.\n";
	if (!m_interLinkLayers.empty())
		Log() << " --> Found " << m_numInterLinksFound << " inter-network links in " << m_interLinkLayers.size() << " layers.\n";
	if (!m_multiplexLinkLayers.empty())
		Log() << " --> Found " << m_numMultiplexLinksFound << " multiplex links in " << m_multiplexLinkLayers.size() << " layers.\n";

	if (!m_interLinkLayers.empty())
	{
//		throw InputDomainError("Need at least two layers of network data for multiplex network.");
		unsigned int maxInterLinkLayer = (--m_interLinkLayers.end())->first + 1;
		if (m_networks.size() < maxInterLinkLayer)
			throw InputDomainError(io::Str() << "No intra-network data for inter-network links at layer " << maxInterLinkLayer << ".");
	}

	if (!m_networks.empty())
	{
		bool printLayerSummary = m_networks.size() <= 10 ||
				(m_networks.size() < 20 && infomath::isBetween(m_config.verbosity, 1, 2)) ||
				(m_networks.size() < 50 && infomath::isBetween(m_config.verbosity, 1, 3));
		// Finalize and check each layer of intra-network links
		for (unsigned int layerIndex = 0; layerIndex < m_networks.size(); ++layerIndex)
		{
			if (printLayerSummary)
				Log() << "Intra-network links on layer " << (layerIndex + 1) << ": " << std::flush;
			m_networks[layerIndex].finalizeAndCheckNetwork(false);
			if (printLayerSummary)
				m_networks[layerIndex].printParsingResult(m_config.verbosity <= 1);
		}

		m_numNodes = adjustForDifferentNumberOfNodes();
	}

	bool simulateInterLayerLinks = m_config.multiplexRelaxRate >= 0.0 || m_numInterLinksFound == 0;
	if (simulateInterLayerLinks)
		generateMemoryNetworkWithSimulatedInterLayerLinks();
	else
		generateMemoryNetworkWithInterLayerLinksFromData();

	addMemoryNetworkFromMultiplexLinks();

	finalizeAndCheckNetwork();
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
		Log() << "[Network layer " << (i + 1) << " from file '" << networkFilenames[i] << "']:\n";
		m_networks[i].readInputData(networkFilenames[i]);
	}

	m_numNodes = adjustForDifferentNumberOfNodes();

	unsigned int numInterLinksFound = 0; //TODO: Assume last additional data is inter-layer data if #additionalInputs > 1 && multiplexRelaxRate < 0

	Log() << "Generating memory network... " << std::flush;

	bool simulateInterLayerLinks = m_config.multiplexRelaxRate >= 0.0 || numInterLinksFound == 0;
	if (simulateInterLayerLinks)
		generateMemoryNetworkWithSimulatedInterLayerLinks();
	else
		generateMemoryNetworkWithInterLayerLinksFromData();

	finalizeAndCheckNetwork();
}

unsigned int MultiplexNetwork::adjustForDifferentNumberOfNodes()
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
		if (!m_networks[layerIndex].nodeNames().empty() && (m_nodeNames.empty() || numNodesInLayer > m_nodeNames.size()))
		{
			m_nodeNames.clear();
			m_networks[layerIndex].swapNodeNames(m_nodeNames);
		}
	}

	if (differentNodeCount)
	{
		Log() << "Adjusting for equal number of nodes... " << std::flush;
		unsigned int numAdjusted = 0;
		for (unsigned int layerIndex = 0; layerIndex < m_networks.size(); ++layerIndex)
		{
			if (m_networks[layerIndex].numNodes() != maxNumNodes)
			{
//				Log() << "  Layer " << (layerIndex + 1) << ": " <<
//						m_networks[layerIndex].numNodes() << " -> " << maxNumNodes << " nodes." << std::endl;
				++numAdjusted;
				m_networks[layerIndex].finalizeAndCheckNetwork(false, maxNumNodes);
			}
		}
		Log() << "done! Adjusted " << numAdjusted << "/" << m_networks.size() << " networks to have " << maxNumNodes << " nodes." << std::endl;
	}

	return maxNumNodes;
}

void MultiplexNetwork::generateMemoryNetworkWithInterLayerLinksFromData()
{
	Log() << "Generating memory network with inter layer links from data... " << std::flush;
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
				addM2Link(layerIndex, n1, layerIndex, n2, linkWeight);
			}
		}
	}

	Log() << "connecting layers... " << std::flush;


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

	unsigned int numInterLinksIgnored = 0;
	// Then generate memory links from inter links (links between nodes in different layers)
	for (std::map<M2Node, InterLinkMap>::const_iterator m2NodeIt(m_interLinks.begin()); m2NodeIt != m_interLinks.end(); ++m2NodeIt)
	{
		const M2Node& m2Node = m2NodeIt->first;
		M2LinkMap::iterator m2SourceIt = m_m2Links.lower_bound(m2Node);
		// Find source iterator to re-use in the loop below
		if (m2SourceIt == m_m2Links.end() || m2SourceIt->first != m2Node)
			m2SourceIt = m_m2Links.insert(m2SourceIt, std::make_pair(m2Node, std::map<M2Node, double>())); // TODO: Use C++11 for optimized insertion with hint from lower_bound
		bool m2SourceNodeAdded = false;
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
					addM2Link(m2SourceIt, layer2, nodeIndex, scaledInterLinkWeight, 0.0, 0.0);
					m2SourceNodeAdded = true;
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

							addM2Link(m2SourceIt, layer2, otherLayerTargetNodeIndex, interIntraLinkWeight, 0.0, 0.0);
							m2SourceNodeAdded = true;
						}
					}
				}
			}
			else {
				++numInterLinksIgnored;
				throw FileFormatError(io::Str() << "\nLink '" <<
					(layer1 + m_indexOffset) << ", " << (nodeIndex + m_indexOffset) << ", " << (layer2 + m_indexOffset) <<
					"' is declared as an inter-layer link (layer1, node, layer2) but is not.");
			}
		}
		if (!m2SourceNodeAdded) {
			// The added source was not used, remove it
			m_m2Links.erase(m2SourceIt);
		}
	}
	Log() << "done!" << std::endl;
	if (numInterLinksIgnored > 0) {
		Log() << "  -> Warning: " << numInterLinksIgnored << " links defined as inter-layer links was same-layer links and ignored.\n";
	}
}

void MultiplexNetwork::generateMemoryNetworkWithSimulatedInterLayerLinks()
{
	// Simulate inter-layer links
	double relaxRate = m_config.multiplexRelaxRate < 0 ? 0.15 : m_config.multiplexRelaxRate; //TODO: Set default in config and use separate bool

	Log() << "Generating memory network with multiplex relax rate " << relaxRate << "... " << std::flush;

	for (unsigned int nodeIndex = 0; nodeIndex < m_numNodes; ++nodeIndex)
	{

		unsigned int layer2from = 0;
		unsigned int layer2to = m_networks.size();
		double sumOutLinkWeightAllLayers = 0.0;
		for (unsigned int i = layer2from; i < layer2to; ++i)
			sumOutLinkWeightAllLayers += m_networks[i].sumLinkOutWeight()[nodeIndex];

		for (unsigned int layer1 = 0; layer1 < m_networks.size(); ++layer1)
		{

			if(m_config.multiplexRelaxLimit >= 0){
				layer2from = ((int)layer1-m_config.multiplexRelaxLimit) < 0 ? 0 : layer1-m_config.multiplexRelaxLimit;
				layer2to = (layer1+m_config.multiplexRelaxLimit) > m_networks.size() ? m_networks.size() : layer1+m_config.multiplexRelaxLimit;
				sumOutLinkWeightAllLayers = 0.0;
				for (unsigned int i = layer2from; i < layer2to; ++i)
					sumOutLinkWeightAllLayers += m_networks[i].sumLinkOutWeight()[nodeIndex];
			}

			M2Node m2Source(layer1, nodeIndex);

			const LinkMap& layer1LinkMap = m_networks[layer1].linkMap();
			LinkMap::const_iterator layer1OutLinksIt = layer1LinkMap.find(nodeIndex);

			// Skip dangling nodes
			if (layer1OutLinksIt == layer1LinkMap.end())
				continue;

			double sumOutLinkWeightLayer1 = m_networks[layer1].sumLinkOutWeight()[nodeIndex];

//			M2LinkMap::iterator m2SourceIt = m_m2Links.lower_bound(m2Source);
//			if (m2SourceIt == m_m2Links.end() || m2SourceIt->first != m2Source)
//				m2SourceIt = m_m2Links.insert(m2SourceIt, std::make_pair(m2Source, std::map<M2Node, double>())); // TODO: Use C++11 for optimized insertion with hint from lower_bound

			for (unsigned int layer2 = layer2from; layer2 < layer2to; ++layer2)
			{
				// Distribute inter-link to the outgoing intra-links of the node in the inter-linked layer
				bool isIntra = layer2 == layer1;
				LinkMap::const_iterator layer2OutLinksIt = layer1OutLinksIt;
				if (!isIntra)
				{
					const LinkMap& layer2LinkMap = m_networks[layer2].linkMap();
					layer2OutLinksIt = layer2LinkMap.find(nodeIndex);
					if (layer2OutLinksIt == layer2LinkMap.end())
					{
//						Log() << "\n  No mirror to node " << m2Source << " on layer " << layer2;
						continue;
					}
				}

				const std::map<unsigned int, double>& layer2OutLinks = layer2OutLinksIt->second;

				for (std::map<unsigned int, double>::const_iterator outLinkIt(layer2OutLinks.begin()); outLinkIt != layer2OutLinks.end(); ++outLinkIt)
				{
					unsigned int n2 = outLinkIt->first;
					double linkWeight = outLinkIt->second;

					double intraLinkWeight = isIntra ? linkWeight : 0.0;

					double aggregatedLinkWeight = relaxRate * linkWeight / sumOutLinkWeightAllLayers  + (1.0 - relaxRate) * intraLinkWeight / sumOutLinkWeightLayer1;

					addM2Link(layer1, nodeIndex, layer2, n2, aggregatedLinkWeight, intraLinkWeight, 0.0);
				}
			}
		}
	}
	Log() << "done!" << std::endl;
}

void MultiplexNetwork::addMemoryNetworkFromMultiplexLinks()
{
	if (m_multiplexLinks.empty())
		return;
	Log() << "Adding memory network from multiplex links... " << std::flush;

	for (MultiplexLinkMap::const_iterator it(m_multiplexLinks.begin()); it != m_multiplexLinks.end(); ++it)
	{
		const M2Node& source(it->first);
		const std::map<M2Node, double>& subLinks(it->second);
		for (std::map<M2Node, double>::const_iterator subIt(subLinks.begin()); subIt != subLinks.end(); ++subIt)
		{
			const M2Node& target(subIt->first);
			double linkWeight = subIt->second;
			addM2Link(source.priorState, source.physIndex, target.priorState, target.physIndex, linkWeight);
		}
	}
	Log() << "done!" << std::endl;
}

std::string MultiplexNetwork::parseIntraLinks(std::ifstream& file)
{
	std::string line;
	while(!std::getline(file, line).fail())
	{
		if (line.length() == 0 || line[0] == '#')
			continue;

		if (line[0] == '*')
			break;

		unsigned int layerIndex, n1, n2;
		double weight;

		parseIntraLink(line, layerIndex, n1, n2, weight);

		while (m_networks.size() < layerIndex + 1)
			m_networks.push_back(Network(m_config));

		m_networks[layerIndex].addLink(n1, n2, weight);

		++m_numIntraLinksFound;
	}
	return line;
}

std::string MultiplexNetwork::parseInterLinks(std::ifstream& file)
{
	std::string line;
	while(!std::getline(file, line).fail())
	{
		if (line.length() == 0 || line[0] == '#')
			continue;

		if (line[0] == '*')
			break;

		unsigned int nodeIndex, layer1, layer2;
		double weight;

		parseInterLink(line, layer1, nodeIndex, layer2, weight);

		m_interLinks[M2Node(layer1, nodeIndex)][layer2] += weight;

		++m_numInterLinksFound;
		++m_interLinkLayers[layer1];
		++m_interLinkLayers[layer2];
	}
	return line;
}

void MultiplexNetwork::addMultiplexLink(int layer1, int node1, int layer2, int node2, double weight){
	m_multiplexLinks[M2Node(layer1, node1)][M2Node(layer2, node2)] += weight;

	++m_numMultiplexLinksFound;
	++m_multiplexLinkLayers[layer1];
	++m_multiplexLinkLayers[layer2];

}

std::string MultiplexNetwork::parseMultiplexLinks(std::ifstream& file)
{
	std::string line;
	while(!std::getline(file, line).fail())
	{
		if (line.length() == 0 || line[0] == '#')
			continue;

		if (line[0] == '*')
			break;

		unsigned int layer1, node1, layer2, node2;
		double weight;

		parseMultiplexLink(line, layer1, node1, layer2, node2, weight);

		addMultiplexLink(layer1, node1, layer2, node2, weight);

	}
	return line;
}

/**
 * Link within the layer:
 *
 *   layer node node [weight]
 */
void MultiplexNetwork::parseIntraLink(const std::string& line, unsigned int& layerIndex, unsigned int& n1, unsigned int& n2, double& weight)
{
	m_extractor.clear();
	m_extractor.str(line);
	if (!(m_extractor >> layerIndex >> n1 >> n2))
		throw FileFormatError(io::Str() << "Can't parse multiplex intra link data (layer node1 node2) from line '" << line << "'");
	(m_extractor >> weight) || (weight = 1.0);
	layerIndex -= m_indexOffset;
	n1 -= m_indexOffset;
	n2 -= m_indexOffset;
}

/**
 * Link between layers:
 *
 *   layer node layer [weight]
 */
void MultiplexNetwork::parseInterLink(const std::string& line, unsigned int& layer1, unsigned int& node, unsigned int& layer2, double& weight)
{
	m_extractor.clear();
	m_extractor.str(line);
	if (!(m_extractor >> layer1 >> node >> layer2))
		throw FileFormatError(io::Str() << "Can't parse multiplex inter link data (layer1 node layer2) from line '" << line << "'");
	(m_extractor >> weight) || (weight = 1.0);
	layer1 -= m_indexOffset;
	node -= m_indexOffset;
	layer2 -= m_indexOffset;
}

/**
 * Parse general multiplex link:
 *
 *   layer1 node1 layer2 node2 [weight]
 */
void MultiplexNetwork::parseMultiplexLink(const std::string& line, unsigned int& layer1, unsigned int& node1, unsigned int& layer2, unsigned int& node2, double& weight)
{
	m_extractor.clear();
	m_extractor.str(line);
	if (!(m_extractor >> layer1 >> node1 >> layer2 >> node2))
		throw FileFormatError(io::Str() << "Can't parse multiplex link data (layer1 node1 layer2 node2) from line '" << line << "'");
	(m_extractor >> weight) || (weight = 1.0);
	layer1 -= m_indexOffset;
	node1 -= m_indexOffset;
	layer2 -= m_indexOffset;
	node2 -= m_indexOffset;
}

void MultiplexNetwork::finalizeAndCheckNetwork(bool printSummary)
{
	// First dispose intermediate data structures to clear memory
	m_interLinks.clear();
	m_networks.clear();

	MemNetwork::finalizeAndCheckNetwork(printSummary);
}

#ifdef NS_INFOMAP
}
#endif
