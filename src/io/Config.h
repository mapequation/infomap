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

namespace infomap {

struct Config
{
	// Input
	std::string networkFile = "";
	std::vector<std::string> additionalInput;
	std::string inputFormat = ""; // 'pajek', 'link-list', '3gram' or 'multiplex'
	bool withMemory = false;
	bool bipartite = false;
	bool skipAdjustBipartiteFlow = false;
	bool hardPartitions = false;
	bool nonBacktracking = false;
	bool parseWithoutIOStreams = false;
	bool zeroBasedNodeNumbers = false;
	bool includeSelfLinks = false;
	bool ignoreEdgeWeights = false;
	bool skipCompleteDanglingMemoryNodes = false;
	unsigned int nodeLimit = 0;
	bool preClusterMultiplex = false;
	std::string clusterDataFile = "";
	bool clusterDataIsHard = false;
	bool noInfomap = false;

	// Flow models, collapse one-hot encoded values to ENUM?
	bool directed = false;
	bool undirdir = false;
	bool outdirdir = false;
	bool rawdir = false;
	bool teleportToNodes = false;
	double selfTeleportationProbability = -1;
	double markovTime = 1.0;
	double multiplexRelaxRate = 0.15;
	int multiplexRelaxLimit = -1;
	
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
	double minimumSingleNodeCodelengthImprovement = 1e-10;
	bool randomizeCoreLoopLimit = false;
	unsigned int coreLoopLimit = 10;
	unsigned int levelAggregationLimit = 0;
	unsigned int tuneIterationLimit = 0; // num iterations of fine-tune/coarse-tune in two-level partition)
	double minimumRelativeTuneIterationImprovement = 1e-5;
	bool fastCoarseTunePartition = false;
	bool alternateCoarseTuneLevel = false;
	unsigned int coarseTuneLevel = 1;
	unsigned int superLevelLimit = std::numeric_limits<unsigned int>::max(); // Max super level iterations
	bool onlySuperModules = false;
	unsigned int fastHierarchicalSolution = 0;
	bool fastFirstIteration = false;
	unsigned int lowMemoryPriority = false; // Prioritize memory efficient algorithms before fast if > 0
	bool innerParallelization = false;

	// Output
	std::string outDirectory = "";
	std::string outName = "";
	bool originallyUndirected = false;
	bool printTree = false;
	bool printFlowTree = false;
	bool printMap = false;
	bool printClu = false;
	bool printNodeRanks = false;
	bool printFlowNetwork = false;
	bool printPajekNetwork = false;
	bool printStateNetwork = false;
	bool printBinaryTree = false;
	bool printBinaryFlowTree = false; // tree including horizontal links (hierarchical network)
	bool printExpanded = false; // Print the expanded network of memory nodes if possible
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
		setOptimizationLevel(1);
	}

	Config(std::string flags, bool requireFileInput = false)
	{
		*this = Config::fromString(flags, requireFileInput);
	}

