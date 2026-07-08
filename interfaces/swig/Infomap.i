%module infomap
#pragma SWIG nowarn=302,325,362,383,389,503,508,509

%include "exceptions.i"

%{
/* Includes the header in the wrapper code */
#include "src/Infomap.h"
#include "src/io/Features.h"
#include "src/utils/Log.h"
#ifdef SWIGPYTHON
namespace infomap {
int run(const std::string& flags);
}
#include <Python.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
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
    if (itemSize == 1) {
      value = readUnaligned<std::uint8_t>(data);
    } else if (itemSize == 2) {
      value = readUnaligned<std::uint16_t>(data);
    } else if (itemSize == 4) {
      value = readUnaligned<std::uint32_t>(data);
    } else if (itemSize == 8) {
      value = readUnaligned<std::uint64_t>(data);
    } else {
      throw std::runtime_error("Unsupported unsigned integer itemsize for NumPy link ids");
    }
  } else if (kind == "i") {
    std::int64_t signedValue = 0;
    if (itemSize == 1) {
      signedValue = readUnaligned<std::int8_t>(data);
    } else if (itemSize == 2) {
      signedValue = readUnaligned<std::int16_t>(data);
    } else if (itemSize == 4) {
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
    if (itemSize == 1)
      return readUnaligned<std::uint8_t>(data);
    if (itemSize == 2)
      return readUnaligned<std::uint16_t>(data);
    if (itemSize == 4)
      return readUnaligned<std::uint32_t>(data);
    if (itemSize == 8)
      return static_cast<double>(readUnaligned<std::uint64_t>(data));
    throw std::runtime_error("Unsupported unsigned integer itemsize for NumPy link weights");
  }
  if (kind == "i") {
    if (itemSize == 1)
      return readUnaligned<std::int8_t>(data);
    if (itemSize == 2)
      return readUnaligned<std::int16_t>(data);
    if (itemSize == 4)
      return readUnaligned<std::int32_t>(data);
    if (itemSize == 8)
      return static_cast<double>(readUnaligned<std::int64_t>(data));
    throw std::runtime_error("Unsupported signed integer itemsize for NumPy link weights");
  }
  throw std::runtime_error("Unsupported NumPy dtype for link weights");
}

std::size_t expectedNumpy2DBytes(std::size_t numRows, unsigned int numColumns, unsigned int itemSize)
{
  if (itemSize == 0) {
    throw std::runtime_error("Invalid NumPy itemsize for link array");
  }

  const auto rowBytes = static_cast<std::size_t>(numColumns) * itemSize;
  if (numColumns != 0 && rowBytes / numColumns != itemSize) {
    throw std::runtime_error("NumPy link array is too large");
  }

  const auto expectedBytes = numRows * rowBytes;
  if (numRows != 0 && expectedBytes / numRows != rowBytes) {
    throw std::runtime_error("NumPy link array is too large");
  }
  return expectedBytes;
}

