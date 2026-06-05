#include "vendor/doctest.h"

#include "utils/ThreadConfig.h"

#include <string>

using infomap::resolveThreadBudget;
using infomap::ThreadBudget;
using infomap::ThreadSource;
using infomap::ThreadSources;
using infomap::threadSourceName;

TEST_CASE("Explicit --num-threads wins over every other source [fast][core][threads]")
{
  ThreadSources s;
  s.explicitThreads = 3;
  s.infomapEnv = 5;
  s.slurmCpusPerTask = 8;
  s.ompEnv = 16;
  s.cpusetCount = 32;
  s.hardwareConcurrency = 64;
  const ThreadBudget b = resolveThreadBudget(s);
  CHECK(b.threads == 3);
  CHECK(b.source == ThreadSource::Explicit);
}

TEST_CASE("Precedence falls through to the first present source [fast][core][threads]")
{
  ThreadSources s;
  s.slurmCpusPerTask = 8; // INFOMAP_NUM_THREADS absent, SLURM present
  s.ompEnv = 16;
  s.cpusetCount = 32;
  s.hardwareConcurrency = 64;
  const ThreadBudget b = resolveThreadBudget(s);
  CHECK(b.threads == 8);
  CHECK(b.source == ThreadSource::Slurm);
}

TEST_CASE("OMP_NUM_THREADS used when SLURM and INFOMAP unset [fast][core][threads]")
{
  ThreadSources s;
  s.ompEnv = 16;
  s.cpusetCount = 32;
  s.hardwareConcurrency = 64;
  const ThreadBudget b = resolveThreadBudget(s);
  CHECK(b.threads == 16);
  CHECK(b.source == ThreadSource::Omp);
}

TEST_CASE("cpuset used when env vars unset [fast][core][threads]")
{
  ThreadSources s;
  s.cpusetCount = 12;
  s.hardwareConcurrency = 64;
  const ThreadBudget b = resolveThreadBudget(s);
  CHECK(b.threads == 12);
  CHECK(b.source == ThreadSource::Cpuset);
}

TEST_CASE("hardware_concurrency is the final fallback [fast][core][threads]")
{
  ThreadSources s;
  s.hardwareConcurrency = 64;
  const ThreadBudget b = resolveThreadBudget(s);
  CHECK(b.threads == 64);
  CHECK(b.source == ThreadSource::Hardware);
}

TEST_CASE("Budget is clamped to at least 1 [fast][core][threads]")
{
  ThreadSources s; // all zero, hardwareConcurrency defaults to 1
  const ThreadBudget b = resolveThreadBudget(s);
  CHECK(b.threads == 1);
}

TEST_CASE("INFOMAP_NUM_THREADS wins when explicit is absent [fast][core][threads]")
{
  ThreadSources s;
  s.infomapEnv = 7;
  s.slurmCpusPerTask = 8;
  s.ompEnv = 16;
  s.cpusetCount = 32;
  s.hardwareConcurrency = 64;
  const ThreadBudget b = resolveThreadBudget(s);
  CHECK(b.threads == 7);
  CHECK(b.source == ThreadSource::InfomapEnv);
}

TEST_CASE("threadSourceName returns documented labels [fast][core][threads]")
{
  CHECK(std::string(threadSourceName(ThreadSource::Explicit)) == "--num-threads");
  CHECK(std::string(threadSourceName(ThreadSource::InfomapEnv)) == "INFOMAP_NUM_THREADS");
  CHECK(std::string(threadSourceName(ThreadSource::Slurm)) == "SLURM_CPUS_PER_TASK");
  CHECK(std::string(threadSourceName(ThreadSource::Omp)) == "OMP_NUM_THREADS");
  CHECK(std::string(threadSourceName(ThreadSource::Cpuset)) == "cpuset");
  CHECK(std::string(threadSourceName(ThreadSource::Hardware)) == "hardware_concurrency");
}
