/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef NETWORK_INPUT_PARSER_H_
#define NETWORK_INPUT_PARSER_H_

#include "SafeFile.h"
#include "../core/StateNetwork.h"
#include "../utils/Log.h"
#include "../utils/convert.h"

#include <cstdlib>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace infomap {
namespace input {

  struct ParsedVertex {
    unsigned int id = 0;
    std::string name;
    bool hasWeight = false;
    double weight = 1.0;
  };

  struct ParsedLink {
    unsigned int source = 0;
    unsigned int target = 0;
    double weight = 1.0;
  };

  struct ParsedStateNode {
    StateNetwork::StateNode node;
    bool hasWeight = false;
  };

  struct ParsedMultilayerLink {
    unsigned int sourceLayer = 0;
    unsigned int sourceNode = 0;
    unsigned int targetLayer = 0;
    unsigned int targetNode = 0;
    double weight = 1.0;
  };

  struct ParsedMultilayerIntraLink {
    unsigned int layer = 0;
    unsigned int sourceNode = 0;
    unsigned int targetNode = 0;
    double weight = 1.0;
  };

  struct ParsedMultilayerInterLink {
    unsigned int sourceLayer = 0;
    unsigned int node = 0;
    unsigned int targetLayer = 0;
    double weight = 1.0;
  };

  namespace detail {

    using InsensitiveStringSet = std::set<std::string, io::InsensitiveCompare>;

    inline bool isInputWhitespace(char c)
    {
      return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
    }

    inline bool isDigit(char c)
    {
      return c >= '0' && c <= '9';
    }

    inline void skipWhitespace(const char*& p)
    {
      while (isInputWhitespace(*p)) {
        ++p;
      }
    }

    inline bool parseUnsigned(const char*& p, unsigned int& value)
    {
      skipWhitespace(p);
      if (*p == '+') {
        ++p;
      }
      if (!isDigit(*p)) {
        return false;
      }

      unsigned long long result = 0;
      const unsigned long long maxValue = std::numeric_limits<unsigned int>::max();
      do {
        result = result * 10 + static_cast<unsigned long long>(*p - '0');
        if (result > maxValue) {
          return false;
        }
        ++p;
      } while (isDigit(*p));

      value = static_cast<unsigned int>(result);
      return true;
    }

    inline bool parseOptionalDouble(const char*& p, double& value)
    {
      skipWhitespace(p);
      if (*p == '\0' || *p == '#') {
        return false;
      }

      char* end = nullptr;
      const double parsed = std::strtod(p, &end);
      if (end == p) {
        return false;
      }

      value = parsed;
      p = end;
      return true;
    }

    inline ParsedLink parseLink(const std::string& line)
    {
      const char* p = line.c_str();
      ParsedLink link;
      if (!parseUnsigned(p, link.source) || !parseUnsigned(p, link.target))
        throw std::runtime_error(io::Str() << "Can't parse link data from line '" << line << "'");
      if (!parseOptionalDouble(p, link.weight)) {
        link.weight = 1.0;
      }
      return link;
    }

    inline ParsedStateNode parseStateNode(const std::string& line)
    {
      const char* p = line.c_str();
      ParsedStateNode parsed;
      if (!parseUnsigned(p, parsed.node.id) || !parseUnsigned(p, parsed.node.physicalId))
        throw std::runtime_error(io::Str() << "Can't parse any state node from line '" << line << "'");

      auto nameStart = line.find_first_of('\"', static_cast<std::size_t>(p - line.c_str()));
      auto nameEnd = line.find_last_of('\"');
      if (nameStart < nameEnd) {
        parsed.node.name = std::string(line.begin() + nameStart + 1, line.begin() + nameEnd);
        p = line.c_str() + nameEnd + 1;
      }
      if (parseOptionalDouble(p, parsed.node.weight)) {
        parsed.hasWeight = true;
        if (parsed.node.weight < 0)
          throw std::runtime_error(io::Str() << "Negative state node weight (" << parsed.node.weight << ") from line '" << line << "'");
      }
      return parsed;
    }

