/* ------------------------------------------------------------------------

 Infomap software package for multi-level network clustering

 * Copyright (c) 2013
 * For license and authors, see README.txt or http://www.mapequation.org

------------------------------------------------------------------------ */

#include "Network.h"
#include "../utils/FileURI.h"
#include "../io/convert.h"
#include "../io/SafeFile.h"
#include <cmath>
using std::make_pair;

void Network::readFromFile(std::string filename)
{
	FileURI networkFilename(filename, false);
	std::string format = m_config.inputFormat;

	if (format == "")
	{
		std::string type = networkFilename.getExtension();
		if (type == "net")
			format = "pajek";
		else if (type == "txt")
			format = "link-list";
	}
	if (format == "")
		throw UnknownFileTypeError("No known input format specified or implied by file extension.");

	if (format == "pajek")
		parsePajekNetwork(filename);
	else if (format == "link-list")
		parseLinkList(filename);
	else if (format == "sparse-link-list")
		parseSparseLinkList(filename);
	else
		throw UnknownFileTypeError("No known input format specified.");

}


void Network::parsePajekNetwork(std::string filename)
{
	string line;
	string buf;
	SafeInFile input(filename.c_str());
	std::cout << "Parsing " << (m_config.directed ? "directed" : "undirected") << " network from file '" <<
			filename << "'... " << std::flush;

	if( getline(input,line) == NULL)
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
//	m_leafNodes.resize(numNodes);
	m_nodeNames.resize(numNodes);
	m_nodeTeleportRates.assign(numNodes, 1.0);
	m_sumNodeWeights = 0.0;

	// Read node names, assuming order 1, 2, 3, ...
	for (unsigned int i = 0; i < numNodes; ++i)
	{
		getline(input, line);
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
		m_nodeTeleportRates[i] = nodeWeight;
//		m_nodeMap.insert(make_pair(name, i));
		m_nodeNames[i] = name;
	}

	if (m_config.nodeLimit > 0 && numNodes < specifiedNumNodes)
	{
		unsigned int surplus = specifiedNumNodes - numNodes;
		for (unsigned int i = 0; i < surplus; ++i)
			getline(input, line);
	}

	// Read the number of links in the network
	getline(input, line);
	ss.clear();
	ss.str(line);
	ss >> buf;
	if(buf != "*Edges" && buf != "*edges" && buf != "*Arcs" && buf != "*arcs") {
		throw FileFormatError("The first line (to lower cases) after the nodes doesn't match *vertices or *arcs.");
	}

	unsigned int numDoubleLinks = 0;
	unsigned int numEdgeLines = 0;
	unsigned int numSkippedEdges = 0;
	unsigned int maxLinkEnd = 0;
	m_totalLinkWeight = 0.0;

	// Read links in format "from to weight", for example "1 3 2" (all integers) and each undirected link only ones (weight is optional).
	while(getline(input, line))
	{
		if (line.length() == 0)
			continue;
		++numEdgeLines;
		ss.clear();
		ss.str(line);
		unsigned int linkEnd1;
		if (!(ss >> linkEnd1))
			throw BadConversionError(io::Str() << "Can't parse first integer of link number " <<
					numEdgeLines << " from line '" << line << "'");
		unsigned int linkEnd2;
		if (!(ss >> linkEnd2))
			throw BadConversionError(io::Str() << "Can't parse second integer of link number " <<
					numEdgeLines << " from line '" << line << "'");

		if (checkNodeLimit && (linkEnd2 > numNodes || linkEnd1 > numNodes))
		{
			++numSkippedEdges;
			continue;
		}

		if (linkEnd2 == linkEnd1)
		{
			++m_numSelfLinks;
			if (!m_config.includeSelfLinks)
				continue;
		}

		double linkWeight;
		if (!(ss >> linkWeight))
			linkWeight = 1.0;

		--linkEnd1; // Node numbering starts from 1 in the .net format
		--linkEnd2;

		maxLinkEnd = std::max(maxLinkEnd, std::max(linkEnd1, linkEnd2));

		// If undirected links, aggregate weight rather than adding an opposite link.
		if (m_config.isUndirected() && linkEnd2 < linkEnd1)
			std::swap(linkEnd1, linkEnd2);

		m_totalLinkWeight += linkWeight;
		if (m_config.isUndirected())
		{
			m_totalLinkWeight += linkWeight;
		}

		// Aggregate link weights if they are definied more than once
		LinkMap::iterator it = m_links.find(std::make_pair(linkEnd1, linkEnd2));
		if(it == m_links.end())
		{
			m_links.insert(std::make_pair(std::make_pair(linkEnd1, linkEnd2), linkWeight));
		}
		else
		{
			it->second += linkWeight;
			++numDoubleLinks;
			if (linkEnd2 == linkEnd1)
				--m_numSelfLinks;
		}
	}

	unsigned int zeroMinusOne = 0;
	--zeroMinusOne;
	if (maxLinkEnd == zeroMinusOne)
		throw InputDomainError(io::Str() << "Integer overflow, be sure to use zero-based node numbering if the node numbers starts from zero.");
	if (maxLinkEnd >= numNodes)
		throw InputDomainError(io::Str() << "At least one link is defined with node numbers that exceeds the number of nodes.");

	unsigned int sumEdgesFound = m_links.size() + m_numSelfLinks + numDoubleLinks + numSkippedEdges;
	std::cout << "done! Found " << specifiedNumNodes << " nodes and " << sumEdgesFound << " links. ";
//	std::cout << "Average node weight: " << (m_sumNodeWeights / numNodes) << ". ";
	if (m_config.nodeLimit > 0)
		std::cout << "Limiting network to " << numNodes << " nodes and " << m_links.size() << " links. ";
	if(numDoubleLinks > 0)
		std::cout << numDoubleLinks << " links was aggregated to existing links. ";
	if (m_numSelfLinks > 0 && !m_config.includeSelfLinks)
		std::cout << m_numSelfLinks << " self-links was ignored. ";
	std::cout << std::endl;

}

