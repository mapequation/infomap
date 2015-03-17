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


#ifndef SAFEFILE_H_
#define SAFEFILE_H_
#include <iostream>
#include <fstream>
#include <ios>
#include <stdexcept>
#include "convert.h"
#include <cstdio>

#ifdef NS_INFOMAP
namespace infomap
{
#endif

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
			throw FileOpenError(io::Str() << "Error opening file '" << filename <<
					"'. Check that the path points to a file and that you have read permissions.");
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
			throw FileOpenError(io::Str() << "Error opening file '" << filename <<
					"'. Check that the directory you are writing to exists and that you have write permissions.");
	}

	~SafeOutFile()
	{
		if (is_open())
			close();
	}
};


template<typename T>
struct BinaryHelper {
	static size_t write(T value, std::FILE* file)
	{
		return fwrite(reinterpret_cast<const char*>(&value), sizeof(value), 1, file);
	}

	static size_t read(T& value, ifstream& ifstream)
	{
		size_t size = sizeof(value);
		ifstream.read(reinterpret_cast<char*>(&value), size);
		return size;
	}
};

template<>
struct BinaryHelper<std::string> {
	static size_t write(std::string value, std::FILE* file)
	{
		unsigned short stringLength = static_cast<unsigned short>(value.length());
		fwrite(&stringLength, sizeof(stringLength), 1, file);
		return fwrite(value.c_str(), sizeof(char), value.length(), file);
	}

	static size_t read(std::string& value, ifstream& ifstream)
	{
		unsigned short stringLength;
		size_t size = sizeof(stringLength);
		ifstream.read(reinterpret_cast<char*>(&stringLength), size);
		if (stringLength > 0)
		{
			char cstr[stringLength];
			ifstream.read(cstr, stringLength);
			std::string str(cstr, stringLength);
			value.swap(str);
		}
		return size + sizeof(char) * stringLength;
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


class ifstream_binary : public ifstream
{
public:
	ifstream_binary(const char* filename)
		: ifstream(filename, ios_base::in | ios_base::binary), m_sizeRead(0) {}

	template<typename T>
	ifstream_binary& operator>>(T& value)
	{
		m_sizeRead += BinaryHelper<T>::read(value, *this);
		return *this;
	}

	size_t sizeRead()
	{
		return m_sizeRead;
	}

protected:
	size_t m_sizeRead;
};


/**
 * Wrapper class for writing low-level binary data with a simple stream interface.
 *
 * @note Strings are prepended with its length as unsigned short.
 */
class SafeBinaryOutFile {
public:
	SafeBinaryOutFile (const char* filename)
	: m_file(std::fopen(filename, "w"))
	{
        if (!m_file)
            throw std::runtime_error("file open failure");
    }

    ~SafeBinaryOutFile() {
        if (std::fclose(m_file)) {
           // failed to flush latest changes.
           // handle it
        }
    }

    template<typename T>
    SafeBinaryOutFile& operator<<(T value)
	{
    	BinaryHelper<T>::write(value, m_file);
    	return *this;
	}

    /**
     * Return the number of bytes written to the file stream
     */
	size_t size()
	{
		return ftell(m_file);
	}

private:
    std::FILE* m_file;

    // prevent copying and assignment; not implemented
	SafeBinaryOutFile (const SafeBinaryOutFile &);
	SafeBinaryOutFile & operator= (const SafeBinaryOutFile &);
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

class SafeBinaryInFile : public ifstream_binary
{
public:
	SafeBinaryInFile(const char* filename)
	: ifstream_binary(filename)
	{
		if (fail())
			throw FileOpenError(io::Str() << "Error opening file '" << filename << "'");
	}

	~SafeBinaryInFile()
	{
		if (is_open())
			close();
	}

};

inline
bool isDirectoryWritable(const std::string& dir)
{
	std::string path = io::Str() << dir << "_1nf0m4p_.tmp";
	bool ok = true;
	try {
		SafeOutFile out(path.c_str());
	}
	catch(const FileOpenError&) {
		ok = false;
	}
	if (ok)
		std::remove(path.c_str());
	return ok;
}

#ifdef NS_INFOMAP
}
#endif

#endif /* SAFEFILE_H_ */
