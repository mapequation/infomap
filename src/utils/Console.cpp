/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "Console.h"
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

Console::Console() : m_ansi(stdoutSupportsAnsi()) {}

void Console::section(const std::string& title) const
{
  Log() << fmt::format(FMT_STRING("\n{}{}{}{}\n"), cyan(), bold(), title, reset());
}

void Console::metric(const std::string& label, const std::string& value) const
{
  // {:<30} == std::left << std::setw(30) (min width, left-justified, space fill).
  // The label (and its padding) is dimmed; reset() lands before the value.
  Log() << fmt::format(FMT_STRING("  {}{:<30}{}{}\n"), dim(), label, reset(), value);
}

std::string Console::statusLine(const std::string& label, const std::string& value) const
{
  return fmt::format(FMT_STRING("  {} {:<14}{}"), bullet(), label, value);
}

void Console::status(const std::string& label, const std::string& value) const
{
  Log() << statusLine(label, value) << "\n";
}

Console::Progress Console::progress(const std::string& label) const
{
  return Progress(label);
}

Console::Progress::Progress(std::string label) : m_label(std::move(label)) {}

Console::Progress::Progress(Progress&& other) noexcept
    : m_console(other.m_console), m_label(std::move(other.m_label)), m_live(other.m_live)
{
  other.m_live = false;
}

Console::Progress::~Progress()
{
  // If updates were drawn but finish() was never reached (e.g. an early return
  // or exception), commit a newline so following output doesn't glue onto the
  // dangling in-place line. Log(0, 0) matches update()'s gating, so this is a
  // no-op unless a live line was actually shown.
  if (m_live)
    Log(0, 0) << "\n";
}

void Console::Progress::update(const std::string& value)
{
  // In-place redraw: carriage return, the line body, then clear-to-end-of-line
  // (\033[K) to wipe any leftover from a previously longer value. Gated on a
  // TTY (escapes would corrupt a file) and on Log(0, 0), which is visible only
  // at verbosity 0 and is muted in silent / parallel-trial workers — so verbose
  // mode keeps its detailed per-step lines instead of a clobbered live line.
  if (!m_console.m_ansi)
    return;
  Log(0, 0) << "\r" << m_console.statusLine(m_label, value) << "\033[K" << std::flush;
  m_live = true;
}

void Console::Progress::finish(const std::string& value)
{
  // Freeze into the permanent status line, visible at verbosity 0 and above.
  // On a TTY, overwrite the live line in place (\r ... \033[K) before
  // committing the newline; otherwise emit the plain line, identical to
  // status().
  if (m_console.m_ansi)
    Log() << "\r" << m_console.statusLine(m_label, value) << "\033[K\n";
  else
    Log() << m_console.statusLine(m_label, value) << "\n";
  m_live = false;
}

std::string Console::note(const std::string& message)
{
  // Keeps the recognizable "  -> " marker (greppable; byte-identical to the
  // previous verbose notes in plain mode) but dims the arrow in a terminal so
  // verbose diagnostics share the level-0 pretty vocabulary.
  Console c;
  return fmt::format(FMT_STRING("  {}->{} {}\n"), c.dim(), c.reset(), message);
}

std::string Console::warn(const std::string& message)
{
  // Warnings get their own marker (yellow "Warning:") instead of the note
  // arrow, so they stand apart from informational notes.
  Console c;
  return fmt::format(FMT_STRING("  {}Warning:{} {}\n"), c.yellow(), c.reset(), message);
}

std::string Console::detail(const std::string& message)
{
  // Indent 4 (one step below the indent-2 status/metric tier) and dim, so the
  // bright section headers and • bullets stay the visual spine and trace reads
  // as subordinate. Plain mode keeps the indent, drops the dim.
  Console c;
  return fmt::format(FMT_STRING("    {}{}{}\n"), c.dim(), message, c.reset());
}

std::string Console::arrow()
{
  // Same TTY/ANSI gate as bullet()/branch() (probed once, cached), so the
  // optimization status lines degrade to "->" when piped or on a dumb terminal.
  return stdoutSupportsAnsi() ? "→" : "->";
}

std::string Console::percent(double value)
{
  if (std::abs(value) < 1e-9)
    return "0%";
  return fmt::format(FMT_STRING("{}%"), io::toPrecision(value, 2, true));
}

std::string Console::fixed(double value, unsigned int precision)
{
  // {:.{}f} reproduces std::fixed << std::setprecision(precision) byte-for-byte
  // (verified across edge cases); see scripts/check_cpp_stream_policy.py for why
  // formatting goes through fmt rather than iostreams.
  return fmt::format(FMT_STRING("{:.{}f}"), value, precision);
}

} // namespace infomap
