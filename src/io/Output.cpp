/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "Output.h"
#include "OutputView.h"
#include "../core/InfomapBase.h"
#include "../core/StateNetwork.h"
#include "../io/SafeFile.h"

namespace infomap {

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
  SafeOutFile outFile { outputFilename };
  OutputView view(im, network, states);

  outFile << std::setprecision(9);
  outFile << getOutputFileHeader(im, network, states) << "\n";
  outFile << "# module level " << moduleIndexLevel << "\n";
  outFile << std::resetiosflags(std::ios::floatfield) << std::setprecision(6);

  if (states) {
    outFile << "# state_id module flow node_id";
    if (view.isMultilayer())
      outFile << " layer_id";
    outFile << '\n';
  } else {
    outFile << "# node_id module flow\n";
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
    outStream << "# path flow name state_id node_id";
    if (view.isMultilayer())
      outStream << " layer_id";
    outStream << '\n';
  } else {
    outStream << "# path flow name node_id\n";
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

using Link = std::pair<unsigned int, unsigned int>;
using LinkMap = std::map<Link, double>;

std::map<std::string, LinkMap> aggregateModuleLinks(InfomapBase& im, OutputView& view)
{
  // Aggregate links between each module. Rest is aggregated as exit flow

  // Links on nodes within sub infomap instances doesn't have links outside the root
  // so iterate over links on main instance and map to infomap tree iterator
  auto stateTargets = view.stateNodeTargets();

  std::map<std::string, LinkMap> moduleLinks;

  for (auto& leaf : im.leafNodes()) {
    for (auto& link : leaf->outEdges()) {
      double flow = link->data.flow;
      auto& sourceTarget = stateTargets[link->source->stateId];
      auto& targetTarget = stateTargets[link->target->stateId];
      InfoNode* sourceParent = sourceTarget.parent;
      InfoNode* targetParent = targetTarget.parent;

      auto sourceDepth = sourceParent->calculatePath().size() + 1;
      auto targetDepth = targetParent->calculatePath().size() + 1;

      auto sourceChildIndex = sourceTarget.childIndex;
      auto targetChildIndex = targetTarget.childIndex;

      auto sourceParentIt = InfomapParentIterator(sourceParent);
      auto targetParentIt = InfomapParentIterator(targetParent);

      // Iterate to same depth
      // First raise target
      while (targetDepth > sourceDepth) {
        ++targetParentIt;
        --targetDepth;
      }

      // Raise source to same depth
      while (sourceDepth > targetDepth) {
        ++sourceParentIt;
        --sourceDepth;
      }

      auto currentDepth = sourceDepth;
      // Add link if same parent

      while (currentDepth > 0) {
        if (sourceParentIt == targetParentIt) {
          // Skip self-links
          if (sourceChildIndex != targetChildIndex) {
            auto parentId = io::stringify(sourceParentIt->calculatePath(), ":");
            auto& linkMap = moduleLinks[parentId];
            linkMap[std::make_pair(sourceChildIndex + 1, targetChildIndex + 1)] += flow;
          }
        }

        sourceChildIndex = sourceParentIt->childIndex();
        targetChildIndex = targetParentIt->childIndex();

        ++sourceParentIt;
        ++targetParentIt;

        --currentDepth;
      }
    }
  }

  return moduleLinks;
}

void writeTreeLinks(InfomapBase& im, std::ostream& outStream, bool states)
{
  auto oldPrecision = outStream.precision();
  outStream << std::setprecision(6);

  OutputView view(im, im.network(), states);
  auto moduleLinks = aggregateModuleLinks(im, view);

  outStream << "*Links " << (im.isUndirectedFlow() ? "undirected" : "directed") << "\n";
  outStream << "#*Links path enterFlow exitFlow numEdges numChildren\n";

  for (auto it(im.iterModules()); !it.isEnd(); ++it) {
    auto parentId = io::stringify(it.path(), ":");
    auto& module = *it;
    auto& links = moduleLinks[parentId];

    outStream << "*Links " << (parentId.empty() ? "root" : parentId) << " " << module.data.enterFlow << " " << module.data.exitFlow << " " << links.size() << " " << module.infomapChildDegree() << "\n";

    for (auto itLink : links) {
      unsigned int sourceId = itLink.first.first;
      unsigned int targetId = itLink.first.second;
      double flow = itLink.second;
      outStream << sourceId << " " << targetId << " " << flow << "\n";
    }
  }

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
        outStream << (states ? row.stateId : row.physicalId) << ":" << row.flow;
    } else if (depth == lastDepth) {
      outStream << ",";
      flowStack[flowStack.size() - 1] = row.flow;
      if (node.isLeaf()) {
        outStream << (states ? row.stateId : row.physicalId) << ":" << row.flow;
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
  auto oldPrecision = outStream.precision();
  OutputView view(im, network, states);
  std::vector<detail::PerLevelStat> perLevelStats;
  aggregatePerLevelCodelength(im.root(), perLevelStats);

  outStream << "{";

  outStream << "\"version\":\"v" << INFOMAP_VERSION << "\","
            << "\"args\":\"" << im.parsedString << "\","
            << "\"startedAt\":\"" << im.getStartDate() << "\","
            << "\"completedIn\":" << im.getElapsedTime().getElapsedTimeInSec() << ","
            << "\"codelength\":" << im.codelength() << ","
            << "\"numLevels\":" << im.maxTreeDepth() << ","
            << "\"numTopModules\":" << im.numTopModules() << ","
            << "\"numModules\":[";

  for (size_t i = 0; i < perLevelStats.size(); ++i) {
    if (i > 0) {
      outStream << ",";
    }
    outStream << perLevelStats[i].numModules;
  }

  outStream << "],"
            << "\"relativeCodelengthSavings\":" << im.getRelativeCodelengthSavings() << ","
            << "\"directed\":" << (im.isUndirectedFlow() ? "false" : "true") << ","
            << "\"flowModel\": \"" << flowModelToString(im.flowModel) << "\","
            << "\"higherOrder\":" << (im.haveMemory() ? "true" : "false") << ",";

  if (im.haveMemory()) {
    outStream << "\"stateLevel\":" << (states ? "true" : "false") << ",";
  }

  if (im.isBipartite()) {
    outStream << "\"bipartiteStartId\":" << network.bipartiteStartId() << ",";
  }

  outStream << std::setprecision(6);

  outStream << "\"nodes\":[";

  auto metaData = network.metaData();
  auto writeMeta = [&metaData](auto& outStream, auto nodeId) {
    outStream << "\"metadata\":{";
    auto meta = metaData[nodeId];
    for (unsigned int i = 0; i < meta.size(); ++i) {
      outStream << '"' << i << "\":"
                << '"' << meta[i] << '"'; // metadata class as string to highlight that this is a categorical variable
      if (i < meta.size() - 1)
        outStream << ',';
    }
    outStream << "},";
  };

  // don't append a comma after the last entry
  auto first = true;

  if (view.isHigherOrderPhysicalLevel()) {
    view.forEachLeaf(1, OutputLeafPolicy::HideBipartite, [&](const OutputLeafRow& row) {
      const auto path = io::stringify(row.path, ",");

      if (first) {
        first = false;
      } else {
        outStream << ",";
      }

      outStream << "{"
                << "\"path\":[" << path << "],"
                << "\"name\":\"" << row.name << "\","
                << "\"flow\":" << row.flow << ","
                << "\"mec\":" << row.modularCentrality << ","
                << "\"id\":" << row.physicalId << "}";
    });
  } else {
    const auto multilevelModules = im.getMultilevelModules(states);

    view.forEachLeaf(1, OutputLeafPolicy::HideBipartite, [&](const OutputLeafRow& row) {
      if (first) {
        first = false;
      } else {
        outStream << ",";
      }

      const auto path = io::stringify(row.path, ", ");
      const auto modules = im.haveModules() ? io::stringify(multilevelModules.at(states ? row.stateId : row.physicalId), ", ") : "1";

      outStream << "{"
                << "\"path\":[" << path << "],"
                << "\"modules\":[" << modules << "],"
                << "\"name\":\"" << row.name << "\","
                << "\"flow\":" << row.flow << ","
                << "\"mec\":" << row.modularCentrality << ",";

      // can't currently use both memory and meta map equation
      if (view.hasMetaData() && !states) {
        writeMeta(outStream, row.physicalId);
      }

      if (states) {
        outStream << "\"stateId\":" << row.stateId << ",";
        if (view.isMultilayer())
          outStream << "\"layerId\":" << row.layerId << ",";
      }

      outStream << "\"id\":" << row.physicalId << "}";
    });
  }

  outStream << "],"; // tree

  // -------------
  // Write modules
  // -------------

  // Uses stateId to store depth on modules to optimize link aggregation
  auto moduleLinks = aggregateModuleLinks(im, view);

  first = true;

  outStream << "\"modules\":[";

  for (auto it(im.iterModules()); !it.isEnd(); ++it) {
    const auto parentId = io::stringify(it.path(), ":");
    const auto& module = *it;
    const auto& links = moduleLinks[parentId];
    const auto path = io::stringify(it.path(), ",");

    if (first) {
      first = false;
    } else {
      outStream << ",";
    }

    outStream << "{";

    outStream << "\"path\":[" << (parentId.empty() ? "0" : path) << "],"
              << "\"enterFlow\":" << module.data.enterFlow << ','
              << "\"exitFlow\":" << module.data.exitFlow << ','
              << "\"numEdges\":" << links.size() << ','
              << "\"numChildren\":" << module.infomapChildDegree() << ','
              << "\"codelength\":" << module.codelength;

    if (writeLinks) {
      outStream << ","
                << "\"links\":[";

      auto firstLink = true;

      for (auto itLink : links) {
        if (firstLink) {
          firstLink = false;
        } else {
          outStream << ",";
        }

        unsigned int sourceId = itLink.first.first;
        unsigned int targetId = itLink.first.second;
        double flow = itLink.second;
        outStream << "{\"source\":" << sourceId << ",\"target\":" << targetId << ",\"flow\":" << flow << "}";
      }
      outStream << "]"; // links
    }

    outStream << "}";
  }

  outStream << "]"; // modules

  outStream << "}";

  outStream << std::setprecision(oldPrecision);
}

void writeCsvTree(InfomapBase& im, const StateNetwork& network, std::ostream& outStream, bool states)
{
  auto oldPrecision = outStream.precision();
  OutputView view(im, network, states);
  outStream << std::setprecision(6);

  outStream << "path,flow,name,";

  if (view.isHigherOrderPhysicalLevel()) {
    outStream << "node_id\n";
  } else {
    if (states) {
      outStream << "state_id,";
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
  SafeOutFile outFile { outputFilename };
  writeTree(im, network, outFile, states);
  return outputFilename;
}

std::string writeFlowTree(InfomapBase& im, const StateNetwork& network, const std::string& filename, bool states)
{
  auto outputFilename = getOutputFilename(im, filename, ".ftree", states);
  SafeOutFile outFile { outputFilename };
  writeTree(im, network, outFile, states);
  writeTreeLinks(im, outFile, states);
  return outputFilename;
}

std::string writeNewickTree(InfomapBase& im, const std::string& filename, bool states)
{
  auto outputFilename = getOutputFilename(im, filename, ".nwk", states);
  SafeOutFile outFile { outputFilename };
  writeNewickTree(im, outFile, states);
  return outputFilename;
}

std::string writeJsonTree(InfomapBase& im, const StateNetwork& network, const std::string& filename, bool states, bool writeLinks)
{
  auto outputFilename = getOutputFilename(im, filename, ".json", states);
  SafeOutFile outFile { outputFilename };
  writeJsonTree(im, network, outFile, states, writeLinks);
  return outputFilename;
}

std::string writeCsvTree(InfomapBase& im, const StateNetwork& network, const std::string& filename, bool states)
{
  auto outputFilename = getOutputFilename(im, filename, ".csv", states);
  SafeOutFile outFile { outputFilename };
  writeCsvTree(im, network, outFile, states);
  return outputFilename;
}

} // namespace infomap
