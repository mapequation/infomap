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
#include <vector>
#include <limits>

#include "../utils/Date.h"
#include "version.h"
#include "ProgramInterface.h"
#include "../utils/exceptions.h"

namespace infomap {

struct FlowModel {
	static const std::string undirected;
	static const std::string directed;
	static const std::string undirdir;
	static const std::string outdirdir;
	static const std::string rawdir;
};

struct Config
{
	// Input
	bool isCLI = false;
	std::string networkFile = "";
	std::vector<std::string> additionalInput;
	std::string inputFormat = "";
	bool memoryInput = false;
	bool multilayerInput = false;
	bool withMemory = false;
	double weightThreshold = 0.0;
	bool unweightedPaths = false;
	unsigned int pathMarkovOrder = 1;
	bool bipartite = false;
	bool skipAdjustBipartiteFlow = false;
	bool hardPartitions = false;
	bool nonBacktracking = false;
	bool parseWithoutIOStreams = false;
	bool zeroBasedNodeNumbers = false;
	bool includeSelfLinks = false;
	bool ignoreEdgeWeights = false;
	unsigned int nodeLimit = 0;
	bool preClusterMultilayer = false;
	std::string clusterDataFile = "";
	std::string metaDataFile = "";
	double metaDataRate = 1.0;
	bool unweightedMetaData = false;
	unsigned int numMetaDataDimensions = 0;
	bool clusterDataIsHard = false;
	bool assignToNeighbouringModule = false;
	bool noInfomap = false;

	std::string flowModel = FlowModel::undirected;
	// TODO: Remove variables for 'one-hot encoded' flow models
	bool directed = false;
	bool undirdir = false;
	bool outdirdir = false;
	bool rawdir = false;
	bool teleportToNodes = false;
	double selfTeleportationProbability = -1;
	double markovTime = 1.0;
	double multilayerRelaxRate = 0.15;
	int multilayerRelaxLimit = -1; // Amount of layers allowed to jump up or down
	int multilayerRelaxLimitUp = -1; // One-sided limit to higher layers
	int multilayerRelaxLimitDown = -1; // One-sided limit to lower layers
	double multilayerJSRelaxRate = 0.15;
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
	std::string outDirectory = "";
	std::string outName = "";
	std::string outputFormats = "";
	bool originallyUndirected = false;
	bool printTree = false;
	bool printFlowTree = false;
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

	unsigned int maxNodeIndexVisible = 0;
	bool showBiNodes = false;
	unsigned int minBipartiteNodeIndex = 0;

	// Other
	Date startDate;
	std::string version = INFOMAP_VERSION;
	std::string parsedString = "";
	std::vector<ParsedOption> parsedOptions;
	std::string error = "";

	Config()
	{
		// adaptDefaults();
	}

	Config(std::string flags, bool isCLI = false)
	{
		*this = Config::fromString(flags, isCLI);
	}

