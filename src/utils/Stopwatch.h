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

#include <ctime>

#ifdef NS_INFOMAP
namespace infomap
{
#endif

class Stopwatch
{
public:
	explicit Stopwatch(bool startImmediately)
        : m_start(0), m_stop(0), m_running(false)
    {
        if (startImmediately)
        {
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
        if (m_running)
        {
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

private:
    std::clock_t m_start, m_stop;
    bool m_running;
};

#ifdef NS_INFOMAP
}
#endif

#endif /* STOPWATCH_H_ */
