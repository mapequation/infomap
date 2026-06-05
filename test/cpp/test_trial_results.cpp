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

TEST_CASE("TrialResults serialize/parse round-trips [fast][core][merge]")
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
  const TrialResultsFile back = parseTrialResults(json, "results.json");

  CHECK(back.networkFingerprint == "abc123");
  CHECK(back.configFingerprint == "def456");
  CHECK(back.infomapVersion == "2.8.0");
  CHECK(back.baseSeed == 100);
  CHECK(back.trialOffset == 3);
  CHECK(back.numTrials == 2);
  CHECK(back.bestTreeFile == "out_trial_4.tree");
  REQUIRE(back.trials.size() == 2);
  CHECK(back.trials[0].trial == 3);
  CHECK(back.trials[0].seed == 103);
  CHECK(back.trials[0].codelength == doctest::Approx(6.5));
  CHECK(back.trials[0].numTopModules == 10);
  CHECK(back.trials[0].numLevels == 3);
  CHECK(back.trials[1].trial == 4);
  CHECK(back.trials[1].seed == 104);
  CHECK(back.trials[1].codelength == doctest::Approx(6.1));
  CHECK(back.trials[1].numTopModules == 12);
  CHECK(back.trials[1].numLevels == 4);
}

TEST_CASE("TrialResults parser tolerates whitespace and handles an empty trials array [fast][core][merge]")
{
  TrialResultsFile r;
  r.networkFingerprint = "h";
  r.configFingerprint = "c";
  r.infomapVersion = "v";
  const std::string json = serializeTrialResults(r);
  const TrialResultsFile back = parseTrialResults(json, "x.json");
  CHECK(back.trials.empty());
  CHECK(back.networkFingerprint == "h");
}

TEST_CASE("Shard run emits trial-results JSON and best-tree file; --no-final-output suppresses aggregate [fast][core][merge]")
{
  // Paths used by this test (in the CWD where the test binary runs).
  const std::string resultsPath = "tr_shard_results.json";
  const std::string outName = "tr_shard_out";
  // Per-trial tree files (trial_offset=5, 2 trials → global indices 5 and 6).
  const std::string trialTree5 = outName + "_trial_6.tree"; // global index 5, file uses 1-based (+1)
  const std::string trialTree6 = outName + "_trial_7.tree"; // global index 6, file uses 1-based (+1)
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
    // Also reset noFileOutput: adaptDefaults sets it to true when outDirectory is
    // empty in non-CLI mode, so we override it after construction.
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

  // The results file must exist and have content.
  REQUIRE(infomap::pathExists(resultsPath));

  const auto json = readTextFile(resultsPath);
  const TrialResultsFile parsed = parseTrialResults(json, resultsPath);

  CHECK(parsed.trialOffset == 5);
  CHECK(parsed.numTrials == 2);
  REQUIRE(parsed.trials.size() == 2);

  // Global trial indices must be 5 and 6 (offset 5 + local slots 0 and 1).
  CHECK(parsed.trials[0].trial == 5);
  CHECK(parsed.trials[1].trial == 6);

  // bestTreeFile must be non-empty.
  CHECK(!parsed.bestTreeFile.empty());

  // The referenced best tree file must exist on disk.
  // bestTreeFile is relative to the results file directory (CWD here).
  CHECK(infomap::pathExists(parsed.bestTreeFile));

  // The aggregate final output must NOT have been written (--no-final-output).
  CHECK(!infomap::pathExists(aggregatePath));

  // Clean up.
  std::remove(resultsPath.c_str());
  std::remove(trialTree5.c_str());
  std::remove(trialTree6.c_str());
  std::remove(aggregatePath.c_str());
}
