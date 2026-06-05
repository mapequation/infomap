/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "InfomapBase.h"
#include "InfomapConfig.h"
#include "InfoNode.h"
#include "FlowData.h"
#include "BiasedMapEquation.h"
#include "MemMapEquation.h"
#include "MetaMapEquation.h"
#if INFOMAP_FEATURE_REGULARIZED_MULTILAYER
#include "RegularizedMultilayerMapEquation.h"
#endif
#include "InfomapOptimizer.h"
#include "../io/RunReport.h"
#include "../io/RunMetadata.h"
#include "../io/SafeFile.h"
#include "../io/OutputPlan.h"
#include "../utils/FileURI.h"
#include "../utils/FlowCalculator.h"
#include "../utils/MemoryUsage.h"
#include "../utils/PrettyOutput.h"
#include "../utils/ThreadConfig.h"
#include "../utils/TimingRegistry.h"
#include "../utils/convert.h"

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <unordered_map>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <exception>
#include <mutex>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace infomap {

namespace {

  std::string compressionSummary(const std::vector<std::string>& compression)
  {
    if (compression.empty())
      return "0%";

    return io::stringify(compression, ", ");
  }

  void printPrettyStart(const Config& config, const Date& startDate, unsigned int numInitialModuleIds)
  {
    PrettyOutput pretty(config.prettyOutput);
    Log::pretty() << pretty.bold() << "Infomap v" << INFOMAP_VERSION << pretty.reset() << "\n";
    Log::pretty() << pretty.dim() << pretty.branch() << " started " << startDate << pretty.reset() << "\n";
    Log::pretty() << pretty.bullet() << " Input      " << config.networkFile << "\n";
    Log::pretty() << pretty.bullet() << " Output     " << (config.noFileOutput ? "disabled" : config.outDirectory) << "\n";
    if (!config.parsedOptions.empty()) {
      Log::pretty() << pretty.bullet() << " Options    ";
      for (unsigned int i = 0; i < config.parsedOptions.size(); ++i) {
        if (i > 0)
          Log::pretty() << ", ";
        Log::pretty() << config.parsedOptions[i];
      }
      Log::pretty() << "\n";
    }
    if (numInitialModuleIds > 0) {
      Log::pretty() << pretty.bullet() << " Initial    " << numInitialModuleIds << " module ids\n";
    }
    Log::pretty() << "\n";
  }

  void printPrettyEnd(const Date& endDate, const Stopwatch& elapsedTime)
  {
    PrettyOutput pretty(true);
    Log::pretty() << "\n"
                  << pretty.dim() << "Finished " << endDate << " in " << elapsedTime << pretty.reset() << "\n";
  }

  void printPrettyTrialTable(const std::vector<double>& codelengths, const std::vector<unsigned int>& numTopModules)
  {
    PrettyOutput pretty(true);
    double bestCodelengthSoFar = std::numeric_limits<double>::max();
    Log::pretty() << "  " << pretty.dim() << "Trial  Codelength     Top modules  Best" << pretty.reset() << "\n";
    for (unsigned int i = 0; i < codelengths.size(); ++i) {
      bool isBest = codelengths[i] < bestCodelengthSoFar;
      if (isBest) {
        bestCodelengthSoFar = codelengths[i];
      }
      bool isEqual = std::abs(codelengths[i] - bestCodelengthSoFar) < 1e-10;
      Log::pretty() << "  " << std::setw(5) << (i + 1)
                    << "  " << std::setw(12) << io::toPrecision(codelengths[i])
                    << "  " << std::setw(11) << numTopModules[i]
                    << "  " << (isBest ? pretty.green() : isEqual ? pretty.yellow()
                                                                  : "")
                    << (isBest ? "best" : isEqual ? "tie"
                                                  : "")
                    << pretty.reset() << "\n";
    }
  }

#ifdef _OPENMP
  class ScopedOpenMpMaxActiveLevels {
  public:
    explicit ScopedOpenMpMaxActiveLevels(int value)
        : m_previous(omp_get_max_active_levels())
    {
      omp_set_max_active_levels(value);
    }

    ~ScopedOpenMpMaxActiveLevels()
    {
      omp_set_max_active_levels(m_previous);
    }

  private:
    int m_previous;
  };
#endif

} // namespace

class InfomapBase::RunSession {
public:
  struct Result {
    std::ostringstream bestSolutionStatistics;
    unsigned int bestNumLevels = 0;
    double bestHierarchicalCodelength = std::numeric_limits<double>::max();
    NodePaths bestTree;
    unsigned int bestTrialIndex = 0;
    unsigned int threadsUsed = 1;
    bool bestTreeNeedsRestore = false;
  };

  RunSession(InfomapBase& infomap, Network& network, TimingRegistry& timing) : m_infomap(infomap), m_network(network), m_timing(timing) {}

  Result run()
  {
    {
      // Resolve the thread budget once for the whole run. RunSession is only
      // constructed for the main Infomap (run(Network&) requires isMainInfomap),
      // so this fires exactly once, before any OpenMP region. Setting the
      // process-global max thread count lets all parallel regions (recursive
      // partition, parallel trials, inner parallelization) inherit the budget
      // via their existing omp_get_max_threads() calls.
      ThreadSources threadSources = readThreadSourcesFromEnv();
      threadSources.explicitThreads = m_infomap.numThreads; // 0 = auto
      m_threadBudget = resolveThreadBudget(threadSources);
      m_cpusetCount = threadSources.cpusetCount;
#ifdef _OPENMP
      // Clamp to INT_MAX before the signed cast: the budget is unsigned, and a value
      // above INT_MAX would make omp_set_num_threads see a negative thread count.
      const unsigned int ompThreads = std::min(m_threadBudget.threads,
          static_cast<unsigned int>(std::numeric_limits<int>::max()));
      omp_set_num_threads(static_cast<int>(ompThreads));
#endif
    }
    preflightOutputTargets(m_infomap);
    validateNetwork();
    {
      auto timer = m_timing.scope("pre_run_output_s");
      writeOutputArtifacts(m_infomap, m_network, OutputPhase::BeforeFlow);
    }
    {
      auto timer = m_timing.scope("configure_network_s");
      configureNetworkMode();
    }
    calculateFlowAndInitNetwork();
    {
      auto timer = m_timing.scope("post_flow_output_s");
      writeOutputArtifacts(m_infomap, m_network, OutputPhase::AfterFlow);
    }
    m_reportNetwork.nodes = m_network.numNodes();
    m_reportNetwork.links = m_network.numLinks();
    m_reportNetwork.directed = !m_infomap.isUndirectedFlow();
    m_runParallelTrials = selectParallelTrialMode();
    m_threadsUsed = m_runParallelTrials ? parallelTrialWorkers() : 1;
    releaseInputLinksIfCli();
    logRunPartitionStart();
    Result result;
    {
      auto timer = m_timing.scope("trial_optimize_s");
      result = runTrials();
    }
    result.threadsUsed = m_threadsUsed;
    return result;
  }

  void printSummary(const Result& result)
  {
    if (!m_infomap.isMainInfomap()) {
      return;
    }

    if (m_numTrials > 1) {
      restoreBestResult(result);
    }

    auto summaryTimer = m_timing.scope("summary_s");

    if (m_infomap.prettyOutput)
      PrettyOutput(true).section(io::Str() << "Summary after " << m_numTrials << (m_numTrials > 1 ? " trials" : " trial"));
    Log() << "\n\n";
    Log() << "================================================\n";
    Log() << "Summary after " << m_numTrials << (m_numTrials > 1 ? " trials\n" : " trial\n");
    Log() << "================================================\n";
    std::string codelengthRange;
    std::string topModulesRange;
    if (m_numTrials > 1) {
      Log() << std::fixed << std::setprecision(9);
      if (m_infomap.prettyOutput)
        Log::pretty() << std::fixed << std::setprecision(9);
      double averageCodelength = 0.0;
      double minCodelength = m_infomap.m_codelengths[0];
      double maxCodelength = m_infomap.m_codelengths[0];
      double averageNumTopModules = 0.0;
      auto minNumTopModules = m_infomap.m_numTopModules[0];
      auto maxNumTopModules = m_infomap.m_numTopModules[0];
      double bestCodelengthSoFar = std::numeric_limits<double>::max();
      if (m_infomap.prettyOutput)
        printPrettyTrialTable(m_infomap.m_codelengths, m_infomap.m_numTopModules);
      Log() << "Trial    Codelength    NumTopModules    Best\n";
      for (unsigned int i = 0; i < m_numTrials; ++i) {
        bool isBest = m_infomap.m_codelengths[i] < bestCodelengthSoFar;
        if (isBest) {
          bestCodelengthSoFar = m_infomap.m_codelengths[i];
        }
        bool isEqual = std::abs(m_infomap.m_codelengths[i] - bestCodelengthSoFar) < 1e-10;
        Log() << std::setw(5) << (i + 1) << std::setw(14) << io::toPrecision(m_infomap.m_codelengths[i]) << std::setw(17) << m_infomap.m_numTopModules[i] << std::setw(8) << (isBest ? '*' : isEqual ? '='
                                                                                                                                                                                                     : ' ')
              << "\n";
        averageCodelength += m_infomap.m_codelengths[i];
        minCodelength = std::min(minCodelength, m_infomap.m_codelengths[i]);
        maxCodelength = std::max(maxCodelength, m_infomap.m_codelengths[i]);
        averageNumTopModules += m_infomap.m_numTopModules[i];
        minNumTopModules = std::min(minNumTopModules, m_infomap.m_numTopModules[i]);
        maxNumTopModules = std::max(maxNumTopModules, m_infomap.m_numTopModules[i]);
      }
      averageCodelength /= m_numTrials;
      averageNumTopModules /= m_numTrials;
      Log() << "\n";
      Log() << "[min, average, max] codelength:      [" << minCodelength << ", " << averageCodelength << ", " << maxCodelength << "]\n";
      Log() << "[min, average, max] num top modules: [" << minNumTopModules << ", " << io::toPrecision(averageNumTopModules, 1, true) << ", " << maxNumTopModules << "]\n\n";
      codelengthRange = io::Str() << minCodelength << " / " << averageCodelength << " / " << maxCodelength;
      topModulesRange = io::Str() << minNumTopModules << " / " << io::toPrecision(averageNumTopModules, 1, true) << " / " << maxNumTopModules;
    }
    if (m_infomap.prettyOutput) {
      PrettyOutput pretty(true);
      if (m_numTrials > 1) {
        pretty.metric("Codelength min/avg/max", codelengthRange);
        pretty.metric("Top modules min/avg/max", topModulesRange);
        Log::pretty() << "\n";
      }
      pretty.metric("Nodes", io::Str() << m_infomap.numLeafNodes());
      pretty.metric("Links", io::Str() << m_network.numLinks());
      pretty.metric("Average degree", io::toPrecision(m_network.numLinks() * 2.0 / m_infomap.numLeafNodes(), 1, true));
      pretty.metric("Top modules", io::Str() << m_infomap.numTopModules() << " (" << m_infomap.numNonTrivialTopModules() << " non-trivial)");
      pretty.metric("Levels", io::Str() << result.bestNumLevels);
      pretty.metric("One-level codelength", io::toPrecision(m_infomap.getOneLevelCodelength()));
      pretty.metric("Best codelength", io::toPrecision(result.bestHierarchicalCodelength));
      pretty.metric("Relative savings", io::Str() << io::toPrecision(m_infomap.getRelativeCodelengthSavings() * 100, 2, true) << "%");
      Log::pretty() << "\n";
    }
    Log() << "Number nodes:                      " << m_infomap.numLeafNodes() << "\n";
    Log() << "Number links:                      " << m_network.numLinks() << "\n";
    Log() << "Average degree:                    " << io::toPrecision(m_network.numLinks() * 2.0 / m_infomap.numLeafNodes(), 1, true) << "\n";
    Log() << "Number of top modules:             " << m_infomap.numTopModules() << "\n";
    Log() << "Number of non-trivial top modules: " << m_infomap.numNonTrivialTopModules() << "\n";
    Log() << "Number of levels:                  " << result.bestNumLevels << "\n";
    Log() << "One-level codelength:              " << io::toPrecision(m_infomap.getOneLevelCodelength()) << "\n";
    Log() << "Codelength:                        " << io::toPrecision(result.bestHierarchicalCodelength) << "\n";
    Log() << "Relative codelength savings:       " << io::toPrecision(m_infomap.getRelativeCodelengthSavings() * 100, 2, true) << "%\n";
    Log() << "\n";
    Log() << result.bestSolutionStatistics.str() << '\n';
    if (m_infomap.prettyOutput)
      Log::pretty() << result.bestSolutionStatistics.str() << '\n';
  }

private:
  Result runTrials()
  {
    Result result;
    result.bestTree = NodePaths(m_infomap.numLeafNodes());
    m_infomap.m_codelengths.clear();
    m_infomap.m_numTopModules.clear();
    m_timing.resetTrials(m_numTrials);

    if (m_runParallelTrials) {
      return runTrialsInParallel();
    }

    for (unsigned int i = 0; i < m_numTrials; ++i) {
      runTrial(i, result);
    }

    return result;
  }

