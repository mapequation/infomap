/**********************************************************************************

 Infomap software package for multi-level network clustering

 Copyright (c) 2013, 2014 Daniel Edler, Martin Rosvall

 For more information, see <http://www.mapequation.org>


 This file is part of Infomap software package.

 Infomap software package is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Infomap software package is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with Infomap software package.  If not, see <http://www.gnu.org/licenses/>.

**********************************************************************************/


#ifndef CONFIG_H_
#define CONFIG_H_

#include <iomanip>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <limits>

#include "../utils/Date.h"
#include "version.h"
#include "ProgramInterface.h"
#include "../utils/exceptions.h"

namespace infomap {

struct FlowModel {
  static constexpr int undirected = 0;
  static constexpr int directed = 1;
  static constexpr int undirdir = 2;
  static constexpr int outdirdir = 3;
  static constexpr int rawdir = 4;

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

struct OptimizationLevel {
  static constexpr int FullCoarseTune = 0;
  static constexpr int FastCoarseTune = 1;
  static constexpr int NoTune = 2;
  static constexpr int NoAggregationNoTune = 3;

  int value = 0;

  operator int&() { return value; }
  operator int() const { return value; }
};

struct Config {
  // Input
  bool isCLI = false;
  std::string networkFile;
  std::vector<std::string> additionalInput;
  std::string inputFormat;
  bool memoryInput = false;
  bool multilayerInput = false;
  //bool withMemory = false; // unused
  double weightThreshold = 0.0;
  bool unweightedPaths = false;
  unsigned int pathMarkovOrder = 1;
  bool bipartite = false;
  bool skipAdjustBipartiteFlow = false;
  bool bipartiteTeleportation = false;
  bool hardPartitions = false;
  //bool nonBacktracking = false; // unused
  bool parseWithoutIOStreams = false;
  bool zeroBasedNodeNumbers = false;
  bool includeSelfLinks = false;
  bool ignoreEdgeWeights = false;
  unsigned int nodeLimit = 0;
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
  double teleportationProbability = 0.15;
  unsigned int preferredNumberOfModules = 0;
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
  unsigned int superLevelLimit = std::numeric_limits<unsigned int>::max(); // Max super level iterations
  bool onlySuperModules = false;
  unsigned int fastHierarchicalSolution = 0;
  bool fastFirstIteration = false;
  bool preferModularSolution = false;
  // unsigned int lowMemoryPriority = false; // Prioritize memory efficient algorithms before fast if > 0
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
  bool printNodeRanks = false;
  bool printFlowNetwork = false;
  bool printPajekNetwork = false;
  bool printStateNetwork = false;
  bool printBinaryTree = false;
  bool printBinaryFlowTree = false; // tree including horizontal links (hierarchical network)
  bool noFileOutput = false;
  unsigned int verbosity = 0;
  unsigned int verboseNumberPrecision = 9;
  bool silent = false;
  bool benchmark = false;
  bool hideBipartiteNodes = false;

  unsigned int maxNodeIndexVisible = 0;
  unsigned int minBipartiteNodeIndex = 0;

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
    inputFormat = other.inputFormat;
    memoryInput = other.memoryInput;
    multilayerInput = other.multilayerInput;
    //withMemory = other.withMemory;
    weightThreshold = other.weightThreshold;
    unweightedPaths = other.unweightedPaths;
    pathMarkovOrder = other.pathMarkovOrder;
    bipartite = other.bipartite;
    skipAdjustBipartiteFlow = other.skipAdjustBipartiteFlow;
    bipartiteTeleportation = other.bipartiteTeleportation;
    hardPartitions = other.hardPartitions;
    //nonBacktracking = other.nonBacktracking;
    parseWithoutIOStreams = other.parseWithoutIOStreams;
    zeroBasedNodeNumbers = other.zeroBasedNodeNumbers;
    includeSelfLinks = other.includeSelfLinks;
    ignoreEdgeWeights = other.ignoreEdgeWeights;
    nodeLimit = other.nodeLimit;
    preClusterMultilayer = other.preClusterMultilayer;
    // clusterDataFile = other.clusterDataFile;
    // metaDataFile = other.metaDataFile;
    metaDataRate = other.metaDataRate;
    unweightedMetaData = other.unweightedMetaData;
    numMetaDataDimensions = other.numMetaDataDimensions;
    // clusterDataIsHard = other.clusterDataIsHard;
    assignToNeighbouringModule = other.assignToNeighbouringModule;
    noInfomap = other.noInfomap;
    flowModel = other.flowModel;
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
    teleportationProbability = other.teleportationProbability;
    // preferredNumberOfModules = other.preferredNumberOfModules;
    seedToRandomNumberGenerator = other.seedToRandomNumberGenerator;
    // numTrials = other.numTrials;
    minimumCodelengthImprovement = other.minimumCodelengthImprovement;
    minimumSingleNodeCodelengthImprovement = other.minimumSingleNodeCodelengthImprovement;
    randomizeCoreLoopLimit = other.randomizeCoreLoopLimit;
    // coreLoopLimit = other.coreLoopLimit;
    // levelAggregationLimit = other.levelAggregationLimit;
    // tuneIterationLimit = other.tuneIterationLimit;
    minimumRelativeTuneIterationImprovement = other.minimumRelativeTuneIterationImprovement;
    fastCoarseTunePartition = other.fastCoarseTunePartition;
    alternateCoarseTuneLevel = other.alternateCoarseTuneLevel;
    coarseTuneLevel = other.coarseTuneLevel;
    // superLevelLimit = other.superLevelLimit;
    // onlySuperModules = other.onlySuperModules;
    // fastHierarchicalSolution = other.fastHierarchicalSolution;
    // fastFirstIteration = other.fastFirstIteration;
    preferModularSolution = other.preferModularSolution;
    //lowMemoryPriority = other.lowMemoryPriority;
    innerParallelization = other.innerParallelization;
    outDirectory = other.outDirectory;
    outName = other.outName;
    outputFormats = other.outputFormats;
    originallyUndirected = other.originallyUndirected;
    // printTree = other.printTree;
    // printFlowTree = other.printFlowTree;
    // printNewick = other.printNewick;
    // printMap = other.printMap;
    // printClu = other.printClu;
    // printNodeRanks = other.printNodeRanks;
    // printFlowNetwork = other.printFlowNetwork;
    // printPajekNetwork = other.printPajekNetwork;
    // printStateNetwork = other.printStateNetwork;
    // printBinaryTree = other.printBinaryTree;
    // printBinaryFlowTree = other.printBinaryFlowTree;
    // noFileOutput = other.noFileOutput;
    verbosity = other.verbosity;
    verboseNumberPrecision = other.verboseNumberPrecision;
    // silent = other.silent;
    // benchmark = other.benchmark;
    // maxNodeIndexVisible = other.maxNodeIndexVisible;
    // minBipartiteNodeIndex = other.minBipartiteNodeIndex;
    startDate = other.startDate;
    version = other.version;
    // parsedString = other.parsedString;
    // parsedOptions = other.parsedOptions;
    // error = other.error;
    return *this;
  }

