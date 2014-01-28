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


#include "InfomapBase.h"
#include "../utils/Logger.h"
#include <algorithm>
#include "../utils/FileURI.h"
#include "../io/SafeFile.h"
#include "../io/TreeDataWriter.h"
#include <cmath>
#include "../io/ClusterReader.h"
#include <limits>
#include <sstream>
#include <iomanip>
#include "../utils/infomath.h"
#include "../io/convert.h"
#include "../utils/Stopwatch.h"
#ifdef _OPENMP
#include <omp.h>
#include <stdio.h>
#endif
#include <map>
#include "../utils/Date.h"
#include "Network.h"
#include "FlowNetwork.h"
#include "../io/version.h"

void InfomapBase::run()
{
	DEBUG_OUT("InfomapBase::run()..." << std::endl);
	ASSERT(m_subLevel == 0);

#ifdef _OPENMP
	RELEASE_OUT("(OpenMP " << _OPENMP << " detected, trying to parallelize the recursive part on " <<
			omp_get_num_procs() << " processors...)\n" << std::flush);
#endif


	if (!initNetwork())
		return;


	indexCodelength = root()->codelength = calcCodelengthFromFlowWithinOrExit(*root());
	RELEASE_OUT("One-level codelength: " << indexCodelength << std::endl);
	oneLevelCodelength = indexCodelength;

	if (m_config.benchmark)
		Logger::benchmark("calcFlow", root()->codelength, 1, 1, 1);


	std::vector<double> codelengths(m_config.numTrials);
	std::ostringstream bestSolutionStatistics;

	for (unsigned int iTrial = 0; iTrial < m_config.numTrials; ++iTrial)
	{
		RELEASE_OUT(std::endl << "Attempt " << (iTrial+1) << "/" << m_config.numTrials <<
				" at " << Date() << std::endl);
		m_iterationCount = 0;

		// First clear existing modular structure
		while ((*m_treeData.begin_leaf())->parent != root())
		{
			root()->replaceChildrenWithGrandChildren();
		}

		if (m_config.clusterDataFile != "")
			consolidateExternalClusterData();

		if (!m_config.noInfomap)
			runPartition();

//		hierarchicalPartition();

		codelengths[iTrial] = hierarchicalCodelength;

		if (hierarchicalCodelength < bestHierarchicalCodelength)
		{
			bestHierarchicalCodelength = hierarchicalCodelength;
			bestSolutionStatistics.str("");
			printNetworkData();
			printPerLevelCodelength(bestSolutionStatistics);
		}
	}

	std::cout << "\n\n";
	if (m_config.numTrials > 1)
	{
		double averageCodelength = 0.0;
		double minCodelength = codelengths[0];
		double maxCodelength = 0.0;
		std::cout << std::fixed << std::setprecision(9);
		std::cout << "Codelengths for " << m_config.numTrials << " trials: [";
		for (std::vector<double>::const_iterator it(codelengths.begin()); it != codelengths.end(); ++it)
		{
			double mdl = *it;
			std::cout << mdl << ", ";
			averageCodelength += mdl;
			minCodelength = std::min(minCodelength, mdl);
			maxCodelength = std::max(maxCodelength, mdl);
		}
		averageCodelength /= m_config.numTrials;
		std::cout << "\b\b]\n";
		std::cout << "[min, average, max] codelength: [" <<
				minCodelength << ", " << averageCodelength << ", " << maxCodelength << "]\n\n";
		std::cout << std::resetiosflags(std::ios::floatfield) << std::setprecision(6);
	}

	if (bestIntermediateStatistics.str() != "")
	{
		std::cout << "Best intermediate solution:" << std::endl;
		std::cout << bestIntermediateStatistics.str() << std::endl << std::endl;
	}

	std::cout << "Best end solution:" << std::endl;
	std::cout << bestSolutionStatistics.str() << std::endl;

//	printNetworkDebug("debug", true, false);


	//TODO: Test recursive search only one step further each time to be able to show progress
	//	if (m_subLevel == 0)
	//	{
	//		RELEASE_OUT("\nTest:" << std::endl);
	////		for (NodeBase::sibling_iterator moduleIt(root()->begin_child()), endIt(root()->end_child());
	////				moduleIt != endIt; ++moduleIt)
	////		{
	////			moduleIt->getSubStructure().subInfomap.reset(0);
	////		}
	////		subCodelength = generateSubInfomapInstancesToLevel(3, true);
	//
	//		double testSubCodelength = findHierarchicalSubstructures(7, true);
	//		double testHierCodelength = indexCodelength + testSubCodelength;
	//
	//		RELEASE_OUT("done! Codelength: " << indexCodelength << " + " << testSubCodelength << " = " <<
	//				testHierCodelength << std::endl);
	//
	//		printPerLevelCodelength();
	//	}

}

void InfomapBase::runPartition()
{
	hierarchicalCodelength = oneLevelCodelength;
	indexCodelength = oneLevelCodelength;
	moduleCodelength = 0.0;

	if (m_config.twoLevel)
	{
		partition();
		hierarchicalCodelength = codelength;
		for (NodeBase::sibling_iterator moduleIt(root()->begin_child()), endIt(root()->end_child());
				moduleIt != endIt; ++moduleIt)
		{
			moduleIt->codelength = calcCodelengthFromFlowWithinOrExit(*moduleIt);
		}
		return;
	}

	PartitionQueue partitionQueue;

	if (m_config.fastHierarchicalSolution != 0)
	{
		unsigned int numLevelsCreated = findSuperModulesIterativelyFast(partitionQueue);

		// Print current hierarchical solution
		if (m_config.fastHierarchicalSolution < 3 && hierarchicalCodelength < bestIntermediateCodelength)
		{
			bestIntermediateCodelength = hierarchicalCodelength;
			bestIntermediateStatistics.clear();
			bestIntermediateStatistics.str("");
			printPerLevelCodelength(bestIntermediateStatistics);
			printNetworkData(io::Str() << FileURI(m_config.networkFile).getName() << "_fast", false);

		}
		if (m_config.fastHierarchicalSolution == 1)
		{
			deleteSubLevels();
			queueTopModules(partitionQueue);
		}
		else
		{
			resetModuleFlowFromLeafNodes();
			partitionQueue.level = numLevelsCreated;
		}
	}
	else
	{
		partitionAndQueueNextLevel(partitionQueue);
	}

	if (m_config.fastHierarchicalSolution > 2 || partitionQueue.size() == 0)
		return;


	// TODO: Write out initial codelength (two-level/hierarchical) on which the compression rate depends
	if (m_config.verbosity == 0)
	{
		RELEASE_OUT("\nRecursive sub-structure compression: " << std::flush);

	}
	else
	{
		RELEASE_OUT("Current codelength: " << indexCodelength << " + " <<
					(hierarchicalCodelength - indexCodelength) << " = " << hierarchicalCodelength<< "\n");
		RELEASE_OUT("\nTrying to find deeper structure under current modules recursively... \n");
	}

	double sumConsolidatedCodelength = hierarchicalCodelength - partitionQueue.moduleCodelength;

//	RELEASE_OUT("(ic: " << indexCodelength << ", mc: " << moduleCodelength << ", c: " <<
//					codelength << ", hc: " << hierarchicalCodelength <<
//					", hc-queue.mc: " << sumConsolidatedCodelength << ") ");

	while (partitionQueue.size() > 0)
	{
		if (m_config.verbosity > 0)
		{
			RELEASE_OUT("Level " << partitionQueue.level << ": " << (partitionQueue.flow*100) <<
					"% of the flow in " << partitionQueue.size() << " modules. Partitioning... " <<
					std::setprecision(6) << std::flush);
		}

//		RELEASE_OUT("(ic: " << indexCodelength << ", mc: " << moduleCodelength << ", c: " <<
//				codelength << ", hc: " << hierarchicalCodelength << ") ");

		PartitionQueue nextLevelQueue;
		// Partition all modules in the queue and fill up the next level queue
		processPartitionQueue(partitionQueue, nextLevelQueue);

		double leftToImprove = partitionQueue.moduleCodelength;
		sumConsolidatedCodelength += partitionQueue.indexCodelength + partitionQueue.leafCodelength;
		double limitCodelength = sumConsolidatedCodelength + leftToImprove;

//		RELEASE_OUT("(ic: " << indexCodelength << ", mc: " << moduleCodelength << ", c: " <<
//						codelength << ", hc: " << hierarchicalCodelength << ") ");

		if (m_config.verbosity == 0)
		{
			RELEASE_OUT( ((hierarchicalCodelength - limitCodelength) / hierarchicalCodelength) * 100 <<
					"% " << std::flush);
		}
		else
		{
//			RELEASE_OUT("done! Codelength: " << partitionQueue.indexCodelength << " + " <<
//					partitionQueue.leafCodelength << " (+ " << leftToImprove << " left to improve)" <<
//					" -> limit: " << limitCodelength << " bits.\n");
			RELEASE_OUT("done! Codelength: " << partitionQueue.indexCodelength << " + " <<
					partitionQueue.leafCodelength << " (+ " << leftToImprove << " left to improve)" <<
					" -> limit: " << std::setprecision(10) << limitCodelength <<
					std::setprecision(6) << " bits.\n");
		}

		hierarchicalCodelength = limitCodelength;

		partitionQueue.swap(nextLevelQueue);
	}
	if (m_config.verbosity == 0)
	{
		RELEASE_OUT("to codelength " << hierarchicalCodelength << "\n");
	}
	else
	{
		RELEASE_OUT("\n");
	}
}