  Result runTrialsInParallel()
  {
    Result result;
    result.bestTree = NodePaths(m_infomap.numLeafNodes());
    result.bestTreeNeedsRestore = true;
    const unsigned int numWorkers = parallelTrialWorkers();

    std::vector<double> codelengths(m_numTrials, std::numeric_limits<double>::max());
    std::vector<unsigned int> numTopModules(m_numTrials, 0);
    bool hasError = false;
    unsigned int firstErrorTrial = 0;
    std::string firstError;
    std::mutex bestResultMutex;
    std::mutex errorMutex;
    std::mutex outputMutex;

#ifdef _OPENMP
    ScopedOpenMpMaxActiveLevels scopedMaxActiveLevels(1);
#pragma omp parallel for schedule(dynamic) num_threads(numWorkers)
#endif
    for (int workerIndex = 0; workerIndex < static_cast<int>(numWorkers); ++workerIndex) {
      auto workerConfig = m_infomap.getConfig();
      workerConfig.numTrials = 1;
      workerConfig.parallelTrials = false;
      workerConfig.innerParallelization = false;
      workerConfig.seedToRandomNumberGenerator = m_infomap.seedToRandomNumberGenerator + static_cast<unsigned int>(workerIndex);

      InfomapBase worker(workerConfig);
      worker.m_initialPartition = m_infomap.m_initialPartition;

      for (unsigned int trialIndex = static_cast<unsigned int>(workerIndex); trialIndex < m_numTrials; trialIndex += numWorkers) {
        try {
          Log::ScopedMute muteWorkerLogs;
          const auto globalIndex = m_infomap.trialOffset + trialIndex;
          const auto trialSeed = m_infomap.seedToRandomNumberGenerator + globalIndex;
          worker.seedToRandomNumberGenerator = trialSeed;
          worker.reseed(trialSeed);
          int threadNumber = 0;
#ifdef _OPENMP
          threadNumber = omp_get_thread_num();
#endif
          Stopwatch trialTimer(true);

          NodePaths trialTree;
          unsigned int trialNumLevels = 0;
          std::ostringstream trialStatistics;
          worker.initNetwork(m_network);
          initTrialPartition(worker);
          executeTrial(worker);
          worker.root().sortChildrenOnFlow();

          trialNumLevels = printPerLevelCodelength(worker.root(), trialStatistics, worker.prettyOutput);
          trialTree.reserve(worker.numLeafNodes());
          for (auto it(worker.iterLeafNodes()); !it.isEnd(); ++it) {
            trialTree.emplace_back(it->stateId, it.path());
          }

          const auto trialCodelength = worker.m_hierarchicalCodelength;
          const auto trialTopModules = worker.numTopModules();
          const auto trialTime = trialTimer.getElapsedTimeInSec();
          codelengths[trialIndex] = trialCodelength;
          numTopModules[trialIndex] = trialTopModules;
          m_timing.recordTrial(trialIndex, threadNumber, trialSeed, trialTime, trialCodelength, trialTopModules, trialNumLevels);

          if (worker.printAllTrials && m_numTrials > 1) {
            std::lock_guard<std::mutex> lock(outputMutex);
            auto outputTimer = m_timing.scope("output_s");
            worker.writeResult(static_cast<int>(globalIndex + 1));
          }

          std::lock_guard<std::mutex> lock(bestResultMutex);
          const auto bestIndexMissing = result.bestTrialIndex >= m_numTrials;
          const auto isBetter = trialCodelength < result.bestHierarchicalCodelength - 1e-10;
          const auto isEarlierTie = std::abs(trialCodelength - result.bestHierarchicalCodelength) < 1e-10 && trialIndex < result.bestTrialIndex;
          if (bestIndexMissing || isBetter || isEarlierTie) {
            result.bestSolutionStatistics.clear();
            result.bestSolutionStatistics.str(trialStatistics.str());
            result.bestNumLevels = trialNumLevels;
            result.bestHierarchicalCodelength = trialCodelength;
            result.bestTrialIndex = trialIndex;
            result.bestTree = std::move(trialTree);
          }
        } catch (const std::exception& e) {
          std::lock_guard<std::mutex> lock(errorMutex);
          if (!hasError || trialIndex < firstErrorTrial) {
            hasError = true;
            firstErrorTrial = trialIndex;
            firstError = e.what();
          }
        } catch (...) {
          std::lock_guard<std::mutex> lock(errorMutex);
          if (!hasError || trialIndex < firstErrorTrial) {
            hasError = true;
            firstErrorTrial = trialIndex;
            firstError = "unknown error";
          }
        }
      }
    }

    if (hasError) {
      throw std::runtime_error(io::Str() << "Parallel trial " << (firstErrorTrial + 1) << "/" << m_numTrials << " failed: " << firstError);
    }

    m_infomap.m_codelengths = std::move(codelengths);
    m_infomap.m_numTopModules = std::move(numTopModules);
    return result;
  }

  unsigned int parallelTrialWorkers() const
  {
#ifdef _OPENMP
    // Worker count is driven directly by the OpenMP thread count (OMP_NUM_THREADS),
    // giving HPC users explicit control with no hidden heuristic. Clamp to numTrials
    // so we never spawn idle workers. Peak memory scales with the worker count (each
    // worker holds its own leaf network) — reduce OMP_NUM_THREADS if memory-constrained.
    const unsigned int maxOpenMpThreads = static_cast<unsigned int>(std::max(1, omp_get_max_threads()));
    return std::max(1u, std::min(m_numTrials, maxOpenMpThreads));
#else
    return 1;
#endif
  }

  void restoreBestResult(const Result& result)
  {
    if (m_numTrials > 1 && (result.bestTreeNeedsRestore || result.bestTrialIndex < m_numTrials - 1)) {
      // Restore Infomap tree to best solution.
      {
        auto timer = m_timing.scope("best_restore_s");
        m_infomap.initTree(result.bestTree);
      }
      auto outputTimer = m_timing.scope("output_s");
      m_infomap.writeResult(); // Overwrite result to get total elapsed time in output file header.
    }
  }

  bool selectParallelTrialMode()
  {
    if (!m_infomap.parallelTrials) {
      return false;
    }

    if (m_numTrials < 2) {
      Log::important() << "  -> Warning: --parallel-trials requires --num-trials > 1; running trials serially.\n";
      return false;
    }

#ifndef _OPENMP
    Log::important() << "  -> Warning: --parallel-trials requires an OpenMP build; running trials serially.\n";
    return false;
#else
    if (m_infomap.innerParallelization) {
      Log::important() << "  -> Warning: --parallel-trials ignores --inner-parallelization inside trial workers.\n";
    }
    const unsigned int workers = parallelTrialWorkers();
    if (m_infomap.prettyOutput) {
      PrettyOutput pretty(true);
      Log::pretty() << "\n"
                    << pretty.dim() << "  Parallel trials: " << workers << " workers from "
                    << omp_get_max_threads() << " OpenMP threads (memory scales with workers; inner parallelization off)"
                    << pretty.reset() << "\n";
    } else {
      Log() << "  -> Running " << m_numTrials << " trials in parallel with " << workers << " workers from " << omp_get_max_threads() << " OpenMP threads (peak memory scales with workers; inner parallelization off).\n";
    }
    return true;
#endif
  }

  void validateNetwork()
  {
    if (m_network.numNodes() == 0) {
      m_network.postProcessInputData();
      if (m_network.numNodes() == 0) {
        throw std::domain_error("Network is empty");
      }
    }
  }

  void configureNetworkMode()
  {
    if (m_network.haveMemoryInput()) {
      Log() << "  -> Found higher order network input, using the Map Equation for higher order network flows\n";
      m_infomap.setStateInput();
      m_infomap.setStateOutput();

      if (m_network.isMultilayerNetwork() && !m_infomap.isMultilayerNetwork()) {
        m_infomap.setMultilayerInput();
      }
    } else {
      if (m_infomap.haveMemory() || m_network.higherOrderInputMethodCalled()) {
        Log::important() << "  -> Warning: Higher order network specified but no higher order input found.\n";
        // Use state output anyway for consistency even in the special case when input is first order
        m_infomap.setStateOutput();
      }
      Log() << "  -> Ordinary network input, using the Map Equation for first order network flows\n";
    }

    if (m_network.haveDirectedInput() && m_infomap.isUndirectedFlow()) {
      Log() << "  -> Notice: Directed input found, changing flow model from '" << m_infomap.flowModel << "' to '" << FlowModel(FlowModel::directed) << "'\n";
      m_infomap.flowModel = FlowModel::directed;
    }
    m_network.setConfig(m_infomap);
  }

  void calculateFlowAndInitNetwork()
  {
    {
      auto timer = m_timing.scope("flow_calculation_s");
      calculateFlow(m_network, m_infomap);
    }

    if (m_network.isBipartite()) {
      m_infomap.bipartite = true;
    }

    {
      auto timer = m_timing.scope("init_network_s");
      m_infomap.initNetwork(m_network);
    }

    if (m_infomap.numLeafNodes() == 0)
      throw std::domain_error("No nodes to partition");
  }

  void releaseInputLinksIfCli()
  {
    // If used as a library, we may want to reuse the network instance, else clear to use less memory
    // TODO: May have to use some meta data for output?
    if (m_infomap.isCLI && !m_runParallelTrials) {
      m_network.clearLinks();
    }
  }

  void logRunPartitionStart()
  {
    if (m_infomap.haveMemory())
      Log(2) << "Run Infomap with memory...\n";
    else
      Log(2) << "Run Infomap...\n";
#ifdef _OPENMP
    {
      // Prefer the cpuset size (the actual allocation under Linux/SLURM) over
      // omp_get_num_procs(), which ignores affinity; fall back when unknown.
      const unsigned int cpus = m_cpusetCount > 0 ? m_cpusetCount : static_cast<unsigned int>(std::max(1, omp_get_num_procs()));
      const char* source = threadSourceName(m_threadBudget.source);
      const char* innerState = m_infomap.innerParallelization ? "enabled" : "disabled";
      if (m_infomap.prettyOutput) {
        PrettyOutput pretty(true);
        Log::pretty() << pretty.dim() << "  Runtime: " << cpus << " CPUs available, thread budget "
                      << m_threadBudget.threads << " (" << source << "), inner parallelization "
                      << innerState << pretty.reset() << "\n";
      } else {
        Log() << "  -> Runtime: " << cpus << " CPUs available, thread budget "
              << m_threadBudget.threads << " (" << source << "), inner parallelization "
              << innerState << ".\n";
      }

      // Oversubscription warnings (only meaningful when cpuset is known, i.e. Linux).
      // Warn whenever the *resolved* budget exceeds the cpuset, regardless of source
      // (explicit --num-threads, INFOMAP_NUM_THREADS, SLURM_CPUS_PER_TASK, OMP_NUM_THREADS).
      if (m_cpusetCount > 0 && m_threadBudget.threads > m_cpusetCount) {
        Log::important() << "  -> Warning: thread budget " << m_threadBudget.threads
                         << " (" << source << ") exceeds the available cpuset of " << m_cpusetCount
                         << " CPUs; this may oversubscribe the node.\n";
      }
      if (m_cpusetCount > 0) {
        const unsigned int innerThreads = (m_infomap.innerParallelization && !m_runParallelTrials) ? m_threadBudget.threads : 1;
        const unsigned int demand = m_threadsUsed * innerThreads;
        if (demand > m_cpusetCount) {
          Log::important() << "  -> Warning: " << m_threadsUsed << " trial workers x " << innerThreads
                           << " inner threads = " << demand << " exceeds the available cpuset of "
                           << m_cpusetCount << " CPUs.\n";
        }
      }
    }
#endif
  }

  void runTrial(unsigned int trialIndex, Result& result)
  {
    const bool shardingMode = m_infomap.trialOffset > 0 || !m_infomap.trialResultsPath.empty();
    if (shardingMode) {
      const unsigned int globalSeed = static_cast<unsigned int>(m_infomap.seedToRandomNumberGenerator + (m_infomap.trialOffset + trialIndex));
      m_infomap.reseed(globalSeed);
    }
    m_infomap.removeModules();
    auto startDate = Date();
    Stopwatch timer(true);

    logTrialStart(trialIndex, startDate);
    initTrialPartition(m_infomap);
    executeTrial(m_infomap);
    finishTrial(trialIndex, timer, result);
  }

  void logTrialStart(unsigned int trialIndex, const Date& startDate)
  {
    if (!m_infomap.isMainInfomap()) {
      return;
    }

    if (m_infomap.prettyOutput) {
      PrettyOutput pretty(true);
      Log::pretty() << "\n"
                    << pretty.cyan() << "Trial " << (trialIndex + 1) << "/" << m_numTrials << pretty.reset()
                    << pretty.dim() << " started " << startDate << pretty.reset() << "\n";
      return;
    }

    Log() << "\n";
    Log() << "================================================\n";
    Log() << "Trial " << (trialIndex + 1) << "/" << m_numTrials << " starting at " << startDate << "\n";
    Log() << "================================================\n";
  }

  void initTrialPartition(InfomapBase& infomap)
  {
    if (!infomap.isMainInfomap()) {
      return;
    }

    if (!infomap.clusterDataFile.empty())
      infomap.initPartition(infomap.clusterDataFile, infomap.clusterDataIsHard, &m_network);
    else if (!infomap.m_initialPartition.empty())
      infomap.initPartition(infomap.m_initialPartition, infomap.clusterDataIsHard);
  }

  void executeTrial(InfomapBase& infomap)
  {
    if (!infomap.noInfomap)
      infomap.runPartition();
    else
      infomap.m_hierarchicalCodelength = infomap.calcCodelengthOnTree(infomap.root(), true);

    if (infomap.haveHardPartition())
      infomap.restoreHardPartition();
  }

  void finishTrial(unsigned int trialIndex, Stopwatch& timer, Result& result)
  {
    if (!m_infomap.isMainInfomap()) {
      return;
    }

    if (m_infomap.prettyOutput) {
      PrettyOutput pretty(true);
      Log::pretty() << "\n"
                    << pretty.green() << "Done" << pretty.reset()
                    << " trial " << (trialIndex + 1) << "/" << m_numTrials
                    << " in " << timer.getElapsedTimeInSec() << "s"
                    << " | codelength " << m_infomap.m_hierarchicalCodelength << "\n";
    } else {
      Log() << "\n=> Trial " << (trialIndex + 1) << "/" << m_numTrials << " finished in " << timer.getElapsedTimeInSec() << "s with codelength " << m_infomap.m_hierarchicalCodelength << "\n";
    }
    m_infomap.m_codelengths.push_back(m_infomap.m_hierarchicalCodelength);
    m_infomap.m_numTopModules.push_back(m_infomap.numTopModules());
    const unsigned long globalSeed = m_infomap.seedToRandomNumberGenerator + (m_infomap.trialOffset + trialIndex);
    m_timing.recordTrial(trialIndex, 0, globalSeed, timer.getElapsedTimeInSec(), m_infomap.m_hierarchicalCodelength, m_infomap.numTopModules(), m_infomap.numLevels());

    if (m_infomap.printAllTrials && m_numTrials > 1) {
      auto outputTimer = m_timing.scope("output_s");
      m_infomap.writeResult(static_cast<int>(m_infomap.trialOffset + trialIndex + 1));
    }

    if (m_infomap.m_hierarchicalCodelength < result.bestHierarchicalCodelength - 1e-10) {
      updateBestResult(trialIndex, result);
    }
  }

