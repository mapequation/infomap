/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Eriksson, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_AGPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "Output.h"
#include "../core/InfomapBase.h"
#include "../Infomap.h"
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

std::string Output::writeTree(InfomapBase& im, const std::string& filename, bool states)
{
  auto outputFilename = getOutputFilename(im, filename, ".tree", states);
  SafeOutFile outFile { outputFilename };
  Output::writeTree(im, outFile, states);
  return outputFilename;
}

std::string Output::writeFlowTree(InfomapBase& im, const std::string& filename, bool states)
{
  auto outputFilename = getOutputFilename(im, filename, ".ftree", states);
  SafeOutFile outFile { outputFilename };
  Output::writeTree(im, outFile, states);
  Output::writeTreeLinks(im, outFile, states);
  return outputFilename;
}

std::string Output::writeNewickTree(InfomapBase& im, const std::string& filename, bool states)
{
  auto outputFilename = getOutputFilename(im, filename, ".nwk", states);
  SafeOutFile outFile { outputFilename };
  Output::writeNewickTree(im, outFile, states);
  return outputFilename;
}

std::string Output::writeJsonTree(InfomapBase& im, const std::string& filename, bool states, bool writeLinks)
{
  auto outputFilename = getOutputFilename(im, filename, ".json", states);
  SafeOutFile outFile { outputFilename };
  Output::writeJsonTree(im, outFile, states, writeLinks);
  return outputFilename;
}

std::string Output::writeCsvTree(InfomapBase& im, const std::string& filename, bool states)
{
  auto outputFilename = getOutputFilename(im, filename, ".csv", states);
  SafeOutFile outFile { outputFilename };
  Output::writeCsvTree(im, outFile, states);
  return outputFilename;
}

std::string Output::writeClu(InfomapBase& im, const std::string& filename, bool states, int moduleIndexLevel)
{
  auto outputFilename = getOutputFilename(im, filename, ".clu", states);
  SafeOutFile outFile { outputFilename };

  outFile << std::setprecision(9);
  outFile << Output::getOutputFileHeader(im, states) << "\n";
  outFile << "# module level " << moduleIndexLevel << "\n";
  outFile << std::resetiosflags(std::ios::floatfield) << std::setprecision(6);

  if (states) {
    outFile << "# state_id module flow node_id";
    if (im.isMultilayerNetwork())
      outFile << " layer_id";
    outFile << '\n';
  } else {
    outFile << "# node_id module flow\n";
  }

  const auto shouldHideBipartiteNodes = im.isBipartite() && im.hideBipartiteNodes;
  const auto bipartiteStartId = shouldHideBipartiteNodes ? im.m_network.bipartiteStartId() : 0;

  if (im.haveMemory() && !states) {
    for (auto it(im.iterTreePhysical(moduleIndexLevel)); !it.isEnd(); ++it) {
      InfoNode& node = *it;
      if (node.isLeaf()) {
        if (shouldHideBipartiteNodes && node.physicalId >= bipartiteStartId) {
          continue;
        }

        outFile << node.physicalId << " " << it.moduleId() << " " << node.data.flow << "\n";
      }
    }
  } else {
    for (auto it(im.iterTree(moduleIndexLevel)); !it.isEnd(); ++it) {
      InfoNode& node = *it;
      if (node.isLeaf()) {
        if (shouldHideBipartiteNodes && node.physicalId >= bipartiteStartId) {
          continue;
        }

        if (states) {
          outFile << node.stateId << " " << it.moduleId() << " " << node.data.flow << " " << node.physicalId;
          if (im.isMultilayerNetwork())
            outFile << " " << node.layerId;
          outFile << "\n";
        } else
          outFile << node.physicalId << " " << it.moduleId() << " " << node.data.flow << "\n";
      }
    }
  }
  return outputFilename;
}

std::string Output::getNodeName(const InfomapBase& im, const InfoNode& node)
{
  try {
    return im.m_network.names().at(node.physicalId);
  } catch (...) {
    return io::stringify(node.physicalId);
  }
}

