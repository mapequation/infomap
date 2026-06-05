/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "vendor/doctest.h"
#include "io/TrialMerge.h"
#include "io/TrialResults.h"
#include "io/Config.h"
#include "io/InfomapError.h"
#include "io/SafeFile.h"
#include "TestUtils.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using namespace infomap;
using infomap::test::readTextFile;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

// Write text to a file, creating or overwriting it.
void writeFile(const std::string& path, const std::string& content)
{
  std::ofstream out(path);
  if (!out)
    throw std::runtime_error("writeFile: cannot open " + path);
  out << content;
}

// Minimal valid .tree file content with n leaf nodes in two modules.
// Uses physical-id format (4 columns: path flow name node_id).
std::string makeTinyTree(double codelength = 2.0, unsigned int numNodes = 4)
{
  std::ostringstream ss;
  ss << "# v2.8.0\n"
     << "# ./Infomap test.net .\n"
     << "# started at 2024-01-01\n"
     << "# completed in 0.001 s\n"
     << "# partitioned into 2 levels with 2 top modules\n"
     << "# codelength " << codelength << " bits\n"
     << "# relative codelength savings 5%\n"
     << "# flow model undirected\n"
     << "# path flow name node_id\n";
  // Two modules, each with numNodes/2 nodes.
  unsigned int half = numNodes / 2 < 1 ? 1 : numNodes / 2;
  for (unsigned int i = 0; i < half; ++i) {
    ss << "1:" << (i + 1) << " " << (1.0 / numNodes) << " \"" << (i + 1) << "\" " << (i + 1) << "\n";
  }
  for (unsigned int i = half; i < numNodes; ++i) {
    ss << "2:" << (i - half + 1) << " " << (1.0 / numNodes) << " \"" << (i + 1) << "\" " << (i + 1) << "\n";
  }
  return ss.str();
}

// Build a TrialResultsFile and its referenced best-tree file in a temp dir.
// Returns the path to the written JSON results file.
std::string writeShardFixture(
    const std::string& dir,
    const std::string& jsonName,
    const std::string& treeName,
    const std::string& networkFp,
    const std::string& configFp,
    unsigned int trialOffset,
    unsigned int numTrials,
    const std::vector<TrialResultEntry>& trials)
{
  const std::string jsonPath = dir + "/" + jsonName;
  const std::string treePath = dir + "/" + treeName;

  // Find winner (min codelength) for bestTreeFile.
  double bestCL = trials.empty() ? 99.0 : trials[0].codelength;
  for (const auto& t : trials) {
    if (t.codelength < bestCL)
      bestCL = t.codelength;
  }

  writeFile(treePath, makeTinyTree(bestCL));

  TrialResultsFile rf;
  rf.networkFingerprint = networkFp;
  rf.configFingerprint = configFp;
  rf.infomapVersion = "2.8.0";
  rf.baseSeed = 123;
  rf.trialOffset = trialOffset;
  rf.numTrials = numTrials;
  rf.bestTreeFile = treeName; // bare filename — resolved relative to json dir

  for (const auto& t : trials) {
    rf.trials.push_back(t);
  }

  writeFile(jsonPath, serializeTrialResults(rf));
  return jsonPath;
}

// Build a minimal Config for merge-mode without parsing (avoids the positional
// requirement for network_file). We set fields directly.
Config makeMergeConfig(
    const std::string& mergeGlob,
    const std::string& outDir,
    const std::string& outName,
    bool requireComplete = false,
    bool printTree = true,
    bool printClu = true)
{
  Config config;
  config.isCLI = false;
  config.mergeTrialResults = mergeGlob;
  config.requireCompleteTrials = requireComplete;
  config.outDirectory = outDir.back() == '/' ? outDir : outDir + "/";
  config.outName = outName;
  config.printTree = printTree;
  config.printClu = printClu;
  config.noFileOutput = false;
  config.silent = true;
  return config;
}

} // namespace

