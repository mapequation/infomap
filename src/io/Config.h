/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Eriksson, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_AGPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef CONFIG_H_
#define CONFIG_H_

#include "../utils/Date.h"
#include "../version.h"
#include "ProgramInterface.h"

#include <stdexcept>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <limits>

namespace infomap {

struct FlowModel {
  static constexpr int undirected = 0;
  static constexpr int directed = 1;
  static constexpr int undirdir = 2;
  static constexpr int outdirdir = 3;
  static constexpr int rawdir = 4;

  int value = 0;

  FlowModel(int val) : value(val) { }
  FlowModel& operator=(int val)
  {
    value = val;
    return *this;
  }

  operator int&() { return value; }
  operator int() const { return value; }
};

std::ostream& operator<<(std::ostream& out, FlowModel f);

inline const char* flowModelToString(const FlowModel& flowModel)
{
  switch (flowModel) {
  case FlowModel::directed:
    return "directed";
  case FlowModel::undirdir:
    return "undirdir";
  case FlowModel::outdirdir:
    return "outdirdir";
  case FlowModel::rawdir:
    return "rawdir";
  case FlowModel::undirected:
  default:
    return "undirected";
  }
}

struct Config {
  // Input
  bool isCLI = false;
  std::string networkFile;
  std::vector<std::string> additionalInput;
  bool stateInput = false;
  bool stateOutput = false;
  bool multilayerInput = false;
  double weightThreshold = 0.0;
  bool unweightedPaths = false;
  unsigned int pathMarkovOrder = 1;
  bool bipartite = false;
  bool skipAdjustBipartiteFlow = false;
  bool bipartiteTeleportation = false;
  bool hardPartitions = false;
  bool parseWithoutIOStreams = false;
  bool zeroBasedNodeNumbers = false;
  bool noSelfLinks = false; // Replaces includeSelfLinks
  bool ignoreEdgeWeights = false;
  unsigned int nodeLimit = 0;
  unsigned int matchableMultilayerIds = 0;
  bool preClusterMultilayer = false;
  std::string clusterDataFile;
  std::string metaDataFile;
  double metaDataRate = 1.0;
  bool unweightedMetaData = false;
  unsigned int numMetaDataDimensions = 0;
  bool clusterDataIsHard = false;
  bool assignToNeighbouringModule = false;
  bool noInfomap = false;

  FlowModel flowModel = FlowModel::undirected;
  bool flowModelIsSet = false;
  // TODO: Remove variables for 'one-hot encoded' flow models
  bool directed = false;
  bool undirdir = false;
  bool outdirdir = false;
  bool rawdir = false;
  bool useNodeWeightsAsFlow = false;
  bool teleportToNodes = false;
  double selfTeleportationProbability = -1;
  double markovTime = 1.0;
  double multilayerRelaxRate = 0.15;
  int multilayerRelaxLimit = -1; // Amount of layers allowed to jump up or down
  int multilayerRelaxLimitUp = -1; // One-sided limit to higher layers
  int multilayerRelaxLimitDown = -1; // One-sided limit to lower layers
  double multilayerJSRelaxRate = 0.15;
  bool multilayerRelaxByJensenShannonDivergence = false;
  int multilayerJSRelaxLimit = -1;

  // Clustering
  bool twoLevel = false;
  bool noCoarseTune = false;
  bool directedEdges = false; // For clustering
  bool recordedTeleportation = false;
  bool regularized = false; // Add a Bayesian prior network with recorded teleportation (sets recordedTeleportation and teleportToNodes to true)
  double regularizationStrength = 1.0; // Scale Bayesian prior constant ln(N)/N with this factor
  double teleportationProbability = 0.15;
  unsigned int preferredNumberOfModules = 0;
  bool entropyBiasCorrection = false;
  double entropyBiasCorrectionMultiplier = 1;
  unsigned long seedToRandomNumberGenerator = 123;

  // Performance and accuracy
  unsigned int numTrials = 1;
  double minimumCodelengthImprovement = 1e-10;
  double minimumSingleNodeCodelengthImprovement = 1e-16;
  bool randomizeCoreLoopLimit = false;
  unsigned int coreLoopLimit = 10;
  unsigned int levelAggregationLimit = 0;
  unsigned int tuneIterationLimit = 0; // Iterations of fine-tune/coarse-tune in two-level partition
  double minimumRelativeTuneIterationImprovement = 1e-5;
  bool fastCoarseTunePartition = false;
  bool alternateCoarseTuneLevel = false;
  unsigned int coarseTuneLevel = 1;
  bool onlySuperModules = false;
  unsigned int fastHierarchicalSolution = 0;
  bool preferModularSolution = false;
  bool innerParallelization = false;

  // Output
  std::string outDirectory;
  std::string outName;
  std::string outputFormats;
  bool originallyUndirected = false;
  bool printTree = false;
  bool printFlowTree = false;
  bool printNewick = false;
  bool printJson = false;
  bool printCsv = false;
  bool printMap = false;
  bool printClu = false;
  int cluLevel = 1; // Write modules at specified depth from root. 1, 2, ... or -1 for bottom level
  bool printFlowNetwork = false;
  bool printPajekNetwork = false;
  bool printStateNetwork = false;
  bool printBinaryTree = false;
  bool printBinaryFlowTree = false; // tree including horizontal links (hierarchical network)
  bool noFileOutput = false;
  unsigned int verbosity = 0;
  unsigned int verboseNumberPrecision = 9;
  bool silent = false;
  bool hideBipartiteNodes = false;

