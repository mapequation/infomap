#include <nlohmann/json.hpp>

#include "Infomap.h"
#include "core/InfoEdge.h"
#include "core/InfoNode.h"
#include "utils/Stopwatch.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#endif

namespace {

using Json = nlohmann::ordered_json;

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
};

Json runSampleJson(const RunSample& sample)
{
  Json json;
  json["iteration"] = sample.iteration;
  json["read_input_sec"] = sample.readInputSec;
  json["run_sec"] = sample.runSec;
  json["total_sec"] = sample.totalSec;
  json["peak_rss_bytes"] = sample.peakRssBytes;
  json["num_top_modules"] = sample.numTopModules;
  json["num_levels"] = sample.numLevels;
  json["codelength"] = sample.codelength;
  return json;
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

    for (int iteration = 0; iteration < iterations; ++iteration) {
      infomap::Stopwatch runTimer(true);
      im.run();
      const double runSec = runTimer.getElapsedTimeInSec();
      const auto currentPeakRss = peakRssBytes();
      totalRunSec += runSec;
      if (currentPeakRss > peakRss) {
        peakRss = currentPeakRss;
      }
      runs.push_back(RunSample {
          static_cast<unsigned int>(iteration + 1),
          0.0,
          runSec,
          runSec,
          currentPeakRss,
          im.numTopModules(),
          im.numLevels(),
          im.codelength(),
      });
    }

    Json result;
    result["name"] = caseName;
    result["path"] = inputPath;
    result["flags"] = flags;
    result["iterations"] = iterations;
    result["read_input_sec"] = readInputSec;
    result["run_sec"] = totalRunSec;
    result["total_sec"] = readInputSec + totalRunSec;
    result["peak_rss_bytes"] = peakRss;
    result["node_size_bytes"] = sizeof(infomap::InfoNode);
    result["edge_size_bytes"] = sizeof(infomap::InfoEdge);
    result["num_nodes"] = inputNodeCount;
    result["num_links"] = inputLinkCount;
    result["num_top_modules"] = im.numTopModules();
    result["num_levels"] = im.numLevels();
    result["codelength"] = im.codelength();
    result["higher_order_input"] = higherOrderInput;
    result["directed_input"] = directedInput;

    Json runItems = Json::array();
    for (const auto& run : runs) {
      runItems.push_back(runSampleJson(run));
    }
    result["runs"] = std::move(runItems);
    std::cout << result.dump() << '\n';
  } catch (const std::exception& error) {
    std::cerr << "Error: " << error.what() << "\n";
    return 1;
  }

  return 0;
}