void Network::parseLinkList(std::string filename)
{
	string line;
	string buf;
	SafeInFile input(filename.c_str());
	std::cout << "Parsing " << (m_config.directed ? "directed" : "undirected") << " link list from file '" <<
			filename << "'... " << std::flush;

	std::istringstream ss;

	unsigned int numDoubleLinks = 0;
	unsigned int numEdgeLines = 0;
	unsigned int numSkippedEdges = 0;
	m_totalLinkWeight = 0.0;
	unsigned int maxLinkEnd = 0;
	bool minLinkIsZero = false;
	bool checkNodeLimit = m_config.nodeLimit > 0;
	unsigned int nodeLimit = m_config.nodeLimit;
	unsigned int lowestNodeNumber = m_config.zeroBasedNodeNumbers ? 0 : 1;

	// Read links in format "from to weight", for example "1 3 2" (all integers) and each undirected link only ones (weight is optional).
	while(getline(input, line))
	{
		if (line.length() == 0)
			continue;
		if (line[0] == '#')
			continue;
		++numEdgeLines;
		ss.clear();
		ss.str(line);
		unsigned int linkEnd1;
		if (!(ss >> linkEnd1))
			throw BadConversionError(io::Str() << "Can't parse first integer of link number " <<
					numEdgeLines << " from line '" << line << "'");
		unsigned int linkEnd2;
		if (!(ss >> linkEnd2))
			throw BadConversionError(io::Str() << "Can't parse second integer of link number " <<
					numEdgeLines << " from line '" << line << "'");

		linkEnd1 -= lowestNodeNumber;
		linkEnd2 -= lowestNodeNumber;

		if (checkNodeLimit && (linkEnd2 >= nodeLimit || linkEnd1 >= nodeLimit))
		{
			++numSkippedEdges;
			continue;
		}

		if (linkEnd2 == linkEnd1)
		{
			++m_numSelfLinks;
			if (!m_config.includeSelfLinks)
				continue;
		}

		double linkWeight = 1.0;
		ss >> linkWeight;

		maxLinkEnd = std::max(maxLinkEnd, std::max(linkEnd1, linkEnd2));
		if (!minLinkIsZero && (linkEnd1 == 0 || linkEnd2 == 0))
			minLinkIsZero = true;

		// If undirected links, aggregate weight rather than adding an opposite link.
		if (m_config.isUndirected() && linkEnd2 < linkEnd1)
			std::swap(linkEnd1, linkEnd2);

		m_totalLinkWeight += linkWeight;
		if (m_config.isUndirected())
		{
			m_totalLinkWeight += linkWeight;
		}

		// Aggregate link weights if they are definied more than once
		LinkMap::iterator it = m_links.find(std::make_pair(linkEnd1, linkEnd2));
		if(it == m_links.end())
		{
			m_links.insert(std::make_pair(std::make_pair(linkEnd1, linkEnd2), linkWeight));
		}
		else
		{
			it->second += linkWeight;
			++numDoubleLinks;
			if (linkEnd2 == linkEnd1)
				--m_numSelfLinks;
		}
	}

	unsigned int zeroMinusOne = 0;
	--zeroMinusOne;
	if (maxLinkEnd == zeroMinusOne)
		throw InputDomainError(io::Str() << "Integer overflow, be sure to use zero-based node numbering if the node numbers starts from zero.");


	m_numNodes = maxLinkEnd + 1;

//	m_leafNodes.resize(m_numNodes);
	m_nodeNames.resize(m_numNodes);
	m_nodeTeleportRates.assign(m_numNodes, 1.0);
	m_sumNodeWeights = 1.0 * m_numNodes;

	for (unsigned int i = 0; i < m_numNodes; ++i)
	{
//		m_nodeMap.insert(make_pair(io::stringify(i + 1), i));
		m_nodeNames[i] = io::stringify(i+1);
	}

	std::cout << "done! Found " << m_numNodes << " nodes and " << m_links.size() << " links." << std::endl;
	if (!minLinkIsZero)
		std::cout << "(Warning: minimum link index is not zero, check that you don't use zero based numbering if it's not true.)\n";
}

