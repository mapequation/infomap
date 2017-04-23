#ifndef __Exceptions_H_
#define __Exceptions_H_

#include <stdexcept>


class AbortAndHelp : public std::runtime_error {
public:
    AbortAndHelp(std::string const &s) : std::runtime_error(s) {
    }
};

class InputSyntaxError : public std::runtime_error {
public:
    InputSyntaxError(std::string const &s) : std::runtime_error(s) {
    }
};

class UnknownFileTypeError : public std::runtime_error {
public:
    UnknownFileTypeError(std::string const &s) : std::runtime_error(s) {
    }
};

class FileFormatError : public std::runtime_error {
public:
    FileFormatError(std::string const &s) : std::runtime_error(s) {
    }
};

class InputDomainError : public std::runtime_error {
public:
    InputDomainError(std::string const &s) : std::runtime_error(s) {
    }
};

class BadConversionError : public std::runtime_error {
public:
    BadConversionError(std::string const &s) : std::runtime_error(s) {
    }
};

class MisMatchError : public std::runtime_error {
public:
    MisMatchError(std::string const &s) : std::runtime_error(s) {
    }
};

class InternalOrderError : public std::logic_error {
public:
    InternalOrderError(std::string const &s) : std::logic_error(s) {
    }
};

struct ImplementationError : public std::runtime_error {
    ImplementationError(std::string const &s) : std::runtime_error(s) {
    }
};

struct DataDomainError : public std::domain_error {
	DataDomainError(std::string const &s) : std::domain_error(s) {
    }
};

struct TypeError : public std::logic_error {
	TypeError(std::string const &s) : std::logic_error(s) {
    }
};


#endif //__Exceptions_H_
