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


#ifndef CONVERT_H_
#define CONVERT_H_

#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>

class AbortAndHelp : public std::runtime_error {
public:
	AbortAndHelp(std::string const& s) : std::runtime_error(s) { }
};

class InputSyntaxError : public std::runtime_error {
public:
	InputSyntaxError(std::string const& s) : std::runtime_error(s) { }
};

class UnknownFileTypeError : public std::runtime_error {
public:
	UnknownFileTypeError(std::string const& s) : std::runtime_error(s) { }
};

class FileFormatError : public std::runtime_error {
public:
	FileFormatError(std::string const& s) : std::runtime_error(s) { }
};

class InputDomainError : public std::runtime_error {
public:
	InputDomainError(std::string const& s) : std::runtime_error(s) { }
};

class BadConversionError : public std::runtime_error {
public:
	BadConversionError(std::string const& s) : std::runtime_error(s) { }
};

class MisMatchError : public std::runtime_error {
public:
	MisMatchError(std::string const& s) : std::runtime_error(s) { }
};

class InternalOrderError : public std::logic_error {
public:
	InternalOrderError(std::string const& s) : std::logic_error(s) { }
};

template<typename T>
struct TypeInfo { static std::string name() { return "non-specialized type"; } };
template<>
struct TypeInfo<bool> { static std::string name() { return "bool"; } };
template<>
struct TypeInfo<int> { static std::string name() { return "int"; } };
template<>
struct TypeInfo<unsigned int> { static std::string name() { return "unsigned int"; } };
template<>
struct TypeInfo<double> { static std::string name() { return "double"; } };


namespace io
{

class Str {
public:
	Str() {}
	template<class T> Str& operator << (const T& t) {
		m_oss << t;
		return *this;
	}
	Str& operator << (std::ostream& (*f) (std::ostream&)) {
		m_oss << f;
		return *this;
	}
	operator std::string() const {
		return m_oss.str();
	}
private:
	std::ostringstream m_oss;
};

template<typename T>
inline T stringToValue(std::string const& str)
{
	std::istringstream istream(str);
	T value;
	if (!(istream >> value))
		throw BadConversionError(Str() << "Error converting '" << str << "' to " << TypeInfo<T>::name());
	return value;
}

// Template specialization for bool type to correctly parse "true" and "false"
template<>
inline bool stringToValue<bool>(std::string const& str)
{
	std::istringstream istream(str);
	istream.setf(std::ios::boolalpha);
	bool value;
	if (!(istream >> value))
		throw BadConversionError(Str() << "Error converting '" << str << "' to bool");
	return value;
}

template<typename T>
inline std::string stringify(T x)
{
	std::ostringstream o;
	if (!(o << x))
		throw BadConversionError((o << "stringify(" << x << ")", o.str()));
	return o.str();
}

inline void padString(std::string &str, const std::string::size_type newSize, const char paddingChar = ' ')
{
	if(newSize > str.size())
		str.assign(newSize, paddingChar);
}

}

#endif /* CONVERT_H_ */
