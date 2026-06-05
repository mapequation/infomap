/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "ThreadConfig.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <string>
#include <thread>

#if defined(__linux__)
#include <sched.h>
#endif

namespace infomap {

ThreadBudget resolveThreadBudget(const ThreadSources& s)
{
  const auto pick = [](unsigned int value, ThreadSource source, ThreadBudget& out) {
    if (value > 0) {
      out.threads = value;
      out.source = source;
      return true;
    }
    return false;
  };

  ThreadBudget budget;
  if (pick(s.explicitThreads, ThreadSource::Explicit, budget)
      || pick(s.infomapEnv, ThreadSource::InfomapEnv, budget)
      || pick(s.slurmCpusPerTask, ThreadSource::Slurm, budget)
      || pick(s.ompEnv, ThreadSource::Omp, budget)
      || pick(s.cpusetCount, ThreadSource::Cpuset, budget)) {
    return budget;
  }
  budget.threads = std::max(1u, s.hardwareConcurrency);
  budget.source = ThreadSource::Hardware;
  return budget;
}

const char* threadSourceName(ThreadSource source)
{
  switch (source) {
    case ThreadSource::Explicit: return "--num-threads";
    case ThreadSource::InfomapEnv: return "INFOMAP_NUM_THREADS";
    case ThreadSource::Slurm: return "SLURM_CPUS_PER_TASK";
    case ThreadSource::Omp: return "OMP_NUM_THREADS";
    case ThreadSource::Cpuset: return "cpuset";
    case ThreadSource::Hardware: return "hardware_concurrency";
  }
  // unreachable — all enumerators handled above
  return "unknown";
}

namespace {
  unsigned int parseEnvCount(const char* name)
  {
    const char* raw = std::getenv(name);
    if (raw == nullptr) {
      return 0;
    }
    try {
      const long value = std::stol(std::string(raw));
      if (value > 0 && value <= static_cast<long>(std::numeric_limits<unsigned int>::max())) {
        return static_cast<unsigned int>(value);
      }
      return 0;
    } catch (...) {
      return 0;
    }
  }

  unsigned int readCpusetCount()
  {
#if defined(__linux__)
    cpu_set_t set;
    CPU_ZERO(&set);
    if (sched_getaffinity(0, sizeof(set), &set) == 0) {
      return static_cast<unsigned int>(CPU_COUNT(&set));
    }
#endif
    return 0;
  }
} // namespace

ThreadSources readThreadSourcesFromEnv()
{
  ThreadSources s;
  s.infomapEnv = parseEnvCount("INFOMAP_NUM_THREADS");
  s.slurmCpusPerTask = parseEnvCount("SLURM_CPUS_PER_TASK");
  s.ompEnv = parseEnvCount("OMP_NUM_THREADS");
  s.cpusetCount = readCpusetCount();
  s.hardwareConcurrency = std::max(1u, std::thread::hardware_concurrency());
  return s;
}

} // namespace infomap