	Config(const Config& other) :
		isCLI(other.isCLI),
		networkFile(other.networkFile),
	 	additionalInput(other.additionalInput),
	 	inputFormat(other.inputFormat),
	 	memoryInput(other.memoryInput),
	 	multilayerInput(other.multilayerInput),
	 	withMemory(other.withMemory),
		weightThreshold(other.weightThreshold),
		unweightedPaths(other.unweightedPaths),
		pathMarkovOrder(other.pathMarkovOrder),
		bipartite(other.bipartite),
		skipAdjustBipartiteFlow(other.skipAdjustBipartiteFlow),
		hardPartitions(other.hardPartitions),
	 	nonBacktracking(other.nonBacktracking),
	 	parseWithoutIOStreams(other.parseWithoutIOStreams),
		zeroBasedNodeNumbers(other.zeroBasedNodeNumbers),
		includeSelfLinks(other.includeSelfLinks),
		ignoreEdgeWeights(other.ignoreEdgeWeights),
		nodeLimit(other.nodeLimit),
		preClusterMultilayer(other.preClusterMultilayer),
	 	clusterDataFile(other.clusterDataFile),
	 	metaDataFile(other.metaDataFile),
	 	metaDataRate(other.metaDataRate),
	 	unweightedMetaData(other.unweightedMetaData),
	 	numMetaDataDimensions(other.numMetaDataDimensions),
	 	clusterDataIsHard(other.clusterDataIsHard),
	 	assignToNeighbouringModule(other.assignToNeighbouringModule),
	 	noInfomap(other.noInfomap),
		flowModel(other.flowModel),
		directed(other.directed),
		undirdir(other.undirdir),
		outdirdir(other.outdirdir),
		rawdir(other.rawdir),
		teleportToNodes(other.teleportToNodes),
		selfTeleportationProbability(other.selfTeleportationProbability),
		markovTime(other.markovTime),
		multilayerRelaxRate(other.multilayerRelaxRate),
		multilayerRelaxLimit(other.multilayerRelaxLimit),
		multilayerRelaxLimitUp(other.multilayerRelaxLimitUp),
		multilayerRelaxLimitDown(other.multilayerRelaxLimitDown),
		multilayerJSRelaxRate(other.multilayerJSRelaxRate),
		multilayerJSRelaxLimit(other.multilayerJSRelaxLimit),
	 	twoLevel(other.twoLevel),
		noCoarseTune(other.noCoarseTune),
		directedEdges(other.directedEdges),
		recordedTeleportation(other.recordedTeleportation),
		teleportationProbability(other.teleportationProbability),
		preferredNumberOfModules(other.preferredNumberOfModules),
		seedToRandomNumberGenerator(other.seedToRandomNumberGenerator),
		numTrials(other.numTrials),
		minimumCodelengthImprovement(other.minimumCodelengthImprovement),
		minimumSingleNodeCodelengthImprovement(other.minimumSingleNodeCodelengthImprovement),
		randomizeCoreLoopLimit(other.randomizeCoreLoopLimit),
		coreLoopLimit(other.coreLoopLimit),
		levelAggregationLimit(other.levelAggregationLimit),
		tuneIterationLimit(other.tuneIterationLimit),
		minimumRelativeTuneIterationImprovement(other.minimumRelativeTuneIterationImprovement),
		fastCoarseTunePartition(other.fastCoarseTunePartition),
		alternateCoarseTuneLevel(other.alternateCoarseTuneLevel),
		coarseTuneLevel(other.coarseTuneLevel),
		superLevelLimit(other.superLevelLimit),
		onlySuperModules(other.onlySuperModules),
		fastHierarchicalSolution(other.fastHierarchicalSolution),
		fastFirstIteration(other.fastFirstIteration),
		preferModularSolution(other.preferModularSolution),
		//lowMemoryPriority(other.lowMemoryPriority),
		innerParallelization(other.innerParallelization),
		outDirectory(other.outDirectory),
		outName(other.outName),
		outputFormats(other.outputFormats),
		originallyUndirected(other.originallyUndirected),
		printTree(other.printTree),
		printFlowTree(other.printFlowTree),
		printMap(other.printMap),
		printClu(other.printClu),
		printNodeRanks(other.printNodeRanks),
		printFlowNetwork(other.printFlowNetwork),
		printPajekNetwork(other.printPajekNetwork),
		printStateNetwork(other.printStateNetwork),
		printBinaryTree(other.printBinaryTree),
		printBinaryFlowTree(other.printBinaryFlowTree),
		noFileOutput(other.noFileOutput),
		verbosity(other.verbosity),
		verboseNumberPrecision(other.verboseNumberPrecision),
		silent(other.silent),
		benchmark(other.benchmark),
		maxNodeIndexVisible(other.maxNodeIndexVisible),
		showBiNodes(other.showBiNodes),
		minBipartiteNodeIndex(other.minBipartiteNodeIndex),
		startDate(other.startDate),
		version(other.version),
		parsedString(other.parsedString),
		parsedOptions(other.parsedOptions),
		error(other.error)

	{
	}

