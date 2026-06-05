/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "TrialMerge.h"
#include "ClusterMap.h"
#include "Config.h"
#include "InfomapError.h"
#include "SafeFile.h"
#include "TrialResults.h"
#include "../utils/FileURI.h"
#include "../utils/Log.h"
#include "../utils/convert.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef _WIN32
#include <glob.h>
#endif

namespace infomap {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

  // Read a file into a string, throws on failure.
  std::string readFileMerge(const std::string& path)
  {
    SafeInFile input(path);
    std::ostringstream buf;
    buf << input.rdbuf();
    return buf.str();
  }

  // Expand a single glob pattern into a list of matching paths.
  // If the pattern contains no wildcard characters and the path exists literally,
  // return it directly without calling glob. Returns an empty vector when a
  // wildcard pattern matches nothing.
  std::vector<std::string> expandGlob(const std::string& pattern)
  {
    // Check for wildcard characters.
    const bool hasWildcard = pattern.find_first_of("*?[") != std::string::npos;

    if (!hasWildcard) {
      // Literal path: just check existence.
      if (pathExists(pattern)) {
        return { pattern };
      }
      return {};
    }

#ifndef _WIN32
    ::glob_t globResult;
    int ret = ::glob(pattern.c_str(), GLOB_TILDE, nullptr, &globResult);
    std::vector<std::string> paths;
    if (ret == 0) {
      for (size_t i = 0; i < globResult.gl_pathc; ++i) {
        paths.emplace_back(globResult.gl_pathv[i]);
      }
    }
    ::globfree(&globResult);
    return paths;
#else
    // Windows: no POSIX glob. Literal paths only (already handled above).
    return {};
#endif
  }

  // Return the directory part of a file path (including trailing slash).
  std::string dirOfPath(const std::string& filePath)
  {
    const auto sep = filePath.find_last_of("/\\");
    if (sep == std::string::npos)
      return "./";
    return filePath.substr(0, sep + 1);
  }

  // Resolve bestTreeFile relative to the shard results file directory.
  std::string resolveBestTree(const std::string& bestTreeFile, const std::string& shardJsonPath)
  {
    // If bestTreeFile is absolute, use as-is.
    if (!bestTreeFile.empty() && (bestTreeFile[0] == '/' || bestTreeFile[0] == '\\'))
      return bestTreeFile;
    // Otherwise, resolve relative to the directory of the shard JSON.
    return dirOfPath(shardJsonPath) + bestTreeFile;
  }

  // Copy a file verbatim, using atomic SafeOutFile + commit.
  void copyFileAtomic(const std::string& srcPath, const std::string& dstPath)
  {
    SafeInFile src(srcPath);
    SafeOutFile dst(dstPath, std::ios_base::out, true /* overwrite */);
    dst << src.rdbuf();
    dst.commit();
  }

  // Write .clu from a parsed ClusterMap (physical level, non-multilayer,
  // non-state). Format matches writeClu() in Output.cpp:
  //   # node_id module flow
  //   physicalId moduleId flow
  void writeCluFromClusterMap(const std::string& cluPath, const ClusterMap& cm)
  {
    SafeOutFile outFile(cluPath, std::ios_base::out, true /* overwrite */);
    outFile << std::setprecision(6);
    outFile << "# node_id module flow\n";

    const auto& clusterIds = cm.clusterIds();
    const auto& flowData = cm.flowData();

    for (const auto& kv : clusterIds) {
      const unsigned int nodeId = kv.first;
      const unsigned int moduleId = kv.second;
      double flow = 0.0;
      auto it = flowData.find(nodeId);
      if (it != flowData.end())
        flow = it->second;
      outFile << nodeId << " " << moduleId << " " << flow << "\n";
    }
    outFile.commit();
  }

} // namespace

// ---------------------------------------------------------------------------
// mergeTrialResults
// ---------------------------------------------------------------------------

