#include "Infomap.h"
#include "core/InfoEdge.h"
#include "core/InfoNode.h"
#include "utils/Stopwatch.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <array>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#endif

namespace {

std::string jsonEscape(const std::string& value)
{
  std::ostringstream escaped;
  for (char c : value) {
    switch (c) {
    case '\\':
      escaped << "\\\\";
      break;
    case '"':
      escaped << "\\\"";
      break;
    case '\n':
      escaped << "\\n";
      break;
    case '\r':
      escaped << "\\r";
      break;
    case '\t':
      escaped << "\\t";
      break;
    default:
      escaped << c;
      break;
    }
  }
  return escaped.str();
}

unsigned long long peakRssBytes()
{
#if defined(__unix__) || defined(__APPLE__)
  struct rusage usage {};
  if (getrusage(RUSAGE_SELF, &usage) != 0) {
    return 0;
  }
#if defined(__APPLE__)
  return static_cast<unsigned long long>(usage.ru_maxrss);
#else
  return static_cast<unsigned long long>(usage.ru_maxrss) * 1024ULL;
#endif
#else
  return 0;
#endif
}

void printUsage(const char* argv0)
{
  std::cerr << "Usage: " << argv0 << " --input <path> [--name <label>] [--flags <infomap flags>] [--iterations <count>]\n";
}

struct RunSample {
  unsigned int iteration = 0;
  double readInputSec = 0.0;
  double runSec = 0.0;
  double totalSec = 0.0;
  unsigned long long peakRssBytes = 0;
  unsigned int numTopModules = 0;
  unsigned int numLevels = 0;
  double codelength = 0.0;
  infomap::InfomapBase::RebuildBenchmarkStats rebuildStats;
};

constexpr std::array<const char*, infomap::InfomapBase::RebuildBenchmarkStats::moduleSizeBucketCount> kModuleSizeBucketLabels = {
  "1-2",
  "3-4",
  "5-8",
  "9-16",
  "17-32",
  "33-64",
  "65+",
};

void aggregateRebuildStats(
    infomap::InfomapBase::RebuildBenchmarkStats& total,
    const infomap::InfomapBase::RebuildBenchmarkStats& sample)
{
  total.networkCalls += sample.networkCalls;
  total.moduleCalls += sample.moduleCalls;
  total.totalCalls += sample.totalCalls;
  total.networkSec += sample.networkSec;
  total.moduleSec += sample.moduleSec;
  total.modulePrepSec += sample.modulePrepSec;
  total.moduleIndexSec += sample.moduleIndexSec;
  total.moduleReserveSec += sample.moduleReserveSec;
  total.moduleCloneSec += sample.moduleCloneSec;
  total.moduleEdgeCloneSec += sample.moduleEdgeCloneSec;
  total.totalSec += sample.totalSec;
  if (sample.peakRssBytesMax > total.peakRssBytesMax) {
    total.peakRssBytesMax = sample.peakRssBytesMax;
  }
  if (sample.peakRssDeltaBytesMax > total.peakRssDeltaBytesMax) {
    total.peakRssDeltaBytesMax = sample.peakRssDeltaBytesMax;
  }
  for (std::size_t i = 0; i < total.moduleSizeBucketCalls.size(); ++i) {
    total.moduleSizeBucketCalls[i] += sample.moduleSizeBucketCalls[i];
    total.moduleSizeBucketSec[i] += sample.moduleSizeBucketSec[i];
    total.moduleSizeBucketPrepSec[i] += sample.moduleSizeBucketPrepSec[i];
    total.moduleSizeBucketIndexSec[i] += sample.moduleSizeBucketIndexSec[i];
    total.moduleSizeBucketReserveSec[i] += sample.moduleSizeBucketReserveSec[i];
    total.moduleSizeBucketCloneSec[i] += sample.moduleSizeBucketCloneSec[i];
    total.moduleSizeBucketEdgeCloneSec[i] += sample.moduleSizeBucketEdgeCloneSec[i];
  }
}

void printRebuildStats(const infomap::InfomapBase::RebuildBenchmarkStats& stats)
{
  std::cout << "{";
  std::cout << "\"network_calls\":" << stats.networkCalls << ",";
  std::cout << "\"module_calls\":" << stats.moduleCalls << ",";
  std::cout << "\"total_calls\":" << stats.totalCalls << ",";
  std::cout << "\"network_sec\":" << stats.networkSec << ",";
  std::cout << "\"module_sec\":" << stats.moduleSec << ",";
  std::cout << "\"module_prep_sec\":" << stats.modulePrepSec << ",";
  std::cout << "\"module_index_sec\":" << stats.moduleIndexSec << ",";
  std::cout << "\"module_reserve_sec\":" << stats.moduleReserveSec << ",";
  std::cout << "\"module_clone_sec\":" << stats.moduleCloneSec << ",";
  std::cout << "\"module_edge_clone_sec\":" << stats.moduleEdgeCloneSec << ",";
  std::cout << "\"total_sec\":" << stats.totalSec << ",";
  std::cout << "\"peak_rss_bytes_max\":" << stats.peakRssBytesMax << ",";
  std::cout << "\"peak_rss_delta_bytes_max\":" << stats.peakRssDeltaBytesMax << ",";
  std::cout << "\"module_size_buckets\":{";
  for (std::size_t i = 0; i < kModuleSizeBucketLabels.size(); ++i) {
    if (i > 0) {
      std::cout << ",";
    }
    std::cout << "\"" << kModuleSizeBucketLabels[i] << "\":{";
    std::cout << "\"calls\":" << stats.moduleSizeBucketCalls[i] << ",";
    std::cout << "\"sec\":" << stats.moduleSizeBucketSec[i] << ",";
    std::cout << "\"prep_sec\":" << stats.moduleSizeBucketPrepSec[i] << ",";
    std::cout << "\"index_sec\":" << stats.moduleSizeBucketIndexSec[i] << ",";
    std::cout << "\"reserve_sec\":" << stats.moduleSizeBucketReserveSec[i] << ",";
    std::cout << "\"clone_sec\":" << stats.moduleSizeBucketCloneSec[i] << ",";
    std::cout << "\"edge_clone_sec\":" << stats.moduleSizeBucketEdgeCloneSec[i];
    std::cout << "}";
  }
  std::cout << "}";
  std::cout << "}";
}

void printRunSample(const RunSample& sample)
{
  std::cout << "{";
  std::cout << "\"iteration\":" << sample.iteration << ",";
  std::cout << "\"read_input_sec\":" << sample.readInputSec << ",";
  std::cout << "\"run_sec\":" << sample.runSec << ",";
  std::cout << "\"total_sec\":" << sample.totalSec << ",";
  std::cout << "\"peak_rss_bytes\":" << sample.peakRssBytes << ",";
  std::cout << "\"num_top_modules\":" << sample.numTopModules << ",";
  std::cout << "\"num_levels\":" << sample.numLevels << ",";
  std::cout << "\"codelength\":" << sample.codelength << ",";
  std::cout << "\"rebuild\":";
  printRebuildStats(sample.rebuildStats);
  std::cout << "}";
}

} // namespace

