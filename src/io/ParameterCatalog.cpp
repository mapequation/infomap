/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "ParameterCatalog.h"
#include "Config.h"
#include "OutputFormats.h"
#include "ProgramInterface.h"
#include "../utils/convert.h"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace infomap {

namespace {

  ParameterSpec spec(ParameterId id,
                     char shortName,
                     std::string longName,
                     std::string description,
                     std::string argumentName,
                     std::string group,
                     bool isAdvanced = false,
                     bool hidden = false,
                     bool requireArgument = false,
                     bool incrementalArgument = false,
                     std::string defaultValue = "",
                     std::vector<std::string> choices = {},
                     std::string pythonName = "",
                     std::string rName = "",
                     std::string tsName = "",
                     std::string renderPolicy = "")
  {
    ParameterSpec parameter;
    parameter.id = id;
    parameter.shortName = shortName;
    parameter.longName = std::move(longName);
    parameter.description = std::move(description);
    parameter.argumentName = std::move(argumentName);
    parameter.group = std::move(group);
    parameter.isAdvanced = isAdvanced;
    parameter.hidden = hidden;
    parameter.requireArgument = requireArgument;
    parameter.incrementalArgument = incrementalArgument;
    parameter.defaultValue = std::move(defaultValue);
    parameter.choices = std::move(choices);
    parameter.pythonName = std::move(pythonName);
    parameter.rName = std::move(rName);
    parameter.tsName = std::move(tsName);
    parameter.renderPolicy = std::move(renderPolicy);
    return parameter;
  }

  ParameterSpec withBindingDefaults(ParameterSpec parameter,
                                    std::string pythonDefault,
                                    std::string pythonDefaultConstant = "",
                                    std::string rDefault = "",
                                    std::string rDefaultConstantValue = "")
  {
    parameter.pythonDefault = std::move(pythonDefault);
    parameter.pythonDefaultConstant = std::move(pythonDefaultConstant);
    parameter.rDefault = std::move(rDefault);
    parameter.rDefaultConstantValue = std::move(rDefaultConstantValue);
    return parameter;
  }

  ParameterSpec withMin(ParameterSpec parameter, std::string minValue)
  {
    parameter.minValue = std::move(minValue);
    return parameter;
  }

  ParameterSpec withRange(ParameterSpec parameter, std::string minValue, std::string maxValue)
  {
    parameter.minValue = std::move(minValue);
    parameter.maxValue = std::move(maxValue);
    return parameter;
  }

  ParameterSpec withPythonDocDescription(ParameterSpec parameter, std::string description)
  {
    parameter.pythonDocDescription = std::move(description);
    return parameter;
  }

  std::string jsonString(const std::string& value)
  {
    std::ostringstream escaped;
    escaped << '"';
    for (char c : value) {
      if (c == '"' || c == '\\') {
        escaped << '\\';
      }
      escaped << c;
    }
    escaped << '"';
    return escaped.str();
  }

  std::string stringDefault(const ParameterSpec& parameter)
  {
    return parameter.requireArgument ? parameter.defaultValue : "";
  }

  Option& addConfiguredOption(Option& option, const ParameterSpec& parameter)
  {
    if (parameter.hidden)
      option.setHidden(true);
    if (!parameter.choices.empty())
      option.setChoices(parameter.choices);
    return option;
  }

  void registerBoolOption(ProgramInterface& api, bool& target, const ParameterSpec& parameter)
  {
    if (parameter.shortName == '\0') {
      addConfiguredOption(api.addOptionArgument(target, parameter.longName, parameter.description, parameter.group, parameter.isAdvanced), parameter);
    } else {
      addConfiguredOption(api.addOptionArgument(target, parameter.shortName, parameter.longName, parameter.description, parameter.group, parameter.isAdvanced), parameter);
    }
  }

  template <typename T>
  void registerValueOption(ProgramInterface& api, T& target, const ParameterSpec& parameter)
  {
    if (parameter.shortName == '\0') {
      addConfiguredOption(api.addOptionArgument(target, parameter.longName, parameter.description, parameter.argumentName, parameter.group, parameter.isAdvanced), parameter);
    } else {
      addConfiguredOption(api.addOptionArgument(target, parameter.shortName, parameter.longName, parameter.description, parameter.argumentName, parameter.group, parameter.isAdvanced), parameter);
    }
  }

