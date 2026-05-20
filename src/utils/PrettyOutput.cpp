/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "PrettyOutput.h"
#include "Log.h"
#include "convert.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <sstream>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace infomap {

namespace {

  bool stdoutSupportsAnsi()
  {
    static const bool supportsAnsi = []() {
#if defined(_WIN32) || defined(__EMSCRIPTEN__)
      return false;
#else
      const char* term = std::getenv("TERM");
      return ::isatty(STDOUT_FILENO) && std::getenv("NO_COLOR") == nullptr && term != nullptr && std::string(term) != "dumb";
#endif
    }();
    return supportsAnsi;
  }

} // namespace

PrettyOutput::PrettyOutput(bool enabled) : m_enabled(enabled), m_ansi(enabled && stdoutSupportsAnsi()) {}

void PrettyOutput::section(const std::string& title) const
{
  if (!m_enabled)
    return;
  Log::pretty() << "\n"
                << cyan() << bold() << title << reset() << "\n";
}

void PrettyOutput::metric(const std::string& label, const std::string& value) const
{
  if (!m_enabled)
    return;
  Log::pretty() << "  " << dim() << std::left << std::setw(30) << label << reset() << value << std::right << "\n";
}

void PrettyOutput::status(const std::string& label, const std::string& value) const
{
  if (!m_enabled)
    return;
  Log::pretty() << "  " << bullet() << " " << std::left << std::setw(14) << label << std::right << value << "\n";
}

void PrettyOutput::table(const std::vector<std::string>& headers, const std::vector<std::vector<std::string>>& rows) const
{
  if (!m_enabled || headers.empty())
    return;

  std::vector<std::size_t> widths(headers.size(), 0);
  for (std::size_t i = 0; i < headers.size(); ++i)
    widths[i] = headers[i].size();
  for (const auto& row : rows) {
    for (std::size_t i = 0; i < std::min(row.size(), headers.size()); ++i)
      widths[i] = std::max(widths[i], row[i].size());
  }

  Log::pretty() << "  " << dim();
  for (std::size_t i = 0; i < headers.size(); ++i) {
    if (i > 0)
      Log::pretty() << "  ";
    Log::pretty() << std::left << std::setw(static_cast<int>(widths[i])) << headers[i];
  }
  Log::pretty() << reset() << std::right << "\n";

  for (const auto& row : rows) {
    Log::pretty() << "  ";
    for (std::size_t i = 0; i < headers.size(); ++i) {
      if (i > 0)
        Log::pretty() << "  ";
      const std::string value = i < row.size() ? row[i] : "";
      Log::pretty() << std::left << std::setw(static_cast<int>(widths[i])) << value;
    }
    Log::pretty() << std::right << "\n";
  }
}

std::string PrettyOutput::percent(double value)
{
  if (std::abs(value) < 1e-9)
    return "0%";
  return io::Str() << io::toPrecision(value, 2, true) << "%";
}

std::string PrettyOutput::fixed(double value, unsigned int precision)
{
  std::ostringstream out;
  out << std::fixed << std::setprecision(static_cast<int>(precision)) << value;
  return out.str();
}

} // namespace infomap
