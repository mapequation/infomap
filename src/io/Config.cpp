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
#include "../utils/format.h"
#include <algorithm>
#include <cstddef>
#include <iterator>
#include <limits>
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

    // --converge reinterprets numTrials as a cap. numTrials has min=1, so a value
    // of 1 is the unspecified/default sentinel; treat it as "no explicit -N" and
    // use the default cap (a single-trial cap would make --converge a no-op).
    if (config.convergeTrials && config.numTrials == 1 && !config.noInfomap) {
      config.numTrials = Config::convergeDefaultMaxTrials;
    }
  }

  void validateConvergeTrials(const Config& config)
  {
    if (!config.convergeTrials) {
      return;
    }
    if (config.parallelTrials) {
      throw std::runtime_error("--converge cannot be combined with --parallel-trials (auto-convergence needs each trial's result before deciding to continue)");
    }
    if (config.trialOffset > 0 || !config.trialResultsPath.empty()) {
      throw std::runtime_error("--converge cannot be combined with distributed sharding (--trial-offset / --trial-results); independent shards cannot coordinate a stop");
    }
  }

#if INFOMAP_FEATURE_LOSSY_MAP_EQUATION
  void applyAndValidateLossyInteraction(Config& config)
  {
    if (!config.lossy)
      return;

    if (config.directed || (config.flowModelIsSet && config.flowModel != FlowModel::undirected))
      throw std::runtime_error("--lossy requires undirected flow");
    if (config.stateInput || config.multilayerInput || !config.additionalInput.empty())
      throw std::runtime_error("--lossy does not support memory or multilayer networks");
    if (config.haveMetaData())
      throw std::runtime_error("--lossy does not support meta data");
    if (config.recordedTeleportation || config.regularized)
      throw std::runtime_error("--lossy does not support recorded teleportation or regularization");
    if (config.variableMarkovTime || config.markovTime != 1.0)
      throw std::runtime_error("--lossy does not support Markov time scaling");

    config.twoLevel = true;
  }
#endif

  void applyAndValidateNonRedundantInteraction(Config& config)
  {
    if (!config.nonRedundant)
      return;

    // Default the hierarchical search to the "keep super structure" mode for L*.
    // The full-recursion default (fastHierarchicalSolution == 0) removes the bottom-up
    // super-module structure and re-optimizes each top module in isolation. That suits
    // standard L (modules are independent given their boundary flow) but not L*, whose
    // leave-one-out exit codebook couples siblings: the isolated rebuild loses that
    // cross-module context and settles in a worse basin -- it can even end above the
    // super-module structure it started from. Keeping that structure and refining
    // downward (fastHierarchicalSolution == 1) gives a lower L* and is ~3x faster on
    // large networks. Only default it in (unless the user asked for more super levels
    // via -F / -FF); two-level runs have no hierarchy so leave them untouched. Standard
    // L never reaches this code path, so its tuned default is unaffected.
    if (!config.twoLevel && config.fastHierarchicalSolution == 0)
      config.fastHierarchicalSolution = 1;

    // Directed flow is supported: the enter and exit codebooks use the module enter
    // and exit rates separately, so enter != exit is handled directly.
    //
    // Meta data is supported: MetaMapEquation reuses the base codebook terms (running
    // the non-redundant path) and only adds an orthogonal, additive meta-data term.
    //
    // Memory/state and multilayer are NOT supported: MemMapEquation /
    // RegularizedMultilayerMapEquation recompute the enter/exit/flow codebook terms with
    // physical-node (and layer-teleport) corrections and bypass the base per-module
    // methods, so the non-redundant codebook would have to be re-derived inside them.
    if (config.stateInput || config.multilayerInput || !config.additionalInput.empty())
      throw std::runtime_error("--non-redundant does not support memory or multilayer networks");
    // Recorded teleportation and its Bayesian-prior form (--regularized) are supported.
    // Intra-module teleportation is excluded from a module's boundary (enter/exit) flow
    // (verified: a single all-network module has zero enter/exit under both), so a module
    // exit still always crosses to a different module and the leave-one-out exit codebook
    // stays valid. Regularization only changes the flow estimates, which flow through the
    // enter/exit/flow statistics L* already consumes. See the "Regularized non-redundant
    // map equation" notebook for the derivation.
  }

  void applyFingerprintOnlyOutputInteraction(Config& config)
  {
    if (config.printConfigFingerprint) {
      config.noFileOutput = true;
    }
  }

  void applyRuntimeOutputInteractions(Config& config)
  {
    // Pretty is the only console rendering and is always on. --pretty/--no-pretty
    // are deprecated no-ops kept for backward compatibility; neutralize them here.
    config.prettyOutput = true;

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

  void applyThreadBudgetInteraction(Config& config)
  {
    if (config.numThreadsArg.empty() || config.numThreadsArg == "auto") {
      config.numThreads = 0;
      return;
    }
    const auto invalid = []() {
      throw std::runtime_error("--num-threads must be 'auto' or a positive integer");
    };
    long value = 0;
    try {
      std::size_t consumed = 0;
      value = std::stol(config.numThreadsArg, &consumed);
      if (consumed != config.numThreadsArg.size()) {
        invalid(); // trailing garbage like "4x"
      }
    } catch (const std::invalid_argument&) {
      invalid();
    } catch (const std::out_of_range&) {
      invalid();
    }
    if (value < 1 || static_cast<unsigned long>(value) > std::numeric_limits<unsigned int>::max()) {
      invalid();
    }
    config.numThreads = static_cast<unsigned int>(value);
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
      throw std::runtime_error(fmt::format(FMT_STRING("Unrecognized flow model: '{}'"), parsed.flowModelArg));
    }
    config.setFlowModel(flowModel);
  }

  void validateOutputDirectory(const Config& config)
  {
    if (config.haveOutput()) {
      ensureDirectoryExists(config.outDirectory);
    }
    if (config.haveOutput() && !isDirectoryWritable(config.outDirectory))
      throw InfomapError(ExitCode::OutputError, fmt::format(FMT_STRING("Can't write to directory '{}'. Check that the directory exists and that you have write permissions."), config.outDirectory));
  }

  void initializeLogging(const Config& config)
  {
    Log::init(config.verbosity, config.silent, config.verboseNumberPrecision);
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
    const auto& mappings = flowModelMappings();
    std::vector<std::string> values;
    values.reserve(mappings.size());
    std::transform(mappings.begin(), mappings.end(), std::back_inserter(values), [](const auto& mapping) { return mapping.first; });
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
      throw std::runtime_error(fmt::format(FMT_STRING("Unrecognized output format: '{}'."), o));
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
  validateConvergeTrials(*this);
#if INFOMAP_FEATURE_LOSSY_MAP_EQUATION
  applyAndValidateLossyInteraction(*this);
#endif
  applyAndValidateNonRedundantInteraction(*this);
  applyThreadBudgetInteraction(*this);
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