  void updateBestResult(unsigned int trialIndex, Result& result)
  {
    result.bestSolutionStatistics.clear();
    result.bestSolutionStatistics.str("");
    result.bestNumLevels = printPerLevelCodelength(m_infomap.root(), result.bestSolutionStatistics, m_infomap.prettyOutput);
    result.bestHierarchicalCodelength = m_infomap.m_hierarchicalCodelength;
    result.bestTrialIndex = trialIndex;
    m_infomap.root().sortChildrenOnFlow();
    auto outputTimer = m_timing.scope("output_s");
    m_infomap.writeResult();
    if (m_numTrials > 1) {
      result.bestTree.clear();
      for (auto it(m_infomap.iterLeafNodes()); !it.isEnd(); ++it) {
        result.bestTree.emplace_back(it->stateId, it.path());
      }
    }
  }

public:
  void writeRunReports(const Result& result)
  {
    if (!m_infomap.summaryJsonPath.empty()) {
      RunSummaryReport report;
      report.version = INFOMAP_VERSION;
      report.codelength = result.bestHierarchicalCodelength;
      report.topModules = m_infomap.numTopModules();
      report.levels = result.bestNumLevels;
      report.trials = m_numTrials;
      report.bestTrial = result.bestTrialIndex + 1;
      report.trialCodelengths = m_infomap.m_codelengths;
      report.trialTopModules = m_infomap.m_numTopModules;
      writeJsonReport(m_infomap.summaryJsonPath, runSummaryReportJson(report), m_infomap.overwriteOutput());
    }

    if (!m_infomap.timingJsonPath.empty()) {
      RunTimingReport report;
      report.version = INFOMAP_VERSION;
#ifdef _OPENMP
      report.openmp = true;
      report.threadsRequested = m_threadBudget.threads;
      report.threadSource = threadSourceName(m_threadBudget.source);
#else
      report.openmp = false;
      report.threadsRequested = 1;
      report.threadSource = "serial";
#endif
      report.threadsUsed = result.threadsUsed;
      report.network = m_reportNetwork;
      report.timing = m_timing.phases();
      report.trials = m_timing.trials();
      report.includeMemory = m_infomap.memoryReport;
      if (report.includeMemory) {
        report.memory = currentMemoryReport(report.network.nodes, report.network.links);
      }
      writeJsonReport(m_infomap.timingJsonPath, runTimingReportJson(report), m_infomap.overwriteOutput());
    }

    if (!m_infomap.runManifestPath.empty()) {
      writeJsonReport(m_infomap.runManifestPath, runManifestJson(m_infomap), m_infomap.overwriteOutput());
    }
  }

private:
  InfomapBase& m_infomap;
  Network& m_network;
  TimingRegistry& m_timing;
  RunReportNetwork m_reportNetwork;
  const unsigned int m_numTrials = m_infomap.numTrials;
  bool m_runParallelTrials = false;
  unsigned int m_threadsUsed = 1;
  ThreadBudget m_threadBudget;
  unsigned int m_cpusetCount = 0;
};

std::map<unsigned int, std::vector<unsigned int>> InfomapBase::getMultilevelModules(bool states)
{
  if (haveMemory() && !states) {
    throw std::runtime_error("Cannot get multilevel modules on higher-order network without states.");
  }
  unsigned int maxDepth = maxTreeDepth();
  unsigned int numModuleLevels = maxDepth - 1;
  std::map<unsigned int, std::vector<unsigned int>> modules;
  if (maxDepth < 2) return modules;
  for (unsigned int level = 1; level <= numModuleLevels; ++level) {
    for (auto it(iterLeafNodes(static_cast<int>(level))); !it.isEnd(); ++it) {
      auto& node = *it;
      auto nodeId = states ? node.stateId : node.physicalId;
      modules[nodeId].push_back(it.moduleId());
    }
  }
  return modules;
}

// ===================================================
// IO
// ===================================================

std::ostream& operator<<(std::ostream& out, const InfomapBase& infomap)
{
  return infomap.toString(out);
}

// ===================================================
// Getters
// ===================================================

unsigned int InfomapBase::numLevels() const
{
  // TODO: Make sure this is not called unless tree is guaranteed to have even depth!
  unsigned int depth = 0;
  InfoNode* n = m_root.firstChild;
  while (n != nullptr) {
    ++depth;
    n = n->firstChild;
  }
  return depth;
}

unsigned int InfomapBase::maxTreeDepth() const
{
  unsigned int maxDepth = 0;
  for (auto it = root().begin_tree(); !it.isEnd(); ++it) {
    const InfoNode& node = *it;
    if (node.isLeaf()) {
      if (it.depth() > maxDepth) {
        maxDepth = it.depth();
      }
    }
  }
  return maxDepth;
}

// ===================================================
// Run
// ===================================================

void InfomapBase::run(const std::string& parameters)
{
  if (!isMainInfomap()) {
    runPartition();
    return;
  }

  m_elapsedTime = Stopwatch(true);
  m_startDate = Date();

  std::string currentParameters = io::Str() << m_initialParameters << (parameters.empty() ? "" : " ") << parameters;
  if (currentParameters != m_currentParameters) {
    m_currentParameters = currentParameters;
    setConfig(Config(m_currentParameters, isCLI));
    m_network.setConfig(*this);
  }

  Log::init(verbosity, silent, verboseNumberPrecision, prettyOutput);

  if (prettyOutput)
    printPrettyStart(*this, m_startDate, m_initialPartition.size());
  Log() << "=======================================================\n";
  Log() << "  Infomap v" << INFOMAP_VERSION << " starts at " << m_startDate << "\n";
  Log() << "  -> Input network: " << networkFile << "\n";
  if (noFileOutput)
    Log() << "  -> No file output!\n";
  else
    Log() << "  -> Output path:   " << outDirectory << "\n";
  if (!parsedOptions.empty()) {
    for (unsigned int i = 0; i < parsedOptions.size(); ++i)
      Log() << (i == 0 ? "  -> Configuration: " : "                    ") << parsedOptions[i] << "\n";
  }
  if (!m_initialPartition.empty()) {
    Log() << "  -> " << m_initialPartition.size() << " initial module ids provided\n";
  }
  Log() << "=======================================================\n";
#ifdef _OPENMP
#pragma omp parallel
#pragma omp master
  {
    if (prettyOutput) {
      PrettyOutput pretty(true);
      Log::pretty() << pretty.bullet() << " Runtime    OpenMP " << _OPENMP << " with " << omp_get_num_threads() << " threads\n\n";
    }
    Log() << "  OpenMP " << _OPENMP << " detected with " << omp_get_num_threads() << " threads...\n";
  }
#endif

  m_network.postProcessInputData();
  if (m_network.numNodes() == 0) {
    m_network.readInputData(networkFile);
  }

  if (!metaDataFile.empty()) {
    initMetaData(metaDataFile);
  }

  run(m_network);

  if (prettyOutput)
    printPrettyEnd(m_endDate, m_elapsedTime);
  Log() << "===================================================\n";
  Log() << "  Infomap ends at " << m_endDate << "\n";
  Log() << "  (Elapsed time: " << m_elapsedTime << ")\n";
  Log() << "===================================================\n";
}

void InfomapBase::run(Network& network)
{
  if (!isMainInfomap())
    throw std::logic_error("Can't run a non-main Infomap with an input network");

  TimingRegistry timing;
  Stopwatch totalTimer(true);
  RunSession runSession(*this, network, timing);
  auto runResult = runSession.run();
  m_elapsedTime.stop();
  m_endDate = Date();
  runSession.printSummary(runResult);
  totalTimer.stop();
  timing.setPhase("total_s", totalTimer.getElapsedTimeInSec());
  runSession.writeRunReports(runResult);
}

// ===================================================
// Run: Init: *
// ===================================================

InfomapBase& InfomapBase::initMetaData(const std::string& metaDataFile)
{
  m_network.readMetaData(metaDataFile);
  return *this;
}

InfomapBase& InfomapBase::initNetwork(Network& network)
{
  if (network.numNodes() == 0)
    throw std::domain_error("No nodes in network");
  // A fresh network init invalidates any previous hard partition restore buffer.
  m_originalLeafNodes.clear();
  if (m_root.firstChild != nullptr || m_root.collapsedFirstChild != nullptr) {
    m_root.deleteChildren();
    m_leafNodes.clear();
  }
  generateSubNetwork(network);

  initOptimizer();

  // Per-instance network properties for entropy bias correction must be set after the
  // optimizer/objective exists and before init() reads them via calcCodelength().
  if (entropyBiasCorrection)
    m_optimizer->setNetworkProperties(network);

  init();
  return *this;
}

InfomapBase& InfomapBase::initNetwork(InfoNode& parent, bool asSuperNetwork)
{
  m_originalLeafNodes.clear();
  generateSubNetwork(parent);

  if (asSuperNetwork)
    transformNodeFlowToEnterFlow(m_root);

  init();
  return *this;
}

InfomapBase& InfomapBase::initPartition(const std::string& clusterDataFile, bool hard, const Network* network)
{
  ClusterMap clusterMap;
  if (network != nullptr && network->isMultilayerNetwork()) {
    const auto& map = network->layerNodeToStateId();
    clusterMap.readClusterData(clusterDataFile, false, &map);
  } else {
    clusterMap.readClusterData(clusterDataFile);
  }

  if (prettyOutput)
    PrettyOutput(true).status("Initial", io::Str() << "reading " << clusterDataFile);
  Log() << "Init partition from file '" << clusterDataFile << "'... ";

  const auto& ext = clusterMap.extension();

  if (ext == "tree" || ext == "ftree") {
    unsigned int numTreeNodesNotInNetwork = 0;
    const auto normalizedTree = normalizeTreePaths(clusterMap.treePaths(), numTreeNodesNotInNetwork);
    if (numTreeNodesNotInNetwork > 0) {
      Log(1) << "\n -> " << numTreeNodesNotInNetwork << " physical nodes in tree not found in network.";
    }
    initTree(normalizedTree);
  } else if (ext == "clu") {
    initPartition(clusterMap.clusterIds(), hard);
  }

  if (prettyOutput)
    PrettyOutput(true).status("Initial", io::Str() << "generated " << numLevels() << " levels, codelength " << io::toPrecision(m_hierarchicalCodelength));
  Log() << "done! Generated " << numLevels() << " levels with codelength " << getIndexCodelength() << " + " << (m_hierarchicalCodelength - getIndexCodelength()) << " = " << io::toPrecision(m_hierarchicalCodelength) << '\n';

  return *this;
}

NodePaths InfomapBase::normalizeTreePaths(const TreePaths& tree, unsigned int& numNodesNotInNetwork) const
{
  numNodesNotInNetwork = 0;
  NodePaths normalized;
  normalized.reserve(tree.size());

  // Fast path: every row is a state-level leaf — pass through.
  bool allState = std::all_of(tree.begin(), tree.end(),
      [](const auto& tp) { return tp.idType == TreeLeafIdType::state; });
  if (allState) {
    for (const auto& tp : tree)
      normalized.emplace_back(tp.nodeId, tp.path);
    return normalized;
  }

  // Build physical -> [state ids] index across the active leaf set.
  std::map<unsigned int, std::vector<unsigned int>> physicalToStateIds;
  for (const auto* leafNode : m_leafNodes) {
    physicalToStateIds[leafNode->physicalId].push_back(leafNode->stateId);
  }

  // Count how many tree rows reference each physical id; a physical id may
  // appear multiple times (one row per state) on higher-order outputs.
  std::map<unsigned int, unsigned int> physicalRowCount;
  for (const auto& tp : tree) {
    if (tp.idType == TreeLeafIdType::physical)
      ++physicalRowCount[tp.nodeId];
  }

  std::map<unsigned int, unsigned int> physicalRowsConsumed;
  std::map<unsigned int, unsigned int> physicalStatesConsumed;

  for (const auto& tp : tree) {
    if (tp.idType == TreeLeafIdType::state) {
      normalized.emplace_back(tp.nodeId, tp.path);
      continue;
    }

    const auto it = physicalToStateIds.find(tp.nodeId);
    if (it == physicalToStateIds.end()) {
      ++numNodesNotInNetwork;
      continue;
    }

    auto& rowsConsumed = physicalRowsConsumed[tp.nodeId];
    auto& statesConsumed = physicalStatesConsumed[tp.nodeId];
    const auto totalRows = physicalRowCount[tp.nodeId];
    const auto remainingRows = totalRows - rowsConsumed;
    const auto remainingStates = it->second.size() - statesConsumed;

    if (remainingStates == 0) {
      ++numNodesNotInNetwork;
      ++rowsConsumed;
      continue;
    }

    // If this is the last row for this physical id, claim every remaining state.
    // Otherwise consume one state per row, preserving 1:1 alignment when the
    // original output wrote one row per state.
    const auto statesToAssign = (remainingRows <= 1) ? remainingStates : 1u;
    for (unsigned int i = 0; i < statesToAssign; ++i) {
      normalized.emplace_back(it->second[statesConsumed + i], tp.path);
    }
    statesConsumed += statesToAssign;
    ++rowsConsumed;
  }

  return normalized;
}

