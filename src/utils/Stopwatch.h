/* ------------------------------------------------------------------------

 Infomap software package for multi-level network clustering

 * Copyright (c) 2013
 * For license and authors, see README.txt or http://www.mapequation.org

------------------------------------------------------------------------ */

#ifndef STOPWATCH_H_
#define STOPWATCH_H_

#include <ctime>

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
    	return ticks / DBL_CLOCKS_PER_SEC;
    }

    double getElapsedTimeInMilliSec() const
    {
    	clock_t ticks = (m_running ? std::clock() : m_stop) - m_start;
    	return ticks * 1000.0 / DBL_CLOCKS_PER_SEC;
    }

    static double getElapsedTimeSinceProgramStartInSec()
    {
    	return std::clock() / DBL_CLOCKS_PER_SEC;
    }

    static double getElapsedTimeSinceProgramStartInMilliSec()
    {
    	return std::clock() * 1000.0 / DBL_CLOCKS_PER_SEC;
    }

private:
    std::clock_t m_start, m_stop;
    bool m_running;
    static const double DBL_CLOCKS_PER_SEC = CLOCKS_PER_SEC; // 1000000
};


#endif /* STOPWATCH_H_ */
