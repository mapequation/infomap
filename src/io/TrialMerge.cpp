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
#include "../utils/Log.h"

#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
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
  void copyFileAtomic(const std::string& srcPath, const std::string& dstPath, bool overwrite)
  {
    SafeInFile src(srcPath);
    SafeOutFile dst(dstPath, std::ios_base::out, overwrite);
    dst << src.rdbuf();
    dst.commit();
  }

  // Write .clu from a parsed ClusterMap (physical level, non-multilayer,
  // non-state). Format matches writeClu() in Output.cpp:
  //   # produced by --merge-trial-results
  //   # module level 1
  //   # node_id module flow
  //   physicalId moduleId flow
  //
  // Module ids are derived from treePaths(): path[0] is the 1-based top-level
  // module id. Flow is taken from flowData() keyed by nodeId (defaults to 0.0
  // if absent).
  void writeCluFromClusterMap(const std::string& cluPath, const ClusterMap& cm, bool overwrite)
  {
    SafeOutFile outFile(cluPath, std::ios_base::out, overwrite);
    outFile << std::setprecision(9);
    outFile << "# produced by --merge-trial-results\n";
    outFile << "# module level 1\n";
    outFile << std::resetiosflags(std::ios::floatfield) << std::setprecision(6);
    outFile << "# node_id module flow\n";

    const auto& paths = cm.treePaths();
    const auto& flowData = cm.flowData();

    for (const auto& tp : paths) {
      if (tp.path.empty())
        continue; // defensive: skip malformed entries
      const unsigned int nodeId = tp.nodeId;
      const unsigned int moduleId = tp.path.front(); // top-level module (1-based)
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
    Log::important() << "Error: --merge-trial-results requires at least one file pattern.\n";
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
        Log::important() << "Error: --merge-trial-results pattern '" << pat << "' matched no files.\n";
      } else {
        Log::important() << "Error: --merge-trial-results file not found: '" << pat << "'.\n";
      }
      return ExitCode::InputError;
    }
    for (auto& p : expanded)
      shardPaths.push_back(std::move(p));
  }

  if (shardPaths.empty()) {
    Log::important() << "Error: --merge-trial-results: no shard files found.\n";
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
      Log::important() << "Error reading shard file '" << jsonPath << "': " << e.what() << "\n";
      return ExitCode::InputError;
    }

    TrialResultsFile rf;
    try {
      rf = parseTrialResults(json, jsonPath);
    } catch (const std::exception& e) {
      Log::important() << "Error parsing shard file '" << jsonPath << "': " << e.what() << "\n";
      return ExitCode::InputError;
    }

    shards.push_back({ std::move(rf), jsonPath });
  }

  // --- 3. Validate fingerprints ---

  // First shard provides the expected fingerprints.
  const std::string& expectedNetFp = shards[0].results.networkFingerprint;
  const std::string& expectedCfgFp = shards[0].results.configFingerprint;

  if (expectedNetFp.empty()) {
    Log::important() << "Error: shard file '" << shards[0].jsonPath
               << "' has an empty network_fingerprint. Cannot validate run identity.\n";
    return ExitCode::InputError;
  }
  if (expectedCfgFp.empty()) {
    Log::important() << "Error: shard file '" << shards[0].jsonPath
               << "' has an empty config_fingerprint. Cannot validate run identity.\n";
    return ExitCode::InputError;
  }

  for (size_t i = 1; i < shards.size(); ++i) {
    const auto& rf = shards[i].results;
    if (rf.networkFingerprint.empty()) {
      Log::important() << "Error: shard file '" << shards[i].jsonPath
                 << "' has an empty network_fingerprint.\n";
      return ExitCode::InputError;
    }
    if (rf.configFingerprint.empty()) {
      Log::important() << "Error: shard file '" << shards[i].jsonPath
                 << "' has an empty config_fingerprint.\n";
      return ExitCode::InputError;
    }
    if (rf.networkFingerprint != expectedNetFp) {
      Log::important() << "Error: network_fingerprint mismatch between shards.\n"
                 << "  Shard 0 (" << shards[0].jsonPath << "): " << expectedNetFp << "\n"
                 << "  Shard " << i << " (" << shards[i].jsonPath << "): " << rf.networkFingerprint << "\n";
      return ExitCode::InputError;
    }
    if (rf.configFingerprint != expectedCfgFp) {
      Log::important() << "Error: config_fingerprint mismatch between shards.\n"
                 << "  Shard 0 (" << shards[0].jsonPath << "): " << expectedCfgFp << "\n"
                 << "  Shard " << i << " (" << shards[i].jsonPath << "): " << rf.configFingerprint << "\n";
      return ExitCode::InputError;
    }
  }

  // --- 4. Validate requested output formats ---
  // Merge supports only tree and clu. Reject any other format requested.
  if (config.printFlowTree || config.printNewick || config.printJson || config.printCsv
      || config.printPajekNetwork || config.printStateNetwork || config.printFlowNetwork) {
    Log::important() << "Error: --merge-trial-results supports only 'tree' and 'clu' output formats. "
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
    Log::important() << "Error: no trial results found across all shard files.\n";
    return ExitCode::InputError;
  }

  // Resolve the winner's bestTreeFile relative to its shard JSON.
  const auto& winnerShard = shards[winner.shardIndex];

  // A shard run without tree output records an empty best_tree_file; resolving it
  // would yield the shard directory (which pathExists accepts), so reject it early
  // with a clear message instead of failing later with a confusing I/O error.
  if (winnerShard.results.bestTreeFile.empty()) {
    Log::important() << "Error: winning shard '" << winnerShard.jsonPath
                     << "' recorded no best tree file; re-run shards with tree output enabled.\n";
    return ExitCode::InputError;
  }

  winner.bestTreeFile = resolveBestTree(winnerShard.results.bestTreeFile, winnerShard.jsonPath);

  if (!pathExists(winner.bestTreeFile)) {
    Log::important() << "Error: winner's best tree file does not exist: '" << winner.bestTreeFile << "'.\n";
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
      std::ostringstream missing;
      missing << "Error: --require-complete-trials: missing "
              << missingTrials.size() << " global trial index(es) in [0, "
              << maxTrial << "]. Missing:";
      for (auto idx : missingTrials)
        missing << " " << idx;
      Log::important() << missing.str() << "\n";
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

  // Ensure output directory exists. When outDirectory is empty but outName
  // contains a path separator, create the parent directory of outName so
  // that callers can specify a full path via --out-name without a positional
  // out_directory argument (useful in merge mode from the CLI).
  {
    std::string dirToCreate = config.outDirectory;
    if (dirToCreate.empty()) {
      const auto sep = outBase.find_last_of("/\\");
      if (sep != std::string::npos) {
        dirToCreate = outBase.substr(0, sep + 1);
      }
    }
    if (!dirToCreate.empty()) {
      try {
        ensureDirectoryExists(dirToCreate);
      } catch (const std::exception& e) {
        Log::important() << "Error: " << e.what() << "\n";
        return ExitCode::OutputError;
      }
    }
  }

  if (config.printTree) {
    const std::string treePath = outBase + ".tree";
    try {
      copyFileAtomic(winner.bestTreeFile, treePath, config.overwriteOutput());
    } catch (const std::exception& e) {
      Log::important() << "Error writing merged tree to '" << treePath << "': " << e.what() << "\n";
      return ExitCode::OutputError;
    }
  }

  if (config.printClu) {
    const std::string cluPath = outBase + ".clu";
    try {
      ClusterMap cm;
      cm.readClusterData(winner.bestTreeFile, true /* includeFlow */);
      writeCluFromClusterMap(cluPath, cm, config.overwriteOutput());
    } catch (const std::exception& e) {
      Log::important() << "Error writing merged clu to '" << cluPath << "': " << e.what() << "\n";
      return ExitCode::OutputError;
    }
  }

  Log() << "Merge complete. Winner: global trial " << winner.trial
        << " with codelength " << winner.codelength
        << " from '" << winnerShard.jsonPath << "'.\n";

  return ExitCode::Success;
}

} // namespace infomap
