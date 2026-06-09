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
#include "format.h"

#include <cmath>
#include <cstdlib>

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

PrettyOutput::PrettyOutput() : m_ansi(stdoutSupportsAnsi()) {}

void PrettyOutput::section(const std::string& title) const
{
  Log() << fmt::format(FMT_STRING("\n{}{}{}{}\n"), cyan(), bold(), title, reset());
}

void PrettyOutput::metric(const std::string& label, const std::string& value) const
{
  // {:<30} == std::left << std::setw(30) (min width, left-justified, space fill).
  // The label (and its padding) is dimmed; reset() lands before the value.
  Log() << fmt::format(FMT_STRING("  {}{:<30}{}{}\n"), dim(), label, reset(), value);
}

void PrettyOutput::status(const std::string& label, const std::string& value) const
{
  Log() << fmt::format(FMT_STRING("  {} {:<14}{}\n"), bullet(), label, value);
}

std::string PrettyOutput::percent(double value)
{
  if (std::abs(value) < 1e-9)
    return "0%";
  return fmt::format(FMT_STRING("{}%"), io::toPrecision(value, 2, true));
}

std::string PrettyOutput::fixed(double value, unsigned int precision)
{
  // {:.{}f} reproduces std::fixed << std::setprecision(precision) byte-for-byte
  // (verified across edge cases); see scripts/check_cpp_stream_policy.py for why
  // formatting goes through fmt rather than iostreams.
  return fmt::format(FMT_STRING("{:.{}f}"), value, precision);
}

} // namespace infomap