double InfomapBase::partitionAndQueueNextLevel(PartitionQueue& partitionQueue, bool tryIndexing)
{
	DEBUG_OUT("InfomapBase::hierarchicalPartition()..." << std::endl);

	hierarchicalCodelength = codelength = root()->codelength; // only != 0 on top root

	if (numLeafNodes() == 1)
	{
		return hierarchicalCodelength;
	}

	// Two-level partition --> index codebook + module codebook

	partition();

	// Instead of a flat codelength, use the two-level structure found.
	hierarchicalCodelength = codelength;

	// Return if trivial result from partitioning
	//	if (numTopModules() <= 2 || numTopModules() == numLeafNodes())
//	if (numTopModules() == 2)
//	{
//		//		return hierarchicalCodelength; // 0.0 but doesn't seem to be used.. check unnecessary checks..
//		root()->codelength = calcModuleCodelength(*root());
//		if (std::abs(root()->codelength - indexCodelength) > 1e-10)
//			RELEASE_OUT("*");
//		indexCodelength = root()->codelength;
////		root()->codelength = indexCodelength;
//		return codelength;
//	}
	if (numTopModules() == 1)
	{
		root()->firstChild->codelength = codelength;
		return hierarchicalCodelength;
	}
	else if (tryIndexing)
	{
		tryIndexingIteratively();
	}

	queueTopModules(partitionQueue);

	return hierarchicalCodelength;
}

void InfomapBase::queueTopModules(PartitionQueue& partitionQueue)
{
	// Add modules to partition queue
	partitionQueue.numNonTrivialModules = numNonTrivialTopModules();
	partitionQueue.flow = getNodeData(*root()).flow;
	partitionQueue.resize(root()->childDegree());
	double nonTrivialFlow = 0.0;
	unsigned int moduleIndex = 0;
	for (NodeBase::sibling_iterator moduleIt(root()->begin_child()), endIt(root()->end_child());
			moduleIt != endIt; ++moduleIt, ++moduleIndex)
	{
		partitionQueue[moduleIndex] = moduleIt.base();
		if (moduleIt->childDegree() > 1)
		{
			nonTrivialFlow += getNodeData(*moduleIt).flow;
		}
	}
	partitionQueue.nonTrivialFlow = nonTrivialFlow;
	partitionQueue.indexCodelength = indexCodelength;
	partitionQueue.moduleCodelength = moduleCodelength;
}

void InfomapBase::tryIndexingIteratively()
{
//	return indexCodelength;//TODO: DEBUG!!
	unsigned int numIndexingCompleted = 0;
	bool verbose = m_subLevel == 0;

	if (verbose)
	{
		if (m_config.verbosity == 0)
			RELEASE_OUT("Finding ");
		else
			RELEASE_OUT("\n");
	}

	double minHierarchicalCodelength = hierarchicalCodelength;
	// Add index codebooks as long as the code gets shorter (and collapse each iteration)
	bool tryIndexing = true;
	bool replaceExistingModules = m_config.fastHierarchicalSolution == 0;
	while(tryIndexing)
	{
		if (verbose)
		{
			if (m_config.verbosity > 0)
			{
				RELEASE_OUT("Trying to find super modules... ");
				if (m_config.verbosity >= 3)
					RELEASE_OUT(std::endl);
			}
		}

		std::auto_ptr<InfomapBase> superInfomap(getNewInfomapInstance());
		superInfomap->reseed(getSeedFromCodelength(minHierarchicalCodelength));
		superInfomap->m_subLevel = m_subLevel + m_TOP_LEVEL_ADDITION;
		superInfomap->initSuperNetwork(*root());
		superInfomap->partition();


		// Break if trivial super structure
		//		if (superInfomap->numTopModules()  == 1 || superInfomap->numTopModules() == numTopModules())
		if (superInfomap->m_numNonTrivialTopModules  == 1 ||
				superInfomap->numTopModules() == numTopModules())
		{
			if (verbose && m_config.verbosity > 0)
			{
				RELEASE_OUT("failed to find non-trivial super modules." << std::endl);
			}
			break;
		}
		else if (superInfomap->codelength > indexCodelength - m_config.minimumCodelengthImprovement)
		{
			if (verbose && m_config.verbosity > 0)
			{
				RELEASE_OUT("two-level index codebook not improved over one-level." << std::endl);
			}
			break;
		}

		minHierarchicalCodelength += superInfomap->codelength - indexCodelength;

		if (verbose)
		{
			if (m_config.verbosity == 0)
			{
				RELEASE_OUT(superInfomap->numTopModules() << " ");
			}
			else
			{
				RELEASE_OUT("succeeded. Found " << superInfomap->numTopModules() << " "
						"super modules with estimated hierarchical codelength " <<
						minHierarchicalCodelength << ".\n");
			}
		}

		// Replace current module structure with the super structure
		setActiveNetworkFromLeafs();
		initModuleOptimization();

		unsigned int i = 0;
		for (TreeData::leafIterator leafIt(m_treeData.begin_leaf()), leafEnd(m_treeData.end_leaf());
				leafIt != leafEnd; ++leafIt, ++i)
		{
			(**leafIt).index = i;
		}


		// Collect the super module indices on the leaf nodes
		TreeData& superTree = superInfomap->m_treeData;
		TreeData::leafIterator superLeafIt(superTree.begin_leaf());
		unsigned int leafIndex = 0;
		for (NodeBase::sibling_iterator moduleIt(root()->begin_child()), endIt(root()->end_child());
				moduleIt != endIt; ++moduleIt, ++superLeafIt)
		{
			unsigned int superModuleIndex = (*superLeafIt)->parent->index;
			for (NodeBase::sibling_iterator nodeIt(moduleIt->begin_child()), nodeEndIt(moduleIt->end_child());
					nodeIt != nodeEndIt; ++nodeIt, ++leafIndex)
			{
				m_moveTo[nodeIt->index] = superModuleIndex;
			}
		}

		// Move the leaf nodes to the modules collected above
		moveNodesToPredefinedModules();
		// Replace the old modular structure with the super structure generated above
		consolidateModules(replaceExistingModules);

		++numIndexingCompleted;
		tryIndexing = m_numNonTrivialTopModules > 1 && numTopModules() != numLeafNodes();
//		RELEASE_OUT("!!! DEBUG, early exit !!!\n");
//		break;
	}

	if (verbose && m_config.verbosity == 0)
		RELEASE_OUT("super modules with estimated codelength " << minHierarchicalCodelength << ". ");

	hierarchicalCodelength = replaceExistingModules ? codelength : minHierarchicalCodelength;
}