InfomapBase& InfomapBase::initTree(const NodePaths& tree)
{
  Log(4) << "Init tree... " << std::setprecision(9);
  int maxDepth = 2;
  // If only two-level partition, we can directly use initPartition
  for (const auto& nodePath : tree) {
    if (nodePath.second.size() > 2) {
      maxDepth = std::max(maxDepth, static_cast<int>(nodePath.second.size()));
    }
  }
  if (maxDepth == 2) {
    std::map<unsigned int, unsigned int> clusterIds;
    for (const auto& nodePath : tree) {
      const auto nodeId = nodePath.first;
      const auto clusterId = nodePath.second[0]; // First level module
      clusterIds[nodeId] = clusterId;
    }
    return initPartition(clusterIds, false);
  }

  // Tear down the existing partition. Detach the leaves first so that the
  // module destruction below does not also destroy them, then collect every
  // released leaf for re-attachment.
  std::map<unsigned int, unsigned int> nodeIdToIndex;
  unsigned int leafIndex = 0;
  for (auto* leafNode : m_leafNodes) {
    if (leafNode->parent != nullptr)
      leafNode->parent->releaseChildren();
    leafNode->parent = nullptr;
    leafNode->next = nullptr;
    leafNode->previous = nullptr;
    nodeIdToIndex[leafNode->stateId] = leafIndex;
    ++leafIndex;
  }
  m_root.deleteChildren();

  // Build the module tree by walking each path top-down and creating only
  // missing prefixes — this guarantees that two leaves sharing a path prefix
  // share the same ancestor modules, which is what determines codelength.
  std::map<Path, InfoNode*> moduleByPathPrefix;

  auto numNodesFound = 0;
  auto numNodesNotInNetwork = 0;
  for (const auto& nodePath : tree) {
    const auto& path = nodePath.second;
    if (path.empty())
      continue;

    ++numNodesFound;
    InfoNode* leafNode = nullptr;
    const auto nodeIdIt = nodeIdToIndex.find(nodePath.first);
    if (nodeIdIt == nodeIdToIndex.end()) {
      ++numNodesNotInNetwork;
      continue;
    }
    leafNode = m_leafNodes[nodeIdIt->second];

    InfoNode* parent = &m_root;
    Path prefix;
    for (size_t i = 0; i + 1 < path.size(); ++i) {
      prefix.push_back(path[i]);
      auto inserted = moduleByPathPrefix.emplace(prefix, nullptr);
      if (inserted.second) {
        auto* module = new InfoNode();
        parent->addChild(module);
        inserted.first->second = module;
      }
      parent = inserted.first->second;
    }

    parent->addChild(leafNode);
    if (static_cast<int>(path.size()) > maxDepth)
      maxDepth = static_cast<int>(path.size());
  }

  if (numNodesNotInNetwork > 0) {
    Log(1) << "\n -> " << numNodesNotInNetwork << "/" << numNodesFound << " nodes in tree not found in network.";
  }

  auto numNodesAddedToNeighbouringModules = 0;
  auto numNodesWithoutClusterInfo = 0;

  // Set orphaned nodes to their own or neighbouring module
  for (auto& leafNode : m_leafNodes) {
    if (leafNode->parent != nullptr) {
      continue;
    }

    ++numNodesWithoutClusterInfo;
    if (assignToNeighbouringModule) {
      // Take first neighbour that has a module assigned
      for (auto& edge : leafNode->outEdges()) {
        if (edge->target->parent != nullptr) {
          edge->target->parent->addChild(leafNode);
          ++numNodesAddedToNeighbouringModules;
          break;
        }
      }
      // Check incoming links if still orphan
      if (leafNode->parent == nullptr) {
        for (auto& edge : leafNode->inEdges()) {
          if (edge->source->parent != nullptr) {
            edge->source->parent->addChild(leafNode);
            ++numNodesAddedToNeighbouringModules;
            break;
          }
        }
      }
      // Set to own module if no neighbour module available
      if (leafNode->parent == nullptr) {
        auto module = new InfoNode();
        root().addChild(module);
        module->addChild(leafNode);
      }
    } else {
      // Set to own module if no neighbour module available
      auto module = new InfoNode();
      root().addChild(module);
      module->addChild(leafNode);
    }
  }
  if (numNodesWithoutClusterInfo > 0) {
    if (assignToNeighbouringModule) {
      Log() << "\n -> " << numNodesWithoutClusterInfo << " nodes not found in tree, " << numNodesAddedToNeighbouringModules << " are put into neighbouring modules and " << (numNodesWithoutClusterInfo - numNodesAddedToNeighbouringModules) << " in separate.";
    } else {
      Log() << "\n -> " << numNodesWithoutClusterInfo << " nodes not found in tree are put into separate modules.";
    }
  }

  aggregateFlowValuesFromLeafToRoot();
  initTree();
  initNetwork();
  m_numNonTrivialTopModules = calculateNumNonTrivialTopModules();
  // Init partition to calculate indexCodelength and moduleCodelength
  if (haveModules())
    setActiveNetworkFromChildrenOfRoot();
  else
    setActiveNetworkFromLeafs();
  initPartition();

  m_hierarchicalCodelength = calcCodelengthOnTree(root(), true);
  if (prettyOutput)
    PrettyOutput(true).status("Initial", io::Str() << "codelength " << m_hierarchicalCodelength << ", " << maxDepth << " levels, " << numTopModules() << " top modules");
  Log() << "\n -> Initiated to codelength " << m_hierarchicalCodelength << " in " << maxDepth << " levels with " << numTopModules() << " top modules.\n";
  Log() << std::setprecision(6);
  return *this;
}

/**
 * clusterIds is a map from nodeId -> clusterId
 */
InfomapBase& InfomapBase::initPartition(const std::map<unsigned int, unsigned int>& clusterIds, bool hard)
{
  // Generate map from node id to index in leaf node vector
  std::map<unsigned int, unsigned int> nodeIdToIndex;
  unsigned int leafIndex = 0;
  for (auto& nodePtr : m_leafNodes) {
    auto& leafNode = *nodePtr;
    nodeIdToIndex[leafNode.stateId] = leafIndex;
    ++leafIndex;
  }

  // Remap clusterIds to vector from leaf node index to module index instead of nodeId -> clusterId
  std::map<unsigned int, std::set<unsigned int>> clusterIdToNodeIds;
  for (auto it : clusterIds) {
    clusterIdToNodeIds[it.second].insert(it.first);
  }
  unsigned int numNodes = numLeafNodes();
  std::vector<unsigned int> modules(numNodes);
  std::vector<unsigned int> selectedNodes(numNodes, 0);
  unsigned int moduleIndex = 0;
  for (const auto& it : clusterIdToNodeIds) {
    auto& nodes = it.second;
    for (auto& nodeId : nodes) {
      auto nodeIndex = nodeIdToIndex[nodeId];
      modules[nodeIndex] = moduleIndex;
      ++selectedNodes[nodeIndex];
    }
    ++moduleIndex;
  }

  unsigned int numNodesWithoutClusterInfo = static_cast<unsigned int>(
      std::count_if(selectedNodes.begin(), selectedNodes.end(), [](unsigned int count) { return count == 0; }));

  if (numNodesWithoutClusterInfo == 0) {
    return initPartition(modules, hard);
  }

  if (assignToNeighbouringModule) {
    Log() << "\n -> " << numNodesWithoutClusterInfo << " nodes not found in cluster file are put into neighbouring modules if possible. ";
    for (unsigned int i = 0; i < numNodes; ++i) {
      if (selectedNodes[i] == 0) {
        // Check out edges greedily for connected modules
        auto& node = *m_leafNodes[i];
        for (auto& e : node.outEdges()) {
          auto& edge = *e;
          auto targetNodeIndex = nodeIdToIndex[edge.target->stateId];
          if (selectedNodes[targetNodeIndex] != 0) {
            selectedNodes[i] = 1;
            modules[i] = modules[targetNodeIndex];
            break;
          }
        }
        if (selectedNodes[i] == 0) {
          // Check in edges greedily for connected modules
          for (auto& e : node.inEdges()) {
            auto& edge = *e;
            auto sourceNodeIndex = nodeIdToIndex[edge.source->stateId];
            if (selectedNodes[sourceNodeIndex] != 0) {
              selectedNodes[i] = 1;
              modules[i] = modules[sourceNodeIndex];
              break;
            }
          }
        }
        if (selectedNodes[i] == 0) {
          // No connected node with a module index, fall back to create new module
          selectedNodes[i] = 1;
          modules[i] = moduleIndex;
          ++moduleIndex;
        }
      }
    }
  } else {
    Log() << "\n -> " << numNodesWithoutClusterInfo << " nodes not found in cluster file are put into separate modules. ";
    // Put non-selected nodes in its own module
    for (unsigned int i = 0; i < numNodes; ++i) {
      if (selectedNodes[i] == 0) {
        modules[i] = moduleIndex;
        ++moduleIndex;
      }
    }
  }

  return initPartition(modules);
}

InfomapBase& InfomapBase::initPartition(std::vector<std::vector<unsigned int>>& clusters, bool hard)
{
  Log(3) << "\n -> Init " << (hard ? "hard" : "soft") << " partition... " << std::flush;
  unsigned int numNodes = numLeafNodes();
  // Get module indices from the merge structure
  std::vector<unsigned int> modules(numNodes);
  std::vector<unsigned int> selectedNodes(numNodes, 0);
  unsigned int moduleIndex = 0;

  for (auto& cluster : clusters) {
    if (cluster.empty())
      continue;
    for (auto id : cluster) {
      ++selectedNodes[id];
      modules[id] = moduleIndex;
    }
    ++moduleIndex;
  }

  // Put non-selected nodes in its own module
  unsigned int numNodesWithoutClusterInfo = 0;
  for (unsigned int i = 0; i < numNodes; ++i) {
    if (selectedNodes[i] == 0) {
      modules[i] = moduleIndex;
      ++moduleIndex;
      ++numNodesWithoutClusterInfo;
    }
  }
  if (numNodesWithoutClusterInfo > 0)
    Log() << "\n -> " << numNodesWithoutClusterInfo << " nodes not found in cluster file are put into separate modules. ";

  return initPartition(modules, hard);
}

InfomapBase& InfomapBase::initPartition(std::vector<unsigned int>& modules, bool hard)
{
  removeModules();
  setActiveNetworkFromLeafs();
  initPartition(); // TODO: confusing same name, should be able to init default without arguments here too
  moveActiveNodesToPredefinedModules(modules);
  consolidateModules(false);

  if (hard) {
    // Save the original network
    m_originalLeafNodes.swap(m_leafNodes);

    // Use the pre-partitioned network as the new leaf network
    unsigned int numNodesInNewNetwork = numTopModules();
    m_leafNodes.resize(numNodesInNewNetwork);
    unsigned int nodeIndex = 0;
    unsigned int numLinksInNewNetwork = 0;
    for (InfoNode& node : m_root) {
      m_leafNodes[nodeIndex] = &node;
      // Collapse children
      node.collapseChildren();
      numLinksInNewNetwork += node.outDegree();
      ++nodeIndex;
    }

    Log() << "\n -> Hard-partitioned the network to " << numNodesInNewNetwork << " nodes and " << numLinksInNewNetwork << " links with codelength " << *this << '\n';
  } else {
    if (prettyOutput)
      PrettyOutput(true).status("Initial", io::Str() << "codelength " << *this << ", " << numTopModules() << " top modules");
    Log() << "\n -> Initiated to codelength " << *this << " in " << numTopModules() << " top modules.\n";
  }
  m_hierarchicalCodelength = getCodelength();

  return *this;
}

