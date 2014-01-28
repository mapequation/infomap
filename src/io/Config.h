/**********************************************************************************

 Infomap software package for multi-level network clustering

 Copyright (c) 2013 Daniel Edler, Martin Rosvall
 
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
#include <ostream>
#include <string>

struct Config
{
	Config()
	:	networkFile(""),
	 	inputFormat(""),
	 	parseWithoutIOStreams(false),
		zeroBasedNodeNumbers(false),
		includeSelfLinks(false),
		ignoreEdgeWeights(false),
		nodeLimit(0),
	 	clusterDataFile(""),
	 	noInfomap(false),
	 	twoLevel(false),
		directed(false),
		undirdir(false),
		outdirdir(false),
		rawdir(false),
		recordedTeleportation(false),
		teleportToNodes(false),
		teleportationProbability(0.15),
		selfTeleportationProbability(-1),
		seedToRandomNumberGenerator(123),
		numTrials(1),
		minimumCodelengthImprovement(1.0e-10),
		randomizeCoreLoopLimit(false),
		coreLoopLimit(0),
		levelAggregationLimit(0),
		tuneIterationLimit(0),
		minimumRelativeTuneIterationImprovement(1.0e-5),
		fastCoarseTunePartition(false),
		alternateCoarseTuneLevel(false),
		coarseTuneLevel(1),
		fastHierarchicalSolution(0),
		outDirectory(""),
		printTree(false),
		printMap(false),
		printClu(false),
		printNodeRanks(false),
		printFlowNetwork(false),
		printPajekNetwork(false),
		printBinaryTree(false),
		printBinaryFlowTree(false),
		noFileOutput(false),
		verbosity(0),
		verboseNumberPrecision(6),
		benchmark(false)
	{
		setOptimizationLevel(1);
		// Default output to .tree
//		printTree = true;
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

	bool isUndirected() const { return !directed && !undirdir && !outdirdir && !rawdir; }

	bool haveModularResultOutput() const
	{
		return printTree ||
				printMap ||
				printClu ||
				printBinaryTree ||
				printBinaryFlowTree;
	}


	// Input
	std::string networkFile;
	std::string inputFormat;
	bool parseWithoutIOStreams;
	bool zeroBasedNodeNumbers;
	bool includeSelfLinks;
	bool ignoreEdgeWeights;
	unsigned int nodeLimit;
	std::string clusterDataFile;
	bool noInfomap;

	// Core algorithm
	bool twoLevel;
	bool directed;
	bool undirdir;
	bool outdirdir;
	bool rawdir;
	bool recordedTeleportation;
	bool teleportToNodes;
	double teleportationProbability;
	double selfTeleportationProbability;
	unsigned long seedToRandomNumberGenerator;

	// Performance and accuracy
	unsigned int numTrials;
	double minimumCodelengthImprovement;
	bool randomizeCoreLoopLimit;
	unsigned int coreLoopLimit;
	unsigned int levelAggregationLimit;
	unsigned int tuneIterationLimit; // num iterations of fine-tune/coarse-tune in two-level partition)
	double minimumRelativeTuneIterationImprovement;
	bool fastCoarseTunePartition;
	bool alternateCoarseTuneLevel;
	unsigned int coarseTuneLevel;
	unsigned int fastHierarchicalSolution;

	// Output
	std::string outDirectory;
	bool printTree;
	bool printMap;
	bool printClu;
	bool printNodeRanks;
	bool printFlowNetwork;
	bool printPajekNetwork;
	bool printBinaryTree;
	bool printBinaryFlowTree; // tree including horizontal links (hierarchical network)
	bool noFileOutput;
	unsigned int verbosity;
	unsigned int verboseNumberPrecision;
	bool benchmark;
};

#endif /* CONFIG_H_ */