/**
 * Like mergeAndConsolidateRepeatedly but let it build up the tree for each
 * new level of aggregation. It doesn't create new Infomap instances.
 */
unsigned int InfomapBase::findSuperModulesIterativelyFast(PartitionQueue& partitionQueue)
{
	bool verbose = m_subLevel == 0;

	if (verbose)
	{
		if (m_config.verbosity < 2)
			RELEASE_OUT("Index module compression: " << std::setprecision(2) << std::flush);
		else
		{
			RELEASE_OUT("Trying to find fast hierarchy... " << std::flush);
			if (m_config.verbosity > 1)
				RELEASE_OUT("\n");
		}
	}


	unsigned int networkLevel = 0;
	unsigned int numLevelsCreated = 0;

	bool isLeafLevel = (*m_treeData.begin_leaf())->parent == root();
	std::string nodesLabel = isLeafLevel ? "nodes" : "modules";

	// Add index codebooks as long as the code gets shorter
	do
	{
		double oldIndexLength = indexCodelength;
		double workingHierarchicalCodelength = hierarchicalCodelength;

		if (isLeafLevel)
		{
			setActiveNetworkFromLeafs();
		}
		else
		{
			setActiveNetworkFromChildrenOfRoot();
			transformNodeFlowToEnterFlow(root());
		}

		initConstantInfomapTerms();
		initModuleOptimization();

		if (verbose && m_config.verbosity > 1)
		{
			RELEASE_OUT("Level " << ++networkLevel << ", moving " << m_activeNetwork.size() <<
								" " << nodesLabel << "... " << std::flush);
		}

		unsigned int numOptimizationLoops = optimizeModules();

		bool acceptSolution = codelength < oldIndexLength - m_config.minimumCodelengthImprovement;
		// Force at least one modular level!
		bool acceptByForce = !acceptSolution && numLevelsCreated == 0;
		if (acceptByForce)
			acceptSolution = true;

		workingHierarchicalCodelength += codelength - oldIndexLength;

		if (verbose)
		{
			if (m_config.verbosity < 2)
			{
				if (acceptSolution)
					RELEASE_OUT( ((hierarchicalCodelength - workingHierarchicalCodelength) \
							/ hierarchicalCodelength * 100) << "% " << std::flush );
			}
			else
			{
				RELEASE_OUT("found " << numDynamicModules() << " modules in " << numOptimizationLoops <<
						" loops with hierarchical codelength " << indexCodelength << " + " <<
						(workingHierarchicalCodelength - indexCodelength) << " = " <<
						workingHierarchicalCodelength << (acceptSolution ? "\n" :
								", discarding the solution.\n") << std::flush);
			}
		}

		if (!acceptSolution)
		{
			indexCodelength = oldIndexLength;
			break;
		}

		// Consolidate the dynamic modules without replacing any existing ones.
		consolidateModules(false);

		hierarchicalCodelength = workingHierarchicalCodelength;
		oldIndexLength = indexCodelength;

		// Store the individual codelengths on each module
		for (NodeBase::sibling_iterator moduleIt(root()->begin_child()), endIt(root()->end_child());
				moduleIt != endIt; ++moduleIt)
		{
			moduleIt->codelength = calcCodelengthFromFlowWithinOrExit(*moduleIt);
		}

		if (isLeafLevel && m_config.fastHierarchicalSolution > 1)
		{
			queueTopModules(partitionQueue);
		}

		nodesLabel = "modules";
		isLeafLevel = false;
		++numLevelsCreated;

	} while (m_numNonTrivialTopModules != 1);

	if (verbose)
	{
		RELEASE_OUT(std::setprecision(m_config.verboseNumberPrecision));
		if (m_config.verbosity == 0)
			RELEASE_OUT("to codelength " << hierarchicalCodelength << " in " <<
					numTopModules() << " top modules. ");
		else
			RELEASE_OUT("done! Added " << numLevelsCreated << " levels with " <<
					numTopModules() << " top modules to codelength: " <<
					hierarchicalCodelength << " ");
	}

	return numLevelsCreated;
}

unsigned int InfomapBase::deleteSubLevels()
{
	NodeBase* node = *m_treeData.begin_leaf();
	unsigned int numLevels = 0;
	while (node = node->parent, node != 0)
	{
		++numLevels;
	}

	if (numLevels <= 1)
		return 0;

	if (m_subLevel == 0 && m_config.verbosity > 0)
	{
		RELEASE_OUT("Clearing " << (numLevels-1) << " levels of sub-modules");
	}

	// Clear all sub-modules
	for (NodeBase::sibling_iterator moduleIt(root()->begin_child()), endIt(root()->end_child());
			moduleIt != endIt; ++moduleIt)
	{
		for (unsigned int i = numLevels - 1; i != 0; --i)
		{
			moduleIt->replaceChildrenWithGrandChildren();
		}
	}

	// Reset to leaf-level codelength terms
	setActiveNetworkFromLeafs();
	initConstantInfomapTerms();

//	recalculateCodelengthFromConsolidatedNetwork();
	resetModuleFlowFromLeafNodes();
	double sumModuleLength = 0.0;
	for (NodeBase::sibling_iterator moduleIt(root()->begin_child()), endIt(root()->end_child());
			moduleIt != endIt; ++moduleIt)
	{
		sumModuleLength += moduleIt->codelength = calcCodelengthFromFlowWithinOrExit(*moduleIt);
	}
	moduleCodelength = sumModuleLength;
	hierarchicalCodelength = codelength = indexCodelength + moduleCodelength;

	if (m_subLevel == 0)
	{
		if (m_config.verbosity == 0)
			RELEASE_OUT("Clearing sub-modules to codelength " << codelength << "\n");
		else
			RELEASE_OUT("done! Two-level codelength " << indexCodelength << " + " << moduleCodelength <<
					" = " << codelength << " in " << numTopModules() << " modules." << std::endl);
	}

	return numLevels-1;
}

