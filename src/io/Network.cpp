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


#include "Network.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "../io/convert.h"
#include "../io/SafeFile.h"
#include "../utils/FileURI.h"
#include "../utils/Log.h"

namespace infomap {

using std::make_pair;

void Network::initValidHeadings()
{
	auto& headingsPajek = m_validHeadings["pajek"];
	headingsPajek.insert("*Vertices");
	headingsPajek.insert("*Edges");
	headingsPajek.insert("*Arcs");
	
	auto& headingsLinklist = m_validHeadings["link-list"];
	headingsLinklist.insert("*Links");
	headingsLinklist.insert("*Edges");
	headingsLinklist.insert("*Arcs");
	
	auto& headingsBipartite = m_validHeadings["bipartite"];
	headingsBipartite.insert("*Vertices");
	headingsBipartite.insert("*Bipartite");
	
	auto& headingsStates = m_validHeadings["states"];
	headingsStates.insert("*Vertices");
	headingsStates.insert("*States");
	headingsStates.insert("*Edges");
	headingsStates.insert("*Arcs");
	headingsStates.insert("*Links");
	headingsStates.insert("*Contexts");
	auto& ignoreHeadingsStates = m_ignoreHeadings["states"];
	// ignoreHeadingsStates.insert("*Arcs"); //TODO: Old allows this but what if both specified?
	ignoreHeadingsStates.insert("*Edges");
	ignoreHeadingsStates.insert("*Contexts");
	
	auto& headingsGeneral = m_validHeadings["general"];
	headingsGeneral.insert("*Vertices");
	headingsGeneral.insert("*States");
	headingsGeneral.insert("*Edges");
	headingsGeneral.insert("*Arcs");
	headingsGeneral.insert("*Links");
	headingsGeneral.insert("*Contexts");
	auto& ignoreHeadingsGeneral = m_ignoreHeadings["general"];
	// ignoreHeadingsGeneral.insert("*States");
	ignoreHeadingsGeneral.insert("*Contexts");
}

void Network::readInputData(std::string filename)
{
	if (filename.empty())
		filename = m_config.networkFile;
	if (filename == "") {
		throw InputSyntaxError("No input file to read network");
	}
	FileURI networkFilename(filename, false);
	std::string format = m_config.inputFormat;

	// if (format == "")
	// {
	// 	std::string type = networkFilename.getExtension();
	// 	if (type == "net")
	// 		format = "pajek";
	// 	else if (type == "txt")
	// 		format = "link-list";
	// }
	// if (format == "")
	// 	throw UnknownFileTypeError("No known input format specified or implied by file extension.");

	if (format == "pajek")
		parsePajekNetwork(filename);
	else if (format == "link-list")
		parseLinkList(filename);
	else if (format == "bipartite")
		parseBipartiteNetwork(filename);
	else if (format == "states")
		parseStateNetwork(filename);
	else 
		parseNetwork(filename);
//		throw UnknownFileTypeError("No known input format specified.");
	printSummary();
}

void Network::parsePajekNetwork(std::string filename)
{
	Log() << "Parsing " << (m_config.isUndirected() ? "undirected" : "directed") << " pajek network from file '" <<
			filename << "'... " << std::flush;

	parseNetwork(filename, m_validHeadings["pajek"], m_ignoreHeadings["pajek"]);
	Log() << "done!" << std::endl;
}

void Network::parseLinkList(std::string filename)
{
	Log() << "Parsing " << (m_config.directed ? "directed" : "undirected") << " link list from file '" <<
			filename << "'... " << std::flush;

	parseNetwork(filename, m_validHeadings["link-list"], m_ignoreHeadings["link-list"]);
	Log() << "done!" << std::endl;
}
void Network::parseBipartiteNetwork(std::string filename)
{
	Log() << "Parsing bipartite network from file '" <<
			filename << "'... " << std::flush;

	parseNetwork(filename, m_validHeadings["bipartite"], m_ignoreHeadings["bipartite"]);
	Log() << "done!" << std::endl;
}

void Network::parseStateNetwork(std::string filename)
{
	Log() << "Parsing state network from file '" <<
			filename << "'... " << std::flush;
	
	parseNetwork(filename, m_validHeadings["states"], m_ignoreHeadings["states"]);
	Log() << "done!" << std::endl;
}

void Network::parseNetwork(std::string filename)
{
	Log() << "Parsing " << (m_config.isUndirected() ? "undirected" : "directed") << " network from file '" <<
				filename << "'... " << std::flush;
	
	parseNetwork(filename, m_validHeadings["general"], m_ignoreHeadings["general"]);
	Log() << "done!" << std::endl;
}

void Network::parseNetwork(std::string filename, const InsensitiveStringSet& validHeadings, const InsensitiveStringSet& ignoreHeadings)
{
	SafeInFile input(filename.c_str());

	std::string line = parseLinks(input);

	while (line.length() > 0 && line[0] == '*')
	{
		std::string heading = io::firstWord(line);
		if (validHeadings.count(heading) == 0) {
			throw FileFormatError(io::Str() << "Unrecognized heading in network file: '" << heading << "'.");
		}
		if (ignoreHeadings.count(heading) > 0) {
			line = ignoreSection(input, heading);
		}
		else if (heading == "*Vertices" || heading == "*vertices") {
			line = parseVertices(input, line);
		}
		else if (heading == "*States" || heading == "*states") {
			line = parseStateNodes(input, line);
		}
		else if (heading == "*Edges" || heading == "*edges") {
			if (!m_config.parseAsUndirected())
				Log() << "\n --> Notice: Links marked as undirected but parsed as directed.\n";
			line = parseLinks(input);
		}
		else if (heading == "*Arcs" || heading == "*arcs") {
			if (m_config.parseAsUndirected())
				Log() << "\n --> Notice: Links marked as directed but parsed as undirected.\n";
			line = parseLinks(input);
		}
		else if (heading == "*Links" || heading == "*links") {
			line = parseLinks(input);
		}
		else if (heading == "*Bipartite" || heading == "*bipartite") {
			line = parseBipartiteLinks(input);
		}
		else {
			line = ignoreSection(input, heading);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
//
//  HELPER METHODS
//
//////////////////////////////////////////////////////////////////////////////////////////

std::string Network::parseVertices(std::ifstream& file, std::string heading)
{
	std::string line;
	while(!std::getline(file, line).fail())
	{
		if (line.length() == 0 || line[0] == '#')
			continue;

		if (line[0] == '*')
			break;

		// parseVertice(line, id, name, weight);
		m_extractor.clear();
		m_extractor.str(line);

		unsigned int id = 0;
		if (!(m_extractor >> id))
			throw FileFormatError(io::Str() << "Can't parse node id from line '" << line << "'");

		unsigned int nameStart = line.find_first_of("\"");
		unsigned int nameEnd = line.find_last_of("\"");
		std::string name("");
		if(nameStart < nameEnd) {
			name = std::string(line.begin() + nameStart + 1, line.begin() + nameEnd);
			line = line.substr(nameEnd + 1);
			m_extractor.clear();
			m_extractor.str(line);
		}
		else {
			if (!(m_extractor >> name))
				throw FileFormatError(io::Str() << "Can't parse node name from line '" << line << "'");
		}
		double weight = 1.0;
		if ((m_extractor >> weight)) {
			if (weight < 0)
				throw FileFormatError(io::Str() << "Negative node weight (" << weight << ") from line '" << line << "'");
		}
		if (m_config.isMemoryInput()) {
			addPhysicalNode(id, name);
		}
		else {
			addNode(id, name, weight);
		}
	}
	return line;
}

std::string Network::parseStateNodes(std::ifstream& file, std::string heading)
{
	std::string line;
	while(!std::getline(file, line).fail())
	{
		if (line.length() == 0 || line[0] == '#')
			continue;

		if (line[0] == '*')
			break;

		StateNode stateNode;
		parseStateNode(line, stateNode);

		addStateNode(stateNode);
		addPhysicalNode(stateNode.physicalId);

		++m_numStateNodesFound;
	}
	return line;
}

std::string Network::parseLinks(std::ifstream& file)
{
	std::string line;
	while(!std::getline(file, line).fail())
	{
		if (line.length() == 0 || line[0] == '#')
			continue;

		if (line[0] == '*')
			break;

		unsigned int n1, n2;
		double weight;
		parseLink(line, n1, n2, weight);

		addLink(n1, n2, weight);
	}
	return line;
}

std::string Network::parseBipartiteLinks(std::ifstream& file)
{
	m_config.bipartite = true;
	std::string line;
	while(!std::getline(file, line).fail())
	{
		if (line.length() == 0 || line[0] == '#')
			continue;

		if (line[0] == '*')
			break;

		// unsigned int featureNode, ordinaryNode;
		// double weight;
		// bool swappedOrder = parseBipartiteLink(line, featureNode, ordinaryNode, weight);

		// addBipartiteLink(featureNode, ordinaryNode, swappedOrder, weight);
	}
	return line;
}

std::string Network::ignoreSection(std::ifstream& file, std::string heading)
{
	Log() << "(Ignoring section " << heading << ") ";
	std::string line;
	while(!std::getline(file, line).fail())
	{
		if (line[0] == '*')
			break;
	}
	return line;
}

// void Network::parseVertice(const std::string& line, unsigned int& id, std::string& name, double& weight)
// {
// 	m_extractor.clear();
// 	m_extractor.str(line);
// 	if (!(m_extractor >> id >> name))
// 		throw FileFormatError(io::Str() << "Can't parse link data from line '" << line << "'");
// 	(m_extractor >> weight) || (weight = 1.0);
// }

void Network::parseStateNode(const std::string& line, StateNetwork::StateNode& stateNode)
{
	m_extractor.clear();
	m_extractor.str(line);
	if (!(m_extractor >> stateNode.id >> stateNode.physicalId))
		throw FileFormatError(io::Str() << "Can't parse any state node from line '" << line << "'");
	(m_extractor >> stateNode.weight) || (stateNode.weight = 1.0);
}

void Network::parseLink(const std::string& line, unsigned int& n1, unsigned int& n2, double& weight)
{
	m_extractor.clear();
	m_extractor.str(line);
	if (!(m_extractor >> n1 >> n2))
		throw FileFormatError(io::Str() << "Can't parse link data from line '" << line << "'");
	(m_extractor >> weight) || (weight = 1.0);
}

bool Network::parseBipartiteLink(const std::string& line, unsigned int& featureNode, unsigned int& node, double& weight)
{
	bool swappedOrder = false;
	m_extractor.clear();
	m_extractor.str(line);
	std::string fn, n;
	if (!(m_extractor >> fn >> n))
		throw FileFormatError(io::Str() << "Can't parse bipartite link data from line '" << line << "'");
	(m_extractor >> weight) || (weight = 1.0);
	if (fn[0] != 'f') {
		std::swap(fn, n);
		swappedOrder = true;
	}
	if (fn[0] != 'f' || fn.length() == 1 || !(std::istringstream( fn.substr(1) ) >> featureNode))
		throw FileFormatError(io::Str() << "Can't parse bipartite feature node (a numerical id prefixed by 'f') from line '" << line << "'");
	if (n[0] != 'n' || n.length() == 1 || !(std::istringstream( n.substr(1) ) >> node))
		throw FileFormatError(io::Str() << "Can't parse bipartite ordinary node (a numerical id prefixed by 'n') from line '" << line << "'");

	return swappedOrder;
}

void Network::printSummary()
{
	Log() << " -> " << numNodes() << " state nodes\n";
	Log() << " -> " << numPhysicalNodes() << " physical nodes\n";
}


// bool Network::addLink(unsigned int n1, unsigned int n2, double weight)
// {
// 	++m_numLinksFound;

// 	if (m_config.nodeLimit > 0 && (n1 >= m_config.nodeLimit || n2 >= m_config.nodeLimit))
// 		return false;

// 	if (n2 == n1)
// 	{
// 		++m_numSelfLinksFound;
// 		if (!m_config.includeSelfLinks)
// 			return false;
// 		++m_numSelfLinks;
// 		m_totalSelfLinkWeight += weight;
// 	}
// 	else if (m_config.parseAsUndirected() && n2 < n1) // minimize number of links
// 		std::swap(n1, n2);

// 	m_maxNodeIndex = std::max(m_maxNodeIndex, std::max(n1, n2));
// 	m_minNodeIndex = std::min(m_minNodeIndex, std::min(n1, n2));

// 	insertLink(n1, n2, weight);

// 	return true;
// }

// bool Network::addBipartiteLink(unsigned int featureNode, unsigned int node, bool swapOrder, double weight)
// {
// 	++m_numLinksFound;

// 	if (m_config.nodeLimit > 0 && node >= m_config.nodeLimit)
// 		return false;

// 	m_maxNodeIndex = std::max(m_maxNodeIndex, node);
// 	m_minNodeIndex = std::min(m_minNodeIndex, node);

// 	m_bipartiteLinks[BipartiteLink(featureNode, node, swapOrder)] += weight;

// 	return true;
// }


// bool Network::insertLink(unsigned int n1, unsigned int n2, double weight)
// {
// 	++m_numLinks;
// 	m_totalLinkWeight += weight;

// 	// Aggregate link weights if they are definied more than once
// 	LinkMap::iterator firstIt = m_links.lower_bound(n1);
// 	if (firstIt != m_links.end() && firstIt->first == n1) // First linkEnd already exists, check second linkEnd
// 	{
// 		std::pair<std::map<unsigned int, double>::iterator, bool> ret2 = firstIt->second.insert(std::make_pair(n2, weight));
// 		if (!ret2.second)
// 		{
// 			ret2.first->second += weight;
// 			++m_numAggregatedLinks;
// 			--m_numLinks;
// 			return false;
// 		}
// 	}
// 	else
// 	{
// 		m_links.insert(firstIt, std::make_pair(n1, std::map<unsigned int, double>()))->second.insert(std::make_pair(n2, weight));
// 	}

// 	return true;
// }

// void Network::finalizeAndCheckNetwork(bool printSummary, unsigned int desiredNumberOfNodes)
// {
// 	// If no nodes defined
// 	if (m_numNodes == 0)
// 		m_numNodes = m_numNodesFound = m_maxNodeIndex + 1;

// 	if (desiredNumberOfNodes != 0)
// 	{
// 		if (!m_nodeNames.empty() && desiredNumberOfNodes != m_nodeNames.size()) {
// 			// throw InputDomainError("Can't change the number of nodes in networks with a specified number of nodes.");
// 			m_nodeNames.reserve(desiredNumberOfNodes);
// 			for (unsigned int i = m_nodeNames.size(); i < desiredNumberOfNodes; ++i) {
// 				m_nodeNames.push_back(io::Str() << "_completion_node_" << (i + 1));
// 			}
// 		}
// 		m_numNodes = desiredNumberOfNodes;
// 	}

// 	unsigned int zeroMinusOne = 0;
// 	--zeroMinusOne;
// 	if (m_maxNodeIndex == zeroMinusOne)
// 		throw InputDomainError(io::Str() << "Integer overflow, be sure to use zero-based node numbering if the node numbers start from zero.");
// 	if (m_maxNodeIndex >= m_numNodes)
// 		throw InputDomainError(io::Str() << "At least one link is defined with node numbers that exceeds the number of nodes.");
// 	if (m_minNodeIndex == 1 && m_config.zeroBasedNodeNumbers)
// 		Log() << "(Warning: minimum link index is one, check that you don't use zero based numbering if it's not true.) ";

// 	if (!m_bipartiteLinks.empty())
// 	{
// 		if (m_numLinks > 0)
// 			throw InputDomainError("Can't add bipartite links together with ordinary links.");
// 		for (std::map<BipartiteLink, Weight>::iterator it(m_bipartiteLinks.begin()); it != m_bipartiteLinks.end(); ++it)
// 		{
// 			const BipartiteLink& link = it->first;
// 			// Offset feature nodes by the number of ordinary nodes to make them unique
// 			unsigned int featureNodeIndex = link.featureNode + m_numNodes;
// 			m_maxNodeIndex = std::max(m_maxNodeIndex, featureNodeIndex);
// 			if (link.swapOrder)
// 				insertLink(link.node, featureNodeIndex, it->second.weight);
// 			else
// 				insertLink(featureNodeIndex, link.node, it->second.weight);
// 		}
// 		m_numBipartiteNodes = m_maxNodeIndex + 1 - m_numNodes;
// 		m_numNodes += m_numBipartiteNodes;
// 	}

// 	if (m_links.empty())
// 		throw InputDomainError("No links added!");

// 	if (m_addSelfLinks)
// 		zoom();

// 	initNodeDegrees();

// 	if (printSummary)
// 		printParsingResult();
// }

// void Network::zoom()
// {
// 	unsigned int numNodes = m_numNodes;
// 	std::vector<unsigned int> nodeOutDegree(numNodes, 0);
// 	std::vector<double> sumLinkOutWeight(numNodes, 0.0);
// 	std::map<unsigned int, double> dummy;
// 	std::vector<std::map<unsigned int, double>::iterator> existingSelfLinks(numNodes, dummy.end());

// 	for (LinkMap::iterator linkIt(m_links.begin()); linkIt != m_links.end(); ++linkIt)
// 	{
// 		unsigned int linkEnd1 = linkIt->first;
// 		std::map<unsigned int, double>& subLinks = linkIt->second;
// 		for (std::map<unsigned int, double>::iterator subIt(subLinks.begin()); subIt != subLinks.end(); ++subIt)
// 		{
// 			unsigned int linkEnd2 = subIt->first;
// 			double linkWeight = subIt->second;
// 			++nodeOutDegree[linkEnd1];
// 			if (linkEnd1 == linkEnd2)
// 			{
// 				// Store existing self-link to aggregate additional weight
// 				existingSelfLinks[linkEnd1] = subIt;
// //				sumLinkOutWeight[linkEnd1] += linkWeight;
// 			}
// 			else
// 			{
// 				if (m_config.isUndirected())
// 				{
// 					sumLinkOutWeight[linkEnd1] += linkWeight * 0.5; // Why half?
// 					sumLinkOutWeight[linkEnd2] += linkWeight * 0.5;
// 					++nodeOutDegree[linkEnd2];
// 				}
// 				else
// 				{
// 					sumLinkOutWeight[linkEnd1] += linkWeight;
// 				}
// 			}
// 		}
// 	}

// 	double selfProb = m_config.selfTeleportationProbability;

// 	for (unsigned int i = 0; i < numNodes; ++i)
// 	{
// 		if (nodeOutDegree[i] == 0)
// 			continue; // Skip dangling nodes at the moment

// 		double selfLinkWeight = sumLinkOutWeight[i] * selfProb / (1.0 - selfProb);

// 		if (existingSelfLinks[i] != dummy.end()) {
// 			existingSelfLinks[i]->second += selfLinkWeight;
// 		}
// 		else {
// 			m_links[i].insert(std::make_pair(i, selfLinkWeight));
// 			++m_numAdditionalLinks;
// 		}
// 		m_sumAdditionalLinkWeight += selfLinkWeight;
// 	}

// 	m_numLinks += m_numAdditionalLinks;
// 	m_numSelfLinks += m_numAdditionalLinks;
// 	m_totalLinkWeight += m_sumAdditionalLinkWeight;
// 	m_totalSelfLinkWeight += m_sumAdditionalLinkWeight;
// }

// void Network::initNodeDegrees()
// {
// 	m_outDegree.assign(m_numNodes, 0.0);
// 	m_sumLinkOutWeight.assign(m_numNodes, 0.0);
// 	m_numDanglingNodes = m_numNodes;
// 	for (LinkMap::iterator linkIt(m_links.begin()); linkIt != m_links.end(); ++linkIt)
// 	{
// 		unsigned int n1 = linkIt->first;
// 		std::map<unsigned int, double>& subLinks = linkIt->second;
// 		for (std::map<unsigned int, double>::iterator subIt(subLinks.begin()); subIt != subLinks.end(); ++subIt)
// 		{
// 			unsigned int n2 = subIt->first;
// 			double linkWeight = subIt->second;
// 			if (m_outDegree[n1] == 0)
// 				--m_numDanglingNodes;
// 			++m_outDegree[n1];
// 			m_sumLinkOutWeight[n1] += linkWeight;
// 			if (n1 != n2 && m_config.parseAsUndirected())
// 			{
// 				if (m_outDegree[n2] == 0)
// 					--m_numDanglingNodes;
// 				++m_outDegree[n2];
// 				m_sumLinkOutWeight[n2] += linkWeight;
// 			}
// 		}
// 	}
// }

// void Network::printParsingResult(bool onlySummary)
// {
// 	bool dataModified = m_numNodesFound != m_numNodes || m_numLinksFound != m_numLinks;

// 	if (onlySummary)
// 	{
// 		Log() << " ==> " << getParsingResultSummary() << '\n';
// 		return;
// 	}

// 	if (!dataModified)
// 		Log() << " ==> " << getParsingResultSummary();
// 	else {
// 		Log() << " --> Found " << m_numNodesFound << io::toPlural(" node", m_numNodesFound);
// 		Log() << " and " << m_numLinksFound << io::toPlural(" link", m_numLinksFound) << ".";
// 	}

// 	if(m_numAggregatedLinks > 0)
// 		Log() << "\n --> Aggregated " << m_numAggregatedLinks << io::toPlural(" link", m_numAggregatedLinks) << " to existing links.";
// 	if (m_numSelfLinksFound > 0 && !m_config.includeSelfLinks)
// 		Log() << "\n --> Ignored " << m_numSelfLinksFound << io::toPlural(" self-link", m_numSelfLinksFound) << ".";
// 	unsigned int numNodesIgnored = m_numNodesFound - m_numNodes;
// 	if (m_config.nodeLimit > 0)
// 		Log() << "\n --> Ignored " << numNodesIgnored << io::toPlural(" node", numNodesIgnored) << " due to specified limit.";
// 	if (m_numDanglingNodes > 0)
// 		Log() << "\n --> " << m_numDanglingNodes << " dangling " << io::toPlural("node", m_numDanglingNodes) << " (nodes with no outgoing links).";

// 	if (m_numAdditionalLinks > 0)
// 		Log() << "\n --> Added " << m_numAdditionalLinks << io::toPlural(" self-link", m_numAdditionalLinks) << " with total weight " << m_sumAdditionalLinkWeight << ".";
// 	if (m_numSelfLinks > 0) {
// 		Log() << "\n --> " << m_numSelfLinks << io::toPlural(" self-link", m_numSelfLinks);
// 		Log() << " with total weight " << m_totalSelfLinkWeight << " (" << (m_totalSelfLinkWeight / m_totalLinkWeight * 100) << "% of the total link weight).";
// 	}

// 	if (dataModified) {
// 		Log() << "\n ==> " << getParsingResultSummary();
// 	}

// 	if (isBipartite())
// 		Log() << "\nBipartite => " << m_numNodes - m_numBipartiteNodes << " ordinary nodes and " <<
// 			m_numBipartiteNodes << " feature nodes.";

// 	Log() << std::endl;
// }

// std::string Network::getParsingResultSummary()
// {
// 	std::ostringstream o;
// 	o << m_numNodes << io::toPlural(" node", m_numNodes);
// 	if (!m_nodeWeights.empty() && std::abs(m_sumNodeWeights / m_numNodes - 1.0) > 1e-9)
// 		o << " (with total weight " << m_sumNodeWeights << ")";
// 	o << " and " << m_numLinks << io::toPlural(" link", m_numLinks);
// 	if (std::abs(m_totalLinkWeight / m_numLinks - 1.0) > 1e-9)
// 		o << " (with total weight " << m_totalLinkWeight << ")";
// 	o << ".";
// 	return o.str();
// }


// void Network::printNetworkAsPajek(std::string filename) const
// {
// 	SafeOutFile out(filename.c_str());

// 	out << "*Vertices " << m_numNodes << "\n";
// 	if (m_nodeNames.empty()) {
// 		for (unsigned int i = 0; i < m_numNodes; ++i)
// 			out << (i+1) << " \"" << i + 1 << "\"\n";
// 	}
// 	else {
// 		for (unsigned int i = 0; i < m_numNodes; ++i)
// 			out << (i+1) << " \"" << m_nodeNames[i] << "\"\n";
// 	}

// 	out << (m_config.isUndirected() ? "*Edges " : "*Arcs ") << m_links.size() << "\n";
// 	for (LinkMap::const_iterator linkIt(m_links.begin()); linkIt != m_links.end(); ++linkIt)
// 	{
// 		unsigned int linkEnd1 = linkIt->first;
// 		const std::map<unsigned int, double>& subLinks = linkIt->second;
// 		for (std::map<unsigned int, double>::const_iterator subIt(subLinks.begin()); subIt != subLinks.end(); ++subIt)
// 		{
// 			unsigned int linkEnd2 = subIt->first;
// 			double linkWeight = subIt->second;
// 			out << (linkEnd1 + 1) << " " << (linkEnd2 + 1) << " " << linkWeight << "\n";
// 		}
// 	}
// }

// void Network::printStateNetwork(std::string filename) const
// {
// 	SafeOutFile out(filename.c_str());

// 	out << "*States " << m_numNodes << "\n";
// 	if (m_nodeNames.empty()) {
// 		for (unsigned int i = 0; i < m_numNodes; ++i)
// 			out << (i+1) << " \"" << i + 1 << "\"\n";
// 	}
// 	else {
// 		for (unsigned int i = 0; i < m_numNodes; ++i)
// 			out << (i+1) << " \"" << m_nodeNames[i] << "\"\n";
// 	}

// 	out << (m_config.isUndirected() ? "*Edges " : "*Arcs ") << m_links.size() << "\n";
// 	for (LinkMap::const_iterator linkIt(m_links.begin()); linkIt != m_links.end(); ++linkIt)
// 	{
// 		unsigned int linkEnd1 = linkIt->first;
// 		const std::map<unsigned int, double>& subLinks = linkIt->second;
// 		for (std::map<unsigned int, double>::const_iterator subIt(subLinks.begin()); subIt != subLinks.end(); ++subIt)
// 		{
// 			unsigned int linkEnd2 = subIt->first;
// 			double linkWeight = subIt->second;
// 			out << (linkEnd1 + 1) << " " << (linkEnd2 + 1) << " " << linkWeight << "\n";
// 		}
// 	}
// }

}
