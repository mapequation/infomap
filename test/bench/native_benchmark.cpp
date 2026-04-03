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

#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#endif

namespace {

std::string jsonEscape(const std::string& value)
{
  std::ostringstream escaped;
  for (char c : value) {
    switch (c) {
      case '\\': escaped << "\\\\"; break;
      case '"': escaped << "\\\""; break;
      case '\n': escaped << "\\n"; break;
      case '\r': escaped << "\\r"; break;
      case '\t': escaped << "\\t"; break;
      default: escaped << c; break;
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
  std::cerr << "Usage: " << argv0 << " --input <path> [--name <label>] [--flags <infomap flags>]\n";
}

} // namespace

int main(int argc, char* argv[])
{
  std::string inputPath;
  std::string caseName;
  std::string flags = "--silent --no-file-output --num-trials 1 --seed 123";

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--input" && i + 1 < argc) {
      inputPath = argv[++i];
    } else if (arg == "--name" && i + 1 < argc) {
      caseName = argv[++i];
    } else if (arg == "--flags" && i + 1 < argc) {
      flags = argv[++i];
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

  try {
    infomap::InfomapWrapper im(flags);

    infomap::Stopwatch readTimer(true);
    im.readInputData(inputPath);
    const double readInputSec = readTimer.getElapsedTimeInSec();

    const auto inputNodeCount = im.network().numNodes();
    const auto inputLinkCount = im.network().numLinks();
    const auto higherOrderInput = im.network().haveMemoryInput();
    const auto directedInput = im.network().haveDirectedInput();

    infomap::Stopwatch runTimer(true);
    im.run();
    const double runSec = runTimer.getElapsedTimeInSec();

    const auto& stats = im.benchmarkStats();
    const auto& activeMaterialization = im.activeGraphMaterialization();
    std::cout << "{";
    std::cout << "\"name\":\"" << jsonEscape(caseName) << "\",";
    std::cout << "\"path\":\"" << jsonEscape(inputPath) << "\",";
    std::cout << "\"backend_mode\":\"pointer\",";
    std::cout << "\"flags\":\"" << jsonEscape(flags) << "\",";
    std::cout << "\"read_input_sec\":" << readInputSec << ",";
    std::cout << "\"run_sec\":" << runSec << ",";
    std::cout << "\"total_sec\":" << (readInputSec + runSec) << ",";
    std::cout << "\"peak_rss_bytes\":" << peakRssBytes() << ",";
    std::cout << "\"node_size_bytes\":" << sizeof(infomap::InfoNode) << ",";
    std::cout << "\"edge_size_bytes\":" << sizeof(infomap::InfoEdge) << ",";
    std::cout << "\"active_payload_nodes\":" << activeMaterialization.payloads.size() << ",";
    std::cout << "\"active_payload_bytes\":" << activeMaterialization.payloadBytes() << ",";
    std::cout << "\"num_nodes\":" << inputNodeCount << ",";
    std::cout << "\"num_links\":" << inputLinkCount << ",";
    std::cout << "\"num_top_modules\":" << im.numTopModules() << ",";
    std::cout << "\"num_levels\":" << im.numLevels() << ",";
    std::cout << "\"codelength\":" << im.codelength() << ",";
    std::cout << "\"higher_order_input\":" << (higherOrderInput ? "true" : "false") << ",";
    std::cout << "\"directed_input\":" << (directedInput ? "true" : "false") << ",";
    std::cout << "\"benchmark_stats\":{";
    std::cout << "\"calculate_flow_sec\":" << stats.calculateFlowSec << ",";
    std::cout << "\"init_network_sec\":" << stats.initNetworkSec << ",";
    std::cout << "\"run_partition_sec\":" << stats.runPartitionSec << ",";
    std::cout << "\"find_top_modules_sec\":" << stats.findTopModulesSec << ",";
    std::cout << "\"fine_tune_sec\":" << stats.fineTuneSec << ",";
    std::cout << "\"coarse_tune_sec\":" << stats.coarseTuneSec << ",";
    std::cout << "\"recursive_partition_sec\":" << stats.recursivePartitionSec << ",";
    std::cout << "\"super_modules_sec\":" << stats.superModulesSec << ",";
    std::cout << "\"super_modules_fast_sec\":" << stats.superModulesFastSec << ",";
    std::cout << "\"find_top_modules_calls\":" << stats.findTopModulesCalls << ",";
    std::cout << "\"fine_tune_calls\":" << stats.fineTuneCalls << ",";
    std::cout << "\"coarse_tune_calls\":" << stats.coarseTuneCalls << ",";
    std::cout << "\"recursive_partition_calls\":" << stats.recursivePartitionCalls << ",";
    std::cout << "\"super_modules_calls\":" << stats.superModulesCalls << ",";
    std::cout << "\"super_modules_fast_calls\":" << stats.superModulesFastCalls << ",";
    std::cout << "\"consolidation_count\":" << stats.consolidationCount << ",";
    std::cout << "\"consolidations\":[";
    for (std::size_t i = 0; i < stats.consolidations.size(); ++i) {
      const auto& consolidation = stats.consolidations[i];
      if (i > 0) {
        std::cout << ",";
      }
      std::cout << "{";
      std::cout << "\"level\":" << consolidation.level << ",";
      std::cout << "\"active_node_count\":" << consolidation.activeNodeCount << ",";
      std::cout << "\"module_node_count\":" << consolidation.moduleNodeCount << ",";
      std::cout << "\"module_edge_count\":" << consolidation.moduleEdgeCount << ",";
      std::cout << "\"replace_existing_modules\":" << (consolidation.replaceExistingModules ? "true" : "false");
      std::cout << "}";
    }
    std::cout << "]";
    std::cout << "}";
    std::cout << "}\n";
  } catch (const std::exception& error) {
    std::cerr << "Error: " << error.what() << "\n";
    return 1;
  }

  return 0;
}
