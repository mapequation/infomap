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
  class ScopedMute {
  public:
    explicit ScopedMute(bool enabled = true) : m_enabled(enabled)
    {
      if (m_enabled)
        ++s_threadMuteDepth;
    }

    ~ScopedMute()
    {
      if (m_enabled)
        --s_threadMuteDepth;
    }

    ScopedMute(const ScopedMute&) = delete;
    ScopedMute& operator=(const ScopedMute&) = delete;

  private:
    bool m_enabled;
  };

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
    return s_threadMuteDepth == 0 && !s_silent && !(channel == LogChannel::Legacy && s_legacyMuted) && s_verboseLevel >= level && s_verboseLevel <= maxLevel;
  }

  static void setVerboseLevel(unsigned int level) { s_verboseLevel = level; }

  static void setSilent(bool silent) { s_silent = silent; }

  static void setLegacyMuted(bool muted) { s_legacyMuted = muted; }

  static bool isSilent() { return s_silent; }

  static bool isThreadMuted() { return s_threadMuteDepth > 0; }

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
    // When muted (a worker thread under --parallel-trials), deliberately do NOT touch the
    // shared global stream — concurrent precision() calls across workers would race on it.
    // The returned value is therefore a no-op echo, not the stream's previous precision.
    // This is safe because muted callers' save/restore pairs are balanced and never reach
    // the stream; do not "fix" this by querying ostream() here (that reintroduces the race).
    if (s_threadMuteDepth > 0)
      return precision;
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
  static thread_local unsigned int s_threadMuteDepth;
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
