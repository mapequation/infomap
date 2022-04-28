/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Eriksson, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_AGPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef STOPWATCH_H_
#define STOPWATCH_H_

#ifndef PYTHON
#include <chrono>
#include <ratio>

namespace infomap {

class Stopwatch {
public:
  using Clock = std::chrono::high_resolution_clock;
  using TimeType = std::chrono::time_point<Clock>;

  explicit Stopwatch(bool startImmediately)
      : m_start(now()), m_stop(now()), m_running(false)
  {
    if (startImmediately) {
      start();
    }
  }

  void start()
  {
    m_start = Clock::now();
    m_running = true;
  }

  void reset()
  {
    if (m_running)
      m_start = Clock::now();
  }

  void stop()
  {
    if (m_running) {
      m_stop = Clock::now();
      m_running = false;
    }
  }

  TimeType getCurrentTimePoint() const
  {
    return m_running ? Clock::now() : m_stop;
  }

  double getElapsedTimeInSec() const
  {
    std::chrono::duration<double> diff = getCurrentTimePoint() - m_start;
    return diff.count();
  }

  double getElapsedTimeInMilliSec() const
  {
    std::chrono::duration<double, std::milli> diff = getCurrentTimePoint() - m_start;
    return diff.count();
  }

  static TimeType now()
  {
    return Clock::now();
  }

  friend std::ostream& operator<<(std::ostream& out, const Stopwatch& stopwatch)
  {
    auto temp = static_cast<unsigned int>(std::floor(stopwatch.getElapsedTimeInMilliSec()));
    if (temp > 60'000) {
      if (temp > 3600'000) {
        if (temp > 86'400'000) {
          out << temp / 86'400'000 << "d ";
          temp %= 86'400'000;
        }
        out << temp / 3600'000 << "h ";
        temp %= 3600'000;
      }
      out << temp / 60'000 << "m ";
      temp %= 60'000;
      out << temp * 1.0 / 1000 << "s";
    } else {
      out << stopwatch.getElapsedTimeInSec() << "s";
    }
    return out;
  }

private:
  TimeType m_start, m_stop;
  bool m_running;
};

} // namespace infomap

#else

#include <ctime>

namespace infomap {

class Stopwatch {
public:
  explicit Stopwatch(bool startImmediately)
      : m_start(0), m_stop(0), m_running(false)
  {
    if (startImmediately) {
      start();
    }
  }

  void start()
  {
    m_start = std::clock();
    m_running = true;
  }

  void reset()
  {
    if (m_running)
      m_start = std::clock();
  }

  void stop()
  {
    if (m_running) {
      m_stop = std::clock();
      m_running = false;
    }
  }

  double getElapsedTimeInSec() const
  {
    clock_t ticks = (m_running ? std::clock() : m_stop) - m_start;
    return (double)ticks / CLOCKS_PER_SEC;
  }

  double getElapsedTimeInMilliSec() const
  {
    clock_t ticks = (m_running ? std::clock() : m_stop) - m_start;
    return ticks * 1000.0 / CLOCKS_PER_SEC;
  }

  static double getElapsedTimeSinceProgramStartInSec()
  {
    return (double)std::clock() / CLOCKS_PER_SEC;
  }

  static double getElapsedTimeSinceProgramStartInMilliSec()
  {
    return std::clock() * 1000.0 / CLOCKS_PER_SEC;
  }

  friend std::ostream& operator<<(std::ostream& out, const Stopwatch& stopwatch)
  {
    unsigned int temp = static_cast<unsigned int>(std::floor(stopwatch.getElapsedTimeInMilliSec()));
    if (temp > 60'000) {
      if (temp > 3600'000) {
        if (temp > 86'400'000) {
          out << temp / 86'400'000 << "d ";
          temp %= 86'400'000;
        }
        out << temp / 3600'000 << "h ";
        temp %= 3600'000;
      }
      out << temp / 60'000 << "m ";
      temp %= 60'000;
      out << temp * 1.0 / 1000 << "s";
    } else {
      out << stopwatch.getElapsedTimeInSec() << "s";
    }
    return out;
  }

private:
  std::clock_t m_start, m_stop;
  bool m_running;
};

} // namespace infomap

#endif

#endif // STOPWATCH_H_