void addLinksFromNumpy2D(InfomapWrapper& infomap, PyObject* links, std::size_t numRows, unsigned int numColumns, const std::string& dtypeKind, unsigned int itemSize)
{
  if (numColumns != 2 && numColumns != 3) {
    throw std::invalid_argument("NumPy link arrays must have 2 or 3 columns");
  }

  PyBufferView linksBuffer(links, "links");
  const auto expectedBytes = expectedNumpy2DBytes(numRows, numColumns, itemSize);
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

void addMultilayerLinksFromNumpy2D(InfomapWrapper& infomap, PyObject* links, std::size_t numRows, unsigned int numColumns, const std::string& dtypeKind, unsigned int itemSize)
{
  if (numColumns != 4 && numColumns != 5) {
    throw std::invalid_argument("NumPy multilayer link arrays must have 4 or 5 columns");
  }

  PyBufferView linksBuffer(links, "links");
  const auto expectedBytes = expectedNumpy2DBytes(numRows, numColumns, itemSize);
  if (linksBuffer.size<char>() != expectedBytes) {
    throw std::runtime_error("NumPy multilayer link array buffer size does not match its shape");
  }

  std::vector<unsigned int> sourceLayerIds;
  std::vector<unsigned int> sourceNodeIds;
  std::vector<unsigned int> targetLayerIds;
  std::vector<unsigned int> targetNodeIds;
  std::vector<double> weights;
  sourceLayerIds.reserve(numRows);
  sourceNodeIds.reserve(numRows);
  targetLayerIds.reserve(numRows);
  targetNodeIds.reserve(numRows);
  weights.reserve(numRows);

  const auto* data = linksBuffer.data<char>();
  const auto rowStride = static_cast<std::size_t>(numColumns) * itemSize;
  for (std::size_t i = 0; i < numRows; ++i) {
    const auto* row = data + i * rowStride;
    sourceLayerIds.push_back(readUnsignedId(row, dtypeKind, itemSize, "source layer id"));
    sourceNodeIds.push_back(readUnsignedId(row + itemSize, dtypeKind, itemSize, "source node id"));
    targetLayerIds.push_back(readUnsignedId(row + 2 * itemSize, dtypeKind, itemSize, "target layer id"));
    targetNodeIds.push_back(readUnsignedId(row + 3 * itemSize, dtypeKind, itemSize, "target node id"));
    weights.push_back(numColumns == 5 ? readWeight(row + 4 * itemSize, dtypeKind, itemSize) : 1.0);
  }

  infomap.addMultilayerLinks(sourceLayerIds, sourceNodeIds, targetLayerIds, targetNodeIds, weights);
}

void addMultilayerIntraLinksFromNumpy2D(InfomapWrapper& infomap, PyObject* links, std::size_t numRows, unsigned int numColumns, const std::string& dtypeKind, unsigned int itemSize)
{
  if (numColumns != 3 && numColumns != 4) {
    throw std::invalid_argument("NumPy multilayer intra-link arrays must have 3 or 4 columns");
  }

  PyBufferView linksBuffer(links, "links");
  const auto expectedBytes = expectedNumpy2DBytes(numRows, numColumns, itemSize);
  if (linksBuffer.size<char>() != expectedBytes) {
    throw std::runtime_error("NumPy multilayer intra-link array buffer size does not match its shape");
  }

  std::vector<unsigned int> layerIds;
  std::vector<unsigned int> sourceNodeIds;
  std::vector<unsigned int> targetNodeIds;
  std::vector<double> weights;
  layerIds.reserve(numRows);
  sourceNodeIds.reserve(numRows);
  targetNodeIds.reserve(numRows);
  weights.reserve(numRows);

  const auto* data = linksBuffer.data<char>();
  const auto rowStride = static_cast<std::size_t>(numColumns) * itemSize;
  for (std::size_t i = 0; i < numRows; ++i) {
    const auto* row = data + i * rowStride;
    layerIds.push_back(readUnsignedId(row, dtypeKind, itemSize, "layer id"));
    sourceNodeIds.push_back(readUnsignedId(row + itemSize, dtypeKind, itemSize, "source node id"));
    targetNodeIds.push_back(readUnsignedId(row + 2 * itemSize, dtypeKind, itemSize, "target node id"));
    weights.push_back(numColumns == 4 ? readWeight(row + 3 * itemSize, dtypeKind, itemSize) : 1.0);
  }

  infomap.addMultilayerIntraLinks(layerIds, sourceNodeIds, targetNodeIds, weights);
}

void addMultilayerInterLinksFromNumpy2D(InfomapWrapper& infomap, PyObject* links, std::size_t numRows, unsigned int numColumns, const std::string& dtypeKind, unsigned int itemSize)
{
  if (numColumns != 3 && numColumns != 4) {
    throw std::invalid_argument("NumPy multilayer inter-link arrays must have 3 or 4 columns");
  }

  PyBufferView linksBuffer(links, "links");
  const auto expectedBytes = expectedNumpy2DBytes(numRows, numColumns, itemSize);
  if (linksBuffer.size<char>() != expectedBytes) {
    throw std::runtime_error("NumPy multilayer inter-link array buffer size does not match its shape");
  }

  std::vector<unsigned int> sourceLayerIds;
  std::vector<unsigned int> nodeIds;
  std::vector<unsigned int> targetLayerIds;
  std::vector<double> weights;
  sourceLayerIds.reserve(numRows);
  nodeIds.reserve(numRows);
  targetLayerIds.reserve(numRows);
  weights.reserve(numRows);

  const auto* data = linksBuffer.data<char>();
  const auto rowStride = static_cast<std::size_t>(numColumns) * itemSize;
  for (std::size_t i = 0; i < numRows; ++i) {
    const auto* row = data + i * rowStride;
    sourceLayerIds.push_back(readUnsignedId(row, dtypeKind, itemSize, "source layer id"));
    nodeIds.push_back(readUnsignedId(row + itemSize, dtypeKind, itemSize, "node id"));
    targetLayerIds.push_back(readUnsignedId(row + 2 * itemSize, dtypeKind, itemSize, "target layer id"));
    weights.push_back(numColumns == 4 ? readWeight(row + 3 * itemSize, dtypeKind, itemSize) : 1.0);
  }

  infomap.addMultilayerInterLinks(sourceLayerIds, nodeIds, targetLayerIds, weights);
}

} // namespace
} // namespace infomap
#endif
%}

