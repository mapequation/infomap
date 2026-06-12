/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef CONFIG_H_
#define CONFIG_H_

#include "ParsedOption.h"
#include "../utils/Date.h"
#include "../version.h"

#include <stdexcept>
#include <iomanip>
#include <iosfwd>
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
  static constexpr int precomputed = 5;

  int value = 0;

  FlowModel(int val) : value(val) {}
  FlowModel& operator=(int val)
  {
    value = val;
    return *this;
  }

  operator int&() { return value; }
  operator int() const { return value; }
};

std::ostream& operator<<(std::ostream& out, FlowModel f);
const std::vector<std::string>& flowModelNames();
bool parseFlowModel(const std::string& name, FlowModel& flowModel);
const char* flowModelToString(const FlowModel& flowModel);

// Mutable runtime state after parameter parsing and default adaptation. Add or
// change parameter metadata in ParameterCatalog, not by editing this struct alone.
struct Config {
  // Input
  bool isCLI = false;
  std::string networkFile;
  std::vector<std::string> additionalInput;
  bool stateInput = false;
  bool stateOutput = false;
  bool multilayerInput = false;
  double weightThreshold = 0.0;
  bool bipartite = false;
  bool skipAdjustBipartiteFlow = false;
  bool bipartiteTeleportation = false;
  bool noSelfLinks = false; // Replaces includeSelfLinks
  unsigned int nodeLimit = 0;
  unsigned int matchableMultilayerIds = 0;
  std::string clusterDataFile;
  std::string metaDataFile;
  double metaDataRate = 1.0;
  bool unweightedMetaData = false;
  unsigned int numMetaDataDimensions = 0;
  bool clusterDataIsHard = false; // FIXME Not used
  bool assignToNeighbouringModule = false;
  bool noInfomap = false;

  FlowModel flowModel = FlowModel::undirected;
  bool flowModelIsSet = false;
  bool directed = false;
  bool useNodeWeightsAsFlow = false;
  bool teleportToNodes = false;
  double markovTime = 1.0;
  bool variableMarkovTime = false;
  double variableMarkovTimeDamping = 1.0; // 0 for linear scaling, 1 for log scaled.
  double variableMarkovTimeMinLocalScale = 1; // Correspond to two links in undirected unweighted networks. Avoids division by zero.
  bool markovTimeNoSelfLinks = false;
  double multilayerRelaxRate = 0.15;
  int multilayerRelaxLimit = -1; // Amount of layers allowed to jump up or down
  int multilayerRelaxLimitUp = -1; // One-sided limit to higher layers
  int multilayerRelaxLimitDown = -1; // One-sided limit to lower layers
  double multilayerJSRelaxRate = 0.15;
  bool multilayerRelaxByJensenShannonDivergence = false;
  int multilayerJSRelaxLimit = -1;
  unsigned int maxFlowIterations = 400;

  // Clustering
  bool twoLevel = false;
  bool noCoarseTune = false;
  bool recordedTeleportation = false;
  bool regularized = false; // Add a Bayesian prior network with recorded teleportation (sets recordedTeleportation and teleportToNodes to true)
  double regularizationStrength = 1.0; // Scale Bayesian prior constant ln(N)/N with this factor
  double teleportationProbability = 0.15;
  unsigned int preferredNumberOfModules = 0;
  bool entropyBiasCorrection = false;
  double entropyBiasCorrectionMultiplier = 1;
  unsigned long seedToRandomNumberGenerator = 123;
#if INFOMAP_FEATURE_LOSSY_MAP_EQUATION
  bool lossy = false; // Lossy map equation: noise modules share one visit codeword
  double lossyLambda = 1.0; // Price of one bit of forgone dynamics, in transmitted bits
#endif

  // Performance and accuracy
  unsigned int numTrials = 1;
  double minimumCodelengthImprovement = 1e-10;
  double minimumSingleNodeCodelengthImprovement = 1e-16;
  bool randomizeCoreLoopLimit = false;
  unsigned int coreLoopLimit = 10;
  unsigned int levelAggregationLimit = 0;
  unsigned int tuneIterationLimit = 0; // Iterations of fine-tune/coarse-tune in two-level partition
  double minimumRelativeTuneIterationImprovement = 1e-5;
  bool onlySuperModules = false;
  unsigned int fastHierarchicalSolution = 0;
  bool preferModularSolution = false;
  bool innerParallelization = false;
  bool parallelTrials = false;
#if INFOMAP_FEATURE_TEST_FEATURE
  bool testFeature = false;
#endif
  unsigned int numRandomMoves = 5; // Amount of random moves to try in core loop, used if regularized/recorded teleportation.
  unsigned int maxDegreeForRandomMoves = 2; // Only try random moves for nodes with degree less than or equal to this value.

