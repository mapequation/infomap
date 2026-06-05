/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef TRIAL_RESULTS_H_
#define TRIAL_RESULTS_H_

#include <string>
#include <vector>

namespace infomap {

struct TrialResultEntry {
  unsigned int trial = 0; // global trial index (0-based)
  unsigned long seed = 0;
  double codelength = 0.0;
  unsigned int numTopModules = 0;
  unsigned int numLevels = 0;
  int thread = 0;
  double timeSec = 0.0;
};

struct TrialResultsFile {
  std::string networkFingerprint;
  std::string configFingerprint;
  std::string infomapVersion;
  unsigned long baseSeed = 0;
  unsigned int trialOffset = 0;
  unsigned int numTrials = 0;
  std::string bestTreeFile; // path relative to this results file's directory
  std::vector<TrialResultEntry> trials;
};

std::string serializeTrialResults(const TrialResultsFile& results);

// sourcePath is the path of the JSON file being parsed (used only for error messages).
TrialResultsFile parseTrialResults(const std::string& json, const std::string& sourcePath);

} // namespace infomap

#endif // TRIAL_RESULTS_H_
