/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include <nlohmann/json.hpp>

#include "Output.h"
#include "OutputView.h"
#include "../core/InfomapBase.h"
#include "../core/StateNetwork.h"
#include "../io/SafeFile.h"

#include <stdexcept>
#include <utility>

namespace infomap {

namespace {

  using Json = nlohmann::ordered_json;

  Json modulePathJson(const std::string& path)
  {
    Json json = Json::array();
    if (path.empty()) {
      json.push_back(0);
      return json;
    }

    unsigned int value = 0;
    bool hasDigit = false;
    for (const auto ch : path) {
      if (ch >= '0' && ch <= '9') {
        value = value * 10 + static_cast<unsigned int>(ch - '0');
        hasDigit = true;
      } else if (ch == ',' && hasDigit) {
        json.push_back(value);
        value = 0;
        hasDigit = false;
      } else {
        throw std::logic_error("Invalid module path: " + path);
      }
    }
    if (!hasDigit) {
      throw std::logic_error("Invalid module path: " + path);
    }
    json.push_back(value);
    return json;
  }

  void writeJsonObjectPrefix(std::ostream& outStream, const Json& json)
  {
    const auto dumped = json.dump();
    outStream << dumped.substr(0, dumped.size() - 1);
  }

} // namespace

std::string getOutputFilename(const InfomapBase& im, const std::string& filename, const std::string& ext, bool states)
{
  if (!filename.empty()) {
    return filename;
  }

  auto defaultFilename = im.outDirectory + im.outName;

  if (im.haveMemory() && states) {
    defaultFilename += "_states";
  }

  return defaultFilename + ext;
}

std::string getOutputFileHeader(const InfomapBase& im, const StateNetwork& network, bool states)
{
  std::string bipartiteInfo = io::Str() << "\n# bipartite start id " << network.bipartiteStartId();
  return io::Str() << "# v" << INFOMAP_VERSION << "\n"
                   << "# ./Infomap " << im.parsedString << "\n"
                   << "# started at " << im.getStartDate() << "\n"
                   << "# completed in " << im.getElapsedTime().getElapsedTimeInSec() << " s\n"
                   << "# partitioned into " << im.maxTreeDepth() << " levels with " << im.numTopModules() << " top modules\n"
                   << "# codelength " << im.codelength() << " bits\n"
                   << "# relative codelength savings " << im.getRelativeCodelengthSavings() * 100 << "%\n"
                   << "# flow model " << flowModelToString(im.flowModel)
                   << (im.haveMemory() ? "\n# higher order" : "")
                   << (im.haveMemory() ? states ? "\n# state level" : "\n# physical level" : "")
                   << (network.isBipartite() ? bipartiteInfo : "");
}

std::string writeClu(InfomapBase& im, const StateNetwork& network, const std::string& filename, bool states, int moduleIndexLevel)
{
  auto outputFilename = getOutputFilename(im, filename, ".clu", states);
  SafeOutFile outFile { outputFilename, std::ios_base::out, im.overwriteOutput() };
  OutputView view(im, network, states);

  outFile << std::setprecision(9);
  outFile << getOutputFileHeader(im, network, states) << "\n";
  outFile << "# module level " << moduleIndexLevel << "\n";
  outFile << std::resetiosflags(std::ios::floatfield) << std::setprecision(6);

  if (states) {
    outFile << "# " << view.nodeIdHeaderName() << " module flow node_id";
    if (view.isMultilayer())
      outFile << " layer_id";
    outFile << '\n';
  } else {
    outFile << "# " << view.nodeIdHeaderName() << " module flow\n";
  }

  view.forEachLeaf(moduleIndexLevel, OutputLeafPolicy::HideBipartite, [&](const OutputLeafRow& row) {
    if (states) {
      outFile << row.stateId << " " << row.moduleId << " " << row.flow << " " << row.physicalId;
      if (view.isMultilayer())
        outFile << " " << row.layerId;
      outFile << "\n";
    } else {
      outFile << row.physicalId << " " << row.moduleId << " " << row.flow << "\n";
    }
  });
  outFile.commit();
  return outputFilename;
}

void writeTree(InfomapBase& im, const StateNetwork& network, std::ostream& outStream, bool states)
{
  auto oldPrecision = outStream.precision();
  OutputView view(im, network, states);
  outStream << std::setprecision(9);
  outStream << getOutputFileHeader(im, network, states) << "\n";
  outStream << std::setprecision(6);

  if (states) {
    outStream << "# path flow name " << view.nodeIdHeaderName() << " node_id";
    if (view.isMultilayer())
      outStream << " layer_id";
    outStream << '\n';
  } else {
    outStream << "# path flow name " << view.nodeIdHeaderName() << "\n";
  }

  view.forEachLeaf(1, OutputLeafPolicy::HideBipartiteUnlessFlowTree, [&](const OutputLeafRow& row) {
    outStream << io::stringify(row.path, ":") << " " << row.flow << " \"" << row.name << "\" ";

    if (states) {
      outStream << row.stateId << " " << row.physicalId;
      if (view.isMultilayer())
        outStream << " " << row.layerId;
      outStream << '\n';
    } else {
      outStream << row.physicalId << '\n';
    }
  });

  outStream << std::setprecision(oldPrecision);
}

void writeTreeLinks(InfomapBase& im, std::ostream& outStream, bool states)
{
  auto oldPrecision = outStream.precision();
  outStream << std::setprecision(6);

  OutputView view(im, im.network(), states);

  outStream << "*Links " << (im.isUndirectedFlow() ? "undirected" : "directed") << "\n";
  outStream << "#*Links path enterFlow exitFlow numEdges numChildren\n";

  view.forEachModule([&](const OutputModuleRow& module) {
    outStream << "*Links " << module.linkPathLabel << " " << module.enterFlow << " " << module.exitFlow << " " << module.links.size() << " " << module.numChildren << "\n";

    for (auto itLink : module.links) {
      unsigned int sourceId = itLink.first.first;
      unsigned int targetId = itLink.first.second;
      double flow = itLink.second;
      outStream << sourceId << " " << targetId << " " << flow << "\n";
    }
  });

  outStream << std::setprecision(oldPrecision);
}

void writeNewickTree(InfomapBase& im, std::ostream& outStream, bool states)
{
  auto oldPrecision = outStream.precision();
  OutputView view(im, im.network(), states);
  outStream << std::setprecision(6);

  auto isRoot = true;
  unsigned int lastDepth = 0;
  std::vector<double> flowStack;

  auto writeNewickNode = [&](const OutputTreeRow& row) {
    const auto& node = row.node;
    const auto depth = row.depth;
    if (depth > lastDepth || isRoot) {
      outStream << "(";
      flowStack.push_back(row.flow);
      if (node.isLeaf())
        outStream << view.leafId(row) << ":" << row.flow;
    } else if (depth == lastDepth) {
      outStream << ",";
      flowStack[flowStack.size() - 1] = row.flow;
      if (node.isLeaf()) {
        outStream << view.leafId(row) << ":" << row.flow;
      }
    } else {
      // depth < lastDepth
      while (flowStack.size() > depth + 1) {
        flowStack.pop_back();
        outStream << "):" << flowStack.back();
      }
      flowStack[flowStack.size() - 1] = row.flow;
      outStream << ",";
    }
    lastDepth = depth;
    isRoot = false;
  };

  view.forEachTreeNode(writeNewickNode);
  while (flowStack.size() > 1) {
    flowStack.pop_back();
    outStream << "):" << flowStack.back();
  }
  outStream << ");\n";
  outStream << std::setprecision(oldPrecision);
}

void writeJsonTree(InfomapBase& im, const StateNetwork& network, std::ostream& outStream, bool states, bool writeLinks)
{
  OutputView view(im, network, states);
  std::vector<detail::PerLevelStat> perLevelStats;
  aggregatePerLevelCodelength(im.root(), perLevelStats);

  Json json;
  json["version"] = std::string("v") + INFOMAP_VERSION;
  json["args"] = im.parsedString;
  json["startedAt"] = io::Str() << im.getStartDate();
  json["completedIn"] = im.getElapsedTime().getElapsedTimeInSec();
  json["codelength"] = im.codelength();
  json["numLevels"] = im.maxTreeDepth();
  json["numTopModules"] = im.numTopModules();

  Json numModules = Json::array();
  for (const auto& perLevelStat : perLevelStats) {
    numModules.push_back(perLevelStat.numModules);
  }
  json["numModules"] = std::move(numModules);
  json["relativeCodelengthSavings"] = im.getRelativeCodelengthSavings();
  json["directed"] = !im.isUndirectedFlow();
  json["flowModel"] = flowModelToString(im.flowModel);
  json["higherOrder"] = im.haveMemory();

  if (im.haveMemory()) {
    json["stateLevel"] = states;
  }

  if (im.isBipartite()) {
    json["bipartiteStartId"] = network.bipartiteStartId();
  }

  writeJsonObjectPrefix(outStream, json);
  outStream << ",\"nodes\":[";

  const auto& metaData = network.metaData();
  auto metadataJson = [&metaData](auto nodeId) {
    Json metadata;
    const auto metaIt = metaData.find(nodeId);
    if (metaIt == metaData.end()) {
      return metadata;
    }
    const auto& meta = metaIt->second;
    for (unsigned int i = 0; i < meta.size(); ++i) {
      metadata[std::to_string(i)] = std::to_string(meta[i]);
    }
    return metadata;
  };
  auto firstNode = true;
  auto writeNode = [&](const Json& node) {
    if (!firstNode) {
      outStream << ",";
    }
    firstNode = false;
    outStream << node;
  };

  if (view.isHigherOrderPhysicalLevel()) {
    view.forEachLeaf(1, OutputLeafPolicy::HideBipartite, [&](const OutputLeafRow& row) {
      Json node;
      node["path"] = row.path;
      node["name"] = row.name;
      node["flow"] = row.flow;
      node["mec"] = row.modularCentrality;
      node["id"] = row.physicalId;
      writeNode(node);
    });
  } else {
    const auto multilevelModules = im.getMultilevelModules(states);

    view.forEachLeaf(1, OutputLeafPolicy::HideBipartite, [&](const OutputLeafRow& row) {
      Json node;
      node["path"] = row.path;
      node["modules"] = im.haveModules() ? Json(multilevelModules.at(view.leafId(row))) : Json::array({ 1 });
      node["name"] = row.name;
      node["flow"] = row.flow;
      node["mec"] = row.modularCentrality;

      // can't currently use both memory and meta map equation
      if (view.hasMetaData() && !states) {
        node["metadata"] = metadataJson(row.physicalId);
      }

      if (states) {
        node["stateId"] = row.stateId;
        if (view.isMultilayer()) {
          node["layerId"] = row.layerId;
        }
      }

      node["id"] = row.physicalId;
      writeNode(node);
    });
  }
  outStream << "],\"modules\":[";

  auto firstModule = true;
  view.forEachModule([&](const OutputModuleRow& module) {
    Json item;
    item["path"] = modulePathJson(module.jsonPath);
    item["enterFlow"] = module.enterFlow;
    item["exitFlow"] = module.exitFlow;
    item["numEdges"] = module.links.size();
    item["numChildren"] = module.numChildren;
    item["codelength"] = module.codelength;

    if (!firstModule) {
      outStream << ",";
    }
    firstModule = false;

    if (writeLinks) {
      writeJsonObjectPrefix(outStream, item);
      outStream << ",\"links\":[";
      auto firstLink = true;
      for (auto itLink : module.links) {
        if (!firstLink) {
          outStream << ",";
        }
        firstLink = false;
        Json link;
        link["source"] = itLink.first.first;
        link["target"] = itLink.first.second;
        link["flow"] = itLink.second;
        outStream << link;
      }
      outStream << "]}";
    } else {
      outStream << item;
    }
  });
  outStream << "]}";
}

void writeCsvTree(InfomapBase& im, const StateNetwork& network, std::ostream& outStream, bool states)
{
  auto oldPrecision = outStream.precision();
  OutputView view(im, network, states);
  outStream << std::setprecision(6);

  outStream << "path,flow,name,";

  if (view.isHigherOrderPhysicalLevel()) {
    outStream << view.nodeIdHeaderName() << "\n";
  } else {
    if (states) {
      outStream << view.nodeIdHeaderName() << ",";
      if (view.isMultilayer())
        outStream << "layer_id,";
    }
    outStream << "node_id\n";
  }

  view.forEachLeaf(1, OutputLeafPolicy::HideBipartite, [&](const OutputLeafRow& row) {
    const auto path = io::stringify(row.path, ":");
    outStream << path << ',' << row.flow << ",\"" << row.name << "\",";

    if (states) {
      outStream << row.stateId << ',';
      if (view.isMultilayer())
        outStream << row.layerId << ',';
    }

    outStream << row.physicalId << '\n';
  });

  outStream << std::setprecision(oldPrecision);
}

std::string writeTree(InfomapBase& im, const StateNetwork& network, const std::string& filename, bool states)
{
  auto outputFilename = getOutputFilename(im, filename, ".tree", states);
  SafeOutFile outFile { outputFilename, std::ios_base::out, im.overwriteOutput() };
  writeTree(im, network, outFile, states);
  outFile.commit();
  return outputFilename;
}

std::string writeFlowTree(InfomapBase& im, const StateNetwork& network, const std::string& filename, bool states)
{
  auto outputFilename = getOutputFilename(im, filename, ".ftree", states);
  SafeOutFile outFile { outputFilename, std::ios_base::out, im.overwriteOutput() };
  writeTree(im, network, outFile, states);
  writeTreeLinks(im, outFile, states);
  outFile.commit();
  return outputFilename;
}

std::string writeNewickTree(InfomapBase& im, const std::string& filename, bool states)
{
  auto outputFilename = getOutputFilename(im, filename, ".nwk", states);
  SafeOutFile outFile { outputFilename, std::ios_base::out, im.overwriteOutput() };
  writeNewickTree(im, outFile, states);
  outFile.commit();
  return outputFilename;
}

std::string writeJsonTree(InfomapBase& im, const StateNetwork& network, const std::string& filename, bool states, bool writeLinks)
{
  auto outputFilename = getOutputFilename(im, filename, ".json", states);
  SafeOutFile outFile { outputFilename, std::ios_base::out, im.overwriteOutput() };
  writeJsonTree(im, network, outFile, states, writeLinks);
  outFile.commit();
  return outputFilename;
}

std::string writeCsvTree(InfomapBase& im, const StateNetwork& network, const std::string& filename, bool states)
{
  auto outputFilename = getOutputFilename(im, filename, ".csv", states);
  SafeOutFile outFile { outputFilename, std::ios_base::out, im.overwriteOutput() };
  writeCsvTree(im, network, outFile, states);
  outFile.commit();
  return outputFilename;
}

} // namespace infomap