  // Set all optimization options at once with different accuracy to performance trade-off
  void setOptimizationLevel(OptimizationLevel level)
  {
    switch (level) {
    case OptimizationLevel::FullCoarseTune:
      randomizeCoreLoopLimit = false;
      coreLoopLimit = 0;
      levelAggregationLimit = 0;
      tuneIterationLimit = 0;
      minimumRelativeTuneIterationImprovement = 1.0e-6;
      fastCoarseTunePartition = false;
      alternateCoarseTuneLevel = true;
      coarseTuneLevel = 3;
      break;
    case OptimizationLevel::FastCoarseTune:
      randomizeCoreLoopLimit = true;
      coreLoopLimit = 10;
      levelAggregationLimit = 0;
      tuneIterationLimit = 0;
      minimumRelativeTuneIterationImprovement = 1.0e-5;
      fastCoarseTunePartition = true;
      alternateCoarseTuneLevel = false;
      coarseTuneLevel = 1;
      break;
    case OptimizationLevel::NoTune:
      randomizeCoreLoopLimit = true;
      coreLoopLimit = 10;
      levelAggregationLimit = 0;
      tuneIterationLimit = 1;
      fastCoarseTunePartition = true;
      alternateCoarseTuneLevel = false;
      coarseTuneLevel = 1;
      break;
    case OptimizationLevel::NoAggregationNoTune:
      randomizeCoreLoopLimit = true;
      coreLoopLimit = 10;
      levelAggregationLimit = 1;
      tuneIterationLimit = 1;
      fastCoarseTunePartition = true;
      alternateCoarseTuneLevel = false;
      coarseTuneLevel = 1;
      break;
    default:
      break;
    }
  }

  void adaptDefaults();

  void setMemoryInput() { memoryInput = true; }

  void setMultilayerInput() { multilayerInput = true; }

  bool isUndirectedClustering() const { return flowModel == FlowModel::undirected; }

  bool isUndirectedFlow() const { return flowModel == FlowModel::undirected || flowModel == FlowModel::undirdir; }

  bool printAsUndirected() const { return isUndirectedClustering(); }

  bool is3gram() const { return inputFormat == "3gram"; }
  bool isPath() const { return inputFormat == "path"; }
  bool isMultilayerNetwork() const { return multilayerInput || inputFormat == "multilayer" || inputFormat == "multiplex" || !additionalInput.empty(); }
  bool isStateNetwork() const { return inputFormat == "states"; }
  bool isBipartite() const { return inputFormat == "bipartite" || bipartite; }

  bool haveMemory() const { return memoryInput; }

  bool haveMetaData() const { return !metaDataFile.empty() || numMetaDataDimensions != 0; }

  bool haveOutput() const { return !noFileOutput; }

  bool haveModularResultOutput() const
  {
    return printTree || printFlowTree || printNewick || printJson || printCsv || printMap || printClu || printBinaryTree || printBinaryFlowTree;
  }
};

} // namespace infomap

#endif /* CONFIG_H_ */
