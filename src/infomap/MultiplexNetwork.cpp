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
	if (m_config.inputFormat == "multilayer" || m_config.inputFormat == "multiplex")
		parseMultiplexNetwork(filename);
	else if (m_config.additionalInput.size() > 0)
		parseMultipleNetworks();
	else {
		// throw ImplementationError("No multiplex identified.");
		MemNetwork::readInputData(filename);
	}
}

/**
 * Example of general multilayer network:
 *
 * *Vertices 4
 * 1 "Node 1"
 * 2 "Node 2"
 * 3 "Node 3"
 * 4 "Node 4"
 * *Multilayer
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
 * The *Vertices section and the *Multilayer line can be omitted.
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
 * multilayer links {layer1 node1 layer2 node2} for all node2 that
 * node1 points to in the second layer.
 *
 */
void MultiplexNetwork::parseMultiplexNetwork(std::string filename)
{
	Log() << "Parsing multilayer network from file '" << filename << "'... " << std::flush;

	SafeInFile input(filename.c_str());

	// Assume general multilayer links
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
		else if (header == "*Multilayer" || header == "*multilayer") {
			line = parseMultiplexLinks(input);
		}
		else if (header == "*Multiplex" || header == "*multiplex") {
			Log() << "\nWarning: Header '*Multiplex' has been deprecated, use *Multilayer\n";
			line = parseMultiplexLinks(input);
		}
		else
			throw FileFormatError(io::Str() << "Unrecognized header in multilayer network file: '" << line << "'.");
	}

	Log() << "done!\n";

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

	bool simulateInterLayerLinks = m_config.multiplexJSRelaxRate >= 0.0 ||  m_config.multiplexRelaxRate >= 0.0 || numInterLinksFound == 0;
	if (simulateInterLayerLinks){
		if(m_config.multiplexJSRelaxRate >= 0.0)
			generateMemoryNetworkWithJensenShannonSimulatedInterLayerLinks();
		else
			generateMemoryNetworkWithSimulatedInterLayerLinks();
	}
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

	if (m_nodeNames.size() > 0 && m_nodeNames.size() < maxNumNodes) {
		m_nodeNames.reserve(maxNumNodes);
		for (unsigned int i = m_nodeNames.size(); i < maxNumNodes; ++i) {
			m_nodeNames.push_back(io::Str() << "_completion_node_" << (i + 1));
		}
	}

	if (differentNodeCount && m_config.multiplexAddMissingNodes)
	{
		Log() << "Adjusting to same set of physical nodes in each layer... " << std::flush;
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

bool MultiplexNetwork::createIntraLinksToNeighbouringNodesInTargetLayer(StateLinkMap::iterator stateSourceIt,
	unsigned int nodeIndex, unsigned int targetLayer, const LinkMap& targetLayerLinks,
	double linkWeightNormalizationFactor, double stateNodeWeightNormalizationFactor) {
	// Distribute inter-links to outgoing intra-links in the target layer
	LinkMap::const_iterator targetLayerOutLinkIt = targetLayerLinks.find(nodeIndex);
	bool linkAdded = false;
	if (targetLayerOutLinkIt != targetLayerLinks.end())
	{
		const std::map<unsigned int, double>& targetLayerOutLinks = targetLayerOutLinkIt->second;
		for (std::map<unsigned int, double>::const_iterator interIntraIt(targetLayerOutLinks.begin()); interIntraIt != targetLayerOutLinks.end(); ++interIntraIt)
		{
			unsigned int targetLayerTargetNodeIndex = interIntraIt->first;
			double targetLayerLinkWeight = interIntraIt->second;

			// double interIntraLinkWeight = scaledInterLinkWeight * targetLayerLinkWeight / sumOutWeights[layer2][nodeIndex];
			double interIntraLinkWeight = linkWeightNormalizationFactor * targetLayerLinkWeight;
			double stateNodeWeight = stateNodeWeightNormalizationFactor * targetLayerLinkWeight;

			// Add weight also as teleportation weight to first state node
			addStateLink(stateSourceIt, targetLayer, targetLayerTargetNodeIndex, interIntraLinkWeight, stateNodeWeight, 0.0);
			linkAdded = true;
		}
	}
	return linkAdded;
}

bool MultiplexNetwork::createIntraLinksToNeighbouringNodesInTargetLayer(unsigned int sourceLayer,
	unsigned int nodeIndex, unsigned int targetLayer, const LinkMap& targetLayerLinks,
	double linkWeightNormalizationFactor, double stateNodeWeightNormalizationFactor) {
	// Distribute inter-links to outgoing intra-links in the target layer
	LinkMap::const_iterator targetLayerOutLinkIt = targetLayerLinks.find(nodeIndex);
	bool linkAdded = false;
	// Log() << "    -> add state link between (" << sourceLayer << "," << nodeIndex << ") to (" << targetLayer << ", *)\n";
	if (targetLayerOutLinkIt != targetLayerLinks.end())
	{
		const std::map<unsigned int, double>& targetLayerOutLinks = targetLayerOutLinkIt->second;
		for (std::map<unsigned int, double>::const_iterator interIntraIt(targetLayerOutLinks.begin()); interIntraIt != targetLayerOutLinks.end(); ++interIntraIt)
		{
			unsigned int targetLayerTargetNodeIndex = interIntraIt->first;
			double targetLayerLinkWeight = interIntraIt->second;

			double interIntraLinkWeight = linkWeightNormalizationFactor * targetLayerLinkWeight;
			double stateNodeWeight = stateNodeWeightNormalizationFactor * targetLayerLinkWeight;

			// Add weight also as teleportation weight to first state node
			addStateLink(sourceLayer, nodeIndex, targetLayer, targetLayerTargetNodeIndex, interIntraLinkWeight, stateNodeWeight, 0.0);
			// Log() << "      -> added state link (" << sourceLayer << "," << nodeIndex << ") to (" << targetLayer << "," << targetLayerTargetNodeIndex << "), interIntraLinkWeight: " << interIntraLinkWeight << ", stateNodeWeight: " << stateNodeWeight << "\n";
			linkAdded = true;
		}
	}
	return linkAdded;
}

void MultiplexNetwork::generateMemoryNetworkWithInterLayerLinksFromData()
{
	Log() << "Generating memory network with inter layer links from data... " << std::flush;
	// First generate memory links from intra links (from ordinary links within each network)
	std::vector<std::vector<double> > sumOutWeights(m_networks.size());

	std::vector<LinkMap> oppositeLinkMaps;
	if (m_config.isUndirected()) {
		oppositeLinkMaps.resize(m_networks.size());
		for (unsigned int i = 0; i < m_networks.size(); ++i) {
			m_networks[i].generateOppositeLinkMap(oppositeLinkMaps[i]);
		}
	}

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
				if (m_config.isUndirected()) {
					sumOutWeights[layerIndex][n2] += linkWeight;
				}
				addStateLink(layerIndex, n1, layerIndex, n2, linkWeight);
			}
		}
	}

	Log() << "connecting layers... " << std::flush;
	if (m_config.isUndirected()) {
		Log() << "using undirected intra-layer links... " << std::flush;
	}


	// Extract the self-layer links to be able to scale the inter-layer links correctly. Use the sum of intra out weights as default.
	std::vector<std::vector<double> > selfLayerWeights(sumOutWeights);
	for (std::map<StateNode, InterLinkMap>::const_iterator stateNodeIt(m_interLinks.begin()); stateNodeIt != m_interLinks.end(); ++stateNodeIt)
	{
		const StateNode& stateNode = stateNodeIt->first;
		unsigned int layer1 = stateNode.layer();
		unsigned int nodeIndex = stateNode.physIndex;
		const InterLinkMap& interLinkMap = stateNodeIt->second;
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
	for (std::map<StateNode, InterLinkMap>::const_iterator stateNodeIt(m_interLinks.begin()); stateNodeIt != m_interLinks.end(); ++stateNodeIt)
	{
		const StateNode& stateNode = stateNodeIt->first;
		StateLinkMap::iterator stateSourceIt = m_stateLinks.lower_bound(stateNode);
		// Find source iterator to re-use in the loop below
		if (stateSourceIt == m_stateLinks.end() || stateSourceIt->first != stateNode)
			stateSourceIt = m_stateLinks.insert(stateSourceIt, std::make_pair(stateNode, std::map<StateNode, double>())); // TODO: Use C++11 for optimized insertion with hint from lower_bound
		bool stateSourceNodeAdded = false;
		unsigned int layer1 = stateNode.layer();
		unsigned int nodeIndex = stateNode.physIndex;
		const InterLinkMap& interLinkMap = stateNodeIt->second;
		for (InterLinkMap::const_iterator interLinkIt(interLinkMap.begin()); interLinkIt != interLinkMap.end(); ++interLinkIt)
		{
			unsigned int layer2 = interLinkIt->first;
			if (layer2 != layer1)
			{
				double interLinkWeight = interLinkIt->second;
				double scaledInterLinkWeight = interLinkWeight;
				double scaledOppositeInterLinkWeight = interLinkWeight;
	//			//Rescale with self-layer weight if possible
				if (selfLayerWeights[layer1][nodeIndex] > 1e-10) {
					scaledInterLinkWeight *= sumOutWeights[layer1][nodeIndex] / selfLayerWeights[layer1][nodeIndex];
				}
				if (selfLayerWeights[layer2][nodeIndex] > 1e-10) {
					scaledOppositeInterLinkWeight *= sumOutWeights[layer2][nodeIndex] / selfLayerWeights[layer2][nodeIndex];
				}
				// Switch to same physical node within target layer
				bool nonPhysicalSwitch = false;
				if (nonPhysicalSwitch)
				{
					addStateLink(stateSourceIt, layer2, nodeIndex, scaledInterLinkWeight, scaledInterLinkWeight, 0.0);
					stateSourceNodeAdded = true;
				}
				else
				{
					// Jump to state note in target layer and distribute inter link to connected intra links
					double weightNormalizationFactor = scaledInterLinkWeight / sumOutWeights[layer2][nodeIndex];
					if (m_config.isUndirected()) {
						// Distribute inter-links to outgoing intra-links in the target layer
						bool add1 = createIntraLinksToNeighbouringNodesInTargetLayer(stateSourceIt, nodeIndex, layer2, m_networks[layer2].linkMap(), weightNormalizationFactor, weightNormalizationFactor);
						
						// Distribute inter-link to incoming intra-links in the target layer
						bool add2 = createIntraLinksToNeighbouringNodesInTargetLayer(stateSourceIt, nodeIndex, layer2, oppositeLinkMaps[layer2], weightNormalizationFactor, weightNormalizationFactor);

						// Distribute inter-links to outgoing intra-links in the source layer
						double oppositeWeightNormalizationFactor = scaledOppositeInterLinkWeight / sumOutWeights[layer1][nodeIndex];
						createIntraLinksToNeighbouringNodesInTargetLayer(layer2, nodeIndex, layer1, m_networks[layer1].linkMap(), oppositeWeightNormalizationFactor, oppositeWeightNormalizationFactor);
						
						// Distribute inter-link to incoming intra-links in the source layer
						createIntraLinksToNeighbouringNodesInTargetLayer(layer2, nodeIndex, layer1, oppositeLinkMaps[layer1], oppositeWeightNormalizationFactor, oppositeWeightNormalizationFactor);
						
						stateSourceNodeAdded = add1 | add2;
					}
					else {
						// Distribute inter-link to the outgoing intra-links in the target layer
						stateSourceNodeAdded = createIntraLinksToNeighbouringNodesInTargetLayer(stateSourceIt, nodeIndex, layer2, m_networks[layer2].linkMap(), weightNormalizationFactor, weightNormalizationFactor);
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
		if (!stateSourceNodeAdded) {
			// The added source was not used, remove it
			m_stateLinks.erase(stateSourceIt);
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

	Log() << "Generating memory network with multilayer relax rate " << relaxRate << "... " << std::flush;
	
	std::vector<LinkMap> oppositeLinkMaps;
	if (m_config.isUndirected()) {
		oppositeLinkMaps.resize(m_networks.size());
		for (unsigned int i = 0; i < m_networks.size(); ++i) {
			m_networks[i].generateOppositeLinkMap(oppositeLinkMaps[i]);
		}
	}


	for (unsigned int nodeIndex = 0; nodeIndex < m_numNodes; ++nodeIndex)
	{
		unsigned int layer2from = 0;
		unsigned int layer2to = m_networks.size();
		double sumOutLinkWeightAllLayers = 0.0;
		for (unsigned int i = layer2from; i < layer2to; ++i)
			sumOutLinkWeightAllLayers += m_networks[i].sumLinkOutWeight()[nodeIndex];
		// Log() << "\n===== Node index: " << nodeIndex << " / " << (m_numNodes - 1) << " ======" <<
		// "  sumOutLinkWeightAllLayers: " << sumOutLinkWeightAllLayers << "\n";

		for (unsigned int layer1 = 0; layer1 < m_networks.size(); ++layer1)
		{
			// Limit possible jumps to close by layers
			if(m_config.multiplexRelaxLimit >= 0){
				layer2from = ((int)layer1-m_config.multiplexRelaxLimit) < 0 ? 0 : layer1-m_config.multiplexRelaxLimit;
				layer2to = (layer1+m_config.multiplexRelaxLimit) > m_networks.size() ? m_networks.size() : layer1+m_config.multiplexRelaxLimit;
				sumOutLinkWeightAllLayers = 0.0;
				for (unsigned int i = layer2from; i < layer2to; ++i)
					sumOutLinkWeightAllLayers += m_networks[i].sumLinkOutWeight()[nodeIndex];
			}

			if (!m_networks[layer1].haveNode(nodeIndex)) {
				// Log() << "  SKIP layer " << layer1 << ", no node " << nodeIndex << "!\n";
				continue;
			}


			double sumOutLinkWeightLayer1 = m_networks[layer1].sumLinkOutWeight()[nodeIndex];

			// Log() << "  Layer " << layer1 << ", sumOutLinkWeight: " << sumOutLinkWeightLayer1 << "\n";

			// StateNode stateSource(layer1, nodeIndex);
//			StateLinkMap::iterator stateSourceIt = m_stateLinks.lower_bound(stateSource);
//			if (stateSourceIt == m_stateLinks.end() || stateSourceIt->first != stateSource)
//				stateSourceIt = m_stateLinks.insert(stateSourceIt, std::make_pair(stateSource, std::map<StateNode, double>())); // TODO: Use C++11 for optimized insertion with hint from lower_bound

			// Create inter-links to the intra-connected nodes in other layers
			for (unsigned int layer2 = layer2from; layer2 < layer2to; ++layer2)
			{
				bool isIntra = layer2 == layer1;

				// Create inter-links to the outgoing nodes in the target layer
				double linkWeightNormalizationFactor = relaxRate / sumOutLinkWeightAllLayers;
				if (isIntra) {
					linkWeightNormalizationFactor += (1.0 - relaxRate) / sumOutLinkWeightLayer1;
				}
				double stateNodeWeightNormalizationFactor = 1.0;

				// Log() << "    -> Layer " << layer2 << ", linkWeightNormalizationFactor: " << linkWeightNormalizationFactor << "\n";
				
				createIntraLinksToNeighbouringNodesInTargetLayer(layer1, nodeIndex, layer2, m_networks[layer2].linkMap(), linkWeightNormalizationFactor, stateNodeWeightNormalizationFactor);
				
				if (m_config.isUndirected()) {
					// Create inter-links to the incoming nodes in the target layer too					
					createIntraLinksToNeighbouringNodesInTargetLayer(layer1, nodeIndex, layer2, oppositeLinkMaps[layer2], linkWeightNormalizationFactor, stateNodeWeightNormalizationFactor);
				}
			}
		}
	}
	Log() << "done!" << std::endl;
}

void MultiplexNetwork::generateMemoryNetworkWithJensenShannonSimulatedInterLayerLinks()
{
	// Simulate inter-layer links
	double jsrelaxRate = m_config.multiplexJSRelaxRate < 0 ? 0.15 : m_config.multiplexJSRelaxRate; //TODO: Set default in config and use separate bool

	Log() << "Generating memory network with Jensen-Shannon-weighted multilayer relax rate " << jsrelaxRate << "... " << std::flush;
	
	std::vector<LinkMap> oppositeLinkMaps;
	if (m_config.isUndirected()) {
		oppositeLinkMaps.resize(m_networks.size());
		for (unsigned int i = 0; i < m_networks.size(); ++i) {
			m_networks[i].generateOppositeLinkMap(oppositeLinkMaps[i]);
		}
	}

	for (unsigned int nodeIndex = 0; nodeIndex < m_numNodes; ++nodeIndex)
	{

		unsigned int layer2from = 0;
		
		// Calculate Jensen-Shannon similarity between all layers such that layer1 >= layer2,
		// and then use its symmetry for layer2 > layer1
		std::map<unsigned int,std::map<unsigned int,double> > jsRelaxWeights;
		std::map<unsigned int,double> jsTotWeight;

		for (unsigned int layer1 = 0; layer1 < m_networks.size(); ++layer1)
		{

			unsigned int layer2to = layer1+1;
			// Limit possible jumps to close by layers
			if(m_config.multiplexRelaxLimit >= 0){
				layer2from = ((int)layer1-m_config.multiplexRelaxLimit) < 0 ? 0 : layer1-m_config.multiplexRelaxLimit;			}

			const LinkMap& layer1LinkMap = m_networks[layer1].linkMap();
			LinkMap::const_iterator layer1OutLinksIt = layer1LinkMap.find(nodeIndex);
			double sumOutLinkWeightLayer1 = m_networks[layer1].sumLinkOutWeight()[nodeIndex];

			if(m_config.isUndirected()){
 				
				const LinkMap& layer1OppositeLinkMap = oppositeLinkMaps[layer1];
				LinkMap::const_iterator layer1OppositeOutLinksIt = layer1OppositeLinkMap.find(nodeIndex);

				// Will not link from dangling node (similarity 0)
				if((layer1OutLinksIt == layer1LinkMap.end()) && (layer1OppositeOutLinksIt == layer1OppositeLinkMap.end()))
					continue;

				// Include non-empty link maps
				std::vector<const InterLinkMap *> layer1LinksVec;
				if(layer1OutLinksIt != layer1LinkMap.end())
					layer1LinksVec.push_back(&layer1OutLinksIt->second);
				if(layer1OppositeOutLinksIt != layer1OppositeLinkMap.end())
					layer1LinksVec.push_back(&layer1OppositeOutLinksIt->second);

				for (unsigned int layer2 = layer2from; layer2 < layer2to; ++layer2){

					const LinkMap& layer2LinkMap = m_networks[layer2].linkMap();
					LinkMap::const_iterator layer2OutLinksIt = layer2LinkMap.find(nodeIndex);						
					const LinkMap& layer2OppositeLinkMap = oppositeLinkMaps[layer2];
					LinkMap::const_iterator layer2OppositeOutLinksIt = layer2OppositeLinkMap.find(nodeIndex);

					// Will not link to dangling node (similarity 0)
					if((layer2OutLinksIt == layer2LinkMap.end()) && (layer2OppositeOutLinksIt == layer2OppositeLinkMap.end()))
						continue;

					// Include non-empty link maps
					std::vector<const InterLinkMap *> layer2LinksVec;
					if(layer2OutLinksIt != layer2LinkMap.end())
						layer2LinksVec.push_back(&layer2OutLinksIt->second);
					if(layer2OppositeOutLinksIt != layer2OppositeLinkMap.end())
						layer2LinksVec.push_back(&layer2OppositeOutLinksIt->second);
	
					double sumOutLinkWeightLayer2 = m_networks[layer2].sumLinkOutWeight()[nodeIndex];

					bool intersect;
					double div = calculateJensenShannonDivergence(intersect,layer1LinksVec,sumOutLinkWeightLayer1,layer2LinksVec,sumOutLinkWeightLayer2);
					double jsWeight = 1.0 - div;
					if(intersect && (jsWeight >= m_config.multiplexRelaxLimit)){						
						jsRelaxWeights[layer1][layer2] = jsWeight;
						jsTotWeight[layer1] += jsWeight*sumOutLinkWeightLayer2;
						if(layer1 != layer2){
							jsRelaxWeights[layer2][layer1] = jsWeight;
							jsTotWeight[layer2] += jsWeight*sumOutLinkWeightLayer1;
						}
					}
				}

			}
			else{

				// Skip dangling nodes, because they have no information to calculate similarity
 				if (layer1OutLinksIt == layer1LinkMap.end())
 					continue;

				for (unsigned int layer2 = layer2from; layer2 < layer2to; ++layer2){
					const LinkMap& layer2LinkMap = m_networks[layer2].linkMap();
					LinkMap::const_iterator layer2OutLinksIt = layer2LinkMap.find(nodeIndex);
					if (layer2OutLinksIt == layer2LinkMap.end())
						continue;
	
					double sumOutLinkWeightLayer2 = m_networks[layer2].sumLinkOutWeight()[nodeIndex];
	
					bool intersect;
					double div = calculateJensenShannonDivergence(intersect,layer1OutLinksIt->second,sumOutLinkWeightLayer1,layer2OutLinksIt->second,sumOutLinkWeightLayer2);
					double jsWeight = 1.0 - div;
					if(intersect && (jsWeight >= m_config.multiplexRelaxLimit)){	
						jsTotWeight[layer1] += jsWeight;
						jsRelaxWeights[layer1][layer2] = jsWeight;
						if(layer1 != layer2){
							jsTotWeight[layer2] += jsWeight;
							jsRelaxWeights[layer2][layer1] = jsWeight;
						}
					}
				}
			}
		}

		// Second loop over all pairs of layers
		unsigned int layer2to = m_networks.size();

		for (unsigned int layer1 = 0; layer1 < m_networks.size(); ++layer1)
		{
			// Limit possible jumps to close by layers
			if(m_config.multiplexRelaxLimit >= 0){
				layer2from = ((int)layer1-m_config.multiplexRelaxLimit) < 0 ? 0 : layer1-m_config.multiplexRelaxLimit;
				layer2to = (layer1+m_config.multiplexRelaxLimit) > m_networks.size() ? m_networks.size() : layer1+m_config.multiplexRelaxLimit;
			}

			double sumOutLinkWeightLayer1 = m_networks[layer1].sumLinkOutWeight()[nodeIndex];

			std::map<unsigned int,std::map<unsigned int,double> >::iterator jsRelaxWeightsLayer1It = jsRelaxWeights.find(layer1);
			std::map<unsigned int,double>::iterator jsTotWeightIt = jsTotWeight.find(layer1);

			// Create inter-links to the intra-connected nodes in other layers
			for (unsigned int layer2 = layer2from; layer2 < layer2to; ++layer2)
			{
				if(jsRelaxWeightsLayer1It != jsRelaxWeights.end()){
					std::map<unsigned int,double>::iterator jsRelaxWeightsIt = jsRelaxWeightsLayer1It->second.find(layer2);
					if(jsRelaxWeightsIt != jsRelaxWeightsLayer1It->second.end()){

						bool isIntra = layer2 == layer1;
		
						// Create inter-links to the outgoing nodes in the target layer
						double linkWeightNormalizationFactor;
						if (isIntra){
							linkWeightNormalizationFactor = 1;
						} else {
							linkWeightNormalizationFactor = jsRelaxWeightsIt->second * jsrelaxRate / (1.0 - jsrelaxRate) * sumOutLinkWeightLayer1 / jsTotWeightIt->second;
						}
						
						double stateNodeWeightNormalizationFactor = 1.0;
						
						createIntraLinksToNeighbouringNodesInTargetLayer(layer1, nodeIndex, layer2, m_networks[layer2].linkMap(), linkWeightNormalizationFactor, stateNodeWeightNormalizationFactor);
						
						if (m_config.isUndirected()) {
							// Create inter-links to the incoming nodes in the target layer too					
							createIntraLinksToNeighbouringNodesInTargetLayer(layer1, nodeIndex, layer2, oppositeLinkMaps[layer2], linkWeightNormalizationFactor, stateNodeWeightNormalizationFactor);
						}
					}
				}
			}
		}
	}
	Log() << "done!" << std::endl;
}

double MultiplexNetwork::calculateJensenShannonDivergence(bool &intersect, const IntraLinkMap &layer1OutLinks, double sumOutLinkWeightLayer1, const IntraLinkMap &layer2OutLinks, double sumOutLinkWeightLayer2){

	intersect = false;
	double h1 = 0.0; // The entropy rate of the node in the first layer
	double h2 = 0.0; // The entropy rate of the node in the second layer
	double h12 = 0.0; // The entropy rate of the lumped node
	// The out-link weights of the nodes
	double ow1 = sumOutLinkWeightLayer1;
	double ow2 = sumOutLinkWeightLayer2;
	// Normalized weights over node in layer 1 and 2
	double pi1 = ow1 / (ow1 + ow2);
	double pi2 = ow2 / (ow1 + ow2);

	IntraLinkMap::const_iterator layer1OutLinkIt = layer1OutLinks.begin();
	IntraLinkMap::const_iterator layer2OutLinkIt = layer2OutLinks.begin();
	IntraLinkMap::const_iterator layer1OutLinkItEnd = layer1OutLinks.end();
	IntraLinkMap::const_iterator layer2OutLinkItEnd = layer2OutLinks.end();
	while(layer1OutLinkIt != layer1OutLinkItEnd && layer2OutLinkIt != layer2OutLinkItEnd){
		int diff = layer1OutLinkIt->first - layer2OutLinkIt->first;		
		if(diff < 0){
		// If the first state node has a link that the second has not
			double p1 = layer1OutLinkIt->second/ow1;
			h1 -= p1*log2(p1);
			double p12 = pi1*layer1OutLinkIt->second/ow1;
			h12 -= p12*log2(p12);
			layer1OutLinkIt++;
		}
		else if(diff > 0){
		// If the second state node has a link that the second has not
			double p2 = layer2OutLinkIt->second/ow2;
			h2 -= p2*log2(p2);
			double p12 = pi2*layer2OutLinkIt->second/ow2;
			h12 -= p12*log2(p12);
			layer2OutLinkIt++;
		}
		else{ // If both state nodes have the link
			intersect = true;
			double p1 = layer1OutLinkIt->second/ow1;
			h1 -= p1*log2(p1);
			double p2 = layer2OutLinkIt->second/ow2;
			h2 -= p2*log2(p2);
			double p12 = pi1*layer1OutLinkIt->second/ow1 + pi2*layer2OutLinkIt->second/ow2;
			h12 -= p12*log2(p12);
			layer1OutLinkIt++;
			layer2OutLinkIt++;
		}
	}

	while(layer1OutLinkIt != layer1OutLinkItEnd){
		// If the first state node has a link that the second has not
		double p1 = layer1OutLinkIt->second/ow1;
		h1 -= p1*log2(p1);
		double p12 = pi1*layer1OutLinkIt->second/ow1;
		h12 -= p12*log2(p12);
		layer1OutLinkIt++;
	}

	while(layer2OutLinkIt != layer2OutLinkItEnd){
		// If the second state node has a link that the second has not
		double p2 = layer2OutLinkIt->second/ow2;
		h2 -= p2*log2(p2);
		double p12 = pi2*layer2OutLinkIt->second/ow2;
		h12 -= p12*log2(p12);
		layer2OutLinkIt++;
	}	
	
	double div = (pi1+pi2)*h12 - pi1*h1 - pi2*h2;

	// Fix precision problems
	if(div < 0.0)
		div = 0.0;
	else if(div > 1.0)
		div = 1.0;
	
	// Log() << "\n" << div << " " << (pi1+pi2) << " " << h12 << " " << pi1 << " " << h1 << " " << pi2 << " " << h2;

	return div;

}

double MultiplexNetwork::calculateJensenShannonDivergence(bool &intersect, std::vector<const IntraLinkMap *> &layer1LinksVec, double sumOutLinkWeightLayer1, std::vector<const IntraLinkMap *> &layer2LinksVec, double sumOutLinkWeightLayer2){

	intersect = false;
	double h1 = 0.0; // The entropy rate of the node in the first layer
	double h2 = 0.0; // The entropy rate of the node in the second layer
	double h12 = 0.0; // The entropy rate of the lumped node
	// The out-link weights of the nodes
	double ow1 = sumOutLinkWeightLayer1;
	double ow2 = sumOutLinkWeightLayer2;
	// Normalized weights over node in layer 1 and 2
	double pi1 = ow1 / (ow1 + ow2);
	double pi2 = ow2 / (ow1 + ow2);

	IntraLinkMap::const_iterator *layer1linkIt;
	IntraLinkMap::const_iterator *layer2linkIt;

	std::vector<pair<IntraLinkMap::const_iterator,IntraLinkMap::const_iterator> > layer1OutLinkItVec;
	for(std::vector<const IntraLinkMap *>::iterator layer1linkPtrIt = layer1LinksVec.begin(); layer1linkPtrIt != layer1LinksVec.end(); layer1linkPtrIt++)
		layer1OutLinkItVec.push_back(make_pair((*layer1linkPtrIt)->begin(),(*layer1linkPtrIt)->end()));

	std::vector<pair<IntraLinkMap::const_iterator,IntraLinkMap::const_iterator> > layer2OutLinkItVec;
	for(std::vector<const IntraLinkMap *>::iterator layer2linkPtrIt = layer2LinksVec.begin(); layer2linkPtrIt != layer2LinksVec.end(); layer2linkPtrIt++)
		layer2OutLinkItVec.push_back(make_pair((*layer2linkPtrIt)->begin(),(*layer2linkPtrIt)->end()));

	while(undirLinkRemains(layer1OutLinkItVec) && undirLinkRemains(layer2OutLinkItVec)){
		
		layer1linkIt = getUndirLinkItPtr(layer1OutLinkItVec);
		layer2linkIt = getUndirLinkItPtr(layer2OutLinkItVec);

		int diff = (*layer1linkIt)->first - (*layer2linkIt)->first;	
		// Log() << "\n" << (*layer1linkIt)->first << " " << (*layer1linkIt)->second << " " << (*layer2linkIt)->first << " " << (*layer2linkIt)->second;
		
		if(diff < 0){
		// If the first state node has a link that the second has not
			double p1 = (*layer1linkIt)->second/ow1;
			h1 -= p1*log2(p1);
			double p12 = pi1*(*layer1linkIt)->second/ow1;
			h12 -= p12*log2(p12);
			(*layer1linkIt)++;
		}
		else if(diff > 0){
		// If the second state node has a link that the second has not
			double p2 = (*layer2linkIt)->second/ow2;
			h2 -= p2*log2(p2);
			double p12 = pi2*(*layer2linkIt)->second/ow2;
			h12 -= p12*log2(p12);
			(*layer2linkIt)++;
		}
		else{ // If both state nodes have the link
			intersect = true;
			double p1 = (*layer1linkIt)->second/ow1;
			h1 -= p1*log2(p1);
			double p2 = (*layer2linkIt)->second/ow2;
			h2 -= p2*log2(p2);
			double p12 = pi1*(*layer1linkIt)->second/ow1 + pi2*(*layer2linkIt)->second/ow2;
			h12 -= p12*log2(p12);
			(*layer1linkIt)++;
			(*layer2linkIt)++;
		}
	}

	while(undirLinkRemains(layer1OutLinkItVec)){
		// If the first state node has a link that the second has not

		layer1linkIt = getUndirLinkItPtr(layer1OutLinkItVec);

		double p1 = (*layer1linkIt)->second/ow1;
		h1 -= p1*log2(p1);
		double p12 = pi1*(*layer1linkIt)->second/ow1;
		h12 -= p12*log2(p12);
		(*layer1linkIt)++;
	}

	while(undirLinkRemains(layer2OutLinkItVec)){

		layer2linkIt = getUndirLinkItPtr(layer2OutLinkItVec);

		// If the second state node has a link that the second has not
		double p2 = (*layer2linkIt)->second/ow2;
		h2 -= p2*log2(p2);
		double p12 = pi2*(*layer2linkIt)->second/ow2;
		h12 -= p12*log2(p12);
		(*layer2linkIt)++;
	}	
	
	double div = (pi1+pi2)*h12 - pi1*h1 - pi2*h2;

	// Fix precision problems
	if(div < 0.0)
		div = 0.0;
	else if(div > 1.0)
		div = 1.0;
	
	return div;

}


MultiplexNetwork::IntraLinkMap::const_iterator *MultiplexNetwork::getUndirLinkItPtr(std::vector<pair<IntraLinkMap::const_iterator,IntraLinkMap::const_iterator> > &outLinkItVec){

	bool assigned = false;
	IntraLinkMap::const_iterator *linkIt;

	for(std::vector<pair<IntraLinkMap::const_iterator,IntraLinkMap::const_iterator> >::iterator outLinkItVecIts = outLinkItVec.begin(); outLinkItVecIts != outLinkItVec.end(); outLinkItVecIts++){
		if(outLinkItVecIts->first != outLinkItVecIts->second){
			if(assigned){
				if(outLinkItVecIts->first->first < (*linkIt)->first){
					linkIt = &outLinkItVecIts->first;
				}
			}
			else{
				linkIt = &outLinkItVecIts->first;
				assigned = true;
			}
		}
	}

	return linkIt;

}

bool MultiplexNetwork::undirLinkRemains(std::vector<pair<IntraLinkMap::const_iterator,IntraLinkMap::const_iterator> > &outLinkItVec){

	for(std::vector<pair<IntraLinkMap::const_iterator,IntraLinkMap::const_iterator> >::iterator outLinkItVecIts = outLinkItVec.begin(); outLinkItVecIts != outLinkItVec.end(); outLinkItVecIts++)
		if(outLinkItVecIts->first != outLinkItVecIts->second)
			return true;

	return false;

}



void MultiplexNetwork::addMemoryNetworkFromMultiplexLinks()
{
	if (m_multiplexLinks.empty())
		return;
	Log() << "Adding memory network from multilayer links... " << std::flush;

	for (MultiplexLinkMap::const_iterator it(m_multiplexLinks.begin()); it != m_multiplexLinks.end(); ++it)
	{
		const StateNode& source(it->first);
		const std::map<StateNode, double>& subLinks(it->second);
		for (std::map<StateNode, double>::const_iterator subIt(subLinks.begin()); subIt != subLinks.end(); ++subIt)
		{
			const StateNode& target(subIt->first);
			double linkWeight = subIt->second;
			addStateLink(source.layer(), source.physIndex, target.layer(), target.physIndex, linkWeight);
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

		m_interLinks[StateNode(layer1, nodeIndex)][layer2] += weight;

		++m_numInterLinksFound;
		++m_interLinkLayers[layer1];
		++m_interLinkLayers[layer2];
	}
	return line;
}

void MultiplexNetwork::addMultiplexLink(int layer1, int node1, int layer2, int node2, double weight){
	m_multiplexLinks[StateNode(layer1, node1)][StateNode(layer2, node2)] += weight;

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

		if(layer1 == layer2)
			++m_numIntraLinksFound;
		else
			++m_numInterLinksFound;

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
		throw FileFormatError(io::Str() << "Can't parse multilayer intra link data (layer node1 node2) from line '" << line << "'");
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
		throw FileFormatError(io::Str() << "Can't parse multilayer inter link data (layer1 node layer2) from line '" << line << "'");
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
		throw FileFormatError(io::Str() << "Can't parse multilayer link data (layer1 node1 layer2 node2) from line '" << line << "'");
	(m_extractor >> weight) || (weight = 1.0);
	layer1 -= m_indexOffset;
	node1 -= m_indexOffset;
	layer2 -= m_indexOffset;
	node2 -= m_indexOffset;
}

void MultiplexNetwork::finalizeAndCheckNetwork(bool printSummary)
{
	if (!m_config.isMultiplexNetwork()) {
		return MemNetwork::finalizeAndCheckNetwork(printSummary);
	}
	
	if (!m_networks.empty())
		Log() << " --> Found " << m_numIntraLinksFound << " intra-network links in " << m_networks.size() << " layers.\n";
	if (!m_interLinkLayers.empty())
		Log() << " --> Found " << m_numInterLinksFound << " inter-network links in " << m_interLinkLayers.size() << " layers.\n";
	if (!m_multiplexLinkLayers.empty())
		Log() << " --> Found " << m_numMultiplexLinksFound << " multilayer links in " << m_multiplexLinkLayers.size() << " layers.\n";

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

	bool simulateInterLayerLinks = m_config.multiplexJSRelaxRate >= 0.0 ||  m_config.multiplexRelaxRate >= 0.0 || m_numInterLinksFound == 0;
	if (simulateInterLayerLinks){
		if(m_config.multiplexJSRelaxRate >= 0.0)
			generateMemoryNetworkWithJensenShannonSimulatedInterLayerLinks();
		else
			generateMemoryNetworkWithSimulatedInterLayerLinks();
	}
	else
		generateMemoryNetworkWithInterLayerLinksFromData();

	addMemoryNetworkFromMultiplexLinks();

	// Dispose intermediate data structures to clear memory
	m_interLinks.clear();
	m_networks.clear();

	MemNetwork::finalizeAndCheckNetwork(printSummary);
}

#ifdef NS_INFOMAP
}
#endif
