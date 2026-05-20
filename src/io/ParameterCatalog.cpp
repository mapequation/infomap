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

  template <typename T>
  void registerOptionForTarget(ProgramInterface& api, T& target, const ParameterSpec& parameter);

  void registerOptionForTarget(ProgramInterface& api, bool& target, const ParameterSpec& parameter);

  void registerIncrementalOptionForTarget(ProgramInterface& api, unsigned int& target, const ParameterSpec& parameter);

  class SpecBuilder {
  public:
    SpecBuilder() = default;

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

    SpecBuilder& cliOnly()
    {
      parameter_.cliOnly = true;
      return *this;
    }

    SpecBuilder& libraryOnly()
    {
      parameter_.libraryOnly = true;
      return *this;
    }

    SpecBuilder& hideFromJson()
    {
      parameter_.includeInJson = false;
      return *this;
    }

    template <typename T>
    SpecBuilder& configTarget(T Config::*member)
    {
      parameter_.registrar = [member](ProgramInterface& api, ConfigParameterTargets& targets, const ParameterSpec& parameter) {
        registerOptionForTarget(api, targets.config.*member, parameter);
      };
      return *this;
    }

    SpecBuilder& incrementalTarget(unsigned int Config::*member)
    {
      parameter_.registrar = [member](ProgramInterface& api, ConfigParameterTargets& targets, const ParameterSpec& parameter) {
        registerIncrementalOptionForTarget(api, targets.config.*member, parameter);
      };
      return *this;
    }

    SpecBuilder& flowModelTarget()
    {
      parameter_.registrar = [](ProgramInterface& api, ConfigParameterTargets& targets, const ParameterSpec& parameter) {
        registerOptionForTarget(api, targets.flowModelArg, parameter);
      };
      return *this;
    }

    SpecBuilder& deprecatedIncludeSelfLinksTarget()
    {
      parameter_.registrar = [](ProgramInterface& api, ConfigParameterTargets& targets, const ParameterSpec& parameter) {
        registerOptionForTarget(api, targets.deprecatedIncludeSelfLinks, parameter);
      };
      return *this;
    }

    SpecBuilder& networkFileArgument()
    {
      parameter_.registrar = [](ProgramInterface& api, ConfigParameterTargets& targets, const ParameterSpec& parameter) {
        api.addNonOptionArgument(targets.config.networkFile, "network_file", parameter.description, parameter.group);
      };
      return *this;
    }

    SpecBuilder& outputDirectoryArgument()
    {
      parameter_.registrar = [](ProgramInterface& api, ConfigParameterTargets& targets, const ParameterSpec& parameter) {
        api.addOptionalNonOptionArguments(targets.optionalOutputDir, "out_directory", parameter.description, parameter.group);
      };
      return *this;
    }

    operator ParameterSpec() const { return parameter_; }

  private:
    ParameterSpec parameter_;
  };

  SpecBuilder param()
  {
    return SpecBuilder();
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

  void registerOptionForTarget(ProgramInterface& api, bool& target, const ParameterSpec& parameter)
  {
    registerBoolOption(api, target, parameter);
  }

  template <typename T>
  void registerOptionForTarget(ProgramInterface& api, T& target, const ParameterSpec& parameter)
  {
    if (!parameter.minValue.empty() && !parameter.maxValue.empty()) {
      registerRangeOption(api, target, parameter);
    } else if (!parameter.minValue.empty()) {
      registerLowerBoundOption(api, target, parameter);
    } else {
      registerValueOption(api, target, parameter);
    }
  }

  void registerIncrementalOptionForTarget(ProgramInterface& api, unsigned int& target, const ParameterSpec& parameter)
  {
    addConfiguredOption(api.addIncrementalOptionArgument(target, parameter.shortName, parameter.longName, parameter.description, parameter.group, parameter.isAdvanced), parameter);
  }

} // namespace

