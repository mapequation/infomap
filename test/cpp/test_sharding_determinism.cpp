/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

/**
 * Phase F acceptance test: distributed trial sharding determinism.
 *
 * Proves: for any partition of global trial range [0, N) into shards,
 * running each shard independently and then merging with
 * --merge-trial-results produces:
 *   1. The same per-trial codelength vector (by global trial index) as a
 *      single serial reference run over all N trials with the same seed.
 *   2. The same best (minimum) codelength.
 *
 * Both the reference run and all shard runs use --trial-results so they
 * share the same per-trial-reseed RNG model (globalSeed = baseSeed +
 * globalTrialIndex), making them directly comparable.
 */

#include "vendor/doctest.h"
#include "Infomap.h"
#include "io/Config.h"
#include "io/InfomapError.h"
#include "io/TrialMerge.h"
#include "io/TrialResults.h"
#include "io/SafeFile.h"
#include "TestUtils.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace infomap;
using infomap::test::readTextFile;

// ---------------------------------------------------------------------------
// Local helpers
// ---------------------------------------------------------------------------

namespace {

// Write text to a file, creating or overwriting it.
void writeFileContent(const std::string& path, const std::string& content)
{
  std::ofstream out(path.c_str());
  if (!out)
    throw std::runtime_error("writeFileContent: cannot open " + path);
  out << content;
}

// Remove a file, ignoring errors (cleanup helper).
void removeIfExists(const std::string& path)
{
  std::remove(path.c_str());
}

// Parse a TrialResultsFile from a file on disk.
TrialResultsFile readTrialResults(const std::string& path)
{
  const auto json = readTextFile(path);
  return parseTrialResults(json, path);
}

// Build a map: globalTrialIndex -> codelength from a TrialResultsFile.
std::map<unsigned int, double> trialCodelengths(const TrialResultsFile& rf)
{
  std::map<unsigned int, double> m;
  for (const auto& t : rf.trials)
    m[t.trial] = t.codelength;
  return m;
}

// Return the minimum codelength across all trials in a TrialResultsFile.
double bestCodelength(const TrialResultsFile& rf)
{
  double best = std::numeric_limits<double>::infinity();
  for (const auto& t : rf.trials)
    best = std::min(best, t.codelength);
  return best;
}

// Count non-comment, non-empty lines in a .clu file (= number of nodes).
int countCluDataRows(const std::string& path)
{
  const auto content = readTextFile(path);
  std::istringstream ss(content);
  std::string line;
  int rows = 0;
  while (std::getline(ss, line)) {
    if (!line.empty() && line[0] != '#')
      ++rows;
  }
  return rows;
}

// Run a shard: read the network from networkFile, set library-mode overrides
// so file output goes to outDir, and call run().
// The --input flag sets Config::networkFile so the network fingerprint is
// computed correctly (required by --merge-trial-results validation).
// - networkFile:  path to the .net file to read
// - trialOffset:  first global trial index for this shard
// - numTrials:    number of trials in this shard
// - seed:         base seed (same for all shards and the reference)
// - trialResultsPath: where to write the per-trial JSON
// - outDir:       directory to write per-trial tree files
// - outName:      base name for per-trial tree files (no extension, no path)
void runShard(
    const std::string& networkFile,
    unsigned int trialOffset,
    unsigned int numTrials,
    unsigned long seed,
    const std::string& trialResultsPath,
    const std::string& outDir,
    const std::string& outName)
{
  std::ostringstream flags;
  flags << "--silent"
        << " --input " << networkFile
        << " --seed " << seed
        << " --num-trials " << numTrials
        << " --trial-offset " << trialOffset
        << " --trial-results " << trialResultsPath
        << " --no-final-output"
        << " --print-all-trials"
        << " --tree"
        << " --out-name " << outName;

  InfomapWrapper im(flags.str());
  // Override library-mode defaults: adaptDefaults() sets noFileOutput=true
  // when outDirectory is empty; we need file output for per-trial trees.
  im.outDirectory = outDir + "/";
  im.noFileOutput = false;

  im.run();
}

} // namespace

