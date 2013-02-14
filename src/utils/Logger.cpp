/*
 * Logger.cpp
 *
 *  Created on: Apr 17, 2012
 *      Author: daniel
 */

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
