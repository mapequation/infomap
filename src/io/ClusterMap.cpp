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
#include "../utils/Console.h"
#include "../utils/FileURI.h"
#include "../utils/format.h"
#include <sstream>
#include <vector>

namespace infomap {

ColumnSchema ClusterMap::parseHeaderSchema(const std::string& headerLine)
{
  std::istringstream hs(headerLine);
  std::string tok;
  std::vector<std::string> tokens;
  while (hs >> tok) {
    if (tok == "#")
      continue; // skip a standalone comment marker
    tokens.push_back(tok);
  }
  // Match the exact declared schema (the new physical forms are an explicit
  // opt-in, so a permissive "contains" match could misclassify other headers).
  // .clu physical multilayer: `# node_id layer_id module`
  static const std::vector<std::string> physicalCluHeader = { "node_id", "layer_id", "module" };
  if (tokens == physicalCluHeader)
    return ColumnSchema::multilayerPhysicalClu;
  // .tree physical multilayer: `# path flow name node_id layer_id`
  static const std::vector<std::string> physicalTreeHeader = { "path", "flow", "name", "node_id", "layer_id" };
  if (tokens == physicalTreeHeader)
    return ColumnSchema::multilayerPhysicalTree;
  return ColumnSchema::legacy;
}

void ClusterMap::readClusterData(const std::string& filename, bool includeFlow, const std::map<unsigned int, std::map<unsigned int, unsigned int>>* layerNodeToStateId)
{
  m_clusterIds.clear();
  m_flowData.clear();
  m_treePaths.clear();
  m_extension.clear();
  m_schema = ColumnSchema::legacy;
  m_isHigherOrder = false;
  m_hasTreeLeafIdType = false;
  m_treeLeafIdType = TreeLeafIdType::physical;

  FileURI file(filename);
  m_extension = file.getExtension();
  if (m_extension == "tree" || m_extension == "ftree") {
    return readTree(filename, includeFlow, layerNodeToStateId);
  }
  if (m_extension == "clu") {
    return readClu(filename, includeFlow, layerNodeToStateId);
  }
  throw std::runtime_error(fmt::format(FMT_STRING("Input cluster data from file '{}' is of unknown extension '{}'. Must be 'clu', 'tree' or 'ftree'."), filename, m_extension));
}

/**
 * Sample from .tree file (physical level)
# Codelength = 3.46227314 bits.
# path flow name node_id
1:1:1 0.0384615 "1" 1
1:1:2 0.025641 "2" 2
 *
 * Sample from .tree file (state level)
# path flow name state_id node_id
1:1 0.166667 "i" 1 1
2:1 0.166667 "i" 4 1
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
    if (line.empty())
      continue;
    if (line[0] == '#') {
      // A recognized column header opts into the physical-multilayer schema;
      // otherwise the header is decoration and the row column count is
      // authoritative (legacy behavior unchanged).
      auto schema = parseHeaderSchema(line);
      if (schema != ColumnSchema::legacy)
        m_schema = schema;
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
      throw std::runtime_error(fmt::format(FMT_STRING("Couldn't parse tree path from line '{}'"), line));
    if (!(lineStream >> flow))
      throw std::runtime_error(fmt::format(FMT_STRING("Couldn't parse node flow from line '{}'"), line));
    if (!getline(lineStream, name, '"'))
      throw std::runtime_error(fmt::format(FMT_STRING("Can't parse node name from line {} ('{}')."), lineNr, line));
    if (!getline(lineStream, name, '"'))
      throw std::runtime_error(fmt::format(FMT_STRING("Can't parse node name from line {} ('{}')."), lineNr, line));
    if (!(lineStream >> parsedId))
      throw std::runtime_error(fmt::format(FMT_STRING("Couldn't parse node id from line '{}'"), line));

    unsigned int stateId = parsedId;

    if (m_schema == ColumnSchema::multilayerPhysicalTree) {
      // `# path flow name node_id layer_id`: parsedId was actually the node id;
      // resolve (node, layer) -> state id. Header-selected, so it cannot be
      // confused with the equally-wide state-higher-order form.
      if (!isMultilayer)
        throw std::runtime_error("Physical multilayer cluster-data (node_id layer_id) requires a multilayer network.");
      nodeId = parsedId;
      if (!(lineStream >> layerId))
        throw std::runtime_error(fmt::format(FMT_STRING("Couldn't parse layer id from line '{}'"), line));
      if (!m_hasTreeLeafIdType) {
        m_treeLeafIdType = TreeLeafIdType::state;
        m_hasTreeLeafIdType = true;
      } else if (m_treeLeafIdType != TreeLeafIdType::state) {
        throw std::runtime_error(fmt::format(FMT_STRING("Mixed state and physical tree ids are not supported in line '{}'."), line));
      }
      m_isHigherOrder = true;
      auto it = layerNodeToStateId->find(layerId);
      if (it == layerNodeToStateId->end())
        continue;
      auto nodeIdToStateId = it->second.find(nodeId);
      if (nodeIdToStateId == it->second.end())
        continue;
      stateId = nodeIdToStateId->second;
    } else {
      const auto hasExplicitNodeId = static_cast<bool>(lineStream >> nodeId);
      const auto inferredLeafIdType = hasExplicitNodeId ? TreeLeafIdType::state : TreeLeafIdType::physical;

      if (!m_hasTreeLeafIdType) {
        m_treeLeafIdType = inferredLeafIdType;
        m_hasTreeLeafIdType = true;
      } else if (inferredLeafIdType != m_treeLeafIdType) {
        // Earlier rows had a different column count. A file that mixes
        // physical-id (4 columns) and state-id (5+ columns) rows cannot be
        // parsed safely.
        throw std::runtime_error(fmt::format(FMT_STRING("Mixed state and physical tree ids are not supported in line '{}'."), line));
      }

      if (m_treeLeafIdType == TreeLeafIdType::state) {
        if (hasExplicitNodeId) {
          m_isHigherOrder = true;
        } else if (m_isHigherOrder) {
          throw std::runtime_error(fmt::format(FMT_STRING("Missing node id from line '{}'."), line));
        }
        if (isMultilayer) {
          if (!hasExplicitNodeId)
            throw std::runtime_error(fmt::format(FMT_STRING("Couldn't parse node key from line '{}'"), line));
          if (!(lineStream >> layerId))
            throw std::runtime_error(fmt::format(FMT_STRING("Couldn't parse layer id from line '{}'"), line));
        }
      }

      if (isMultilayer && m_treeLeafIdType == TreeLeafIdType::state) {
        // Re-map state id from layer/node ids in case the multilayer state ids differ
        bool multilayerNodeFound = false;
        auto it = layerNodeToStateId->find(layerId);
        if (it != layerNodeToStateId->end()) {
          auto nodeIdToStateId = it->second.find(nodeId);
          if (nodeIdToStateId != it->second.end()) {
            stateId = nodeIdToStateId->second;
            multilayerNodeFound = true;
          }
        }
        if (!multilayerNodeFound) {
          // Skip rows whose layer/node combination is not present in the network
          continue;
        }
      }
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

  Console::detail(1, "reading initial partition from '{}'", filename);
  SafeInFile input(filename);
  std::string line;
  std::istringstream lineStream;
  std::map<unsigned int, unsigned int> clusterData;

  while (!std::getline(input, line).fail()) {
    if (line.empty() || line[0] == '*')
      continue;
    if (line[0] == '#') {
      // A recognized column header selects the schema; legacy comment lines
      // (version, command, "# state_id module ...") leave it unchanged.
      auto schema = parseHeaderSchema(line);
      if (schema != ColumnSchema::legacy)
        m_schema = schema;
      continue;
    }

    lineStream.clear();
    lineStream.str(line);

    unsigned int stateId;
    unsigned int nodeId;
    unsigned int moduleId;
    unsigned int layerId;

    // Physical multilayer form (# node_id layer_id module): resolve to state id.
    if (m_schema == ColumnSchema::multilayerPhysicalClu) {
      if (!isMultilayer)
        throw std::runtime_error("Physical multilayer cluster-data (node_id layer_id module) requires a multilayer network.");
      if (!(lineStream >> nodeId >> layerId >> moduleId))
        throw std::runtime_error(fmt::format(FMT_STRING("Couldn't parse 'node_id layer_id module' from line '{}'"), line));
      auto it = layerNodeToStateId->find(layerId);
      if (it == layerNodeToStateId->end())
        continue;
      auto nodeIdToStateId = it->second.find(nodeId);
      if (nodeIdToStateId == it->second.end())
        continue;
      m_clusterIds[nodeIdToStateId->second] = moduleId;
      continue;
    }

    // # state_id module flow node_id layer_id
    if (!(lineStream >> stateId >> moduleId))
      throw std::runtime_error(fmt::format(FMT_STRING("Couldn't parse node key and cluster id from line '{}'"), line));

    auto flow = 0.0;
    if (lineStream >> flow) {
      if (includeFlow)
        m_flowData[stateId] = flow;
    }

    auto multilayerNodeFound = false;
    if (isMultilayer) {
      if (!(lineStream >> nodeId))
        throw std::runtime_error(fmt::format(FMT_STRING("Couldn't parse node key from line '{}'"), line));

      if (!(lineStream >> layerId))
        throw std::runtime_error(fmt::format(FMT_STRING("Couldn't parse layer id from line '{}'"), line));

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
