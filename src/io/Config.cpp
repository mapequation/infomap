/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "Config.h"
#include "InfomapError.h"
#include "OutputFormats.h"
#include "ParameterCatalog.h"
#include "ProgramInterface.h"
#include "SafeFile.h"
#include "../utils/FileURI.h"
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

  // Config invariants applied by adaptDefaults. These describe the coherent
  // state of a Config after fields have been set, regardless of whether the
  // fields were set by flag parsing or by direct mutation through bindings.

  void applyLibraryOutputDefaults(Config& config)
  {
    if (!config.isCLI && config.outDirectory.empty())
      config.noFileOutput = true;
  }

  void validateRequiredCliOutput(const Config& config)
  {
    if (!config.noFileOutput && config.outDirectory.empty() && config.isCLI) {
      throw std::runtime_error("Missing out_directory");
    }
  }

  void applyOptionInteractions(Config& config)
  {
    if (config.regularized) {
      config.recordedTeleportation = true;
    }

    if (config.noInfomap) {
      config.numTrials = 1;
    }
  }

  void applyFingerprintOnlyOutputInteraction(Config& config)
  {
    if (config.printConfigFingerprint) {
      config.noFileOutput = true;
    }
  }

  void applyRuntimeOutputInteractions(Config& config)
  {
    if (config.verbosity > 0) {
      config.prettyOutput = false;
    }

    if (config.printAllTrials && config.numTrials < 2) {
      config.printAllTrials = false;
    }
  }

  void validateRunReportOutput(const Config& config)
  {
    if (config.memoryReport && config.timingJsonPath.empty()) {
      throw std::runtime_error("--memory-report requires --timing-json");
    }
    if (config.timingJsonPath == "-" && config.summaryJsonPath == "-") {
      throw std::runtime_error("--timing-json - and --summary-json - cannot both write to stdout");
    }
    if (config.timingJsonPath == "-" && config.runManifestPath == "-") {
      throw std::runtime_error("--timing-json - and --manifest-json - cannot both write to stdout");
    }
    if (config.summaryJsonPath == "-" && config.runManifestPath == "-") {
      throw std::runtime_error("--summary-json - and --manifest-json - cannot both write to stdout");
    }
    if (config.timingJsonPath == "-" && !config.silent) {
      throw std::runtime_error("--timing-json - requires --silent");
    }
    if (config.summaryJsonPath == "-" && !config.silent) {
      throw std::runtime_error("--summary-json - requires --silent");
    }
    if (config.runManifestPath == "-" && !config.silent) {
      throw std::runtime_error("--manifest-json - requires --silent");
    }
  }

  void normalizeOutputDirectory(Config& config)
  {
    if (!config.haveOutput() || config.outDirectory.empty())
      return;

    if (config.outDirectory.back() != '/')
      config.outDirectory.push_back('/');
  }

  void applyOutputNameDefault(Config& config)
  {
    if (config.outName.empty()) {
      config.outName = !config.networkFile.empty() ? FileURI(config.networkFile).getName() : "no-name";
    }
  }

  // Lifecycle-only steps. These read staged parse state, touch the filesystem,
  // or mutate global state — they are not Config invariants and must not fire
  // when a library user calls adaptDefaults() on a mutated Config.

  void rejectDeprecatedAliases(const ParsedParameterSet& parsed)
  {
    if (parsed.deprecatedIncludeSelfLinks) {
      throw std::runtime_error("The --include-self-links flag is deprecated; self-links are included by default. Use --no-self-links to exclude them.");
    }
  }

  void applyOutputDirectory(Config& config, const ParsedParameterSet& parsed)
  {
    if (!parsed.optionalOutputDir.empty())
      config.outDirectory = parsed.optionalOutputDir[0];
  }

  void applyFlowModelSelection(Config& config, const ParsedParameterSet& parsed)
  {
    if (config.directed) {
      config.setFlowModel(FlowModel::directed);
      return;
    }

    if (parsed.flowModelArg.empty()) {
      return;
    }

    FlowModel flowModel = FlowModel::undirected;
    if (!parseFlowModel(parsed.flowModelArg, flowModel)) {
      throw std::runtime_error(io::Str() << "Unrecognized flow model: '" << parsed.flowModelArg << "'");
    }
    config.setFlowModel(flowModel);
  }

  void validateOutputDirectory(const Config& config)
  {
    if (config.haveOutput()) {
      ensureDirectoryExists(config.outDirectory);
    }
    if (config.haveOutput() && !isDirectoryWritable(config.outDirectory))
      throw InfomapError(ExitCode::OutputError, io::Str() << "Can't write to directory '" << config.outDirectory << "'. Check that the directory exists and that you have write permissions.");
  }

  void initializeLogging(const Config& config)
  {
    Log::init(config.verbosity, config.silent, config.verboseNumberPrecision, config.prettyOutput);
  }

  void buildConfigFromFlags(Config& config, const std::string& flags, bool isCLI)
  {
    config.parsedString = flags;

    ProgramInterface api("Infomap",
                         "Implementation of the Infomap clustering algorithm based on the Map Equation (see www.mapequation.org)",
                         INFOMAP_VERSION);

    api.setGroups({ "Input", "Algorithm", "Accuracy", "Output" });
    api.setJsonParametersProvider(parameterCatalogJson);

    ParsedParameterSet staging;
    registerCatalogWithProgramInterface(api, { config, staging }, isCLI);

    api.parseArgs(flags);
    config.parsedOptions = api.getUsedOptionArguments();

    rejectDeprecatedAliases(staging);
    applyOutputDirectory(config, staging);
    applyFlowModelSelection(config, staging);

    config.adaptDefaults();

    validateOutputDirectory(config);
    initializeLogging(config);
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
  buildConfigFromFlags(*this, flags, isCLI);
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

  // Cross-field invariants. These run whether construction was via flag parsing
  // or library mutation followed by adaptDefaults().
  applyFingerprintOnlyOutputInteraction(*this);
  applyLibraryOutputDefaults(*this);
  validateRequiredCliOutput(*this);
  applyOptionInteractions(*this);
  validateRunReportOutput(*this);
  normalizeOutputDirectory(*this);
  applyOutputNameDefault(*this);
  applyRuntimeOutputInteractions(*this);
}

std::ostream& operator<<(std::ostream& out, FlowModel f)
{
  return out << flowModelToString(f);
}

} // namespace infomap