	Config& operator=(const Config& other)
	{
		isCLI = other.isCLI;
		networkFile = other.networkFile;
	 	additionalInput = other.additionalInput;
	 	inputFormat = other.inputFormat;
	 	memoryInput = other.memoryInput;
	 	multilayerInput = other.multilayerInput;
	 	withMemory = other.withMemory;
		weightThreshold = other.weightThreshold;
		unweightedPaths = other.unweightedPaths;
		pathMarkovOrder = other.pathMarkovOrder;
	 	bipartite = other.bipartite;
	 	skipAdjustBipartiteFlow = other.skipAdjustBipartiteFlow;
	 	hardPartitions = other.hardPartitions;
	 	nonBacktracking = other.nonBacktracking;
	 	parseWithoutIOStreams = other.parseWithoutIOStreams;
		zeroBasedNodeNumbers = other.zeroBasedNodeNumbers;
		includeSelfLinks = other.includeSelfLinks;
		ignoreEdgeWeights = other.ignoreEdgeWeights;
		nodeLimit = other.nodeLimit;
		preClusterMultilayer = other.preClusterMultilayer;
	 	clusterDataFile = other.clusterDataFile;
	 	metaDataFile = other.metaDataFile;
	 	metaDataRate = other.metaDataRate;
	 	unweightedMetaData = other.unweightedMetaData;
	 	numMetaDataDimensions = other.numMetaDataDimensions;
	 	clusterDataIsHard = other.clusterDataIsHard;
	 	assignToNeighbouringModule = other.assignToNeighbouringModule;
	 	noInfomap = other.noInfomap;
		flowModel = other.flowModel;
		directed = other.directed;
		undirdir = other.undirdir;
		outdirdir = other.outdirdir;
		rawdir = other.rawdir;
		teleportToNodes = other.teleportToNodes;
		selfTeleportationProbability = other.selfTeleportationProbability;
	 	markovTime = other.markovTime;
		multilayerRelaxRate = other.multilayerRelaxRate;
		multilayerRelaxLimit = other.multilayerRelaxLimit;
		multilayerRelaxLimitUp = other.multilayerRelaxLimitUp;
		multilayerRelaxLimitDown = other.multilayerRelaxLimitDown;
		multilayerJSRelaxRate = other.multilayerJSRelaxRate;
		multilayerJSRelaxLimit = other.multilayerJSRelaxLimit;
	 	twoLevel = other.twoLevel;
		noCoarseTune = other.noCoarseTune;
		directedEdges = other.directedEdges;
		recordedTeleportation = other.recordedTeleportation;
		teleportationProbability = other.teleportationProbability;
	 	preferredNumberOfModules = other.preferredNumberOfModules;
		seedToRandomNumberGenerator = other.seedToRandomNumberGenerator;
		numTrials = other.numTrials;
		minimumCodelengthImprovement = other.minimumCodelengthImprovement;
		minimumSingleNodeCodelengthImprovement = other.minimumSingleNodeCodelengthImprovement;
		randomizeCoreLoopLimit = other.randomizeCoreLoopLimit;
		coreLoopLimit = other.coreLoopLimit;
		levelAggregationLimit = other.levelAggregationLimit;
		tuneIterationLimit = other.tuneIterationLimit;
		minimumRelativeTuneIterationImprovement = other.minimumRelativeTuneIterationImprovement;
		fastCoarseTunePartition = other.fastCoarseTunePartition;
		alternateCoarseTuneLevel = other.alternateCoarseTuneLevel;
		coarseTuneLevel = other.coarseTuneLevel;
		superLevelLimit = other.superLevelLimit;
		onlySuperModules = other.onlySuperModules;
		fastHierarchicalSolution = other.fastHierarchicalSolution;
		fastFirstIteration = other.fastFirstIteration;
		preferModularSolution = other.preferModularSolution;
		//lowMemoryPriority = other.lowMemoryPriority;
		innerParallelization = other.innerParallelization;
		outDirectory = other.outDirectory;
		outName = other.outName;
		outputFormats = other.outputFormats;
		originallyUndirected = other.originallyUndirected;
		printTree = other.printTree;
		printFlowTree = other.printFlowTree;
		printMap = other.printMap;
		printClu = other.printClu;
		printNodeRanks = other.printNodeRanks;
		printFlowNetwork = other.printFlowNetwork;
		printPajekNetwork = other.printPajekNetwork;
		printStateNetwork = other.printStateNetwork;
		printBinaryTree = other.printBinaryTree;
		printBinaryFlowTree = other.printBinaryFlowTree;
		noFileOutput = other.noFileOutput;
		verbosity = other.verbosity;
		verboseNumberPrecision = other.verboseNumberPrecision;
		silent = other.silent;
		benchmark = other.benchmark;
	 	maxNodeIndexVisible = other.maxNodeIndexVisible;
	 	showBiNodes = other.showBiNodes;
	 	minBipartiteNodeIndex = other.minBipartiteNodeIndex;
		startDate = other.startDate;
		version = other.version;
		parsedString = other.parsedString;
		parsedOptions = other.parsedOptions;
		error = other.error;
		return *this;
	}

	Config& cloneAsNonMain(const Config& other)
	{
		isCLI = other.isCLI;
		networkFile = other.networkFile;
	 	additionalInput = other.additionalInput;
	 	inputFormat = other.inputFormat;
	 	memoryInput = other.memoryInput;
	 	multilayerInput = other.multilayerInput;
	 	withMemory = other.withMemory;
		weightThreshold = other.weightThreshold;
		unweightedPaths = other.unweightedPaths;
		pathMarkovOrder = other.pathMarkovOrder;
	 	bipartite = other.bipartite;
	 	skipAdjustBipartiteFlow = other.skipAdjustBipartiteFlow;
	 	hardPartitions = other.hardPartitions;
	 	nonBacktracking = other.nonBacktracking;
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
		teleportToNodes = other.teleportToNodes;
		selfTeleportationProbability = other.selfTeleportationProbability;
	 	markovTime = other.markovTime;
		multilayerRelaxRate = other.multilayerRelaxRate;
		multilayerRelaxLimit = other.multilayerRelaxLimit;
		multilayerRelaxLimitUp = other.multilayerRelaxLimitUp;
		multilayerRelaxLimitDown = other.multilayerRelaxLimitDown;
		multilayerJSRelaxRate = other.multilayerJSRelaxRate;
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
	 	// showBiNodes = other.showBiNodes;
	 	// minBipartiteNodeIndex = other.minBipartiteNodeIndex;
		startDate = other.startDate;
		version = other.version;
		// parsedString = other.parsedString;
		// parsedOptions = other.parsedOptions;
		// error = other.error;
		return *this;
	}

