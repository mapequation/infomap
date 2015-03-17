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


#include "MemNetwork.h"
#include "../utils/FileURI.h"
#include "../io/convert.h"
#include "../io/SafeFile.h"
#include "../utils/Logger.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <set>

#ifdef NS_INFOMAP
namespace infomap
{
#endif

using std::make_pair;

void MemNetwork::readInputData(std::string filename)
{
	if (filename.empty())
		filename = m_config.networkFile;
	if (m_config.inputFormat == "3gram")
		parseTrigram(filename);
	else
	{
		Network::readInputData(filename);
		finalizeAndCheckNetwork();
	}
}

void MemNetwork::parseTrigram(std::string filename)
{
	Log() << "Parsing directed trigram from file '" << filename << "'... " << std::flush;
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

	m_totalLinkWeight = 0.0;

	// Read links in format "from through to weight", for example "1 2 3 2" (all integers) and each undirected link only ones (weight is optional).
	while(!std::getline(input, line).fail())
	{
		if (line.length() == 0)
			continue;

		int n1;
		unsigned int n2, n3;
		double weight;
		parseM2Link(line, n1, n2, n3, weight);

		if (n1 + static_cast<int>(m_indexOffset) == -1)
		{
			addIncompleteM2Link(n2, n3, weight);
		}
		else
			addM2Link(n1, n2, n2, n3, weight);

		if (n2 != n3 || m_config.includeSelfLinks)
			insertLink(n2, n3, weight);
	}

	Log() << "done!" << std::endl;

	finalizeAndCheckNetwork();

}

void MemNetwork::simulateMemoryFromOrdinaryNetwork()
{
	Log() << "Simulating memory from ordinary network by chaining pair of overlapping links to trigrams... " << std::flush;

	// Reset some data from ordinary network
	m_totalLinkWeight = 0.0;
	m_numSelfLinks = 0.0;
	m_totalSelfLinkWeight = 0.0;

	if (m_config.originallyUndirected)
	{
		Log() << "(inflating undirected network... " << std::flush;
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
		Log() << ") " << std::flush;
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

					if(!m_config.nonBacktracking || (n1 != n3))
						addM2Link(n1, n2, n2, n3, linkWeight, firstLinkWeight / secondLinkSubMap.size(), 0.0);

				}
			}
			else
			{
				// No chainable link found, create a dangling memory node (or remove need for existence in MemFlowNetwork?)
//				addM2Node(n1, n2, firstLinkWeight);
				addM2Link(n1, n1, n1, n2, firstLinkWeight);
			}
		}
	}

	Log() << "done!" << std::endl;
}