void Output::writeTree(InfomapBase& im, std::ostream& outStream, bool states)
{
  auto oldPrecision = outStream.precision();
  outStream << std::setprecision(9);
  outStream << Output::getOutputFileHeader(im, states) << "\n";
  outStream << std::setprecision(6);

  if (states) {
    outStream << "# path flow name state_id node_id";
    if (im.isMultilayerNetwork())
      outStream << " layer_id";
    outStream << '\n';
  } else {
    outStream << "# path flow name node_id\n";
  }

  const auto shouldHideBipartiteNodes = !im.printFlowTree && im.isBipartite() && im.hideBipartiteNodes;
  const auto bipartiteStartId = shouldHideBipartiteNodes ? im.m_network.bipartiteStartId() : 0;

  // TODO: Make a general iterator where merging physical nodes depend on a parameter rather than type to be able to DRY here
  if (im.haveMemory() && !states) {
    for (auto it(im.iterTreePhysical()); !it.isEnd(); ++it) {
      InfoNode& node = *it;
      if (node.isLeaf()) {
        if (shouldHideBipartiteNodes && node.physicalId >= bipartiteStartId) {
          continue;
        }

        auto& path = it.path();

        outStream << io::stringify(path, ":") << " " << node.data.flow << " \"" << getNodeName(im, node) << "\" " << node.physicalId << '\n';
      }
    }
  } else {
    for (auto it(im.iterTree()); !it.isEnd(); ++it) {
      InfoNode& node = *it;
      if (node.isLeaf()) {
        if (shouldHideBipartiteNodes && node.physicalId >= bipartiteStartId) {
          continue;
        }

        auto& path = it.path();

        outStream << io::stringify(path, ":") << " " << node.data.flow << " \"" << getNodeName(im, node) << "\" ";

        if (states) {
          outStream << node.stateId << " " << node.physicalId;
          if (im.isMultilayerNetwork())
            outStream << " " << node.layerId;
          outStream << '\n';
        } else {
          outStream << node.physicalId << '\n';
        }
      }
    }
  }

  outStream << std::setprecision(oldPrecision);
}