  template <typename T>
  T numericConstraint(const ParameterSpec& parameter, const std::string& value, const std::string& name)
  {
    T constraint;
    if (value.empty() || !io::stringToValue(value, constraint)) {
      throw std::runtime_error(io::Str() << "Invalid " << name << " for parameter --" << parameter.longName << ".");
    }
    return constraint;
  }

  template <typename T>
  void registerLowerBoundOption(ProgramInterface& api, T& target, const ParameterSpec& parameter)
  {
    const auto minValue = numericConstraint<T>(parameter, parameter.minValue, "minValue");
    if (parameter.shortName == '\0') {
      addConfiguredOption(api.addOptionArgument(target, parameter.longName, parameter.description, parameter.argumentName, parameter.group, minValue, parameter.isAdvanced), parameter);
    } else {
      addConfiguredOption(api.addOptionArgument(target, parameter.shortName, parameter.longName, parameter.description, parameter.argumentName, parameter.group, minValue, parameter.isAdvanced), parameter);
    }
  }

  template <typename T>
  void registerRangeOption(ProgramInterface& api, T& target, const ParameterSpec& parameter)
  {
    const auto minValue = numericConstraint<T>(parameter, parameter.minValue, "minValue");
    const auto maxValue = numericConstraint<T>(parameter, parameter.maxValue, "maxValue");
    if (parameter.shortName == '\0') {
      addConfiguredOption(api.addOptionArgument(target, parameter.longName, parameter.description, parameter.argumentName, parameter.group, minValue, maxValue, parameter.isAdvanced), parameter);
    } else {
      addConfiguredOption(api.addOptionArgument(target, parameter.shortName, parameter.longName, parameter.description, parameter.argumentName, parameter.group, minValue, maxValue, parameter.isAdvanced), parameter);
    }
  }

