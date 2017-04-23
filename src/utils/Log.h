/*
 * Log.h
 *
 *  Created on: 26 feb 2015
 *      Author: Daniel
 */

#ifndef SRC_IO_LOG_H_
#define SRC_IO_LOG_H_

#include <iostream>
#include <limits>
#include <iomanip>

namespace infomap {

struct hideIf;

class Log
{
	public:

	/**
	 * Log when level is below or equal Log::verboseLevel()
	 * and maxLevel is above or equal Log::verboseLevel()
	 */
	explicit Log(unsigned int level = 0, unsigned int maxLevel = std::numeric_limits<unsigned int>::max()) :
		m_level(level),
		m_maxLevel(maxLevel),
		m_visible(levelVisible(m_level, m_maxLevel)),
		m_ostream(getOutputStream(m_level, m_maxLevel))
	{}

	explicit Log(const Log& other) :
		m_level(other.m_level),
		m_maxLevel(other.m_maxLevel),
		m_visible(other.m_visible),
		m_ostream(other.m_ostream)
	{}

	Log& operator= (const Log& other)
	{
		m_level = other.m_level;
		m_maxLevel = other.m_maxLevel;
		m_visible = other.m_visible;
		return *this;
	}

	bool levelVisible()
	{
		return levelVisible(m_level, m_maxLevel);
	}

	void hide(bool value)
	{
		m_visible = !value && levelVisible();
	}

	Log& operator<< (const hideIf& manip)
	{
		return *this;
	}

	template<typename T>
	Log& operator<< (const T& data)
	{
		if (m_visible)
			m_ostream << data;
		return *this;
	}

	Log& operator<<( std::ostream& (*f) (std::ostream&) )
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

	static bool levelVisible(unsigned int level, unsigned int maxLevel)
	{
		return !s_silent && s_verboseLevel >= level && s_verboseLevel <= maxLevel;
	}

	static void setVerboseLevel(unsigned int level)
	{
		s_verboseLevel = level;
	}

	static unsigned int verboseLevel()
	{
		return s_verboseLevel;
	}

	static void setSilent(bool silent)
	{
		s_silent = silent;
	}

	static bool isSilent()
	{
		return s_silent;
	}

	static std::ostream& getOutputStream(unsigned int level, unsigned int maxLevel)
	{
		return std::cout;
	}

	static std::streamsize precision()
	{
		return std::cout.precision();
	}

	static std::streamsize precision(std::streamsize precision)
	{
		return std::cout.precision(precision);
	}

	private:
	unsigned int m_level;
	unsigned int m_maxLevel;
	bool m_visible;
	std::ostream& m_ostream;

	static unsigned int s_verboseLevel;
	static unsigned int s_silent;
};

struct hideIf
{
	hideIf(bool value) : hide(value) {}

	friend
	Log& operator<< (Log& out, const hideIf& manip)
	{
		out.hide(manip.hide);
		return out;
	}

	bool hide;
};

}

#endif /* SRC_IO_LOG_H_ */
