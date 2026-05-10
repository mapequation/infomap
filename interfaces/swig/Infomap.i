%module infomap
#pragma SWIG nowarn=302,325,362,383,389,503,508,509

%include "exceptions.i"

%{
/* Includes the header in the wrapper code */
#include "src/Infomap.h"
#ifdef SWIGPYTHON
namespace infomap {
int run(const std::string& flags);
}
#include <Python.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#endif
// SWIG strips namespaces, so include infomap in global namespace in wrapper code
using namespace infomap;

#ifdef SWIGPYTHON
namespace infomap {
namespace {

class PyBufferView {
public:
  PyBufferView(PyObject* object, const char* name) : m_name(name)
  {
    if (PyObject_GetBuffer(object, &m_view, PyBUF_SIMPLE) != 0) {
      PyErr_Clear();
      throw std::runtime_error(std::string("Expected a contiguous NumPy-compatible buffer for ") + m_name);
    }
  }

  ~PyBufferView()
  {
    PyBuffer_Release(&m_view);
  }

  PyBufferView(const PyBufferView&) = delete;
  PyBufferView& operator=(const PyBufferView&) = delete;

  template <typename T>
  const T* data() const
  {
    if (m_view.len < 0 || static_cast<std::size_t>(m_view.len) % sizeof(T) != 0) {
      throw std::runtime_error(std::string("Invalid buffer size for ") + m_name);
    }
    return static_cast<const T*>(m_view.buf);
  }

  template <typename T>
  std::size_t size() const
  {
    if (m_view.len < 0 || static_cast<std::size_t>(m_view.len) % sizeof(T) != 0) {
      throw std::runtime_error(std::string("Invalid buffer size for ") + m_name);
    }
    return static_cast<std::size_t>(m_view.len) / sizeof(T);
  }

private:
  Py_buffer m_view {};
  const char* m_name;
};

template <typename T>
T readUnaligned(const char* data)
{
  T value {};
  std::memcpy(&value, data, sizeof(T));
  return value;
}

unsigned int readUnsignedId(const char* data, const std::string& kind, unsigned int itemSize, const char* name)
{
  std::uint64_t value = 0;
  if (kind == "u") {
    if (itemSize == 4) {
      value = readUnaligned<std::uint32_t>(data);
    } else if (itemSize == 8) {
      value = readUnaligned<std::uint64_t>(data);
    } else {
      throw std::runtime_error("Unsupported unsigned integer itemsize for NumPy link ids");
    }
  } else if (kind == "i") {
    std::int64_t signedValue = 0;
    if (itemSize == 4) {
      signedValue = readUnaligned<std::int32_t>(data);
    } else if (itemSize == 8) {
      signedValue = readUnaligned<std::int64_t>(data);
    } else {
      throw std::runtime_error("Unsupported signed integer itemsize for NumPy link ids");
    }
    if (signedValue < 0) {
      throw std::runtime_error(std::string("NumPy link array contains negative ") + name);
    }
    value = static_cast<std::uint64_t>(signedValue);
  } else if (kind == "f") {
    double floatValue = 0.0;
    if (itemSize == 4) {
      floatValue = readUnaligned<float>(data);
    } else if (itemSize == 8) {
      floatValue = readUnaligned<double>(data);
    } else {
      throw std::runtime_error("Unsupported floating point itemsize for NumPy link ids");
    }
    if (!std::isfinite(floatValue) || floatValue < 0.0 || floatValue > static_cast<double>(std::numeric_limits<unsigned int>::max())) {
      throw std::runtime_error(std::string("NumPy link array contains invalid ") + name);
    }
    value = static_cast<std::uint64_t>(floatValue);
  } else {
    throw std::runtime_error("Unsupported NumPy dtype for link ids");
  }

  if (value > std::numeric_limits<unsigned int>::max()) {
    throw std::runtime_error(std::string("NumPy link array contains out-of-range ") + name);
  }
  return static_cast<unsigned int>(value);
}

double readWeight(const char* data, const std::string& kind, unsigned int itemSize)
{
  if (kind == "f") {
    if (itemSize == 4)
      return readUnaligned<float>(data);
    if (itemSize == 8)
      return readUnaligned<double>(data);
    throw std::runtime_error("Unsupported floating point itemsize for NumPy link weights");
  }
  if (kind == "u") {
    if (itemSize == 4)
      return readUnaligned<std::uint32_t>(data);
    if (itemSize == 8)
      return static_cast<double>(readUnaligned<std::uint64_t>(data));
    throw std::runtime_error("Unsupported unsigned integer itemsize for NumPy link weights");
  }
  if (kind == "i") {
    if (itemSize == 4)
      return readUnaligned<std::int32_t>(data);
    if (itemSize == 8)
      return static_cast<double>(readUnaligned<std::int64_t>(data));
    throw std::runtime_error("Unsupported signed integer itemsize for NumPy link weights");
  }
  throw std::runtime_error("Unsupported NumPy dtype for link weights");
}

void addLinksFromNumpy2D(InfomapWrapper& infomap, PyObject* links, std::size_t numRows, unsigned int numColumns, const std::string& dtypeKind, unsigned int itemSize)
{
  if (numColumns != 2 && numColumns != 3) {
    throw std::invalid_argument("NumPy link arrays must have 2 or 3 columns");
  }
  if (itemSize == 0) {
    throw std::runtime_error("Invalid NumPy itemsize for link array");
  }

  PyBufferView linksBuffer(links, "links");
  const auto expectedBytes = numRows * static_cast<std::size_t>(numColumns) * itemSize;
  if (numRows != 0 && expectedBytes / numRows != static_cast<std::size_t>(numColumns) * itemSize) {
    throw std::runtime_error("NumPy link array is too large");
  }
  if (linksBuffer.size<char>() != expectedBytes) {
    throw std::runtime_error("NumPy link array buffer size does not match its shape");
  }

  const auto* data = linksBuffer.data<char>();
  const auto rowStride = static_cast<std::size_t>(numColumns) * itemSize;
  for (std::size_t i = 0; i < numRows; ++i) {
    const auto* row = data + i * rowStride;
    const auto source = readUnsignedId(row, dtypeKind, itemSize, "source id");
    const auto target = readUnsignedId(row + itemSize, dtypeKind, itemSize, "target id");
    const auto weight = numColumns == 3 ? readWeight(row + 2 * itemSize, dtypeKind, itemSize) : 1.0;
    infomap.addLink(source, target, weight);
  }
}

} // namespace
} // namespace infomap
#endif
%}

