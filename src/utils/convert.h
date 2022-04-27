/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Eriksson, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_AGPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef CONVERT_H_
#define CONVERT_H_

#include <stdexcept>
#include <iomanip>
#include <sstream>
#include <string>
#include <locale> // std::locale, std::tolower
#include <iostream>
#include <vector>

namespace infomap {

template <typename T>
struct TypeInfo {
  static std::string type() { return "undefined"; }
  static bool isNumeric() { return false; }
};
template <>
struct TypeInfo<bool> {
  static std::string type() { return "bool"; }
  static bool isNumeric() { return false; }
};
template <>
struct TypeInfo<int> {
  static std::string type() { return "int"; }
  static bool isNumeric() { return true; }
};
template <>
struct TypeInfo<unsigned int> {
  static std::string type() { return "unsigned int"; }
  static bool isNumeric() { return true; }
};
template <>
struct TypeInfo<double> {
  static std::string type() { return "double"; }
  static bool isNumeric() { return true; }
};

namespace io {

  inline std::string tolower(std::string str)
  {
    std::locale loc;
    for (std::string::size_type i = 0; i < str.length(); ++i)
      str[i] = std::tolower(str[i], loc);
    return str;
  }

  template <typename T>
  inline std::string stringify(T& x)
  {
    std::ostringstream o;
    if (!(o << x))
      throw std::runtime_error((o << "stringify(" << x << ")", o.str()));
    return o.str();
  }

  template <>
  inline std::string stringify(bool& b)
  {
    return b ? "true" : "false";
  }

  template <typename Container>
  inline std::string stringify(const Container& cont, std::string delimiter)
  {
    std::ostringstream o;
    if (cont.empty())
      return "";
    unsigned int maxIndex = cont.size() - 1;
    for (unsigned int i = 0; i < maxIndex; ++i) {
      if (!(o << cont[i]))
        throw std::runtime_error((o << "stringify(container[" << i << "])", o.str()));
      o << delimiter;
    }
    if (!(o << cont[maxIndex]))
      throw std::runtime_error((o << "stringify(container[" << maxIndex << "])", o.str()));
    return o.str();
  }

  template <typename Container>
  inline std::string stringify(const Container& cont, std::string delimiter, unsigned int offset)
  {
    std::ostringstream o;
    if (cont.empty())
      return "";
    unsigned int maxIndex = cont.size() - 1;
    for (unsigned int i = 0; i < maxIndex; ++i) {
      if (!(o << (cont[i] + offset)))
        throw std::runtime_error((o << "stringify(container[" << i << "])", o.str()));
      o << delimiter;
    }
    if (!(o << (cont[maxIndex] + offset)))
      throw std::runtime_error((o << "stringify(container[" << maxIndex << "])", o.str()));
    return o.str();
  }

  template <typename Container>
  inline std::string stringifyContainer(const Container& cont, std::string delimiter, unsigned int startIndex = 0, unsigned int length = 0)
  {
    std::ostringstream o;
    if (cont.empty())
      return "";
    if (startIndex >= cont.size())
      throw std::out_of_range("stringifyContainer called with startIndex out of range");
    unsigned int stopIndex = length == 0 || (startIndex + length > cont.size()) ? cont.size() : (startIndex + length);
    unsigned int endIndex = stopIndex - 1;
    for (unsigned int i = startIndex; i < endIndex; ++i) {
      if (!(o << (cont[i])))
        throw std::runtime_error((o << "stringifyContainer(container[" << i << "])", o.str()));
      o << delimiter;
    }
    if (!(o << (cont[endIndex])))
      throw std::runtime_error((o << "stringifyContainer(container[" << endIndex << "])", o.str()));
    return o.str();
  }

  struct InsensitiveCompare {
    bool operator()(const std::string& a, const std::string& b) const
    {
      auto lhs = a.begin();
      auto rhs = b.begin();

      std::locale loc;
      for (; lhs != a.end() && rhs != b.end(); ++lhs, ++rhs) {
        auto lhs_val = std::tolower(*lhs, loc);
        auto rhs_val = std::tolower(*rhs, loc);

        if (lhs_val != rhs_val)
          return lhs_val < rhs_val;
      }

      return (rhs != b.end());
    }
  };

  inline std::vector<std::string>& split(const std::string& s, char delim, std::vector<std::string>& items)
  {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
      if (item.length() > 0) {
        items.push_back(item);
      }
    }
    return items;
  }

  inline std::vector<std::string> split(const std::string& s, char delim)
  {
    std::vector<std::string> items;
    split(s, delim, items);
    return items;
  }

  class Str {
  public:
    Str() = default;
    template <class T>
    Str& operator<<(const T& t)
    {
      m_oss << stringify(t);
      return *this;
    }
    Str& operator<<(std::ostream& (*f)(std::ostream&))
    {
      m_oss << f;
      return *this;
    }
    operator std::string() const
    {
      return m_oss.str();
    }

  private:
    std::ostringstream m_oss;
  };

  template <typename T>
  inline bool stringToValue(std::string const& str, T& value)
  {
    std::istringstream istream(str);
    return !!(istream >> value);
  }

  template <>
  inline bool stringToValue<bool>(std::string const& str, bool& value)
  {
    std::istringstream istream(str);
    return !!(istream >> value);
  }

  template <typename T>
  inline T parse(std::string const& str)
  {
    std::istringstream istream(str);
    T value;
    if (!(istream >> value))
      throw std::runtime_error(Str() << "Error converting '" << str << "' to " << TypeInfo<T>::type());
    return value;
  }

  // Template specialization for bool type to correctly parse "true" and "false"
  template <>
  inline bool parse<bool>(std::string const& str)
  {
    std::istringstream istream(str);
    istream.setf(std::ios::boolalpha);
    bool value;
    if (!(istream >> value))
      throw std::runtime_error(Str() << "Error converting '" << str << "' to bool");
    return value;
  }

  inline std::string firstWord(const std::string& line)
  {
    std::istringstream ss;
    std::string buf;
    ss.str(line);
    ss >> buf;
    return buf;
  }

  inline void padString(std::string& str, const std::string::size_type newSize, const char paddingChar = ' ')
  {
    if (newSize > str.size())
      str.append(newSize - str.size(), paddingChar);
  }

  template <typename T>
  inline std::string padValue(T value, const std::string::size_type size, bool rightAligned = true, const char paddingChar = ' ')
  {
    std::string valStr = stringify(value);
    if (size == valStr.size())
      return valStr;
    if (size < valStr.size())
      return valStr.substr(0, size);

    if (!rightAligned)
      return valStr.append(size - valStr.size(), paddingChar);

    return std::string(size - valStr.size(), paddingChar).append(valStr);
  }

  inline std::string toPrecision(double value, unsigned int precision = 10, bool fixed = false)
  {
    std::ostringstream o;
    if (fixed)
      o << std::fixed << std::setprecision(precision);
    else
      o << std::setprecision(precision);
    if (!(o << value))
      throw std::runtime_error((o << "stringify(" << value << ")", o.str()));
    return o.str();
  }

  inline std::string toPlural(std::string object, unsigned int num)
  {
    if (num > 1 || num == 0)
      object.push_back('s');
    return object;
  }

} // namespace io

} // namespace infomap

#endif /* CONVERT_H_ */