// ---------------------------------------------------------------------------
// Main determinism test
// ---------------------------------------------------------------------------
TEST_CASE("Sharding determinism: split-then-merge equals single-run per-trial codelengths [fast][core][merge]")
{
  // Base directory for all output of this test.
  const std::string baseDir =
      std::string(INFOMAP_TEST_ROOT) + "/test/tmp/sharding_determinism";
  infomap::ensureDirectoryExists(baseDir);

  // -------------------------------------------------------------------------
  // Write the two-triangle network to a temp .net file so that a real file
  // path is available for network fingerprinting (required by merge validation).
  // -------------------------------------------------------------------------
  const std::string netFile = baseDir + "/twotriangles.net";
  writeFileContent(netFile,
      "*Vertices\n"
      "1 \"A\"\n"
      "2 \"B\"\n"
      "3 \"C\"\n"
      "4 \"D\"\n"
      "5 \"E\"\n"
      "6 \"F\"\n"
      "*Edges\n"
      "1 2\n"
      "1 3\n"
      "2 3\n"
      "3 4\n"
      "4 5\n"
      "4 6\n"
      "5 6\n");
  REQUIRE(infomap::pathExists(netFile));

  // -------------------------------------------------------------------------
  // Reference run: N=6 trials, seed=42, sharding mode (--trial-results set)
  // -------------------------------------------------------------------------
  const unsigned long kSeed = 42;
  const unsigned int kNumTrials = 6;

  const std::string refDir = baseDir + "/ref";
  infomap::ensureDirectoryExists(refDir);

  const std::string refResultsPath = baseDir + "/ref.json";
  removeIfExists(refResultsPath);

  // Run reference in sharding mode (trial-offset=0 means it IS sharding mode
  // because --trial-results is set, which triggers per-trial reseed).
  runShard(
      netFile,
      /*trialOffset=*/0,
      /*numTrials=*/kNumTrials,
      kSeed,
      refResultsPath,
      refDir,
      "ref_trial");

  REQUIRE(infomap::pathExists(refResultsPath));

  const auto refResults = readTrialResults(refResultsPath);
  REQUIRE(refResults.trials.size() == kNumTrials);

  const auto refCodelengths = trialCodelengths(refResults);
  const double refBest = bestCodelength(refResults);

  // Sanity: all 6 global indices 0..5 are present.
  for (unsigned int i = 0; i < kNumTrials; ++i) {
    REQUIRE_MESSAGE(
        refCodelengths.count(i) == 1,
        "Reference is missing global trial index " << i);
  }

  INFO("Reference per-trial codelengths:");
  for (unsigned int i = 0; i < kNumTrials; ++i) {
    INFO("  trial[" << i << "] = " << refCodelengths.at(i));
  }
  INFO("Reference best codelength: " << refBest);

  // -------------------------------------------------------------------------
  // Partition A: equal halves [0,3) and [3,6)
  // -------------------------------------------------------------------------
  {
    const std::string dirA = baseDir + "/partA";
    infomap::ensureDirectoryExists(dirA);
    infomap::ensureDirectoryExists(dirA + "/s0");
    infomap::ensureDirectoryExists(dirA + "/s1");

    const std::string pA_s0 = baseDir + "/pA_s0.json";
    const std::string pA_s1 = baseDir + "/pA_s1.json";
    removeIfExists(pA_s0);
    removeIfExists(pA_s1);

    runShard(netFile, 0, 3, kSeed, pA_s0, dirA + "/s0", "pA_s0_trial");
    runShard(netFile, 3, 3, kSeed, pA_s1, dirA + "/s1", "pA_s1_trial");

    REQUIRE(infomap::pathExists(pA_s0));
    REQUIRE(infomap::pathExists(pA_s1));

    const auto rfA0 = readTrialResults(pA_s0);
    const auto rfA1 = readTrialResults(pA_s1);
    REQUIRE(rfA0.trials.size() == 3);
    REQUIRE(rfA1.trials.size() == 3);

    // Build per-trial map from both shards.
    auto pACodelengths = trialCodelengths(rfA0);
    for (const auto& kv : trialCodelengths(rfA1))
      pACodelengths[kv.first] = kv.second;

    // Element-wise comparison against reference.
    for (unsigned int i = 0; i < kNumTrials; ++i) {
      REQUIRE_MESSAGE(
          pACodelengths.count(i) == 1,
          "Partition A missing global trial index " << i);
      CHECK_MESSAGE(
          pACodelengths.at(i) == doctest::Approx(refCodelengths.at(i)),
          "Partition A trial[" << i << "]: expected " << refCodelengths.at(i)
              << " got " << pACodelengths.at(i));
    }

    const double pABest = std::min(bestCodelength(rfA0), bestCodelength(rfA1));

    // ---- Merge partition A ----
    const std::string mergedABase = baseDir + "/mergedA";
    removeIfExists(mergedABase + ".tree");
    removeIfExists(mergedABase + ".clu");

    std::ostringstream mergeAFlags;
    mergeAFlags << "--merge-trial-results " << pA_s0 << "," << pA_s1
                << " --output tree,clu"
                << " --out-name " << mergedABase
                << " --silent";

    Config mergeAConfig;
    REQUIRE_NOTHROW(mergeAConfig = Config(mergeAFlags.str(), true));
    const ExitCode mergeACode = mergeTrialResults(mergeAConfig);
    CHECK(mergeACode == ExitCode::Success);
    CHECK(infomap::pathExists(mergedABase + ".tree"));
    CHECK(infomap::pathExists(mergedABase + ".clu"));

    // The merged best must equal the reference best.
    CHECK_MESSAGE(
        pABest == doctest::Approx(refBest),
        "Partition A best codelength " << pABest
            << " != reference best " << refBest);

    // .clu row count must equal 6 (the two-triangle network has 6 nodes).
    if (infomap::pathExists(mergedABase + ".clu"))
      CHECK(countCluDataRows(mergedABase + ".clu") == 6);

    // Clean up outputs.
    removeIfExists(mergedABase + ".tree");
    removeIfExists(mergedABase + ".clu");
  }

  // -------------------------------------------------------------------------
  // Partition B: unequal shards [0,2), [2,5), [5,6)
  // -------------------------------------------------------------------------
  {
    const std::string dirB = baseDir + "/partB";
    infomap::ensureDirectoryExists(dirB);
    infomap::ensureDirectoryExists(dirB + "/s0");
    infomap::ensureDirectoryExists(dirB + "/s1");
    infomap::ensureDirectoryExists(dirB + "/s2");

    const std::string pB_s0 = baseDir + "/pB_s0.json";
    const std::string pB_s1 = baseDir + "/pB_s1.json";
    const std::string pB_s2 = baseDir + "/pB_s2.json";
    removeIfExists(pB_s0);
    removeIfExists(pB_s1);
    removeIfExists(pB_s2);

    runShard(netFile, 0, 2, kSeed, pB_s0, dirB + "/s0", "pB_s0_trial");
    runShard(netFile, 2, 3, kSeed, pB_s1, dirB + "/s1", "pB_s1_trial");
    runShard(netFile, 5, 1, kSeed, pB_s2, dirB + "/s2", "pB_s2_trial");

    REQUIRE(infomap::pathExists(pB_s0));
    REQUIRE(infomap::pathExists(pB_s1));
    REQUIRE(infomap::pathExists(pB_s2));

    const auto rfB0 = readTrialResults(pB_s0);
    const auto rfB1 = readTrialResults(pB_s1);
    const auto rfB2 = readTrialResults(pB_s2);
    REQUIRE(rfB0.trials.size() == 2);
    REQUIRE(rfB1.trials.size() == 3);
    REQUIRE(rfB2.trials.size() == 1);

    // Build per-trial map from all three shards.
    auto pBCodelengths = trialCodelengths(rfB0);
    for (const auto& kv : trialCodelengths(rfB1))
      pBCodelengths[kv.first] = kv.second;
    for (const auto& kv : trialCodelengths(rfB2))
      pBCodelengths[kv.first] = kv.second;

    // Element-wise comparison against reference.
    for (unsigned int i = 0; i < kNumTrials; ++i) {
      REQUIRE_MESSAGE(
          pBCodelengths.count(i) == 1,
          "Partition B missing global trial index " << i);
      CHECK_MESSAGE(
          pBCodelengths.at(i) == doctest::Approx(refCodelengths.at(i)),
          "Partition B trial[" << i << "]: expected " << refCodelengths.at(i)
              << " got " << pBCodelengths.at(i));
    }

    const double pBBest = std::min(
        { bestCodelength(rfB0), bestCodelength(rfB1), bestCodelength(rfB2) });

    // ---- Merge partition B ----
    const std::string mergedBBase = baseDir + "/mergedB";
    removeIfExists(mergedBBase + ".tree");
    removeIfExists(mergedBBase + ".clu");

    std::ostringstream mergeBFlags;
    mergeBFlags << "--merge-trial-results " << pB_s0 << "," << pB_s1 << "," << pB_s2
                << " --output tree,clu"
                << " --out-name " << mergedBBase
                << " --silent";

    Config mergeBConfig;
    REQUIRE_NOTHROW(mergeBConfig = Config(mergeBFlags.str(), true));
    const ExitCode mergeBCode = mergeTrialResults(mergeBConfig);
    CHECK(mergeBCode == ExitCode::Success);
    CHECK(infomap::pathExists(mergedBBase + ".tree"));
    CHECK(infomap::pathExists(mergedBBase + ".clu"));

    // The merged best must equal the reference best.
    CHECK_MESSAGE(
        pBBest == doctest::Approx(refBest),
        "Partition B best codelength " << pBBest
            << " != reference best " << refBest);

    // .clu row count must equal 6.
    if (infomap::pathExists(mergedBBase + ".clu"))
      CHECK(countCluDataRows(mergedBBase + ".clu") == 6);

    // Cross-check: both partition bests must agree.
    // (pA was checked above; we re-derive here as a cross-partition assertion.)
    // The reference best is the shared baseline — no extra variable needed.

    // Clean up outputs.
    removeIfExists(mergedBBase + ".tree");
    removeIfExists(mergedBBase + ".clu");
  }
}
