#include "vendor/doctest.h"

#include "utils/ThreadConfig.h"

#include <cstdlib>
#include <string>

using infomap::readThreadSourcesFromEnv;
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

#if defined(__unix__) || defined(__APPLE__)
namespace {
// RAII save/restore of an env var so the test never leaks state into other cases.
struct ScopedEnv {
  std::string name;
  bool hadValue = false;
  std::string previous;
  explicit ScopedEnv(const char* envName) : name(envName)
  {
    if (const char* current = std::getenv(envName)) {
      hadValue = true;
      previous = current;
    }
  }
  void set(const char* value) const { setenv(name.c_str(), value, 1); }
  void clear() const { unsetenv(name.c_str()); }
  ~ScopedEnv()
  {
    if (hadValue) {
      setenv(name.c_str(), previous.c_str(), 1);
    } else {
      unsetenv(name.c_str());
    }
  }
};
} // namespace

TEST_CASE("readThreadSourcesFromEnv reads env vars and rejects trailing garbage [fast][core][threads]")
{
  ScopedEnv infomapEnv("INFOMAP_NUM_THREADS");
  ScopedEnv slurmEnv("SLURM_CPUS_PER_TASK");
  ScopedEnv ompEnv("OMP_NUM_THREADS");

  infomapEnv.set("7");
  slurmEnv.set("8");
  ompEnv.set("16");
  {
    const ThreadSources s = readThreadSourcesFromEnv();
    CHECK(s.infomapEnv == 7);
    CHECK(s.slurmCpusPerTask == 8);
    CHECK(s.ompEnv == 16);
    CHECK(s.hardwareConcurrency >= 1); // always present fallback
    // The env values feed precedence resolution: INFOMAP_NUM_THREADS wins here.
    CHECK(resolveThreadBudget(s).source == ThreadSource::InfomapEnv);
    CHECK(resolveThreadBudget(s).threads == 7);
  }

  // Trailing garbage and non-numeric values are treated as unset (0), not parsed loosely.
  infomapEnv.set("4x");
  slurmEnv.clear();
  ompEnv.clear();
  {
    const ThreadSources s = readThreadSourcesFromEnv();
    CHECK(s.infomapEnv == 0);
    CHECK(s.slurmCpusPerTask == 0);
    CHECK(s.ompEnv == 0);
  }
}
#endif
