#ifndef __Exceptions_H_
#define __Exceptions_H_

#include <stdexcept>

namespace infomap {

using InputSyntaxError = std::runtime_error;
using UnknownFileTypeError = std::runtime_error;
using FileFormatError = std::runtime_error;
using InputDomainError = std::runtime_error;
using BadConversionError = std::runtime_error;
using InternalOrderError = std::logic_error;
using ImplementationError = std::runtime_error;
using DataDomainError = std::domain_error;
using BadArgumentError = std::runtime_error;
using OptionConflictError = std::runtime_error;
using FileOpenError = std::runtime_error;

} // namespace infomap

#endif //__Exceptions_H_