  void registerConfigParameter(ProgramInterface& api, ConfigParameterTargets& targets, const ParameterSpec& parameter)
  {
    Config& config = targets.config;
    switch (parameter.id) {
    case ParameterId::NetworkFile:
      api.addNonOptionArgument(config.networkFile, "network_file", parameter.description, parameter.group);
      break;
    case ParameterId::Input:
      registerValueOption(api, config.networkFile, parameter);
      break;
    case ParameterId::SkipAdjustBipartiteFlow:
      registerBoolOption(api, config.skipAdjustBipartiteFlow, parameter);
      break;
    case ParameterId::BipartiteTeleportation:
      registerBoolOption(api, config.bipartiteTeleportation, parameter);
      break;
    case ParameterId::WeightThreshold:
      registerLowerBoundOption(api, config.weightThreshold, parameter);
      break;
    case ParameterId::IncludeSelfLinks:
      registerBoolOption(api, targets.deprecatedIncludeSelfLinks, parameter);
      break;
    case ParameterId::NoSelfLinks:
      registerBoolOption(api, config.noSelfLinks, parameter);
      break;
    case ParameterId::NodeLimit:
      registerLowerBoundOption(api, config.nodeLimit, parameter);
      break;
    case ParameterId::MatchableMultilayerIds:
      registerLowerBoundOption(api, config.matchableMultilayerIds, parameter);
      break;
    case ParameterId::ClusterData:
      registerValueOption(api, config.clusterDataFile, parameter);
      break;
    case ParameterId::AssignToNeighbouringModule:
      registerBoolOption(api, config.assignToNeighbouringModule, parameter);
      break;
    case ParameterId::MetaData:
      registerValueOption(api, config.metaDataFile, parameter);
      break;
    case ParameterId::MetaDataRate:
      registerLowerBoundOption(api, config.metaDataRate, parameter);
      break;
    case ParameterId::MetaDataUnweighted:
      registerBoolOption(api, config.unweightedMetaData, parameter);
      break;
    case ParameterId::NoInfomap:
      registerBoolOption(api, config.noInfomap, parameter);
      break;
    case ParameterId::OutName:
      registerValueOption(api, config.outName, parameter);
      break;
    case ParameterId::NoFileOutput:
      registerBoolOption(api, config.noFileOutput, parameter);
      break;
    case ParameterId::Tree:
      registerBoolOption(api, config.printTree, parameter);
      break;
    case ParameterId::FlowTree:
      registerBoolOption(api, config.printFlowTree, parameter);
      break;
    case ParameterId::Clu:
      registerBoolOption(api, config.printClu, parameter);
      break;
    case ParameterId::CluLevel:
      registerLowerBoundOption(api, config.cluLevel, parameter);
      break;
    case ParameterId::Output:
      registerValueOption(api, config.outputFormats, parameter);
      break;
    case ParameterId::HideBipartiteNodes:
      registerBoolOption(api, config.hideBipartiteNodes, parameter);
      break;
    case ParameterId::PrintAllTrials:
      registerBoolOption(api, config.printAllTrials, parameter);
      break;
    case ParameterId::TwoLevel:
      registerBoolOption(api, config.twoLevel, parameter);
      break;
    case ParameterId::FlowModel:
      registerValueOption(api, targets.flowModelArg, parameter);
      break;
    case ParameterId::Directed:
      registerBoolOption(api, config.directed, parameter);
      break;
    case ParameterId::RecordedTeleportation:
      registerBoolOption(api, config.recordedTeleportation, parameter);
      break;
    case ParameterId::UseNodeWeightsAsFlow:
      registerBoolOption(api, config.useNodeWeightsAsFlow, parameter);
      break;
    case ParameterId::TeleportToNodes:
      registerBoolOption(api, config.teleportToNodes, parameter);
      break;
    case ParameterId::TeleportationProbability:
      registerRangeOption(api, config.teleportationProbability, parameter);
      break;
    case ParameterId::Regularized:
      registerBoolOption(api, config.regularized, parameter);
      break;
    case ParameterId::RegularizationStrength:
      registerLowerBoundOption(api, config.regularizationStrength, parameter);
      break;
    case ParameterId::EntropyCorrected:
      registerBoolOption(api, config.entropyBiasCorrection, parameter);
      break;
    case ParameterId::EntropyCorrectionStrength:
      registerValueOption(api, config.entropyBiasCorrectionMultiplier, parameter);
      break;
    case ParameterId::MarkovTime:
      registerLowerBoundOption(api, config.markovTime, parameter);
      break;
    case ParameterId::VariableMarkovTime:
      registerBoolOption(api, config.variableMarkovTime, parameter);
      break;
    case ParameterId::VariableMarkovDamping:
      registerValueOption(api, config.variableMarkovTimeDamping, parameter);
      break;
    case ParameterId::VariableMarkovMinScale:
      registerValueOption(api, config.variableMarkovTimeMinLocalScale, parameter);
      break;
    case ParameterId::PreferredNumberOfModules:
      registerLowerBoundOption(api, config.preferredNumberOfModules, parameter);
      break;
    case ParameterId::MultilayerRelaxRate:
      registerRangeOption(api, config.multilayerRelaxRate, parameter);
      break;
    case ParameterId::MultilayerRelaxLimit:
      registerValueOption(api, config.multilayerRelaxLimit, parameter);
      break;
    case ParameterId::MultilayerRelaxLimitUp:
      registerValueOption(api, config.multilayerRelaxLimitUp, parameter);
      break;
    case ParameterId::MultilayerRelaxLimitDown:
      registerValueOption(api, config.multilayerRelaxLimitDown, parameter);
      break;
    case ParameterId::MultilayerRelaxByJsd:
      registerBoolOption(api, config.multilayerRelaxByJensenShannonDivergence, parameter);
      break;
    case ParameterId::Seed:
      registerLowerBoundOption(api, config.seedToRandomNumberGenerator, parameter);
      break;
    case ParameterId::NumTrials:
      registerLowerBoundOption(api, config.numTrials, parameter);
      break;
    case ParameterId::CoreLoopLimit:
      registerLowerBoundOption(api, config.coreLoopLimit, parameter);
      break;
    case ParameterId::CoreLevelLimit:
      registerLowerBoundOption(api, config.levelAggregationLimit, parameter);
      break;
    case ParameterId::TuneIterationLimit:
      registerLowerBoundOption(api, config.tuneIterationLimit, parameter);
      break;
    case ParameterId::CoreLoopCodelengthThreshold:
      registerLowerBoundOption(api, config.minimumCodelengthImprovement, parameter);
      break;
    case ParameterId::TuneIterationRelativeThreshold:
      registerLowerBoundOption(api, config.minimumRelativeTuneIterationImprovement, parameter);
      break;
    case ParameterId::FastHierarchicalSolution:
      addConfiguredOption(api.addIncrementalOptionArgument(config.fastHierarchicalSolution, parameter.shortName, parameter.longName, parameter.description, parameter.group, parameter.isAdvanced), parameter);
      break;
    case ParameterId::InnerParallelization:
      registerBoolOption(api, config.innerParallelization, parameter);
      break;
    case ParameterId::PreferModularSolution:
      registerBoolOption(api, config.preferModularSolution, parameter);
      break;
    case ParameterId::NumRandomMoves:
      registerLowerBoundOption(api, config.numRandomMoves, parameter);
      break;
    case ParameterId::MaxDegreeForRandomMoves:
      registerLowerBoundOption(api, config.maxDegreeForRandomMoves, parameter);
      break;
    case ParameterId::OutputDirectory:
      api.addOptionalNonOptionArguments(targets.optionalOutputDir, "out_directory", parameter.description, parameter.group);
      break;
    case ParameterId::Verbose:
      addConfiguredOption(api.addIncrementalOptionArgument(config.verbosity, parameter.shortName, parameter.longName, parameter.description, parameter.group), parameter);
      break;
    case ParameterId::Silent:
      registerBoolOption(api, config.silent, parameter);
      break;
    case ParameterId::Pretty:
      registerBoolOption(api, config.prettyOutput, parameter);
      break;
    default:
      break;
    }
  }

} // namespace

