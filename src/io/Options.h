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


#ifndef OPTIONS_H_
#define OPTIONS_H_
#include <string>

struct Options
{
	Options()
	{
		optimizationLevel(1);
	}

	// Set all optimization options at once with different accuracy to performance trade-off
	void optimizationLevel(unsigned int level)
	{
		switch (level)
		{
		case 0: // full coarse-tune
			randomizeMoveNodesLoopLimit = false;
			coreLoopLimit = 0;
			levelAggregationLimit = 0;
			tuneIterationLimit = 0;
			fastCoarseTunePartition = false;
			alternateCoarseTuneLevel = true;
			coarseTuneLevel = 3;
			break;
		case 1: // fast coarse-tune
			randomizeMoveNodesLoopLimit = true;
			coreLoopLimit = 10;
			levelAggregationLimit = 0;
			tuneIterationLimit = 0;
			fastCoarseTunePartition = true;
			alternateCoarseTuneLevel = false;
			coarseTuneLevel = 1;
			break;
		case 2: // no tuning
			randomizeMoveNodesLoopLimit = true;
			coreLoopLimit = 10;
			levelAggregationLimit = 0;
			tuneIterationLimit = 1;
			fastCoarseTunePartition = true;
			alternateCoarseTuneLevel = false;
			coarseTuneLevel = 1;
			break;
		case 3: // no aggregation nor any tuning
			randomizeMoveNodesLoopLimit = true;
			coreLoopLimit = 10;
			levelAggregationLimit = 1;
			tuneIterationLimit = 1;
			fastCoarseTunePartition = true;
			alternateCoarseTuneLevel = false;
			coarseTuneLevel = 1;
			break;
		}
	}

	// Input
	std::string networkFile;
	bool zeroBasedNodeNumbers;
	bool includeSelfLinks;
	bool ignoreEdgeWeights;
	unsigned int nodeLimit;

	// Core algorithm
	bool directed;
	bool undirdir;
	bool unrecordedTeleportation;
	bool teleportToLinks;
	double teleportationProbability;
	double seedToRandomNumberGenerator;

	// Performance and accuracy
	unsigned int numTrials;
	double minimumCodelengthImprovement;
	bool randomizeMoveNodesLoopLimit;
	unsigned int coreLoopLimit;
	unsigned int levelAggregationLimit;
	unsigned int tuneIterationLimit; // num iterations of fine-tune/coarse-tune in two-level partition)
	bool fastCoarseTunePartition;
	bool alternateCoarseTuneLevel;
	unsigned int coarseTuneLevel;

	// Output
	std::string outDirectory;
	bool printNodeRanks;
	unsigned int verbosity;
};

#endif /* OPTIONS_H_ */
