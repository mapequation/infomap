/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "Config.h"
#include "ConfigBuilder.h"
#include "OutputFormats.h"
#include "../utils/Log.h"
#include "../utils/convert.h"
#include <vector>
#include <stdexcept>
#include <utility>

namespace infomap {

constexpr int FlowModel::undirected;
constexpr int FlowModel::directed;
constexpr int FlowModel::undirdir;
constexpr int FlowModel::outdirdir;
constexpr int FlowModel::rawdir;
constexpr int FlowModel::precomputed;

namespace {

  const std::vector<std::pair<std::string, FlowModel>>& flowModelMappings()
  {
    static const std::vector<std::pair<std::string, FlowModel>> mappings = {
      { "undirected", FlowModel::undirected },
      { "directed", FlowModel::directed },
      { "undirdir", FlowModel::undirdir },
      { "outdirdir", FlowModel::outdirdir },
      { "rawdir", FlowModel::rawdir },
      { "precomputed", FlowModel::precomputed },
    };
    return mappings;
  }

  void enableOutputFormat(Config& config, const OutputFormat& format)
  {
    switch (format.kind) {
    case OutputKind::Clu:
      config.printClu = true;
      break;
    case OutputKind::Tree:
      config.printTree = true;
      break;
    case OutputKind::FlowTree:
      config.printFlowTree = true;
      break;
    case OutputKind::Newick:
      config.printNewick = true;
      break;
    case OutputKind::Json:
      config.printJson = true;
      break;
    case OutputKind::Csv:
      config.printCsv = true;
      break;
    case OutputKind::PajekNetwork:
      config.printPajekNetwork = true;
      break;
    case OutputKind::StateNetwork:
      config.printStateNetwork = true;
      break;
    case OutputKind::FlowNetwork:
      config.printFlowNetwork = true;
      break;
    }
  }

} // namespace

const std::vector<std::string>& flowModelNames()
{
  static const std::vector<std::string> names = [] {
    std::vector<std::string> values;
    for (const auto& mapping : flowModelMappings()) {
      values.push_back(mapping.first);
    }
    return values;
  }();
  return names;
}

bool parseFlowModel(const std::string& name, FlowModel& flowModel)
{
  for (const auto& mapping : flowModelMappings()) {
    if (mapping.first == name) {
      flowModel = mapping.second;
      return true;
    }
  }
  return false;
}

const char* flowModelToString(const FlowModel& flowModel)
{
  for (const auto& mapping : flowModelMappings()) {
    if (mapping.second == flowModel) {
      return mapping.first.c_str();
    }
  }
  return "undirected";
}

Config::Config(const std::string& flags, bool isCLI) : isCLI(isCLI)
{
  const auto parsed = ConfigBuilder::parseRaw(*this, flags, isCLI);
  ConfigBuilder::applyParsed(*this, parsed, isCLI);

  parsedString = flags;
  parsedOptions = parsed.usedOptions;

  if (verbosity > 0) {
    prettyOutput = false;
  }

  if (printAllTrials && numTrials < 2) {
    printAllTrials = false;
  }

  adaptDefaults();

  Log::init(verbosity, silent, verboseNumberPrecision, prettyOutput);
}

void Config::adaptDefaults()
{
  auto outputs = io::split(outputFormats, ',');
  for (std::string& o : outputs) {
    const auto* format = findOutputFormat(o);
    if (format == nullptr) {
      throw std::runtime_error(io::Str() << "Unrecognized output format: '" << o << "'.");
    }
    enableOutputFormat(*this, *format);
  }

  // Of no output format specified, use tree as default (if not used as a library).
  if (isCLI && !haveModularResultOutput()) {
    printTree = true;
  }
}

std::ostream& operator<<(std::ostream& out, FlowModel f)
{
  return out << flowModelToString(f);
}

} // namespace infomap
