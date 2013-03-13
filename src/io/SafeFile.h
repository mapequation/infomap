/* ------------------------------------------------------------------------

 Infomap software package for multi-level network clustering

 * Copyright (c) 2013
 * For license and authors, see README.txt or http://www.mapequation.org

------------------------------------------------------------------------ */

#ifndef SAFEFILE_H_
#define SAFEFILE_H_
#include <iostream>
#include <fstream>
#include <ios>
#include <stdexcept>

using std::ifstream;
using std::ofstream;

class FileOpenError : public std::runtime_error {
public:
	FileOpenError(std::string const& s)
	: std::runtime_error(s)
	{ }
};

/**
 * A wrapper for the C++ file stream class that automatically closes
 * the file stream when the destructor is called. Allocate it on the
 * stack to have it automatically closed when going out of scope.
 *
 * Note:
 * In C++, the only code that can be guaranteed to be executed after an
 * exception is thrown are the destructors of objects residing on the stack.
 *
 * You can exploit that fact to avoid resource leaks by tying all resources
 * to the lifespan of an object allocated on the stack. This technique is
 * called Resource Acquisition Is Initialization (RAII).
 *
 */
class SafeInFile : public ifstream
{
public:
	SafeInFile(const char* filename, ios_base::openmode mode = ios_base::in)
	: ifstream(filename, mode)
	{
		if (fail())
			throw FileOpenError("Error opening file '" + std::string(filename) + "'");
	}

	~SafeInFile()
	{
		if (is_open())
			close();
	}
};

class SafeOutFile : public ofstream
{
public:
	SafeOutFile(const char* filename, ios_base::openmode mode = ios_base::out)
	: ofstream(filename, mode)
	{
		if (fail())
			throw FileOpenError("Error opening file '" + std::string(filename) + "'");
	}

	~SafeOutFile()
	{
		if (is_open())
			close();
	}
};

#endif /* SAFEFILE_H_ */
