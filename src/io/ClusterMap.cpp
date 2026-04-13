/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "ClusterMap.h"
#include "SafeFile.h"
#include "../utils/Log.h"
#include "../utils/FileURI.h"
#include <sstream>

namespace infomap {

namespace {

bool tryParseTreeLeafIdTypeFromHeader(const std::string& line, TreeLeafIdType& idType)
{
  std::istringstream headerStream(line);
  std::string hash;
  std::string path;
  std::string flowField;
  std::string name;
  std::string idField;

  if (!(headerStream >> hash >> path >> flowField >> name >> idField)) {
    return false;
  }

  if (hash != "#" || path != "path" || name != "name") {
    return false;
  }

  if (idField == "state_id") {
    idType = TreeLeafIdType::state;
    return true;
  }

  if (idField == "node_id" || idField == "physical_id" || idField == "physicalId") {
    idType = TreeLeafIdType::physical;
    return true;
  }

  return false;
}

} // namespace

void ClusterMap::readClusterData(const std::string& filename, bool includeFlow, const std::map<unsigned int, std::map<unsigned int, unsigned int>>* layerNodeToStateId)
{
  m_clusterIds.clear();
  m_flowData.clear();
  m_treePaths.clear();
  m_extension.clear();
  m_isHigherOrder = false;
  m_hasTreeLeafIdType = false;
  m_treeLeafIdTypeFromHeader = false;
  m_treeLeafIdType = TreeLeafIdType::physical;

  FileURI file(filename);
  m_extension = file.getExtension();
  if (m_extension == "tree" || m_extension == "ftree") {
    return readTree(filename, includeFlow, layerNodeToStateId);
  }
  if (m_extension == "clu") {
    return readClu(filename, includeFlow, layerNodeToStateId);
  }
  throw std::runtime_error(io::Str() << "Input cluster data from file '" << filename << "' is of unknown extension '" << m_extension << "'. Must be 'clu', 'tree' or 'ftree'.");
}

/**
 * Sample from .tree file
# Codelength = 3.46227314 bits.
# path flow name physicalId
1:1:1 0.0384615 "1" 1
1:1:2 0.025641 "2" 2
1:1:3 0.0384615 "3" 3
1:2:1 0.0384615 "4" 4
 */
void ClusterMap::readTree(const std::string& filename, bool includeFlow, const std::map<unsigned int, std::map<unsigned int, unsigned int>>* layerNodeToStateId)
{
  bool isMultilayer = layerNodeToStateId != nullptr;

  SafeInFile input(filename);
  std::string line;
  std::istringstream lineStream;
  std::istringstream pathStream;

  unsigned int lineNr = 0;

  while (!std::getline(input, line).fail()) {
    ++lineNr;
    if (line.length() == 0)
      continue;
    if (line[0] == '#') {
      TreeLeafIdType idType;
      if (tryParseTreeLeafIdTypeFromHeader(line, idType)) {
        m_treeLeafIdType = idType;
        m_hasTreeLeafIdType = true;
        m_treeLeafIdTypeFromHeader = true;
      }
      continue;
    }
    if (line[0] == '*') {
      break;
    }

    lineStream.clear();
    lineStream.str(line);

    std::string pathString;
    double flow;
    std::string name;
    unsigned int parsedId;
    unsigned int nodeId = 0;
    unsigned int layerId = 0;
    if (!(lineStream >> pathString))
      throw std::runtime_error(io::Str() << "Couldn't parse tree path from line '" << line << "'");
    if (!(lineStream >> flow))
      throw std::runtime_error(io::Str() << "Couldn't parse node flow from line '" << line << "'");
    // Get the name by extracting the rest of the stream until the first quotation mark and then the last.
    if (!getline(lineStream, name, '"'))
      throw std::runtime_error(io::Str() << "Can't parse node name from line " << lineNr << " ('" << line << "').");
    if (!getline(lineStream, name, '"'))
      throw std::runtime_error(io::Str() << "Can't parse node name from line " << lineNr << " ('" << line << "').");
    if (!(lineStream >> parsedId))
      throw std::runtime_error(io::Str() << "Couldn't parse node id from line '" << line << "'");

    const auto hasExplicitNodeId = static_cast<bool>(lineStream >> nodeId);
    const auto inferredLeafIdType = hasExplicitNodeId ? TreeLeafIdType::state : TreeLeafIdType::physical;

    if (!m_hasTreeLeafIdType) {
      m_treeLeafIdType = inferredLeafIdType;
      m_hasTreeLeafIdType = true;
    } else if (!m_treeLeafIdTypeFromHeader && inferredLeafIdType != m_treeLeafIdType) {
      throw std::runtime_error(io::Str() << "Mixed state and physical tree ids are not supported in line '" << line << "'.");
    }

    if (m_treeLeafIdType == TreeLeafIdType::state) {
      if (hasExplicitNodeId) {
        m_isHigherOrder = true;
      } else if (m_isHigherOrder) {
        throw std::runtime_error(io::Str() << "Missing node id from line '" << line << "'.");
      }
      if (isMultilayer) {
        if (!hasExplicitNodeId)
          throw std::runtime_error(io::Str() << "Couldn't parse node key from line '" << line << "'");
        if (!(lineStream >> layerId))
          throw std::runtime_error(io::Str() << "Couldn't parse layer id from line '" << line << "'");
      }
    }

    bool multilayerNodeFound = false;
    unsigned int stateId = parsedId;

    if (isMultilayer && m_treeLeafIdType == TreeLeafIdType::state) {
      // get new state id from map
      auto it = layerNodeToStateId->find(layerId);

      if (it != layerNodeToStateId->end()) {
        auto nodeIdToStateId = it->second.find(nodeId);
        if (nodeIdToStateId != it->second.end()) {
          stateId = nodeIdToStateId->second;
          multilayerNodeFound = true;
        }
      }
    }

    if (isMultilayer && m_treeLeafIdType == TreeLeafIdType::state && !multilayerNodeFound) {
      continue;
    }

    pathStream.clear();
    pathStream.str(pathString);
    unsigned int childNumber;

    Path path;
    while (pathStream >> childNumber) {
      pathStream.get(); // Extract the delimiting character also
      if (childNumber == 0)
        throw std::runtime_error("There is a '0' in the tree path, lowest allowed integer is 1.");
      path.push_back(childNumber); // Keep 1-based indexing in path
    }

    m_treePaths.push_back({ stateId, path, m_treeLeafIdType });

    if (includeFlow)
      m_flowData[stateId] = flow;
  }
}

void ClusterMap::readClu(const std::string& filename, bool includeFlow, const std::map<unsigned int, std::map<unsigned int, unsigned int>>* layerNodeToStateId)
{
  auto isMultilayer = layerNodeToStateId != nullptr;

  Log() << "Read initial partition from '" << filename << "'... " << std::flush;
  SafeInFile input(filename);
  std::string line;
  std::istringstream lineStream;
  std::map<unsigned int, unsigned int> clusterData;

  while (!std::getline(input, line).fail()) {
    if (line.length() == 0 || line[0] == '#' || line[0] == '*')
      continue;

    lineStream.clear();
    lineStream.str(line);
    // # state_id module flow node_id layer_id

    unsigned int stateId;
    unsigned int nodeId;
    unsigned int moduleId;
    unsigned int layerId;

    if (!(lineStream >> stateId >> moduleId))
      throw std::runtime_error(io::Str() << "Couldn't parse node key and cluster id from line '" << line << "'");

    auto flow = 0.0;
    if (lineStream >> flow) {
      if (includeFlow)
        m_flowData[stateId] = flow;
    }

    auto multilayerNodeFound = false;
    if (isMultilayer) {
      if (!(lineStream >> nodeId))
        throw std::runtime_error(io::Str() << "Couldn't parse node key from line '" << line << "'");

      if (!(lineStream >> layerId))
        throw std::runtime_error(io::Str() << "Couldn't parse layer id from line '" << line << "'");

      // get new state id from map
      auto it = layerNodeToStateId->find(layerId);

      if (it != layerNodeToStateId->end()) {
        auto nodeIdToStateId = it->second.find(nodeId);
        if (nodeIdToStateId != it->second.end()) {
          stateId = nodeIdToStateId->second;
          multilayerNodeFound = true;
        }
      }
    }

    if (isMultilayer && !multilayerNodeFound) {
      continue;
    }

    m_clusterIds[stateId] = moduleId;
  }
}

} // namespace infomap
