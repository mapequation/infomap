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
#include <algorithm>

#include "../io/convert.h"
#include "../io/SafeFile.h"
#include "../utils/FileURI.h"
#include "../utils/Log.h"

namespace infomap {

using std::make_pair;

void Network::initValidHeadings()
{
  auto& headingsPajek = m_validHeadings["pajek"];
  headingsPajek.insert("*vertices");
  headingsPajek.insert("*edges");
  headingsPajek.insert("*arcs");

  auto& headingsLinklist = m_validHeadings["link-list"];
  headingsLinklist.insert("*links");
  headingsLinklist.insert("*edges");
  headingsLinklist.insert("*arcs");

  auto& headingsBipartite = m_validHeadings["bipartite"];
  headingsBipartite.insert("*vertices");
  headingsBipartite.insert("*bipartite");

  auto& headingsStates = m_validHeadings["states"];
  headingsStates.insert("*vertices");
  headingsStates.insert("*states");
  headingsStates.insert("*edges");
  headingsStates.insert("*arcs");
  headingsStates.insert("*links");
  headingsStates.insert("*contexts");
  auto& ignoreHeadingsStates = m_ignoreHeadings["states"];
  // ignoreHeadingsStates.insert("*arcs"); //TODO: Old allows this but what if both specified?
  ignoreHeadingsStates.insert("*edges");
  ignoreHeadingsStates.insert("*contexts");

  auto& headingsMultilayer = m_validHeadings["multilayer"];
  headingsMultilayer.insert("*vertices");
  headingsMultilayer.insert("*multiplex");
  headingsMultilayer.insert("*multilayer");
  headingsMultilayer.insert("*intra");
  headingsMultilayer.insert("*inter");
  // auto& ignoreHeadingsMultilayer = m_ignoreHeadings["multilayer"];

  auto& headingsGeneral = m_validHeadings["general"];
  headingsGeneral.insert("*vertices");
  headingsGeneral.insert("*states");
  headingsGeneral.insert("*multilayer");
  headingsGeneral.insert("*intra");
  headingsGeneral.insert("*inter");
  headingsGeneral.insert("*paths");
  headingsGeneral.insert("*edges");
  headingsGeneral.insert("*arcs");
  headingsGeneral.insert("*links");
  headingsGeneral.insert("*contexts");
  headingsGeneral.insert("*bipartite");
  auto& ignoreHeadingsGeneral = m_ignoreHeadings["general"];
  // ignoreHeadingsGeneral.insert("*states");
  ignoreHeadingsGeneral.insert("*contexts");
}

void Network::clear()
{
  StateNetwork::clear();
  m_networks.clear();
  m_interLinks.clear();
  m_layerNodeToStateId.clear();
  m_sumIntraOutWeight.clear();
  m_layers.clear();
  m_numInterLayerLinks = 0;
  m_numIntraLayerLinks = 0;

  // Bipartite
  m_bipartiteStartId = 0;

  // Meta data
  m_metaData.clear();
  m_numMetaDataColumns = 0;
}

void Network::readInputData(std::string filename, bool accumulate)
{
  if (!accumulate) {
    clear();
  }
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

  if (format == "")
    parseNetwork(filename);
  else if (format == "pajek")
    parsePajekNetwork(filename);
  else if (format == "link-list")
    parseLinkList(filename);
  else if (format == "bipartite")
    parseBipartiteNetwork(filename);
  else if (format == "states")
    parseStateNetwork(filename);
  else if (format == "multilayer")
    parseMultilayerNetwork(filename);
  else
    throw UnknownFileTypeError(io::Str() << "Unrecognised input format specified: '" << format << "'.");
  printSummary();
}

void Network::parsePajekNetwork(std::string filename)
{
  Log() << "Parsing " << (m_config.isUndirectedFlow() ? "undirected" : "directed") << " pajek network from file '" << filename << "'... " << std::endl;

  parseNetwork(filename, m_validHeadings["pajek"], m_ignoreHeadings["pajek"]);
}

void Network::parseLinkList(std::string filename)
{
  Log() << "Parsing " << (m_config.directed ? "directed" : "undirected") << " link list from file '" << filename << "'... " << std::endl;

  parseNetwork(filename, m_validHeadings["link-list"], m_ignoreHeadings["link-list"]);
}

void Network::parseBipartiteNetwork(std::string filename)
{
  Log() << "Parsing bipartite network from file '" << filename << "'... " << std::endl;

  parseNetwork(filename, m_validHeadings["bipartite"], m_ignoreHeadings["bipartite"]);
}

void Network::parseMultilayerNetwork(std::string filename)
{
  Log() << "Parsing multilayer network from file '" << filename << "'... " << std::endl;

  parseNetwork(filename, m_validHeadings["multilayer"], m_ignoreHeadings["multilayer"], "*multilayer");
}

void Network::parseStateNetwork(std::string filename)
{
  Log() << "Parsing state network from file '" << filename << "'... " << std::endl;

  parseNetwork(filename, m_validHeadings["states"], m_ignoreHeadings["states"]);
}

void Network::parseNetwork(std::string filename)
{
  Log() << "Parsing " << (m_config.isUndirectedFlow() ? "undirected" : "directed") << " network from file '" << filename << "'... " << std::endl;

  parseNetwork(filename, m_validHeadings["general"], m_ignoreHeadings["general"]);
}

void Network::parseNetwork(std::string filename, const InsensitiveStringSet& validHeadings, const InsensitiveStringSet& ignoreHeadings, std::string startHeading)
{
  SafeInFile input(filename.c_str());

  // Parse standard links by default until possible heading is reached
  std::string heading = startHeading.length() > 0 ? startHeading : parseLinks(input);

  while (heading.length() > 0 && heading[0] == '*') {
    std::string headingLowerCase = io::tolower(io::firstWord(heading));
    if (validHeadings.count(headingLowerCase) == 0) {
      throw FileFormatError(io::Str() << "Unrecognized heading in network file: '" << headingLowerCase << "'.");
    }
    if (ignoreHeadings.count(headingLowerCase) > 0) {
      heading = ignoreSection(input, headingLowerCase);
    } else if (headingLowerCase == "*vertices") {
      heading = parseVertices(input, heading);
    } else if (headingLowerCase == "*states") {
      heading = parseStateNodes(input, heading);
    } else if (headingLowerCase == "*edges") {
      if (!m_config.isUndirectedFlow())
        Log() << "\n --> Notice: Links marked as undirected but parsed as directed.\n";
      heading = parseLinks(input);
    } else if (headingLowerCase == "*arcs") {
      if (m_config.isUndirectedFlow())
        Log() << "\n --> Notice: Links marked as directed but parsed as undirected.\n";
      heading = parseLinks(input);
    } else if (headingLowerCase == "*links") {
      heading = parseLinks(input);
    } else if (headingLowerCase == "*multilayer" || headingLowerCase == "*multiplex") {
      heading = parseMultilayerLinks(input);
    } else if (headingLowerCase == "*intra") {
      heading = parseMultilayerIntraLinks(input);
    } else if (headingLowerCase == "*inter") {
      heading = parseMultilayerInterLinks(input);
    } else if (headingLowerCase == "*bipartite") {
      heading = parseBipartiteLinks(input, heading);
    } else {
      heading = ignoreSection(input, headingLowerCase);
    }
  }

  if (!m_networks.empty()) {
    generateStateNetworkFromMultilayer();
  }

  if (!haveMemoryInput()) {
    // If no memory input, add physical nodes as state nodes to not miss unconnected nodes
    for (auto& it : m_physNodes) {
      addNode(it.second.physId, it.second.weight);
    }
  }

  Log() << "Done!\n";
}


void Network::readMetaData(std::string filename)
{
  Log() << "Parsing meta data from '" << filename << "'..." << std::endl;
  SafeInFile input(filename.c_str());
  std::string line;
  while (!std::getline(input, line).fail()) {
    if (line.length() == 0 || line[0] == '#')
      continue;

    if (line[0] == '*')
      break;

    // parseVertice(line, id, name, weight);
    m_extractor.clear();
    m_extractor.str(line);

    unsigned int nodeId;
    if (!(m_extractor >> nodeId))
      throw FileFormatError(io::Str() << "Can't parse node id from line '" << line << "'");

    std::vector<int> metaData;
    unsigned int metaId;
    while (m_extractor >> metaId) {
      metaData.push_back(metaId);
    }
    if (metaData.empty())
      throw FileFormatError(io::Str() << "Can't parse any meta data from line '" << line << "'");

    addMetaData(nodeId, metaData);
  }
  Log() << " -> Parsed " << m_numMetaDataColumns << " columns of meta data for " << m_metaData.size() << " nodes." << std::endl;
}
//////////////////////////////////////////////////////////////////////////////////////////
//
//  HELPER METHODS
//
//////////////////////////////////////////////////////////////////////////////////////////

std::string Network::parseVertices(std::ifstream& file, std::string heading)
{
  Log() << "  Parsing vertices...\n"
        << std::flush;
  std::string line;
  while (!std::getline(file, line).fail()) {
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

    auto nameStart = line.find_first_of("\"");
    auto nameEnd = line.find_last_of("\"");
    std::string name("");
    if (nameStart < nameEnd) {
      name = std::string(line.begin() + nameStart + 1, line.begin() + nameEnd);
      line = line.substr(nameEnd + 1);
      m_extractor.clear();
      m_extractor.str(line);
    } else {
      if (!(m_extractor >> name))
        throw FileFormatError(io::Str() << "Can't parse node name from line '" << line << "'");
    }
    double weight = 1.0;
    if ((m_extractor >> weight)) {
      if (weight < 0)
        throw FileFormatError(io::Str() << "Negative node weight (" << weight << ") from line '" << line << "'");
    }

    addPhysicalNode(id, weight, name);
  }
  Log() << "  -> " << m_physNodes.size() << " physical nodes added\n";
  return line;
}

std::string Network::parseStateNodes(std::ifstream& file, /* [[maybe_unused]] */ std::string heading)
{
  Log() << "  Parsing state nodes...\n"
        << std::flush;
  std::string line;
  while (!std::getline(file, line).fail()) {
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
  Log() << "  -> " << m_numStateNodesFound << " state nodes added\n";
  return line;
}

std::string Network::parseLinks(std::ifstream& file)
{
  // This is the default action, so check for links before printing
  bool parsingLinks = false;
  std::string line;
  while (!std::getline(file, line).fail()) {
    if (line.length() == 0 || line[0] == '#')
      continue;

    if (line[0] == '*')
      break;

    if (!parsingLinks) {
      parsingLinks = true;
      Log() << "  Parsing links...\n"
            << std::flush;
    }

    unsigned int n1, n2;
    double weight;
    parseLink(line, n1, n2, weight);

    addLink(n1, n2, weight);
  }
  if (parsingLinks)
    Log() << "  -> " << m_numLinks << " links\n";
  return line;
}

std::string Network::parseMultilayerLinks(std::ifstream& file)
{
  Log() << "  Parsing multilayer links...\n"
        << std::flush;
  std::string line;
  while (!std::getline(file, line).fail()) {
    if (line.length() == 0 || line[0] == '#')
      continue;

    if (line[0] == '*')
      break;

    unsigned int layer1, n1, layer2, n2;
    double weight;
    parseMultilayerLink(line, layer1, n1, layer2, n2, weight);

    //TODO: This explicit multilayer format can allow undirected but not the inter/intra format, clear?
    addMultilayerLink(layer1, n1, layer2, n2, weight);
  }
  Log() << "  -> " << (m_numIntraLayerLinks + m_numInterLayerLinks) << " links in " << m_layers.size() << " layers\n";
  Log() << "    -> " << m_numIntraLayerLinks << " intra-layer links\n";
  Log() << "    -> " << m_numInterLayerLinks << " inter-layer links\n";
  return line;
}

std::string Network::parseMultilayerIntraLinks(std::ifstream& file)
{
  Log() << "  Parsing intra-layer links...\n"
        << std::flush;
  std::string line;
  while (!std::getline(file, line).fail()) {
    if (line.length() == 0 || line[0] == '#')
      continue;

    if (line[0] == '*')
      break;

    unsigned int layer, n1, n2;
    double weight;
    parseMultilayerIntraLink(line, layer, n1, n2, weight);

    // addMultilayerLink(layer, n1, layer, n2, weight);
    addMultilayerIntraLink(layer, n1, n2, weight);
  }
  Log() << "  -> " << m_numIntraLayerLinks << " intra-layer links\n";
  return line;
}

std::string Network::parseMultilayerInterLinks(std::ifstream& file)
{
  Log() << "  Parsing inter-layer links...\n"
        << std::flush;
  std::string line;
  while (!std::getline(file, line).fail()) {
    if (line.length() == 0 || line[0] == '#')
      continue;

    if (line[0] == '*')
      break;

    unsigned int layer1, n, layer2;
    double weight;
    parseMultilayerInterLink(line, layer1, n, layer2, weight);

    addMultilayerInterLink(layer1, n, layer2, weight);
  }
  Log() << "  -> " << m_numInterLayerLinks << " inter-layer links\n";
  return line;
}

std::string Network::parseBipartiteLinks(std::ifstream& file, std::string heading)
{
  Log() << "  Parsing bipartite links...\n"
        << std::flush;
  // Extract break point for bipartite links
  m_extractor.clear();
  m_extractor.str(heading);
  std::string tmp;
  if (!(m_extractor >> tmp >> m_bipartiteStartId))
    throw FileFormatError(io::Str() << "Can't parse bipartite start id from line '" << heading << "'");

  Log() << "  -> Using bipartite start id " << m_bipartiteStartId << "\n";
  m_config.bipartite = true;
  std::string line;
  while (!std::getline(file, line).fail()) {
    if (line.length() == 0 || line[0] == '#')
      continue;

    if (line[0] == '*')
      break;

    unsigned int n1, n2;
    double weight;
    parseLink(line, n1, n2, weight);
    bool sourceIsFeature = n1 >= m_bipartiteStartId;
    bool targetIsFeature = n2 >= m_bipartiteStartId;
    if (sourceIsFeature == targetIsFeature) {
      throw FileFormatError(io::Str() << "Bipartite link '" << line << "' must cross bipartite start id " << m_bipartiteStartId << ".");
    }
    addLink(n1, n2, weight);
  }
  return line;
}

std::string Network::ignoreSection(std::ifstream& file, std::string heading)
{
  Log() << "(Ignoring section " << heading << ") ";
  std::string line;
  while (!std::getline(file, line).fail()) {
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

  // Optional name enclosed in double quotes
  auto nameStart = line.find_first_of("\"", m_extractor.tellg());
  auto nameEnd = line.find_last_of("\"");
  if (nameStart < nameEnd) {
    stateNode.name = std::string(line.begin() + nameStart + 1, line.begin() + nameEnd);
    m_extractor.seekg(nameEnd + 1);
  }
  // Optional weight, default to 1.0
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

void Network::parseMultilayerLink(const std::string& line, unsigned int& layer1, unsigned int& n1, unsigned int& layer2, unsigned int& n2, double& weight)
{
  m_extractor.clear();
  m_extractor.str(line);
  if (!(m_extractor >> layer1 >> n1 >> layer2 >> n2))
    throw FileFormatError(io::Str() << "Can't parse multilayer link data from line '" << line << "'");
  (m_extractor >> weight) || (weight = 1.0);
}

void Network::parseMultilayerIntraLink(const std::string& line, unsigned int& layer, unsigned int& n1, unsigned int& n2, double& weight)
{
  m_extractor.clear();
  m_extractor.str(line);
  if (!(m_extractor >> layer >> n1 >> n2))
    throw FileFormatError(io::Str() << "Can't parse intra-multilayer link data from line '" << line << "'");
  (m_extractor >> weight) || (weight = 1.0);
}

void Network::parseMultilayerInterLink(const std::string& line, unsigned int& layer1, unsigned int& n, unsigned int& layer2, double& weight)
{
  m_extractor.clear();
  m_extractor.str(line);
  if (!(m_extractor >> layer1 >> n >> layer2))
    throw FileFormatError(io::Str() << "Can't parse inter-multilayer link data from line '" << line << "'");
  (m_extractor >> weight) || (weight = 1.0);
  if (layer1 == layer2)
    throw FileFormatError(io::Str() << "Inter-layer link from line '" << line << "' doesn't go between different layers.");
  //TODO: Same as intra-layer self-link?
}

void Network::printSummary()
{
  Log() << "-------------------------------------\n";
  if (haveMemoryInput()) {
    Log() << "  -> " << numNodes() << " state nodes\n";
    Log() << "  -> " << numPhysicalNodes() << " physical nodes\n";
  } else {
    if (m_bipartiteStartId > 0)
      Log() << "  -> " << numNodes() << " bipartite nodes\n";
    else
      Log() << "  -> " << numNodes() << " nodes\n";
  }
  Log() << "  -> " << numLinks() << " links with total weight " << m_totalLinkWeightAdded << "\n";
  if (m_numLinksIgnoredByWeightThreshold > 0) {
    Log() << "  -> " << m_numLinksIgnoredByWeightThreshold << " links ignored by weight threshold with total weight " << m_totalLinkWeightIgnored << " (" << io::toPrecision(m_totalLinkWeightIgnored / (m_totalLinkWeightIgnored + m_totalLinkWeightAdded) * 100, 1, true) << "%)\n";
  }
}

void Network::addMultilayerLink(unsigned int layer1, unsigned int n1, unsigned int layer2, unsigned int n2, double weight)
{
  if (weight < m_config.weightThreshold) {
    ++m_numLinksIgnoredByWeightThreshold;
    m_totalLinkWeightIgnored += weight;
    return;
  }
  unsigned int stateId1 = addMultilayerNode(layer1, n1);
  unsigned int stateId2 = addMultilayerNode(layer2, n2);

  if (stateId1 == stateId2) {
    // TODO: Handle self-links?
  }

  if (layer1 == layer2) {
    ++m_numIntraLayerLinks;
    m_sumIntraOutWeight[layer1][n1] += weight; // TODO: Not used? Add on target also if undirected (not inter/intra format)?
  } else {
    ++m_numInterLayerLinks;
  }

  addLink(stateId1, stateId2, weight);
}


void Network::generateStateNetworkFromMultilayer()
{
  // As inter-layer links is directed to neighbouring nodes in target layer,
  // the symmetry is broken so we need directed links for inter-layer flow
  m_haveDirectedInput = true;
  if (m_config.isUndirectedFlow()) {
    // TODO: Don't allow undirdir/outdirdir/rawdir?
    // Expand each undirected intra-layer link to two opposite directed links
    Log() << "  -> Expanding undirected links to directed...\n";
    for (auto& layerIt : m_networks) {
      auto& network = layerIt.second;
      network.undirectedToDirected();
    }
  }

  if (!m_interLinks.empty()) {
    generateStateNetworkFromMultilayerWithInterLinks();
  } else {
    generateStateNetworkFromMultilayerWithSimulatedInterLinks();
  }
}

void Network::generateStateNetworkFromMultilayerWithInterLinks()
{
  Log() << "Generating state network from multilayer networks with inter-layer links...\n"
        << std::flush;
  // First add intra-layer links
  for (auto& layerIt : m_networks) {
    unsigned int layer1 = layerIt.first;
    auto& network = layerIt.second;
    for (auto& linkIt : network.nodeLinkMap()) {
      auto& source = linkIt.first;
      const auto& subLinks = linkIt.second;
      for (auto& subIt : subLinks) {
        auto& target = subIt.first;
        double linkWeight = subIt.second.weight;
        addMultilayerLink(layer1, source.physicalId, layer1, target.physicalId, linkWeight);
      }
    }
  }

  Log() << "Connecting layers...\n";
  // Connect layers with inter-layer links spread out in target layer
  for (auto& it : m_interLinks) {
    auto& layerNode = it.first;
    unsigned int layer1 = layerNode.layer;
    unsigned int physId = layerNode.node;
    // Log() << "From layer: " << layer1 << ", phys: " << physId << "\n";
    unsigned int stateId1 = addMultilayerNode(layer1, physId);
    for (auto& it2 : it.second) {
      unsigned int layer2 = it2.first;
      double interWeight = it2.second;
      // Log() << "  -> layer: " << layer2 << ", weight: " << interWeight << "\n";
      auto& targetNetwork = m_networks[layer2];

      // auto& targetLinks = targetNetwork.nodeLinkMap();
      std::map<StateNode, std::map<StateNode, LinkData>>& targetLinks = targetNetwork.nodeLinkMap();
      auto& outlinks = targetLinks[StateNode(physId)];
      if (outlinks.empty()) {
        // Log() << "  Dangling\n";
        continue;
      }
      // unsigned int stateId2 = addMultilayerNode(layer2, physId);
      auto& targetOutWeights = targetNetwork.outWeights();
      double sumIntraOutWeightTargetLayer = targetOutWeights[physId];

      // Log() << "    -> " << outlinks.size() << " outlinks:\n";
      for (auto& outLink : outlinks) {
        auto& targetPhysId = outLink.first.physicalId;
        auto& linkData = outLink.second;
        double intraWeight = linkData.weight;
        unsigned int stateId2i = addMultilayerNode(layer2, targetPhysId);

        // Log() << "      -> " << targetPhysId << "\n";
        double weight = sumIntraOutWeightTargetLayer == 0.0 ? 0.0 : interWeight * intraWeight / sumIntraOutWeightTargetLayer;

        addLink(stateId1, stateId2i, weight);
        ++m_numInterLayerLinks; // TODO: Count all as one?
      }
    }
  }
  if (m_config.isUndirectedFlow()) {
    // For undirected inter-layer links, expand and add in other direction too
    for (auto& it : m_interLinks) {
      auto& layerNode = it.first;
      unsigned int layer2 = layerNode.layer;
      unsigned int physId = layerNode.node;
      // Log() << "From layer: " << layer1 << ", phys: " << physId << "\n";
      // unsigned int stateId1 = addMultilayerNode(layer1, physId);
      auto& targetNetwork = m_networks[layer2];
      std::map<StateNode, std::map<StateNode, LinkData>>& targetLinks = targetNetwork.nodeLinkMap();
      auto& outlinks = targetLinks[StateNode(physId)];
      if (outlinks.empty()) {
        // Log() << "  Dangling\n";
        continue;
      }
      auto& targetOutWeights = targetNetwork.outWeights();
      double sumIntraOutWeightTargetLayer = targetOutWeights[physId];
      for (auto& it2 : it.second) {
        unsigned int layer1 = it2.first;
        double interWeight = it2.second;
        // Log() << "  -> layer: " << layer2 << ", weight: " << interWeight << "\n";
        unsigned int stateId1 = addMultilayerNode(layer1, physId);

        // Log() << "    -> " << outlinks.size() << " outlinks:\n";
        for (auto& outLink : outlinks) {
          auto& targetPhysId = outLink.first.physicalId;
          auto& linkData = outLink.second;
          double intraWeight = linkData.weight;
          unsigned int stateId2i = addMultilayerNode(layer2, targetPhysId);

          // Log() << "      -> " << targetPhysId << "\n";
          double weight = sumIntraOutWeightTargetLayer == 0.0 ? 0.0 : interWeight * intraWeight / sumIntraOutWeightTargetLayer;

          addLink(stateId1, stateId2i, weight);
          ++m_numInterLayerLinks; // TODO: Count all as one?
        }
      }
    }
  }
}

void Network::generateStateNetworkFromMultilayerWithSimulatedInterLinks()
{
  Log() << "Generating state network from multilayer networks with simulated inter-layer links...\n"
        << std::flush;
  double relaxRate = m_config.multilayerRelaxRate;

  int maxRelaxLimit = m_networks.size();
  int relaxLimitSymmetric = m_config.multilayerRelaxLimit < 0 ? maxRelaxLimit : m_config.multilayerRelaxLimit;
  int relaxLimitDown = m_config.multilayerRelaxLimitDown < 0 ? relaxLimitSymmetric : std::min(relaxLimitSymmetric, m_config.multilayerRelaxLimitDown);
  int relaxLimitUp = m_config.multilayerRelaxLimitUp < 0 ? relaxLimitSymmetric : std::min(relaxLimitSymmetric, m_config.multilayerRelaxLimitUp);
  auto haveUpOrDownLimit = m_config.multilayerRelaxLimitDown >= 0 || m_config.multilayerRelaxLimitUp >= 0;

  Log() << "-> " << m_networks.size() << " networks\n";
  Log() << "-> Relax rate: " << relaxRate << "\n";
  if (haveUpOrDownLimit) {
    Log() << "-> Relax limit up: " << relaxLimitUp << (relaxLimitUp == maxRelaxLimit ? " (no limit)\n" : "\n");
    Log() << "-> Relax limit down: " << relaxLimitDown << (relaxLimitDown == maxRelaxLimit ? " (no limit)\n" : "\n");
  } else if (m_config.multilayerRelaxLimit >= 0) {
    Log() << "-> Relax limit: " << m_config.multilayerRelaxLimit << "\n";
  }

  auto withinRelaxLimit = [relaxLimitDown, relaxLimitUp](auto& layer1, auto& layer2) {
    int diff = layer1 - layer2;
    return layer1 >= layer2 ? diff <= relaxLimitDown : -diff <= relaxLimitUp;
  };

  if (m_config.multilayerRelaxByJensenShannonDivergence) {
    Log() << "-> Using Jensen-Shannon Divergence\n";

    for (unsigned int nodeId = 0; nodeId <= m_maxNodeIdInIntraLayerNetworks; ++nodeId) {
      unsigned int layer2from = 0;

      // Calculate Jensen-Shannon similarity between all layers such that layer1 >= layer2,
      // and then use its symmetry for layer2 > layer1
      std::map<unsigned int, std::map<unsigned int, double>> jsRelaxWeights;
      std::map<unsigned int, double> jsTotWeight;

      for (unsigned int layer1 = 0; layer1 < m_networks.size(); ++layer1) {
        unsigned int layer2to = layer1 + 1;
        // Limit possible jumps to close by layers
        if (m_config.multilayerRelaxLimit >= 0) {
          layer2from = ((int)layer1 - m_config.multilayerRelaxLimit) < 0 ? 0 : layer1 - m_config.multilayerRelaxLimit;
        }

        auto& layer1LinkMap = m_networks[layer1].nodeLinkMap();
        auto& layer1OutLinks = layer1LinkMap[StateNode(nodeId)];
        // Skip dangling nodes, because they have no information to calculate similarity
        if (layer1OutLinks.empty())
          continue;

        double sumOutLinkWeightLayer1 = m_networks[layer1].outWeights()[nodeId];


        for (unsigned int layer2 = layer2from; layer2 < layer2to; ++layer2) {
          auto& layer2LinkMap = m_networks[layer2].nodeLinkMap();
          auto& layer2OutLinks = layer2LinkMap[StateNode(nodeId)];
          if (layer2OutLinks.empty())
            continue;

          double sumOutLinkWeightLayer2 = m_networks[layer2].outWeights()[nodeId];

          bool intersect;
          double div = calculateJensenShannonDivergence(intersect, layer1OutLinks, sumOutLinkWeightLayer1, layer2OutLinks, sumOutLinkWeightLayer2);
          double jsWeight = 1.0 - div;
          if (intersect && (jsWeight >= m_config.multilayerJSRelaxLimit)) {
            jsTotWeight[layer1] += jsWeight;
            jsRelaxWeights[layer1][layer2] = jsWeight;
            if (layer1 != layer2) {
              jsTotWeight[layer2] += jsWeight;
              jsRelaxWeights[layer2][layer1] = jsWeight;
            }
          }
        }
      }

      // Second loop over all pairs of layers
      unsigned int layer2to = m_networks.size();

      for (unsigned int layer1 = 0; layer1 < m_networks.size(); ++layer1) {
        // Limit possible jumps to close by layers
        if (m_config.multilayerRelaxLimit >= 0) {
          layer2from = ((int)layer1 - m_config.multilayerRelaxLimit) < 0 ? 0 : layer1 - m_config.multilayerRelaxLimit;
          layer2to = (layer1 + m_config.multilayerRelaxLimit) > m_networks.size() ? m_networks.size() : layer1 + m_config.multilayerRelaxLimit;
        }

        double sumOutLinkWeightLayer1 = m_networks[layer1].outWeights()[nodeId];

        std::map<unsigned int, std::map<unsigned int, double>>::iterator jsRelaxWeightsLayer1It = jsRelaxWeights.find(layer1);
        std::map<unsigned int, double>::iterator jsTotWeightIt = jsTotWeight.find(layer1);

        // Create inter-links to the intra-connected nodes in other layers
        for (unsigned int layer2 = layer2from; layer2 < layer2to; ++layer2) {
          if (jsRelaxWeightsLayer1It != jsRelaxWeights.end()) {
            std::map<unsigned int, double>::iterator jsRelaxWeightsIt = jsRelaxWeightsLayer1It->second.find(layer2);
            if (jsRelaxWeightsIt != jsRelaxWeightsLayer1It->second.end()) {
              bool isIntra = layer2 == layer1;

              // Create inter-links to the outgoing nodes in the target layer
              double linkWeightNormalizationFactor;
              if (isIntra) {
                linkWeightNormalizationFactor = 1;
              } else {
                linkWeightNormalizationFactor = jsRelaxWeightsIt->second * relaxRate / (1.0 - relaxRate) * sumOutLinkWeightLayer1 / jsTotWeightIt->second;
              }

              // double stateNodeWeightNormalizationFactor = 1.0;
              // createIntraLinksToNeighbouringNodesInTargetLayer(layer1, nodeId, layer2, m_networks[layer2].nodeLinkMap(), linkWeightNormalizationFactor, stateNodeWeightNormalizationFactor);

              auto& targetLinks = m_networks[layer2].nodeLinkMap();
              auto& targetOutlinks = targetLinks[StateNode(nodeId)];
              if (targetOutlinks.empty()) {
                // Log() << "   -> Dangling\n";
                continue;
              }
              for (auto& outLink : targetOutlinks) {
                auto& n2 = outLink.first.physicalId;
                auto& linkData = outLink.second;
                double intraWeight = linkData.weight;
                // Add intra link weight as teleport weight to source node
                //asdf
                unsigned int stateId1 = addMultilayerNode(layer1, nodeId, intraWeight);
                unsigned int stateId2i = addMultilayerNode(layer2, n2, 0.0);

                // Log() << "      -> " << n2 << "\n";
                double weight = intraWeight == 0.0 ? 0.0 : linkWeightNormalizationFactor * intraWeight;
                addLink(stateId1, stateId2i, weight);
                ++m_numInterLayerLinks;
              }
            }
          }
        }
      }
    }

    return;
  }


  for (auto& it1 : m_networks) {
    auto layer1 = it1.first;
    auto& network1 = it1.second;
    // Log() << "Layer " << layer1 << "\n";

    for (auto& n1It : network1.nodes()) {
      auto& n1 = n1It.first;
      // Log() << "   Node " << n1 << "\n";
      unsigned int stateId1 = addMultilayerNode(layer1, n1);

      double sumOutLinkWeightLayer1 = network1.outWeights()[n1];
      double sumOutWeightAllLayers = 0.0;

      for (auto& it2 : m_networks) {
        auto layer2 = it2.first;
        if (!withinRelaxLimit(layer1, layer2)) {
          continue;
        }
        auto& network2 = it2.second;
        sumOutWeightAllLayers += network2.outWeights()[n1];
      }
      // Log() << "   -> sumOutWeightAllLayers: " << sumOutWeightAllLayers << "\n";

      if (sumOutWeightAllLayers <= 0) {
        continue;
      }
      for (auto& it2 : m_networks) {
        auto layer2 = it2.first;
        if (!withinRelaxLimit(layer1, layer2)) {
          continue;
        }
        auto& network2 = it2.second;
        // Log() << "     Layer " << layer2 << "\n";
        bool isIntra = layer2 == layer1;

        double linkWeightNormalizationFactor = relaxRate / sumOutWeightAllLayers;
        if (isIntra) {
          linkWeightNormalizationFactor += (1.0 - relaxRate) / sumOutLinkWeightLayer1;
        }
        // double stateNodeWeightNormalizationFactor = 1.0;

        // createIntraLinksToNeighbouringNodesInTargetLayer(layer1, nodeIndex, layer2, m_networks[layer2].linkMap(), linkWeightNormalizationFactor, stateNodeWeightNormalizationFactor);
        auto& targetLinks = network2.nodeLinkMap();
        auto& targetOutlinks = targetLinks[StateNode(n1)];
        if (targetOutlinks.empty()) {
          // Log() << "   -> Dangling\n";
          continue;
        }
        for (auto& outLink : targetOutlinks) {
          auto& n2 = outLink.first.physicalId;
          auto& linkData = outLink.second;
          double intraWeight = linkData.weight;
          unsigned int stateId2i = addMultilayerNode(layer2, n2);

          // Log() << "      -> " << n2 << "\n";
          double weight = intraWeight == 0.0 ? 0.0 : linkWeightNormalizationFactor * intraWeight;
          addLink(stateId1, stateId2i, weight);
          ++m_numInterLayerLinks; // TODO: Count all as one?
        }
      }
    }
  }
}

double Network::calculateJensenShannonDivergence(bool& intersect, const OutLinkMap& layer1OutLinks, double sumOutLinkWeightLayer1, const OutLinkMap& layer2OutLinks, double sumOutLinkWeightLayer2)
{
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

  auto layer1OutLinkIt = layer1OutLinks.begin();
  auto layer2OutLinkIt = layer2OutLinks.begin();
  auto layer1OutLinkItEnd = layer1OutLinks.end();
  auto layer2OutLinkItEnd = layer2OutLinks.end();
  while (layer1OutLinkIt != layer1OutLinkItEnd && layer2OutLinkIt != layer2OutLinkItEnd) {
    auto diff = layer1OutLinkIt->first.id - layer2OutLinkIt->first.id;
    if (diff < 0) {
      // If the first state node has a link that the second has not
      double p1 = layer1OutLinkIt->second.weight / ow1;
      h1 -= p1 * log2(p1);
      double p12 = pi1 * layer1OutLinkIt->second.weight / ow1;
      h12 -= p12 * log2(p12);
      layer1OutLinkIt++;
    } else if (diff > 0) {
      // If the second state node has a link that the second has not
      double p2 = layer2OutLinkIt->second.weight / ow2;
      h2 -= p2 * log2(p2);
      double p12 = pi2 * layer2OutLinkIt->second.weight / ow2;
      h12 -= p12 * log2(p12);
      layer2OutLinkIt++;
    } else { // If both state nodes have the link
      intersect = true;
      double p1 = layer1OutLinkIt->second.weight / ow1;
      h1 -= p1 * log2(p1);
      double p2 = layer2OutLinkIt->second.weight / ow2;
      h2 -= p2 * log2(p2);
      double p12 = pi1 * layer1OutLinkIt->second.weight / ow1 + pi2 * layer2OutLinkIt->second.weight / ow2;
      h12 -= p12 * log2(p12);
      layer1OutLinkIt++;
      layer2OutLinkIt++;
    }
  }

  while (layer1OutLinkIt != layer1OutLinkItEnd) {
    // If the first state node has a link that the second has not
    double p1 = layer1OutLinkIt->second.weight / ow1;
    h1 -= p1 * log2(p1);
    double p12 = pi1 * layer1OutLinkIt->second.weight / ow1;
    h12 -= p12 * log2(p12);
    layer1OutLinkIt++;
  }

  while (layer2OutLinkIt != layer2OutLinkItEnd) {
    // If the second state node has a link that the second has not
    double p2 = layer2OutLinkIt->second.weight / ow2;
    h2 -= p2 * log2(p2);
    double p12 = pi2 * layer2OutLinkIt->second.weight / ow2;
    h12 -= p12 * log2(p12);
    layer2OutLinkIt++;
  }

  double div = (pi1 + pi2) * h12 - pi1 * h1 - pi2 * h2;

  // Fix precision problems
  if (div < 0.0)
    div = 0.0;
  else if (div > 1.0)
    div = 1.0;

  // Log() << "\n" << div << " " << (pi1+pi2) << " " << h12 << " " << pi1 << " " << h1 << " " << pi2 << " " << h2;

  return div;
}

void Network::simulateInterLayerLinks()
{
}


void Network::addMultilayerIntraLink(unsigned int layer, unsigned int n1, unsigned int n2, double weight)
{
  bool added = m_networks[layer].addLink(n1, n2, weight);
  if (added) {
    ++m_numIntraLayerLinks;
    m_maxNodeIdInIntraLayerNetworks = std::max(m_maxNodeIdInIntraLayerNetworks, std::max(n1, n2));
  }
}


void Network::addMultilayerInterLink(unsigned int layer1, unsigned int n, unsigned int layer2, double interWeight)
{
  if (layer1 == layer2) {
    throw InputDomainError(io::Str() << "Inter-layer link (layer1, node, layer2): " << layer1 << ", " << n << ", " << layer2 << " must have layer1 != layer2");
  }

  // m_interLinks[LayerNode(layer1, n)][layer2] += interWeight;

  auto& interLinks = m_interLinks[LayerNode(layer1, n)];
  auto it = interLinks.find(layer2);

  if (it == interLinks.end()) {
    ++m_numInterLayerLinks;
  }
  interLinks[layer2] += interWeight;


  // unsigned int stateId1 = addMultilayerNode(layer1, n);
  // unsigned int stateId2 = addMultilayerNode(layer2, n);

  // auto& outLinks = m_nodeLinkMap[stateId2]; // std::map<StateNode, LinkData>
  // if (outLinks.empty()) {
  // 	// TODO: Do nothing?
  // 	return;
  // }

  // for (auto& outLink : outLinks) {
  // 	auto& targetNode = outLink.first;
  // 	auto& linkData = outLink.second;
  // 	double intraWeight = linkData.weight;
  // 	double weight = interWeight * intraWeight / m_sumIntraOutWeight[layer2][n];
  // 	addLink(stateId1, targetNode.id, weight);
  // 	++m_numInterLayerLinks; // TODO: Count all as one?
  // }
}

unsigned int Network::addMultilayerNode(unsigned int layerId, unsigned int physicalId, double weight)
{
  // auto layerNode = LayerNode(layerId, physicalId);
  // auto it = m_layerNodeToStateId.find(layerNode);
  // if (it != m_layerNodeToStateId.end()) {
  // 	return it->second;
  // }
  // auto ret = addStateNodeWithAutogeneratedId(physicalId);
  // auto& stateNode = ret.first->second;
  // m_layerNodeToStateId[layerNode] = stateNode.id;
  // m_layers.insert(layerId);
  // return stateNode.id;

  // Create state node if not already exist, return state node id
  auto& layerIt = m_layerNodeToStateId[layerId];
  auto it = layerIt.find(physicalId);

  if (it != layerIt.end()) {
    return it->second;
  }
  auto ret = addStateNodeWithAutogeneratedId(physicalId);
  auto& stateNode = ret.first->second;
  stateNode.layerId = layerId;
  stateNode.weight = weight;
  m_layerNodeToStateId[layerId][physicalId] = stateNode.id;
  m_layers.insert(layerId);
  return stateNode.id;
}


void Network::addMetaData(unsigned int nodeId, int meta)
{
  std::vector<int> metaData(1, meta);
  addMetaData(nodeId, metaData);
}

void Network::addMetaData(unsigned int nodeId, const std::vector<int>& metaData)
{
  m_metaData[nodeId] = metaData;
  if (m_numMetaDataColumns == 0) {
    m_numMetaDataColumns = metaData.size();
  } else if (metaData.size() != m_numMetaDataColumns) {
    throw FileFormatError(io::Str() << "Must have same number of dimensions in meta data, error trying to add meta data '" << io::stringify(metaData, ",") << "' on node " << nodeId << ".");
  }
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
// 		Log() << "  ==> " << getParsingResultSummary() << '\n';
// 		return;
// 	}

// 	if (!dataModified)
// 		Log() << "  ==> " << getParsingResultSummary();
// 	else {
// 		Log() << "  --> Found " << m_numNodesFound << io::toPlural(" node", m_numNodesFound);
// 		Log() << "  and " << m_numLinksFound << io::toPlural(" link", m_numLinksFound) << ".";
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
// 		Log() << "  with total weight " << m_totalSelfLinkWeight << " (" << (m_totalSelfLinkWeight / m_totalLinkWeight * 100) << "% of the total link weight).";
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

} // namespace infomap
