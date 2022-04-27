/*******************************************************************************
  Infomap software package for multi-level network clustering

  Copyright (c) 2013, 2014 Daniel Edler, Martin Rosvall

  For more information, see <http://www.mapequation.org>

  This file is part of Infomap software package.

  Infomap software package is free software: you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Infomap software package is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with Infomap software package.  If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/

#ifndef _LOG_H_
#define _LOG_H_

#include <iostream>
#include <limits>
#include <iomanip>
#include <type_traits>

namespace infomap {

struct hideIf;

class Log {
  using ostreamFuncPtr = std::add_pointer_t<std::ostream&(std::ostream&)>;

public:
  /**
   * Log when level is below or equal Log::verboseLevel()
   * and maxLevel is above or equal Log::verboseLevel()
   */
  explicit Log(unsigned int level = 0, unsigned int maxLevel = std::numeric_limits<int>::max())
      : m_level(level), m_maxLevel(maxLevel), m_visible(isVisible(m_level, m_maxLevel)) { }

  bool isVisible() const { return isVisible(m_level, m_maxLevel); }

  void hide(bool value) { m_visible = !value && isVisible(); }

  Log& operator<<(const hideIf&) { return *this; }

  template <typename T>
  Log& operator<<(const T& data)
  {
    if (m_visible)
      m_ostream << data;
    return *this;
  }

  Log& operator<<(ostreamFuncPtr f)
  {
    if (m_visible)
      m_ostream << f;
    return *this;
  }

  static void init(unsigned int verboseLevel, bool silent, unsigned int numberPrecision)
  {
    setVerboseLevel(verboseLevel);
    setSilent(silent);
    Log() << std::setprecision(numberPrecision);
  }

  static bool isVisible(unsigned int level, unsigned int maxLevel)
  {
    return !s_silent && s_verboseLevel >= level && s_verboseLevel <= maxLevel;
  }

  static void setVerboseLevel(unsigned int level) { s_verboseLevel = level; }

  static unsigned int verboseLevel() { return s_verboseLevel; }

  static void setSilent(bool silent) { s_silent = silent; }

  static bool isSilent() { return s_silent; }

  static std::streamsize precision() { return std::cout.precision(); }

  static std::streamsize precision(std::streamsize precision)
  {
    return std::cout.precision(precision);
  }

private:
  unsigned int m_level;
  unsigned int m_maxLevel;
  bool m_visible;
  std::ostream& m_ostream = std::cout;

  static unsigned int s_verboseLevel;
  static bool s_silent;
};

struct hideIf {
  explicit hideIf(bool value) : hide(value) { }

  friend Log& operator<<(Log& out, const hideIf& manip)
  {
    out.hide(manip.hide);
    return out;
  }

  bool hide;
};

} // namespace infomap

#endif /* _LOG_H_ */