const std::vector<ParameterSpec>& parameterCatalog()
{
  static const std::vector<ParameterSpec> parameters = {
    spec(ParameterId::Help, 'h', "help", "Prints this help message. Use -hh to show advanced options.", "", "About", false, false, false, true, "false", {}, "", "", "", "repeated_short"),
    spec(ParameterId::Version, 'V', "version", "Display program version information.", "", "About", false, false, false, false, "false"),
    spec(ParameterId::Completion, '\0', "completion", "Print shell completion script for bash or zsh.", ArgType::option, "About", false, false, true, false, "", { "bash", "zsh" }),
    spec(ParameterId::NetworkFile, '\0', "network_file", "Network file to read. Infomap assumes link-list format unless the file starts with a Pajek heading.", "", "Input"),
    spec(ParameterId::Input, '\0', "input", "Network file to read. Infomap assumes link-list format unless the file starts with a Pajek heading.", ArgType::path, "Input", false, false, true),
    spec(ParameterId::SkipAdjustBipartiteFlow, '\0', "skip-adjust-bipartite-flow", "Keep flow on bipartite nodes instead of distributing it to primary nodes.", "", "Input", true),
    spec(ParameterId::BipartiteTeleportation, '\0', "bipartite-teleportation", "Use bipartite teleportation instead of the default two-step unipartite teleportation.", "", "Input", true),
    withMin(spec(ParameterId::WeightThreshold, '\0', "weight-threshold", "Ignore input links with weight below this threshold.", ArgType::number, "Input", true, false, true, false, "0"), "0"),
    spec(ParameterId::IncludeSelfLinks, 'k', "include-self-links", "DEPRECATED. Self-links are included by default; use --no-self-links to exclude them.", "", "Input", true, true),
    spec(ParameterId::NoSelfLinks, '\0', "no-self-links", "Exclude self-links from the input network.", "", "Input", true, false, false, false, "false", {}, "", "", "", "deprecated_alias_target"),
    withMin(spec(ParameterId::NodeLimit, '\0', "node-limit", "Read only nodes up to this node id and ignore links connected to higher node ids.", ArgType::integer, "Input", true, false, true, false, "0"), "1"),
    withMin(spec(ParameterId::MatchableMultilayerIds, '\0', "matchable-multilayer-ids", "Construct state ids from node ids and layer ids that stay comparable across networks. Set at least to the largest layer id among networks to match.", ArgType::integer, "Input", true, false, true, false, "0"), "1"),
    spec(ParameterId::ClusterData, 'c', "cluster-data", "Read an initial partition from a clu file or a hierarchy from a tree/ftree file. Tree input may use physical or state nodes for higher-order networks.", ArgType::path, "Input", false, false, true),
    spec(ParameterId::AssignToNeighbouringModule, '\0', "assign-to-neighbouring-module", "With --cluster-data, assign nodes missing module ids to a neighboring node's module when possible.", "", "Input", true),
    spec(ParameterId::MetaData, '\0', "meta-data", "Read metadata to encode from a clu-format file.", ArgType::path, "Input", true, false, true),
    withBindingDefaults(withMin(spec(ParameterId::MetaDataRate, '\0', "meta-data-rate", "With --meta-data, set the metadata encoding rate. The default encodes metadata at each step.", ArgType::number, "Input", true, false, true, false, "1"), "0"), "1.0", "_DEFAULT_META_DATA_RATE", "DEFAULT_META_DATA_RATE", "1.0"),
    spec(ParameterId::MetaDataUnweighted, '\0', "meta-data-unweighted", "With --meta-data, encode metadata without weighting by node flow.", "", "Input", true),
    spec(ParameterId::NoInfomap, '\0', "no-infomap", "Skip optimization. Use this to calculate codelength for --cluster-data or to print non-modular statistics.", "", "Input"),
    spec(ParameterId::OutName, '\0', "out-name", "Base name for output files, for example [out_directory]/[out-name].tree.", ArgType::string, "Output", true, false, true),
    spec(ParameterId::NoFileOutput, '0', "no-file-output", "Do not write output files.", "", "Output", true),
    spec(ParameterId::Tree, '\0', "tree", "Write the modular hierarchy to a tree file. Enabled by default when no other output format is selected.", "", "Output"),
    spec(ParameterId::FlowTree, '\0', "ftree", "Write the modular hierarchy and aggregated links between nested modules to an ftree file. Used by Network Navigator.", "", "Output"),
    spec(ParameterId::Clu, '\0', "clu", "Write top-level module ids for each node to a clu file.", "", "Output"),
    withMin(spec(ParameterId::CluLevel, '\0', "clu-level", "With --clu or --output clu, write module ids at this depth from the root. Use -1 for bottom-level modules.", ArgType::integer, "Output", true, false, true, false, "1"), "-1"),
    spec(ParameterId::Output, 'o', "output", "Write selected output formats as a comma-separated list without spaces, e.g. -o clu,tree,ftree. Options: clu, tree, ftree, newick, json, csv, network, states, flow.", ArgType::list, "Output", true, false, true, false, "", outputFormatNames(), "", "", "", "comma_list"),
    spec(ParameterId::HideBipartiteNodes, '\0', "hide-bipartite-nodes", "Hide bipartite nodes in output by projecting the solution to primary nodes.", "", "Output", true),
    spec(ParameterId::PrintAllTrials, '\0', "print-all-trials", "Write each trial to separate output files. Has effect only when --num-trials is greater than 1.", "", "Output", true),
    spec(ParameterId::TwoLevel, '2', "two-level", "Optimize a two-level partition instead of the default multi-level hierarchy.", "", "Algorithm"),
    spec(ParameterId::FlowModel, 'f', "flow-model", "Choose how Infomap derives flow from the input links. Options: undirected, directed, undirdir, outdirdir, rawdir, precomputed.", ArgType::option, "Algorithm", false, false, true, false, "", { "undirected", "directed", "undirdir", "outdirdir", "rawdir", "precomputed" }),
    withBindingDefaults(spec(ParameterId::Directed, 'd', "directed", "Treat input links as directed. Shorthand for --flow-model directed.", "", "Algorithm", false, false, false, false, "false", {}, "", "", "", "directed_alias"), "None", "", "NULL"),
    spec(ParameterId::RecordedTeleportation, 'e', "recorded-teleportation", "When teleportation is used to calculate flow, also record teleportation steps in the codelength.", "", "Algorithm", true),
    spec(ParameterId::UseNodeWeightsAsFlow, '\0', "use-node-weights-as-flow", "Use node weights from the API or Pajek node records as normalized node flow.", "", "Algorithm", true),
    spec(ParameterId::TeleportToNodes, '\0', "to-nodes", "Teleport to nodes instead of links. Uses uniform node weights unless node weights are provided.", "", "Algorithm", true),
    withBindingDefaults(withRange(spec(ParameterId::TeleportationProbability, 'p', "teleportation-probability", "Set the probability of teleporting to a random node or link when calculating flow.", ArgType::probability, "Algorithm", true, false, true, false, "0.15"), "0", "1"), "0.15", "_DEFAULT_TELEPORTATION_PROB", "DEFAULT_TELEPORTATION_PROB", "0.15"),
    spec(ParameterId::Regularized, '\0', "regularized", "Add a fully connected Bayesian prior network to reduce overfitting to missing links. Activates --recorded-teleportation.", "", "Algorithm", true),
    withBindingDefaults(withMin(spec(ParameterId::RegularizationStrength, '\0', "regularization-strength", "Scale the relative strength of the Bayesian prior network used by --regularized.", ArgType::number, "Algorithm", true, false, true, false, "1"), "0"), "1.0", "", "1.0"),
    spec(ParameterId::EntropyCorrected, '\0', "entropy-corrected", "Correct for negative entropy bias in small samples, especially solutions with many modules.", "", "Algorithm", true),
    withBindingDefaults(spec(ParameterId::EntropyCorrectionStrength, '\0', "entropy-correction-strength", "Scale the default correction used by --entropy-corrected.", ArgType::number, "Algorithm", true, false, true, false, "1"), "1.0", "", "1.0"),
    withBindingDefaults(withMin(spec(ParameterId::MarkovTime, '\0', "markov-time", "Scale link flow to change the cost of moving between modules. Higher values result in fewer modules.", ArgType::number, "Algorithm", true, false, true, false, "1"), "0"), "1.0", "", "1.0"),
    spec(ParameterId::VariableMarkovTime, '\0', "variable-markov-time", "Vary Markov time locally to reduce overpartitioning in sparse areas while keeping higher resolution in dense areas.", "", "Algorithm", true),
    withBindingDefaults(spec(ParameterId::VariableMarkovDamping, '\0', "variable-markov-damping", "With --variable-markov-time, set damping between local effective degree (0) and local entropy (1).", ArgType::number, "Algorithm", true, false, true, false, "1"), "1.0", "", "1.0"),
    withBindingDefaults(spec(ParameterId::VariableMarkovMinScale, '\0', "variable-markov-min-scale", "With --variable-markov-time, set the minimum local scale for zero-entropy nodes. Local Markov time is max scale divided by local scale.", ArgType::number, "Algorithm", true, false, true, false, "1"), "1.0", "", "1.0"),
    withMin(spec(ParameterId::PreferredNumberOfModules, '\0', "preferred-number-of-modules", "Penalize solutions by how far their number of modules differs from this value.", ArgType::integer, "Algorithm", true, false, true, false, "0"), "1"),
    withBindingDefaults(withRange(spec(ParameterId::MultilayerRelaxRate, '\0', "multilayer-relax-rate", "Set the probability of relaxing from a state node to neighboring layers instead of staying in the current layer.", ArgType::probability, "Algorithm", true, false, true, false, "0.15"), "0", "1"), "0.15", "_DEFAULT_MULTILAYER_RELAX_RATE", "DEFAULT_MULTILAYER_RELAX_RATE", "0.15"),
    withBindingDefaults(spec(ParameterId::MultilayerRelaxLimit, '\0', "multilayer-relax-limit", "Limit relaxation to this many neighboring layer ids in each direction. Use a negative value to allow relaxation to any layer.", ArgType::integer, "Algorithm", true, false, true, false, "-1"), "-1", "", "NULL"),
    withBindingDefaults(spec(ParameterId::MultilayerRelaxLimitUp, '\0', "multilayer-relax-limit-up", "Limit relaxation upward to this many higher neighboring layer ids. Use a negative value to allow relaxation to any higher layer.", ArgType::integer, "Algorithm", true, false, true, false, "-1"), "-1", "", "NULL"),
    withBindingDefaults(spec(ParameterId::MultilayerRelaxLimitDown, '\0', "multilayer-relax-limit-down", "Limit relaxation downward to this many lower neighboring layer ids. Use a negative value to allow relaxation to any lower layer.", ArgType::integer, "Algorithm", true, false, true, false, "-1"), "-1", "", "NULL"),
    spec(ParameterId::MultilayerRelaxByJsd, '\0', "multilayer-relax-by-jsd", "Weight multilayer relaxation by out-link similarity measured with Jensen-Shannon divergence.", "", "Algorithm", true),
    withBindingDefaults(withMin(spec(ParameterId::Seed, 's', "seed", "Set the random number generator seed for reproducible results.", ArgType::integer, "Accuracy", false, false, true, false, "123"), "1"), "123", "_DEFAULT_SEED", "DEFAULT_SEED", "123L"),
    withBindingDefaults(withMin(spec(ParameterId::NumTrials, 'N', "num-trials", "Run this many independent trials and keep the best solution.", ArgType::integer, "Accuracy", false, false, true, false, "1"), "1"), "1", "", "1L"),
    withBindingDefaults(withMin(spec(ParameterId::CoreLoopLimit, 'M', "core-loop-limit", "Limit how many core loops try to move each node to the best module.", ArgType::integer, "Accuracy", true, false, true, false, "10"), "1"), "10", "", "10L"),
    withMin(spec(ParameterId::CoreLevelLimit, 'L', "core-level-limit", "Limit how many times core loops are reapplied to the aggregated modular network to find larger structures.", ArgType::integer, "Accuracy", true, false, true, false, "0"), "1"),
    withMin(spec(ParameterId::TuneIterationLimit, 'T', "tune-iteration-limit", "Limit the main iterations in the two-level partition algorithm. 0 means no limit.", ArgType::integer, "Accuracy", true, false, true, false, "0"), "1"),
    withBindingDefaults(withMin(spec(ParameterId::CoreLoopCodelengthThreshold, '\0', "core-loop-codelength-threshold", "Require at least this codelength improvement to accept a new solution in a core loop.", ArgType::number, "Accuracy", true, false, true, false, "1e-10"), "0"), "1e-10", "_DEFAULT_CORE_LOOP_CODELENGTH_THRESHOLD", "DEFAULT_CORE_LOOP_CODELENGTH_THRESHOLD", "1e-10"),
    withBindingDefaults(withMin(spec(ParameterId::TuneIterationRelativeThreshold, '\0', "tune-iteration-relative-threshold", "Require each tune iteration to improve codelength by this fraction of the initial two-level codelength.", ArgType::number, "Accuracy", true, false, true, false, "1e-05"), "0"), "1e-5", "_DEFAULT_TUNE_ITER_RELATIVE_THRESHOLD", "DEFAULT_TUNE_ITER_RELATIVE_THRESHOLD", "1e-5"),
    withPythonDocDescription(withBindingDefaults(spec(ParameterId::FastHierarchicalSolution, 'F', "fast-hierarchical-solution", "Find top modules quickly. Use -FF to keep all fast levels. Use -FFF to skip recursive refinement.", "", "Accuracy", true, false, false, true, "false", {}, "", "", "", "repeated_short"), "None", "", "NULL"), "Find top modules fast. Use 2 to keep all fast levels and 3 to skip the recursive part."),
    spec(ParameterId::InnerParallelization, '\0', "inner-parallelization", "Parallelize the innermost loop for speed, with a possible accuracy tradeoff.", "", "Accuracy", true),
    spec(ParameterId::PreferModularSolution, '\0', "prefer-modular-solution", "Prefer a modular solution even when one module gives a lower codelength.", "", "Accuracy", true),
    withMin(spec(ParameterId::NumRandomMoves, '\0', "num-random-moves", "Try this many random moves in each core loop to merge weakly connected nodes.", ArgType::integer, "Accuracy", true, false, true, false, "5"), "0"),
    withMin(spec(ParameterId::MaxDegreeForRandomMoves, '\0', "max-degree-for-random-moves", "Try random moves only for nodes with degree at most this value.", ArgType::integer, "Accuracy", true, false, true, false, "2"), "0"),
    spec(ParameterId::OutputDirectory, '\0', "out_directory", "Directory where output files are written.", "", "Output"),
    withPythonDocDescription(withBindingDefaults(spec(ParameterId::Verbose, 'v', "verbose", "Increase console verbosity. Add more v flags to increase verbosity up to -vvv.", "", "Output", false, false, false, true, "false", {}, "verbosity_level", "verbosity_level", "verbose", "repeated_short"), "1", "_DEFAULT_VERBOSITY_LEVEL", "DEFAULT_VERBOSITY_LEVEL", "1L"), "Verbosity level on the console. 1 keeps the default output level, 2 renders -vv and so on."),
    spec(ParameterId::Silent, '\0', "silent", "Suppress console output.", "", "Output"),
    spec(ParameterId::Pretty, '\0', "pretty", "Use modernized console output with color and Unicode on interactive terminals.", "", "Output"),
  };
  return parameters;
}

