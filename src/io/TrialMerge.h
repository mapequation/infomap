/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef TRIAL_MERGE_H_
#define TRIAL_MERGE_H_

#include "InfomapError.h"
#include <string>

namespace infomap {

struct Config;

// Read the shard result files named by config.mergeTrialResults
// (comma-separated, globs), validate fingerprints, select the global best
// trial, and write the requested output (tree/clu) from the winner's .tree.
// Returns the process exit code.
ExitCode mergeTrialResults(const Config& config);

} // namespace infomap

#endif // TRIAL_MERGE_H_