    inline ParsedMultilayerLink parseMultilayerLink(const std::string& line)
    {
      const char* p = line.c_str();
      ParsedMultilayerLink link;
      if (!parseUnsigned(p, link.sourceLayer) || !parseUnsigned(p, link.sourceNode) || !parseUnsigned(p, link.targetLayer) || !parseUnsigned(p, link.targetNode))
        throw std::runtime_error(io::Str() << "Can't parse multilayer link data from line '" << line << "'");
      if (!parseOptionalDouble(p, link.weight)) {
        link.weight = 1.0;
      }
      return link;
    }

    inline ParsedMultilayerIntraLink parseMultilayerIntraLink(const std::string& line)
    {
      const char* p = line.c_str();
      ParsedMultilayerIntraLink link;
      if (!parseUnsigned(p, link.layer) || !parseUnsigned(p, link.sourceNode) || !parseUnsigned(p, link.targetNode))
        throw std::runtime_error(io::Str() << "Can't parse intra-multilayer link data from line '" << line << "'");
      if (!parseOptionalDouble(p, link.weight)) {
        link.weight = 1.0;
      }
      return link;
    }

    inline ParsedMultilayerInterLink parseMultilayerInterLink(const std::string& line)
    {
      const char* p = line.c_str();
      ParsedMultilayerInterLink link;
      if (!parseUnsigned(p, link.sourceLayer) || !parseUnsigned(p, link.node) || !parseUnsigned(p, link.targetLayer))
        throw std::runtime_error(io::Str() << "Can't parse inter-multilayer link data from line '" << line << "'");
      if (!parseOptionalDouble(p, link.weight)) {
        link.weight = 1.0;
      }
      if (link.sourceLayer == link.targetLayer)
        throw std::runtime_error(io::Str() << "Inter-layer link from line '" << line << "' doesn't go between different layers.");
      return link;
    }

    inline InsensitiveStringSet generalValidHeadings()
    {
      return { "*vertices", "*states", "*multilayer", "*multiplex", "*intra", "*inter", "*paths", "*edges", "*arcs", "*links", "*contexts", "*bipartite" };
    }

    inline InsensitiveStringSet generalIgnoredHeadings()
    {
      return { "*contexts" };
    }

    template <typename Sink>
    std::string parseVertices(std::ifstream& file, const std::string&, Sink& sink)
    {
      Log() << "  Parsing vertices...\n"
            << std::flush;
      std::string line;
      while (!std::getline(file, line).fail()) {
        if (line.empty() || line[0] == '#')
          continue;

        if (line[0] == '*')
          break;

        std::istringstream extractor(line);
        ParsedVertex vertex;
        if (!(extractor >> vertex.id))
          throw std::runtime_error(io::Str() << "Can't parse node id from line '" << line << "'");

        auto nameStart = line.find_first_of('\"');
        auto nameEnd = line.find_last_of('\"');
        if (nameStart < nameEnd) {
          vertex.name = std::string(line.begin() + nameStart + 1, line.begin() + nameEnd);
          line = line.substr(nameEnd + 1);
          extractor.clear();
          extractor.str(line);
        } else {
          if (!(extractor >> vertex.name))
            throw std::runtime_error(io::Str() << "Can't parse node name from line '" << line << "'");
        }
        if ((extractor >> vertex.weight)) {
          vertex.hasWeight = true;
          if (vertex.weight < 0)
            throw std::runtime_error(io::Str() << "Negative node weight (" << vertex.weight << ") from line '" << line << "'");
        }

        sink.onPhysicalNode(vertex);
      }
      Log() << "  -> " << sink.numPhysicalNodes() << " physical nodes added\n";
      return line;
    }

    template <typename Sink>
    std::string parseStateNodes(std::ifstream& file, const std::string&, Sink& sink)
    {
      Log() << "  Parsing state nodes...\n"
            << std::flush;
      unsigned int numStateNodesFound = 0;
      std::string line;
      while (!std::getline(file, line).fail()) {
        if (line.empty() || line[0] == '#')
          continue;

        if (line[0] == '*')
          break;

        auto stateNode = parseStateNode(line);
        sink.onStateNode(stateNode);
        ++numStateNodesFound;
      }
      Log() << "  -> " << numStateNodesFound << " state nodes added\n";
      return line;
    }

