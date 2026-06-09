/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef INFOMAP_CONSOLE_H_
#define INFOMAP_CONSOLE_H_

#include <string>

namespace infomap {

// The console presentation layer: renders structured program state to the
// terminal as section headers, key/value metrics, status bullets, live
// progress, and verbose notes/warnings, with ANSI styling and TTY detection.
// This is the human-facing console output, distinct from the file writers in
// io/ (Output, OutputPlan, ...). Everything is routed through infomap::Log.
class Console {
public:
  Console();

  const char* reset() const { return m_ansi ? "\033[0m" : ""; }
  const char* bold() const { return m_ansi ? "\033[1m" : ""; }
  const char* dim() const { return m_ansi ? "\033[2m" : ""; }
  const char* cyan() const { return m_ansi ? "\033[36m" : ""; }
  const char* green() const { return m_ansi ? "\033[32m" : ""; }
  const char* yellow() const { return m_ansi ? "\033[33m" : ""; }

  const char* bullet() const { return m_ansi ? "•" : "-"; }
  const char* branch() const { return m_ansi ? "╰─" : "  "; }

  void section(const std::string& title) const;
  void metric(const std::string& label, const std::string& value) const;
  void status(const std::string& label, const std::string& value) const;

  class Progress;

  // A live, single-line progress indicator for a long phase. On a TTY at
  // verbosity 0 it redraws one "• Label  <live value>" line in place as the
  // phase advances, then freezes into the permanent status() line via finish().
  // When output is piped/redirected, or in verbose/silent mode, the live
  // redraws are skipped and only the final frozen line is emitted (see
  // Console::Progress). The label is conventionally the one passed to finish().
  Progress progress(const std::string& label) const;

  // Verbose (-v/-vv) diagnostic lines, in the same visual grammar as the
  // level-0 output. These only format a styled string (with trailing newline);
  // the caller owns the verbosity level, e.g. Log(1) << Console::note(...).
  // Static like percent()/fixed(): no per-call-site object needed, and ANSI
  // capability is probed once (cached). In plain mode (NO_COLOR / non-tty) the
  // escapes collapse to "" so the output stays greppable.
  static std::string note(const std::string& message);
  static std::string warn(const std::string& message);

  static std::string percent(double value);
  static std::string fixed(double value, unsigned int precision = 6);

private:
  // Renders the "  • Label  value" body shared by status() and Progress
  // (no leading carriage return, no trailing newline).
  std::string statusLine(const std::string& label, const std::string& value) const;

  bool m_ansi;

  friend class Progress;
};

// Handle for an in-place progress line; see Console::progress(). Construct
// one per phase, call update() in the work loop, and finish() with the final
// summary. Owns the on-screen line for its scope.
class Console::Progress {
public:
  explicit Progress(std::string label);
  // Movable so Console::progress() can return one by value; the moved-from
  // instance relinquishes the on-screen line (m_live = false) so only the live
  // owner commits a newline on destruction. Copying would duplicate ownership.
  Progress(Progress&& other) noexcept;
  Progress(const Progress&) = delete;
  Progress& operator=(const Progress&) = delete;
  Progress& operator=(Progress&&) = delete;
  ~Progress();

  // Redraw the live line with the current metric (TTY + verbosity 0 only).
  void update(const std::string& value);
  // Freeze into the permanent status line (always emitted when visible).
  void finish(const std::string& value);

private:
  Console m_console;
  std::string m_label;
  bool m_live = false; // an uncommitted in-place line is currently on screen
};

} // namespace infomap

#endif // INFOMAP_CONSOLE_H_
