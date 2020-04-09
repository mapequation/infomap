#ifndef __Exceptions_H_
#define __Exceptions_H_

#include <stdexcept>

namespace infomap {

struct AbortAndHelp : public std::runtime_error {
  AbortAndHelp(std::string const& s) : std::runtime_error(s) {}
};

struct InputSyntaxError : public std::runtime_error {
  InputSyntaxError(std::string const& s) : std::runtime_error(s) {}
};

struct UnknownFileTypeError : public std::runtime_error {
  UnknownFileTypeError(std::string const& s) : std::runtime_error(s) {}
};

struct FileFormatError : public std::runtime_error {
  FileFormatError(std::string const& s) : std::runtime_error(s) {}
};

struct InputDomainError : public std::runtime_error {
  InputDomainError(std::string const& s) : std::runtime_error(s) {}
};

struct BadConversionError : public std::runtime_error {
  BadConversionError(std::string const& s) : std::runtime_error(s) {}
};

struct MisMatchError : public std::runtime_error {
  MisMatchError(std::string const& s) : std::runtime_error(s) {}
};

struct InternalOrderError : public std::logic_error {
  InternalOrderError(std::string const& s) : std::logic_error(s) {}
};

struct ImplementationError : public std::runtime_error {
  ImplementationError(std::string const& s) : std::runtime_error(s) {}
};

struct DataDomainError : public std::domain_error {
  DataDomainError(std::string const& s) : std::domain_error(s) {}
};

struct ArgumentOutOfRangeError : public std::out_of_range {
  ArgumentOutOfRangeError(std::string const& s) : std::out_of_range(s) {}
};

struct BadArgumentError : public std::runtime_error {
  BadArgumentError(std::string const& s) : std::runtime_error(s) {}
};

struct TypeError : public std::logic_error {
  TypeError(std::string const& s) : std::logic_error(s) {}
};

struct OptionConflictError : public std::logic_error {
  OptionConflictError(std::string const& s) : std::logic_error(s) {}
};

struct FileOpenError : public std::runtime_error {
  FileOpenError(std::string const& s) : std::runtime_error(s) {}
};

} // namespace infomap

#endif //__Exceptions_H_