    template <typename Sink>
    std::string parseLinks(std::ifstream& file, Sink& sink)
    {
      bool parsingLinks = false;
      std::string line;
      while (!std::getline(file, line).fail()) {
        if (line.empty() || line[0] == '#')
          continue;

        if (line[0] == '*')
          break;

        if (!parsingLinks) {
          parsingLinks = true;
          Log() << "  Parsing links...\n"
                << std::flush;
        }

        sink.onLink(parseLink(line));
      }
      if (parsingLinks)
        Log() << "  -> " << sink.numLinks() << " links\n";
      return line;
    }

    template <typename Sink>
    std::string parseMultilayerLinks(std::ifstream& file, Sink& sink)
    {
      Log() << "  Parsing multilayer links...\n"
            << std::flush;

      if (sink.matchableMultilayerIds() > 0) {
        Log() << "  Creating matchable state ids using: nodeId << (log2(" << sink.matchableMultilayerIds() << ") + 1) | layerId\n";
      }

      std::string line;
      while (!std::getline(file, line).fail()) {
        if (line.empty() || line[0] == '#')
          continue;

        if (line[0] == '*')
          break;

        sink.onMultilayerLink(parseMultilayerLink(line));
      }
      Log() << "  -> " << sink.numLayerLinks() << " links in " << sink.numLayers() << " layers\n";
      Log() << "    -> " << sink.numIntraLayerLinks() << " intra-layer links\n";
      Log() << "    -> " << sink.numInterLayerLinks() << " inter-layer links\n";
      return line;
    }

    template <typename Sink>
    std::string parseMultilayerIntraLinks(std::ifstream& file, Sink& sink)
    {
      Log() << "  Parsing intra-layer links...\n"
            << std::flush;

      if (sink.matchableMultilayerIds() > 0) {
        Log() << "  Creating matchable state ids using: nodeId << (log2(" << sink.matchableMultilayerIds() << ") + 1) | layerId\n";
      }

      std::string line;
      while (!std::getline(file, line).fail()) {
        if (line.empty() || line[0] == '#')
          continue;

        if (line[0] == '*')
          break;

        sink.onMultilayerIntraLink(parseMultilayerIntraLink(line));
      }
      Log() << "  -> " << sink.numIntraLayerLinks() << " intra-layer links\n";
      return line;
    }

    template <typename Sink>
    std::string parseMultilayerInterLinks(std::ifstream& file, Sink& sink)
    {
      Log() << "  Parsing inter-layer links...\n"
            << std::flush;
      std::string line;
      while (!std::getline(file, line).fail()) {
        if (line.empty() || line[0] == '#')
          continue;

        if (line[0] == '*')
          break;

        sink.onMultilayerInterLink(parseMultilayerInterLink(line));
      }
      Log() << "  -> " << sink.numInterLayerLinks() << " inter-layer links\n";
      return line;
    }

    template <typename Sink>
    std::string parseBipartiteLinks(std::ifstream& file, const std::string& heading, Sink& sink)
    {
      Log() << "  Parsing bipartite links...\n";
      std::istringstream extractor(heading);
      std::string tmp;
      unsigned int bipartiteStartId = 0;
      if (!(extractor >> tmp >> bipartiteStartId))
        throw std::runtime_error(io::Str() << "Can't parse bipartite start id from line '" << heading << "'");

      Log() << "  -> Using bipartite start id " << bipartiteStartId << "\n";
      sink.onBipartiteStart(bipartiteStartId);
      std::string line;
      while (!std::getline(file, line).fail()) {
        if (line.empty() || line[0] == '#')
          continue;

        if (line[0] == '*')
          break;

        sink.onBipartiteLink(line, parseLink(line));
      }
      return line;
    }

    inline std::string ignoreSection(std::ifstream& file, const std::string& heading)
    {
      Log() << "(Ignoring section " << heading << ") ";
      std::string line;
      while (!std::getline(file, line).fail()) {
        if (line[0] == '*')
          break;
      }
      return line;
    }