void InfomapBase::generateSubNetwork(Network& network)
{
  unsigned int numNodes = network.numNodes();
  auto& metaData = network.metaData();
  numMetaDataDimensions = network.numMetaDataColumns();

  Log() << "Build internal network with " << numNodes << " nodes and " << network.numLinks() << " links...\n";
  if (!metaData.empty()) {
    Log() << "and " << metaData.size() << " meta-data records in " << numMetaDataDimensions << " dimensions\n";
  }

  m_leafNodes.reserve(numNodes);
  double sumNodeFlow = 0.0;
  double sumTeleFlow = 0.0;
  std::unordered_map<unsigned int, unsigned int> nodeIndexMap;
  nodeIndexMap.reserve(numNodes);
  for (auto& nodeIt : network.nodes()) {
    auto& networkNode = nodeIt.second;
    auto* node = new InfoNode(networkNode.flow, networkNode.id, networkNode.physicalId, networkNode.layerId);
    node->data.teleportWeight = networkNode.weight;
    node->data.teleportFlow = networkNode.teleFlow;
    node->data.exitFlow = networkNode.exitFlow;
    node->data.enterFlow = networkNode.enterFlow;
    node->layerTeleFlowData.emplace_back(networkNode.layerId, networkNode.intraLayerTeleFlow, networkNode.intraLayerTeleWeight);
    // Log() << "Node " << node->stateId << " (" << node->layerId << "," << node->physicalId << ") data: " << node->data << ", tele-flow: " << networkNode.teleFlow << ", intra-tele: " << networkNode.intraLayerTeleFlow << "\n";
    if (haveMetaData()) {
      auto meta = metaData.find(networkNode.id);
      if (meta != metaData.end()) {
        node->metaData = meta->second;
      } else {
        node->metaData = std::vector<int>(numMetaDataDimensions, -1);
      }
    }
    sumNodeFlow += networkNode.flow;
    sumTeleFlow += networkNode.teleFlow;
    m_root.addChild(node);
    nodeIndexMap[networkNode.id] = m_leafNodes.size();
    m_leafNodes.push_back(node);
  }
  m_root.data.flow = sumNodeFlow;
  m_root.data.teleportFlow = sumTeleFlow;
  // m_calculateEnterExitFlow = true; //TODO: Implement always in flow calculation
  if (!this->regularized) {
    m_calculateEnterExitFlow = true;
  }
  if (std::abs(sumNodeFlow - 1.0) > 1e-10)
    Log() << "Warning, total flow on nodes differs from 1.0 by " << sumNodeFlow - 1.0 << ".\n";

  bool changeMarkovTime = std::abs(markovTime - 1) > 1e-3;
  if (changeMarkovTime) {
    Log() << "  -> Rescale link flow with global Markov time " << markovTime << "\n";
  }

  for (auto& linkIt : network.nodeLinkMap()) {
    unsigned int linkSourceId = linkIt.first.id;
    unsigned int sourceIndex = nodeIndexMap[linkSourceId];
    const auto& subLinks = linkIt.second;
    for (auto& subIt : subLinks) {
      unsigned int linkTargetId = subIt.first.id;
      unsigned int targetIndex = nodeIndexMap[linkTargetId];
      // Ignore self-links in optimization as it doesn't change enter/exit flow on modular level
      if (sourceIndex != targetIndex) {
        auto& linkData = subIt.second;
        m_leafNodes[sourceIndex]->addOutEdge(*m_leafNodes[targetIndex], linkData.weight, linkData.flow * markovTime);
      }
    }
  }

  if (variableMarkovTime) {
    Log() << "  -> Rescale link flow with variable Markov time\n";
    if (std::abs(variableMarkovTimeDamping - 1) > 1e-9) {
      Log() << "  -> Use variable Markov time strength " << variableMarkovTimeDamping << "\n";
    }
  }

  double maxEntropy = 0.0;
  double maxFlow = 0.0;
  double entropyRate = 0.0;
  unsigned int maxDegree = 0;
  unsigned int maxOutDegree = 0;
  unsigned int maxInDegree = 0;
  double totDegree = network.sumDegree();
  std::vector<double> entropies(numNodes, 0);

  for (unsigned i = 0; i < numNodes; ++i) {
    InfoNode& node = *m_leafNodes[i];
    maxDegree = std::max(maxDegree, node.degree());
    maxOutDegree = std::max(maxOutDegree, node.outDegree());
    maxInDegree = std::max(maxInDegree, node.inDegree());
    double entropy = 0;
    double sumOut = 0;
    for (InfoEdge* e : node.outEdges()) {
      entropy -= infomath::plogp(e->data.weight);
      sumOut += e->data.weight;
    }
    if (isUndirectedFlow()) {
      for (InfoEdge* e : node.inEdges()) {
        entropy -= infomath::plogp(e->data.weight);
        sumOut += e->data.weight;
      }
    }
    entropy = sumOut > 1e-9 ? (entropy + infomath::plogp(sumOut)) / sumOut : 0;
    maxEntropy = std::max(maxEntropy, entropy);
    entropyRate += m_leafNodes[i]->data.flow * entropy;
    maxFlow = std::max(maxFlow, node.data.flow);
    entropies[i] = entropy; // Store for undirected networks
  }

  m_entropyRate = entropyRate;
  m_maxEntropy = maxEntropy;
  m_maxFlow = maxFlow;

  double minLocalScale = variableMarkovTimeMinLocalScale;
  double damping = variableMarkovTimeDamping;

  double maxScale = infomath::linlog(maxFlow * totDegree, damping);

  if (variableMarkovTime) {
    if (damping < 0) {
      maxScale = infomath::linlog(pow(2.0, maxEntropy), -damping);
    }
    for (unsigned i = 0; i < numNodes; ++i) {
      InfoNode& node = *m_leafNodes[i];
      double localScale = damping < 0 ? infomath::linlog(pow(2.0, entropies[i]), -damping) : infomath::linlog(std::max(minLocalScale, node.data.flow * totDegree), damping);
      for (InfoEdge* e : node.outEdges()) {
        if (isUndirectedFlow()) {
          double oppositeLocalScale = damping < 0 ? infomath::linlog(pow(2.0, entropies[nodeIndexMap[e->target->stateId]]), -damping) : infomath::linlog(std::max(minLocalScale, e->target->data.flow * totDegree), damping);
          localScale = std::max(localScale, oppositeLocalScale);
        }
        double localMarkovTimeScale = maxScale / std::max(minLocalScale, localScale);
        e->data.flow *= localMarkovTimeScale;
        // Note: deliberately do NOT write the scaled flow back into the shared StateNetwork
        // (network.nodeLinkMap()). The optimizer and output use only the per-instance InfoEdge
        // flow (e->data.flow), so the write-back was a non-idempotent side effect that made
        // --parallel-trials unsafe (workers share one Network). Library getLinks(flow=True) /
        // getLinkResults() now return the input link flow rather than the VMT-scaled flow.
      }
    }
  }
}

void InfomapBase::generateSubNetwork(InfoNode& parent)
{
  // Store parent module
  m_root.owner = &parent;
  m_root.data = parent.data;

  unsigned int numNodes = parent.childDegree();
  m_leafNodes.resize(numNodes);

  Log(1) << "Generate sub network with " << numNodes << " nodes...\n";

  unsigned int childIndex = 0;
  for (InfoNode& node : parent) {
    auto* clonedNode = new InfoNode(node);
    clonedNode->initClean();
    m_root.addChild(clonedNode);
    node.index = childIndex; // Set index to its place in this subnetwork to be able to find edge target below
    m_leafNodes[childIndex] = clonedNode;
    ++childIndex;
  }

  InfoNode* parentPtr = &parent;
  // Clone edges
  for (InfoNode& node : parent) {
    for (InfoEdge* e : node.outEdges()) {
      InfoEdge& edge = *e;
      // If neighbour node is within the same module, add the link to this subnetwork.
      if (edge.target->parent == parentPtr) {
        m_leafNodes[edge.source->index]->addOutEdge(*m_leafNodes[edge.target->index], edge.data.weight, edge.data.flow);
      }
    }
  }
}

void InfomapBase::init()
{
  Log(3) << "InfomapBase::init()...\n";

  if (m_calculateEnterExitFlow && isMainInfomap())
    initEnterExitFlow();

  initNetwork();

  m_oneLevelCodelength = calcCodelength(m_root);
  Log() << "  -> One-level codelength: " << m_oneLevelCodelength << '\n';
}

// ===================================================
// Run: *
// ===================================================

void InfomapBase::hierarchicalPartition()
{
  Log(1) << "Hierarchical partition...\n";

  const auto depth = maxTreeDepth();
  if (depth > 2) {
    Log(1) << "Continuing from a tree with " << depth << " levels...\n";

    if (fastHierarchicalSolution == 0) {
      Log(1) << "Removing sub modules...\n";
      removeSubModules(true);
      m_hierarchicalCodelength = calcCodelengthOnTree(root(), true);
    }

    else if (fastHierarchicalSolution == 1) {
      Log(1) << "Fine-tune bottom modules... ";
      const bool shouldRestoreSilent = isMainInfomap() && !Log::isThreadMuted();
      bool isSilent = shouldRestoreSilent && Log::isSilent();

      double codelengthBefore = 0.0;
      double codelengthAfter = 0.0;

      for (InfoNode& module : m_root.infomapTree()) {
        if (!module.isLeaf() && module.firstChild->isLeafModule()) {
          codelengthBefore += module.codelength;
          auto numLeafs = 0;
          for (auto& leafModule : module) {
            numLeafs += leafModule.childDegree();
          }

          std::vector<unsigned int> modules(numLeafs);
          std::vector<InfoNode*> leafs(numLeafs);

          auto i = 0;
          for (auto it = module.begin_tree(); !it.isEnd(); ++it) {
            if (it->isLeaf()) {
              modules[i] = it.moduleIndex();
              leafs[i] = it.current();
              ++i;
            }
          }

          module.replaceChildrenWithGrandChildren();

          auto& subInfomap = getSubInfomap(module).initNetwork(module);

          subInfomap.initPartition(modules);

          // Run two-level partition + find hierarchically super modules (skip recursion)
          subInfomap.setOnlySuperModules(true).run();

          // Collect sub Infomap modules
          i = 0;
          for (auto& subLeafPtr : subInfomap.leafNodes()) {
            modules[i] = subLeafPtr->index;
            ++i;
          }

          // Create new sub modules
          std::vector<InfoNode*> subModules(numLeafs, nullptr);
          module.releaseChildren();

          for (auto j = 0; j < numLeafs; ++j) {
            InfoNode* leaf = leafs[j];
            unsigned int moduleIndex = modules[j];
            if (subModules[moduleIndex] == nullptr) {
              subModules[moduleIndex] = new InfoNode(subInfomap.leafNodes()[j]->parent->data);
              subModules[moduleIndex]->index = moduleIndex;
              module.addChild(subModules[moduleIndex]);
            }
            subModules[moduleIndex]->addChild(leaf);
          }
          module.disposeInfomap();
          module.codelength = calcCodelength(module);
          codelengthAfter += module.codelength;
        }
      }

      if (shouldRestoreSilent)
        Log::setSilent(isSilent);

      const double diffCodelength = codelengthBefore - codelengthAfter;
      Log() << "done! Codelength improvement " << (diffCodelength / codelengthBefore) * 100 << "% to codelength " << codelengthAfter << "\n";
    }

    recursivePartition();
    return;
  }

  partition();

  if (numTopModules() == 1 || numTopModules() == numLeafNodes()) {
    Log(1) << "Trivial partition, skip search for hierarchical solution.\n";
    return;
  }

  if (numTopModules() > preferredNumberOfModules) {
    findHierarchicalSuperModules();
  }

  if (onlySuperModules) {
    removeSubModules(true);
    m_hierarchicalCodelength = calcCodelengthOnTree(root(), true);
    return;
  }

  if (fastHierarchicalSolution >= 2) {
    // FIXME Calculate individual module codelengths and store on the modules?
    return;
  }

  if (fastHierarchicalSolution == 0) {
    removeSubModules(true);
    m_hierarchicalCodelength = calcCodelengthOnTree(root(), true);
  }

  recursivePartition();
}

void InfomapBase::partition()
{
  auto oldPrecision = Log::precision();
  std::vector<std::string> prettyCompression;
  if (prettyOutput)
    PrettyOutput(true).section("Optimization");
  Log(0, 0) << "Two-level compression: " << std::setprecision(2) << std::flush;
  Log(1) << "Trying to find modular structure... \n";
  double initialCodelength = m_oneLevelCodelength;
  double oldCodelength = initialCodelength;

  m_tuneIterationIndex = 0;
  findTopModulesRepeatedly(levelAggregationLimit);

  double newCodelength = getCodelength();
  double compression = oldCodelength < 1e-16 ? 0.0 : (oldCodelength - newCodelength) / oldCodelength;
  if (prettyOutput)
    prettyCompression.push_back(PrettyOutput::percent(compression * 100));
  Log(0, 0) << (compression * 100) << "% " << std::flush;
  oldCodelength = newCodelength;

  bool doFineTune = true;
  bool coarseTuned = false;
  while (numTopModules() > 1 && (m_tuneIterationIndex + 1) != tuneIterationLimit) {
    ++m_tuneIterationIndex;
    if (doFineTune) {
      Log(3) << "\n";
      Log(1, 3) << "Fine tune... " << std::flush;
      unsigned int numEffectiveLoops = fineTune();
      if (numEffectiveLoops > 0) {
        Log(1, 3) << " -> done in " << numEffectiveLoops << " effective loops to codelength " << *this << " in " << numTopModules() << " modules.\n";
        // Continue to merge fine-tuned modules
        findTopModulesRepeatedly(levelAggregationLimit);
      } else {
        Log(1, 3) << "no improvement.\n";
      }
    } else {
      coarseTuned = true;
      if (!noCoarseTune) {
        Log(3) << "\n";
        Log(1, 3) << "Coarse tune... " << std::flush;
        unsigned int numEffectiveLoops = coarseTune();
        if (numEffectiveLoops > 0) {
          Log(1, 3) << "done in " << numEffectiveLoops << " effective loops to codelength " << *this << " in " << numTopModules() << " modules.\n";
          // Continue to merge fine-tuned modules
          findTopModulesRepeatedly(levelAggregationLimit);
        } else {
          Log(1, 3) << "no improvement.\n";
        }
      }
    }
    newCodelength = getCodelength();
    compression = oldCodelength < 1e-16 ? 0.0 : (oldCodelength - newCodelength) / oldCodelength;
    bool isImprovement = newCodelength <= oldCodelength - minimumCodelengthImprovement && newCodelength < oldCodelength - initialCodelength * minimumRelativeTuneIterationImprovement;
    if (!isImprovement) {
      // Make sure coarse-tuning have been tried
      if (coarseTuned)
        break;
    } else {
      oldCodelength = newCodelength;
    }
    if (prettyOutput)
      prettyCompression.push_back(PrettyOutput::percent(compression * 100));
    Log(0, 0) << (compression * 100) << "% " << std::flush;
    doFineTune = !doFineTune;
  }

  if (prettyOutput) {
    std::string nonTrivialSummary;
    if (m_numNonTrivialTopModules != numTopModules())
      nonTrivialSummary = io::Str() << " (" << m_numNonTrivialTopModules << " non-trivial)";
    PrettyOutput(true).status("Two-level", io::Str() << compressionSummary(prettyCompression) << " -> codelength " << io::toPrecision(getCodelength()) << ", " << numTopModules() << " modules" << nonTrivialSummary);
  }
  Log(0, 0) << std::setprecision(oldPrecision) << '\n';
  Log() << "Partitioned to codelength " << *this << " in " << numTopModules();
  if (m_numNonTrivialTopModules != numTopModules())
    Log() << " (" << m_numNonTrivialTopModules << " non-trivial)";
  Log() << " modules.\n";

  const bool regularizedPriorOnly = regularized && network().numLinks() == 0;
  if (!preferModularSolution && preferredNumberOfModules == 0 && (haveNonTrivialModules() || regularizedPriorOnly) && getCodelength() > getOneLevelCodelength()) {
    Log() << "Worse codelength than one-level codelength, putting all nodes in one module... ";

    // Create new single module between modules and root
    auto& module = root().replaceChildrenWithOneNode();
    // TODO: Extract copying from root and resetting index to a method, also copy metadata?
    module.data = m_root.data;
    module.physicalNodes = m_root.physicalNodes;
    module.index = 0;
    for (auto& node : module) {
      node.index = 0;
    }
    module.codelength = getOneLevelCodelength();
    m_hierarchicalCodelength = getOneLevelCodelength();
    // calcCodelengthOnTree(root(), true);
    m_root.codelength = 0.0;
    m_numNonTrivialTopModules = calculateNumNonTrivialTopModules();

  } else {
    // Set consolidated cluster index on nodes and modules
    for (auto clusterIt = m_root.begin_tree(); !clusterIt.isEnd(); ++clusterIt) {
      clusterIt->index = clusterIt.moduleIndex();
    }

    // Calculate individual module codelengths and store on the modules
    calcCodelengthOnTree(root(), false);
    m_root.codelength = getIndexCodelength();
    m_hierarchicalCodelength = getCodelength();
  }
}

