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
#include <numeric>
#include "../utils/FileURI.h"
#include "../io/SafeFile.h"
#include "../io/TreeDataWriter.h"
#include <cmath>
#include "../io/ClusterReader.h"
#include <limits>
#include <sstream>
#include <ios>
#include <iomanip>
#include "../utils/infomath.h"
#include "../io/convert.h"
#include "../utils/Stopwatch.h"
#include "../utils/Date.h"
#include "MemFlowNetwork.h"
#include "MemNetwork.h"
#include "MultiplexNetwork.h"
#include "NetworkAdapter.h"
#include "MemoryNetworkAdapter.h"
#ifdef _OPENMP
#include <omp.h>
#include <stdio.h>
#endif
#include <map>
#include "Network.h"
#include "FlowNetwork.h"
#include "../io/version.h"
#include <functional>

#ifdef NS_INFOMAP
namespace infomap
{
#endif

void InfomapBase::run()
{

#ifdef _OPENMP
#pragma omp parallel
	#pragma omp master
	{
		Log() << "(OpenMP " << _OPENMP << " detected, trying to parallelize the recursive part on " <<
				omp_get_num_threads() << " threads...)\n" << std::flush;
	}
#endif


	if (!initNetwork())
		return;

	run(m_ioNetwork);
}

void InfomapBase::run(Network& input, HierarchicalNetwork& output)
{

#ifdef _OPENMP
#pragma omp parallel
	#pragma omp master
	{
		Log() << "(OpenMP " << _OPENMP << " detected, trying to parallelize the recursive part on " <<
				omp_get_num_threads() << " threads...)\n" << std::flush;
	}
#endif

	m_externalOutput = true;

	if (!initNetwork(input))
		return;
	
	run(output);
}

void InfomapBase::run(HierarchicalNetwork& output)
{
	calcOneLevelCodelength();
	calcEntropyRate();

	if (m_config.benchmark)
		Logger::benchmark("calcFlow", root()->codelength, 1, 1, 1);

	unsigned int numTrials = m_config.numTrials;
	m_iterationStats.resize(numTrials);
	// std::vector<double> codelengths(numTrials);
	// std::vector<double> modulePerplexity(numTrials);
	// std::vector<double> moduleAssignments(numTrials);
	// std::vector<unsigned int> topModules(numTrials);
	std::ostringstream bestSolutionStatistics;
	unsigned int bestNumLevels = 0;

	Log() << "Initiating done in " << Stopwatch::getElapsedTimeSinceProgramStartInSec() << "s\n";

	for (unsigned int iTrial = 0; iTrial < numTrials; ++iTrial)
	{
		Log() << "\nAttempt " << (iTrial+1) << "/" << numTrials <<	" at " << Date();
		Log() << std::endl;
		m_trialIndex = iTrial;
		Stopwatch timer(true);

		// First clear existing modular structure
		while ((*m_treeData.begin_leaf())->parent != root())
		{
			root()->replaceChildrenWithGrandChildren();
		}

		hierarchicalCodelength = codelength = moduleCodelength = oneLevelCodelength;
		indexCodelength = 0.0;

		if (m_config.preClusterMultiplex && m_config.isMultiplexNetwork())
			preClusterMultiplexNetwork();

		if (m_config.clusterDataFile != "")
			consolidateExternalClusterData();

		if (!m_config.noInfomap)
			runPartition();

		if (oneLevelCodelength < hierarchicalCodelength - m_config.minimumCodelengthImprovement)
		{
			Log() << "Warning: No codelength improvement in modular solution over one-level solution!\n";
		}
		
		double topPerplexity = 0.0;
		double bottomPerplexity = 0.0;
		for (NodeBase::sibling_iterator moduleIt(root()->begin_child()), endIt(root()->end_child());
		moduleIt != endIt; ++moduleIt)
		{
			topPerplexity += -infomath::plogp(getNodeData(*moduleIt).flow);
		}
		topPerplexity = std::pow(2, topPerplexity);
		for (InfomapIterator it(root(), 1); !it.isEnd(); ++it) {
			if (it->isLeafModule()) {
				bottomPerplexity += -infomath::plogp(getNodeData(*it).flow);
			}
		}
		bottomPerplexity = std::pow(2, bottomPerplexity);
		
		// physicalId -> (moduleId -> flow)
		std::map<unsigned int, std::map<unsigned int, double> > topModulesPerPhysicalNode;
		std::map<unsigned int, std::map<unsigned int, double> > bottomModulesPerPhysicalNode;
		double weightedDepth = 0.0;
		for (InfomapIterator it(root(), 1); !it.isEnd(); ++it) {
			if (it->isLeaf()) {
				topModulesPerPhysicalNode[it->getPhysicalIndex()][it.moduleIndex()] += getNodeData(*it).flow;
				weightedDepth += it.depth() * getNodeData(*it).flow;
			}
		}
		for (InfomapIterator it(root(), -1); !it.isEnd(); ++it) {
			if (it->isLeaf()) {
				bottomModulesPerPhysicalNode[it->getPhysicalIndex()][it.moduleIndex()] += getNodeData(*it).flow;
			}
		}
		double topOverlap = 0.0;
		for (std::map<unsigned int, std::map<unsigned int, double> >::iterator it(topModulesPerPhysicalNode.begin()); it != topModulesPerPhysicalNode.end(); ++it) {
			std::map<unsigned int, double>& moduleFlow = it->second;
			double flow = std::accumulate(moduleFlow.begin(), moduleFlow.end(), 0.0, AddMapValues());
			double topOverlapPerplexityPerNode = 0.0;
			for (std::map<unsigned int, double>::iterator itModuleFlow(moduleFlow.begin()); itModuleFlow != moduleFlow.end(); ++itModuleFlow) {
				topOverlapPerplexityPerNode += -infomath::plogp(itModuleFlow->second / flow);
			}
			topOverlapPerplexityPerNode = std::pow(2, topOverlapPerplexityPerNode);
			topOverlap += flow * topOverlapPerplexityPerNode;
		}
		double bottomOverlap = 0.0;
		for (std::map<unsigned int, std::map<unsigned int, double> >::iterator it(bottomModulesPerPhysicalNode.begin()); it != bottomModulesPerPhysicalNode.end(); ++it) {
			std::map<unsigned int, double>& moduleFlow = it->second;
			double flow = std::accumulate(moduleFlow.begin(), moduleFlow.end(), 0.0, AddMapValues());
			double bottomOverlapPerplexityPerNode = 0.0;
			for (std::map<unsigned int, double>::iterator itModuleFlow(moduleFlow.begin()); itModuleFlow != moduleFlow.end(); ++itModuleFlow) {
				bottomOverlapPerplexityPerNode += -infomath::plogp(itModuleFlow->second / flow);
			}
			bottomOverlapPerplexityPerNode = std::pow(2, bottomOverlapPerplexityPerNode);
			bottomOverlap += flow * bottomOverlapPerplexityPerNode;
		}

		m_iterationStats[iTrial].iterationIndex = iTrial;
		m_iterationStats[iTrial].codelength = hierarchicalCodelength;
		m_iterationStats[iTrial].maxDepth = maxDepth();
		m_iterationStats[iTrial].weightedDepth = weightedDepth;
		m_iterationStats[iTrial].numTopModules = numTopModules();
		m_iterationStats[iTrial].numBottomModules = numBottomModules();
		m_iterationStats[iTrial].topPerplexity = topPerplexity;
		m_iterationStats[iTrial].bottomPerplexity = bottomPerplexity;
		m_iterationStats[iTrial].topOverlap = topOverlap;
		m_iterationStats[iTrial].bottomOverlap = bottomOverlap;
		m_iterationStats[iTrial].seconds = timer.getElapsedTimeInSec();

		if (numTrials > 1 && m_config.printAllTrials) {
			std::string outName = io::Str() << m_config.outName << "_" << (iTrial+1);
			printNetworkData(outName);
		}
		
		if (hierarchicalCodelength < bestHierarchicalCodelength)
		{
			bestHierarchicalCodelength = hierarchicalCodelength;
			bestSolutionStatistics.str("");
			printNetworkData(output);
			bestNumLevels = printPerLevelCodelength(bestSolutionStatistics);
			m_iterationStats[iTrial].isMinimum = true;
		}
	}

	Log() << "\n\n";

	unsigned int fieldWidth = 16;
	Log() << std::fixed << std::setprecision(9);
	Log() << "Finished clustering '" << m_config.outName << "' in " << numTrials << (
		numTrials == 1 ? " trial with:\n" : " trials with:\n");
	Log() << std::left << std::setw(fieldWidth) << "Iteration" << std::right;
	Log() << " ";
	Log() << std::setw(fieldWidth) << "Seconds";
	Log() << " ";
	if (m_config.twoLevel) {
		Log() << std::setw(fieldWidth) << "Modules";
		Log() << " ";
		Log() << std::setw(fieldWidth) << "Perplexity";
		Log() << " ";
		if (m_config.isMemoryNetwork()) {
			Log() << std::setw(fieldWidth) << "Overlap";
			Log() << " ";
		}
	} else {
		Log() << std::setw(fieldWidth) << "TopModules";
		Log() << " ";
		Log() << std::setw(fieldWidth) << "TopPerplexity";
		Log() << " ";
		if (m_config.isMemoryNetwork()) {
			Log() << std::setw(fieldWidth) << "TopOverlap";
			Log() << " ";
		}
		Log() << std::setw(fieldWidth) << "BottomModules";
		Log() << " ";
		Log() << std::setw(fieldWidth) << "BottomPerplexity";
		Log() << " ";
		if (m_config.isMemoryNetwork()) {
			Log() << std::setw(fieldWidth) << "BottomOverlap";
			Log() << " ";
		}
	}
	Log() << std::setw(fieldWidth) << "maxDepth";
	Log() << " ";
	Log() << std::setw(fieldWidth) << "weightedDepth";
	Log() << " ";
	Log() << std::setw(fieldWidth) << "Codelength";
	Log() << " ";
	// Log() << std::left << std::setw(fieldWidth) << "Minimum" << std::right;
	Log() << "\n";
	for (unsigned int i = 0; i < numTrials; ++i) {
		PerIterationStats& s = m_iterationStats[i];
		Log() << std::left << std::setw(fieldWidth) << s.iterationIndex + 1 << std::right;
		Log() << " ";
		Log() << std::setw(fieldWidth) << s.seconds;
		Log() << " ";
		Log() << std::setw(fieldWidth) << s.numTopModules;
		Log() << " ";
		Log() << std::setw(fieldWidth) << s.topPerplexity;
		Log() << " ";
		if (m_config.isMemoryNetwork()) {
			Log() << std::setw(fieldWidth) << s.topOverlap;
			Log() << " ";
		}
		if (!m_config.twoLevel) {
			Log() << std::setw(fieldWidth) << s.numBottomModules;
			Log() << " ";
			Log() << std::setw(fieldWidth) << s.bottomPerplexity;
			Log() << " ";
			if (m_config.isMemoryNetwork()) {
				Log() << std::setw(fieldWidth) << s.bottomOverlap;
				Log() << " ";
			}
		}
		Log() << std::setw(fieldWidth) << s.maxDepth;
		Log() << " ";
		Log() << std::setw(fieldWidth) << s.weightedDepth;
		Log() << " ";
		Log() << std::setw(fieldWidth) << s.codelength;
		Log() << " ";
		Log() << (s.isMinimum ? '*' : ' ');
		Log() << "\n";
	}
	
	if (numTrials > 1)
	{
		unsigned int iLast = numTrials - 1;
		unsigned int numFields = 5 + ((2 + (m_config.isMemoryNetwork() ? 1 : 0)) * (m_config.twoLevel ? 1 : 2));
		// Log() << "numFields: " << numFields << "\n";

		std::sort(m_iterationStats.begin(), m_iterationStats.end(), IterationStatsSortSeconds());
		double secondsMin = m_iterationStats[0].seconds;
		double secondsMax = m_iterationStats[iLast].seconds;
		double secondsMedian = m_iterationStats[numTrials / 2].seconds;
		double secondsAverage = std::accumulate(m_iterationStats.begin(),
			m_iterationStats.end(), 0.0, IterationStatsAddSeconds()) / numTrials;
		
		std::sort(m_iterationStats.begin(), m_iterationStats.end(), IterationStatsSortNumTopModules());
		unsigned int numTopModulesMin = m_iterationStats[0].numTopModules;
		unsigned int numTopModulesMax = m_iterationStats[iLast].numTopModules;
		unsigned int numTopModulesMedian = m_iterationStats[numTrials / 2].numTopModules;
		double numTopModulesAverage = std::accumulate(m_iterationStats.begin(),
			m_iterationStats.end(), 0.0, IterationStatsAddNumTopModules()) / numTrials;
		
		std::sort(m_iterationStats.begin(), m_iterationStats.end(), IterationStatsSortTopPerplexity());
		double topPerplexityMin = m_iterationStats[0].topPerplexity;
		double topPerplexityMax = m_iterationStats[iLast].topPerplexity;
		double topPerplexityMedian = m_iterationStats[numTrials / 2].topPerplexity;
		double topPerplexityAverage = std::accumulate(m_iterationStats.begin(),
			m_iterationStats.end(), 0.0, IterationStatsAddTopPerplexity()) / numTrials;
		
		std::sort(m_iterationStats.begin(), m_iterationStats.end(), IterationStatsSortTopOverlap());
		double topOverlapMin = m_iterationStats[0].topOverlap;
		double topOverlapMax = m_iterationStats[iLast].topOverlap;
		double topOverlapMedian = m_iterationStats[numTrials / 2].topOverlap;
		double topOverlapAverage = std::accumulate(m_iterationStats.begin(),
			m_iterationStats.end(), 0.0, IterationStatsAddTopOverlap()) / numTrials;
		
		std::sort(m_iterationStats.begin(), m_iterationStats.end(), IterationStatsSortNumBottomModules());
		unsigned int numBottomModulesMin = m_iterationStats[0].numBottomModules;
		unsigned int numBottomModulesMax = m_iterationStats[iLast].numBottomModules;
		unsigned int numBottomModulesMedian = m_iterationStats[numTrials / 2].numBottomModules;
		double numBottomModulesAverage = std::accumulate(m_iterationStats.begin(),
			m_iterationStats.end(), 0.0, IterationStatsAddNumBottomModules()) / numTrials;
		
		std::sort(m_iterationStats.begin(), m_iterationStats.end(), IterationStatsSortBottomPerplexity());
		double bottomPerplexityMin = m_iterationStats[0].bottomPerplexity;
		double bottomPerplexityMax = m_iterationStats[iLast].bottomPerplexity;
		double bottomPerplexityMedian = m_iterationStats[numTrials / 2].bottomPerplexity;
		double bottomPerplexityAverage = std::accumulate(m_iterationStats.begin(),
			m_iterationStats.end(), 0.0, IterationStatsAddBottomPerplexity()) / numTrials;
		
		std::sort(m_iterationStats.begin(), m_iterationStats.end(), IterationStatsSortBottomOverlap());
		double bottomOverlapMin = m_iterationStats[0].bottomOverlap;
		double bottomOverlapMax = m_iterationStats[iLast].bottomOverlap;
		double bottomOverlapMedian = m_iterationStats[numTrials / 2].bottomOverlap;
		double bottomOverlapAverage = std::accumulate(m_iterationStats.begin(),
			m_iterationStats.end(), 0.0, IterationStatsAddBottomOverlap()) / numTrials;
		
		std::sort(m_iterationStats.begin(), m_iterationStats.end(), IterationStatsSortMaxDepth());
		unsigned int maxDepthMin = m_iterationStats[0].maxDepth;
		unsigned int maxDepthMax = m_iterationStats[iLast].maxDepth;
		unsigned int maxDepthMedian = m_iterationStats[numTrials / 2].maxDepth;
		double maxDepthAverage = std::accumulate(m_iterationStats.begin(),
			m_iterationStats.end(), 0.0, IterationStatsAddMaxDepth()) / numTrials;
		
		std::sort(m_iterationStats.begin(), m_iterationStats.end(), IterationStatsSortWeightedDepth());
		double weightedDepthMin = m_iterationStats[0].weightedDepth;
		double weightedDepthMax = m_iterationStats[iLast].weightedDepth;
		double weightedDepthMedian = m_iterationStats[numTrials / 2].weightedDepth;
		double weightedDepthAverage = std::accumulate(m_iterationStats.begin(),
			m_iterationStats.end(), 0.0, IterationStatsAddWeightedDepth()) / numTrials;
		
		std::sort(m_iterationStats.begin(), m_iterationStats.end(), IterationStatsSortCodelength());
		double codelengthMin = m_iterationStats[0].codelength;
		double codelengthMax = m_iterationStats[iLast].codelength;
		double codelengthMedian = m_iterationStats[numTrials / 2].codelength;
		double codelengthAverage = std::accumulate(m_iterationStats.begin(),
		m_iterationStats.end(), 0.0, IterationStatsAddCodelength()) / numTrials;
		

		Log() << std::setfill('-') << std::setw(fieldWidth * numFields + numFields) << "\n" << std::setfill(' ');
		Log() << std::left << std::setw(fieldWidth) << "Min:" << std::right;
		Log() << " ";
		Log() << std::setw(fieldWidth) << secondsMin;
		Log() << " ";
		Log() << std::setw(fieldWidth) << numTopModulesMin;
		Log() << " ";
		Log() << std::setw(fieldWidth) << topPerplexityMin;
		Log() << " ";
		if (m_config.isMemoryNetwork()) {
			Log() << std::setw(fieldWidth) << topOverlapMin;
			Log() << " ";
		}
		if (!m_config.twoLevel) {
			Log() << std::setw(fieldWidth) << numBottomModulesMin;
			Log() << " ";
			Log() << std::setw(fieldWidth) << bottomPerplexityMin;
			Log() << " ";
			if (m_config.isMemoryNetwork()) {
				Log() << std::setw(fieldWidth) << bottomOverlapMin;
				Log() << " ";
			}
		}
		Log() << std::setw(fieldWidth) << maxDepthMin;
		Log() << " ";
		Log() << std::setw(fieldWidth) << weightedDepthMin;
		Log() << " ";
		Log() << std::setw(fieldWidth) << codelengthMin;
		Log() << "\n";
		Log() << std::left << std::setw(fieldWidth) << "Median:" << std::right;
		Log() << " ";
		Log() << std::setw(fieldWidth) << secondsMedian;
		Log() << " ";
		Log() << std::setw(fieldWidth) << numTopModulesMedian;
		Log() << " ";
		Log() << std::setw(fieldWidth) << topPerplexityMedian;
		Log() << " ";
		if (m_config.isMemoryNetwork()) {
			Log() << std::setw(fieldWidth) << topOverlapMedian;
			Log() << " ";
		}
		if (!m_config.twoLevel) {
			Log() << std::setw(fieldWidth) << numBottomModulesMedian;
			Log() << " ";
			Log() << std::setw(fieldWidth) << bottomPerplexityMedian;
			Log() << " ";
			if (m_config.isMemoryNetwork()) {
				Log() << std::setw(fieldWidth) << bottomOverlapMedian;
				Log() << " ";
			}
		}
		Log() << std::setw(fieldWidth) << maxDepthMedian;
		Log() << " ";
		Log() << std::setw(fieldWidth) << weightedDepthMedian;
		Log() << " ";
		Log() << std::setw(fieldWidth) << codelengthMedian;
		Log() << "\n";
		Log() << std::left << std::setw(fieldWidth) << "Average:" << std::right;
		Log() << " ";
		Log() << std::setw(fieldWidth) << secondsAverage;
		Log() << " ";
		Log() << std::setw(fieldWidth) << int(numTopModulesAverage + 0.5);
		Log() << " ";
		Log() << std::setw(fieldWidth) << topPerplexityAverage;
		Log() << " ";
		if (m_config.isMemoryNetwork()) {
			Log() << std::setw(fieldWidth) << topOverlapAverage;
			Log() << " ";
		}
		if (!m_config.twoLevel) {
			Log() << std::setw(fieldWidth) << int(numBottomModulesAverage + 0.5);
			Log() << " ";
			Log() << std::setw(fieldWidth) << bottomPerplexityAverage;
			Log() << " ";
			if (m_config.isMemoryNetwork()) {
				Log() << std::setw(fieldWidth) << bottomOverlapAverage;
				Log() << " ";
			}
		}
		Log() << std::setw(fieldWidth) << maxDepthAverage;
		Log() << " ";
		Log() << std::setw(fieldWidth) << weightedDepthAverage;
		Log() << " ";
		Log() << std::setw(fieldWidth) << codelengthAverage;
		Log() << "\n";
		Log() << std::left << std::setw(fieldWidth) << "Max:" << std::right;
		Log() << " ";
		Log() << std::setw(fieldWidth) << secondsMax;
		Log() << " ";
		Log() << std::setw(fieldWidth) << numTopModulesMax;
		Log() << " ";
		Log() << std::setw(fieldWidth) << topPerplexityMax;
		Log() << " ";
		if (m_config.isMemoryNetwork()) {
			Log() << std::setw(fieldWidth) << topOverlapMax;
			Log() << " ";
		}
		if (!m_config.twoLevel) {
			Log() << std::setw(fieldWidth) << numBottomModulesMax;
			Log() << " ";
			Log() << std::setw(fieldWidth) << bottomPerplexityMax;
			Log() << " ";
			if (m_config.isMemoryNetwork()) {
				Log() << std::setw(fieldWidth) << bottomOverlapMax;
				Log() << " ";
			}
		}
		Log() << std::setw(fieldWidth) << maxDepthMax;
		Log() << " ";
		Log() << std::setw(fieldWidth) << weightedDepthMax;
		Log() << " ";
		Log() << std::setw(fieldWidth) << codelengthMax;
		Log() << "\n";
	}
	
	Log() << "\n\n";
	

	if (bestIntermediateStatistics.str() != "")
	{
		Log() << "Best intermediate solution:" << std::endl;
		Log() << bestIntermediateStatistics.str() << std::endl << std::endl;
	}

	Log() << "Best end modular solution in " << bestNumLevels << " levels";
	if (bestHierarchicalCodelength > oneLevelCodelength)
		Log() << " (warning: worse than one-level solution)";
	Log() << ":" << std::endl;
	Log() << bestSolutionStatistics.str() << std::endl;

}

void InfomapBase::calcOneLevelCodelength()
{
	Log() << "Calculating one-level codelength... " << std::flush;
	oneLevelCodelength = indexCodelength = root()->codelength = calcCodelengthOnRootOfLeafNodes(*root());
	Log() << "done!\n  -> One-level codelength: " << io::toPrecision(indexCodelength) << std::endl;
}

void InfomapBase::calcEntropyRate()
{
	Log() << "Calculating entropy rate... " << std::flush;
	double entropyRate = 0.0;
	double physEntropyRate = 0.0;

	for (TreeData::leafIterator it(m_treeData.begin_leaf()), itEnd(m_treeData.end_leaf());
		it != itEnd; ++it)
	{
		NodeBase& node = **it;
		double sumOutFlow = 0.0;
		double entropy = 0.0;
		double physEntropy = 0.0;
		std::map<unsigned int, double> physOutFlow;
		for (NodeBase::edge_iterator edgeIt(node.begin_outEdge()), edgeEnd(node.end_outEdge());
				edgeIt != edgeEnd; ++edgeIt)
		{
			EdgeType& edge = **edgeIt;
			sumOutFlow += edge.data.flow;
			physOutFlow[edge.target.getPhysicalIndex()] += edge.data.flow;
		}
		for (NodeBase::edge_iterator edgeIt(node.begin_outEdge()), edgeEnd(node.end_outEdge());
				edgeIt != edgeEnd; ++edgeIt)
		{
			EdgeType& edge = **edgeIt;
			entropy += -infomath::plogp(edge.data.flow / sumOutFlow);
		}
		if (m_config.isMemoryNetwork()) {
			for (std::map<unsigned int, double>::iterator physIt(physOutFlow.begin()); physIt != physOutFlow.end(); ++physIt)
			{
				double physFlow = physIt->second;
				physEntropy += -infomath::plogp(physFlow / sumOutFlow);
			}
			physEntropyRate += getNodeData(node).flow * physEntropy;
		}
		entropyRate += getNodeData(node).flow * entropy;
	}

	Log() << "done!\n";
	if (m_config.isMemoryNetwork()) {
		Log() << "  -> State entropy rate:    " << io::toPrecision(entropyRate) << std::endl;
		Log() << "  -> Physical entropy rate: " << io::toPrecision(physEntropyRate) << std::endl;
	}
	else {
		Log() << "  -> Entropy rate: " << io::toPrecision(entropyRate) << std::endl;
	}
}

void InfomapBase::runPartition()
{
	m_tuneIterationIndex = 0;
	if (m_config.twoLevel)
	{
		partition();
		hierarchicalCodelength = codelength;
		for (NodeBase::sibling_iterator moduleIt(root()->begin_child()), endIt(root()->end_child());
				moduleIt != endIt; ++moduleIt)
		{
			moduleIt->codelength = calcCodelengthOnModuleOfLeafNodes(*moduleIt);
		}
		return;
	}

	PartitionQueue partitionQueue;

	if (haveModules())
	{
		if (m_config.fastHierarchicalSolution <= 1)
		{
			// Try to tune existing solution before hierarchical algorithm
			partition();
			hierarchicalCodelength = codelength;
			for (NodeBase::sibling_iterator moduleIt(root()->begin_child()), endIt(root()->end_child());
					moduleIt != endIt; ++moduleIt)
			{
				moduleIt->codelength = calcCodelengthOnModuleOfLeafNodes(*moduleIt);
			}
			queueTopModules(partitionQueue);
		}
		else
		{
			queueLeafModules(partitionQueue);
		}
	}
	else if (m_config.fastHierarchicalSolution != 0)
	{
		unsigned int numLevelsCreated = findSuperModulesIterativelyFast(partitionQueue);

		// Print current hierarchical solution
		if (m_config.fastHierarchicalSolution < 3 && hierarchicalCodelength < bestIntermediateCodelength)
		{
			bestIntermediateCodelength = hierarchicalCodelength;
			bestIntermediateStatistics.clear();
			bestIntermediateStatistics.str("");
			printPerLevelCodelength(bestIntermediateStatistics);
			printNetworkData(io::Str() << m_config.outName << "_fast");

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
	Log(0,0) << "\nRecursive sub-structure compression: " << std::flush;
	Log(1) << "Current codelength: " << indexCodelength << " + " <<
		(hierarchicalCodelength - indexCodelength) << " = " <<
		io::toPrecision(hierarchicalCodelength) << " in " << numTopModules() << " modules\n" <<
		"\nTrying to find deeper structure under current modules recursively... \n";

	double sumConsolidatedCodelength = hierarchicalCodelength - partitionQueue.moduleCodelength;
	// Log(1) << "Consolidated codelength: " << hierarchicalCodelength << " - " << partitionQueue.moduleCodelength <<
	// 	" = " << sumConsolidatedCodelength << "\n";

	if (m_config.resetConfigBeforeRecursion) {
		m_config.reset();
	}

//	double t0 = omp_get_wtime();

	while (partitionQueue.size() > 0)
	{
		Log(1,1) << "Level " << partitionQueue.level << ": " << (partitionQueue.flow*100) <<
				"% of the flow in " << partitionQueue.size() << " modules. Finding sub-modules... " << std::setprecision(6) << std::flush;
		Log(2) << "Level " << partitionQueue.level << ": " << (partitionQueue.flow*100) <<
				"% of the flow in " << partitionQueue.size() << " modules with consolidated codelength " <<
				sumConsolidatedCodelength << ". Finding sub-modules... " << std::setprecision(6) << std::flush;

		PartitionQueue nextLevelQueue;
		// Partition all modules in the queue and fill up the next level queue
		processPartitionQueue(partitionQueue, nextLevelQueue);

		double leftToImprove = partitionQueue.moduleCodelength;
		sumConsolidatedCodelength += partitionQueue.indexCodelength + partitionQueue.leafCodelength;
		double limitCodelength = sumConsolidatedCodelength + leftToImprove;

		Log(0,0) << ((hierarchicalCodelength - limitCodelength) / hierarchicalCodelength) * 100 <<
			"% " << std::flush;
		Log(1,1) << "done! Codelength " << partitionQueue.indexCodelength << " + " <<
					partitionQueue.leafCodelength << " (+ " << leftToImprove << " left to improve)" <<
					" -> limit: " << io::toPrecision(limitCodelength) << " bits.\n";
		Log(2) << "done!\n      -> " << nextLevelQueue.size() << " sub-modules with codelength " << partitionQueue.indexCodelength << " + " <<
					partitionQueue.leafCodelength << " (+ " << leftToImprove << " left to improve)" <<
					" -> limit: " << io::toPrecision(limitCodelength) << " bits.\n";

		hierarchicalCodelength = limitCodelength;

		partitionQueue.swap(nextLevelQueue);
	}
	Log(0,0) << ". Found " << partitionQueue.level << " levels with codelength " <<
		io::toPrecision(hierarchicalCodelength) << "\n";
	Log(1) << "==> Found " << partitionQueue.level << " levels with codelength " <<
		io::toPrecision(hierarchicalCodelength) << "\n";
//	double t1 = omp_get_wtime();
}


double InfomapBase::partitionAndQueueNextLevel(PartitionQueue& partitionQueue, bool tryIndexing)
{
	DEBUG_OUT("InfomapBase::hierarchicalPartition()..." << std::endl);

//	hierarchicalCodelength = codelength = root()->codelength; // only != 0 on top root

	if (numLeafNodes() == 1)
	{
		hierarchicalCodelength = codelength = root()->codelength;
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
//			Log() << "*";
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

void InfomapBase::queueLeafModules(PartitionQueue& partitionQueue)
{
	unsigned int numLeafModules = 0;
	for (NodeBase::leaf_module_iterator leafModuleIt(m_treeData.root()); !leafModuleIt.isEnd(); ++leafModuleIt, ++numLeafModules)
	{}

	// Add modules to partition queue
	partitionQueue.resize(numLeafModules);
	unsigned int numNonTrivialModules = 0;
	double sumFlow = 0.0;
	double sumNonTrivialFlow = 0.0;
	double sumModuleCodelength = 0.0;
	unsigned int moduleIndex = 0;
	unsigned int maxDepth = 0;
	for (NodeBase::leaf_module_iterator leafModuleIt(m_treeData.root()); !leafModuleIt.isEnd(); ++leafModuleIt, ++moduleIndex)
	{
		partitionQueue[moduleIndex] = leafModuleIt.base();
		double flow = getNodeData(*leafModuleIt).flow;
		sumFlow += flow;
		sumModuleCodelength += leafModuleIt->codelength;
		if (leafModuleIt->childDegree() > 1)
		{
			++numNonTrivialModules;
			sumNonTrivialFlow += flow;
		}
		maxDepth = std::max(maxDepth, leafModuleIt.depth());
	}
	partitionQueue.flow = sumFlow;
	partitionQueue.numNonTrivialModules = numNonTrivialModules;
	partitionQueue.nonTrivialFlow = sumNonTrivialFlow;
	partitionQueue.indexCodelength = indexCodelength;
	partitionQueue.moduleCodelength = sumModuleCodelength;
	partitionQueue.level = maxDepth;
}

void InfomapBase::tryIndexingIteratively()
{
//	return indexCodelength;//TODO: DEBUG!!
	unsigned int numIndexingCompleted = 0;
	bool verbose = m_subLevel == 0;

	if (verbose)
	{
		Log(0,0) << "Finding ";
		Log(1) << "\n";
	}

	double minHierarchicalCodelength = hierarchicalCodelength;
	// Add index codebooks as long as the code gets shorter (and collapse each iteration)
	bool tryIndexing = true;
	bool replaceExistingModules = m_config.fastHierarchicalSolution == 0;
	replaceExistingModules = true; // Uses leaf network below
	while(tryIndexing)
	{
		if (verbose)
		{
			Log(1) << "Trying to find super modules... ";
			Log(3) << std::endl;
		}

		// std::auto_ptr<InfomapBase> superInfomap(getNewInfomapInstance());
		std::auto_ptr<InfomapBase> superInfomap(getNewInfomapInstanceWithoutMemory());
		superInfomap->m_trialIndex = m_trialIndex;

		superInfomap->m_subLevel = m_subLevel + m_TOP_LEVEL_ADDITION;
		superInfomap->reseed(numIndexingCompleted);
		superInfomap->initSuperNetwork(*root());
		superInfomap->partition();


		// Break if trivial super structure
		//		if (superInfomap->numTopModules()  == 1 || superInfomap->numTopModules() == numTopModules())
		if (superInfomap->m_numNonTrivialTopModules  == 1 ||
				superInfomap->numTopModules() == numTopModules())
		{
			if (verbose)
			{
				Log(1) << "failed to find non-trivial super modules." << std::endl;
			}
			break;
		}
		else if (superInfomap->codelength > indexCodelength - m_config.minimumCodelengthImprovement)
		{
			if (verbose)
			{
				Log(1) << "two-level index codebook not improved over one-level." << std::endl;
			}
			break;
		}

		minHierarchicalCodelength += superInfomap->codelength - indexCodelength;

		if (verbose)
		{
			Log(0,0) << superInfomap->numTopModules() << " ";
			Log(1) << "succeeded. Found " << superInfomap->numTopModules() << " "
						"super modules with estimated hierarchical codelength " <<
						minHierarchicalCodelength << ".\n";
		}

		// Replace current module structure with the super structure
		setActiveNetworkFromLeafs();
//		setActiveNetworkFromChildrenOfRoot();
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

		double superIndexCodelength = superInfomap->indexCodelength;
		if (std::abs(superIndexCodelength - indexCodelength) > 1e-10)
			std::cout << "*** (" << superIndexCodelength << " / " << indexCodelength << ") ";


		++numIndexingCompleted;
		tryIndexing = m_numNonTrivialTopModules > 1 && numTopModules() != numLeafNodes();
	}

	if (verbose)
		Log(0,0) << "super modules with estimated codelength " <<
				io::toPrecision(minHierarchicalCodelength) << ". ";

	hierarchicalCodelength = replaceExistingModules ? codelength : minHierarchicalCodelength;
}

/**
 * Like mergeAndConsolidateRepeatedly but let it build up the tree for each
 * new level of aggregation. It doesn't create new Infomap instances.
 */
unsigned int InfomapBase::findSuperModulesIterativelyFast(PartitionQueue& partitionQueue)
{
	Log(0,1) << "Index module compression: " << std::setprecision(2) << std::flush;
	Log(2) << "Trying to find fast hierarchy... " << std::endl;

	unsigned int networkLevel = 0;
	unsigned int numLevelsCreated = 0;
	hierarchicalCodelength = 0.0;

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

		Log(2) << "Level " << ++networkLevel << ", moving " << m_activeNetwork.size() <<
							" " << nodesLabel << "... " << std::flush;

		unsigned int numOptimizationLoops = optimizeModules();

		bool acceptSolution = codelength < oldIndexLength - m_config.minimumCodelengthImprovement;
		// Force at least one modular level!
		bool acceptByForce = !acceptSolution && numLevelsCreated == 0;
		if (acceptByForce)
			acceptSolution = true;

		workingHierarchicalCodelength += codelength - oldIndexLength;

		Log(0,1) << hideIf(!acceptSolution) << ((hierarchicalCodelength - workingHierarchicalCodelength) \
				/ hierarchicalCodelength * 100) << "% " << std::flush;
		Log(2) << "found " << numDynamicModules() << " modules in " << numOptimizationLoops <<
					" loops with hierarchical codelength " << indexCodelength << " + " <<
					(workingHierarchicalCodelength - indexCodelength) << " = " <<
					workingHierarchicalCodelength << (acceptSolution ? "\n" :
							", discarding the solution.\n") << std::flush;

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
			moduleIt->codelength = calcCodelengthOnModuleOfLeafNodes(*moduleIt);
		}

		if (isLeafLevel && m_config.fastHierarchicalSolution > 1)
		{
			queueTopModules(partitionQueue);
		}

		nodesLabel = "modules";
		isLeafLevel = false;
		++numLevelsCreated;

	} while (m_numNonTrivialTopModules != 1);

	codelength = hierarchicalCodelength;
	moduleCodelength = codelength - indexCodelength;

	Log() << std::setprecision(m_config.verboseNumberPrecision);
	Log(0,0) << "to codelength " << io::toPrecision(hierarchicalCodelength) << " in " <<
				numTopModules() << " top modules. ";
	Log(1) << "done! Added " << numLevelsCreated << " levels with " <<
				numTopModules() << " top modules to codelength: " <<
				io::toPrecision(hierarchicalCodelength) << " ";

	return numLevelsCreated;
}

unsigned int InfomapBase::deleteSubLevels()
{
	if (!haveModules())
		return 0;

	// Clear all possible sub-modules
	unsigned int numSubModulesRemoved = 0;
	unsigned int maxNumLevelsRemoved = 0;
	for (NodeBase::sibling_iterator moduleIt(root()->begin_child()), endIt(root()->end_child());
			moduleIt != endIt; ++moduleIt)
	{
		unsigned int numLevelsRemoved = 0;
		while (!moduleIt->isLeafModule())
		{
			numSubModulesRemoved += moduleIt->replaceChildrenWithGrandChildren();
			if (numSubModulesRemoved > 0)
				++numLevelsRemoved;
		}
		maxNumLevelsRemoved = std::max(maxNumLevelsRemoved, numLevelsRemoved);
	}

	if (numSubModulesRemoved == 0)
		return 0;

	// Reset to leaf-level codelength terms
	setActiveNetworkFromLeafs();
	initConstantInfomapTerms();

//	recalculateCodelengthFromConsolidatedNetwork();
	resetModuleFlowFromLeafNodes();
	double sumModuleLength = 0.0;
	for (NodeBase::sibling_iterator moduleIt(root()->begin_child()), endIt(root()->end_child());
			moduleIt != endIt; ++moduleIt)
	{
		sumModuleLength += moduleIt->codelength = calcCodelengthOnModuleOfLeafNodes(*moduleIt);
	}
	moduleCodelength = sumModuleLength;
	hierarchicalCodelength = codelength = indexCodelength + moduleCodelength;

	if (m_subLevel == 0)
	{
		Log(1) << "Cleared " << numSubModulesRemoved << " sub-modules in " << maxNumLevelsRemoved <<
				io::toPlural(" level", maxNumLevelsRemoved) << " to codelength " << indexCodelength <<
				" + " << moduleCodelength << " = " << io::toPrecision(codelength) << " in " <<
				numTopModules() << " modules." << std::endl;
	}

	return maxNumLevelsRemoved;
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
#pragma omp parallel for schedule(dynamic)
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
		module.codelength = calcCodelengthOnModuleOfLeafNodes(module);

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
		subInfomap->reseed(moduleIndex + m_subLevel);

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
	bool verbose = m_subLevel == 0 || (isSuperLevelOnTopLevel() && m_config.verbosity > 2);
	// verbose = m_subLevel == 0;
//	verbose = m_subLevel == 0 || (m_subLevel == 1 && m_config.verbosity > 2);

	bool initiatedWithModules = haveModules();
	if (initiatedWithModules)
	{
		// Delete possible sub-modules and move nodes into current modular structure
		deleteSubLevels();
		unsigned int i = 0;
		for (NodeBase::sibling_iterator moduleIt(root()->begin_child()), endIt(root()->end_child());
				moduleIt != endIt; ++moduleIt, ++i)
		{
			moduleIt->index = i;
		}
		i = 0;
		setActiveNetworkFromLeafs();
		for (TreeData::leafIterator leafIt(m_treeData.begin_leaf()), endIt(m_treeData.end_leaf());
				leafIt != endIt; ++leafIt, ++i)
		{
			m_moveTo[i] = (*leafIt)->parent->index;
		}

		initConstantInfomapTerms();
		initModuleOptimization();
		moveNodesToPredefinedModules();
	}
	else
	{
		setActiveNetworkFromLeafs();
		initConstantInfomapTerms();
		initModuleOptimization();
	}

	m_tuneIterationIndex = 0;

	if (verbose)
	{
		Log() << "Initiated to codelength " << indexCodelength << " + " << moduleCodelength << " = " <<
				io::toPrecision(codelength) << " in " << numTopModules() << " modules.\n";
		Log(0,0) << "Two-level compression: " << std::setprecision(2) << std::flush;
		Log(1) << "Trying to find modular structure... \n";

		if (m_config.benchmark)
			Logger::benchmark("init", codelength, numTopModules(), numNonTrivialTopModules(), 2);
	}

	double initialCodelength = codelength;

	if (useHardPartitions())
	{
		Log() << "Move " << numLeafNodes() << " memory nodes into physical node structure... ";
		// Move memory nodes into their physical nodes
		unsigned int i = 0;
		for (TreeData::leafIterator leafIt(m_treeData.begin_leaf()), endIt(m_treeData.end_leaf());
				leafIt != endIt; ++leafIt, ++i)
		{
			m_moveTo[i] = getMemoryNode(**leafIt).physIndex;
		}

		moveNodesToPredefinedModules();
		consolidateModules();

		Log() << "done! Codelength: " << io::toPrecision(codelength) << " in " << numTopModules() << " modules\n";

		setActiveNetworkFromChildrenOfRoot();
		initModuleOptimization();
	}

	// First optimization iteration
	mergeAndConsolidateRepeatedly(forceConsolidation, fast);

	double diffInitial = codelength - initialCodelength;
	if (diffInitial > 1e-10) {
		Log() << "*"; //TODO: Check how much and why!
		if (diffInitial > 1e-6)
			Log() << "(warning: codelength " << initialCodelength << " -> " << codelength << ") ";
	}

	double oldCodelength = oneLevelCodelength;
	double compression = (oldCodelength - codelength)/oldCodelength;
	if (verbose)
		Log(0,0) << (compression * 100) << "% " << std::flush;

	if (!fast && m_config.tuneIterationLimit != 1 && numTopModules() != numLeafNodes())
	{
		unsigned int coarseTuneLevel = m_config.coarseTuneLevel - 1;
		bool doFineTune = true;
		bool fineTuneLeafNodes = !useHardPartitions();
		bool coarseTuned = false;
		oldCodelength = codelength;
		while (numTopModules() > 1)
		{
			++m_tuneIterationIndex;
			if (doFineTune)
			{
				fineTune(fineTuneLeafNodes);
				if (coarseTuned &&
						(codelength > oldCodelength - initialCodelength*m_config.minimumRelativeTuneIterationImprovement ||
								codelength > oldCodelength - m_config.minimumCodelengthImprovement))
					break;
				compression = (oldCodelength - codelength)/oldCodelength;
				if (verbose)
					Log(0,0) << (compression * 100) << "% " << std::flush;
				oldCodelength = codelength;
			}
			else
			{
				coarseTune(m_config.alternateCoarseTuneLevel ? (++coarseTuneLevel % m_config.coarseTuneLevel) :
						m_config.coarseTuneLevel - 1);
				coarseTuned = true;
				if (codelength > oldCodelength - initialCodelength*m_config.minimumRelativeTuneIterationImprovement ||
						codelength > oldCodelength - m_config.minimumCodelengthImprovement)
					break;
				compression = (oldCodelength - codelength)/oldCodelength;
				if (verbose)
					Log(0,0) << (compression * 100) << "% " << std::flush;
				oldCodelength = codelength;
			}
			if (m_config.tuneIterationLimit == m_tuneIterationIndex + 1)
				break;
			doFineTune = !doFineTune;
		}
	}

	if (verbose)
	{
		Log(0,0) << "to " << numTopModules() << " modules with codelength " <<
					std::setprecision(6) << io::toPrecision(codelength) << std::endl;
		Log(1) << "Two-level codelength: " << indexCodelength << " + " << moduleCodelength << " = " <<
					io::toPrecision(codelength) << std::endl;
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
	m_aggregationLevel = 0;
	unsigned int numLevelsConsolidated = 0;

	bool verbose = (m_subLevel == 0 && m_config.verbosity != 0) ||
			(isSuperLevelOnTopLevel() && m_config.verbosity >= 3);
	// Merge and collapse repeatedly until no code improvement or only one big cluster left

	if (m_config.fastFirstIteration && m_tuneIterationIndex == 0 && m_subLevel == 0) {
		if (verbose) {
			Log() << "Iteration 0, moving " << m_activeNetwork.size() << "*" << std::flush;
		}
		unsigned int numFastLoops = optimizeModulesCrude();

		consolidateModules(!useHardPartitions());
		++numLevelsConsolidated;

		codelength = calcCodelengthOnAllNodesInTree();
		indexCodelength = root()->codelength;
		moduleCodelength = codelength - indexCodelength;

		if (verbose)
			Log() << numFastLoops << " nodes*loops to codelength " << codelength <<
				" (" << indexCodelength << " + " << moduleCodelength << ")" <<
				" in " << numTopModules() << " modules. (" << m_numNonTrivialTopModules <<
				" non-trivial modules)" << std::endl;

		setActiveNetworkFromChildrenOfRoot();
		initModuleOptimization();
	}

	if (verbose) {
		Log() << "Iteration " << (m_tuneIterationIndex + 1) << ", moving " << m_activeNetwork.size() << "*" << std::flush;
	}

	// Core loop, merging modules
	unsigned int numOptimizationLoops = optimizeModules();

	if (verbose)
		Log() << numOptimizationLoops << ", " << std::flush;

	// Force create modules even if worse (don't mix modules and leaf nodes under the same parent)
	bool replaceExistingModules = !useHardPartitions();
	consolidateModules(replaceExistingModules);

	++numLevelsConsolidated;
	unsigned int levelAggregationLimit = getLevelAggregationLimit();

	// Reapply core algorithm on modular network, replacing modules with super modules
	while (numTopModules() > 1 && numLevelsConsolidated != levelAggregationLimit)
	{
		double consolidatedCodelength = codelength;
		double consolidatedIndexLength = indexCodelength;
		double consolidatedModuleLength = moduleCodelength;

		++m_aggregationLevel;

		if (m_subLevel == 0 && m_config.benchmark)
			Logger::benchmark(io::Str() << "lvl" << numLevelsConsolidated, codelength, numTopModules(),
					numNonTrivialTopModules(), 2);

		if (verbose)
			Log() << "" << numTopModules() << "*" << std::flush;

		setActiveNetworkFromChildrenOfRoot();
		initModuleOptimization();

		numOptimizationLoops = optimizeModules();

		if (verbose)
			Log() << numOptimizationLoops << ", " << std::flush;

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

	if (verbose)
	{
		Log() << (m_isCoarseTune ? "modules" : "nodes") << "*loops to codelength " << codelength <<
				" (" << indexCodelength << " + " << moduleCodelength << ")" <<
				" in " << numTopModules() << " modules. (" << m_numNonTrivialTopModules <<
				" non-trivial modules)" << std::endl;
	}
	if (m_subLevel == 0 && m_config.benchmark)
	{
		Logger::benchmark(io::Str() << "iter" << m_tuneIterationIndex, codelength, numTopModules(),
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

void InfomapBase::fineTune(bool leafLevel)
{
	if (!leafLevel && !haveSubModules())
		leafLevel = true;

	if(leafLevel)
	{
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
	}
	else
	{
		setActiveNetworkFromLeafModules();
		m_moveTo.resize(m_activeNetwork.size());
		for (unsigned int i = 0; i < m_activeNetwork.size(); ++i)
		{
			m_moveTo[i] = m_activeNetwork[i]->parent->index;
		}
	}

	initModuleOptimization();

//	Log() << "\n--> FineTune: initial codelength: " << indexCodelength << " + " << moduleCodelength << " = " << codelength << "";

	moveNodesToPredefinedModules();

//	Log() << "\n--> FineTune: predefined codelength: " << indexCodelength << " + " << moduleCodelength << " = " << codelength << "";

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
	if (numTopModules() == 1)
		return;

	m_isCoarseTune = true;
	if (m_subLevel == 0)
		partitionEachModuleParallel(recursiveCount, m_config.fastCoarseTunePartition);
	else
		partitionEachModule(recursiveCount, m_config.fastCoarseTunePartition);

	bool keepLeafModules = useHardPartitions();
	unsigned int i = 0;
	if (keepLeafModules)
	{
		setActiveNetworkFromLeafModules();
		for (unsigned int i = 0; i < m_activeNetwork.size(); ++i)
			m_moveTo[i] = m_activeNetwork[i]->index;
	}
	else
	{
		// Prepare leaf network to move into the sub-module structure given from partitioning each module
		setActiveNetworkFromLeafs();
		for (TreeData::leafIterator leafIt(m_treeData.begin_leaf()), endIt(m_treeData.end_leaf());
				leafIt != endIt; ++leafIt, ++i)
		{
			m_moveTo[i] = (*leafIt)->index;
			ASSERT(m_moveTo[i] < m_activeNetwork.size());
		}
	}

	initModuleOptimization();
	moveNodesToPredefinedModules();
	if (keepLeafModules)
	{
		// Don't replace leaf modules
		consolidateModules(false, true);
		// Replace top-modules with coarse-tune modules
		root()->replaceChildrenWithGrandChildren();
	}
	else
	{
		// Replace the module level with the sub-module level and store the module level structure on the sub-modules
		consolidateModules(true, true);
	}

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
	m_isCoarseTune = false;

	// TODO: don't keep initial structure in mergeAndConsolidateRepeatedly if already having sub-modules
	if (keepLeafModules)
	{
		for (NodeBase::sibling_iterator moduleIt(root()->begin_child()), endIt(root()->end_child());
				moduleIt != endIt; ++moduleIt)
		{
			moduleIt->replaceChildrenWithGrandChildren();
		}
	}
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

		std::auto_ptr<InfomapBase> subInfomap(getNewInfomapInstance());
		// To not happen to get back the same network with the same seed
		subInfomap->m_subLevel = m_subLevel + 1;
		subInfomap->initSubNetwork(*moduleIt, false);
		subInfomap->reseed(getSeedFromCodelength(codelength));
		//		if (hierarchical)
		//			subInfomap->hierarchicalPartition();
		//		else
		subInfomap->partition(recursiveCount, fast);

		NodeBase::sibling_iterator originalLeafNodeIt(moduleIt->begin_child());
		for (TreeData::leafIterator leafIt(subInfomap->m_treeData.begin_leaf()), endIt(subInfomap->m_treeData.end_leaf());
				leafIt != endIt; ++leafIt, ++originalLeafNodeIt)
		{
			NodeBase& node = **leafIt;
			originalLeafNodeIt->index = node.parent->index + moduleIndexOffset;
		}
		moduleIndexOffset += subInfomap->m_treeData.root()->childDegree();
	}

}

void InfomapBase::partitionEachModuleParallel(unsigned int recursiveCount, bool fast)
{
#ifndef _OPENMP
	return partitionEachModule(recursiveCount, fast);
#endif

//	double t0 = omp_get_wtime();

	// Store pointers to all modules in a vector
	unsigned int numModules = root()->childDegree();
	std::vector<NodeBase*> modules(numModules);
	NodeBase::sibling_iterator moduleIt(root()->begin_child());
	for (unsigned int i = 0; i < numModules; ++i, ++moduleIt)
		modules[i] = moduleIt.base();

	// Sort modules on flow
	std::multimap<double, NodeBase*, std::greater<double> > sortedModules;
	for (unsigned int i = 0; i < numModules; ++i)
	{
		sortedModules.insert(std::pair<double, NodeBase*>(getNodeData(*modules[i]).flow, modules[i]));
	}
	std::multimap<double, NodeBase*, std::greater<double> >::const_iterator sortedModuleIt(sortedModules.begin());
	for (unsigned int i = 0; i < numModules; ++i, ++sortedModuleIt)
		modules[i] = sortedModuleIt->second;

//	double t1 = omp_get_wtime();
//	Log() << "Sorting: " << (t1-t0)*1000 << " ms";

	// Partition each module in a parallel loop
	int numModulesInt = static_cast<int>(numModules);
	int iModule;
#pragma omp parallel for schedule(dynamic)
	for(iModule = 0; iModule < numModulesInt; ++iModule)
	{
//		omp_sched_t sched;
//		int mod;
//		omp_get_schedule(&sched, &mod);
//		printf("\nThread %d in loop index %d (schedule type: %d, chunk size: %d)\n", omp_get_thread_num(), iModule, sched, mod);

		unsigned int moduleIndex = static_cast<unsigned int>(iModule);
		NodeBase& module = *modules[moduleIndex];

		// Delete former sub-structure if exists
		module.getSubStructure().subInfomap.reset(0);

		if (module.childDegree() > 1)
		{
			std::auto_ptr<InfomapBase> subInfomap(getNewInfomapInstance());
			subInfomap->m_subLevel = m_subLevel + 1;
			subInfomap->reseed(getSeedFromCodelength(codelength));
			subInfomap->initSubNetwork(module, false);
			subInfomap->partition(recursiveCount, fast);

			module.getSubStructure().subInfomap = subInfomap;
		}
	}

//	double t2 = omp_get_wtime();
//	Log() << "\nParallel for: " << (t2-t1)*1000 << " ms\n";

	// Collect result: set sub-module index on each module
	unsigned int moduleIndexOffset = 0;
	for (unsigned int i = 0; i < numModules; ++i)
	{
		NodeBase& module = *modules[i];
		if (module.getSubStructure().haveSubInfomapInstance())
		{
			InfomapBase& subInfomap = *module.getSubStructure().subInfomap;

			NodeBase::sibling_iterator originalLeafNodeIt(module.begin_child());
			for (TreeData::leafIterator leafIt(subInfomap.m_treeData.begin_leaf()), endIt(subInfomap.m_treeData.end_leaf());
					leafIt != endIt; ++leafIt, ++originalLeafNodeIt)
			{
				NodeBase& node = **leafIt;
				originalLeafNodeIt->index = node.parent->index + moduleIndexOffset;
			}
			moduleIndexOffset += subInfomap.m_treeData.root()->childDegree();
		}
		else
		{
			for (NodeBase::sibling_iterator nodeIt(module.begin_child()), endIt(module.end_child());
					nodeIt != endIt; ++nodeIt)
			{
				nodeIt->index = moduleIndexOffset;
			}
			moduleIndexOffset += 1;
		}
	}
}

bool InfomapBase::initNetwork()
{
 	if (checkAndConvertBinaryTree())
 		return false;

	if (m_config.isMemoryNetwork())
	{
		initMemoryNetwork();
		return true;
	}

	Network network(m_config);

	network.readInputData();

	if (m_config.isBipartite() && m_config.hideBipartiteNodes) {
		m_config.maxNodeIndexVisible = network.numNodes() - network.numBipartiteNodes() - 1;
		Log() << "Skip " << network.numBipartiteNodes() << " bipartites nodes in output, limit to " <<
				m_config.maxNodeIndexVisible + 1 << " ordinary nodes.\n";
	}
	m_config.minBipartiteNodeIndex = network.numNodes() - network.numBipartiteNodes();

	return initNetwork(network);
}

bool InfomapBase::initNetwork(Network& network)
{
	if (m_config.isMemoryNetwork())
	{
		initMemoryNetwork(static_cast<MemNetwork&>(network));
		return true;
	}

	if (!network.isFinalized()) {
		Log() << "Finalizing network...\n";
		network.finalizeAndCheckNetwork();
	}

 	if (network.numNodes() == 0)
		throw InternalOrderError("Zero nodes or missing finalization of network.");


 	network.initNodeNames();

	std::string outname = m_config.outName;
 	if (m_config.printPajekNetwork)
 	{
 		std::string outName = io::Str() << m_config.outDirectory << outname << ".net";
 		Log() << "Printing network to " << outName << "... " << std::flush;
 		network.printNetworkAsPajek(outName);
		Log() << "done!\n";
 	}
	if (m_config.printStateNetwork)
 	{
 		std::string outName = io::Str() << m_config.outDirectory << outname << "_states.net";
 		Log() << "Printing state network to " << outName << "... " << std::flush;
 		network.printStateNetwork(outName);
		Log() << "done!\n";
 	}

 	FlowNetwork flowNetwork;
 	flowNetwork.calculateFlow(network, m_config);

 	network.disposeLinks();
	network.swapNodeNames(m_nodeNames);

 	const std::vector<double>& nodeFlow = flowNetwork.getNodeFlow();
 	const std::vector<double>& nodeTeleportWeights = flowNetwork.getNodeTeleportRates();
 	m_treeData.reserveNodeCount(network.numNodes());

 	for (unsigned int i = 0; i < network.numNodes(); ++i)
 		m_treeData.addNewNode(m_nodeNames[i], nodeFlow[i], nodeTeleportWeights[i]);
 	const FlowNetwork::LinkVec& links = flowNetwork.getFlowLinks();
 	for (unsigned int i = 0; i < links.size(); ++i)
 		m_treeData.addEdge(links[i].source, links[i].target, links[i].weight, links[i].flow * m_config.markovTime);

	
	if (m_config.variableMarkovTime)
	{
		// Adjust flow for constant entropy
		bool useWeightedEntropy = true;
		double averageNodeFlow = 1.0 / network.numNodes();
		
		double sumEntropy = 0.0;
		for (TreeData::leafIterator it(m_treeData.begin_leaf()), itEnd(m_treeData.end_leaf());
				it != itEnd; ++it)
		{
			NodeBase& node = **it;
			double sumOutFlow = 0.0;
			double entropy = 0.0;
			for (NodeBase::edge_iterator edgeIt(node.begin_outEdge()), edgeEnd(node.end_outEdge());
					edgeIt != edgeEnd; ++edgeIt)
			{
				EdgeType& edge = **edgeIt;
				sumOutFlow += edge.data.flow;
				// entropy += -infomath::plogp(edge.data.flow);
			}
			for (NodeBase::edge_iterator edgeIt(node.begin_outEdge()), edgeEnd(node.end_outEdge());
					edgeIt != edgeEnd; ++edgeIt)
			{
				EdgeType& edge = **edgeIt;
				entropy += -infomath::plogp(edge.data.flow / sumOutFlow);
			}
			entropy *= useWeightedEntropy ? getNodeData(node).flow : averageNodeFlow;
			sumEntropy += entropy;

			// Log() << " " << entropy << ", ";
			
		}
		double averageEntropyPerNode = sumEntropy;
		Log() << "  -> Adjust variable markov time (current weighted average node entropy: " << averageEntropyPerNode << ")\n";
		// double entropyFactor = m_config.constantEntropy / sumEntropy;
		for (TreeData::leafIterator it(m_treeData.begin_leaf()), itEnd(m_treeData.end_leaf());
				it != itEnd; ++it)
		{
			NodeBase& node = **it;
			double sumOutFlow = 0.0;
			double entropy = 0.0;
			for (NodeBase::edge_iterator edgeIt(node.begin_outEdge()), edgeEnd(node.end_outEdge());
					edgeIt != edgeEnd; ++edgeIt)
			{
				EdgeType& edge = **edgeIt;
				sumOutFlow += edge.data.flow;
			}
			for (NodeBase::edge_iterator edgeIt(node.begin_outEdge()), edgeEnd(node.end_outEdge());
					edgeIt != edgeEnd; ++edgeIt)
			{
				EdgeType& edge = **edgeIt;
				entropy += -infomath::plogp(edge.data.flow / sumOutFlow);
			}
			// entropy *= useWeightedEntropy ? getNodeData(node).flow : averageNodeFlow;
			for (NodeBase::edge_iterator edgeIt(node.begin_outEdge()), edgeEnd(node.end_outEdge());
					edgeIt != edgeEnd; ++edgeIt)
			{
				EdgeType& edge = **edgeIt;
				edge.data.flow *= entropy < 1e-10 ? 1000 : averageEntropyPerNode / entropy;
			}

			// Log() << " " << (entropy < 1e-10 ? 1000 : m_config.constantEntropy / entropy) << ", ";
			
		}
	}


 	double sumNodeFlow = 0.0;
	for (unsigned int i = 0; i < nodeFlow.size(); ++i)
		sumNodeFlow += nodeFlow[i];
	if (std::abs(1.0 - sumNodeFlow) > 1e-10)
		Log() << "Warning: Sum node flow differ from 1 by " << (1.0 - sumNodeFlow) << "\n";

 	initEnterExitFlow();


	if (m_config.printNodeRanks)
	{
		//TODO: Split printNetworkData to printNetworkData and printModuleData, and move this to first
		std::string outfile = io::Str() <<
				m_config.outDirectory << outname << ".rank";
		Log() << "Printing node flow to " << outfile << "... ";
		SafeOutFile out(outfile.c_str());

		out << "# node-flow\n";
		for (unsigned int i = 0; i < nodeFlow.size(); ++i)
		{
			out << nodeFlow[i] << "\n";
		}

		Log() << "done!\n";
	}

	// Print flow network
	if (m_config.printFlowNetwork)
	{
		std::string outName = io::Str() << m_config.outDirectory << outname << (m_config.printExpanded? "_expanded.flow" : ".flow");
		SafeOutFile flowOut(outName.c_str());
		Log() << "Printing flow network to " << outName << "... " << std::flush;
		printFlowNetwork(flowOut);
		Log() << "done!\n";
	}

 	return true;
}

void InfomapBase::initMemoryNetwork()
{
	std::auto_ptr<MemNetwork> net(m_config.isMultiplexNetwork() ? new MultiplexNetwork(m_config) : new MemNetwork(m_config));
	MemNetwork& network = *net;

	network.readInputData();

	initMemoryNetwork(network);
}

void InfomapBase::initMemoryNetwork(MemNetwork& network)
{
	if (!network.isFinalized()) {
		Log() << "Finalizing memory network...\n";
		network.finalizeAndCheckNetwork();
	}

	if (network.numNodes() == 0)
		throw InternalOrderError("Zero nodes or missing finalization of network.");

	network.initNodeNames();

	std::string outname = m_config.outName;
	if (m_config.printPajekNetwork)
 	{
 		std::string outName = io::Str() << m_config.outDirectory << outname << ".net";
 		Log() << "Printing network to " << outName << "... " << std::flush;
 		network.printNetworkAsPajek(outName);
		Log() << "done!\n";
 	}
	if (m_config.printStateNetwork)
 	{
 		std::string outName = io::Str() << m_config.outDirectory << outname << "_states.net";
 		Log() << "Printing state network to " << outName << "... " << std::flush;
 		network.printStateNetwork(outName);
		Log() << "done!\n";
 	}


	MemFlowNetwork flowNetwork;
	flowNetwork.calculateFlow(network, m_config);

	network.disposeLinks();
	network.swapNodeNames(m_nodeNames);

//	const std::vector<std::string>& nodeNames = network.nodeNames();
	const std::vector<double>& nodeFlow = flowNetwork.getNodeFlow();
	const std::vector<double>& nodeTeleportWeights = flowNetwork.getNodeTeleportRates();
	m_treeData.reserveNodeCount(network.numStateNodes());
	const std::vector<StateNode>& stateNodes = flowNetwork.getStateNodes();

	for (unsigned int i = 0; i < network.numStateNodes(); ++i) {
		m_treeData.addNewNode("", nodeFlow[i], nodeTeleportWeights[i]);
		StateNode& stateNode = getMemoryNode(m_treeData.getLeafNode(i));
		stateNode.stateIndex = stateNodes[i].stateIndex;
		stateNode.physIndex = stateNodes[i].physIndex;
	}

	const FlowNetwork::LinkVec& links = flowNetwork.getFlowLinks();

	for (unsigned int i = 0; i < links.size(); ++i) {
		// Ignore self-links
//		if (links[i].source == links[i].target)
//			continue;
		m_treeData.addEdge(links[i].source, links[i].target, links[i].weight, links[i].flow * m_config.markovTime);
	}

//	std::vector<double> m1Flow(network.numNodes(), 0.0);

	// Add physical nodes
	const MemNetwork::StateNodeMap& nodeMap = network.stateNodeMap();
	for (MemNetwork::StateNodeMap::const_iterator statenodeIt(nodeMap.begin()); statenodeIt != nodeMap.end(); ++statenodeIt)
	{
		unsigned int nodeIndex = statenodeIt->second;
		getPhysicalMembers(m_treeData.getLeafNode(nodeIndex)).push_back(PhysData(statenodeIt->first.physIndex, nodeFlow[nodeIndex]));
//		m1Flow[statenodeIt->first.physIndex] += nodeFlow[nodeIndex];
	}

	double sumNodeFlow = 0.0;
	for (unsigned int i = 0; i < nodeFlow.size(); ++i)
		sumNodeFlow += nodeFlow[i];
	if (std::abs(1.0 - sumNodeFlow) > 1e-10)
		Log() << "Warning: Sum node flow differ from 1 by " << (1.0 - sumNodeFlow) << "\n";

	initEnterExitFlow();


	if (m_config.printNodeRanks)
	{
		unsigned int indexOffset = m_config.zeroBasedNodeNumbers ? 0 : 1;
		if (m_config.printExpanded)
		{
			std::string outName = io::Str() << m_config.outDirectory << outname << "_expanded.rank";
			Log() << "Printing node flow to " << outName << "... " << std::flush;
			SafeOutFile out(outName.c_str());

			// Sort the state nodes on flow
			std::multimap<double, StateNode, std::greater<double> > sortedMemNodes;
			for (unsigned int i = 0; i < stateNodes.size(); ++i)
				sortedMemNodes.insert(std::make_pair(nodeFlow[i], stateNodes[i]));

			out << "# statestate nodeIndex flow teleportationWeight\n";
			std::multimap<double, StateNode, std::greater<double> >::const_iterator it(sortedMemNodes.begin());
			for (unsigned int i = 0; i < stateNodes.size(); ++i, ++it)
			{
				const StateNode& stateNode = it->second;
				out << stateNode.print(indexOffset) << " " <<
						it->first << " " << nodeTeleportWeights[i] << "\n";

			}

			Log() << "done!\n";
		}
		else
		{
			//TODO: Split printNetworkData to printNetworkData and printModuleData, and move this to first
			std::string outName = io::Str() <<
					m_config.outDirectory << outname << ".rank";
			Log() << "Printing physical flow to " << outName << "... " << std::flush;
			SafeOutFile out(outName.c_str());
			double sumFlow = 0.0;
			double sumStateflow = 0.0;
			std::vector<double> m1Flow(network.numNodes(), 0.0);
			for (unsigned int i = 0; i < network.numStateNodes(); ++i)
			{
				const PhysData& physData = getPhysicalMembers(m_treeData.getLeafNode(i))[0];
				m1Flow[physData.physNodeIndex] += physData.sumFlowFromStateNode;
				sumFlow += physData.sumFlowFromStateNode;
				sumStateflow += nodeFlow[i];
			}
			for (unsigned int i = 0; i < m1Flow.size(); ++i)
			{
				out << m1Flow[i] << "\n";
			}
			Log() << "done!\n";
		}
	}

	// Print flow network
	if (m_config.printFlowNetwork)
	{
		std::string outName = io::Str() << m_config.outDirectory << outname << (m_config.printExpanded? "_expanded.flow" : ".flow");
		SafeOutFile flowOut(outName.c_str());
		Log() << "Printing flow network to " << outName << "... " << std::flush;
		printFlowNetwork(flowOut);
		Log() << "done!\n";
	}
}

void InfomapBase::initSubNetwork(NodeBase& parent, bool recalculateFlow)
{
	DEBUG_OUT("InfomapBase::initSubNetwork()..." << std::endl);
	root()->owner = &parent;
	cloneFlowData(parent, *root());
	generateNetworkFromChildren(parent); // Updates the exitNetworkFlow for the nodes
}

void InfomapBase::initSuperNetwork(NodeBase& parent)
{
	DEBUG_OUT("InfomapBase::initSuperNetwork()..." << std::endl);
	root()->owner = &parent;
	generateNetworkFromChildren(parent);
	transformNodeFlowToEnterFlow(root());
}

void InfomapBase::setActiveNetworkFromChildrenOfRoot()
{
	DEBUG_OUT("InfomapBase::setActiveNetworkFromChildrenOfRoot() with childDegree: " <<
			root()->childDegree() << "... ");
	unsigned int numNodes = root()->childDegree();
	m_activeNetwork = m_nonLeafActiveNetwork;
	m_activeNetwork.resize(numNodes);
	unsigned int i = 0;
	for (NodeBase::sibling_iterator childIt(root()->begin_child()), endIt(root()->end_child());
			childIt != endIt; ++childIt, ++i)
	{
		m_activeNetwork[i] = childIt.base();
	}
	DEBUG_OUT("done!\n");
}

void InfomapBase::setActiveNetworkFromLeafModules()
{
	unsigned int numNodes = 0;
	for (NodeBase::leaf_module_iterator leafModuleIt(root()); !leafModuleIt.isEnd(); ++leafModuleIt)
		++numNodes;
	m_activeNetwork = m_nonLeafActiveNetwork;
	m_activeNetwork.resize(numNodes);
	unsigned int i = 0;
	for (NodeBase::leaf_module_iterator leafModuleIt(root()); !leafModuleIt.isEnd(); ++leafModuleIt, ++i)
	{
		m_activeNetwork[i] = leafModuleIt.base();
	}
}

void InfomapBase::setActiveNetworkFromLeafs()
{
	DEBUG_OUT("InfomapBase::setActiveNetworkFromLeafs(), numNodes: " << m_treeData.numLeafNodes() << std::endl);

	m_activeNetwork = m_treeData.m_leafNodes;
	m_moveTo.resize(m_activeNetwork.size());
}

bool InfomapBase::consolidateExternalClusterData(bool printResults)
{
	Log() << "Build hierarchical structure from external cluster data... " << std::flush;

	std::auto_ptr<NetworkAdapter> adapter;
	if (m_config.isMemoryNetwork())
		adapter = std::auto_ptr<NetworkAdapter>(new MemoryNetworkAdapter(m_config, m_treeData));
	else
		adapter = std::auto_ptr<NetworkAdapter>(new NetworkAdapter(m_config, m_treeData));

	bool isModulesLoaded = adapter->readExternalHierarchy(m_config.clusterDataFile);

	if (!isModulesLoaded)
		return false;

	initPreClustering(printResults);
	return true;
}

bool InfomapBase::preClusterMultiplexNetwork(bool printResults)
{
	// overridden
	return false;
}

void InfomapBase::initPreClustering(bool printResults)
{
	unsigned int numLevels = aggregateFlowValuesFromLeafToRoot();

	m_initialMaxNumberOfModularLevels = numLevels - 1;

	hierarchicalCodelength = codelength = calcCodelengthOnAllNodesInTree();

	indexCodelength = root()->codelength;

	moduleCodelength = hierarchicalCodelength - indexCodelength;

	Log() << " -> Codelength " << indexCodelength << " + " << moduleCodelength <<
			" = " << io::toPrecision(hierarchicalCodelength) << std::endl;

	if (!printResults)
		return;

	if (oneLevelCodelength < hierarchicalCodelength - m_config.minimumCodelengthImprovement)
	{
		Log() << "\n -> Warning: No improvement in modular solution over one-level solution.";
	}

	printNetworkData();
	std::ostringstream solutionStatistics;
	printPerLevelCodelength(solutionStatistics);

	Log() << "Hierarchical solution in " << numLevels << " levels:\n";
	Log() << solutionStatistics.str() << std::endl;

	return;
}

bool InfomapBase::checkAndConvertBinaryTree()
{
	if (FileURI(m_config.networkFile).getExtension() != "bftree" &&
			FileURI(m_config.networkFile).getExtension() != "btree")
		return false;

	m_ioNetwork.readStreamableTree(m_config.networkFile);

	printHierarchicalData(m_ioNetwork);

	return true;
}


unsigned int InfomapBase::maxDepth()
{
	unsigned int maxDepth = 0;
	for (InfomapIterator it(root()); !it.isEnd(); ++it) {
		if (it->isLeaf()) {
			maxDepth = std::max(maxDepth, it.depth());
		}
	}
	return maxDepth;
}

unsigned int InfomapBase::numBottomModules()
{
	unsigned int numBottomModules = 0;
	for (InfomapIterator it(root(), -1); !it.isEnd(); ++it) {
		if (it->isLeafModule()) {
			// numBottomModules = it.moduleIndex();
			++numBottomModules;
		}
	}
	// ++numBottomModules;
	return numBottomModules;
}

void InfomapBase::printNetworkData(std::string filename)
{
	printNetworkData(m_ioNetwork, filename);
}

void InfomapBase::printNetworkData(HierarchicalNetwork& output, std::string filename)
{
	if (m_config.noFileOutput && !m_externalOutput)
		return;

	if (filename.empty())
		filename = m_config.outName;

	std::string outName;

	// Print hierarchy
	if (m_config.printTree ||
			m_config.printFlowTree ||
			m_config.printBinaryTree ||
			m_config.printBinaryFlowTree ||
			m_config.printMap ||
			m_config.printClu)
	{
		// Sort tree on flow
		sortTree();

		bool writeEdges = m_config.printBinaryFlowTree || m_config.printFlowTree || m_config.printMap || m_externalOutput;
		Log() << "\nBuilding output tree" << (writeEdges ? " with links" : "") << "... " << std::flush;

		output.clear(m_config);
		saveHierarchicalNetwork(output, filename, writeEdges);

		if (!m_config.noFileOutput)
		{
			printHierarchicalData(output, filename);

			// Clear the data after written to file, if not used as a library
			if (!m_externalOutput)
				output.clear();
		}

	}

//	// Print .clu
//	if (m_config.printClu && !m_config.noFileOutput && !m_externalOutput)
//	{
//		outName = io::Str() << m_config.outDirectory << filename <<
//			(m_config.printExpanded ? "_expanded.clu" : ".clu");
//		Log(0,0) << "(Writing .clu file.. ) ";
//		Log(1) << "Print cluster data to " << outName << "... ";
//		SafeOutFile cluOut(outName.c_str());
//		printClusterNumbers(cluOut);
//		Log(1) << "done!\n";
//	}

}

void InfomapBase::printHierarchicalData(HierarchicalNetwork& hierarchicalNetwork, std::string filename)
{
	if (filename.empty())
		filename = m_config.outName;

	std::string outName;
	std::string outNameWithoutExtension = io::Str() << m_config.outDirectory << filename <<
			(m_config.printExpanded && m_config.isMemoryNetwork() ? "_expanded" : "");

	// Print .tree
	if (m_config.printTree)
	{
		outName = io::Str() << outNameWithoutExtension << ".tree";
		Log(0,0) << "writing .tree... " << std::flush;
		Log(1) << "\n  -> Writing " << outName << "..." << std::flush;
		hierarchicalNetwork.writeHumanReadableTree(outName);
	}

	if (m_config.printFlowTree)
	{
		outName = io::Str() << outNameWithoutExtension << ".ftree";
		Log(0,0) << "writing .ftree... " << std::flush;
		Log(1) << "\n  -> Writing " << outName << "..." << std::flush;
		hierarchicalNetwork.writeHumanReadableTree(outName, true);
	}

	if (m_config.printBinaryTree)
	{
		outName = io::Str() << outNameWithoutExtension << ".btree";
		Log(0,0) << "writing .btree... " << std::flush;
		Log(1) << "\n  -> Writing " << outName << "..." << std::flush;
		hierarchicalNetwork.writeStreamableTree(outName, false);
	}

	if (m_config.printBinaryFlowTree)
	{
		outName = io::Str() << outNameWithoutExtension << ".bftree";
		Log(0,0) << "writing .bftree... " << std::flush;
		Log(1) << "\n  -> Writing " << outName << "..." << std::flush;
		hierarchicalNetwork.writeStreamableTree(outName, true);
	}

	if (m_config.printMap)
	{
		outName = io::Str() << outNameWithoutExtension << ".map";
		Log(0,0) << "writing .map... " << std::flush;
		Log(1) << "\n  -> Writing " << outName << "..." << std::flush;

		hierarchicalNetwork.writeMap(outName);
	}

	if (m_config.printClu)
	{
		outName = io::Str() << outNameWithoutExtension << ".clu";
		Log(0,0) << "writing .clu... " << std::flush;
		Log(1) << "\n  -> Writing " << outName << "..." << std::flush;

		hierarchicalNetwork.writeClu(outName);
	}

	Log(0,0) << "done!" << std::endl;
	Log(1) << "\nDone!" << std::endl;
}

void InfomapBase::printClusterNumbers(std::ostream& out)
{
	out << "# '" << m_config.parsedArgs << "' -> " << numLeafNodes() << " nodes " <<
		"partitioned in " << m_config.elapsedTime() << " from codelength " <<
		io::toPrecision(oneLevelCodelength, 9, true) << " in one level to codelength " <<
		io::toPrecision(codelength, 9, true) << ".\n";

	out << "*Vertices " << m_treeData.numLeafNodes() << "\n";
	for (TreeData::leafIterator it(m_treeData.begin_leaf()), itEnd(m_treeData.end_leaf());
			it != itEnd; ++it)
	{
		unsigned int index = (*it)->parent->index;
		out <<  (index + 1) << "\n";
	}
}

void InfomapBase::printTreeLevelSizes(std::ostream& out, std::string heading)
{
	Log() << heading << std::endl;
	std::map<unsigned int, unsigned int> levelMap;
	for (NodeBase::pre_depth_first_iterator it(root()); !it.isEnd(); ++it) {
		++levelMap[it.depth()];
	}

	for (std::map<unsigned int, unsigned int>::iterator it(levelMap.begin()); it != levelMap.end(); ++it) {
		Log() << "[Level " << it->first << "]: " << it->second << "\n";
	}
	Log() << std::flush;
}


void InfomapBase::sortTree()
{
	sortTree(*root());
}

unsigned int InfomapBase::printPerLevelCodelength(std::ostream& out)
{
	std::vector<PerLevelStat> perLevelStats;
	aggregatePerLevelCodelength(perLevelStats);

	unsigned int numLevels = perLevelStats.size();
	double averageNumNodesPerLevel = 0.0;
	for (unsigned int i = 0; i < numLevels; ++i)
		averageNumNodesPerLevel += perLevelStats[i].numNodes();
	averageNumNodesPerLevel /= numLevels;

	out << "Per level number of modules:         [";
	for (unsigned int i = 0; i < numLevels - 1; ++i)
	{
		out << io::padValue(perLevelStats[i].numModules, 11) << ", ";
	}
	out << io::padValue(perLevelStats[numLevels-1].numModules, 11) << "]";
	unsigned int sumNumModules = 0;
	for (unsigned int i = 0; i < numLevels; ++i)
		sumNumModules += perLevelStats[i].numModules;
	out << " (sum: " << sumNumModules << ")" << std::endl;

	out << "Per level number of leaf nodes:      [";
	for (unsigned int i = 0; i < numLevels - 1; ++i)
	{
		out << io::padValue(perLevelStats[i].numLeafNodes, 11) << ", ";
	}
	out << io::padValue(perLevelStats[numLevels-1].numLeafNodes, 11) << "]";
	unsigned int sumNumLeafNodes = 0;
	for (unsigned int i = 0; i < numLevels; ++i)
		sumNumLeafNodes += perLevelStats[i].numLeafNodes;
	out << " (sum: " << sumNumLeafNodes << ")" << std::endl;


	out << "Per level average child degree:      [";
	double childDegree = perLevelStats[0].numNodes();
	double sumAverageChildDegree = childDegree * childDegree;
	if (numLevels > 1) {
		out << io::padValue(perLevelStats[0].numModules, 11) << ", ";
	}
	for (unsigned int i = 1; i < numLevels - 1; ++i)
	{
		childDegree = perLevelStats[i].numNodes() * 1.0 / perLevelStats[i-1].numModules;
		sumAverageChildDegree += childDegree * perLevelStats[i].numNodes();
		out << io::padValue(childDegree, 11) << ", ";
	}
	if (numLevels > 1) {
		childDegree = perLevelStats[numLevels-1].numNodes() * 1.0 / perLevelStats[numLevels-2].numModules;
		sumAverageChildDegree += childDegree * perLevelStats[numLevels-1].numNodes();
	}
	out << io::padValue(childDegree, 11) << "]";
	out << " (average: " << sumAverageChildDegree/(sumNumModules + sumNumLeafNodes) << ")" << std::endl;

	out << std::fixed << std::setprecision(9);
	out << "Per level codelength for modules:    [";
	for (unsigned int i = 0; i < numLevels - 1; ++i)
	{
		out << perLevelStats[i].indexLength << ", ";
	}
	out << perLevelStats[numLevels-1].indexLength << "]";
	double sumIndexLengths = 0.0;
	for (unsigned int i = 0; i < numLevels; ++i)
		sumIndexLengths += perLevelStats[i].indexLength;
	out << " (sum: " << sumIndexLengths << ")" << std::endl;

	out << "Per level codelength for leaf nodes: [";
	for (unsigned int i = 0; i < numLevels - 1; ++i)
	{
		out << perLevelStats[i].leafLength << ", ";
	}
	out << perLevelStats[numLevels-1].leafLength << "]";

	double sumLeafLengths = 0.0;
	for (unsigned int i = 0; i < numLevels; ++i)
		sumLeafLengths += perLevelStats[i].leafLength;
	out << " (sum: " << sumLeafLengths << ")" << std::endl;

	out << "Per level codelength total:          [";
	for (unsigned int i = 0; i < numLevels - 1; ++i)
	{
		out << perLevelStats[i].codelength() << ", ";
	}
	out << perLevelStats[numLevels-1].codelength() << "]";

	double sumCodelengths = 0.0;
	for (unsigned int i = 0; i < numLevels; ++i)
		sumCodelengths += perLevelStats[i].codelength();
	out << " (sum: " << sumCodelengths << ")" << std::endl;

	return numLevels;
}

void InfomapBase::aggregatePerLevelCodelength(std::vector<PerLevelStat>& perLevelStat, unsigned int level)
{
	aggregatePerLevelCodelength(*root(), perLevelStat, level);
}

void InfomapBase::aggregatePerLevelCodelength(NodeBase& parent, std::vector<PerLevelStat>& perLevelStat, unsigned int level)
{
	if (perLevelStat.size() < level+1)
		perLevelStat.resize(level+1);

	if (parent.firstChild->isLeaf()) {
		perLevelStat[level].numLeafNodes += parent.childDegree();
		perLevelStat[level].leafLength += parent.codelength;
		return;
	}

	perLevelStat[level].numModules += parent.childDegree();
	perLevelStat[level].indexLength += parent.isRoot() ? indexCodelength : parent.codelength;

	for (NodeBase::sibling_iterator moduleIt(parent.begin_child()), endIt(parent.end_child());
			moduleIt != endIt; ++moduleIt)
	{
		if (moduleIt->getSubStructure().haveSubInfomapInstance())
			moduleIt->getSubStructure().subInfomap->aggregatePerLevelCodelength(perLevelStat, level+1);
		else
			aggregatePerLevelCodelength(*moduleIt, perLevelStat, level+1);
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

#ifdef NS_INFOMAP
}
#endif