// ---------------------------------------------------------------------------
// Test (a): two valid shards → Success + winner's tree/clu written
// ---------------------------------------------------------------------------
TEST_CASE("mergeTrialResults: two valid shards picks winner and writes outputs [fast][core][merge]")
{
  const std::string dir = std::string(INFOMAP_TEST_ROOT) + "/test/tmp/merge_valid";
  infomap::ensureDirectoryExists(dir);

  const std::string netFp = "nfp_abc123";
  const std::string cfgFp = "cfg_def456";

  // Shard A: trials 0,1 with codelengths 3.5, 3.2 → best = trial 1 (3.2)
  auto pathA = writeShardFixture(dir, "shardA.json", "shardA_best.tree",
      netFp, cfgFp, 0, 2,
      { { 0, 100, 3.5, 2, 2, 0, 0.1 }, { 1, 101, 3.2, 2, 2, 0, 0.1 } });

  // Shard B: trials 2,3 with codelengths 4.0, 2.8 → best = trial 3 (2.8)
  auto pathB = writeShardFixture(dir, "shardB.json", "shardB_best.tree",
      netFp, cfgFp, 2, 2,
      { { 2, 102, 4.0, 2, 2, 0, 0.1 }, { 3, 103, 2.8, 2, 2, 0, 0.1 } });

  const std::string outDir = dir + "/out";
  infomap::ensureDirectoryExists(outDir);

  // Clean up output files before.
  std::remove((outDir + "/merged.tree").c_str());
  std::remove((outDir + "/merged.clu").c_str());

  auto config = makeMergeConfig(pathA + "," + pathB, outDir, "merged");

  const ExitCode code = mergeTrialResults(config);

  CHECK(code == ExitCode::Success);
  CHECK(infomap::pathExists(outDir + "/merged.tree"));
  CHECK(infomap::pathExists(outDir + "/merged.clu"));

  // The written tree should come from shardB_best.tree (codelength 2.8 < 3.2).
  // Verify the codelength line in the output tree matches 2.8.
  const auto treeContent = readTextFile(outDir + "/merged.tree");
  CHECK(treeContent.find("2.8") != std::string::npos);

  // Clean up.
  std::remove((outDir + "/merged.tree").c_str());
  std::remove((outDir + "/merged.clu").c_str());
}

// ---------------------------------------------------------------------------
// Test (b): mismatched configFingerprint → InputError
// ---------------------------------------------------------------------------
TEST_CASE("mergeTrialResults: mismatched configFingerprint returns InputError [fast][core][merge]")
{
  const std::string dir = std::string(INFOMAP_TEST_ROOT) + "/test/tmp/merge_cfp_mismatch";
  infomap::ensureDirectoryExists(dir);

  auto pathA = writeShardFixture(dir, "shardA.json", "shardA.tree",
      "netfp_same", "cfp_AAA", 0, 1,
      { { 0, 1, 3.0, 2, 2, 0, 0.1 } });
  auto pathB = writeShardFixture(dir, "shardB.json", "shardB.tree",
      "netfp_same", "cfp_BBB", 1, 1,
      { { 1, 2, 2.5, 2, 2, 0, 0.1 } });

  const std::string outDir = dir + "/out";
  infomap::ensureDirectoryExists(outDir);

  auto config = makeMergeConfig(pathA + "," + pathB, outDir, "merged_cfp");
  const ExitCode code = mergeTrialResults(config);

  CHECK(code == ExitCode::InputError);
  CHECK_FALSE(infomap::pathExists(outDir + "/merged_cfp.tree"));
  CHECK_FALSE(infomap::pathExists(outDir + "/merged_cfp.clu"));
}