ExitCode mergeTrialResults(const Config& config)
{
  // --- 1. Split comma-separated patterns and expand globs ---

  const std::string& patterns = config.mergeTrialResults;
  if (patterns.empty()) {
    std::cerr << "Error: --merge-trial-results requires at least one file pattern.\n";
    return ExitCode::InvalidArguments;
  }

  // Split on commas.
  std::vector<std::string> patternList;
  {
    std::istringstream ss(patterns);
    std::string token;
    while (std::getline(ss, token, ',')) {
      // Trim whitespace.
      while (!token.empty() && (token.front() == ' ' || token.front() == '\t'))
        token.erase(token.begin());
      while (!token.empty() && (token.back() == ' ' || token.back() == '\t'))
        token.pop_back();
      if (!token.empty())
        patternList.push_back(token);
    }
  }

  // Expand patterns to absolute paths.
  std::vector<std::string> shardPaths;
  for (const auto& pat : patternList) {
    const bool hasWildcard = pat.find_first_of("*?[") != std::string::npos;
    auto expanded = expandGlob(pat);
    if (expanded.empty()) {
      if (hasWildcard) {
        std::cerr << "Error: --merge-trial-results pattern '" << pat << "' matched no files.\n";
      } else {
        std::cerr << "Error: --merge-trial-results file not found: '" << pat << "'.\n";
      }
      return ExitCode::InputError;
    }
    for (auto& p : expanded)
      shardPaths.push_back(std::move(p));
  }

  if (shardPaths.empty()) {
    std::cerr << "Error: --merge-trial-results: no shard files found.\n";
    return ExitCode::InputError;
  }

  // --- 2. Parse shard files ---

  struct ShardData {
    TrialResultsFile results;
    std::string jsonPath;
  };

  std::vector<ShardData> shards;
  shards.reserve(shardPaths.size());

  for (const auto& jsonPath : shardPaths) {
    std::string json;
    try {
      json = readFileMerge(jsonPath);
    } catch (const std::exception& e) {
      std::cerr << "Error reading shard file '" << jsonPath << "': " << e.what() << "\n";
      return ExitCode::InputError;
    }

    TrialResultsFile rf;
    try {
      rf = parseTrialResults(json, jsonPath);
    } catch (const std::exception& e) {
      std::cerr << "Error parsing shard file '" << jsonPath << "': " << e.what() << "\n";
      return ExitCode::InputError;
    }

    shards.push_back({ std::move(rf), jsonPath });
  }

  // --- 3. Validate fingerprints ---

  // First shard provides the expected fingerprints.
  const std::string& expectedNetFp = shards[0].results.networkFingerprint;
  const std::string& expectedCfgFp = shards[0].results.configFingerprint;

  if (expectedNetFp.empty()) {
    std::cerr << "Error: shard file '" << shards[0].jsonPath
               << "' has an empty network_fingerprint. Cannot validate run identity.\n";
    return ExitCode::InputError;
  }
  if (expectedCfgFp.empty()) {
    std::cerr << "Error: shard file '" << shards[0].jsonPath
               << "' has an empty config_fingerprint. Cannot validate run identity.\n";
    return ExitCode::InputError;
  }

  for (size_t i = 1; i < shards.size(); ++i) {
    const auto& rf = shards[i].results;
    if (rf.networkFingerprint.empty()) {
      std::cerr << "Error: shard file '" << shards[i].jsonPath
                 << "' has an empty network_fingerprint.\n";
      return ExitCode::InputError;
    }
    if (rf.configFingerprint.empty()) {
      std::cerr << "Error: shard file '" << shards[i].jsonPath
                 << "' has an empty config_fingerprint.\n";
      return ExitCode::InputError;
    }
    if (rf.networkFingerprint != expectedNetFp) {
      std::cerr << "Error: network_fingerprint mismatch between shards.\n"
                 << "  Shard 0 (" << shards[0].jsonPath << "): " << expectedNetFp << "\n"
                 << "  Shard " << i << " (" << shards[i].jsonPath << "): " << rf.networkFingerprint << "\n";
      return ExitCode::InputError;
    }
    if (rf.configFingerprint != expectedCfgFp) {
      std::cerr << "Error: config_fingerprint mismatch between shards.\n"
                 << "  Shard 0 (" << shards[0].jsonPath << "): " << expectedCfgFp << "\n"
                 << "  Shard " << i << " (" << shards[i].jsonPath << "): " << rf.configFingerprint << "\n";
      return ExitCode::InputError;
    }
  }

  // --- 4. Validate requested output formats ---
  // Merge supports only tree and clu. Reject any other format requested.
  if (config.printFlowTree || config.printNewick || config.printJson || config.printCsv
      || config.printPajekNetwork || config.printStateNetwork || config.printFlowNetwork) {
    std::cerr << "Error: --merge-trial-results supports only 'tree' and 'clu' output formats. "
               << "Other formats require a live network and are not supported in merge mode.\n";
    return ExitCode::InvalidArguments;
  }

  // --- 5. Aggregate all trials and select winner ---

  struct WinnerInfo {
    double codelength = std::numeric_limits<double>::infinity();
    unsigned int trial = std::numeric_limits<unsigned int>::max();
    size_t shardIndex = 0;
    std::string bestTreeFile; // resolved absolute path
  };

  WinnerInfo winner;
  std::set<unsigned int> coveredTrials;

  for (size_t si = 0; si < shards.size(); ++si) {
    const auto& rf = shards[si].results;

    for (const auto& entry : rf.trials) {
      coveredTrials.insert(entry.trial);

      bool isBetter = entry.codelength < winner.codelength
          || (entry.codelength == winner.codelength && entry.trial < winner.trial);

      if (isBetter) {
        winner.codelength = entry.codelength;
        winner.trial = entry.trial;
        winner.shardIndex = si;
      }
    }
  }

  if (coveredTrials.empty()) {
    std::cerr << "Error: no trial results found across all shard files.\n";
    return ExitCode::InputError;
  }

  // Resolve the winner's bestTreeFile relative to its shard JSON.
  const auto& winnerShard = shards[winner.shardIndex];
  winner.bestTreeFile = resolveBestTree(winnerShard.results.bestTreeFile, winnerShard.jsonPath);

  if (!pathExists(winner.bestTreeFile)) {
    std::cerr << "Error: winner's best tree file does not exist: '" << winner.bestTreeFile << "'.\n";
    return ExitCode::InputError;
  }

  // --- 6. Completeness check ---

  const unsigned int maxTrial = *coveredTrials.rbegin();
  std::vector<unsigned int> missingTrials;
  for (unsigned int i = 0; i <= maxTrial; ++i) {
    if (coveredTrials.find(i) == coveredTrials.end())
      missingTrials.push_back(i);
  }

  if (!missingTrials.empty()) {
    if (config.requireCompleteTrials) {
      std::cerr << "Error: --require-complete-trials: missing "
                 << missingTrials.size() << " global trial index(es) in [0, "
                 << maxTrial << "]. Missing:";
      for (auto idx : missingTrials)
        std::cerr << " " << idx;
      std::cerr << "\n";
      return ExitCode::InputError;
    } else {
      Log() << "Warning: missing " << missingTrials.size()
            << " global trial index(es) in [0, " << maxTrial << "]. Missing:";
      for (auto idx : missingTrials)
        Log() << " " << idx;
      Log() << "\n";
    }
  }

  // --- 7. Write outputs ---

  const std::string outBase = config.outDirectory + config.outName;

  // Ensure output directory exists.
  if (!config.outDirectory.empty()) {
    try {
      ensureDirectoryExists(config.outDirectory);
    } catch (const std::exception& e) {
      std::cerr << "Error: " << e.what() << "\n";
      return ExitCode::OutputError;
    }
  }

  if (config.printTree) {
    const std::string treePath = outBase + ".tree";
    try {
      copyFileAtomic(winner.bestTreeFile, treePath);
    } catch (const std::exception& e) {
      std::cerr << "Error writing merged tree to '" << treePath << "': " << e.what() << "\n";
      return ExitCode::OutputError;
    }
  }

  if (config.printClu) {
    const std::string cluPath = outBase + ".clu";
    try {
      ClusterMap cm;
      cm.readClusterData(winner.bestTreeFile, true /* includeFlow */);
      writeCluFromClusterMap(cluPath, cm);
    } catch (const std::exception& e) {
      std::cerr << "Error writing merged clu to '" << cluPath << "': " << e.what() << "\n";
      return ExitCode::OutputError;
    }
  }

  Log() << "Merge complete. Winner: global trial " << winner.trial
        << " with codelength " << winner.codelength
        << " from '" << winnerShard.jsonPath << "'.\n";

  return ExitCode::Success;
}

} // namespace infomap
