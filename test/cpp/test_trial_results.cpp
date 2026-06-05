/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "vendor/doctest.h"
#include "io/TrialResults.h"
#include "io/SafeFile.h"
#include "Infomap.h"
#include "TestUtils.h"

#include <string>
#include <cstdio>

using namespace infomap;
using infomap::test::readTextFile;

TEST_CASE("serializeTrialResults emits the documented JSON fields [fast][core][merge]")
{
  TrialResultsFile r;
  r.networkFingerprint = "abc123";
  r.configFingerprint = "def456";
  r.infomapVersion = "2.8.0";
  r.baseSeed = 100;
  r.trialOffset = 3;
  r.numTrials = 2;
  r.bestTreeFile = "out_trial_4.tree";
  r.trials.push_back({ 3, 103, 6.5, 10, 3, 0, 1.2 });
  r.trials.push_back({ 4, 104, 6.1, 12, 4, 1, 1.3 });

  const std::string json = serializeTrialResults(r);

  // Run-identity header.
  CHECK(json.find("\"network_fingerprint\":\"abc123\"") != std::string::npos);
  CHECK(json.find("\"config_fingerprint\":\"def456\"") != std::string::npos);
  CHECK(json.find("\"infomap_version\":\"2.8.0\"") != std::string::npos);
  CHECK(json.find("\"base_seed\":100") != std::string::npos);
  CHECK(json.find("\"trial_offset\":3") != std::string::npos);
  CHECK(json.find("\"num_trials\":2") != std::string::npos);
  CHECK(json.find("\"best_tree_file\":\"out_trial_4.tree\"") != std::string::npos);

  // Per-trial entries (global indices, codelength, levels).
  CHECK(json.find("\"trial\":3") != std::string::npos);
  CHECK(json.find("\"trial\":4") != std::string::npos);
  CHECK(json.find("\"seed\":104") != std::string::npos);
  CHECK(json.find("\"num_levels\":4") != std::string::npos);

  // An empty trials array serializes to a valid (empty) array.
  TrialResultsFile empty;
  empty.networkFingerprint = "h";
  const std::string emptyJson = serializeTrialResults(empty);
  CHECK(emptyJson.find("\"trials\":[]") != std::string::npos);
}

TEST_CASE("Shard run emits trial-results JSON and best-tree file; --no-final-output suppresses aggregate [fast][core][merge]")
{
  // Paths used by this test (in the CWD where the test binary runs).
  const std::string resultsPath = "tr_shard_results.json";
  const std::string outName = "tr_shard_out";
  // Per-trial tree files (trial_offset=5, 2 trials → global indices 5 and 6;
  // file names use the 1-based trial number, i.e. global index + 1).
  const std::string trialTree5 = outName + "_trial_6.tree"; // global index 5
  const std::string trialTree6 = outName + "_trial_7.tree"; // global index 6
  const std::string aggregatePath = outName + ".tree";

  // Clean up before.
  std::remove(resultsPath.c_str());
  std::remove(trialTree5.c_str());
  std::remove(trialTree6.c_str());
  std::remove(aggregatePath.c_str());

  {
    InfomapWrapper im("--silent --seed 7 --num-trials 2 --trial-offset 5"
                      " --trial-results " + resultsPath
                      + " --no-final-output"
                      + " --tree --out-name " + outName);
    // Set output directory explicitly so library mode writes tree files to CWD.
    // adaptDefaults sets noFileOutput=true when outDirectory is empty in non-CLI
    // mode, so we override both after construction.
    im.outDirectory = "./";
    im.noFileOutput = false;
    // Two-triangle network: nodes 0-2 and 3-5.
    im.addLink(0, 1);
    im.addLink(1, 2);
    im.addLink(2, 0);
    im.addLink(3, 4);
    im.addLink(4, 5);
    im.addLink(5, 3);
    im.run();
  }

  // The results file must exist and have content. Verify via string checks on
  // the emitted JSON (the JSON is read back by the separate Python merge tool,
  // not by the C++ binary, so there is no C++ parser to rely on here).
  REQUIRE(infomap::pathExists(resultsPath));
  const auto json = readTextFile(resultsPath);

  CHECK(json.find("\"trial_offset\":5") != std::string::npos);
  CHECK(json.find("\"num_trials\":2") != std::string::npos);
  // Global trial indices must be 5 and 6 (offset 5 + local slots 0 and 1).
  CHECK(json.find("\"trial\":5") != std::string::npos);
  CHECK(json.find("\"trial\":6") != std::string::npos);
  // best_tree_file must be recorded and non-empty.
  CHECK(json.find("\"best_tree_file\":\"") != std::string::npos);
  CHECK(json.find("\"best_tree_file\":\"\"") == std::string::npos);
  // The winning trial's tree must exist on disk (only the best trial's tree is
  // written without --print-all-trials, so exactly one of these is present).
  CHECK((infomap::pathExists(trialTree5) || infomap::pathExists(trialTree6)));
  // The aggregate final output must NOT have been written (--no-final-output).
  CHECK(!infomap::pathExists(aggregatePath));

  // Clean up.
  std::remove(resultsPath.c_str());
  std::remove(trialTree5.c_str());
  std::remove(trialTree6.c_str());
  std::remove(aggregatePath.c_str());
}
