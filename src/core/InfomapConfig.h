/*
 * InfomapConfig.h
 *
 *  Created on: 2 mar 2015
 *      Author: Daniel
 */

#ifndef MODULES_CLUSTERING_CLUSTERING_INFOMAPCONFIG_H_
#define MODULES_CLUSTERING_CLUSTERING_INFOMAPCONFIG_H_

#include "../io/Config.h"
#include <string>

namespace infomap {

struct ClusterConfig {
	bool twoLevel = false;
	bool noCoarseTune = false;
	bool directedEdges = false;
	double codeRate = 1.0;
	unsigned int preferredNumberOfModules = 0;
	double multiplexRelaxRate = 0.15;
	unsigned long seedToRandomNumberGenerator = 123;

	unsigned int numTrials = 1;
	double minimumCodelengthImprovement = 1e-10;
	double minimumSingleNodeCodelengthImprovement = 1e-10;
	bool randomizeCoreLoopLimit = false;
	unsigned int coreLoopLimit = 20;
	unsigned int levelAggregationLimit = 0;
	unsigned int tuneIterationLimit = 0; // num iterations of fine-tune/coarse-tune in two-level partition)
	double minimumRelativeTuneIterationImprovement = 1e-10;
	bool fastCoarseTunePartition = false;
	bool alternateCoarseTuneLevel = false;
	unsigned int coarseTuneLevel = 0;
	unsigned int fastHierarchicalSolution = false;
	int superLevelLimit = -1; // Max super level iterations
	bool onlySuperModules = false;
	bool fastFirstIteration = false;
	bool rawdir = false;
	unsigned int lowMemoryPriority = false; // Prioritize memory efficient algorithms before fast if > 0
	bool innerParallelization = false;
	std::string clusterDataFile = "";
	bool clusterDataIsHard = false;

	ClusterConfig& operator=(const ClusterConfig& other) {
		twoLevel = other.twoLevel;
		noCoarseTune = other.noCoarseTune;
		directedEdges = other.directedEdges;
		codeRate = other.codeRate;
		preferredNumberOfModules = other.preferredNumberOfModules;
		multiplexRelaxRate = other.multiplexRelaxRate;
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
		fastHierarchicalSolution = other.fastHierarchicalSolution;
		superLevelLimit = other.superLevelLimit;
		onlySuperModules = other.onlySuperModules;
		fastFirstIteration = other.fastFirstIteration;
		rawdir = other.rawdir;
		lowMemoryPriority = other.lowMemoryPriority;
		innerParallelization = other.innerParallelization;
		clusterDataFile = other.clusterDataFile;
		clusterDataIsHard = other.clusterDataIsHard;
		return *this;
	}

	ClusterConfig& cloneAsNonMain(const ClusterConfig& other) {
		twoLevel = other.twoLevel;
		noCoarseTune = other.noCoarseTune;
		directedEdges = other.directedEdges;
		codeRate = other.codeRate;
		// preferredNumberOfModules = other.preferredNumberOfModules;
		multiplexRelaxRate = other.multiplexRelaxRate;
		seedToRandomNumberGenerator = other.seedToRandomNumberGenerator;
		numTrials = other.numTrials;
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
		// fastHierarchicalSolution = other.fastHierarchicalSolution;
		// superLevelLimit = other.superLevelLimit;
		// onlySuperModules = other.onlySuperModules;
		// fastFirstIteration = other.fastFirstIteration;
		rawdir = other.rawdir;
		lowMemoryPriority = other.lowMemoryPriority;
		innerParallelization = other.innerParallelization;
		// clusterDataFile = other.clusterDataFile;
		// clusterDataIsHard = other.clusterDataIsHard;
		return *this;
	}
};


template<typename Infomap>
class InfomapConfig : public Config {
public:
	InfomapConfig() {}
	InfomapConfig(const Config& conf) :
		Config(conf),
		m_rand(infomath::RandGen(conf.seedToRandomNumberGenerator))
	{
		Log::precision(conf.verboseNumberPrecision);
	}
	virtual ~InfomapConfig() {}

private:
	Infomap& get() {
		return static_cast<Infomap&>(*this);
	}
protected:
	infomath::RandGen m_rand = infomath::RandGen(123);

public:

	Infomap& setConfig(const Config& conf) {
		*this = conf;
		m_rand.seed(conf.seedToRandomNumberGenerator);
		Log::precision(conf.verboseNumberPrecision);
		return get();
	}

	Infomap& setNonMainConfig(const Config& conf) {
		cloneAsNonMain(conf);
		return get();
	}

	Infomap& setNumTrials(unsigned int N) {
		numTrials = N;
		return get();
	}

	Infomap& setTwoLevel(bool value) {
		twoLevel = value;
		return get();
	}

	Infomap& setFastHierarchicalSolution(unsigned int level) {
		fastHierarchicalSolution = level;
		return get();
	}

	Infomap& setOnlySuperModules(bool value) {
		onlySuperModules = value;
		return get();
	}

	Infomap& setNoCoarseTune(bool value) {
		noCoarseTune = value;
		return get();
	}

	Infomap& setDirected(bool directed) {
		directedEdges = directed;
		return get();
	}

	Infomap& setMarkovTime(double codeRate) {
		markovTime = codeRate;
		return get();
	}
};

}

#endif /* MODULES_CLUSTERING_CLUSTERING_INFOMAPCONFIG_H_ */