    template <typename Sink>
    void parseNetwork(const std::string& filename, const InsensitiveStringSet& validHeadings, const InsensitiveStringSet& ignoreHeadings, Sink& sink, const std::string& startHeading = "")
    {
      sink.onFileInput();
      SafeInFile input(filename);

      std::string heading = !startHeading.empty() ? startHeading : parseLinks(input, sink);

      while (!heading.empty() && heading[0] == '*') {
        std::string headingLowerCase = io::tolower(io::firstWord(heading));
        if (validHeadings.count(headingLowerCase) == 0) {
          throw std::runtime_error(io::Str() << "Unrecognized heading in network file: '" << headingLowerCase << "'.");
        }
        bool shouldIgnoreHeading = ignoreHeadings.count(headingLowerCase) > 0;
        if (!shouldIgnoreHeading && headingLowerCase == "*vertices") {
          heading = parseVertices(input, heading, sink);
        } else if (!shouldIgnoreHeading && headingLowerCase == "*states") {
          heading = parseStateNodes(input, heading, sink);
        } else if (!shouldIgnoreHeading && headingLowerCase == "*edges") {
          if (!sink.isUndirectedFlow())
            Log() << "\n --> Notice: Links marked as undirected but parsed as directed.\n";
          heading = parseLinks(input, sink);
        } else if (!shouldIgnoreHeading && headingLowerCase == "*arcs") {
          if (sink.isUndirectedFlow())
            Log() << "\n --> Notice: Links marked as directed but parsed as undirected.\n";
          heading = parseLinks(input, sink);
        } else if (!shouldIgnoreHeading && headingLowerCase == "*links") {
          heading = parseLinks(input, sink);
        } else if (!shouldIgnoreHeading && (headingLowerCase == "*multilayer" || headingLowerCase == "*multiplex")) {
          heading = parseMultilayerLinks(input, sink);
        } else if (!shouldIgnoreHeading && headingLowerCase == "*intra") {
          heading = parseMultilayerIntraLinks(input, sink);
        } else if (!shouldIgnoreHeading && headingLowerCase == "*inter") {
          heading = parseMultilayerInterLinks(input, sink);
        } else if (!shouldIgnoreHeading && headingLowerCase == "*bipartite") {
          heading = parseBipartiteLinks(input, heading, sink);
        } else {
          heading = ignoreSection(input, headingLowerCase);
        }
      }

      sink.onNetworkParsed();
      Log() << "Done!\n";
    }

  } // namespace detail

  template <typename Sink>
  void parseNetworkInput(const std::string& filename, Sink& sink)
  {
    Log() << "Parsing " << (sink.isUndirectedFlow() ? "undirected" : "directed") << " network from file '" << filename << "'...\n";
    detail::parseNetwork(filename, detail::generalValidHeadings(), detail::generalIgnoredHeadings(), sink);
  }

  template <typename Sink>
  void parseMetaDataInput(const std::string& filename, Sink& sink)
  {
    Log() << "Parsing meta data from '" << filename << "'...\n";
    SafeInFile input(filename);
    std::string line;
    while (!std::getline(input, line).fail()) {
      if (line.empty() || line[0] == '#')
        continue;

      if (line[0] == '*')
        break;

      std::istringstream extractor(line);
      unsigned int nodeId = 0;
      if (!(extractor >> nodeId))
        throw std::runtime_error(io::Str() << "Can't parse node id from line '" << line << "'");

      std::vector<int> metaData;
      unsigned int metaId = 0;
      while (extractor >> metaId) {
        metaData.push_back(metaId);
      }
      if (metaData.empty())
        throw std::runtime_error(io::Str() << "Can't parse any meta data from line '" << line << "'");

      sink.onMetaData(nodeId, metaData);
    }
    Log() << " -> Parsed " << sink.numMetaDataColumns() << " columns of meta data for " << sink.numMetaDataRows() << " nodes.\n";
  }

} // namespace input
} // namespace infomap

#endif // NETWORK_INPUT_PARSER_H_