void InfomapBase::restoreHardPartition()
{
  // Collect all leaf nodes in a separate sequence to be able to iterate independent of tree structure
  std::vector<InfoNode*> leafNodes(numLeafNodes());
  unsigned int leafIndex = 0;
  for (InfoNode& node : m_root.infomapTree()) {
    if (node.isLeaf()) {
      leafNodes[leafIndex] = &node;
      ++leafIndex;
    }
  }
  // Expand all children
  unsigned int numExpandedChildren = 0;
  unsigned int numExpandedNodes = 0;
  for (InfoNode* node : leafNodes) {
    ++numExpandedNodes;
    numExpandedChildren += node->expandChildren();
    node->replaceWithChildren();
  }

  // Swap back original leaf nodes
  m_originalLeafNodes.swap(m_leafNodes);

  Log() << "Expanded " << numExpandedNodes << " hard modules to " << numExpandedChildren << " original nodes.\n";
}

// ===================================================
// Run: Init: *
// ===================================================

void InfomapBase::initEnterExitFlow()
{
  // Not done in Bayesian
  // TODO: Skip this, always add enter/exit/tele flow from flow calculator
  // Calculate enter/exit
  double alpha = teleportationProbability;
  double beta = 1.0 - alpha;

  for (auto* n : m_leafNodes) {
    n->data.enterFlow = n->data.exitFlow = 0.0;
  }
  if (!isUndirectedClustering()) {
    for (auto* n : m_leafNodes) {
      auto& node = *n;
      node.data.teleportFlow = alpha * node.data.flow;
      node.data.teleportSourceFlow = node.data.flow;
      if (node.isDangling()) {
        node.data.teleportFlow = node.data.flow;
        node.data.danglingFlow = node.data.flow;
        m_sumDanglingFlow += node.data.flow;
      }
    }
    for (auto* n : m_leafNodes) {
      auto& node = *n;
      for (InfoEdge* e : node.outEdges()) {
        InfoEdge& edge = *e;
        // Self-links not included here, should not add to enter and exit flow in its enclosing module
        edge.source->data.exitFlow += edge.data.flow;
        edge.target->data.enterFlow += edge.data.flow;
      }
      if (recordedTeleportation) {
        // Don't let self-teleportation add to the enter/exit flow (i.e. multiply with (1.0 - node.data.teleportWeight))
        node.data.exitFlow += (alpha * node.data.flow + beta * node.data.danglingFlow) * (1.0 - node.data.teleportWeight);
        node.data.enterFlow += (alpha * (1.0 - node.data.flow) + beta * (m_sumDanglingFlow - node.data.danglingFlow)) * node.data.teleportWeight;
      }
    }
  } else {
    for (auto* n : m_leafNodes) {
      auto& node = *n;
      node.data.teleportFlow = alpha * node.data.flow;
      node.data.teleportSourceFlow = node.data.flow;
      if (node.isDangling()) {
        node.data.teleportFlow = node.data.flow;
        node.data.danglingFlow = node.data.flow;
      }
      for (InfoEdge* e : node.outEdges()) {
        InfoEdge& edge = *e;
        // Self-links not included here, should not add to enter and exit flow in its enclosing module
        double halfFlow = edge.data.flow / 2;
        edge.source->data.exitFlow += halfFlow;
        edge.target->data.exitFlow += halfFlow;
        edge.source->data.enterFlow += halfFlow;
        edge.target->data.enterFlow += halfFlow;
      }
    }
  }
}

// Aggregate node and enter/exit flow to all tree nodes
void InfomapBase::aggregateFlowValuesFromLeafToRoot()
{
  for (auto& node : root().infomapTree()) {
    if (!node.isLeaf()) {
      node.data = {};
    }
  }

  // Aggregate flow from leaf nodes to root node
  unsigned int numLevels = 0;
  for (auto it = root().begin_post_depth_first(); !it.isEnd(); ++it) {
    auto& node = *it;
    if (!node.isRoot()) {
      node.parent->data.flow += node.data.flow;
      node.parent->data.teleportFlow += node.data.teleportFlow;
      node.parent->data.teleportSourceFlow += node.data.teleportSourceFlow;
      node.parent->data.teleportWeight += node.data.teleportWeight;
      node.parent->data.danglingFlow += node.data.danglingFlow;
    }
    // Don't aggregate enter and exit flow
    if (!node.isLeaf()) {
      node.index = it.depth(); // Use index to store the depth on modules
      node.data.enterFlow = 0.0;
      node.data.exitFlow = 0.0;
    } else
      numLevels = std::max(numLevels, it.depth());
  }

  if (std::abs(root().data.flow - 1.0) > 1e-10) {
    Log() << "Warning, aggregated flow is not exactly 1.0, but " << std::setprecision(10) << root().data.flow << std::setprecision(9) << ".\n";
  }

  // Aggregate enter and exit flow between modules
  for (auto& leafNode : m_leafNodes) {
    auto& leafNodeSource = *leafNode;
    for (InfoEdge* e : leafNodeSource.outEdges()) {
      auto& edge = *e;
      auto& leafNodeTarget = edge.target;
      double linkFlow = edge.data.flow;
      double halfFlow = linkFlow / 2;

      InfoNode* node1 = leafNodeSource.parent;
      InfoNode* node2 = leafNodeTarget->parent;

      if (node1 == node2)
        continue;

      // First aggregate link flow until equal depth
      while (node1->index > node2->index) {
        if (isUndirectedClustering()) {
          node1->data.exitFlow += halfFlow;
          node1->data.enterFlow += halfFlow;
        } else {
          node1->data.exitFlow += linkFlow;
        }
        node1 = node1->parent;
      }
      while (node2->index > node1->index) {
        if (isUndirectedClustering()) {
          node2->data.enterFlow += halfFlow;
          node2->data.exitFlow += halfFlow;
        } else {
          node2->data.enterFlow += linkFlow;
        }
        node2 = node2->parent;
      }

      // Then aggregate link flow until equal parent
      while (node1 != node2) {
        if (isUndirectedClustering()) {
          node1->data.exitFlow += halfFlow;
          node1->data.enterFlow += halfFlow;
          node2->data.enterFlow += halfFlow;
          node2->data.exitFlow += halfFlow;
        } else {
          node1->data.exitFlow += linkFlow;
          node2->data.enterFlow += linkFlow;
        }
        node1 = node1->parent;
        node2 = node2->parent;
      }
    }
  }
  if (recordedTeleportation) {
    Log() << "\n\nAggregating enter/exit flow for recorded teleportation, sum teleFlow: " << m_root.data.teleportFlow << "\n";

    for (auto& node : m_root.infomapTree()) {
      if (!node.isLeaf()) {
        // Don't code self-teleportation

        node.data.enterFlow += (m_root.data.teleportFlow - node.data.teleportFlow) * node.data.teleportWeight;
        node.data.exitFlow += node.data.teleportFlow * (1.0 - node.data.teleportWeight);
      }
    }
  }
}

double InfomapBase::calcCodelengthOnTree(InfoNode& root, bool includeRoot) const
{
  double totalCodelength = 0.0;
  // Calculate enter/exit flow (assuming 0 by default)
  for (auto& node : root.infomapTree()) {
    if (node.isLeaf() || (!includeRoot && node.isRoot()))
      continue;
    node.codelength = calcCodelength(node);
    totalCodelength += node.codelength;
  }
  return totalCodelength;
}

// ===================================================
// Run: Partition: *
// ===================================================

void InfomapBase::setActiveNetworkFromChildrenOfRoot()
{
  m_moduleNodes.resize(m_root.childDegree());
  unsigned int i = 0;
  for (auto& node : m_root)
    m_moduleNodes[i++] = &node;
  m_activeNetwork = &m_moduleNodes;
}

void InfomapBase::findTopModulesRepeatedly(unsigned int maxLevels)
{
  Log(1, 2) << "Iteration " << (m_tuneIterationIndex + 1) << ", moving ";
  Log(3) << "\nIteration " << (m_tuneIterationIndex + 1) << ":\n";
  m_aggregationLevel = 0;
  unsigned int numLevelsConsolidated = numLevels() - 1;
  if (maxLevels == 0)
    maxLevels = std::numeric_limits<unsigned int>::max();

  std::string initialCodelength;

  // Reapply core algorithm on modular network, replacing modules with super modules
  while (numTopModules() > 1 && numLevelsConsolidated != maxLevels) {
    if (haveModules())
      setActiveNetworkFromChildrenOfRoot();
    else
      setActiveNetworkFromLeafs();
    initPartition();
    if (m_aggregationLevel == 0) {
      initialCodelength = io::Str() << "" << *this;
    }

    Log(1, 2) << activeNetwork().size() << "*" << std::flush;
    Log(3) << "Level " << (numLevelsConsolidated + 1) << " (codelength: " << *this << "): Moving " << activeNetwork().size() << " nodes... " << std::flush;
    // Core loop, merging modules
    unsigned int numOptimizationLoops = optimizeActiveNetwork();

    Log(1, 2) << numOptimizationLoops << ", " << std::flush;
    Log(3) << "done! -> codelength " << *this << " in " << numActiveModules() << " modules.\n";

    // If no improvement, revert codelength terms to the last consolidated state
    if (haveModules() && restoreConsolidatedOptimizationPointIfNoImprovement()) {
      Log(3) << "-> Restored to codelength " << *this << " in " << numTopModules() << " modules.\n";
      break;
    }

    // Consolidate modules
    bool replaceExistingModules = haveModules();
    consolidateModules(replaceExistingModules);
    ++numLevelsConsolidated;
    ++m_aggregationLevel;
  }
  m_aggregationLevel = 0;

  m_numNonTrivialTopModules = calculateNumNonTrivialTopModules();

  Log(1, 2) << (m_isCoarseTune ? "modules" : "nodes") << "*loops from codelength " << initialCodelength << " to codelength " << *this << " in " << numTopModules() << " modules. (" << m_numNonTrivialTopModules << " non-trivial modules)\n";
}

unsigned int InfomapBase::fineTune()
{
  if (numLevels() != 2)
    throw std::logic_error("InfomapBase::fineTune() called but numLevels != 2");

  setActiveNetworkFromLeafs();
  initPartition();

  // Make sure module nodes have correct index //TODO: Assume always correct here?
  unsigned int moduleIndex = 0;
  for (auto& module : m_root) {
    module.index = moduleIndex++;
  }

  // Put leaf modules in existing modules
  std::vector<unsigned int> modules(numLeafNodes());
  for (unsigned int i = 0; i < numLeafNodes(); ++i) {
    modules[i] = m_leafNodes[i]->parent->index;
  }
  moveActiveNodesToPredefinedModules(modules);

  Log(3) << " -> moved to codelength " << *this << " in " << numActiveModules() << " existing modules. Try tuning...\n";

  // Continue to optimize from there to tune leaf nodes
  unsigned int numEffectiveLoops = optimizeActiveNetwork();
  if (numEffectiveLoops == 0) {
    restoreConsolidatedOptimizationPointIfNoImprovement();
    Log(4) << "Fine-tune didn't improve solution, restoring last.\n";
  } else {
    // Delete existing modules and consolidate fine-tuned modules
    root().replaceChildrenWithGrandChildren();
    consolidateModules(false);
    Log(4) << "Fine-tune done in " << numEffectiveLoops << " effective loops to codelength " << *this << " in " << numTopModules() << " modules.\n";
  }

  return numEffectiveLoops;
}

unsigned int InfomapBase::coarseTune()
{
  if (numLevels() != 2)
    throw std::logic_error("InfomapBase::coarseTune() called but numLevels != 2");

  Log(4) << "Coarse-tune...\nPartition each module in sub-modules for coarse tune...\n";

  Log::ScopedMute muteNestedMainRun(isMainInfomap());

  unsigned int moduleIndexOffset = 0;
  for (auto& node : m_root) {
    // Don't search for sub-modules in too small modules
    if (node.childDegree() < 2) {
      for (auto& child : node) {
        child.index = moduleIndexOffset;
      }
      moduleIndexOffset += 1;
      continue;
    } else {
      InfomapBase& subInfomap = getSubInfomap(node)
                                    .setTwoLevel(true)
                                    .setTuneIterationLimit(1);
      subInfomap.initNetwork(node).run();

      auto originalLeafIt = node.begin_child();
      for (auto& subLeafPtr : subInfomap.leafNodes()) {
        InfoNode& subLeaf = *subLeafPtr;
        originalLeafIt->index = subLeaf.index + moduleIndexOffset;
        ++originalLeafIt;
      }
      moduleIndexOffset += subInfomap.numTopModules();

      node.disposeInfomap();
    }
  }

  Log(4) << "Move leaf nodes to " << moduleIndexOffset << " sub-modules... \n";
  // Put leaf modules in the calculated sub-modules
  std::vector<unsigned int> subModules(numLeafNodes());
  for (unsigned int i = 0; i < numLeafNodes(); ++i) {
    subModules[i] = m_leafNodes[i]->index;
  }

  setActiveNetworkFromLeafs();
  initPartition();
  moveActiveNodesToPredefinedModules(subModules);

  Log(4) << "Moved to " << numActiveModules() << " modules...\n";

  // Replace existing modules but store that structure on the sub-modules
  consolidateModules(true);

  Log(4) << "Consolidated " << numTopModules() << " sub-modules to codelength " << *this << '\n';

  Log(4) << "Move sub-modules to former modular structure... \n";
  // Put sub-modules in the former modular structure
  std::vector<unsigned int> modules(numTopModules());
  unsigned int iModule = 0;
  for (auto& subModule : m_root) {
    modules[iModule++] = subModule.index;
  }

  setActiveNetworkFromChildrenOfRoot();
  initPartition();
  // Move sub-modules to former modular structure
  moveActiveNodesToPredefinedModules(modules);

  Log(4) << "Tune sub-modules from codelength " << *this << " in " << numActiveModules() << " modules... \n";
  // Continue to optimize from there to tune sub-modules
  unsigned int numEffectiveLoops = optimizeActiveNetwork();
  Log(4) << "Tuned sub-modules in " << numEffectiveLoops << " effective loops to codelength " << *this << " in " << numActiveModules() << " modules.\n";
  consolidateModules(true);
  Log(4) << "Consolidated tuned sub-modules to codelength " << *this << " in " << numTopModules() << " modules.\n";
  return numEffectiveLoops;
}

