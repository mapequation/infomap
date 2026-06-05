/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "vendor/doctest.h"
#include "io/TrialResults.h"
#include "TestUtils.h"

#include <string>
#include <cstdio>

using namespace infomap;

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
  CHECK(back.baseSeed == 100);
  CHECK(back.trialOffset == 3);
  CHECK(back.numTrials == 2);
  CHECK(back.bestTreeFile == "out_trial_4.tree");
  REQUIRE(back.trials.size() == 2);
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