void Network::parseSparseLinkList(std::string filename)
{
	string line;
	string buf;
	SafeInFile input(filename.c_str());
	std::cout << "Parsing " << (m_config.directed ? "directed" : "undirected") <<
			" sparse link list from file '" << filename << "'... " << std::flush;

	std::istringstream ss;

	unsigned int numDoubleLinks = 0;
	unsigned int numEdgeLines = 0;
	unsigned int numSkippedEdges = 0;
	m_totalLinkWeight = 0.0;
	unsigned int maxLinkEnd = 0;
	bool minLinkIsZero = false;
	bool checkNodeLimit = m_config.nodeLimit > 0;
	unsigned int nodeLimit = m_config.nodeLimit > 0 ? m_config.nodeLimit : 0;
	unsigned int lowestNodeNumber = m_config.zeroBasedNodeNumbers ? 0 : 1;

	typedef std::map<unsigned int, std::map<unsigned int, double> > OutLinkMap;
	OutLinkMap links;
	unsigned int numLinks = 0;

	// Read links in format "from to weight", for example "1 3 2" (all integers) and each undirected link only ones (weight is optional).
	while(getline(input, line))
	{
		if (line.length() == 0)
			continue;
		if (line[0] == '#')
			continue;
		++numEdgeLines;
		ss.clear();
		ss.str(line);
		unsigned int linkEnd1;
		if (!(ss >> linkEnd1))
			throw BadConversionError(io::Str() << "Can't parse first integer of link number " <<
					numEdgeLines << " from line '" << line << "'");
		unsigned int linkEnd2;
		if (!(ss >> linkEnd2))
			throw BadConversionError(io::Str() << "Can't parse second integer of link number " <<
					numEdgeLines << " from line '" << line << "'");

		linkEnd1 -= lowestNodeNumber;
		linkEnd2 -= lowestNodeNumber;

		if (checkNodeLimit && (linkEnd2 >= nodeLimit || linkEnd1 >= nodeLimit))
		{
			++numSkippedEdges;
			continue;
		}

		if (linkEnd2 == linkEnd1)
		{
			++m_numSelfLinks;
			if (!m_config.includeSelfLinks)
				continue;
		}

		double linkWeight = 1.0;
		ss >> linkWeight;

		maxLinkEnd = std::max(maxLinkEnd, std::max(linkEnd1, linkEnd2));
		if (!minLinkIsZero && (linkEnd1 == 0 || linkEnd2 == 0))
			minLinkIsZero = true;

		// If undirected links, aggregate weight rather than adding an opposite link.
		if (m_config.isUndirected() && linkEnd2 < linkEnd1)
			std::swap(linkEnd1, linkEnd2);

		m_totalLinkWeight += linkWeight;
		if (m_config.isUndirected())
		{
			m_totalLinkWeight += linkWeight;
		}

		// Aggregate link weights if they are definied more than once
		OutLinkMap::iterator fromLinkIt = links.find(linkEnd1);
		if(fromLinkIt == links.end())
		{
			std::map<unsigned int, double> outLinkMap;
			outLinkMap.insert(std::make_pair(linkEnd2, linkWeight));
			links.insert(std::make_pair(linkEnd1, outLinkMap));
			++numLinks;
		}
		else
		{
			std::map<unsigned int, double>::iterator toLink_it = fromLinkIt->second.find(linkEnd2);
			if(toLink_it == fromLinkIt->second.end())
			{
				fromLinkIt->second.insert(make_pair(linkEnd2,linkWeight));
				++numLinks;
			}
			else{
				toLink_it->second += linkWeight;
				++numDoubleLinks;
				if (linkEnd2 == linkEnd1)
					--m_numSelfLinks;
			}
		}
	}


	m_numNodes = links.size();

//	m_leafNodes.resize(m_numNodes);
	m_nodeNames.resize(m_numNodes);
	m_nodeTeleportRates.assign(m_numNodes, 1.0);
	m_sumNodeWeights = 1.0 * m_numNodes;

	std::map<unsigned int, unsigned int> packedIndices;
	unsigned int linkIndex = 0;
	for (OutLinkMap::iterator linkIt(links.begin()); linkIt != links.end(); ++linkIt, ++linkIndex)
	{
		packedIndices.insert(std::make_pair(linkIt->first, linkIndex));
		m_nodeMap.insert(make_pair(io::stringify(linkIt->first + 1), linkIndex));
		m_nodeNames[linkIndex] = io::stringify(linkIt->first + 1);
	}

	for (OutLinkMap::iterator linkIt(links.begin()); linkIt != links.end(); ++linkIt, ++linkIndex)
	{
		std::map<unsigned int, double>& outLinkMap = linkIt->second;
		for (std::map<unsigned int, double>::iterator toLinkIt(outLinkMap.begin());
				toLinkIt != outLinkMap.end(); ++toLinkIt)
		{
			unsigned int first = packedIndices[linkIt->first];
			unsigned int second = packedIndices[toLinkIt->first];
			m_links.insert(make_pair(make_pair(first, second), toLinkIt->second));
		}
	}

	std::cout << "done! Found " << m_numNodes << " nodes and " << m_links.size() << " links." << std::endl;
	if (!minLinkIsZero)
		std::cout << "(Warning: minimum link index is not zero, check that you don't use zero based numbering if it's not true.)\n";

}


