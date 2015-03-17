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


#ifndef DATE_H_
#define DATE_H_

#include <ctime>
#include <cmath>
#include <ostream>

#ifdef NS_INFOMAP
namespace infomap
{
#endif

class ElapsedTime
{
public:
	enum InSeconds
	{
		SECONDS_IN_A_MINUTE = 60,
		SECONDS_IN_AN_HOUR = 3600,
		SECONDS_IN_A_DAY = 86400 // 3600*24
	};

	ElapsedTime(double elapsedTime) : m_elapsedTime(elapsedTime) {}
	~ElapsedTime() {}

	double getSeconds() const { return m_elapsedTime; }
	double getMinutes() const { return m_elapsedTime / 60; }
	double getHours() const { return m_elapsedTime / 3600; }
	double getDays() const { return m_elapsedTime / 86400; }


	friend std::ostream& operator<<(std::ostream& out, const ElapsedTime& elapsedTime)
	{
		unsigned int temp = static_cast<unsigned int>(std::floor(elapsedTime.getSeconds()));
		if (temp > 60)
		{
			if (temp > 3600)
			{
				if (temp > 86400)
				{
					out << temp / 86400 << "d ";
					temp %= 86400;
				}
				out << temp / 3600 << "h ";
				temp %= 3600;
			}
			out << temp / 60 << "m ";
			temp %= 60;
			out << temp << "s";
		}
		else
		{
			out << temp << "s";
		}
		return out;
	}
private:
	double m_elapsedTime;
};

class Date
{
public:
	Date() : m_timeOfCreation(time(NULL)) {}
	Date(const Date& other) : m_timeOfCreation(other.m_timeOfCreation) {}
	Date& operator=(Date other) {
		m_timeOfCreation = other.m_timeOfCreation;
		return *this;
	}

	virtual ~Date() {}

	friend std::ostream& operator<<(std::ostream& out, const Date& date)
	{
		struct std::tm t = *localtime(&date.m_timeOfCreation);
		return out << "[" <<
				(t.tm_year+1900) <<
				(t.tm_mon < 9 ? "-0" : "-") <<
				(t.tm_mon+1) <<
				(t.tm_mday < 10 ? "-0" : "-") <<
				t.tm_mday <<
				(t.tm_hour < 10 ? " 0" : " ") <<
				t.tm_hour <<
				(t.tm_min < 10 ? ":0" : ":") <<
				t.tm_min <<
				(t.tm_sec < 10 ? ":0" : ":") <<
				t.tm_sec << "]";
	}

	ElapsedTime operator-(const Date& date)
	{
		return ElapsedTime(difftime(m_timeOfCreation, date.m_timeOfCreation));
	}

private:
	std::time_t m_timeOfCreation;
};

#ifdef NS_INFOMAP
}
#endif

#endif /* DATE_H_ */