const std::vector<ParameterSpec>& parameterCatalog()
{
  static const std::vector<ParameterSpec> parameters = {
    param()
        .shortName('h')
        .longName("help")
        .description("Prints this help message. Use -hh to show advanced options.")
        .group("About")
        .incremental()
        .defaultValue("false")
        .renderPolicy("repeated_short"),
    param()
        .shortName('V')
        .longName("version")
        .description("Display program version information.")
        .group("About")
        .defaultValue("false"),
    param()
        .longName("completion")
        .description("Print shell completion script for bash or zsh.")
        .argument(ArgType::option)
        .group("About")
        .choices({ "bash", "zsh" })
        .hideFromJson(),
    param()
        .longName("network_file")
        .description("Network file to read. Infomap assumes link-list format unless the file starts with a Pajek heading.")
        .group("Input")
        .cliOnly()
        .hideFromJson()
        .networkFileArgument(),
    param()
        .longName("input")
        .description("Network file to read. Infomap assumes link-list format unless the file starts with a Pajek heading.")
        .argument(ArgType::path)
        .group("Input")
        .libraryOnly()
        .hideFromJson()
        .configTarget(&Config::networkFile),
    param()
        .longName("skip-adjust-bipartite-flow")
        .description("Keep flow on bipartite nodes instead of distributing it to primary nodes.")
        .group("Input")
        .advanced()
        .configTarget(&Config::skipAdjustBipartiteFlow),
    param()
        .longName("bipartite-teleportation")
        .description("Use bipartite teleportation instead of the default two-step unipartite teleportation.")
        .group("Input")
        .advanced()
        .configTarget(&Config::bipartiteTeleportation),
    param()
        .longName("weight-threshold")
        .description("Ignore input links with weight below this threshold.")
        .argument(ArgType::number)
        .group("Input")
        .advanced()
        .defaultValue("0")
        .min("0")
        .configTarget(&Config::weightThreshold),
    param()
        .shortName('k')
        .longName("include-self-links")
        .description("DEPRECATED. Self-links are included by default; use --no-self-links to exclude them.")
        .group("Input")
        .advanced()
        .hidden()
        .deprecatedIncludeSelfLinksTarget(),
    param()
        .longName("no-self-links")
        .description("Exclude self-links from the input network.")
        .group("Input")
        .advanced()
        .defaultValue("false")
        .renderPolicy("deprecated_alias_target")
        .configTarget(&Config::noSelfLinks),
    param()
        .longName("node-limit")
        .description("Read only nodes up to this node id and ignore links connected to higher node ids.")
        .argument(ArgType::integer)
        .group("Input")
        .advanced()
        .defaultValue("0")
        .min("1")
        .configTarget(&Config::nodeLimit),
    param()
        .longName("matchable-multilayer-ids")
        .description("Construct state ids from node ids and layer ids that stay comparable across networks. Set at least to the largest layer id among networks to match.")
        .argument(ArgType::integer)
        .group("Input")
        .advanced()
        .defaultValue("0")
        .min("1")
        .configTarget(&Config::matchableMultilayerIds),
    param()
        .shortName('c')
        .longName("cluster-data")
        .description("Read an initial partition from a clu file or a hierarchy from a tree/ftree file. Tree input may use physical or state nodes for higher-order networks.")
        .argument(ArgType::path)
        .group("Input")
        .configTarget(&Config::clusterDataFile),
    param()
        .longName("assign-to-neighbouring-module")
        .description("With --cluster-data, assign nodes missing module ids to a neighboring node's module when possible.")
        .group("Input")
        .advanced()
        .configTarget(&Config::assignToNeighbouringModule),
    param()
        .longName("meta-data")
        .description("Read metadata to encode from a clu-format file.")
        .argument(ArgType::path)
        .group("Input")
        .advanced()
        .configTarget(&Config::metaDataFile),
    param()
        .longName("meta-data-rate")
        .description("With --meta-data, set the metadata encoding rate. The default encodes metadata at each step.")
        .argument(ArgType::number)
        .group("Input")
        .advanced()
        .defaultValue("1")
        .min("0")
        .bindingDefaults()
        .configTarget(&Config::metaDataRate),
    param()
        .longName("meta-data-unweighted")
        .description("With --meta-data, encode metadata without weighting by node flow.")
        .group("Input")
        .advanced()
        .configTarget(&Config::unweightedMetaData),
    param()
        .longName("no-infomap")
        .description("Skip optimization. Use this to calculate codelength for --cluster-data or to print non-modular statistics.")
        .group("Input")
        .configTarget(&Config::noInfomap),
    param()
        .longName("out-name")
        .description("Base name for output files, for example [out_directory]/[out-name].tree.")
        .argument(ArgType::string)
        .group("Output")
        .advanced()
        .configTarget(&Config::outName),
    param()
        .shortName('0')
        .longName("no-file-output")
        .description("Do not write output files.")
        .group("Output")
        .advanced()
        .configTarget(&Config::noFileOutput),
    param()
        .longName("tree")
        .description("Write the modular hierarchy to a tree file. Enabled by default when no other output format is selected.")
        .group("Output")
        .configTarget(&Config::printTree),
    param()
        .longName("ftree")
        .description("Write the modular hierarchy and aggregated links between nested modules to an ftree file. Used by Network Navigator.")
        .group("Output")
        .configTarget(&Config::printFlowTree),
    param()
        .longName("clu")
        .description("Write top-level module ids for each node to a clu file.")
        .group("Output")
        .configTarget(&Config::printClu),
    param()
        .longName("clu-level")
        .description("With --clu or --output clu, write module ids at this depth from the root. Use -1 for bottom-level modules.")
        .argument(ArgType::integer)
        .group("Output")
        .advanced()
        .defaultValue("1")
        .min("-1")
        .configTarget(&Config::cluLevel),
    param()
        .shortName('o')
        .longName("output")
        .description("Write selected output formats as a comma-separated list without spaces, e.g. -o clu,tree,ftree. Options: clu, tree, ftree, newick, json, csv, network, states, flow.")
        .argument(ArgType::list)
        .group("Output")
        .advanced()
        .choices(outputFormatNames())
        .renderPolicy("comma_list")
        .configTarget(&Config::outputFormats),
    param()
        .longName("hide-bipartite-nodes")
        .description("Hide bipartite nodes in output by projecting the solution to primary nodes.")
        .group("Output")
        .advanced()
        .configTarget(&Config::hideBipartiteNodes),
    param()
        .longName("print-all-trials")
        .description("Write each trial to separate output files. Has effect only when --num-trials is greater than 1.")
        .group("Output")
        .advanced()
        .configTarget(&Config::printAllTrials),
    param()
        .shortName('2')
        .longName("two-level")
        .description("Optimize a two-level partition instead of the default multi-level hierarchy.")
        .group("Algorithm")
        .configTarget(&Config::twoLevel),
    param()
        .shortName('f')
        .longName("flow-model")
        .description("Choose how Infomap derives flow from the input links. Options: undirected, directed, undirdir, outdirdir, rawdir, precomputed.")
        .argument(ArgType::option)
        .group("Algorithm")
        .choices({ "undirected", "directed", "undirdir", "outdirdir", "rawdir", "precomputed" })
        .flowModelTarget(),
    param()
        .shortName('d')
        .longName("directed")
        .description("Treat input links as directed. Shorthand for --flow-model directed.")
        .group("Algorithm")
        .defaultValue("false")
        .renderPolicy("directed_alias")
        .bindingDefaults("None", "NULL")
        .configTarget(&Config::directed),
    param()
        .shortName('e')
        .longName("recorded-teleportation")
        .description("When teleportation is used to calculate flow, also record teleportation steps in the codelength.")
        .group("Algorithm")
        .advanced()
        .configTarget(&Config::recordedTeleportation),
    param()
        .longName("use-node-weights-as-flow")
        .description("Use node weights from the API or Pajek node records as normalized node flow.")
        .group("Algorithm")
        .advanced()
        .configTarget(&Config::useNodeWeightsAsFlow),
    param()
        .longName("to-nodes")
        .description("Teleport to nodes instead of links. Uses uniform node weights unless node weights are provided.")
        .group("Algorithm")
        .advanced()
        .configTarget(&Config::teleportToNodes),
    param()
        .shortName('p')
        .longName("teleportation-probability")
        .description("Set the probability of teleporting to a random node or link when calculating flow.")
        .argument(ArgType::probability)
        .group("Algorithm")
        .advanced()
        .defaultValue("0.15")
        .range("0", "1")
        .bindingDefaults()
        .configTarget(&Config::teleportationProbability),
    param()
        .longName("regularized")
        .description("Add a fully connected Bayesian prior network to reduce overfitting to missing links. Activates --recorded-teleportation.")
        .group("Algorithm")
        .advanced()
        .configTarget(&Config::regularized),
    param()
        .longName("regularization-strength")
        .description("Scale the relative strength of the Bayesian prior network used by --regularized.")
        .argument(ArgType::number)
        .group("Algorithm")
        .advanced()
        .defaultValue("1")
        .min("0")
        .bindingDefaults()
        .configTarget(&Config::regularizationStrength),
    param()
        .longName("entropy-corrected")
        .description("Correct for negative entropy bias in small samples, especially solutions with many modules.")
        .group("Algorithm")
        .advanced()
        .configTarget(&Config::entropyBiasCorrection),
    param()
        .longName("entropy-correction-strength")
        .description("Scale the default correction used by --entropy-corrected.")
        .argument(ArgType::number)
        .group("Algorithm")
        .advanced()
        .defaultValue("1")
        .bindingDefaults()
        .configTarget(&Config::entropyBiasCorrectionMultiplier),
    param()
        .longName("markov-time")
        .description("Scale link flow to change the cost of moving between modules. Higher values result in fewer modules.")
        .argument(ArgType::number)
        .group("Algorithm")
        .advanced()
        .defaultValue("1")
        .min("0")
        .bindingDefaults()
        .configTarget(&Config::markovTime),
    param()
        .longName("variable-markov-time")
        .description("Vary Markov time locally to reduce overpartitioning in sparse areas while keeping higher resolution in dense areas.")
        .group("Algorithm")
        .advanced()
        .configTarget(&Config::variableMarkovTime),
    param()
        .longName("variable-markov-damping")
        .description("With --variable-markov-time, set damping between local effective degree (0) and local entropy (1).")
        .argument(ArgType::number)
        .group("Algorithm")
        .advanced()
        .defaultValue("1")
        .bindingDefaults()
        .configTarget(&Config::variableMarkovTimeDamping),
    param()
        .longName("variable-markov-min-scale")
        .description("With --variable-markov-time, set the minimum local scale for zero-entropy nodes. Local Markov time is max scale divided by local scale.")
        .argument(ArgType::number)
        .group("Algorithm")
        .advanced()
        .defaultValue("1")
        .bindingDefaults()
        .configTarget(&Config::variableMarkovTimeMinLocalScale),
    param()
        .longName("preferred-number-of-modules")
        .description("Penalize solutions by how far their number of modules differs from this value.")
        .argument(ArgType::integer)
        .group("Algorithm")
        .advanced()
        .defaultValue("0")
        .min("1")
        .configTarget(&Config::preferredNumberOfModules),
    param()
        .longName("multilayer-relax-rate")
        .description("Set the probability of relaxing from a state node to neighboring layers instead of staying in the current layer.")
        .argument(ArgType::probability)
        .group("Algorithm")
        .advanced()
        .defaultValue("0.15")
        .range("0", "1")
        .bindingDefaults()
        .configTarget(&Config::multilayerRelaxRate),
    param()
        .longName("multilayer-relax-limit")
        .description("Limit relaxation to this many neighboring layer ids in each direction. Use a negative value to allow relaxation to any layer.")
        .argument(ArgType::integer)
        .group("Algorithm")
        .advanced()
        .defaultValue("-1")
        .bindingDefaults("-1", "NULL")
        .configTarget(&Config::multilayerRelaxLimit),
    param()
        .longName("multilayer-relax-limit-up")
        .description("Limit relaxation upward to this many higher neighboring layer ids. Use a negative value to allow relaxation to any higher layer.")
        .argument(ArgType::integer)
        .group("Algorithm")
        .advanced()
        .defaultValue("-1")
        .bindingDefaults("-1", "NULL")
        .configTarget(&Config::multilayerRelaxLimitUp),
    param()
        .longName("multilayer-relax-limit-down")
        .description("Limit relaxation downward to this many lower neighboring layer ids. Use a negative value to allow relaxation to any lower layer.")
        .argument(ArgType::integer)
        .group("Algorithm")
        .advanced()
        .defaultValue("-1")
        .bindingDefaults("-1", "NULL")
        .configTarget(&Config::multilayerRelaxLimitDown),
    param()
        .longName("multilayer-relax-by-jsd")
        .description("Weight multilayer relaxation by out-link similarity measured with Jensen-Shannon divergence.")
        .group("Algorithm")
        .advanced()
        .configTarget(&Config::multilayerRelaxByJensenShannonDivergence),
    param()
        .shortName('s')
        .longName("seed")
        .description("Set the random number generator seed for reproducible results.")
        .argument(ArgType::integer)
        .group("Accuracy")
        .defaultValue("123")
        .min("1")
        .bindingDefaults()
        .configTarget(&Config::seedToRandomNumberGenerator),
    param()
        .shortName('N')
        .longName("num-trials")
        .description("Run this many independent trials and keep the best solution.")
        .argument(ArgType::integer)
        .group("Accuracy")
        .defaultValue("1")
        .min("1")
        .bindingDefaults()
        .configTarget(&Config::numTrials),
    param()
        .shortName('M')
        .longName("core-loop-limit")
        .description("Limit how many core loops try to move each node to the best module.")
        .argument(ArgType::integer)
        .group("Accuracy")
        .advanced()
        .defaultValue("10")
        .min("1")
        .bindingDefaults()
        .configTarget(&Config::coreLoopLimit),
    param()
        .shortName('L')
        .longName("core-level-limit")
        .description("Limit how many times core loops are reapplied to the aggregated modular network to find larger structures.")
        .argument(ArgType::integer)
        .group("Accuracy")
        .advanced()
        .defaultValue("0")
        .min("1")
        .configTarget(&Config::levelAggregationLimit),
    param()
        .shortName('T')
        .longName("tune-iteration-limit")
        .description("Limit the main iterations in the two-level partition algorithm. 0 means no limit.")
        .argument(ArgType::integer)
        .group("Accuracy")
        .advanced()
        .defaultValue("0")
        .min("1")
        .configTarget(&Config::tuneIterationLimit),
    param()
        .longName("core-loop-codelength-threshold")
        .description("Require at least this codelength improvement to accept a new solution in a core loop.")
        .argument(ArgType::number)
        .group("Accuracy")
        .advanced()
        .defaultValue("1e-10")
        .min("0")
        .bindingDefaults()
        .configTarget(&Config::minimumCodelengthImprovement),
    param()
        .longName("tune-iteration-relative-threshold")
        .description("Require each tune iteration to improve codelength by this fraction of the initial two-level codelength.")
        .argument(ArgType::number)
        .group("Accuracy")
        .advanced()
        .defaultValue("1e-05")
        .min("0")
        .bindingDefaults()
        .configTarget(&Config::minimumRelativeTuneIterationImprovement),
    param()
        .shortName('F')
        .longName("fast-hierarchical-solution")
        .description("Find top modules quickly. Use -FF to keep all fast levels. Use -FFF to skip recursive refinement.")
        .group("Accuracy")
        .advanced()
        .incremental()
        .defaultValue("false")
        .renderPolicy("repeated_short")
        .bindingDefaults("None", "NULL")
        .pythonDoc("Find top modules fast. Use 2 to keep all fast levels and 3 to skip the recursive part.")
        .incrementalTarget(&Config::fastHierarchicalSolution),
    param()
        .longName("inner-parallelization")
        .description("Parallelize the innermost loop for speed, with a possible accuracy tradeoff.")
        .group("Accuracy")
        .advanced()
        .configTarget(&Config::innerParallelization),
    param()
        .longName("prefer-modular-solution")
        .description("Prefer a modular solution even when one module gives a lower codelength.")
        .group("Accuracy")
        .advanced()
        .configTarget(&Config::preferModularSolution),
    param()
        .longName("num-random-moves")
        .description("Try this many random moves in each core loop to merge weakly connected nodes.")
        .argument(ArgType::integer)
        .group("Accuracy")
        .advanced()
        .defaultValue("5")
        .min("0")
        .configTarget(&Config::numRandomMoves),
    param()
        .longName("max-degree-for-random-moves")
        .description("Try random moves only for nodes with degree at most this value.")
        .argument(ArgType::integer)
        .group("Accuracy")
        .advanced()
        .defaultValue("2")
        .min("0")
        .configTarget(&Config::maxDegreeForRandomMoves),
    param()
        .longName("out_directory")
        .description("Directory where output files are written.")
        .group("Output")
        .hideFromJson()
        .outputDirectoryArgument(),
    param()
        .shortName('v')
        .longName("verbose")
        .description("Increase console verbosity. Add more v flags to increase verbosity up to -vvv.")
        .group("Output")
        .incremental()
        .defaultValue("false")
        .bindingNames("verbosity_level", "verbosity_level", "verbose")
        .renderPolicy("repeated_short")
        .bindingDefaults("1", "1L")
        .pythonDoc("Verbosity level on the console. 1 keeps the default output level, 2 renders -vv and so on.")
        .incrementalTarget(&Config::verbosity),
    param()
        .longName("silent")
        .description("Suppress console output.")
        .group("Output")
        .configTarget(&Config::silent),
    param()
        .longName("pretty")
        .description("Use modernized console output with color and Unicode on interactive terminals.")
        .group("Output")
        .configTarget(&Config::prettyOutput),
  };
  return parameters;
}

void registerConfigParameters(ProgramInterface& api, ConfigParameterTargets targets, bool isCli)
{
  for (const auto& parameter : parameterCatalog()) {
    if (!parameter.registrar) {
      continue;
    }
    if (isCli && parameter.libraryOnly) {
      continue;
    }
    if (!isCli && parameter.cliOnly) {
      continue;
    }
    parameter.registrar(api, targets, parameter);
  }
}

std::string parameterCatalogJson()
{
  std::ostringstream json;
  json << "{\n  \"parameters\": [\n";
  bool firstParameter = true;
  for (const auto& parameter : parameterCatalog()) {
    if (parameter.hidden || !parameter.includeInJson) {
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
