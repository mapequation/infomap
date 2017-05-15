/**********************************************************************************

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

**********************************************************************************/

#ifndef STOPWATCH_H_
#define STOPWATCH_H_

#include <chrono>
#include <ratio>

namespace infomap {


class Stopwatch
{
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimeType = std::chrono::time_point<Clock>;

	explicit Stopwatch(bool startImmediately)
        : m_start(now()), m_stop(now()), m_running(false)
    {
        if (startImmediately)
        {
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
        if (m_running)
        {
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

    // static double getElapsedTimeSinceProgramStartInSec()
    // {
    // 	return (double)Clock::now() / CLOCKS_PER_SEC;
    // }

    // static double getElapsedTimeSinceProgramStartInMilliSec()
    // {
    // 	return Clock::now() * 1000.0 / CLOCKS_PER_SEC;
    // }

private:
    TimeType m_start, m_stop;
    bool m_running;
};

}

#endif /* STOPWATCH_H_ */