	Config(const Config& other) :
		networkFile(other.networkFile),
	 	additionalInput(other.additionalInput),
	 	inputFormat(other.inputFormat),
	 	withMemory(other.withMemory),
		bipartite(other.bipartite),
		skipAdjustBipartiteFlow(other.skipAdjustBipartiteFlow),
		hardPartitions(other.hardPartitions),
	 	nonBacktracking(other.nonBacktracking),
	 	parseWithoutIOStreams(other.parseWithoutIOStreams),
		zeroBasedNodeNumbers(other.zeroBasedNodeNumbers),
		includeSelfLinks(other.includeSelfLinks),
		ignoreEdgeWeights(other.ignoreEdgeWeights),
		skipCompleteDanglingMemoryNodes(other.skipCompleteDanglingMemoryNodes),
		nodeLimit(other.nodeLimit),
		preClusterMultiplex(other.preClusterMultiplex),
	 	clusterDataFile(other.clusterDataFile),
	 	clusterDataIsHard(other.clusterDataIsHard),
	 	noInfomap(other.noInfomap),
		directed(other.directed),
		undirdir(other.undirdir),
		outdirdir(other.outdirdir),
		rawdir(other.rawdir),
		teleportToNodes(other.teleportToNodes),
		selfTeleportationProbability(other.selfTeleportationProbability),
		markovTime(other.markovTime),
		multiplexRelaxRate(other.multiplexRelaxRate),
		multiplexRelaxLimit(other.multiplexRelaxLimit),
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
		lowMemoryPriority(other.lowMemoryPriority),
		innerParallelization(other.innerParallelization),
		outDirectory(other.outDirectory),
		outName(other.outName),
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
		printExpanded(other.printExpanded),
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
		networkFile = other.networkFile;
	 	additionalInput = other.additionalInput;
	 	inputFormat = other.inputFormat;
	 	withMemory = other.withMemory;
	 	bipartite = other.bipartite;
	 	skipAdjustBipartiteFlow = other.skipAdjustBipartiteFlow;
	 	hardPartitions = other.hardPartitions;
	 	nonBacktracking = other.nonBacktracking;
	 	parseWithoutIOStreams = other.parseWithoutIOStreams;
		zeroBasedNodeNumbers = other.zeroBasedNodeNumbers;
		includeSelfLinks = other.includeSelfLinks;
		ignoreEdgeWeights = other.ignoreEdgeWeights;
		skipCompleteDanglingMemoryNodes = other.skipCompleteDanglingMemoryNodes;
		nodeLimit = other.nodeLimit;
		preClusterMultiplex = other.preClusterMultiplex;
	 	clusterDataFile = other.clusterDataFile;
	 	clusterDataIsHard = other.clusterDataIsHard;
	 	noInfomap = other.noInfomap;
		directed = other.directed;
		undirdir = other.undirdir;
		outdirdir = other.outdirdir;
		rawdir = other.rawdir;
		teleportToNodes = other.teleportToNodes;
		selfTeleportationProbability = other.selfTeleportationProbability;
	 	markovTime = other.markovTime;
		multiplexRelaxRate = other.multiplexRelaxRate;
		multiplexRelaxLimit = other.multiplexRelaxLimit;
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
		lowMemoryPriority = other.lowMemoryPriority;
		innerParallelization = other.innerParallelization;
		outDirectory = other.outDirectory;
		outName = other.outName;
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
		printExpanded = other.printExpanded;
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
		networkFile = other.networkFile;
	 	additionalInput = other.additionalInput;
	 	inputFormat = other.inputFormat;
	 	withMemory = other.withMemory;
	 	bipartite = other.bipartite;
	 	skipAdjustBipartiteFlow = other.skipAdjustBipartiteFlow;
	 	hardPartitions = other.hardPartitions;
	 	nonBacktracking = other.nonBacktracking;
	 	parseWithoutIOStreams = other.parseWithoutIOStreams;
		zeroBasedNodeNumbers = other.zeroBasedNodeNumbers;
		includeSelfLinks = other.includeSelfLinks;
		ignoreEdgeWeights = other.ignoreEdgeWeights;
		skipCompleteDanglingMemoryNodes = other.skipCompleteDanglingMemoryNodes;
		nodeLimit = other.nodeLimit;
		preClusterMultiplex = other.preClusterMultiplex;
	 	// clusterDataFile = other.clusterDataFile;
	 	// clusterDataIsHard = other.clusterDataIsHard;
	 	noInfomap = other.noInfomap;
		directed = other.directed;
		undirdir = other.undirdir;
		outdirdir = other.outdirdir;
		rawdir = other.rawdir;
		teleportToNodes = other.teleportToNodes;
		selfTeleportationProbability = other.selfTeleportationProbability;
	 	markovTime = other.markovTime;
		multiplexRelaxRate = other.multiplexRelaxRate;
		multiplexRelaxLimit = other.multiplexRelaxLimit;
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
		lowMemoryPriority = other.lowMemoryPriority;
		innerParallelization = other.innerParallelization;
		outDirectory = other.outDirectory;
		outName = other.outName;
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
		printExpanded = other.printExpanded;
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

	void adaptDefaults()
	{
		if (!haveModularResultOutput())
			printTree = true;

		originallyUndirected = isUndirected();
		if (isMemoryNetwork())
		{
			if (isMultiplexNetwork())
			{
				// Include self-links in multiplex networks as layer and node numbers are unrelated
				includeSelfLinks = true;
				if (!isUndirected())
				{
					// teleportToNodes = true;
					recordedTeleportation = false;
				}
			}
			else
			{
				// teleportToNodes = true;
				recordedTeleportation = false;
				if (isUndirected())
					directed = true;
			}
			if (is3gram()) {
				// Teleport to start of physical chains
				teleportToNodes = true;
			}
		}
		if (isBipartite())
		{
			bipartite = true;
		}

		directedEdges = !isUndirected();
	}

	bool isUndirected() const { return !directed && !undirdir && !outdirdir && !rawdir; }

	void setUndirected() { directed = undirdir = outdirdir = rawdir = false; }

	bool isUndirectedFlow() const { return !directed && !outdirdir && !rawdir; } // isUndirected() || undirdir

	bool printAsUndirected() const { return originallyUndirected; }

	bool parseAsUndirected() const { return originallyUndirected; }

	bool useTeleportation() const { return 	directed; }

	bool is3gram() const { return inputFormat == "3gram"; }
	bool isMultiplexNetwork() const { return inputFormat == "multilayer" || inputFormat == "multiplex" || additionalInput.size() > 0; }
	bool isStateNetwork() const { return inputFormat == "states"; }
	bool isBipartite() const { return inputFormat == "bipartite"; }

	bool isMemoryInput() const { return isStateNetwork() || is3gram() || isMultiplexNetwork(); }

	bool isMemoryNetwork() const { return withMemory || nonBacktracking || isMemoryInput(); }
	
	bool isSimulatedMemoryNetwork() const { return (withMemory || nonBacktracking) && !isMemoryInput(); }

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

	static Config fromString(std::string flags, bool requireFileInput = false);
};

}

#endif /* CONFIG_H_ */