// Makes it possible to use for example numpy.int64 as link weights
%begin %{
#define SWIG_PYTHON_CAST_MODE
%}

// --- Cooperative interruption host bridge (issue #412) ---
// run() installs a poll the core invokes only on the owner (R/Python main)
// thread; the C++ seam is %ignore'd in InfomapBase.i — this is the language glue.
%{
#if defined(SWIGR)
#include <R_ext/Utils.h>   // R_CheckUserInterrupt
#include <Rinternals.h>    // R_ToplevelExec
namespace {
// R_ToplevelExec contains R_CheckUserInterrupt's longjmp (so C++ destructors run)
// and reports a pending interrupt as a non-TRUE result. R main thread only.
// A cancel surfaces to R as a plain error (-> Rf_error), NOT an "interrupt"
// condition — handle with tryCatch(error=), not tryCatch(interrupt=).
void infomapRCheckInterrupt(void*) { R_CheckUserInterrupt(); }
bool infomapHostInterruptPoll(void*) { return R_ToplevelExec(infomapRCheckInterrupt, nullptr) != TRUE; }
} // namespace
#elif defined(SWIGPYTHON)
namespace infomap { extern InterruptCallback g_runInterruptCallback; } // defined in main.cpp
namespace {
// PyErr_CheckSignals runs pending Python signal handlers (turns a queued SIGINT
// into KeyboardInterrupt). Needs the GIL + main thread; off the main thread it is
// a no-op, so such a run is simply non-interruptible.
bool infomapHostInterruptPoll(void*) { return PyErr_CheckSignals() != 0; }
} // namespace
#endif
%}

#if defined(SWIGR) || defined(SWIGPYTHON)
%exception infomap::InfomapBase::run {
  arg1->setInterruptHandler(&infomapHostInterruptPoll, nullptr);
  try {
    $action
  } catch (const infomap::InterruptionError&) {
    arg1->clearInterruptHandler();
#if defined(SWIGPYTHON)
    // Propagate the pending KeyboardInterrupt rather than masking it.
    if (PyErr_Occurred()) SWIG_fail;
#endif
    SWIG_exception(SWIG_RuntimeError, "Infomap run interrupted.");
  } catch (const std::exception& e) {
    arg1->clearInterruptHandler();
    SWIG_exception(SWIG_RuntimeError, e.what());
  }
  arg1->clearInterruptHandler();
}
#endif

#ifdef SWIGPYTHON
// Make the run(flags) free function (the pip `infomap` console script) poll too.
%init %{
  infomap::g_runInterruptCallback = &infomapHostInterruptPoll;
%}
#endif

%include "std_string.i"
%include "Config.i"
%include "InfomapBase.i"

%include "std_map.i"
%include "std_vector.i"
%include "std_pair.i"

namespace std {
    %template(vector_uint) std::vector<unsigned int>;
#ifndef SWIGPYTHON
    %template(vector_link_result) std::vector<infomap::LinkResult>;
#endif
    %template(map_uint_uint) std::map<unsigned int, unsigned int>;
    %template(map_uint_vector_uint) std::map<unsigned int, std::vector<unsigned int>>;
    %template(map_uint_string) std::map<unsigned int, std::string>;
    %template(pair_uint_uint) std::pair<unsigned int, unsigned int>;
    %template(map_pair_uint_uint_double) std::map<std::pair<unsigned int, unsigned int>, double>;
}

#ifdef SWIGPYTHON
%{
namespace {

// Bridges infomap::Log lines to a Python callable `callback(level, line)`.
//
// Threading contract (see issue #745): the SWIG module never releases the
// GIL, so during run() the *calling* thread holds it for the whole call and
// may enter Python directly. Lines written by other threads (OpenMP task
// threads emit -vv detail lines) must NEVER acquire the GIL — the calling
// thread waits for them at OpenMP barriers while holding it, so a worker
// blocking in PyGILState_Ensure() would deadlock. Such lines are queued
// under a mutex and drained by the next GIL-holding write, or by
// _drain_log_queue() after run() returns.
class PythonLogSink : public infomap::LogSink {
public:
  explicit PythonLogSink(PyObject* callable) : m_callable(callable)
  {
    Py_INCREF(m_callable);
  }

  // Only constructed/destroyed from Python calls (GIL held).
  ~PythonLogSink() override { Py_DECREF(m_callable); }

  PythonLogSink(const PythonLogSink&) = delete;
  PythonLogSink& operator=(const PythonLogSink&) = delete;

  void writeLine(unsigned int level, const std::string& line) override
  {
    if (!PyGILState_Check()) {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_queued.emplace_back(level, line);
      return;
    }
    drainQueued();
    deliver(level, line);
  }

  void drainQueued()
  {
    // Caller must hold the GIL.
    std::vector<std::pair<unsigned int, std::string>> pending;
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      pending.swap(m_queued);
    }
    for (const auto& entry : pending)
      deliver(entry.first, entry.second);
  }

private:
  void deliver(unsigned int level, const std::string& line)
  {
    PyObject* result = PyObject_CallFunction(m_callable, "(Is)", level, line.c_str());
    if (result == nullptr) {
      // A raising log callback must not kill the engine run — same policy
      // as the logging module's own handler-error swallowing.
      PyErr_WriteUnraisable(m_callable);
      return;
    }
    Py_DECREF(result);
  }

  PyObject* m_callable;
  std::mutex m_mutex;
  std::vector<std::pair<unsigned int, std::string>> m_queued;
};

PythonLogSink* g_pythonLogSink = nullptr;

} // namespace
%}

