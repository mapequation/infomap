/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef PRETTY_OUTPUT_H_
#define PRETTY_OUTPUT_H_

#include <string>
#include <vector>

namespace infomap {

class PrettyOutput {
public:
  explicit PrettyOutput(bool enabled);

  bool enabled() const { return m_enabled; }
  bool ansi() const { return m_ansi; }

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
  void table(const std::vector<std::string>& headers, const std::vector<std::vector<std::string>>& rows) const;

  static std::string percent(double value);
  static std::string fixed(double value, unsigned int precision = 6);

private:
  bool m_enabled;
  bool m_ansi;
};

} // namespace infomap

#endif // PRETTY_OUTPUT_H_