unsigned int InfomapBase::calculateNumNonTrivialTopModules() const
{
  if (root().childDegree() == 1)
    return 0;
  unsigned int numNonTrivialTopModules = 0;
  for (auto& module : m_root) {
    if (module.childDegree() > 1)
      ++numNonTrivialTopModules;
  }
  return numNonTrivialTopModules;
}

unsigned int InfomapBase::calculateMaxDepth() const
{
  unsigned int maxDepth = 0;
  for (auto it(m_root.begin_tree()); !it.isEnd(); ++it) {
    if (it->isLeaf()) {
      maxDepth = std::max(maxDepth, it.depth());
    }
  }
  return maxDepth;
}

unsigned int InfomapBase::findHierarchicalSuperModulesFast(unsigned int superLevelLimit)
{
  if (superLevelLimit == 0)
    return 0;

  unsigned int numLevelsCreated = 0;
  double oldIndexLength = getIndexCodelength();
  double hierarchicalCodelength = getCodelength();
  double workingHierarchicalCodelength = hierarchicalCodelength;

  Log(1) << "\nFind hierarchical super modules iteratively...\n";
  Log(0, 0) << "Fast super-level compression: " << std::setprecision(2) << std::flush;

  // Add index codebooks as long as the code gets shorter
  do {
    Log(1) << "Iteration " << numLevelsCreated + 1 << ", finding super modules fast to " << numTopModules() << (haveModules() ? " modules" : " nodes") << "... \n";

    if (haveModules()) {
      setActiveNetworkFromChildrenOfRoot();
      transformNodeFlowToEnterFlow(m_root);
      initSuperNetwork();
    } else {
      setActiveNetworkFromLeafs();
    }

    initPartition();

    unsigned int numEffectiveLoops = optimizeActiveNetwork();

    double codelength = getCodelength();
    double indexCodelength = getIndexCodelength();
    unsigned int numSuperModules = numActiveModules();
    bool trivialSolution = numSuperModules == 1 || numSuperModules == activeNetwork().size();

    bool acceptSolution = !trivialSolution && codelength < oldIndexLength - minimumCodelengthImprovement;
    // Force at least one modular level!
    bool acceptByForce = !acceptSolution && !haveModules();
    if (acceptByForce)
      acceptSolution = true;

    workingHierarchicalCodelength += codelength - oldIndexLength;

    Log(0, 0) << hideIf(!acceptSolution) << ((hierarchicalCodelength - workingHierarchicalCodelength) / hierarchicalCodelength * 100) << "% " << std::flush;
    Log(1) << "  -> Found " << numActiveModules() << " super modules in " << numEffectiveLoops << " effective loops with hierarchical codelength " << indexCodelength << " + " << (workingHierarchicalCodelength - indexCodelength) << " = " << workingHierarchicalCodelength << (acceptSolution ? "\n" : ", discarding the solution.\n") << std::flush;
    Log(1) << oldIndexLength << " -> " << *this << "\n";

    if (!acceptSolution) {
      restoreConsolidatedOptimizationPointIfNoImprovement(true);
      break;
    }

    // Consolidate the dynamic modules without replacing any existing ones.
    consolidateModules(false);

    hierarchicalCodelength = workingHierarchicalCodelength;
    oldIndexLength = indexCodelength;

    ++numLevelsCreated;

    m_numNonTrivialTopModules = calculateNumNonTrivialTopModules();

  } while (m_numNonTrivialTopModules > 1 && numLevelsCreated != superLevelLimit);

  // Restore the temporary transformation of flow to enter flow on super modules
  resetFlowOnModules();

  Log(0, 0) << "to codelength " << io::toPrecision(hierarchicalCodelength) << " in " << numTopModules() << " top modules.\n";
  Log(1) << "Finding super modules done! Added " << numLevelsCreated << " levels with hierarchical codelength " << io::toPrecision(hierarchicalCodelength) << " in " << numTopModules() << " top modules.\n";

  // Calculate and store hierarchical codelengths
  m_hierarchicalCodelength = calcCodelengthOnTree(root(), true);

  return numLevelsCreated;
}

unsigned int InfomapBase::findHierarchicalSuperModules(unsigned int superLevelLimit)
{
  if (superLevelLimit == 0)
    return 0;

  unsigned int numLevelsCreated = 0;
  double oldIndexLength = getIndexCodelength();
  double hierarchicalCodelength = getCodelength();
  double workingHierarchicalCodelength = hierarchicalCodelength;

  if (!haveModules())
    throw std::logic_error("Trying to find hierarchical super modules without any modules");

  Log(1) << "\nFind hierarchical super modules iteratively...\n";
  std::vector<std::string> prettyCompression;
  Log(0, 0) << "Super-level compression: " << std::setprecision(2) << std::flush;

  // Add index codebooks as long as the code gets shorter
  do {
    Log(1) << "Iteration " << numLevelsCreated + 1 << ", finding super modules to " << numTopModules() << " modules... \n";
    const bool shouldMuteNestedMainRun = isMainInfomap() && !Log::isThreadMuted();
    bool isSilent = false;
    if (shouldMuteNestedMainRun) {
      isSilent = Log::isSilent();
      Log::setSilent(true);
    }
    InfoNode tmp(m_root.data); // Temporary owner for the super Infomap instance
    auto& superInfomap = getSuperInfomap(tmp)
                             .setTwoLevel(true);
    superInfomap.initNetwork(m_root, true); //.initSuperNetwork();
    superInfomap.run();
    if (shouldMuteNestedMainRun) {
      Log::setSilent(isSilent);
    }
    double superCodelength = superInfomap.getCodelength();
    double superIndexCodelength = superInfomap.getIndexCodelength();

    unsigned int numSuperModules = superInfomap.numTopModules();
    bool trivialSolution = numSuperModules == 1 || numSuperModules == numTopModules();

    if (trivialSolution) {
      Log(1) << "failed to find non-trivial super modules.\n";
      break;
    }

    bool improvedCodelength = superCodelength < oldIndexLength - minimumCodelengthImprovement;
    if (!improvedCodelength) {
      Log(1) << "two-level index codebook not improved over one-level.\n";
      break;
    }

    workingHierarchicalCodelength += superCodelength - oldIndexLength;

    const double superCompression = ((hierarchicalCodelength - workingHierarchicalCodelength)
                                     / hierarchicalCodelength * 100);
    if (prettyOutput)
      prettyCompression.push_back(PrettyOutput::percent(superCompression));
    Log(0, 0) << ((hierarchicalCodelength - workingHierarchicalCodelength)
                  / hierarchicalCodelength * 100)
              << "% " << std::flush;
    Log(1) << "  -> Found " << numSuperModules << " super modules in with hierarchical codelength " << superIndexCodelength << " + " << (superCodelength - superIndexCodelength) << " + " << (workingHierarchicalCodelength - superCodelength) << " = " << workingHierarchicalCodelength << '\n';

    // Merge current top modules to two-level solution found in the super Infomap instance.
    std::vector<unsigned int> modules(numTopModules());
    unsigned int iModule = 0;
    for (InfoNode* n : superInfomap.leafNodes()) {
      modules[iModule++] = n->index;
    }

    setActiveNetworkFromChildrenOfRoot();
    initPartition();
    // Move top modules to super modules
    moveActiveNodesToPredefinedModules(modules);

    // Consolidate the dynamic modules without replacing any existing ones.
    consolidateModules(false);

    Log(1) << "Consolidated " << numTopModules() << " super modules. Index codelength: " << oldIndexLength << " -> " << *this << "\n";

    hierarchicalCodelength = workingHierarchicalCodelength;
    oldIndexLength = superIndexCodelength;

    ++numLevelsCreated;

    m_numNonTrivialTopModules = calculateNumNonTrivialTopModules();

  } while (m_numNonTrivialTopModules > 1 && numLevelsCreated != superLevelLimit);

  // Restore the temporary transformation of flow to enter flow on super modules
  resetFlowOnModules();

  if (prettyOutput)
    PrettyOutput(true).status("Super-level", io::Str() << compressionSummary(prettyCompression) << " -> codelength " << io::toPrecision(hierarchicalCodelength) << ", " << numTopModules() << " top modules");
  Log(0, 0) << "to codelength " << io::toPrecision(hierarchicalCodelength) << " in " << numTopModules() << " top modules.\n";
  Log(1) << "Finding super modules done! Added " << numLevelsCreated << " levels with hierarchical codelength " << io::toPrecision(hierarchicalCodelength) << " in " << numTopModules() << " top modules.\n";

  // Calculate and store hierarchical codelengths
  m_hierarchicalCodelength = calcCodelengthOnTree(root(), true);

  return numLevelsCreated;
}

void InfomapBase::transformNodeFlowToEnterFlow(InfoNode& parent)
{
  double sumFlow = 0.0;
  for (auto& module : m_root) {
    module.data.flow = module.data.enterFlow;
    sumFlow += module.data.flow;
  }
  parent.data.flow = sumFlow;
}

void InfomapBase::resetFlowOnModules()
{
  // Reset flow on all modules
  for (auto& module : m_root.infomapTree()) {
    if (!module.isLeaf())
      module.data.flow = 0.0;
  }
  // Aggregate flow from leaf nodes up in the tree
  for (InfoNode* n : m_leafNodes) {
    double leafNodeFlow = n->data.flow;
    InfoNode* module = n->parent;
    do {
      module->data.flow += leafNodeFlow;
      module = module->parent;
    } while (module != nullptr);
  }
}

unsigned int InfomapBase::removeModules()
{
  unsigned int numLevelsDeleted = 0;
  while (numLevels() > 1) {
    m_root.replaceChildrenWithGrandChildren();
    ++numLevelsDeleted;
  }
  if (numLevelsDeleted > 0) {
    Log(2) << "Removed " << numLevelsDeleted << " levels of modules.\n";
  }
  return numLevelsDeleted;
}

unsigned int InfomapBase::removeSubModules(bool recalculateCodelengthOnTree)
{
  unsigned int numLevelsDeleted = 0;
  while (numLevels() > 2) {
    for (InfoNode& module : m_root)
      module.replaceChildrenWithGrandChildren();
    ++numLevelsDeleted;
  }
  if (numLevelsDeleted > 0) {
    Log(2) << "Removed " << numLevelsDeleted << " levels of sub-modules.\n";
    if (recalculateCodelengthOnTree)
      calcCodelengthOnTree(root(), false);
  }
  return numLevelsDeleted;
}

unsigned int InfomapBase::recursivePartition()
{
  double indexCodelength = getIndexCodelength();
  double hierarchicalCodelength = m_hierarchicalCodelength;

  std::vector<std::string> prettyCompression;
  Log(0, 0) << "\nRecursive sub-structure compression: " << std::flush;

  PartitionQueue partitionQueue;
  if (fastHierarchicalSolution > 0) {
    queueLeafModules(partitionQueue);
    Log(1) << "\nFind sub modules recursively from " << partitionQueue.size() << " sub modules on level " << numLevels() - 2;
  } else {
    queueTopModules(partitionQueue);
    Log(1) << "\nFind sub modules recursively from " << partitionQueue.size() << " top modules";
    double moduleCodelength = 0.0;
    for (auto& module : m_root)
      moduleCodelength += module.codelength;
    hierarchicalCodelength = indexCodelength + moduleCodelength;
  }
  Log(1) << " with codelength: " << indexCodelength << " + " << (hierarchicalCodelength - indexCodelength) << " = " << io::toPrecision(hierarchicalCodelength) << '\n';

  double sumConsolidatedCodelength = hierarchicalCodelength - partitionQueue.moduleCodelength;

  const bool shouldMuteNestedMainRun = isMainInfomap() && !Log::isThreadMuted();
  bool isSilent = false;
  if (shouldMuteNestedMainRun) {
    isSilent = Log::isSilent();
  }

  while (partitionQueue.size() > 0) {
    Log(1) << "Level " << partitionQueue.level << ": " << (partitionQueue.flow * 100) << "% of the flow in " << partitionQueue.size() << " modules. Partitioning... " << std::setprecision(6) << std::flush;

    if (shouldMuteNestedMainRun)
      Log::setSilent(true);

    // Partition all modules in the queue and fill up the next level queue
    PartitionQueue nextLevelQueue;
    processPartitionQueue(partitionQueue, nextLevelQueue);

    if (shouldMuteNestedMainRun)
      Log::setSilent(isSilent);

    double leftToImprove = partitionQueue.moduleCodelength;
    sumConsolidatedCodelength += partitionQueue.indexCodelength + partitionQueue.leafCodelength;
    double limitCodelength = sumConsolidatedCodelength + leftToImprove;

    const double recursiveCompression = ((hierarchicalCodelength - limitCodelength) / hierarchicalCodelength) * 100;
    if (prettyOutput)
      prettyCompression.push_back(PrettyOutput::percent(recursiveCompression));
    Log(0, 0) << ((hierarchicalCodelength - limitCodelength) / hierarchicalCodelength) * 100 << "% " << std::flush;
    Log(1) << "done! Codelength: " << partitionQueue.indexCodelength << " + " << partitionQueue.leafCodelength << " (+ " << leftToImprove << " left to improve)"
           << " -> limit: " << io::toPrecision(limitCodelength) << " bits.\n";

    hierarchicalCodelength = limitCodelength;

    partitionQueue.swap(nextLevelQueue);
  }

  // Store resulting hierarchical codelength
  m_hierarchicalCodelength = hierarchicalCodelength;

  if (prettyOutput)
    PrettyOutput(true).status("Recursive", io::Str() << compressionSummary(prettyCompression) << " -> " << partitionQueue.level << " levels, codelength " << io::toPrecision(hierarchicalCodelength));
  Log(0, 0) << ". Found " << partitionQueue.level << " levels with codelength " << io::toPrecision(hierarchicalCodelength) << "\n";
  Log(1) << "  -> Found " << partitionQueue.level << " levels with codelength " << io::toPrecision(hierarchicalCodelength) << "\n";

  return partitionQueue.level;
}