// ---------------------------------------------------------------------------
// Test (c): mismatched networkFingerprint → InputError
// ---------------------------------------------------------------------------
TEST_CASE("mergeTrialResults: mismatched networkFingerprint returns InputError [fast][core][merge]")
{
  const std::string dir = std::string(INFOMAP_TEST_ROOT) + "/test/tmp/merge_nfp_mismatch";
  infomap::ensureDirectoryExists(dir);

  auto pathA = writeShardFixture(dir, "shardA.json", "shardA.tree",
      "netfp_AAA", "cfp_same", 0, 1,
      { { 0, 1, 3.0, 2, 2, 0, 0.1 } });
  auto pathB = writeShardFixture(dir, "shardB.json", "shardB.tree",
      "netfp_BBB", "cfp_same", 1, 1,
      { { 1, 2, 2.5, 2, 2, 0, 0.1 } });

  const std::string outDir = dir + "/out";
  infomap::ensureDirectoryExists(outDir);

  auto config = makeMergeConfig(pathA + "," + pathB, outDir, "merged_nfp");
  const ExitCode code = mergeTrialResults(config);

  CHECK(code == ExitCode::InputError);
  CHECK_FALSE(infomap::pathExists(outDir + "/merged_nfp.tree"));
}

// ---------------------------------------------------------------------------
// Test (d): empty networkFingerprint in any shard → InputError
// ---------------------------------------------------------------------------
TEST_CASE("mergeTrialResults: empty networkFingerprint in any shard returns InputError [fast][core][merge]")
{
  const std::string dir = std::string(INFOMAP_TEST_ROOT) + "/test/tmp/merge_empty_nfp";
  infomap::ensureDirectoryExists(dir);

  // Shard A has valid fingerprints.
  auto pathA = writeShardFixture(dir, "shardA.json", "shardA.tree",
      "netfp_valid", "cfp_valid", 0, 1,
      { { 0, 1, 3.0, 2, 2, 0, 0.1 } });

  // Shard B has empty networkFingerprint.
  auto pathB = writeShardFixture(dir, "shardB.json", "shardB.tree",
      "", "cfp_valid", 1, 1,
      { { 1, 2, 2.5, 2, 2, 0, 0.1 } });

  const std::string outDir = dir + "/out";
  infomap::ensureDirectoryExists(outDir);

  auto config = makeMergeConfig(pathA + "," + pathB, outDir, "merged_empty_nfp");
  const ExitCode code = mergeTrialResults(config);

  // Must NOT treat empty as matching anything.
  CHECK(code == ExitCode::InputError);
  CHECK_FALSE(infomap::pathExists(outDir + "/merged_empty_nfp.tree"));
}

// ---------------------------------------------------------------------------
// Test (e): missing trial index
// ---------------------------------------------------------------------------
TEST_CASE("mergeTrialResults: missing global trial index — warning by default, error with requireCompleteTrials [fast][core][merge]")
{
  const std::string dir = std::string(INFOMAP_TEST_ROOT) + "/test/tmp/merge_gaps";
  infomap::ensureDirectoryExists(dir);

  const std::string netFp = "nfp_gap";
  const std::string cfgFp = "cfg_gap";

  // Two shards covering global trials 0 and 2 (gap at index 1).
  auto pathA = writeShardFixture(dir, "shardA.json", "shardA_best.tree",
      netFp, cfgFp, 0, 1,
      { { 0, 100, 3.0, 2, 2, 0, 0.1 } });
  auto pathB = writeShardFixture(dir, "shardB.json", "shardB_best.tree",
      netFp, cfgFp, 2, 1,
      { { 2, 102, 2.5, 2, 2, 0, 0.1 } });

  {
    const std::string outDir = dir + "/out_warn";
    infomap::ensureDirectoryExists(outDir);
    std::remove((outDir + "/merged_gap.tree").c_str());

    auto config = makeMergeConfig(pathA + "," + pathB, outDir, "merged_gap", false);
    const ExitCode code = mergeTrialResults(config);
    CHECK(code == ExitCode::Success);
    CHECK(infomap::pathExists(outDir + "/merged_gap.tree"));
    std::remove((outDir + "/merged_gap.tree").c_str());
  }

  {
    const std::string outDir = dir + "/out_strict";
    infomap::ensureDirectoryExists(outDir);
    std::remove((outDir + "/merged_gap_strict.tree").c_str());

    auto config = makeMergeConfig(pathA + "," + pathB, outDir, "merged_gap_strict", true);
    const ExitCode code = mergeTrialResults(config);
    CHECK(code == ExitCode::InputError);
    CHECK_FALSE(infomap::pathExists(outDir + "/merged_gap_strict.tree"));
  }
}
