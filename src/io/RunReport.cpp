/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "RunReport.h"
#include "SafeFile.h"
#include "../utils/Log.h"

#include <iomanip>
#include <sstream>

namespace infomap {

namespace {

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

  void writeDoubleArray(std::ostream& out, const std::vector<double>& values)
  {
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (i > 0) {
        out << ',';
      }
      out << values[i];
    }
    out << ']';
  }

  void writeUnsignedArray(std::ostream& out, const std::vector<unsigned int>& values)
  {
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (i > 0) {
        out << ',';
      }
      out << values[i];
    }
    out << ']';
  }

} // namespace

std::string runSummaryReportJson(const RunSummaryReport& report)
{
  std::ostringstream out;
  out << std::setprecision(12);
  out << '{'
      << "\"version\":" << jsonString(report.version) << ','
      << "\"codelength\":" << report.codelength << ','
      << "\"top_modules\":" << report.topModules << ','
      << "\"levels\":" << report.levels << ','
      << "\"trials\":" << report.trials << ','
      << "\"best_trial\":" << report.bestTrial << ','
      << "\"trial_codelengths\":";
  writeDoubleArray(out, report.trialCodelengths);
  out << ",\"trial_top_modules\":";
  writeUnsignedArray(out, report.trialTopModules);
  out << "}\n";
  return out.str();
}

std::string runTimingReportJson(const RunTimingReport& report)
{
  std::ostringstream out;
  out << std::setprecision(12);
  out << '{'
      << "\"version\":" << jsonString(report.version) << ','
      << "\"openmp\":" << (report.openmp ? "true" : "false") << ','
      << "\"threads_requested\":" << report.threadsRequested << ','
      << "\"threads_used\":" << report.threadsUsed << ','
      << "\"thread_source\":" << jsonString(report.threadSource) << ','
      << "\"network\":{"
      << "\"nodes\":" << report.network.nodes << ','
      << "\"links\":" << report.network.links << ','
      << "\"directed\":" << (report.network.directed ? "true" : "false")
      << "},"
      << "\"timing\":{";

  for (std::size_t i = 0; i < report.timing.size(); ++i) {
    if (i > 0) {
      out << ',';
    }
    out << jsonString(report.timing[i].first) << ':' << report.timing[i].second;
  }

  out << "},\"trials\":[";
  bool firstTrial = true;
  for (const auto& trial : report.trials) {
    if (!trial.valid) {
      continue;
    }
    if (!firstTrial) {
      out << ',';
    }
    firstTrial = false;
    out << '{'
        << "\"trial\":" << trial.trial << ','
        << "\"thread\":" << trial.thread << ','
        << "\"seed\":" << trial.seed << ','
        << "\"time_s\":" << trial.timeSec << ','
        << "\"codelength\":" << trial.codelength << ','
        << "\"top_modules\":" << trial.topModules << ','
        << "\"num_levels\":" << trial.numLevels
        << '}';
  }
  out << ']';

  if (report.includeMemory) {
    out << ",\"memory\":{"
        << "\"rss_peak_mb\":" << report.memory.rssPeakMb;
    if (report.memory.haveBytesPerNode) {
      out << ",\"bytes_per_node\":" << report.memory.bytesPerNode;
    }
    if (report.memory.haveBytesPerLink) {
      out << ",\"bytes_per_link\":" << report.memory.bytesPerLink;
    }
    out << '}';
  }

  out << "}\n";
  return out.str();
}

void writeJsonReport(const std::string& path, const std::string& json, bool overwrite)
{
  if (path == "-") {
    Log::getOutputStream() << json;
    return;
  }

  SafeOutFile output(path, std::ios_base::out, overwrite);
  output << json;
  output.commit();
}

} // namespace infomap