void Output::writeTreeLinks(InfomapBase& im, std::ostream& outStream, bool states)
{
  auto oldPrecision = outStream.precision();
  outStream << std::setprecision(6);

  auto moduleLinks = Output::aggregateModuleLinks(im, states);

  outStream << "*Links " << (im.isUndirectedFlow() ? "undirected" : "directed") << "\n";
  outStream << "#*Links path enterFlow exitFlow numEdges numChildren\n";

  // Use stateId to store depth on modules to optimize link aggregation
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

void Output::writeNewickTree(InfomapBase& im, std::ostream& outStream, bool states)
{
  auto oldPrecision = outStream.precision();
  outStream << std::setprecision(6);

  auto isRoot = true;
  unsigned int lastDepth = 0;
  std::vector<double> flowStack;

  auto writeNewickNode = [&](const InfoNode& node, unsigned int depth) {
    if (depth > lastDepth || isRoot) {
      outStream << "(";
      flowStack.push_back(node.data.flow);
      if (node.isLeaf())
        outStream << (states ? node.stateId : node.physicalId) << ":" << node.data.flow;
    } else if (depth == lastDepth) {
      outStream << ",";
      flowStack[flowStack.size() - 1] = node.data.flow;
      if (node.isLeaf()) {
        outStream << (states ? node.stateId : node.physicalId) << ":" << node.data.flow;
      }
    } else {
      // depth < lastDepth
      while (flowStack.size() > depth + 1) {
        flowStack.pop_back();
        outStream << "):" << flowStack.back();
      }
      flowStack[flowStack.size() - 1] = node.data.flow;
      outStream << ",";
    }
    lastDepth = depth;
    isRoot = false;
  };

  // TODO: Make a general iterator where merging physical nodes depend on a parameter rather than type to be able to DRY here
  if (im.haveMemory() && !states) {
    for (auto it(im.iterTreePhysical()); !it.isEnd(); ++it) {
      writeNewickNode(*it, it.depth());
    }
  } else {
    for (auto it(im.iterTree()); !it.isEnd(); ++it) {
      writeNewickNode(*it, it.depth());
    }
  }
  while (flowStack.size() > 1) {
    flowStack.pop_back();
    outStream << "):" << flowStack.back();
  }
  outStream << ");\n";
  outStream << std::setprecision(oldPrecision);
}

void Output::writeJsonTree(InfomapBase& im, std::ostream& outStream, bool states, bool writeLinks)
{
  auto oldPrecision = outStream.precision();

  outStream << "{";

  outStream << "\"version\":\"v" << INFOMAP_VERSION << "\","
            << "\"args\":\"" << im.parsedString << "\","
            << "\"startedAt\":\"" << im.m_startDate << "\","
            << "\"completedIn\":" << im.m_elapsedTime.getElapsedTimeInSec() << ","
            << "\"codelength\":" << im.codelength() << ","
            << "\"numLevels\":" << im.maxTreeDepth() << ","
            << "\"numTopModules\":" << im.numTopModules() << ","
            << "\"relativeCodelengthSavings\":" << im.getRelativeCodelengthSavings() << ","
            << "\"directed\":" << (im.isUndirectedFlow() ? "false" : "true") << ","
            << "\"flowModel\": \"" << flowModelToString(im.flowModel) << "\","
            << "\"higherOrder\":" << (im.haveMemory() ? "true" : "false") << ",";

  if (im.haveMemory()) {
    outStream << "\"stateLevel\":" << (states ? "true" : "false") << ",";
  }

  if (im.isBipartite()) {
    outStream << "\"bipartiteStartId\":" << im.m_network.bipartiteStartId() << ",";
  }

  outStream << std::setprecision(6);

  outStream << "\"nodes\":[";

  const auto shouldHideBipartiteNodes = im.isBipartite() && im.hideBipartiteNodes;
  const auto bipartiteStartId = shouldHideBipartiteNodes ? im.m_network.bipartiteStartId() : 0;

  // Hack to re-use getMultilevelModules from Infomap.cpp
  const auto multilevelModules = dynamic_cast<InfomapWrapper*>(&im)->getMultilevelModules(states);

  // don't append a comma after the last entry
  auto first = true;

  if (im.haveMemory() && !states) {
    for (auto it(im.iterTreePhysical()); !it.isEnd(); ++it) {
      InfoNode& node = *it;
      if (node.isLeaf()) {
        if (shouldHideBipartiteNodes && node.physicalId >= bipartiteStartId) {
          continue;
        }

        const auto path = io::stringify(it.path(), ",");
        const auto modules = im.haveModules() ? io::stringify(multilevelModules.at(node.physicalId), ",") : "1";

        if (first) {
          first = false;
        } else {
          outStream << ",";
        }

        outStream << "{"
                  << "\"path\":[" << path << "],"
                  << "\"modules\":[" << modules << "],"
                  << "\"name\":\"" << getNodeName(im, node) << "\","
                  << "\"flow\":" << node.data.flow << ","
                  << "\"mec\":" << it.modularCentrality() << ","
                  << "\"id\":" << node.physicalId << "}";
      }
    }
  } else {
    for (auto it(im.iterTree()); !it.isEnd(); ++it) {
      InfoNode& node = *it;
      if (node.isLeaf()) {
        if (shouldHideBipartiteNodes && node.physicalId >= bipartiteStartId) {
          continue;
        }

        if (first) {
          first = false;
        } else {
          outStream << ",";
        }

        const auto path = io::stringify(it.path(), ", ");
        const auto modules = im.haveModules() ? io::stringify(multilevelModules.at(node.stateId), ", ") : "1";

        outStream << "{"
                  << "\"path\":[" << path << "],"
                  << "\"modules\":[" << modules << "],"
                  << "\"name\":\"" << getNodeName(im, node) << "\","
                  << "\"flow\":" << node.data.flow << ","
                  << "\"mec\":" << it.modularCentrality() << ",";

        if (states) {
          outStream << "\"stateId\":" << node.stateId << ",";
          if (im.isMultilayerNetwork())
            outStream << "\"layerId\":" << node.layerId << ",";
        }

        outStream << "\"id\":" << node.physicalId << "}";
      }
    }
  }

  outStream << "],"; // tree

  // -------------
  // Write modules
  // -------------
  auto moduleLinks = Output::aggregateModuleLinks(im, states);

  first = true;

  outStream << "\"modules\":[";

  // Use stateId to store depth on modules to optimize link aggregation
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
              << "\"enterFlow\":" << module.data.enterFlow << ","
              << "\"exitFlow\":" << module.data.exitFlow << ","
              << "\"numEdges\":" << links.size() << ","
              << "\"numChildren\":" << module.infomapChildDegree();

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

void Output::writeCsvTree(InfomapBase& im, std::ostream& outStream, bool states)
{
  auto oldPrecision = outStream.precision();
  outStream << std::setprecision(6);

  outStream << "path,flow,name,";

  if (im.haveMemory() && !states) {
    outStream << "node_id\n";
  } else {
    if (states) {
      outStream << "state_id,";
      if (im.isMultilayerNetwork())
        outStream << "layer_id,";
    }
    outStream << "node_id\n";
  }

  const auto shouldHideBipartiteNodes = im.isBipartite() && im.hideBipartiteNodes;
  const auto bipartiteStartId = shouldHideBipartiteNodes ? im.m_network.bipartiteStartId() : 0;

  if (im.haveMemory() && !states) {
    for (auto it(im.iterTreePhysical()); !it.isEnd(); ++it) {
      InfoNode& node = *it;
      if (node.isLeaf()) {
        if (shouldHideBipartiteNodes && node.physicalId >= bipartiteStartId) {
          continue;
        }

        const auto path = io::stringify(it.path(), ":");
        outStream << path << ',' << node.data.flow << ",\"" << getNodeName(im, node) << "\"," << node.physicalId << '\n';
      }
    }
  } else {
    for (auto it(im.iterTree()); !it.isEnd(); ++it) {
      InfoNode& node = *it;
      if (node.isLeaf()) {
        if (shouldHideBipartiteNodes && node.physicalId >= bipartiteStartId) {
          continue;
        }

        const auto path = io::stringify(it.path(), ":");
        outStream << path << ',' << node.data.flow << ",\"" << getNodeName(im, node) << "\",";

        if (states) {
          outStream << node.stateId << ',';
          if (im.isMultilayerNetwork())
            outStream << node.layerId << ',';
        }

        outStream << node.physicalId << '\n';
      }
    }
  }

  outStream << std::setprecision(oldPrecision);
}

std::string Output::getOutputFileHeader(const InfomapBase& im, bool states)
{
  std::string bipartiteInfo = io::Str() << "\n# bipartite start id " << im.m_network.bipartiteStartId();
  return io::Str() << "# v" << INFOMAP_VERSION << "\n"
                   << "# ./Infomap " << im.parsedString << "\n"
                   << "# started at " << im.m_startDate << "\n"
                   << "# completed in " << im.m_elapsedTime.getElapsedTimeInSec() << " s\n"
                   << "# partitioned into " << im.maxTreeDepth() << " levels with " << im.numTopModules() << " top modules\n"
                   << "# codelength " << im.codelength() << " bits\n"
                   << "# relative codelength savings " << im.getRelativeCodelengthSavings() * 100 << "%\n"
                   << "# flow model " << flowModelToString(im.flowModel)
                   << (im.haveMemory() ? "\n# higher order" : "")
                   << (im.haveMemory() ? states ? "\n# state level" : "\n# physical level" : "")
                   << (im.m_network.isBipartite() ? bipartiteInfo : "");
}

std::map<std::string, Output::LinkMap> Output::aggregateModuleLinks(InfomapBase& im, bool states)
{
  // Aggregate links between each module. Rest is aggregated as exit flow

  // Links on nodes within sub infomap instances doesn't have links outside the root
  // so iterate over links on main instance and map to infomap tree iterator
  bool mergePhysicalNodes = im.haveMemory() && !states;

  // Map state id to parent in infomap tree iterator
  std::map<unsigned int, InfoNode*> stateIdToParent;
  std::map<unsigned int, unsigned int> stateIdToChildIndex;

  if (mergePhysicalNodes) {
    for (auto it(im.iterTreePhysical()); !it.isEnd(); ++it) {
      if (it->isLeaf()) {
        for (auto stateId : it->stateNodes) {
          stateIdToParent[stateId] = it->parent;
          stateIdToChildIndex[stateId] = it.childIndex();
        }
      } else {
        // Use stateId to store depth on modules to simplify link aggregation
        it->stateId = it.depth();
        it->index = it.childIndex();
      }
    }
  } else {
    for (auto it(im.iterTree()); !it.isEnd(); ++it) {
      if (it->isLeaf()) {
        stateIdToParent[it->stateId] = it->parent;
        stateIdToChildIndex[it->stateId] = it.childIndex();
      } else {
        // Use stateId to store depth on modules to simplify link aggregation
        it->stateId = it.depth();
        it->index = it.childIndex();
      }
    }
  }

  std::map<std::string, LinkMap> moduleLinks;

  for (auto& leaf : im.m_leafNodes) {
    for (auto& link : leaf->outEdges()) {
      double flow = link->data.flow;
      InfoNode* sourceParent = stateIdToParent[link->source->stateId];
      InfoNode* targetParent = stateIdToParent[link->target->stateId];

      auto sourceDepth = sourceParent->calculatePath().size() + 1;
      auto targetDepth = targetParent->calculatePath().size() + 1;

      auto sourceChildIndex = stateIdToChildIndex[link->source->stateId];
      auto targetChildIndex = stateIdToChildIndex[link->target->stateId];

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

        sourceChildIndex = sourceParentIt->index;
        targetChildIndex = targetParentIt->index;

        ++sourceParentIt;
        ++targetParentIt;

        --currentDepth;
      }
    }
  }

  return moduleLinks;
}

} // namespace infomap