// Makes it possible to use for example numpy.int64 as link weights
%begin %{
#define SWIG_PYTHON_CAST_MODE
%}

%include "std_string.i"
%include "Config.i"
%include "InfomapBase.i"

%include "std_map.i"
%include "std_vector.i"
%include "std_pair.i"

namespace std {
    %template(vector_uint) std::vector<unsigned int>;
    %template(map_uint_uint) std::map<unsigned int, unsigned int>;
    %template(map_uint_vector_uint) std::map<unsigned int, std::vector<unsigned int>>;
    %template(map_uint_string) std::map<unsigned int, std::string>;
    %template(pair_uint_uint) std::pair<unsigned int, unsigned int>;
    %template(map_pair_uint_uint_double) std::map<std::pair<unsigned int, unsigned int>, double>;
}

namespace infomap {
#ifdef SWIGPYTHON
int run(const std::string& flags);
#endif
}

%extend infomap::InfomapBase
{
    double elapsedTime()
    {
        return $self->getElapsedTime().getElapsedTimeInSec();
    }
}

#ifdef SWIGPYTHON
%extend infomap::InfomapWrapper
{
    void addLinksFromNumpy2D(PyObject* links, std::size_t numRows, unsigned int numColumns, const std::string& dtypeKind, unsigned int itemSize)
    {
        infomap::addLinksFromNumpy2D(*$self, links, numRows, numColumns, dtypeKind, itemSize);
    }

	// overrides to convert swig map proxy object to pure dict
	%insert("python") %{
        def getModules(self, level=1, states=False):
            return dict(_infomap.InfomapWrapper_getModules(self, level, states))

        def getMultilevelModules(self, states=False):
            return dict(_infomap.InfomapWrapper_getMultilevelModules(self, states))

        def getNames(self):
            return dict(_infomap.InfomapWrapper_getNames(self))

        def getLinks(self, flow=False):
            return dict(_infomap.InfomapWrapper_getLinks(self, flow))
    %}
}
#endif

#ifdef SWIGR
// R-only: route Infomap log output through Rprintf/REprintf so the
// optimizer's progress text appears in the R console (and obeys R's
// output buffering / sink redirection) instead of going to a null sink.
// The hook is exposed to R as `infomap::initRLogging()` and called from
// the package's .onLoad.
%{
#include "src/utils/Log.h"
#include <R_ext/Print.h>
#include <ostream>
#include <streambuf>

namespace {

class RPrintStreambuf : public std::streambuf {
protected:
  int_type overflow(int_type c) override
  {
    if (c != traits_type::eof()) {
      char ch = static_cast<char>(c);
      Rprintf("%c", ch);
    }
    return c;
  }
  std::streamsize xsputn(const char* s, std::streamsize n) override
  {
    Rprintf("%.*s", static_cast<int>(n), s);
    return n;
  }
};

} // namespace

namespace infomap {
void initRLogging()
{
  static RPrintStreambuf rbuf;
  static std::ostream rstream(&rbuf);
  Log::setOutputStream(rstream);
}
} // namespace infomap
%}

%inline %{
namespace infomap {
void initRLogging();
}
%}
#endif

/* Parse the header file to generate wrappers */
%include "src/Infomap.h"