void MemNetwork::simulateMemoryToIncompleteData()
{
	if (m_numIncompleteM2Links == 0)
		return;

	Log() << "\n  -> Found " << m_numM2LinksFound << " trigrams with " <<
			m_numIncompleteM2LinksFound << " incomplete trigrams.";
	Log() << "\n  -> Patching " << m_numIncompleteM2Links << " incomplete trigrams.." << std::flush;

	// Store all incomplete data on a compact array, with fast mapping from incomplete link source index to the compact index
	std::vector<std::deque<ComplementaryData> > complementaryData(m_numIncompleteM2Links);
	std::vector<int> incompleteSourceMapping(m_numNodes, -1);
	int compactIndex = 0;
	for (LinkMap::const_iterator linkIt(m_incompleteM2Links.begin()); linkIt != m_incompleteM2Links.end(); ++linkIt, ++compactIndex)
	{
		unsigned int n1 = linkIt->first;
		incompleteSourceMapping[n1] = compactIndex;
		const std::map<unsigned int, double>& subLinks = linkIt->second;
		for (std::map<unsigned int, double>::const_iterator subIt(subLinks.begin()); subIt != subLinks.end(); ++subIt)
		{
			unsigned int n2 = subIt->first;
			double weight = subIt->second;
			complementaryData[compactIndex].push_back(ComplementaryData(n1, n2, weight));
		}
	}

	Log() << "." << std::endl;
	unsigned int lastProgress = 0;
	unsigned int linkCount = 0;

	// Loop through all m2 links and store all feasible chainable links to the incomplete data
	unsigned int numExactMatches = 0;
	unsigned int numPartialMatches = 0;
	unsigned int numShiftedMatches = 0;
	for (M2LinkMap::const_iterator linkIt(m_m2Links.begin()); linkIt != m_m2Links.end(); ++linkIt)
	{
		const M2Node& m2source = linkIt->first;
		const std::map<M2Node, double>& subLinks = linkIt->second;
		for (std::map<M2Node, double>::const_iterator subIt(subLinks.begin()); subIt != subLinks.end(); ++subIt)
		{
			const M2Node& m2target = subIt->first;
			double weight = subIt->second;

			// Check physical source index for exact and partial match
			int compactIndex = incompleteSourceMapping[m2source.physIndex];
			if (compactIndex != -1)
			{
				std::deque<ComplementaryData>& matchedComplementaryData = complementaryData[compactIndex];
				for (unsigned int i = 0; i < matchedComplementaryData.size(); ++i)
				{
					ComplementaryData& data = matchedComplementaryData[i];
					if (data.incompleteLink.n2 == m2target.physIndex) {
						data.addExactMatch(m2source.priorState, weight);
						++numExactMatches;
					}
					else {
						// Partial matches not used if exact matches available
						if (data.exactMatch.empty())
							data.addPartialMatch(m2source.priorState, weight);
						++numPartialMatches;
					}
				}
			}

			// Check target index for shifted match (using the physical source as memory data)
			compactIndex = incompleteSourceMapping[m2target.physIndex];
			if (compactIndex != -1)
			{
				std::deque<ComplementaryData>& matchedComplementaryData = complementaryData[compactIndex];
				for (unsigned int i = 0; i < matchedComplementaryData.size(); ++i)
				{
					ComplementaryData& data = matchedComplementaryData[i];
					// Shifted match only used if no better match
					if (data.exactMatch.empty() && data.partialMatch.empty())
						data.addShiftedMatch(m2target.priorState, weight);
					++numShiftedMatches;
				}
			}

			++linkCount;
			unsigned int progress = linkCount * 1000 / m_numM2Links; // 0.1% resolution
			if (progress != lastProgress) {
				Log() << "\r    -> Collecting matches... (" << progress * 0.1 << "%)      " << std::flush;
				lastProgress = progress;
			}

		}
	}

	Log() << "\r    -> Found " << numExactMatches << " exact, " << numPartialMatches << " partial and " <<
			numShiftedMatches << " shifted matches.\n" << std::flush;
	linkCount = 0;
	unsigned int numM2LinksBefore = m_numM2Links;
	unsigned int tempNumM2LinksBefore = 0;
	unsigned int tempNumM2LinksAggregatedBefore = 0;
	unsigned int numAggregatedLinksBefore = m_numAggregatedM2Links;
	unsigned int numExactLinksAdded = 0;
	unsigned int numPartialLinksAdded = 0;
	unsigned int numShiftedLinksAdded = 0;
	unsigned int numExactAggregations = 0;
	unsigned int numPartialAggregations = 0;
	unsigned int numShiftedAggregations = 0;
	unsigned int numIncompleteLinksWithExactMatches = 0;
	unsigned int numIncompleteLinksWithPartialMatches = 0;
	unsigned int numIncompleteLinksWithShiftedMatches = 0;
	unsigned int numIncompleteLinksWithNoMatches = 0;

	// Create the additional memory links from the aggregated complementary data
	for (unsigned int i = 0; i < complementaryData.size(); ++i)
	{
		std::deque<ComplementaryData>& complementaryLinks = complementaryData[i];
		for (unsigned int i = 0; i < complementaryLinks.size(); ++i)
		{
			ComplementaryData& data = complementaryLinks[i];

			if (data.exactMatch.size() > 0)
			{
				tempNumM2LinksBefore = m_numM2Links;
				tempNumM2LinksAggregatedBefore = m_numAggregatedM2Links;
				for (ComplementaryData::MapType::const_iterator linkIt(data.exactMatch.begin()); linkIt != data.exactMatch.end(); ++linkIt)
					addM2Link(linkIt->first, data.incompleteLink.n1, data.incompleteLink.n1, data.incompleteLink.n2, data.incompleteLink.weight * linkIt->second / data.sumWeightExactMatch);
				numExactLinksAdded += m_numM2Links - tempNumM2LinksBefore;
				numExactAggregations += m_numAggregatedM2Links - tempNumM2LinksAggregatedBefore;
				++numIncompleteLinksWithExactMatches;
			}
			else if (data.partialMatch.size() > 0)
			{
				tempNumM2LinksBefore = m_numM2Links;
				tempNumM2LinksAggregatedBefore = m_numAggregatedM2Links;
				for (ComplementaryData::MapType::const_iterator linkIt(data.partialMatch.begin()); linkIt != data.partialMatch.end(); ++linkIt)
					addM2Link(linkIt->first, data.incompleteLink.n1, data.incompleteLink.n1, data.incompleteLink.n2, data.incompleteLink.weight * linkIt->second / data.sumWeightPartialMatch);
				numPartialLinksAdded += m_numM2Links - tempNumM2LinksBefore;
				numPartialAggregations += m_numAggregatedM2Links - tempNumM2LinksAggregatedBefore;
				++numIncompleteLinksWithPartialMatches;
			}
			else if (data.shiftedMatch.size() > 0)
			{
				tempNumM2LinksBefore = m_numM2Links;
				tempNumM2LinksAggregatedBefore = m_numAggregatedM2Links;
				for (ComplementaryData::MapType::const_iterator linkIt(data.shiftedMatch.begin()); linkIt != data.shiftedMatch.end(); ++linkIt)
					addM2Link(linkIt->first, data.incompleteLink.n1, data.incompleteLink.n1, data.incompleteLink.n2, data.incompleteLink.weight * linkIt->second / data.sumWeightShiftedMatch);
				numShiftedLinksAdded += m_numM2Links - tempNumM2LinksBefore;
				numShiftedAggregations += m_numAggregatedM2Links - tempNumM2LinksAggregatedBefore;
				++numIncompleteLinksWithShiftedMatches;
			}
			else
			{
				// No matching data to complement, create self-memory link
				addM2Link(data.incompleteLink.n1, data.incompleteLink.n1, data.incompleteLink.n1, data.incompleteLink.n2, data.incompleteLink.weight);
				++numIncompleteLinksWithNoMatches;
			}
		}
		++linkCount;
		unsigned int progress = linkCount * 1000 / complementaryData.size(); // 0.1% resolution
		if (progress != lastProgress) {
			Log() << "\r    -> Patching network from collected matches in priority... (" << progress * 0.1 << "%)      " << std::flush;
			lastProgress = progress;
		}
	}

	Log() << "\n  -> " << m_numM2Links - numM2LinksBefore << " memory links added and " <<
		m_numAggregatedM2Links - numAggregatedLinksBefore << " updated:" <<
		"\n    -> " << numIncompleteLinksWithExactMatches << " incomplete " << io::toPlural("link", numIncompleteLinksWithExactMatches) << " patched by " <<
			numExactAggregations << " updates and " << numExactLinksAdded << " new links from exact matches." <<
		"\n    -> " << numIncompleteLinksWithPartialMatches << " incomplete " << io::toPlural("link", numIncompleteLinksWithPartialMatches) << " patched by " <<
			numPartialAggregations << " updates and " << numPartialLinksAdded << " new links from partial matches." <<
		"\n    -> " << numIncompleteLinksWithShiftedMatches << " incomplete " << io::toPlural("link", numIncompleteLinksWithShiftedMatches) << " patched by " <<
			numShiftedAggregations << " updates and " << numShiftedLinksAdded << " new links from shifted matches." << std::flush;
	if (numIncompleteLinksWithNoMatches != 0)
		Log() << "\n    -> " << numIncompleteLinksWithNoMatches << " incomplete " << io::toPlural("link", numIncompleteLinksWithNoMatches) << " with no matches patched by self-links." << std::flush;

}