  // Other
  Date startDate;
  std::string version = INFOMAP_VERSION;
  std::string parsedString;
  std::vector<ParsedOption> parsedOptions;
  std::string error;

  Config() = default;

  explicit Config(std::string flags, bool isCLI = false);

  Config& cloneAsNonMain(const Config& other)
  {
    isCLI = other.isCLI;
    networkFile = other.networkFile;
    additionalInput = other.additionalInput;
    stateInput = other.stateInput;
    stateOutput = other.stateOutput;
    multilayerInput = other.multilayerInput;
    weightThreshold = other.weightThreshold;
    unweightedPaths = other.unweightedPaths;
    pathMarkovOrder = other.pathMarkovOrder;
    bipartite = other.bipartite;
    skipAdjustBipartiteFlow = other.skipAdjustBipartiteFlow;
    bipartiteTeleportation = other.bipartiteTeleportation;
    hardPartitions = other.hardPartitions;
    parseWithoutIOStreams = other.parseWithoutIOStreams;
    zeroBasedNodeNumbers = other.zeroBasedNodeNumbers;
    noSelfLinks = other.noSelfLinks;
    ignoreEdgeWeights = other.ignoreEdgeWeights;
    nodeLimit = other.nodeLimit;
    matchableMultilayerIds = other.matchableMultilayerIds;
    preClusterMultilayer = other.preClusterMultilayer;
    metaDataRate = other.metaDataRate;
    unweightedMetaData = other.unweightedMetaData;
    numMetaDataDimensions = other.numMetaDataDimensions;
    assignToNeighbouringModule = other.assignToNeighbouringModule;
    noInfomap = other.noInfomap;
    flowModel = other.flowModel;
    flowModelIsSet = other.flowModelIsSet;
    directed = other.directed;
    undirdir = other.undirdir;
    outdirdir = other.outdirdir;
    rawdir = other.rawdir;
    useNodeWeightsAsFlow = other.useNodeWeightsAsFlow;
    teleportToNodes = other.teleportToNodes;
    selfTeleportationProbability = other.selfTeleportationProbability;
    markovTime = other.markovTime;
    multilayerRelaxRate = other.multilayerRelaxRate;
    multilayerRelaxLimit = other.multilayerRelaxLimit;
    multilayerRelaxLimitUp = other.multilayerRelaxLimitUp;
    multilayerRelaxLimitDown = other.multilayerRelaxLimitDown;
    multilayerJSRelaxRate = other.multilayerJSRelaxRate;
    multilayerRelaxByJensenShannonDivergence = other.multilayerRelaxByJensenShannonDivergence;
    multilayerJSRelaxLimit = other.multilayerJSRelaxLimit;
    twoLevel = other.twoLevel;
    noCoarseTune = other.noCoarseTune;
    directedEdges = other.directedEdges;
    recordedTeleportation = other.recordedTeleportation;
    regularized = other.regularized;
    regularizationStrength = other.regularizationStrength;
    teleportationProbability = other.teleportationProbability;
    entropyBiasCorrection = other.entropyBiasCorrection;
    entropyBiasCorrectionMultiplier = other.entropyBiasCorrectionMultiplier;
    seedToRandomNumberGenerator = other.seedToRandomNumberGenerator;
    minimumCodelengthImprovement = other.minimumCodelengthImprovement;
    minimumSingleNodeCodelengthImprovement = other.minimumSingleNodeCodelengthImprovement;
    randomizeCoreLoopLimit = other.randomizeCoreLoopLimit;
    minimumRelativeTuneIterationImprovement = other.minimumRelativeTuneIterationImprovement;
    fastCoarseTunePartition = other.fastCoarseTunePartition;
    alternateCoarseTuneLevel = other.alternateCoarseTuneLevel;
    coarseTuneLevel = other.coarseTuneLevel;
    preferModularSolution = other.preferModularSolution;
    innerParallelization = other.innerParallelization;
    outDirectory = other.outDirectory;
    outName = other.outName;
    outputFormats = other.outputFormats;
    originallyUndirected = other.originallyUndirected;
    verbosity = other.verbosity;
    verboseNumberPrecision = other.verboseNumberPrecision;
    startDate = other.startDate;
    version = other.version;
    return *this;
  }

  void adaptDefaults();

  void setStateInput() { stateInput = true; }

  void setStateOutput() { stateOutput = true; }

  void setMultilayerInput() { multilayerInput = true; }

  void setFlowModel(FlowModel value)
  {
    flowModel = value;
    flowModelIsSet = true;
  }

  bool isUndirectedClustering() const { return flowModel == FlowModel::undirected; }

  bool isUndirectedFlow() const { return flowModel == FlowModel::undirected || flowModel == FlowModel::undirdir; }

  bool printAsUndirected() const { return isUndirectedClustering(); }

  bool isMultilayerNetwork() const { return multilayerInput || !additionalInput.empty(); }
  bool isBipartite() const { return bipartite; }

  bool haveMemory() const { return stateInput; }
  bool printStates() const { return stateOutput; }

  bool haveMetaData() const { return !metaDataFile.empty() || numMetaDataDimensions != 0; }

  bool haveOutput() const { return !noFileOutput; }

  bool haveModularResultOutput() const
  {
    return printTree || printFlowTree || printNewick || printJson || printCsv || printMap || printClu || printBinaryTree || printBinaryFlowTree;
  }
};

} // namespace infomap

#endif // CONFIG_H_