	// Set all optimization options at once with different accuracy to performance trade-off
	void setOptimizationLevel(unsigned int level)
	{
		switch (level)
		{
		case 0: // full coarse-tune
			randomizeCoreLoopLimit = false;
			coreLoopLimit = 0;
			levelAggregationLimit = 0;
			tuneIterationLimit = 0;
			minimumRelativeTuneIterationImprovement = 1.0e-6;
			fastCoarseTunePartition = false;
			alternateCoarseTuneLevel = true;
			coarseTuneLevel = 3;
			break;
		case 1: // fast coarse-tune
			randomizeCoreLoopLimit = true;
			coreLoopLimit = 10;
			levelAggregationLimit = 0;
			tuneIterationLimit = 0;
			minimumRelativeTuneIterationImprovement = 1.0e-5;
			fastCoarseTunePartition = true;
			alternateCoarseTuneLevel = false;
			coarseTuneLevel = 1;
			break;
		case 2: // no tuning
			randomizeCoreLoopLimit = true;
			coreLoopLimit = 10;
			levelAggregationLimit = 0;
			tuneIterationLimit = 1;
			fastCoarseTunePartition = true;
			alternateCoarseTuneLevel = false;
			coarseTuneLevel = 1;
			break;
		case 3: // no aggregation nor any tuning
			randomizeCoreLoopLimit = true;
			coreLoopLimit = 10;
			levelAggregationLimit = 1;
			tuneIterationLimit = 1;
			fastCoarseTunePartition = true;
			alternateCoarseTuneLevel = false;
			coarseTuneLevel = 1;
			break;
		}
	}

	void adaptDefaults();

	bool setDirectedInput() {
		if (flowModel == FlowModel::undirected || flowModel == FlowModel::undirdir) {
			flowModel = FlowModel::directed;
			return true;
		}
		return false;
	}

	void setMemoryInput() {
		memoryInput = true;
	}

	void setMultilayerInput() {
		multilayerInput = true;
	}

	// bool isUndirected() const { return !directed && !undirdir && !outdirdir && !rawdir; }
	bool isUndirectedClustering() const { return flowModel == FlowModel::undirected; }

	bool isUndirectedFlow() const { return flowModel == FlowModel::undirected || flowModel == FlowModel::undirdir; }

	// bool printAsUndirected() const { return originallyUndirected; }
	bool printAsUndirected() const { return isUndirectedClustering(); }

	// bool useTeleportation() const { return 	directed; }

	bool is3gram() const { return inputFormat == "3gram"; }
	bool isPath() const { return inputFormat == "path"; }
	bool isMultilayerNetwork() const { return multilayerInput || inputFormat == "multilayer" || inputFormat == "multiplex" || additionalInput.size() > 0; }
	bool isStateNetwork() const { return inputFormat == "states"; }
	bool isBipartite() const { return inputFormat == "bipartite" || bipartite; }

	bool isMemoryNetwork() const { return isStateNetwork() || is3gram() || isPath() || isMultilayerNetwork() || withMemory || nonBacktracking || memoryInput; }

	// bool isSimulatedMemoryNetwork() const { return (withMemory || nonBacktracking) && !isMemoryInput(); }

	bool haveMetaData() const { return metaDataFile != "" || numMetaDataDimensions != 0; }

	bool haveOutput() const
	{
		return !noFileOutput;
	}

	bool haveModularResultOutput() const
	{
		return printTree ||
				printFlowTree ||
				printMap ||
				printClu ||
				printBinaryTree ||
				printBinaryFlowTree;
	}

	ElapsedTime elapsedTime() const { return Date() - startDate; }

	void setError(const std::string& err)
	{
		error = err;
	}

	bool haveError() const
	{
		return error != "";
	}

	static Config fromString(std::string flags, bool isCLI = false);
};

}

#endif /* CONFIG_H_ */
