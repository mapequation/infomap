/* ------------------------------------------------------------------------

 Infomap software package for multi-level network clustering

 * Copyright (c) 2013
 * For license and authors, see README.txt or http://www.mapequation.org

------------------------------------------------------------------------ */

#include "Logger.h"

unsigned int Logger::s_indentLevel = 0;
unsigned int Logger::s_indentWidth = 4;
std::string Logger::s_indentString = "";
unsigned int Logger::MAX_INDENT_LEVEL = 10;
std::string Logger::s_benchmarkFilename = "benchmark.tsv";


//void Logger::logToFile(std::string row, std::string filename)
//{
//	SafeOutFile logFile(filename.c_str());
//	if (logFile.is_open())
//		logFile << Stopwatch::getElapsedTimeSinceProgramStartInSec() << " " << row << std::endl;
//}
