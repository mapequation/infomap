/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "TrialResults.h"

#include <iomanip>
#include <sstream>

namespace infomap {

namespace {

  // Local JSON string escaper — identical pattern to RunReport.cpp / ParameterCatalog.cpp.
  std::string jsonString(const std::string& value)
  {
    std::ostringstream escaped;
    escaped << '"';
    for (char c : value) {
      switch (c) {
      case '"':
      case '\\':
        escaped << '\\' << c;
        break;
      case '\b':
        escaped << "\\b";
        break;
      case '\f':
        escaped << "\\f";
        break;
      case '\n':
        escaped << "\\n";
        break;
      case '\r':
        escaped << "\\r";
        break;
      case '\t':
        escaped << "\\t";
        break;
      default:
        escaped << c;
        break;
      }
    }
    escaped << '"';
    return escaped.str();
  }


} // namespace

// ---------------------------------------------------------------------------
// serializeTrialResults
// ---------------------------------------------------------------------------

std::string serializeTrialResults(const TrialResultsFile& r)
{
  std::ostringstream out;
  out << std::setprecision(12);
  out << '{'
      << "\"network_fingerprint\":" << jsonString(r.networkFingerprint) << ','
      << "\"config_fingerprint\":" << jsonString(r.configFingerprint) << ','
      << "\"infomap_version\":" << jsonString(r.infomapVersion) << ','
      << "\"base_seed\":" << r.baseSeed << ','
      << "\"trial_offset\":" << r.trialOffset << ','
      << "\"num_trials\":" << r.numTrials << ','
      << "\"best_tree_file\":" << jsonString(r.bestTreeFile) << ','
      << "\"trials\":[";

  bool first = true;
  for (const auto& t : r.trials) {
    if (!first) out << ',';
    first = false;
    out << '{'
        << "\"trial\":" << t.trial << ','
        << "\"seed\":" << t.seed << ','
        << "\"codelength\":" << t.codelength << ','
        << "\"num_top_modules\":" << t.numTopModules << ','
        << "\"num_levels\":" << t.numLevels << ','
        << "\"thread\":" << t.thread << ','
        << "\"time_s\":" << t.timeSec
        << '}';
  }

  out << "]}\n";
  return out.str();
}

} // namespace infomap