void Network::calculateFlow()
{
	std::cout << "Calculating global flow... ";

//	bool addSelfLinks = m_config.selfTeleportationProbability > 0 && m_config.selfTeleportationProbability < 1;
//	double outRate = 1.0;
//	if (addSelfLinks)
//		outRate = 1.0 - m_config.selfTeleportationProbability;

	// Prepare data in sequence containers for fast access of individual elements
	unsigned int numNodes = m_numNodes;
	m_nodeOutDegree.assign(numNodes, 0);
	m_sumLinkOutWeight.assign(numNodes, 0.0);
	m_nodeFlow.assign(numNodes, 0.0);
//	unsigned int numLinks = addSelfLinks ? (m_links.size() + m_numNodes - m_numSelfLinks) : m_links.size();
	unsigned int numLinks = m_links.size();
	m_flowLinks.resize(numLinks);
	double sumUndirLinkWeight = (m_config.isUndirected() ? 1 : 2) * m_totalLinkWeight;
	unsigned int linkIndex = 0;

	for (LinkMap::iterator linkIt(m_links.begin()); linkIt != m_links.end(); ++linkIt, ++linkIndex)
	{
		const std::pair<unsigned int, unsigned int>& linkEnds = linkIt->first;
		++m_nodeOutDegree[linkEnds.first];
		double linkWeight = linkIt->second;
		m_sumLinkOutWeight[linkEnds.first] += linkWeight;
		if (m_config.isUndirected())
			m_sumLinkOutWeight[linkEnds.second] += linkWeight;
		m_nodeFlow[linkEnds.first] += linkWeight / sumUndirLinkWeight;
		m_nodeFlow[linkEnds.second] += linkWeight / sumUndirLinkWeight;
		m_flowLinks[linkIndex] = Link(linkEnds.first, linkEnds.second, linkWeight);
	}

//	if (addSelfLinks)
//	{
//		for (unsigned int i = 0; i < numNodes; ++i)
//		{
//			double linkWeight = m_sumLinkOutWeight[i] > 0 ?
//					(m_sumLinkOutWeight[i] * m_config.selfTeleportationProbability) : 1.0;
//			LinkMap::iterator it = m_links.find(std::make_pair(i, i));
//			if(it == m_links.end())
//			{
//				m_links.insert(std::make_pair(std::make_pair(i, i), linkWeight));
//				m_flowLinks[linkIndex++] = Link(i, i, linkWeight);
//			}
//			else
//			{
//				it->second += linkWeight;
//				++m_numSelfLinks;
////				m_flowLinks[i].weight += linkWeight;
//			}
//		}
//	}


	if (m_config.rawdir)
	{
		// Treat the link weights as flow (after global normalization) and
		// do one power iteration to set the node flow
		m_nodeFlow.assign(numNodes, 0.0);
		for (LinkVec::iterator linkIt(m_flowLinks.begin()); linkIt != m_flowLinks.end(); ++linkIt)
		{
			Link& link = *linkIt;
			link.flow /= m_totalLinkWeight;
			m_nodeFlow[link.target] += link.flow;
		}
		//Normalize node flow
		double sumNodeRank = 0.0;
		for (unsigned int i = 0; i < numNodes; ++i)
			sumNodeRank += m_nodeFlow[i];
		for (unsigned int i = 0; i < numNodes; ++i)
			m_nodeFlow[i] /= sumNodeRank;
		std::cout << "using directed links with raw flow... done!" << std::endl;
		std::cout << "Total link weight: " << m_totalLinkWeight << "\n";
		return;
	}

	if (!m_config.directed)
	{
		if (m_config.undirdir)
		{
			//Take one last power iteration
			std::vector<double> nodeFlowSteadyState(m_nodeFlow);
			m_nodeFlow.assign(numNodes, 0.0);
			for (LinkVec::iterator linkIt(m_flowLinks.begin()); linkIt != m_flowLinks.end(); ++linkIt)
			{
				Link& link = *linkIt;
				m_nodeFlow[link.target] += nodeFlowSteadyState[link.source] * link.flow / m_sumLinkOutWeight[link.source];
			}
			//Normalize node flow
			double sumNodeRank = 0.0;
			for (unsigned int i = 0; i < numNodes; ++i)
				sumNodeRank += m_nodeFlow[i];
			for (unsigned int i = 0; i < numNodes; ++i)
				m_nodeFlow[i] /= sumNodeRank;
			// Update link data to represent flow instead of weight
			for (LinkVec::iterator linkIt(m_flowLinks.begin()); linkIt != m_flowLinks.end(); ++linkIt)
			{
				Link& link = *linkIt;
				link.flow *= nodeFlowSteadyState[link.source] / m_sumLinkOutWeight[link.source] / sumNodeRank;
			}
		}
		else // undirected
		{
			for (unsigned int i = 0; i < numLinks; ++i)
				m_flowLinks[i].flow /= sumUndirLinkWeight;
		}

		std::cout << "using undirected links" << (m_config.undirdir? ", switching to directed after steady state... done!" :
				"... done!") << std::endl;
		return;
	}

	std::cout << "using " << (m_config.unrecordedTeleportation ? "unrecorded" : "recorded") << " teleportation to " <<
			(m_config.teleportToNodes ? "nodes" : "links") << "... " << std::flush;

	// Calculate the teleport rate distribution
	if (m_config.teleportToNodes)
	{
		// Teleport proportionally to node weights specified in network file or default uniformly.
		for (unsigned int i = 0; i < numNodes; ++i)
			m_nodeTeleportRates[i] /= m_sumNodeWeights;
	}
	else
	{
		m_nodeTeleportRates.assign(numNodes, 0.0);
		// Teleport proportionally to in-degree, or out-degree if unrecorded teleportation.
		for (LinkVec::iterator linkIt(m_flowLinks.begin()); linkIt != m_flowLinks.end(); ++linkIt)
		{
			unsigned int toNode = m_config.unrecordedTeleportation ? linkIt->source : linkIt->target;
			m_nodeTeleportRates[toNode] += linkIt->flow / m_totalLinkWeight;
		}
	}


	// Normalize link weights with respect to its source nodes total out-link weight;
	for (LinkVec::iterator linkIt(m_flowLinks.begin()); linkIt != m_flowLinks.end(); ++linkIt)
	{
		linkIt->flow /= m_sumLinkOutWeight[linkIt->source];
	}

	// Collect dangling nodes
	std::vector<unsigned int> danglings;
	for (unsigned int i = 0; i < numNodes; ++i)
	{
		if (m_nodeOutDegree[i] == 0)
			danglings.push_back(i);
	}

	// Calculate PageRank
	std::vector<double> nodeFlowTmp(numNodes, 0.0);
	unsigned int numIterations = 0;
	double alpha = m_config.teleportationProbability;
	double beta = 1.0 - alpha;
	double sqdiff = 1.0;
	double danglingRank = 0.0;
	do
	{
		// Calculate dangling rank
		danglingRank = 0.0;
		for (std::vector<unsigned int>::iterator danglingIt(danglings.begin()); danglingIt != danglings.end(); ++danglingIt)
		{
			danglingRank += m_nodeFlow[*danglingIt];
		}

		// Flow from teleportation
		for (unsigned int i = 0; i < numNodes; ++i)
		{
			nodeFlowTmp[i] = (alpha + beta*danglingRank) * m_nodeTeleportRates[i];
		}

		// Flow from links
		for (LinkVec::iterator linkIt(m_flowLinks.begin()); linkIt != m_flowLinks.end(); ++linkIt)
		{
			Link& link = *linkIt;
			nodeFlowTmp[link.target] += beta * link.flow * m_nodeFlow[link.source];
		}

		// Update node flow from the power iteration above and check if converged
		double sum = 0.0;
		double sqdiff_old = sqdiff;
		sqdiff = 0.0;
		for (unsigned int i = 0; i < numNodes; ++i)
		{
			sum += nodeFlowTmp[i];
			sqdiff += std::abs(nodeFlowTmp[i] - m_nodeFlow[i]);
			m_nodeFlow[i] = nodeFlowTmp[i];
		}

		// Normalize if needed
		if (std::abs(sum - 1.0) > 1.0e-10)
		{
			std::cout << "(Normalizing ranks after " <<	numIterations << " power iterations with error " << (sum-1.0) << ") ";
			for (unsigned int i = 0; i < numNodes; ++i)
			{
				m_nodeFlow[i] /= sum;
			}
		}

		// Perturb the system if equilibrium
		if(sqdiff == sqdiff_old)
		{
			alpha += 1.0e-10;
			beta = 1.0 - alpha;
		}

		numIterations++;
	}  while((numIterations < 200) && (sqdiff > 1.0e-15 || numIterations < 50));

	double sumNodeRank = 1.0;

	if (m_config.unrecordedTeleportation)
	{
		//Take one last power iteration excluding the teleportation (and normalize node flow to sum 1.0)
		sumNodeRank = 1.0 - danglingRank;
		m_nodeFlow.assign(numNodes, 0.0);
		for (LinkVec::iterator linkIt(m_flowLinks.begin()); linkIt != m_flowLinks.end(); ++linkIt)
		{
			Link& link = *linkIt;
			m_nodeFlow[link.target] += link.flow * nodeFlowTmp[link.source] / sumNodeRank;
		}
		beta = 1.0;
	}


	// Update the links with their global flow from the PageRank values. (Note: beta is set to 1 if unrec)
	for (LinkVec::iterator linkIt(m_flowLinks.begin()); linkIt != m_flowLinks.end(); ++linkIt)
	{
		linkIt->flow *= beta * nodeFlowTmp[linkIt->source] / sumNodeRank;
	}

	std::cout << "done in " << numIterations << " iterations!" << std::endl;
}