int main(int argc, char* argv[])
{
  std::string inputPath;
  std::string caseName;
  std::string flags = "--silent --no-file-output --num-trials 1 --seed 123";
  int iterations = 1;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--input" && i + 1 < argc) {
      inputPath = argv[++i];
    } else if (arg == "--name" && i + 1 < argc) {
      caseName = argv[++i];
    } else if (arg == "--flags" && i + 1 < argc) {
      flags = argv[++i];
    } else if (arg == "--iterations" && i + 1 < argc) {
      try {
        iterations = std::stoi(argv[++i]);
      } catch (const std::exception&) {
        std::cerr << "Invalid --iterations value: " << argv[i] << "\n";
        return 2;
      }
    } else if (arg == "--help" || arg == "-h") {
      printUsage(argv[0]);
      return 0;
    } else {
      std::cerr << "Unknown or incomplete argument: " << arg << "\n";
      printUsage(argv[0]);
      return 2;
    }
  }

  if (inputPath.empty()) {
    printUsage(argv[0]);
    return 2;
  }
  if (caseName.empty()) {
    caseName = inputPath;
  }
  if (iterations < 1) {
    std::cerr << "Error: --iterations must be at least 1\n";
    return 2;
  }

  try {
    infomap::InfomapWrapper im(flags);

    infomap::Stopwatch readTimer(true);
    im.readInputData(inputPath);
    const double readInputSec = readTimer.getElapsedTimeInSec();

    const auto inputNodeCount = im.network().numNodes();
    const auto inputLinkCount = im.network().numLinks();
    const auto higherOrderInput = im.network().haveMemoryInput();
    const auto directedInput = im.network().haveDirectedInput();

    std::vector<RunSample> runs;
    runs.reserve(static_cast<std::size_t>(iterations));
    double totalRunSec = 0.0;
    unsigned long long peakRss = 0;
    infomap::InfomapBase::RebuildBenchmarkStats totalRebuildStats;

    for (int iteration = 0; iteration < iterations; ++iteration) {
      infomap::Stopwatch runTimer(true);
      im.run();
      const double runSec = runTimer.getElapsedTimeInSec();
      const auto currentPeakRss = peakRssBytes();
      const auto runRebuildStats = im.getRebuildBenchmarkStats();
      totalRunSec += runSec;
      if (currentPeakRss > peakRss) {
        peakRss = currentPeakRss;
      }
      aggregateRebuildStats(totalRebuildStats, runRebuildStats);
      runs.push_back(RunSample {
          static_cast<unsigned int>(iteration + 1),
          0.0,
          runSec,
          runSec,
          currentPeakRss,
          im.numTopModules(),
          im.numLevels(),
          im.codelength(),
          runRebuildStats,
      });
    }

    std::cout << "{";
    std::cout << "\"name\":\"" << jsonEscape(caseName) << "\",";
    std::cout << "\"path\":\"" << jsonEscape(inputPath) << "\",";
    std::cout << "\"flags\":\"" << jsonEscape(flags) << "\",";
    std::cout << "\"iterations\":" << iterations << ",";
    std::cout << "\"read_input_sec\":" << readInputSec << ",";
    std::cout << "\"run_sec\":" << totalRunSec << ",";
    std::cout << "\"total_sec\":" << (readInputSec + totalRunSec) << ",";
    std::cout << "\"peak_rss_bytes\":" << peakRss << ",";
    std::cout << "\"node_size_bytes\":" << sizeof(infomap::InfoNode) << ",";
    std::cout << "\"edge_size_bytes\":" << sizeof(infomap::InfoEdge) << ",";
    std::cout << "\"num_nodes\":" << inputNodeCount << ",";
    std::cout << "\"num_links\":" << inputLinkCount << ",";
    std::cout << "\"num_top_modules\":" << im.numTopModules() << ",";
    std::cout << "\"num_levels\":" << im.numLevels() << ",";
    std::cout << "\"codelength\":" << im.codelength() << ",";
    std::cout << "\"higher_order_input\":" << (higherOrderInput ? "true" : "false") << ",";
    std::cout << "\"directed_input\":" << (directedInput ? "true" : "false") << ",";
    std::cout << "\"rebuild\":";
    printRebuildStats(totalRebuildStats);
    std::cout << ",";
    std::cout << "\"runs\":[";
    for (std::size_t i = 0; i < runs.size(); ++i) {
      if (i > 0) {
        std::cout << ",";
      }
      printRunSample(runs[i]);
    }
    std::cout << "]";
    std::cout << "}\n";
  } catch (const std::exception& error) {
    std::cerr << "Error: " << error.what() << "\n";
    return 1;
  }

  return 0;
}
