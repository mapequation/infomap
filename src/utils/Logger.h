/**********************************************************************************

 Infomap software package for multi-level network clustering

 Copyright (c) 2013 Daniel Edler, Martin Rosvall
 
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


#ifndef LOGGER_H_
#define LOGGER_H_

#include <iostream>
#include <cassert>
#include "Stopwatch.h"
#include "../io/SafeFile.h"

class Logger
{
public:
	enum LogLevel {
		LOG_INFO,
		LOG_DEBUG,
		LOG_WARN,
	};

	static void pushIndentLevel()
	{
		++s_indentLevel;
	}

	static void popIndentLevel()
	{
		if (s_indentLevel == 0)
			std::cerr << "Warning: Calling Logger::popIndentLevel when already zero!" << std::endl;
		else
			--s_indentLevel;
	}

	static const std::string& indent()
	{
		if (s_indentString.length() != s_indentLevel * s_indentWidth)
			s_indentString.assign(s_indentLevel * s_indentWidth, ' ');
		return s_indentString;
	}

	static unsigned int indentLevel()
	{
		return s_indentLevel;
	}

	static void setBenchmarkFilename(std::string filename)
	{
		s_benchmarkFilename = filename;
	}

	static void benchmark(std::string tag, double codelength, unsigned int numTopModules,
			unsigned int numNonTrivialTopModules, unsigned int numLevels, bool writeOnlyTag = false)
	{
		static SafeOutFile logFile(s_benchmarkFilename.c_str());
		if (logFile.is_open())
		{
			if (writeOnlyTag)
				logFile << tag << "\n";
			else
				logFile << Stopwatch::getElapsedTimeSinceProgramStartInSec() << "\t" << tag << "\t" <<
					codelength << "\t" << numTopModules << "\t" << numNonTrivialTopModules << "\t" <<
					numLevels << "\n";
		}
	}


private:
	static unsigned int s_indentLevel;
	static unsigned int s_indentWidth;
	static std::string s_indentString;
	static std::string s_benchmarkFilename;

public:
	static unsigned int MAX_INDENT_LEVEL;

//	static bool DEBUG;

	// Macro-like non-evaluating expressions as arguments to ordinary functions?
};

//#define USE_FUNCTION_LOGGER // Prepend the current function name in the log output.
//#define USE_PRETTY_FUNCTION_LOGGER // Prepend the current pretty function name in the log output, which includes function signatures.
//#define USE_LOGGER_TIME // Prepend the current time (HH:MM:SS) in the log output.
#define NO_DEBUG_LOGGING // Remove debug logging even if not in release mode


//#ifndef NDEBUG
//#define ASSERT_ENABLED
//#endif


/**
 * Define a macro that compiles to nothing.
 * The sizeof operator have the special ability to compile to a no-op
 * and at the same time suppress compiler warnings of unused variable.
 */
//#define TO_NOTHING(x) (void)(sizeof((x), 0))
//#define TO_NOTHING(x) do { (void)sizeof(x); } while(0)
#define TO_NOTHING(x) ((void)0)


#if defined(ASSERT_ENABLED)
	#define ASSERT( x ) assert(x)
#else
	#define ASSERT( x ) TO_NOTHING(x)
#endif

#if defined(RELEASE) || defined(NO_DEBUG_LOGGING)
	#define DEBUG_EXEC( x )
	#define DEBUG_OUT( x )
#else
	#include <iostream>
	#define DEBUG_EXEC( x ) x
	#ifdef USE_LOGGER_TIME
		#define LOGGER_TIME __TIME__ << " "
	#else
		#define LOGGER_TIME ""
	#endif

	#define LOGGER_OUT( x )  do { \
		if (Logger::indentLevel() <= Logger::MAX_INDENT_LEVEL) \
			std::cout << Logger::indent() << LOGGER_TIME << x; \
	} while (0)

	#define LOGGER_OUT_WITH_FUNCTION_NAME( x )  do { \
		std::cout << LOGGER_TIME << "@" << __FUNCTION__ << ": " << x; \
	} while (0)

	#define LOGGER_OUT_WITH_PRETTY_FUNCTION_NAME( x )  do { \
		std::cout << LOGGER_TIME << "@" << __PRETTY_FUNCTION__ << ": " << x; \
	} while (0)

	#if defined(USE_PRETTY_FUNCTION_LOGGER)
		#define DEBUG_OUT LOGGER_OUT_WITH_PRETTY_FUNCTION_NAME
	#elif defined(USE_FUNCTION_LOGGER)
		#define DEBUG_OUT LOGGER_OUT_WITH_FUNCTION_NAME
	#else
		#define DEBUG_OUT LOGGER_OUT
	#endif

#endif
#if defined(RELEASE) || defined(NO_DEBUG_LOGGING)
	#define RELEASE_OUT( x )  do { \
		std::cout << x; \
	} while (0)
	#define INDENTED_RELEASE_OUT( x )  do { \
		if (Logger::indentLevel() <= Logger::MAX_INDENT_LEVEL ) { std::cout << Logger::indent() << x; } \
	} while (0)
#else
	#define RELEASE_OUT( x )
#endif

#define ALL_OUT( x ) do { \
	std::cout << x; \
} while (0)

#define LOGGER_INFO_LEVEL
#ifdef LOGGER_INFO_LEVEL
	#include <iostream>
	#define INFO_OUT( x )  do { \
		std::cout << x; \
	} while (0)
#else
	#define INFO_OUT( x )
#endif


#if defined(RELEASE) || defined(NO_LOGGING)
	#define PRINT_FUNCTION_NAME()
#else
	#define PRINT_FUNCTION_NAME()  do { \
		std::cout << __FILE__ << ": " << __PRETTY_FUNCTION__ << std::endl; \
	} while (0)
#endif


#endif /* LOGGER_H_ */