bool InfomapBase::processPartitionQueue(PartitionQueue& queue, PartitionQueue& nextLevelQueue, bool tryIndexing)
{
	PartitionQueue::size_t numModules = queue.size();
	std::vector<double> indexCodelengths(numModules, 0.0);
	std::vector<double> moduleCodelengths(numModules, 0.0);
	std::vector<double> leafCodelengths(numModules, 0.0);
	std::vector<PartitionQueue> subQueues(numModules);

//#define _OPENMP
#ifdef _OPENMP
//	sortPartitionQueue(queue);
	int numModulesInt = static_cast<int>(numModules);
//	int numProcs = std::min(numModulesInt, omp_get_num_procs());
	int iModule;

//#pragma omp parallel
//	{
//#pragma omp for nowait
//		for (iProc = 0; iProc < numProcs; ++iProc)
//		{
//			for (int moduleIndex = iProc; moduleIndex < numModulesInt; moduleIndex += numProcs)
//			{
//#pragma omp parallel for schedule(dynamic, 1)
#pragma omp for
	for(iModule = 0; iModule < numModulesInt; ++iModule)
	{
		unsigned int moduleIndex = static_cast<unsigned int>(iModule);
		NodeBase& module = *queue[moduleIndex];
#else
	for (PartitionQueue::size_t moduleIndex = 0; moduleIndex < numModules; ++moduleIndex)
	{
		NodeBase& module = *queue[moduleIndex];
#endif

		// Delete former sub-structure if exists
		module.getSubStructure().subInfomap.reset(0);
		module.codelength = calcCodelengthFromFlowWithinOrExit(module);

		// If only trivial substructure is to be found, no need to create infomap instance to find sub-module structures.
		if (module.childDegree() <= 2)
		{
			leafCodelengths[moduleIndex] = module.codelength;
			continue;
		}

		PartitionQueue& subQueue = subQueues[moduleIndex];
		subQueue.level = queue.level + 1;

		std::auto_ptr<InfomapBase> subInfomap(getNewInfomapInstance());
		subInfomap->m_subLevel = m_subLevel + 1;

		subInfomap->initSubNetwork(module, false);

		subInfomap->partitionAndQueueNextLevel(subQueue, tryIndexing);

		// If non-trivial substructure is found which improves the codelength, store it on the module
		bool nonTrivialSubstructure = subInfomap->numTopModules() > 1 &&
				subInfomap->numTopModules() < subInfomap->numLeafNodes();
		bool improvement = nonTrivialSubstructure &&
				(subInfomap->hierarchicalCodelength < module.codelength - m_config.minimumCodelengthImprovement);

		if (improvement)
		{
//			moduleCodelength -= module.codelength;
			indexCodelengths[moduleIndex] = subInfomap->indexCodelength;
			moduleCodelengths[moduleIndex] = subInfomap->moduleCodelength;
//						improvements[moduleIndex] = module.codelength - subInfomap->hierarchicalCodelength;
			module.getSubStructure().subInfomap = subInfomap;
			//				nextLevelSize += subQueue.size();
		}
		else
		{
			leafCodelengths[moduleIndex] = module.codelength;
			module.getSubStructure().exploredWithoutImprovement = true;
			subQueue.skip = true;
			// Else use the codelength from the flat substructure
		}

	}


	double sumLeafCodelength = 0.0;
	double sumIndexCodelength = 0.0;
	double sumModuleCodelengths = 0.0;
	PartitionQueue::size_t nextLevelSize = 0;
	for (PartitionQueue::size_t moduleIndex = 0; moduleIndex < numModules; ++moduleIndex)
	{
		nextLevelSize += subQueues[moduleIndex].skip ? 0 : subQueues[moduleIndex].size();
		sumLeafCodelength += leafCodelengths[moduleIndex];
		sumIndexCodelength += indexCodelengths[moduleIndex];
		sumModuleCodelengths += moduleCodelengths[moduleIndex];
	}

	queue.indexCodelength = sumIndexCodelength;
	queue.leafCodelength = sumLeafCodelength;
	queue.moduleCodelength = sumModuleCodelengths;

	// Collect the sub-queues and build the next-level queue
	nextLevelQueue.level = queue.level + 1;
	nextLevelQueue.resize(nextLevelSize);
	PartitionQueue::size_t nextLevelIndex = 0;
	for (PartitionQueue::size_t moduleIndex = 0; moduleIndex < numModules; ++moduleIndex)
	{
		PartitionQueue& subQueue = subQueues[moduleIndex];
		if (!subQueue.skip)
		{
			for (PartitionQueue::size_t subIndex = 0; subIndex < subQueue.size(); ++subIndex)
			{
				nextLevelQueue[nextLevelIndex++] = subQueue[subIndex];
			}
			nextLevelQueue.flow += subQueue.flow;
			nextLevelQueue.nonTrivialFlow += subQueue.nonTrivialFlow;
			nextLevelQueue.numNonTrivialModules += subQueue.numNonTrivialModules;
		}
	}

	return nextLevelSize > 0;
}

void InfomapBase::sortPartitionQueue(PartitionQueue& queue)
{
	std::multimap<double, PendingModule, std::greater<double> > sortedModules;
	for (PartitionQueue::size_t i = 0; i < queue.size(); ++i)
	{
		sortedModules.insert(std::pair<double, PendingModule>(getNodeData(*queue[i]).flow, queue[i]));
	}
	std::multimap<double, PendingModule, std::greater<double> >::iterator mapIt = sortedModules.begin();
	for (PartitionQueue::size_t i = 0; i < queue.size(); ++i, ++mapIt)
	{
		queue[i] = mapIt->second;
	}


}

void InfomapBase::partition(unsigned int recursiveCount, bool fast, bool forceConsolidation)
{
	bool verbose = (m_subLevel == 0 && m_config.verbosity != 0) ||
			(isSuperLevelOnTopLevel() && m_config.verbosity == 2);
	verbose = m_subLevel == 0;

	if ((*m_treeData.begin_leaf())->parent != root())
	{
		RELEASE_OUT("Already partitioned with codelength " << codelength <<
				" in " << numTopModules() << " modules." << std::endl);
		return;
	}

	setActiveNetworkFromChildrenOfRoot();

	initConstantInfomapTerms();
	initModuleOptimization();


	if (verbose)
	{
		if (m_config.verbosity == 0)
		{
			RELEASE_OUT("Two-level compression: " << std::setprecision(2) << std::flush);
		}
		else
		{
			RELEASE_OUT("\nTrying to find modular structure... \n");
			RELEASE_OUT("Initiated to codelength " << indexCodelength << " + " << moduleCodelength << " = " <<
					codelength << " in " << numTopModules() << " modules." << std::endl);
		}
		if (m_config.benchmark)
			Logger::benchmark("init", codelength, numTopModules(), numNonTrivialTopModules(), 2);
	}

	double initialCodelength = codelength;

	mergeAndConsolidateRepeatedly(forceConsolidation, fast);

	if (codelength > initialCodelength)
		RELEASE_OUT("*");

	double oldCodelength = oneLevelCodelength;
	double compression = (oldCodelength - codelength)/oldCodelength;
	if (verbose && m_config.verbosity == 0)
		RELEASE_OUT((compression * 100) << "% " << std::flush);

	if (!fast && m_config.tuneIterationLimit != 1 && numTopModules() != numLeafNodes())
	{
		unsigned int tuneIterationCount = 1;
		unsigned int coarseTuneLevel = m_config.coarseTuneLevel - 1;
		bool doFineTune = true;
//		bool coarseTuned = false;
		oldCodelength = codelength;
		while (numTopModules() > 1)
		{
			if (doFineTune)
			{
				fineTune();
				if (//coarseTuned &&
						(codelength > oldCodelength - initialCodelength*m_config.minimumRelativeTuneIterationImprovement ||
								codelength > oldCodelength - m_config.minimumCodelengthImprovement))
					break;
				compression = (oldCodelength - codelength)/oldCodelength;
				if (verbose && m_config.verbosity == 0)
					RELEASE_OUT((compression * 100) << "% " << std::flush);
				oldCodelength = codelength;
			}
			else
			{
				coarseTune(m_config.alternateCoarseTuneLevel ? (++coarseTuneLevel % m_config.coarseTuneLevel) :
						m_config.coarseTuneLevel - 1);
				if (codelength > oldCodelength - initialCodelength*m_config.minimumRelativeTuneIterationImprovement ||
						codelength > oldCodelength - m_config.minimumCodelengthImprovement)
					break;
//				coarseTuned = true;
				compression = (oldCodelength - codelength)/oldCodelength;
				if (verbose && m_config.verbosity == 0)
					RELEASE_OUT((compression * 100) << "% " << std::flush);
				oldCodelength = codelength;
			}
			++tuneIterationCount;
			if (m_config.tuneIterationLimit == tuneIterationCount)
				break;
			doFineTune = !doFineTune;
		}
	}

	if (verbose)
	{
		if (m_config.verbosity == 0)
			RELEASE_OUT("to " << numTopModules() << " modules with codelength " <<
					std::setprecision(m_config.verboseNumberPrecision) << codelength << std::endl);
		else
		{
			RELEASE_OUT("Two-level codelength: " << indexCodelength << " + " << moduleCodelength << " = " <<
					codelength << std::endl);
		}
	}


	if (!fast && recursiveCount > 0 && numTopModules() != 1 && numTopModules() != numLeafNodes())
	{
		partitionEachModule(recursiveCount - 1);
		// Prepare leaf network to move into the sub-module structure given from partitioning each module
		setActiveNetworkFromLeafs();
		unsigned int i = 0;
		for (TreeData::leafIterator leafIt(m_treeData.begin_leaf()), endIt(m_treeData.end_leaf());
				leafIt != endIt; ++leafIt, ++i)
		{
			m_moveTo[i] = (*leafIt)->index;
			ASSERT(m_moveTo[i] < m_activeNetwork.size());
		}
		initModuleOptimization();
		moveNodesToPredefinedModules();
		// Consolidate the sub-modules and store the current module structure in the sub-modules before replacing it
		consolidateModules(true, true);

		// Set module indices from a zero-based contiguous set
		unsigned int packedModuleIndex = 0;
		for (NodeBase::sibling_iterator moduleIt(root()->begin_child()), endIt(root()->end_child());
				moduleIt != endIt; ++moduleIt)
		{
			moduleIt->index = moduleIt->originalIndex = packedModuleIndex++;
		}
	}
}

void InfomapBase::mergeAndConsolidateRepeatedly(bool forceConsolidation, bool fast)
{
	++m_iterationCount;
	bool verbose = (m_subLevel == 0 && m_config.verbosity != 0) ||
			(isSuperLevelOnTopLevel() && m_config.verbosity >= 3);
	// Merge and collapse repeatedly until no code improvement or only one big cluster left
	if (verbose) {
		RELEASE_OUT("Iteration " << m_iterationCount << ", moving " << m_activeNetwork.size() << "*" << std::flush);
	}

	// Core loop, merging modules
	unsigned int numOptimizationLoops = optimizeModules();

	if (verbose)
		RELEASE_OUT(numOptimizationLoops << ", " << std::flush);

	// Force create modules even if worse (don't mix modules and leaf nodes under the same parent)
	consolidateModules();
	unsigned int numLevelsConsolidated = 1;

	// Reapply core algorithm on modular network, replacing modules with super modules
	while (numTopModules() > 1 && numLevelsConsolidated != m_config.levelAggregationLimit)
	{
		double consolidatedCodelength = codelength;
		double consolidatedIndexLength = indexCodelength;
		double consolidatedModuleLength = moduleCodelength;

		if (m_subLevel == 0 && m_config.benchmark)
			Logger::benchmark(io::Str() << "lvl" << numLevelsConsolidated, codelength, numTopModules(),
					numNonTrivialTopModules(), 2);

		if (verbose)
			RELEASE_OUT("" << numTopModules() << "*" << std::flush);

		setActiveNetworkFromChildrenOfRoot();
		initModuleOptimization();
		numOptimizationLoops = optimizeModules();

		if (verbose)
			RELEASE_OUT(numOptimizationLoops << ", " << std::flush);

		// If no improvement, revert codelength terms to the actual structure
		if (!(codelength < consolidatedCodelength - m_config.minimumCodelengthImprovement))
		{
			indexCodelength = consolidatedIndexLength;
			moduleCodelength = consolidatedModuleLength;
			codelength = consolidatedCodelength;
			break;
		}

		consolidateModules();
		++numLevelsConsolidated;
	}


//	if (m_subLevel < m_TOP_LEVEL_ADDITION)
//	{
//		//TODO: Why does the enter version give correct results here and not flow, it's leaf level?!
////		double debug = calcCodelengthFromFlowWithinOrExit(*root());
//		double debug = calcCodelengthFromEnterWithinOrExit(*root());
//		if (std::abs(debug - indexCodelength) > 1e-10)
//		{
//			RELEASE_OUT("+" << numLevelsConsolidated << ":" << numTopModules());
//			indexCodelength = debug;
//		}
//	}


	if (verbose) {
		RELEASE_OUT((m_isCoarseTune ? "modules" : "nodes") << "*loops to codelength " << codelength <<
				" in " << numTopModules() << " modules. (" << m_numNonTrivialTopModules <<
				" non-trivial modules)" << std::endl);
	}
	if (m_subLevel == 0 && m_config.benchmark)
	{
		Logger::benchmark(io::Str() << "iter" << m_iterationCount, codelength, numTopModules(),
				numNonTrivialTopModules(), 2);
	}

	// Set module indices from a zero-based contiguous set
	unsigned int packedModuleIndex = 0;
	for (NodeBase::sibling_iterator moduleIt(root()->begin_child()), endIt(root()->end_child());
			moduleIt != endIt; ++moduleIt)
	{
		moduleIt->index = moduleIt->originalIndex = packedModuleIndex++;
	}

}

void InfomapBase::generalTune(unsigned int level)
{

}

void InfomapBase::fineTune()
{
	DEBUG_OUT("InfomapBase::fineTune(" << *root() << ")..." << std::endl);
	m_isCoarseTune = false;
	setActiveNetworkFromLeafs();

	// Init dynamic modules from existing modular structure
	ASSERT(m_activeNetwork[0]->parent->parent == root());
	unsigned int i = 0;
	for (TreeData::leafIterator leafIt(m_treeData.begin_leaf()), endIt(m_treeData.end_leaf());
			leafIt != endIt; ++leafIt, ++i)
	{
		m_moveTo[i] = (*leafIt)->parent->index;
		ASSERT(m_moveTo[i] < m_activeNetwork.size());
	}

	initModuleOptimization();
	moveNodesToPredefinedModules();

	mergeAndConsolidateRepeatedly();

}

/**
 * Coarse-tune:
 * 1. Partition each cluster to find optimal modules in each module, i.e. sub modules.
 * 2. Move the leaf-nodes into the sub-module structure.
 * 3. Consolidate the sub-modules.
 * 3a.	Consolidate the sub-modules under their modules in the tree.
 * 3b.	Store their module index and delete the top module level.
 * 4. Move the sub-modules into the former module structure.
 * 5. Optimize by trying to move and merge sub-modules.
 * 6. Consolidate the result.
 */
void InfomapBase::coarseTune(unsigned int recursiveCount)
{
	DEBUG_OUT("InfomapBase::coarseTune(" << *root() << ")..." << std::endl);
	if (numTopModules() == 1)
		return;

	m_isCoarseTune = true;
	partitionEachModule(recursiveCount, m_config.fastCoarseTunePartition);

	// Prepare leaf network to move into the sub-module structure given from partitioning each module
	setActiveNetworkFromLeafs();
	unsigned int i = 0;
	for (TreeData::leafIterator leafIt(m_treeData.begin_leaf()), endIt(m_treeData.end_leaf());
			leafIt != endIt; ++leafIt, ++i)
	{
		m_moveTo[i] = (*leafIt)->index;
		ASSERT(m_moveTo[i] < m_activeNetwork.size());
	}

	initModuleOptimization();
	moveNodesToPredefinedModules();
	// Consolidate the sub-modules and store the current module structure in the sub-modules before replacing it
	consolidateModules(true, true);

	// Prepare the sub-modules to move into the former module structure and begin optimization from there
	setActiveNetworkFromChildrenOfRoot();
	m_moveTo.resize(m_activeNetwork.size());
	i = 0;
	for (NodeBase::sibling_iterator subModuleIt(root()->begin_child()), endIt(root()->end_child());
			subModuleIt != endIt; ++subModuleIt, ++i)
	{
		m_moveTo[i] = subModuleIt->index;
		ASSERT(m_moveTo[i] < m_activeNetwork.size());
	}
	initModuleOptimization();
	moveNodesToPredefinedModules();
	TO_NOTHING(old_codelength);
	ASSERT(std::abs(codelength - old_codelength) < 1.0e-4);

	mergeAndConsolidateRepeatedly(true);
}

void InfomapBase::partitionEachModule(unsigned int recursiveCount, bool fast)
{
	unsigned int moduleIndexOffset = 0;
	for (NodeBase::sibling_iterator moduleIt(root()->begin_child()), endIt(root()->end_child());
			moduleIt != endIt; ++moduleIt)
	{
		// If only one child in the module, no need to create infomap instance to find sub-module structures.
		if (moduleIt->childDegree() == 1)
		{
			for (NodeBase::sibling_iterator nodeIt(moduleIt->begin_child()), endIt(moduleIt->end_child());
					nodeIt != endIt; ++nodeIt)
			{
				nodeIt->index = moduleIndexOffset;
			}
			moduleIndexOffset += 1;
			continue;
		}
		DEBUG_OUT(">>>>>>>>>>>>>>>>>> RUN SUB_INFOMAP on node n" << moduleIt->id << " with childDegree: " << moduleIt->childDegree() << " >>>>>>>>>>>>" << std::endl);

		std::auto_ptr<InfomapBase> subInfomap(getNewInfomapInstance());
		// To not happen to get back the same network with the same seed
		subInfomap->reseed(getSeedFromCodelength(codelength));
		subInfomap->m_subLevel = m_subLevel + 1;
		subInfomap->initSubNetwork(*moduleIt, false);
		//		if (hierarchical)
		//			subInfomap->hierarchicalPartition();
		//		else
		subInfomap->partition(recursiveCount, fast);

		DEBUG_OUT("<<<<<<<<<<<<<<<<<<< BACK FROM SUB_INFOMAP!!!! <<<<<<<<<<<<<<<<<<<" << std::endl);
		DEBUG_OUT("Node n" << moduleIt->id << " with " << subInfomap->m_treeData.numLeafNodes() << " leaf-nodes gave " <<
				subInfomap->m_treeData.root()->childDegree() << " sub-clusters!" << std::endl);

		NodeBase::sibling_iterator originalLeafNodeIt(moduleIt->begin_child());
		for (TreeData::leafIterator leafIt(subInfomap->m_treeData.begin_leaf()), endIt(subInfomap->m_treeData.end_leaf());
				leafIt != endIt; ++leafIt, ++originalLeafNodeIt)
		{
			NodeBase& node = **leafIt;
			originalLeafNodeIt->index = node.parent->index + moduleIndexOffset;
		}
		moduleIndexOffset += subInfomap->m_treeData.root()->childDegree();
	}

	DEBUG_OUT("Partitioning each module gave " << moduleIndexOffset << " sub-modules from " << m_treeData.numLeafNodes() <<
			" nodes in " << root()->childDegree() << " modules." << std::endl);
}

void InfomapBase::initSubNetwork(NodeBase& parent, bool recalculateFlow)
{
	DEBUG_OUT("InfomapBase::initSubNetwork()..." << std::endl);
	cloneFlowData(parent, *root());
	generateNetworkFromChildren(parent); // Updates the exitNetworkFlow for the nodes
	root()->setChildDegree(numLeafNodes());
}

void InfomapBase::initSuperNetwork(NodeBase& parent)
{
	DEBUG_OUT("InfomapBase::initSuperNetwork()..." << std::endl);
	generateNetworkFromChildren(parent);
	root()->setChildDegree(numLeafNodes());

	transformNodeFlowToEnterFlow(root());
}

void InfomapBase::setActiveNetworkFromChildrenOfRoot()
{
	DEBUG_OUT("InfomapBase::setActiveNetworkFromChildrenOfRoot() with childDegree: " <<
			root()->childDegree() << "..." << std::endl);
	unsigned int numNodes = root()->childDegree();
	m_activeNetwork = m_nonLeafActiveNetwork;
	m_activeNetwork.resize(numNodes);
	unsigned int i = 0;
	for (NodeBase::sibling_iterator childIt(root()->begin_child()), endIt(root()->end_child());
			childIt != endIt; ++childIt, ++i)
	{
		m_activeNetwork[i] = childIt.base();
	}
}

void InfomapBase::setActiveNetworkFromLeafs()
{
	DEBUG_OUT("InfomapBase::setActiveNetworkFromLeafs(), numNodes: " << m_treeData.numLeafNodes() << std::endl);

	m_activeNetwork = m_treeData.m_leafNodes;
	m_moveTo.resize(m_activeNetwork.size());
}

void InfomapBase::consolidateExternalClusterData()
{
	ALL_OUT("Calculating codelength from external data... ");
	ClusterReader cluReader(numLeafNodes());
	try
	{
		cluReader.readData(m_config.clusterDataFile);
	}
	catch (const std::runtime_error& error)
	{
		std::cerr << "Error parsing input cluster data: " << error.what() << std::endl;
		return;
	}

	const std::vector<unsigned int>& clusterIndices = cluReader.getClusterData();

	m_activeNetwork = m_treeData.m_leafNodes;
	m_moveTo.resize(m_activeNetwork.size());

	initConstantInfomapTerms();

	unsigned int i = 0;
	for (TreeData::leafIterator leafIt(m_treeData.begin_leaf()), endIt(m_treeData.end_leaf());
			leafIt != endIt; ++leafIt, ++i)
	{
		m_moveTo[i] = clusterIndices[i];
		ASSERT(m_moveTo[i] < m_activeNetwork.size());
	}

	initModuleOptimization();
	moveNodesToPredefinedModules();
	consolidateModules();

	// Set module indices from a zero-based contiguous set
	unsigned int packedModuleIndex = 0;
	for (NodeBase::sibling_iterator moduleIt(root()->begin_child()), endIt(root()->end_child());
			moduleIt != endIt; ++moduleIt)
	{
		moduleIt->index = moduleIt->originalIndex = packedModuleIndex++;
		moduleIt->codelength = calcCodelengthFromFlowWithinOrExit(*moduleIt);
	}

	hierarchicalCodelength = codelength;

	ALL_OUT("done! Two-level codelength: " << indexCodelength << " + " << moduleCodelength << " = " <<
			codelength << " in " << numTopModules() << " modules." << std::endl);
}

bool InfomapBase::initNetwork()
{
	Network network(m_config);

	if (checkAndConvertBinaryTree())
		return false;

	try
	{
		network.readFromFile(m_config.networkFile);
	}
	catch (const std::runtime_error& error)
	{
		std::cerr << "Error parsing the input file: " << error.what() << std::endl;
		return false;
	}

	FlowNetwork flowNetwork;
	flowNetwork.calculateFlow(network, m_config);

	if (m_config.printPajekNetwork)
	{
		std::string outName = io::Str() <<
				m_config.outDirectory << FileURI(m_config.networkFile).getName() << ".net";
		network.printNetworkAsPajek(outName);
	}

	const std::vector<std::string>& nodeNames = network.nodeNames();
	const std::vector<double>& nodeFlow = flowNetwork.getNodeFlow();
	const std::vector<double>& nodeTeleportWeights = flowNetwork.getNodeTeleportRates();
	m_treeData.reserveNodeCount(network.numNodes());

	for (unsigned int i = 0; i < network.numNodes(); ++i)
		m_treeData.addNewNode(nodeNames[i], nodeFlow[i], nodeTeleportWeights[i]);
	const FlowNetwork::LinkVec& links = flowNetwork.getFlowLinks();
	for (unsigned int i = 0; i < links.size(); ++i)
		m_treeData.addEdge(links[i].source, links[i].target, links[i].weight, links[i].flow);

	initEnterExitFlow();

	return true;
//	printNetworkDebug("debug", false, true);
}

bool InfomapBase::checkAndConvertBinaryTree()
{
	if (FileURI(m_config.networkFile).getExtension() != "bftree" &&
			FileURI(m_config.networkFile).getExtension() != "btree")
		return false;

	HierarchicalNetwork network("", 0, false, 0.0, 0.0, "");

	try
	{
		network.readStreamableTree(m_config.networkFile);
	}
	catch (const std::runtime_error& error)
	{
		std::cerr << "Error parsing the input file: " << error.what() << std::endl;
		return true;
	}

	if (m_config.printMap)
	{
		std::string mapFilename = io::Str() <<
				m_config.outDirectory <<
				FileURI(m_config.networkFile).getName() << ".map";
		std::cout << "Write map to '" << mapFilename << "'... ";
		network.writeMap(mapFilename);
		std::cout << "done!\n";
	}


	return true;
}

void InfomapBase::printNetworkData(std::string filename, bool sort)
{
	if (m_config.noFileOutput)
		return;

	if (filename.length() == 0)
		filename = FileURI(m_config.networkFile).getName();

	std::string outName;

	// Sort tree on flow
	sortTree();

	// Print .tree
	if (m_config.printTree)
	{
		outName = io::Str() << m_config.outDirectory << filename << ".tree";
		if (m_config.verbosity == 0)
			RELEASE_OUT("(Writing .tree file.." << std::flush);
		else
			RELEASE_OUT("Print hierarchical cluster data to " << outName << "... " << std::flush);
		SafeOutFile treeOut(outName.c_str());
		treeOut << "# Codelength " << hierarchicalCodelength << " bits in " <<
				Stopwatch::getElapsedTimeSinceProgramStartInSec() << "s. Network size: "
				<< numLeafNodes() << " nodes and " << m_treeData.numLeafEdges() << " links." << std::endl;

		printSubInfomapTree(treeOut, m_treeData);
	//	printSubInfomapTreeDebug(treeOut, m_treeData);

		if (m_config.verbosity == 0)
			RELEASE_OUT(") ");
		else
			RELEASE_OUT("done!" << std::endl);
	}

//	// Print .debug.tree
//	outName = io::Str() << m_config.outDirectory << filename << ".debug.tree";
//	ALL_OUT("Print hierarchical debug cluster data to " << outName << "... ");
//	SafeOutFile debugTreeOut(outName.c_str());
//	debugTreeOut << "# Codelength " << hierarchicalCodelength << " bits in " <<
//			Stopwatch::getElapsedTimeSinceProgramStartInSec() << "s. Network size: "
//			<< numLeafNodes() << " nodes and " << m_treeData.numLeafEdges() << " links." << std::endl;
//	printSubInfomapTreeDebug(debugTreeOut, m_treeData);
//	ALL_OUT("done!" << std::endl);

	// Print .map
	if (m_config.printMap)
	{
		outName = io::Str() << m_config.outDirectory << filename << ".map";
		if (m_config.verbosity > 0)
			RELEASE_OUT("Print top modules to " << outName << "... ");
		SafeOutFile mapOut(outName.c_str());
		printMap(mapOut);
		if (m_config.verbosity > 0)
			RELEASE_OUT("done!\n");
	}

//	if (root()->firstChild->getSubInfomap() != 0)
//	{
//		outName = io::Str() << m_config.outDirectory << filename << "_1.map";
//		if (m_config.verbosity > 0)
//			RELEASE_OUT("Print first super module to " << outName << "... ");
//		SafeOutFile mapOut1(outName.c_str());
//		root()->firstChild->getSubInfomap()->printMap(mapOut1);
//		if (m_config.verbosity > 0)
//			RELEASE_OUT("done!\n");
//	}


	// Print .clu
	if (m_config.printClu)
	{
		outName = io::Str() << m_config.outDirectory << filename << ".clu";
		if (m_config.verbosity == 0)
			RELEASE_OUT("(Writing .clu file.. ) ");
		else
			RELEASE_OUT("Print cluster data to " << outName << "... ");
		SafeOutFile cluOut(outName.c_str());
		printClusterVector(cluOut);
		if (m_config.verbosity > 0)
			RELEASE_OUT("done!\n");
	}

	if (m_config.printNodeRanks)
	{
		outName = io::Str() << m_config.outDirectory << filename << ".rank";
		SafeOutFile rankOut(outName.c_str());
		if (m_config.verbosity > 0)
			RELEASE_OUT("Print node ranks to " << outName << "... " << std::flush);
		printNodeRanks(rankOut);
		if (m_config.verbosity > 0)
			RELEASE_OUT("done!\n");
	}

	// Print flow network
	if (m_config.printFlowNetwork)
	{
		outName = io::Str() << m_config.outDirectory << filename << ".flow";
		SafeOutFile flowOut(outName.c_str());
		if (m_config.verbosity == 0)
			RELEASE_OUT("(Writing .flow file.. " << std::flush);
		else
			RELEASE_OUT("Print flow network to " << outName << "... " << std::flush);
		printFlowNetwork(flowOut);
		if (m_config.verbosity == 0)
			RELEASE_OUT(") ");
		else
			RELEASE_OUT("done!\n");
	}

	if (m_config.printBinaryTree || m_config.printBinaryFlowTree)
	{
		bool writeEdges = m_config.printBinaryFlowTree;
		outName = io::Str() << m_config.outDirectory << filename << (writeEdges? ".bftree" : ".btree");

		HierarchicalNetwork hierData(filename, numLeafNodes(), !m_config.isUndirected(),
				hierarchicalCodelength, oneLevelCodelength, INFOMAP_VERSION);
		RELEASE_OUT("\nBuild streamable " << (writeEdges ? "flow " : "") <<	"tree... " << std::flush);
		buildHierarchicalNetwork(hierData, writeEdges);
		RELEASE_OUT("done! Writing to " << outName << "... " << std::flush);
		hierData.writeStreamableTree(outName, writeEdges);
		RELEASE_OUT("done!" << std::endl);
	}

}


void InfomapBase::printClusterVector(std::ostream& out)
{
	out << "*Vertices " << m_treeData.numLeafNodes() << std::endl;
	for (TreeData::leafIterator it(m_treeData.begin_leaf()), itEnd(m_treeData.end_leaf());
			it != itEnd; ++it)
	{
		unsigned int index = (*it)->parent->index;
		out <<  (index + 1) << std::endl;
	}
}


void InfomapBase::sortTree()
{
	sortTree(*root());
}

void InfomapBase::printSubInfomapTree(std::ostream& out, const TreeData& originalData, const std::string& prefix)
{
	unsigned int moduleIndex = 0;
	for (NodeBase::sibling_iterator moduleIt(root()->begin_child()), endIt(root()->end_child());
			moduleIt != endIt; ++moduleIt, ++moduleIndex)
	{
		std::ostringstream subPrefix;
		subPrefix << prefix << moduleIndex << ":";
		const std::string& subPrefixStr = subPrefix.str();
		if (moduleIt->getSubInfomap() == 0)
		{
			unsigned int nodeIndex = 0;
			for (NodeBase::sibling_iterator childIt(moduleIt->begin_child()), endChildIt(moduleIt->end_child());
					childIt != endChildIt; ++childIt, ++nodeIndex)
			{
				out << subPrefixStr << nodeIndex << " " << originalData.getLeafNode(childIt->originalIndex) <<
						" (" << childIt->originalIndex << ")" << std::endl;
			}
		}
		else
		{
			moduleIt->getSubInfomap()->printSubInfomapTree(out, originalData, subPrefixStr);
		}
	}
}

void InfomapBase::printSubInfomapTreeDebug(std::ostream& out, const TreeData& originalData, const std::string& prefix)
{
}

void InfomapBase::printTree(std::ostream& out, const NodeBase& root, const std::string& prefix)
{
	if (root.isLeaf())
	{
		out << prefix << " (" << root.parent->index << ")" << " \"" << root.name << "\"" << std::endl;
	}
	else
	{
		unsigned int childNumber = 1;
		for (NodeBase::const_sibling_iterator nodeIt(root.begin_child()), endIt(root.end_child());
				nodeIt != endIt; ++nodeIt, ++childNumber)
		{
			std::ostringstream subPrefix;
			subPrefix << prefix << childNumber << ":";
			const std::string& subPrefixStr = subPrefix.str();
			printTree(out, *nodeIt, subPrefixStr);
		}
	}
}

void InfomapBase::printPerLevelCodelength(std::ostream& out)
{
	std::vector<double> indexLengths;
	std::vector<double> leafLengths;
	aggregatePerLevelCodelength(indexLengths, leafLengths);

	unsigned int numLevels = leafLengths.size();
	indexLengths.resize(numLevels, 0.0);

	out << std::fixed << std::setprecision(9);
	out << "Per level codelength for modules:    [";
	for (unsigned int i = 0; i < numLevels - 1; ++i)
	{
		out << indexLengths[i] << ", ";
	}
	out << indexLengths[numLevels-1] << "]";
	double sumIndexLengths = 0.0;
	for (unsigned int i = 0; i < numLevels; ++i)
		sumIndexLengths += indexLengths[i];
	out << " (sum: " << sumIndexLengths << ")" << std::endl;

	out << "Per level codelength for leaf nodes: [";
	for (unsigned int i = 0; i < numLevels - 1; ++i)
	{
		out << leafLengths[i] << ", ";
	}
	out << leafLengths[numLevels-1] << "]";

	double sumLeafLengths = 0.0;
	for (unsigned int i = 0; i < numLevels; ++i)
		sumLeafLengths += leafLengths[i];
	out << " (sum: " << sumLeafLengths << ")" << std::endl;


	std::vector<double> codelengths(leafLengths);
	for (unsigned int i = 0; i < numLevels; ++i)
		codelengths[i] += indexLengths[i];
	out << "Per level codelength total:          [";
	for (unsigned int i = 0; i < numLevels - 1; ++i)
	{
		out << codelengths[i] << ", ";
	}
	out << codelengths[numLevels-1] << "]";

	double sumCodelengths = 0.0;
	for (unsigned int i = 0; i < numLevels; ++i)
		sumCodelengths += codelengths[i];
	out << " (sum: " << sumCodelengths << ")" << std::endl;

}

void InfomapBase::aggregatePerLevelCodelength(std::vector<double>& indexLengths,
		std::vector<double>& leafLengths, unsigned int level)
{
	aggregatePerLevelCodelength(*root(), indexLengths, leafLengths, level);
}

void InfomapBase::aggregatePerLevelCodelength(NodeBase& parent, std::vector<double>& indexLengths,
		std::vector<double>& leafLengths, unsigned int level)
{
	if (indexLengths.size() < level+1)
		indexLengths.resize(level+1, 0.0);
	if (leafLengths.size() < level+2)
		leafLengths.resize(level+2, 0.0);
	indexLengths[level] += parent.isRoot() ? indexCodelength : parent.codelength;

	for (NodeBase::sibling_iterator moduleIt(parent.begin_child()), endIt(parent.end_child());
			moduleIt != endIt; ++moduleIt)
	{
		if (moduleIt->getSubInfomap() != 0)
			moduleIt->getSubInfomap()->aggregatePerLevelCodelength(indexLengths, leafLengths, level+1);
		else if (!moduleIt->isLeaf())
		{
			if (moduleIt->firstChild->isLeaf())
				leafLengths[level+1] += moduleIt->codelength;
			else
				aggregatePerLevelCodelength(*moduleIt, indexLengths, leafLengths, level+1);
		}
	}
}

DepthStat InfomapBase::calcMaxAndAverageDepth()
{
	DepthStat stat;
	calcMaxAndAverageDepthHelper(*root(), stat.maxDepth, stat.averageDepth, 0);
	stat.averageDepth /= numLeafNodes();
	return stat;
}

void InfomapBase::calcMaxAndAverageDepthHelper(NodeBase& root, unsigned int& maxDepth, double& sumLeafDepth,
		unsigned int currentDepth)
{
	++currentDepth;
	for (NodeBase::sibling_iterator moduleIt(root.begin_child()), endIt(root.end_child());
			moduleIt != endIt; ++moduleIt)
	{
		if (moduleIt->getSubInfomap() != 0)
			calcMaxAndAverageDepthHelper(*moduleIt->getSubInfomap()->root(), maxDepth, sumLeafDepth, currentDepth);
		else if (!moduleIt->isLeaf())
			calcMaxAndAverageDepthHelper(*moduleIt, maxDepth, sumLeafDepth, currentDepth);
		else
		{
			sumLeafDepth += currentDepth;
			maxDepth = std::max(maxDepth, currentDepth);
		}
	}
}







