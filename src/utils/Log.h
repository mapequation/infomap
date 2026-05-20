/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef LOG_H_
#define LOG_H_

#include <ostream>
#include <limits>
#include <iomanip>
#include <type_traits>
#include <cstdint>

namespace infomap {

struct hideIf;

enum class LogChannel : std::uint8_t {
  Legacy,
  Pretty,
  Important,
};

class Log {
  using ostreamFuncPtr = std::add_pointer_t<std::ostream&(std::ostream&)>;

public:
  /**
   * Log when level is below or equal Log::verboseLevel()
   * and maxLevel is above or equal Log::verboseLevel()
   */
  explicit Log(unsigned int level = 0, unsigned int maxLevel = std::numeric_limits<int>::max())
      : Log(LogChannel::Legacy, level, maxLevel) {}

  explicit Log(LogChannel channel, unsigned int level = 0, unsigned int maxLevel = std::numeric_limits<int>::max())
      : m_channel(channel), m_level(level), m_maxLevel(maxLevel), m_visible(isVisible(m_channel, m_level, m_maxLevel)) {}

  bool isVisible() const { return isVisible(m_channel, m_level, m_maxLevel); }

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

  static Log pretty(unsigned int level = 0, unsigned int maxLevel = std::numeric_limits<int>::max())
  {
    return Log(LogChannel::Pretty, level, maxLevel);
  }

  static Log important(unsigned int level = 0, unsigned int maxLevel = std::numeric_limits<int>::max())
  {
    return Log(LogChannel::Important, level, maxLevel);
  }

  static void init(unsigned int verboseLevel, bool silent, unsigned int numberPrecision)
  {
    init(verboseLevel, silent, numberPrecision, false);
  }

  static void init(unsigned int verboseLevel, bool silent, unsigned int numberPrecision, bool prettyOutput)
  {
    setVerboseLevel(verboseLevel);
    setSilent(silent);
    setLegacyMuted(prettyOutput && verboseLevel == 0);
    Log() << std::setprecision(static_cast<int>(numberPrecision));
  }

  static bool isVisible(unsigned int level, unsigned int maxLevel)
  {
    return isVisible(LogChannel::Legacy, level, maxLevel);
  }

  static bool isVisible(LogChannel channel, unsigned int level, unsigned int maxLevel)
  {
    return !s_silent && !(channel == LogChannel::Legacy && s_legacyMuted) && s_verboseLevel >= level && s_verboseLevel <= maxLevel;
  }

  static void setVerboseLevel(unsigned int level) { s_verboseLevel = level; }

  static void setSilent(bool silent) { s_silent = silent; }

  static void setLegacyMuted(bool muted) { s_legacyMuted = muted; }

  static bool isSilent() { return s_silent; }

  /// Set a custom output stream for all Log output.
  /// The caller must ensure @p os outlives all subsequent logging,
  /// or remains valid until another stream is configured.
  static void setOutputStream(std::ostream& os)
  {
    s_ostream = &os;
  }
  static void setOutputStream(std::ostream&&) = delete;

  /// Guarantee zero output: redirect to a null sink and set silent.
  static void setNoOutput();

  static std::ostream& getOutputStream() { return ostream(); }

  static std::streamsize precision() { return ostream().precision(); }

  static std::streamsize precision(std::streamsize precision)
  {
    return ostream().precision(precision);
  }

private:
  LogChannel m_channel;
  unsigned int m_level;
  unsigned int m_maxLevel;
  bool m_visible;
  std::ostream& m_ostream = ostream();

  static std::ostream& ostream()
  {
#ifdef INFOMAP_R
    return *s_ostream;
#else
    return s_ostream ? *s_ostream : defaultStream();
#endif
  }
#ifndef INFOMAP_R
  static std::ostream& defaultStream();
#endif
  static std::ostream* s_ostream;
  static unsigned int s_verboseLevel;
  static bool s_silent;
  static bool s_legacyMuted;
};

struct hideIf {
  explicit hideIf(bool value) : hide(value) {}

  friend Log& operator<<(Log& out, const hideIf& manip)
  {
    out.hide(manip.hide);
    return out;
  }

  bool hide;
};

} // namespace infomap

#endif // LOG_H_