  // Output
  std::string outDirectory;
  std::string outName;
  std::string outputFormats;
  bool printTree = false;
  bool printFlowTree = false;
  bool printNewick = false;
  bool printJson = false;
  bool printCsv = false;
  bool printClu = false;
  bool printAllTrials = false;
#ifndef SWIG
  bool noOverwriteOutput = false;
  bool printConfigFingerprint = false;
  std::string timingJsonPath;
  std::string summaryJsonPath;
  std::string runManifestPath;
  bool memoryReport = false;
  std::string numThreadsArg = "auto"; // raw --num-threads / --threads argument
  unsigned int numThreads = 0; // 0 = auto; positive = explicit request
  unsigned int trialOffset = 0; // global index of this shard's first trial
  std::string trialResultsPath; // --trial-results <file.json>
  bool noFinalOutput = false; // suppress aggregate final write (per-trial still written)
#endif
  int cluLevel = 1; // Write modules at specified depth from root. 1, 2, ... or -1 for bottom level
  bool printFlowNetwork = false;
  bool printPajekNetwork = false;
  bool printStateNetwork = false;
  bool noFileOutput = false;
  unsigned int verbosity = 0;
  unsigned int verboseNumberPrecision = 9;
  bool silent = false;
  // Pretty (structured) console output is the only console rendering and is
  // always on; -v/-vv add diagnostics on top. --pretty/--no-pretty are
  // deprecated no-ops kept for backward compatibility (see ParameterCatalog).
  bool prettyOutput = true;
  bool hideBipartiteNodes = false;

  // Other
  Date startDate;
  std::string version = INFOMAP_VERSION;
  std::string parsedString;
  std::vector<ParsedOption> parsedOptions;

  Config() = default;

  explicit Config(const std::string& flags, bool isCLI = false);

  Config& cloneAsNonMain(const Config& other)
  {
    isCLI = other.isCLI;
    networkFile = other.networkFile;
    additionalInput = other.additionalInput;
    stateInput = other.stateInput;
    stateOutput = other.stateOutput;
    multilayerInput = other.multilayerInput;
    weightThreshold = other.weightThreshold;
    bipartite = other.bipartite;
    skipAdjustBipartiteFlow = other.skipAdjustBipartiteFlow;
    bipartiteTeleportation = other.bipartiteTeleportation;
    noSelfLinks = other.noSelfLinks;
    nodeLimit = other.nodeLimit;
    matchableMultilayerIds = other.matchableMultilayerIds;
    metaDataRate = other.metaDataRate;
    unweightedMetaData = other.unweightedMetaData;
    numMetaDataDimensions = other.numMetaDataDimensions;
    assignToNeighbouringModule = other.assignToNeighbouringModule;
    noInfomap = other.noInfomap;
    flowModel = other.flowModel;
    flowModelIsSet = other.flowModelIsSet;
    directed = other.directed;
    useNodeWeightsAsFlow = other.useNodeWeightsAsFlow;
    teleportToNodes = other.teleportToNodes;
    markovTime = other.markovTime;
    variableMarkovTime = other.variableMarkovTime;
    variableMarkovTimeDamping = other.variableMarkovTimeDamping;
    markovTimeNoSelfLinks = other.markovTimeNoSelfLinks;
    multilayerRelaxRate = other.multilayerRelaxRate;
    multilayerRelaxLimit = other.multilayerRelaxLimit;
    multilayerRelaxLimitUp = other.multilayerRelaxLimitUp;
    multilayerRelaxLimitDown = other.multilayerRelaxLimitDown;
    multilayerJSRelaxRate = other.multilayerJSRelaxRate;
    multilayerRelaxByJensenShannonDivergence = other.multilayerRelaxByJensenShannonDivergence;
    multilayerJSRelaxLimit = other.multilayerJSRelaxLimit;
    maxFlowIterations = other.maxFlowIterations;
    twoLevel = other.twoLevel;
    noCoarseTune = other.noCoarseTune;
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
    preferModularSolution = other.preferModularSolution;
    innerParallelization = other.innerParallelization;
#if INFOMAP_FEATURE_TEST_FEATURE
    testFeature = other.testFeature;
#endif
#if INFOMAP_FEATURE_LOSSY_MAP_EQUATION
    lossy = other.lossy;
    lossyLambda = other.lossyLambda;
#endif
    numRandomMoves = other.numRandomMoves;
    maxDegreeForRandomMoves = other.maxDegreeForRandomMoves;
    outDirectory = other.outDirectory;
    outName = other.outName;
    outputFormats = other.outputFormats;
#ifndef SWIG
    noOverwriteOutput = other.noOverwriteOutput;
    printConfigFingerprint = other.printConfigFingerprint;
    timingJsonPath = other.timingJsonPath;
    summaryJsonPath = other.summaryJsonPath;
    runManifestPath = other.runManifestPath;
    memoryReport = other.memoryReport;
    numThreadsArg = other.numThreadsArg;
    numThreads = other.numThreads;
    trialOffset = other.trialOffset;
    trialResultsPath = other.trialResultsPath;
    noFinalOutput = other.noFinalOutput;
#endif
    verbosity = other.verbosity;
    verboseNumberPrecision = other.verboseNumberPrecision;
    prettyOutput = other.prettyOutput;
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
#ifndef SWIG
  bool isRegularizedMultilayerFlow() const { return isMultilayerNetwork() && regularized; }
#endif

  bool haveMemory() const { return stateInput; }
  bool printStates() const { return stateOutput; }

  bool haveMetaData() const { return !metaDataFile.empty() || numMetaDataDimensions != 0; }

  bool haveOutput() const { return !noFileOutput; }

#ifndef SWIG
  bool overwriteOutput() const { return !noOverwriteOutput; }
#endif

  bool haveModularResultOutput() const
  {
    return printTree || printFlowTree || printNewick || printJson || printCsv || printClu;
  }
};

} // namespace infomap

#endif // CONFIG_H_