void InfomapBase::queueTopModules(PartitionQueue& partitionQueue)
{
  // Add modules to partition queue
  unsigned int numNonTrivialModules = 0;
  partitionQueue.resize(numTopModules());
  double sumFlow = 0.0;
  double sumNonTrivialFlow = 0.0;
  double sumModuleCodelength = 0.0;
  unsigned int moduleIndex = 0;
  for (auto& module : m_root) {
    partitionQueue[moduleIndex] = &module;
    sumFlow += module.data.flow;
    sumModuleCodelength += module.codelength;
    if (module.childDegree() > 1) {
      ++numNonTrivialModules;
      sumNonTrivialFlow += module.data.flow;
    }
    ++moduleIndex;
  }
  partitionQueue.flow = sumFlow;
  partitionQueue.numNonTrivialModules = numNonTrivialModules;
  partitionQueue.nonTrivialFlow = sumNonTrivialFlow;
  partitionQueue.indexCodelength = getIndexCodelength();
  partitionQueue.moduleCodelength = sumModuleCodelength;
  partitionQueue.level = 1;
}

void InfomapBase::queueLeafModules(PartitionQueue& partitionQueue)
{
  unsigned int numLeafModules = 0;
  for (auto& node : m_root.infomapTree()) {
    if (node.isLeafModule())
      ++numLeafModules;
  }

  // Add modules to partition queue
  partitionQueue.resize(numLeafModules);
  unsigned int numNonTrivialModules = 0;
  double sumFlow = 0.0;
  double sumNonTrivialFlow = 0.0;
  double sumModuleCodelength = 0.0;
  unsigned int moduleIndex = 0;
  unsigned int maxDepth = 0;
  for (auto it = m_root.begin_tree(); !it.isEnd(); ++it) {
    if (it->isLeafModule()) {
      auto& module = *it;
      partitionQueue[moduleIndex] = it.current();
      sumFlow += module.data.flow;
      sumModuleCodelength += module.codelength;
      if (module.childDegree() > 1) {
        ++numNonTrivialModules;
        sumNonTrivialFlow += module.data.flow;
      }
      maxDepth = std::max(maxDepth, it.depth());
      ++moduleIndex;
    }
  }
  partitionQueue.flow = sumFlow;
  partitionQueue.numNonTrivialModules = numNonTrivialModules;
  partitionQueue.nonTrivialFlow = sumNonTrivialFlow;
  partitionQueue.indexCodelength = getIndexCodelength();
  partitionQueue.moduleCodelength = sumModuleCodelength;
  partitionQueue.level = maxDepth;
}

bool InfomapBase::processPartitionQueue(PartitionQueue& queue, PartitionQueue& nextLevelQueue) const
{
  PartitionQueue::size_t numModules = queue.size();
  std::vector<double> indexCodelengths(numModules, 0.0);
  std::vector<double> moduleCodelengths(numModules, 0.0);
  std::vector<double> leafCodelengths(numModules, 0.0);
  std::vector<PartitionQueue> subQueues(numModules);

#pragma omp parallel for schedule(dynamic)
  for (PartitionQueue::size_t moduleIndex = 0; moduleIndex < numModules; ++moduleIndex) {
    InfoNode& module = *queue[moduleIndex];

    module.codelength = calcCodelength(module);
    // Delete former sub-structure if exists
    if (module.disposeInfomap())
      module.codelength = calcCodelength(module);

    // If only trivial substructure is to be found, no need to create infomap instance to find sub-module structures.
    if (module.childDegree() <= 2) {
      module.codelength = calcCodelength(module);
      leafCodelengths[moduleIndex] = module.codelength;
      continue;
    }

    double oldModuleCodelength = module.codelength;
    PartitionQueue& subQueue = subQueues[moduleIndex];
    subQueue.level = queue.level + 1;

    auto& subInfomap = getSubInfomap(module)
                           .initNetwork(module);
    // Run two-level partition + find hierarchically super modules (skip recursion)
    subInfomap.setOnlySuperModules(true).run();

    double subCodelength = subInfomap.getHierarchicalCodelength();
    double subIndexCodelength = subInfomap.root().codelength;
    double subModuleCodelength = subCodelength - subIndexCodelength;
    InfoNode& subRoot = *module.getInfomapRoot();
    unsigned int numSubModules = subRoot.childDegree();
    bool trivialSubPartition = numSubModules == 1 || numSubModules == module.childDegree();
    bool improvedCodelength = subCodelength < oldModuleCodelength - minimumCodelengthImprovement;

    if (trivialSubPartition || !improvedCodelength) {
      Log(1) << "Disposing unaccepted sub Infomap instance.\n";
      module.disposeInfomap();
      module.codelength = oldModuleCodelength;
      subQueue.skip = true;
      leafCodelengths[moduleIndex] = module.codelength;
    } else {
      // Improvement
      subInfomap.queueTopModules(subQueue);
      indexCodelengths[moduleIndex] = subIndexCodelength;
      moduleCodelengths[moduleIndex] = subModuleCodelength;
    }
  }

  double sumLeafCodelength = 0.0;
  double sumIndexCodelength = 0.0;
  double sumModuleCodelengths = 0.0;
  PartitionQueue::size_t nextLevelSize = 0;
  for (PartitionQueue::size_t moduleIndex = 0; moduleIndex < numModules; ++moduleIndex) {
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
  for (PartitionQueue::size_t moduleIndex = 0; moduleIndex < numModules; ++moduleIndex) {
    PartitionQueue& subQueue = subQueues[moduleIndex];
    if (!subQueue.skip) {
      for (PartitionQueue::size_t subIndex = 0; subIndex < subQueue.size(); ++subIndex) {
        nextLevelQueue[nextLevelIndex++] = subQueue[subIndex];
      }
      nextLevelQueue.flow += subQueue.flow;
      nextLevelQueue.nonTrivialFlow += subQueue.nonTrivialFlow;
      nextLevelQueue.numNonTrivialModules += subQueue.numNonTrivialModules;
    }
  }

  return nextLevelSize > 0;
}

// ===================================================
// Write output
// ===================================================

void InfomapBase::writeResult(int trial)
{
  writeOutputArtifacts(*this, m_network, OutputPhase::AfterPartition, trial);
}

unsigned int printPerLevelCodelength(const InfoNode& parent, std::ostream& out, bool prettyOutput)
{
  std::vector<detail::PerLevelStat> perLevelStats;
  aggregatePerLevelCodelength(parent, perLevelStats);

  unsigned int numLevels = perLevelStats.size();

  if (prettyOutput) {
    std::vector<std::vector<std::string>> rows;
    rows.reserve(numLevels + 1);
    unsigned int sumNumModules = 0;
    unsigned int sumNumLeafNodes = 0;
    double sumAverageChildDegree = 0.0;
    double sumIndexLengths = 0.0;
    double sumLeafLengths = 0.0;
    double sumCodelengths = 0.0;
    for (unsigned int i = 0; i < numLevels; ++i) {
      double averageChildDegree = perLevelStats[i].numNodes();
      if (i > 0 && perLevelStats[i - 1].numModules > 0) {
        averageChildDegree = perLevelStats[i].numNodes() * 1.0 / perLevelStats[i - 1].numModules;
      }
      sumNumModules += perLevelStats[i].numModules;
      sumNumLeafNodes += perLevelStats[i].numLeafNodes;
      sumAverageChildDegree += averageChildDegree * perLevelStats[i].numNodes();
      sumIndexLengths += perLevelStats[i].indexLength;
      sumLeafLengths += perLevelStats[i].leafLength;
      sumCodelengths += perLevelStats[i].codelength();
      rows.push_back({
          io::Str() << (i + 1),
          io::Str() << perLevelStats[i].numModules,
          io::Str() << perLevelStats[i].numLeafNodes,
          io::toPrecision(averageChildDegree, 2, true),
          PrettyOutput::fixed(perLevelStats[i].indexLength),
          PrettyOutput::fixed(perLevelStats[i].leafLength),
          PrettyOutput::fixed(perLevelStats[i].codelength()),
      });
    }
    const unsigned int sumNumNodes = sumNumModules + sumNumLeafNodes;
    rows.push_back({
        "Total",
        io::Str() << sumNumModules,
        io::Str() << sumNumLeafNodes,
        sumNumNodes > 0 ? io::toPrecision(sumAverageChildDegree / sumNumNodes, 2, true) : "0",
        PrettyOutput::fixed(sumIndexLengths),
        PrettyOutput::fixed(sumLeafLengths),
        PrettyOutput::fixed(sumCodelengths),
    });
    out << "\nLevels\n";
    out << "  Level  Modules  Leaves  Avg degree  Module bits  Leaf bits  Total bits\n";
    for (const auto& row : rows) {
      out << "  " << std::setw(5) << row[0]
          << "  " << std::setw(7) << row[1]
          << "  " << std::setw(6) << row[2]
          << "  " << std::setw(10) << row[3]
          << "  " << std::setw(11) << row[4]
          << "  " << std::setw(9) << row[5]
          << "  " << std::setw(10) << row[6] << "\n";
    }
    return numLevels;
  }

  out << "Per level number of modules:         [";
  for (unsigned int i = 0; i < numLevels - 1; ++i) {
    out << io::padValue(perLevelStats[i].numModules, 11) << ", ";
  }
  out << io::padValue(perLevelStats[numLevels - 1].numModules, 11) << "]";
  unsigned int sumNumModules = 0;
  for (unsigned int i = 0; i < numLevels; ++i)
    sumNumModules += perLevelStats[i].numModules;
  out << " (sum: " << sumNumModules << ")\n";

  out << "Per level number of leaf nodes:      [";
  for (unsigned int i = 0; i < numLevels - 1; ++i) {
    out << io::padValue(perLevelStats[i].numLeafNodes, 11) << ", ";
  }
  out << io::padValue(perLevelStats[numLevels - 1].numLeafNodes, 11) << "]";
  unsigned int sumNumLeafNodes = 0;
  for (unsigned int i = 0; i < numLevels; ++i)
    sumNumLeafNodes += perLevelStats[i].numLeafNodes;
  out << " (sum: " << sumNumLeafNodes << ")\n";

  out << "Per level average child degree:      [";
  double childDegree = perLevelStats[0].numNodes();
  double sumAverageChildDegree = childDegree * childDegree;
  if (numLevels > 1) {
    out << io::padValue(perLevelStats[0].numModules, 11) << ", ";
  }
  for (unsigned int i = 1; i < numLevels - 1; ++i) {
    childDegree = perLevelStats[i].numNodes() * 1.0 / perLevelStats[i - 1].numModules;
    sumAverageChildDegree += childDegree * perLevelStats[i].numNodes();
    out << io::padValue(childDegree, 11) << ", ";
  }
  if (numLevels > 1) {
    childDegree = perLevelStats[numLevels - 1].numNodes() * 1.0 / perLevelStats[numLevels - 2].numModules;
    sumAverageChildDegree += childDegree * perLevelStats[numLevels - 1].numNodes();
  }
  out << io::padValue(childDegree, 11) << "]";
  out << " (average: " << sumAverageChildDegree / (sumNumModules + sumNumLeafNodes) << ")\n";

  std::ios::fmtflags oldFlags = out.flags();
  std::streamsize oldPrecision = out.precision();
  out << std::fixed << std::setprecision(9);
  out << "Per level codelength for modules:    [";
  for (unsigned int i = 0; i < numLevels - 1; ++i) {
    out << perLevelStats[i].indexLength << ", ";
  }
  out << perLevelStats[numLevels - 1].indexLength << "]";
  double sumIndexLengths = 0.0;
  for (unsigned int i = 0; i < numLevels; ++i)
    sumIndexLengths += perLevelStats[i].indexLength;
  out << " (sum: " << sumIndexLengths << ")\n";

  out << "Per level codelength for leaf nodes: [";
  for (unsigned int i = 0; i < numLevels - 1; ++i) {
    out << perLevelStats[i].leafLength << ", ";
  }
  out << perLevelStats[numLevels - 1].leafLength << "]";

  double sumLeafLengths = 0.0;
  for (unsigned int i = 0; i < numLevels; ++i)
    sumLeafLengths += perLevelStats[i].leafLength;
  out << " (sum: " << sumLeafLengths << ")\n";

  out << "Per level codelength total:          [";
  for (unsigned int i = 0; i < numLevels - 1; ++i) {
    out << perLevelStats[i].codelength() << ", ";
  }
  out << perLevelStats[numLevels - 1].codelength() << "]";

  double sumCodelengths = 0.0;
  for (unsigned int i = 0; i < numLevels; ++i)
    sumCodelengths += perLevelStats[i].codelength();
  out << " (sum: " << sumCodelengths << ")\n";
  out.flags(oldFlags);
  out << std::setprecision(oldPrecision);

  return numLevels;
}

void aggregatePerLevelCodelength(const InfoNode& parent, std::vector<detail::PerLevelStat>& perLevelStat, unsigned int level)
{
  if (perLevelStat.size() < level + 1)
    perLevelStat.resize(level + 1);

  if (parent.firstChild->isLeaf()) {
    perLevelStat[level].numLeafNodes += parent.childDegree();
    perLevelStat[level].leafLength += parent.codelength;
    return;
  }

  perLevelStat[level].numModules += parent.childDegree();
  perLevelStat[level].indexLength += parent.codelength;

  for (auto& module : parent) {
    if (module.getInfomapRoot() != nullptr)
      aggregatePerLevelCodelength(*module.getInfomapRoot(), perLevelStat, level + 1);
    else
      aggregatePerLevelCodelength(module, perLevelStat, level + 1);
  }
}

void InfomapBase::initOptimizer(bool forceNoMemory)
{
  if (haveMetaData()) {
    m_optimizer = std::make_unique<InfomapOptimizer<MetaMapEquation>>();
  } else if (haveMemory() && !forceNoMemory) {
#if INFOMAP_FEATURE_REGULARIZED_MULTILAYER
    if (isRegularizedMultilayerFlow()) {
      m_optimizer = std::make_unique<InfomapOptimizer<RegularizedMultilayerMapEquation>>();
    } else {
      m_optimizer = std::make_unique<InfomapOptimizer<MemMapEquation>>();
    }
#else
    m_optimizer = std::make_unique<InfomapOptimizer<MemMapEquation>>();
#endif
  } else {
    m_optimizer = std::make_unique<InfomapOptimizer<BiasedMapEquation>>();
  }
  m_optimizer->init(this);
}

} // namespace infomap
