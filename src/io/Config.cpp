/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "Config.h"
#include "OutputFormats.h"
#include "ProgramInterface.h"
#include "SafeFile.h"
#include "../utils/FileURI.h"
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

  void applyFlowModelSelection(Config& config, const std::string& flowModelArg)
  {
    if (flowModelArg == "directed" || config.directed) {
      config.setFlowModel(FlowModel::directed);
    } else if (flowModelArg == "undirected") {
      config.setFlowModel(FlowModel::undirected);
    } else if (flowModelArg == "undirdir") {
      config.setFlowModel(FlowModel::undirdir);
    } else if (flowModelArg == "outdirdir") {
      config.setFlowModel(FlowModel::outdirdir);
    } else if (flowModelArg == "rawdir") {
      config.setFlowModel(FlowModel::rawdir);
    } else if (flowModelArg == "precomputed") {
      config.setFlowModel(FlowModel::precomputed);
    } else if (!flowModelArg.empty()) {
      throw std::runtime_error(io::Str() << "Unrecognized flow model: '" << flowModelArg << "'");
    }
  }

  void normalizeOutputDirectory(Config& config)
  {
    if (!config.haveOutput() || config.outDirectory.empty())
      return;

    if (config.outDirectory.back() != '/')
      config.outDirectory.push_back('/');
  }

  void validateOutputDirectory(const Config& config)
  {
    if (config.haveOutput() && !isDirectoryWritable(config.outDirectory))
      throw std::runtime_error(io::Str() << "Can't write to directory '" << config.outDirectory << "'. Check that the directory exists and that you have write permissions.");
  }

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
  ProgramInterface api("Infomap",
                       "Implementation of the Infomap clustering algorithm based on the Map Equation (see www.mapequation.org)",
                       INFOMAP_VERSION);

  api.setGroups({ "Input", "Algorithm", "Accuracy", "Output" });

  std::vector<std::string> optionalOutputDir; // Used if !isCLI
  // --------------------- Input options ---------------------
  if (isCLI) {
    api.addNonOptionArgument(networkFile, "network_file", "Network file to read. Infomap assumes link-list format unless the file starts with a Pajek heading.", "Input");
  } else {
    api.addOptionArgument(networkFile, "input", "Network file to read. Infomap assumes link-list format unless the file starts with a Pajek heading.", ArgType::path, "Input");
  }

  api.addOptionArgument(skipAdjustBipartiteFlow, "skip-adjust-bipartite-flow", "Keep flow on bipartite nodes instead of distributing it to primary nodes.", "Input", true);

  api.addOptionArgument(bipartiteTeleportation, "bipartite-teleportation", "Use bipartite teleportation instead of the default two-step unipartite teleportation.", "Input", true);

  api.addOptionArgument(weightThreshold, "weight-threshold", "Ignore input links with weight below this threshold.", ArgType::number, "Input", 0.0, true);

  bool deprecated_includeSelfLinks = false;
  api.addOptionArgument(deprecated_includeSelfLinks, 'k', "include-self-links", "DEPRECATED. Self-links are included by default; use --no-self-links to exclude them.", "Input", true).setHidden(true);

  api.addOptionArgument(noSelfLinks, "no-self-links", "Exclude self-links from the input network.", "Input", true);

  api.addOptionArgument(nodeLimit, "node-limit", "Read only nodes up to this node id and ignore links connected to higher node ids.", ArgType::integer, "Input", 1u, true);

  api.addOptionArgument(matchableMultilayerIds, "matchable-multilayer-ids", "Construct state ids from node ids and layer ids that stay comparable across networks. Set at least to the largest layer id among networks to match.", ArgType::integer, "Input", 1u, true);

  api.addOptionArgument(clusterDataFile, 'c', "cluster-data", "Read an initial partition from a clu file or a hierarchy from a tree/ftree file. Tree input may use physical or state nodes for higher-order networks.", ArgType::path, "Input");

  api.addOptionArgument(assignToNeighbouringModule, "assign-to-neighbouring-module", "With --cluster-data, assign nodes missing module ids to a neighboring node's module when possible.", "Input", true);

  api.addOptionArgument(metaDataFile, "meta-data", "Read metadata to encode from a clu-format file.", ArgType::path, "Input", true);

  api.addOptionArgument(metaDataRate, "meta-data-rate", "With --meta-data, set the metadata encoding rate. The default encodes metadata at each step.", ArgType::number, "Input", 0.0, true);

  api.addOptionArgument(unweightedMetaData, "meta-data-unweighted", "With --meta-data, encode metadata without weighting by node flow.", "Input", true);

  api.addOptionArgument(noInfomap, "no-infomap", "Skip optimization. Use this to calculate codelength for --cluster-data or to print non-modular statistics.", "Input");

  // --------------------- Output options ---------------------

  api.addOptionArgument(outName, "out-name", "Base name for output files, for example [out_directory]/[out-name].tree.", ArgType::string, "Output", true);

  api.addOptionArgument(noFileOutput, '0', "no-file-output", "Do not write output files.", "Output", true);

  api.addOptionArgument(printTree, "tree", "Write the modular hierarchy to a tree file. Enabled by default when no other output format is selected.", "Output");

  api.addOptionArgument(printFlowTree, "ftree", "Write the modular hierarchy and aggregated links between nested modules to an ftree file. Used by Network Navigator.", "Output");

  api.addOptionArgument(printClu, "clu", "Write top-level module ids for each node to a clu file.", "Output");

  api.addOptionArgument(cluLevel, "clu-level", "With --clu or --output clu, write module ids at this depth from the root. Use -1 for bottom-level modules.", ArgType::integer, "Output", -1, true);

  api.addOptionArgument(outputFormats, 'o', "output", "Write selected output formats as a comma-separated list without spaces, e.g. -o clu,tree,ftree. Options: clu, tree, ftree, newick, json, csv, network, states, flow.", ArgType::list, "Output", true)
      .setChoices(outputFormatNames());

  api.addOptionArgument(hideBipartiteNodes, "hide-bipartite-nodes", "Hide bipartite nodes in output by projecting the solution to primary nodes.", "Output", true);

  api.addOptionArgument(printAllTrials, "print-all-trials", "Write each trial to separate output files. Has effect only when --num-trials is greater than 1.", "Output", true);

  // --------------------- Core algorithm options ---------------------
  api.addOptionArgument(twoLevel, '2', "two-level", "Optimize a two-level partition instead of the default multi-level hierarchy.", "Algorithm");

  std::string flowModelArg;

  api.addOptionArgument(flowModelArg, 'f', "flow-model", "Choose how Infomap derives flow from the input links. Options: undirected, directed, undirdir, outdirdir, rawdir, precomputed.", ArgType::option, "Algorithm")
      .setChoices({ "undirected", "directed", "undirdir", "outdirdir", "rawdir", "precomputed" });

  api.addOptionArgument(directed, 'd', "directed", "Treat input links as directed. Shorthand for --flow-model directed.", "Algorithm");

  api.addOptionArgument(recordedTeleportation, 'e', "recorded-teleportation", "When teleportation is used to calculate flow, also record teleportation steps in the codelength.", "Algorithm", true);

  api.addOptionArgument(useNodeWeightsAsFlow, "use-node-weights-as-flow", "Use node weights from the API or Pajek node records as normalized node flow.", "Algorithm", true);

  api.addOptionArgument(teleportToNodes, "to-nodes", "Teleport to nodes instead of links. Uses uniform node weights unless node weights are provided.", "Algorithm", true);

  api.addOptionArgument(teleportationProbability, 'p', "teleportation-probability", "Set the probability of teleporting to a random node or link when calculating flow.", ArgType::probability, "Algorithm", 0.0, 1.0, true);

  api.addOptionArgument(regularized, "regularized", "Add a fully connected Bayesian prior network to reduce overfitting to missing links. Activates --recorded-teleportation.", "Algorithm", true);

  api.addOptionArgument(regularizationStrength, "regularization-strength", "Scale the relative strength of the Bayesian prior network used by --regularized.", ArgType::number, "Algorithm", 0.0, true);

  api.addOptionArgument(entropyBiasCorrection, "entropy-corrected", "Correct for negative entropy bias in small samples, especially solutions with many modules.", "Algorithm", true);

  api.addOptionArgument(entropyBiasCorrectionMultiplier, "entropy-correction-strength", "Scale the default correction used by --entropy-corrected.", ArgType::number, "Algorithm", true);

  api.addOptionArgument(markovTime, "markov-time", "Scale link flow to change the cost of moving between modules. Higher values result in fewer modules.", ArgType::number, "Algorithm", 0.0, true);

  api.addOptionArgument(variableMarkovTime, "variable-markov-time", "Vary Markov time locally to reduce overpartitioning in sparse areas while keeping higher resolution in dense areas.", "Algorithm", true);

  api.addOptionArgument(variableMarkovTimeDamping, "variable-markov-damping", "With --variable-markov-time, set damping between local effective degree (0) and local entropy (1).", ArgType::number, "Algorithm", true);

  api.addOptionArgument(variableMarkovTimeMinLocalScale, "variable-markov-min-scale", "With --variable-markov-time, set the minimum local scale for zero-entropy nodes. Local Markov time is max scale divided by local scale.", ArgType::number, "Algorithm", true);

  // api.addOptionArgument(markovTimeNoSelfLinks, "markov-time-no-self-links", "For testing.", "Algorithm", true);

  api.addOptionArgument(preferredNumberOfModules, "preferred-number-of-modules", "Penalize solutions by how far their number of modules differs from this value.", ArgType::integer, "Algorithm", 1u, true);

  api.addOptionArgument(multilayerRelaxRate, "multilayer-relax-rate", "Set the probability of relaxing from a state node to neighboring layers instead of staying in the current layer.", ArgType::probability, "Algorithm", 0.0, 1.0, true);

  api.addOptionArgument(multilayerRelaxLimit, "multilayer-relax-limit", "Limit relaxation to this many neighboring layer ids in each direction. Use a negative value to allow relaxation to any layer.", ArgType::integer, "Algorithm", -1, true);

  api.addOptionArgument(multilayerRelaxLimitUp, "multilayer-relax-limit-up", "Limit relaxation upward to this many higher neighboring layer ids. Use a negative value to allow relaxation to any higher layer.", ArgType::integer, "Algorithm", -1, true);

  api.addOptionArgument(multilayerRelaxLimitDown, "multilayer-relax-limit-down", "Limit relaxation downward to this many lower neighboring layer ids. Use a negative value to allow relaxation to any lower layer.", ArgType::integer, "Algorithm", -1, true);

  api.addOptionArgument(multilayerRelaxByJensenShannonDivergence, "multilayer-relax-by-jsd", "Weight multilayer relaxation by out-link similarity measured with Jensen-Shannon divergence.", "Algorithm", true);

  // --------------------- Performance and accuracy options ---------------------
  api.addOptionArgument(seedToRandomNumberGenerator, 's', "seed", "Set the random number generator seed for reproducible results.", ArgType::integer, "Accuracy", 1ul);

  api.addOptionArgument(numTrials, 'N', "num-trials", "Run this many independent trials and keep the best solution.", ArgType::integer, "Accuracy", 1u);

  api.addOptionArgument(coreLoopLimit, 'M', "core-loop-limit", "Limit how many core loops try to move each node to the best module.", ArgType::integer, "Accuracy", 1u, true);

  api.addOptionArgument(levelAggregationLimit, 'L', "core-level-limit", "Limit how many times core loops are reapplied to the aggregated modular network to find larger structures.", ArgType::integer, "Accuracy", 1u, true);

  api.addOptionArgument(tuneIterationLimit, 'T', "tune-iteration-limit", "Limit the main iterations in the two-level partition algorithm. 0 means no limit.", ArgType::integer, "Accuracy", 1u, true);

  api.addOptionArgument(minimumCodelengthImprovement, "core-loop-codelength-threshold", "Require at least this codelength improvement to accept a new solution in a core loop.", ArgType::number, "Accuracy", 0.0, true);

  api.addOptionArgument(minimumRelativeTuneIterationImprovement, "tune-iteration-relative-threshold", "Require each tune iteration to improve codelength by this fraction of the initial two-level codelength.", ArgType::number, "Accuracy", 0.0, true);

  api.addIncrementalOptionArgument(fastHierarchicalSolution, 'F', "fast-hierarchical-solution", "Find top modules quickly. Use -FF to keep all fast levels. Use -FFF to skip recursive refinement.", "Accuracy", true);

  api.addOptionArgument(innerParallelization, "inner-parallelization", "Parallelize the innermost loop for speed, with a possible accuracy tradeoff.", "Accuracy", true);

  api.addOptionArgument(preferModularSolution, "prefer-modular-solution", "Prefer a modular solution even when one module gives a lower codelength.", "Accuracy", true);

  api.addOptionArgument(numRandomMoves, "num-random-moves", "Try this many random moves in each core loop to merge weakly connected nodes.", ArgType::integer, "Accuracy", 0u, true);

  api.addOptionArgument(maxDegreeForRandomMoves, "max-degree-for-random-moves", "Try random moves only for nodes with degree at most this value.", ArgType::integer, "Accuracy", 0u, true);

  api.addOptionalNonOptionArguments(optionalOutputDir, "out_directory", "Directory where output files are written.", "Output");

  api.addIncrementalOptionArgument(verbosity, 'v', "verbose", "Increase console verbosity. Add more v flags to increase verbosity up to -vvv.", "Output");

  api.addOptionArgument(silent, "silent", "Suppress console output.", "Output");

  api.parseArgs(flags);

  if (deprecated_includeSelfLinks) {
    throw std::runtime_error("The --include-self-links flag is deprecated to include self links by default. Use --no-self-links to exclude.");
  }

  if (!optionalOutputDir.empty())
    outDirectory = optionalOutputDir[0];

  if (!isCLI && outDirectory.empty())
    noFileOutput = true;

  if (!noFileOutput && outDirectory.empty() && isCLI) {
    throw std::runtime_error("Missing out_directory");
  }

  applyFlowModelSelection(*this, flowModelArg);

  if (regularized) {
    recordedTeleportation = true;
  }

  normalizeOutputDirectory(*this);
  validateOutputDirectory(*this);

  if (outName.empty()) {
    outName = !networkFile.empty() ? FileURI(networkFile).getName() : "no-name";
  }

  if (noInfomap) {
    numTrials = 1;
  }

  parsedString = flags;
  parsedOptions = api.getUsedOptionArguments();

  if (printAllTrials && numTrials < 2) {
    printAllTrials = false;
  }

  adaptDefaults();

  Log::init(verbosity, silent, verboseNumberPrecision);
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
