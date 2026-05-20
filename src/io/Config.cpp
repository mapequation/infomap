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
#include <vector>
#include <stdexcept>

namespace infomap {

constexpr int FlowModel::undirected;
constexpr int FlowModel::directed;
constexpr int FlowModel::undirdir;
constexpr int FlowModel::outdirdir;
constexpr int FlowModel::rawdir;
constexpr int FlowModel::precomputed;

namespace {

  void enableOutputFormat(Config& config, const OutputFormat& format)
  {
    switch (format.flag) {
    case OutputFormatFlag::Clu:
      config.printClu = true;
      break;
    case OutputFormatFlag::Tree:
      config.printTree = true;
      break;
    case OutputFormatFlag::FlowTree:
      config.printFlowTree = true;
      break;
    case OutputFormatFlag::Newick:
      config.printNewick = true;
      break;
    case OutputFormatFlag::Json:
      config.printJson = true;
      break;
    case OutputFormatFlag::Csv:
      config.printCsv = true;
      break;
    case OutputFormatFlag::PajekNetwork:
      config.printPajekNetwork = true;
      break;
    case OutputFormatFlag::StateNetwork:
      config.printStateNetwork = true;
      break;
    case OutputFormatFlag::FlowNetwork:
      config.printFlowNetwork = true;
      break;
    }
  }

} // namespace

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