%inline %{
namespace infomap {
// Install `callback(level, line)` as the process-global engine log sink,
// replacing stream output; pass None to uninstall and restore stream output.
// Called with the GIL held (any Python call). Must not race with a running
// engine — Log state is process-global.
void _set_log_callback(PyObject* callback)
{
  // Fail fast on a non-callable: installing it would turn every log write
  // into a swallowed PyErr_WriteUnraisable, silently disabling the log.
  if (callback != Py_None && PyCallable_Check(callback) == 0) {
    throw std::runtime_error("log callback must be callable (or None to uninstall)");
  }
  if (g_pythonLogSink != nullptr) {
    Log::setSink(nullptr);
    delete g_pythonLogSink;
    g_pythonLogSink = nullptr;
  }
  if (callback != Py_None) {
    g_pythonLogSink = new PythonLogSink(callback);
    Log::setSink(g_pythonLogSink);
  }
}

// Deliver lines queued by non-GIL-holding threads and flush the calling
// thread's trailing partial line. Call after an engine run returns.
void _drain_log_queue()
{
  Log::flushSinkLines();
  if (g_pythonLogSink != nullptr)
    g_pythonLogSink->drainQueued();
}
}
%}

%rename(_enabled_features_string) infomap::pythonEnabledFeaturesString;
%inline %{
namespace infomap {
std::string pythonEnabledFeaturesString()
{
  const auto features = enabledFeatures();
  std::string encoded;
  for (const auto& feature : features) {
    if (!encoded.empty())
      encoded += ",";
    encoded += feature;
  }
  return encoded;
}
}
%}

%insert("python") %{
def build_info():
    features = _infomap._enabled_features_string()
    enabled_features = tuple(feature for feature in features.split(",") if feature)
    return {"enabled_features": enabled_features}
%}
#endif

#ifdef SWIGPYTHON
// run(flags) rethrows InterruptionError under AS_LIB; propagate the pending
// KeyboardInterrupt here rather than masking it with a RuntimeError.
%exception infomap::run {
  try {
    $action
  } catch (const infomap::InterruptionError&) {
    if (PyErr_Occurred()) SWIG_fail;
    SWIG_exception(SWIG_RuntimeError, "Infomap run interrupted.");
  } catch (const std::exception& e) {
    SWIG_exception(SWIG_RuntimeError, e.what());
  }
}
#endif

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

    void addMultilayerLinksFromNumpy2D(PyObject* links, std::size_t numRows, unsigned int numColumns, const std::string& dtypeKind, unsigned int itemSize)
    {
        infomap::addMultilayerLinksFromNumpy2D(*$self, links, numRows, numColumns, dtypeKind, itemSize);
    }

    void addMultilayerIntraLinksFromNumpy2D(PyObject* links, std::size_t numRows, unsigned int numColumns, const std::string& dtypeKind, unsigned int itemSize)
    {
        infomap::addMultilayerIntraLinksFromNumpy2D(*$self, links, numRows, numColumns, dtypeKind, itemSize);
    }

    void addMultilayerInterLinksFromNumpy2D(PyObject* links, std::size_t numRows, unsigned int numColumns, const std::string& dtypeKind, unsigned int itemSize)
    {
        infomap::addMultilayerInterLinksFromNumpy2D(*$self, links, numRows, numColumns, dtypeKind, itemSize);
    }

	// overrides to convert swig map proxy object to pure dict
	%insert("python") %{
        def getModules(self, level=1, states=False):
            return dict(_infomap.InfomapWrapper_getModules(self, level, states))

        def getMultilevelModules(self, states=False):
            return dict(_infomap.InfomapWrapper_getMultilevelModules(self, states))

        def getNames(self):
            return dict(_infomap.InfomapWrapper_getNames(self))

        def getStateNames(self):
            return dict(_infomap.InfomapWrapper_getStateNames(self))

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
