/*
 * Stopwatch.h
 *
 *  Created on: Oct 4, 2012
 *      Author: Daniel
 */

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


#endif /* STOPWATCH_H_ */
