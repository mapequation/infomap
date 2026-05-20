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

  bool isFloatingPointArgument(const ParameterSpec& parameter)
  {
    return parameter.argumentName == ArgType::number || parameter.argumentName == ArgType::probability;
  }

  std::string numericBindingDefault(const ParameterSpec& parameter)
  {
    auto value = parameter.defaultValue;
    if (isFloatingPointArgument(parameter) && value.find_first_of(".eE") == std::string::npos) {
      value += ".0";
    }
    return value;
  }

  std::string rBindingDefault(const ParameterSpec& parameter)
  {
    auto value = numericBindingDefault(parameter);
    if (parameter.argumentName == ArgType::integer) {
      value += "L";
    }
    return value;
  }

  class SpecBuilder {
  public:
    explicit SpecBuilder(ParameterId id)
    {
      parameter_.id = id;
    }

    SpecBuilder& shortName(char value)
    {
      parameter_.shortName = value;
      return *this;
    }

    SpecBuilder& longName(std::string value)
    {
      parameter_.longName = std::move(value);
      return *this;
    }

    SpecBuilder& description(std::string value)
    {
      parameter_.description = std::move(value);
      return *this;
    }

    SpecBuilder& group(std::string value)
    {
      parameter_.group = std::move(value);
      return *this;
    }

    SpecBuilder& argument(std::string value)
    {
      parameter_.argumentName = std::move(value);
      parameter_.requireArgument = true;
      return *this;
    }

    SpecBuilder& defaultValue(std::string value)
    {
      parameter_.defaultValue = std::move(value);
      return *this;
    }

    SpecBuilder& advanced()
    {
      parameter_.isAdvanced = true;
      return *this;
    }

    SpecBuilder& hidden()
    {
      parameter_.hidden = true;
      return *this;
    }

    SpecBuilder& incremental()
    {
      parameter_.incrementalArgument = true;
      return *this;
    }

    SpecBuilder& choices(std::vector<std::string> values)
    {
      parameter_.choices = std::move(values);
      return *this;
    }

    SpecBuilder& min(std::string value)
    {
      parameter_.minValue = std::move(value);
      return *this;
    }

    SpecBuilder& range(std::string minValue, std::string maxValue)
    {
      parameter_.minValue = std::move(minValue);
      parameter_.maxValue = std::move(maxValue);
      return *this;
    }

    SpecBuilder& bindingNames(std::string pythonName, std::string rName, std::string tsName)
    {
      parameter_.pythonName = std::move(pythonName);
      parameter_.rName = std::move(rName);
      parameter_.tsName = std::move(tsName);
      return *this;
    }

    SpecBuilder& renderPolicy(std::string value)
    {
      parameter_.renderPolicy = std::move(value);
      return *this;
    }

    SpecBuilder& bindingDefaults()
    {
      parameter_.pythonDefault = numericBindingDefault(parameter_);
      parameter_.rDefault = rBindingDefault(parameter_);
      return *this;
    }

    SpecBuilder& bindingDefaults(std::string pythonDefault, std::string rDefault)
    {
      parameter_.pythonDefault = std::move(pythonDefault);
      parameter_.rDefault = std::move(rDefault);
      return *this;
    }

    SpecBuilder& pythonDoc(std::string value)
    {
      parameter_.pythonDocDescription = std::move(value);
      return *this;
    }

    operator ParameterSpec() const { return parameter_; }

  private:
    ParameterSpec parameter_;
  };

  SpecBuilder param(ParameterId id)
  {
    return SpecBuilder(id);
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
      addConfiguredOption(api.addIncrementalOptionArgument(config.verbosity, parameter.shortName, parameter.longName, parameter.description, parameter.group, parameter.isAdvanced), parameter);
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
    param(ParameterId::Help)
        .shortName('h')
        .longName("help")
        .description("Prints this help message. Use -hh to show advanced options.")
        .group("About")
        .incremental()
        .defaultValue("false")
        .renderPolicy("repeated_short"),
    param(ParameterId::Version)
        .shortName('V')
        .longName("version")
        .description("Display program version information.")
        .group("About")
        .defaultValue("false"),
    param(ParameterId::Completion)
        .longName("completion")
        .description("Print shell completion script for bash or zsh.")
        .argument(ArgType::option)
        .group("About")
        .choices({ "bash", "zsh" }),
    param(ParameterId::NetworkFile)
        .longName("network_file")
        .description("Network file to read. Infomap assumes link-list format unless the file starts with a Pajek heading.")
        .group("Input"),
    param(ParameterId::Input)
        .longName("input")
        .description("Network file to read. Infomap assumes link-list format unless the file starts with a Pajek heading.")
        .argument(ArgType::path)
        .group("Input"),
    param(ParameterId::SkipAdjustBipartiteFlow)
        .longName("skip-adjust-bipartite-flow")
        .description("Keep flow on bipartite nodes instead of distributing it to primary nodes.")
        .group("Input")
        .advanced(),
    param(ParameterId::BipartiteTeleportation)
        .longName("bipartite-teleportation")
        .description("Use bipartite teleportation instead of the default two-step unipartite teleportation.")
        .group("Input")
        .advanced(),
    param(ParameterId::WeightThreshold)
        .longName("weight-threshold")
        .description("Ignore input links with weight below this threshold.")
        .argument(ArgType::number)
        .group("Input")
        .advanced()
        .defaultValue("0")
        .min("0"),
    param(ParameterId::IncludeSelfLinks)
        .shortName('k')
        .longName("include-self-links")
        .description("DEPRECATED. Self-links are included by default; use --no-self-links to exclude them.")
        .group("Input")
        .advanced()
        .hidden(),
    param(ParameterId::NoSelfLinks)
        .longName("no-self-links")
        .description("Exclude self-links from the input network.")
        .group("Input")
        .advanced()
        .defaultValue("false")
        .renderPolicy("deprecated_alias_target"),
    param(ParameterId::NodeLimit)
        .longName("node-limit")
        .description("Read only nodes up to this node id and ignore links connected to higher node ids.")
        .argument(ArgType::integer)
        .group("Input")
        .advanced()
        .defaultValue("0")
        .min("1"),
    param(ParameterId::MatchableMultilayerIds)
        .longName("matchable-multilayer-ids")
        .description("Construct state ids from node ids and layer ids that stay comparable across networks. Set at least to the largest layer id among networks to match.")
        .argument(ArgType::integer)
        .group("Input")
        .advanced()
        .defaultValue("0")
        .min("1"),
    param(ParameterId::ClusterData)
        .shortName('c')
        .longName("cluster-data")
        .description("Read an initial partition from a clu file or a hierarchy from a tree/ftree file. Tree input may use physical or state nodes for higher-order networks.")
        .argument(ArgType::path)
        .group("Input"),
    param(ParameterId::AssignToNeighbouringModule)
        .longName("assign-to-neighbouring-module")
        .description("With --cluster-data, assign nodes missing module ids to a neighboring node's module when possible.")
        .group("Input")
        .advanced(),
    param(ParameterId::MetaData)
        .longName("meta-data")
        .description("Read metadata to encode from a clu-format file.")
        .argument(ArgType::path)
        .group("Input")
        .advanced(),
    param(ParameterId::MetaDataRate)
        .longName("meta-data-rate")
        .description("With --meta-data, set the metadata encoding rate. The default encodes metadata at each step.")
        .argument(ArgType::number)
        .group("Input")
        .advanced()
        .defaultValue("1")
        .min("0")
        .bindingDefaults(),
    param(ParameterId::MetaDataUnweighted)
        .longName("meta-data-unweighted")
        .description("With --meta-data, encode metadata without weighting by node flow.")
        .group("Input")
        .advanced(),
    param(ParameterId::NoInfomap)
        .longName("no-infomap")
        .description("Skip optimization. Use this to calculate codelength for --cluster-data or to print non-modular statistics.")
        .group("Input"),
    param(ParameterId::OutName)
        .longName("out-name")
        .description("Base name for output files, for example [out_directory]/[out-name].tree.")
        .argument(ArgType::string)
        .group("Output")
        .advanced(),
    param(ParameterId::NoFileOutput)
        .shortName('0')
        .longName("no-file-output")
        .description("Do not write output files.")
        .group("Output")
        .advanced(),
    param(ParameterId::Tree)
        .longName("tree")
        .description("Write the modular hierarchy to a tree file. Enabled by default when no other output format is selected.")
        .group("Output"),
    param(ParameterId::FlowTree)
        .longName("ftree")
        .description("Write the modular hierarchy and aggregated links between nested modules to an ftree file. Used by Network Navigator.")
        .group("Output"),
    param(ParameterId::Clu)
        .longName("clu")
        .description("Write top-level module ids for each node to a clu file.")
        .group("Output"),
    param(ParameterId::CluLevel)
        .longName("clu-level")
        .description("With --clu or --output clu, write module ids at this depth from the root. Use -1 for bottom-level modules.")
        .argument(ArgType::integer)
        .group("Output")
        .advanced()
        .defaultValue("1")
        .min("-1"),
    param(ParameterId::Output)
        .shortName('o')
        .longName("output")
        .description("Write selected output formats as a comma-separated list without spaces, e.g. -o clu,tree,ftree. Options: clu, tree, ftree, newick, json, csv, network, states, flow.")
        .argument(ArgType::list)
        .group("Output")
        .advanced()
        .choices(outputFormatNames())
        .renderPolicy("comma_list"),
    param(ParameterId::HideBipartiteNodes)
        .longName("hide-bipartite-nodes")
        .description("Hide bipartite nodes in output by projecting the solution to primary nodes.")
        .group("Output")
        .advanced(),
    param(ParameterId::PrintAllTrials)
        .longName("print-all-trials")
        .description("Write each trial to separate output files. Has effect only when --num-trials is greater than 1.")
        .group("Output")
        .advanced(),
    param(ParameterId::TwoLevel)
        .shortName('2')
        .longName("two-level")
        .description("Optimize a two-level partition instead of the default multi-level hierarchy.")
        .group("Algorithm"),
    param(ParameterId::FlowModel)
        .shortName('f')
        .longName("flow-model")
        .description("Choose how Infomap derives flow from the input links. Options: undirected, directed, undirdir, outdirdir, rawdir, precomputed.")
        .argument(ArgType::option)
        .group("Algorithm")
        .choices({ "undirected", "directed", "undirdir", "outdirdir", "rawdir", "precomputed" }),
    param(ParameterId::Directed)
        .shortName('d')
        .longName("directed")
        .description("Treat input links as directed. Shorthand for --flow-model directed.")
        .group("Algorithm")
        .defaultValue("false")
        .renderPolicy("directed_alias")
        .bindingDefaults("None", "NULL"),
    param(ParameterId::RecordedTeleportation)
        .shortName('e')
        .longName("recorded-teleportation")
        .description("When teleportation is used to calculate flow, also record teleportation steps in the codelength.")
        .group("Algorithm")
        .advanced(),
    param(ParameterId::UseNodeWeightsAsFlow)
        .longName("use-node-weights-as-flow")
        .description("Use node weights from the API or Pajek node records as normalized node flow.")
        .group("Algorithm")
        .advanced(),
    param(ParameterId::TeleportToNodes)
        .longName("to-nodes")
        .description("Teleport to nodes instead of links. Uses uniform node weights unless node weights are provided.")
        .group("Algorithm")
        .advanced(),
    param(ParameterId::TeleportationProbability)
        .shortName('p')
        .longName("teleportation-probability")
        .description("Set the probability of teleporting to a random node or link when calculating flow.")
        .argument(ArgType::probability)
        .group("Algorithm")
        .advanced()
        .defaultValue("0.15")
        .range("0", "1")
        .bindingDefaults(),
    param(ParameterId::Regularized)
        .longName("regularized")
        .description("Add a fully connected Bayesian prior network to reduce overfitting to missing links. Activates --recorded-teleportation.")
        .group("Algorithm")
        .advanced(),
    param(ParameterId::RegularizationStrength)
        .longName("regularization-strength")
        .description("Scale the relative strength of the Bayesian prior network used by --regularized.")
        .argument(ArgType::number)
        .group("Algorithm")
        .advanced()
        .defaultValue("1")
        .min("0")
        .bindingDefaults(),
    param(ParameterId::EntropyCorrected)
        .longName("entropy-corrected")
        .description("Correct for negative entropy bias in small samples, especially solutions with many modules.")
        .group("Algorithm")
        .advanced(),
    param(ParameterId::EntropyCorrectionStrength)
        .longName("entropy-correction-strength")
        .description("Scale the default correction used by --entropy-corrected.")
        .argument(ArgType::number)
        .group("Algorithm")
        .advanced()
        .defaultValue("1")
        .bindingDefaults(),
    param(ParameterId::MarkovTime)
        .longName("markov-time")
        .description("Scale link flow to change the cost of moving between modules. Higher values result in fewer modules.")
        .argument(ArgType::number)
        .group("Algorithm")
        .advanced()
        .defaultValue("1")
        .min("0")
        .bindingDefaults(),
    param(ParameterId::VariableMarkovTime)
        .longName("variable-markov-time")
        .description("Vary Markov time locally to reduce overpartitioning in sparse areas while keeping higher resolution in dense areas.")
        .group("Algorithm")
        .advanced(),
    param(ParameterId::VariableMarkovDamping)
        .longName("variable-markov-damping")
        .description("With --variable-markov-time, set damping between local effective degree (0) and local entropy (1).")
        .argument(ArgType::number)
        .group("Algorithm")
        .advanced()
        .defaultValue("1")
        .bindingDefaults(),
    param(ParameterId::VariableMarkovMinScale)
        .longName("variable-markov-min-scale")
        .description("With --variable-markov-time, set the minimum local scale for zero-entropy nodes. Local Markov time is max scale divided by local scale.")
        .argument(ArgType::number)
        .group("Algorithm")
        .advanced()
        .defaultValue("1")
        .bindingDefaults(),
    param(ParameterId::PreferredNumberOfModules)
        .longName("preferred-number-of-modules")
        .description("Penalize solutions by how far their number of modules differs from this value.")
        .argument(ArgType::integer)
        .group("Algorithm")
        .advanced()
        .defaultValue("0")
        .min("1"),
    param(ParameterId::MultilayerRelaxRate)
        .longName("multilayer-relax-rate")
        .description("Set the probability of relaxing from a state node to neighboring layers instead of staying in the current layer.")
        .argument(ArgType::probability)
        .group("Algorithm")
        .advanced()
        .defaultValue("0.15")
        .range("0", "1")
        .bindingDefaults(),
    param(ParameterId::MultilayerRelaxLimit)
        .longName("multilayer-relax-limit")
        .description("Limit relaxation to this many neighboring layer ids in each direction. Use a negative value to allow relaxation to any layer.")
        .argument(ArgType::integer)
        .group("Algorithm")
        .advanced()
        .defaultValue("-1")
        .bindingDefaults("-1", "NULL"),
    param(ParameterId::MultilayerRelaxLimitUp)
        .longName("multilayer-relax-limit-up")
        .description("Limit relaxation upward to this many higher neighboring layer ids. Use a negative value to allow relaxation to any higher layer.")
        .argument(ArgType::integer)
        .group("Algorithm")
        .advanced()
        .defaultValue("-1")
        .bindingDefaults("-1", "NULL"),
    param(ParameterId::MultilayerRelaxLimitDown)
        .longName("multilayer-relax-limit-down")
        .description("Limit relaxation downward to this many lower neighboring layer ids. Use a negative value to allow relaxation to any lower layer.")
        .argument(ArgType::integer)
        .group("Algorithm")
        .advanced()
        .defaultValue("-1")
        .bindingDefaults("-1", "NULL"),
    param(ParameterId::MultilayerRelaxByJsd)
        .longName("multilayer-relax-by-jsd")
        .description("Weight multilayer relaxation by out-link similarity measured with Jensen-Shannon divergence.")
        .group("Algorithm")
        .advanced(),
    param(ParameterId::Seed)
        .shortName('s')
        .longName("seed")
        .description("Set the random number generator seed for reproducible results.")
        .argument(ArgType::integer)
        .group("Accuracy")
        .defaultValue("123")
        .min("1")
        .bindingDefaults(),
    param(ParameterId::NumTrials)
        .shortName('N')
        .longName("num-trials")
        .description("Run this many independent trials and keep the best solution.")
        .argument(ArgType::integer)
        .group("Accuracy")
        .defaultValue("1")
        .min("1")
        .bindingDefaults(),
    param(ParameterId::CoreLoopLimit)
        .shortName('M')
        .longName("core-loop-limit")
        .description("Limit how many core loops try to move each node to the best module.")
        .argument(ArgType::integer)
        .group("Accuracy")
        .advanced()
        .defaultValue("10")
        .min("1")
        .bindingDefaults(),
    param(ParameterId::CoreLevelLimit)
        .shortName('L')
        .longName("core-level-limit")
        .description("Limit how many times core loops are reapplied to the aggregated modular network to find larger structures.")
        .argument(ArgType::integer)
        .group("Accuracy")
        .advanced()
        .defaultValue("0")
        .min("1"),
    param(ParameterId::TuneIterationLimit)
        .shortName('T')
        .longName("tune-iteration-limit")
        .description("Limit the main iterations in the two-level partition algorithm. 0 means no limit.")
        .argument(ArgType::integer)
        .group("Accuracy")
        .advanced()
        .defaultValue("0")
        .min("1"),
    param(ParameterId::CoreLoopCodelengthThreshold)
        .longName("core-loop-codelength-threshold")
        .description("Require at least this codelength improvement to accept a new solution in a core loop.")
        .argument(ArgType::number)
        .group("Accuracy")
        .advanced()
        .defaultValue("1e-10")
        .min("0")
        .bindingDefaults(),
    param(ParameterId::TuneIterationRelativeThreshold)
        .longName("tune-iteration-relative-threshold")
        .description("Require each tune iteration to improve codelength by this fraction of the initial two-level codelength.")
        .argument(ArgType::number)
        .group("Accuracy")
        .advanced()
        .defaultValue("1e-05")
        .min("0")
        .bindingDefaults(),
    param(ParameterId::FastHierarchicalSolution)
        .shortName('F')
        .longName("fast-hierarchical-solution")
        .description("Find top modules quickly. Use -FF to keep all fast levels. Use -FFF to skip recursive refinement.")
        .group("Accuracy")
        .advanced()
        .incremental()
        .defaultValue("false")
        .renderPolicy("repeated_short")
        .bindingDefaults("None", "NULL")
        .pythonDoc("Find top modules fast. Use 2 to keep all fast levels and 3 to skip the recursive part."),
    param(ParameterId::InnerParallelization)
        .longName("inner-parallelization")
        .description("Parallelize the innermost loop for speed, with a possible accuracy tradeoff.")
        .group("Accuracy")
        .advanced(),
    param(ParameterId::PreferModularSolution)
        .longName("prefer-modular-solution")
        .description("Prefer a modular solution even when one module gives a lower codelength.")
        .group("Accuracy")
        .advanced(),
    param(ParameterId::NumRandomMoves)
        .longName("num-random-moves")
        .description("Try this many random moves in each core loop to merge weakly connected nodes.")
        .argument(ArgType::integer)
        .group("Accuracy")
        .advanced()
        .defaultValue("5")
        .min("0"),
    param(ParameterId::MaxDegreeForRandomMoves)
        .longName("max-degree-for-random-moves")
        .description("Try random moves only for nodes with degree at most this value.")
        .argument(ArgType::integer)
        .group("Accuracy")
        .advanced()
        .defaultValue("2")
        .min("0"),
    param(ParameterId::OutputDirectory)
        .longName("out_directory")
        .description("Directory where output files are written.")
        .group("Output"),
    param(ParameterId::Verbose)
        .shortName('v')
        .longName("verbose")
        .description("Increase console verbosity. Add more v flags to increase verbosity up to -vvv.")
        .group("Output")
        .incremental()
        .defaultValue("false")
        .bindingNames("verbosity_level", "verbosity_level", "verbose")
        .renderPolicy("repeated_short")
        .bindingDefaults("1", "1L")
        .pythonDoc("Verbosity level on the console. 1 keeps the default output level, 2 renders -vv and so on."),
    param(ParameterId::Silent)
        .longName("silent")
        .description("Suppress console output.")
        .group("Output"),
    param(ParameterId::Pretty)
        .longName("pretty")
        .description("Use modernized console output with color and Unicode on interactive terminals.")
        .group("Output"),
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
        json << "}";
        firstLanguage = false;
      }
      if (!parameter.rDefault.empty()) {
        json << (firstLanguage ? "" : ", ")
             << "\"r\": {\"value\": " << jsonString(parameter.rDefault);
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