void registerConfigParameters(ProgramInterface& api, ConfigParameterTargets targets, bool isCli)
{
  for (const auto& parameter : parameterCatalog()) {
    if (parameter.group == "About") {
      continue;
    }
    if (isCli && parameter.id == ParameterId::Input) {
      continue;
    }
    if (!isCli && parameter.id == ParameterId::NetworkFile) {
      continue;
    }
    registerConfigParameter(api, targets, parameter);
  }
}

std::string parameterCatalogJson()
{
  std::ostringstream json;
  json << "{\n  \"parameters\": [\n";
  bool firstParameter = true;
  for (const auto& parameter : parameterCatalog()) {
    if (parameter.hidden || parameter.id == ParameterId::Completion || parameter.id == ParameterId::NetworkFile || parameter.id == ParameterId::Input || parameter.id == ParameterId::OutputDirectory) {
      continue;
    }
    if (!firstParameter) {
      json << ",\n";
    }
    firstParameter = false;
    json << "    { "
         << "\"long\": " << jsonString("--" + parameter.longName) << ", "
         << "\"short\": " << jsonString(parameter.shortName == '\0' ? "" : std::string("-") + parameter.shortName) << ", "
         << "\"description\": " << jsonString(parameter.description) << ", "
         << "\"group\": " << jsonString(parameter.group) << ", "
         << "\"required\": " << (parameter.requireArgument ? "true" : "false") << ", "
         << "\"advanced\": " << (parameter.isAdvanced ? "true" : "false") << ", "
         << "\"incremental\": " << (parameter.incrementalArgument ? "true" : "false") << ", ";
    if (parameter.requireArgument) {
      json << "\"longType\": " << jsonString(parameter.argumentName) << ", "
           << "\"shortType\": " << jsonString(std::string(1, ArgType::toShort.at(parameter.argumentName))) << ", "
           << "\"default\": " << jsonString(stringDefault(parameter));
    } else {
      json << "\"default\": false";
    }
    if (!parameter.choices.empty()) {
      json << ", \"choices\": [";
      for (std::size_t i = 0; i < parameter.choices.size(); ++i) {
        if (i > 0) {
          json << ", ";
        }
        json << jsonString(parameter.choices[i]);
      }
      json << "]";
    }
    if (!parameter.minValue.empty()) {
      json << ", \"min\": " << jsonString(parameter.minValue);
    }
    if (!parameter.maxValue.empty()) {
      json << ", \"max\": " << jsonString(parameter.maxValue);
    }
    if (!parameter.pythonName.empty() || !parameter.rName.empty() || !parameter.tsName.empty()) {
      json << ", \"bindingNames\": {";
      bool firstName = true;
      if (!parameter.pythonName.empty()) {
        json << "\"python\": " << jsonString(parameter.pythonName);
        firstName = false;
      }
      if (!parameter.rName.empty()) {
        json << (firstName ? "" : ", ") << "\"r\": " << jsonString(parameter.rName);
        firstName = false;
      }
      if (!parameter.tsName.empty()) {
        json << (firstName ? "" : ", ") << "\"ts\": " << jsonString(parameter.tsName);
      }
      json << "}";
    }
    if (!parameter.renderPolicy.empty()) {
      json << ", \"renderPolicy\": " << jsonString(parameter.renderPolicy);
    }
    if (!parameter.pythonDocDescription.empty()) {
      json << ", \"bindingDocs\": {\"python\": {\"description\": " << jsonString(parameter.pythonDocDescription) << "}}";
    }
    if (!parameter.pythonDefault.empty() || !parameter.rDefault.empty()) {
      json << ", \"bindingDefaults\": {";
      bool firstLanguage = true;
      if (!parameter.pythonDefault.empty()) {
        json << "\"python\": {\"value\": " << jsonString(parameter.pythonDefault);
        if (!parameter.pythonDefaultConstant.empty()) {
          json << ", \"constant\": " << jsonString(parameter.pythonDefaultConstant);
        }
        json << "}";
        firstLanguage = false;
      }
      if (!parameter.rDefault.empty()) {
        json << (firstLanguage ? "" : ", ")
             << "\"r\": {\"value\": " << jsonString(parameter.rDefault);
        if (!parameter.rDefaultConstantValue.empty()) {
          json << ", \"constantValue\": " << jsonString(parameter.rDefaultConstantValue);
        }
        json << "}";
      }
      json << "}";
    }
    json << " }";
  }
  json << "\n  ]\n}";
  return json.str();
}

} // namespace infomap