void MemNetwork::parseM2Link(const std::string& line, int& n1, unsigned int& n2, unsigned int& n3, double& weight)
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

void MemNetwork::parseM2Link(char line[], int& n1, unsigned int& n2, unsigned int& n3, double& weight)
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

	if(m_config.includeSelfLinks)
	{
		if (n1 == n2 && n1PriorState == n2PriorState)
		{
			++m_numMemorySelfLinks;
			m_totalMemorySelfLinkWeight += weight;
		}

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

bool MemNetwork::addM2Link(M2LinkMap::iterator firstM2Node, unsigned int n2PriorState, unsigned int n2, double weight, double firstM2NodeWeight, double secondM2NodeWeight)
{
	++m_numM2LinksFound;

	if (m_config.nodeLimit > 0 && (n2 >= m_config.nodeLimit))
		return false;

	const M2Node& m2Source = firstM2Node->first;
	unsigned int n1 = m2Source.physIndex;
	unsigned int n1PriorState = m2Source.priorState;

	if(m_config.includeSelfLinks)
	{
		if (n1 == n2 && n1PriorState == n2PriorState)
		{
			++m_numMemorySelfLinks;
			m_totalMemorySelfLinkWeight += weight;
		}

		insertM2Link(firstM2Node, n2PriorState, n2, weight);
		addM2Node(n1PriorState, n1, firstM2NodeWeight);
		addM2Node(n2PriorState, n2, secondM2NodeWeight);
	}
	else if (n1 != n2)
	{
		if(n1PriorState != n1)
		{
			insertM2Link(firstM2Node, n2PriorState, n2, weight);
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

bool MemNetwork::addIncompleteM2Link(unsigned int n1, unsigned int n2, double weight)
{
	++m_numIncompleteM2LinksFound;

	if (m_config.nodeLimit > 0 && (n1 >= m_config.nodeLimit || n2 >= m_config.nodeLimit))
		return false;

	++m_numIncompleteM2Links;

	// Aggregate link weights if they are definied more than once
	LinkMap::iterator firstIt = m_incompleteM2Links.lower_bound(n1);
	if (firstIt != m_incompleteM2Links.end() && firstIt->first == n1) // First linkEnd already exists, check second linkEnd
	{
		std::pair<std::map<unsigned int, double>::iterator, bool> ret2 = firstIt->second.insert(std::make_pair(n2, weight));
		if (!ret2.second)
		{
			ret2.first->second += weight;
			++m_numAggregatedIncompleteM2Links;
			--m_numIncompleteM2Links;
			return false;
		}
	}
	else
	{
		m_incompleteM2Links.insert(firstIt, std::make_pair(n1, std::map<unsigned int, double>()))->second.insert(std::make_pair(n2, weight));
	}

	return true;
}

void MemNetwork::finalizeAndCheckNetwork(bool printSummary)
{
	simulateMemoryToIncompleteData();

	if (m_m2Links.empty())
	{
		if (m_numLinks > 0)
			simulateMemoryFromOrdinaryNetwork();
		else
			throw InputDomainError("No memory links added!");
	}

	// If no nodes defined
	if (m_numNodes == 0)
		m_numNodes = m_numNodesFound = m_maxNodeIndex + 1;

	if (m_numNodesFound == 0)
		m_numNodesFound = m_numNodes;
	if (m_numLinksFound == 0)
		m_numLinksFound = m_numLinks;

	unsigned int zeroMinusOne = 0;
	--zeroMinusOne;
	if (m_maxNodeIndex == zeroMinusOne)
		throw InputDomainError(io::Str() << "Integer overflow, be sure to use zero-based node numbering if the node numbers start from zero.");
	if (m_maxNodeIndex >= m_numNodes)
		throw InputDomainError(io::Str() << "At least one link is defined with node numbers that exceeds the number of nodes.");
	if (m_minNodeIndex == 1 && m_config.zeroBasedNodeNumbers)
		Log() << "(Warning: minimum link index is one, check that you don't use zero based numbering if it's not true.) ";

	addMissingPhysicalNodes();

	m_m2NodeWeights.resize(m_m2Nodes.size());
	m_totM2NodeWeight = 0.0;
	unsigned int m2NodeIndex = 0;
	std::vector<unsigned int> existingPhysicalNodes(m_numNodes);
	for(std::map<M2Node,double>::iterator it = m_m2Nodes.begin(); it != m_m2Nodes.end(); ++it, ++m2NodeIndex)
	{
		m_m2NodeMap[it->first] = m2NodeIndex;
		m_m2NodeWeights[m2NodeIndex] += it->second;
		m_totM2NodeWeight += it->second;
	}

	initNodeDegrees();

	if (printSummary)
		printParsingResult();
}

unsigned int MemNetwork::addMissingPhysicalNodes()
{
	std::vector<unsigned int> existingPhysicalNodes(m_numNodes);
	for(std::map<M2Node,double>::iterator it = m_m2Nodes.begin(); it != m_m2Nodes.end(); ++it)
	{
		++existingPhysicalNodes[it->first.physIndex];
	}
	unsigned int numMissingPhysicalNodes = 0;
	for (unsigned int i = 0; i < m_numNodes; ++i)
	{
		if (existingPhysicalNodes[i] == 0)
		{
			++numMissingPhysicalNodes;
			addM2Node(i, i, 0.0);
		}
	}
	return numMissingPhysicalNodes;
}

void MemNetwork::initNodeDegrees()
{
	m_outDegree.assign(m_m2Nodes.size(), 0.0);
	m_sumLinkOutWeight.assign(m_m2Nodes.size(), 0.0);

	for (MemNetwork::M2LinkMap::const_iterator linkIt(m_m2Links.begin()); linkIt != m_m2Links.end(); ++linkIt)
	{
		const M2Node& m2source = linkIt->first;
		const std::map<M2Node, double>& subLinks = linkIt->second;

		// Get the index for the m2 source node
		MemNetwork::M2NodeMap::const_iterator nodeMapIt = m_m2NodeMap.find(m2source);
		if (nodeMapIt == m_m2NodeMap.end())
			throw InputDomainError(io::Str() << "Couldn't find mapped index for source M2 node " << m2source);
		unsigned int sourceIndex = nodeMapIt->second;

		for (std::map<M2Node, double>::const_iterator subIt(subLinks.begin()); subIt != subLinks.end(); ++subIt)
		{
//			const M2Node& m2target = subIt->first;
			double linkWeight = subIt->second;
//			nodeMapIt = m_m2NodeMap.find(m2target);
//			if (nodeMapIt == m_m2NodeMap.end())
//				throw InputDomainError(io::Str() << "Couldn't find mapped index for target M2 node " << m2target);
//			unsigned int targetIndex = nodeMapIt->second;

			++m_outDegree[sourceIndex];
			m_sumLinkOutWeight[sourceIndex] += linkWeight;

			// Never undirected memory links
		}
	}
}

void MemNetwork::printParsingResult(bool includeFirstOrderData)
{
	if (includeFirstOrderData)
	{
		Log() << "-------------------\n";
		Log() << "First order data:";
		Log() << "\n  -> Found " << m_numNodesFound << " nodes and " << m_numLinksFound << " links.";
		if(m_numAggregatedLinks > 0)
			Log() << "\n  -> " << m_numAggregatedLinks << " links was aggregated to existing links. ";
		if (m_numSelfLinks > 0 && !m_config.includeSelfLinks)
			Log() << "\n  -> " << m_numSelfLinks << " self-links was ignored. ";
		if (m_config.nodeLimit > 0)
			Log() << "\n  -> " << (m_numNodesFound - m_numNodes) << "/" << m_numNodesFound << " last nodes ignored due to limit. ";

		Log() << "\n  -> Resulting size: " << m_numNodes << " nodes";
		if (!m_nodeWeights.empty() && std::abs(m_sumNodeWeights / m_numNodes - 1.0) > 1e-9)
			Log() << " (with total weight " << m_sumNodeWeights << ")";
		Log() << " and " << m_numLinks << " links";
		if (std::abs(m_totalLinkWeight / m_numLinks - 1.0) > 1e-9)
			Log() << " (with total weight " << m_totalLinkWeight << ")";
		Log() << ".";
		Log() << "-------------------\n";
	}

	Log() << "  -> Found " << m_numNodesFound << " nodes and " << m_numM2LinksFound << " memory links.\n";
	Log() << "  -> Generated " << m_m2Nodes.size() << " memory nodes and " << m_numM2Links << " memory links.\n";
	if (m_numAggregatedM2Links > 0)
		Log() << "  -> Aggregated " << m_numAggregatedM2Links << " memory links.\n";
	Log() << std::flush;
}

void MemNetwork::printNetworkAsPajek(std::string filename) const
{
	SafeOutFile out(filename.c_str());

	out << "*Vertices " << m_numNodes << "\n";
	for (unsigned int i = 0; i < m_numNodes; ++i)
		out << (i+1) << " \"" << m_nodeNames[i] << "\"\n";

	out << "*3grams " << m_numM2Links << "\n";
	for (M2LinkMap::const_iterator linkIt(m_m2Links.begin()); linkIt != m_m2Links.end(); ++linkIt)
	{
		const M2Node& m2source = linkIt->first;
		const std::map<M2Node, double>& subLinks = linkIt->second;
		for (std::map<M2Node, double>::const_iterator subIt(subLinks.begin()); subIt != subLinks.end(); ++subIt)
		{
			const M2Node& m2target = subIt->first;
			double linkWeight = subIt->second;
			out << (m2source.priorState + 1) << " " << (m2source.physIndex + 1) << " " << (m2target.physIndex + 1) << " " << linkWeight << "\n";
		}
	}
}

void MemNetwork::disposeLinks()
{
	Network::disposeLinks();
	m_m2Links.clear();
	m_incompleteM2Links.clear();
}

#ifdef NS_INFOMAP
}
#endif
