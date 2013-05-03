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


#ifndef SAFEFILE_H_
#define SAFEFILE_H_
#include <iostream>
#include <fstream>
#include <ios>
#include <stdexcept>
#include "convert.h"

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
			throw FileOpenError(io::Str() << "Error opening file '" << filename << "'");
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
			throw FileOpenError(io::Str() << "Error opening file '" << filename << "'");
	}

	~SafeOutFile()
	{
		if (is_open())
			close();
	}
};

#include <stddef.h>

class ofstream_binary : public ofstream
{
public:
	ofstream_binary(const char* filename)
		: ofstream(filename, ios_base::out | ios_base::trunc | ios_base::binary), m_size(0) {}

	template<typename T>
	ofstream_binary& operator<<(T value)
	{
		size_t size = sizeof(value);
		m_size += size;
		write(reinterpret_cast<const char*>(&value), size);
		return *this;
	}

	size_t size()
	{
		return m_size;
	}

protected:
	size_t m_size;
};

class SafeOutFileBinary : public ofstream_binary
{
public:
	SafeOutFileBinary(const char* filename)
	: ofstream_binary(filename)
	{
		if (fail())
			throw FileOpenError(io::Str() << "Error opening file '" << filename << "'");
	}

	~SafeOutFileBinary()
	{
		if (is_open())
			close();
	}

};

#endif /* SAFEFILE_H_ */
