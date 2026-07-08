// Nanobind spike for issue #743: a minimal engine binding behind the Core
// boundary, covering the measured paths (construct, build, run, getModules,
// getNodeData) plus the two capabilities the spike must demonstrate:
//
//   1. Zero-copy NumPy views over NodeData (the SWIG path copies every column
//      into a Python list).
//   2. C++ exceptions mapped to the typed Python taxonomy (infomap.errors)
//      at the binding layer instead of in Python wrappers.
//
// NOT covered (documented in the go/no-go report): the live tree-walking
// iterator surface (InfoNode, InfomapIterator, ...) which is public API, the
// network() sub-object, writers, and the ~40 remaining Core methods. Those
// are mechanical but constitute the bulk of a real port.

#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/vector.h>

#include "src/Infomap.h"

#include <cstring>
#include <memory>
#include <string>
#include <utility>

namespace nb = nanobind;

namespace {

// Classify engine messages the same way infomap.errors does, so the binding
// raises the public taxonomy directly with no Python-side translation layer.
bool isParseMessage(const std::string& message)
{
  static const char* prefixes[] = {
    "Can't parse", "Cannot parse", "Cannot open input file",
    "Can't open file", "Unrecognized heading", "JSON parse error"
  };
  for (const char* prefix : prefixes) {
    if (message.rfind(prefix, 0) == 0)
      return true;
  }
  return message.find("read permissions") != std::string::npos;
}

// Cached handles to the public taxonomy (infomap.errors). Resolved lazily on
// first throw so importing the spike module alone does not import infomap.
nb::object taxonomyClass(const char* name)
{
  nb::object errors = nb::module_::import_("infomap.errors");
  return errors.attr(name);
}

// One zero-copy NumPy view over a NodeData column. The ndarray owner is a
// capsule holding a shared_ptr to the whole NodeData, so every column keeps
// the snapshot alive independently and Python never copies the buffer.
template <typename T>
nb::object columnView(const std::shared_ptr<infomap::NodeData>& data, const std::vector<T>& column)
{
  auto* holder = new std::shared_ptr<infomap::NodeData>(data);
  nb::capsule owner(holder, [](void* p) noexcept {
    delete static_cast<std::shared_ptr<infomap::NodeData>*>(p);
  });
  const size_t shape[1] = { column.size() };
  return nb::cast(nb::ndarray<nb::numpy, const T, nb::ndim<1>>(
      column.data(), 1, shape, owner));
}

} // namespace

using infomap::InfomapWrapper;

NB_MODULE(_infomap_nb, m)
{
  m.doc() = "Nanobind spike backend for the infomap Core boundary (#743)";

  // std::exception -> the public taxonomy, message preserved. This replaces
  // both SWIG's blanket RuntimeError mapping AND the Python-side
  // _translate_engine_errors re-raise for the bound surface.
  nb::register_exception_translator([](const std::exception_ptr& p, void*) {
    try {
      std::rethrow_exception(p);
    } catch (const std::exception& e) {
      const std::string message = e.what();
      nb::object cls = taxonomyClass(isParseMessage(message) ? "NetworkParseError" : "InfomapError");
      PyErr_SetString(reinterpret_cast<PyObject*>(cls.ptr()), message.c_str());
    }
  });

  nb::class_<InfomapWrapper>(m, "Core")
      .def(nb::init<const std::string&>())
      .def("run", [](InfomapWrapper& im, const std::string& args) { im.run(args); },
           nb::arg("args") = "")
      .def("readInputData", &InfomapWrapper::readInputData,
           nb::arg("filename") = "", nb::arg("accumulate") = true)
      .def("addNode", nb::overload_cast<unsigned int>(&InfomapWrapper::addNode))
      .def("addName", &InfomapWrapper::addName)
      .def("addStateNode", &InfomapWrapper::addStateNode)
      .def("addLink",
           [](InfomapWrapper& im, unsigned int sourceId, unsigned int targetId, double weight) {
             im.addLink(sourceId, targetId, weight);
           },
           nb::arg("sourceId"), nb::arg("targetId"), nb::arg("weight") = 1.0)
      // Bulk link input from a contiguous (n, 2) uint64 array, mirroring the
      // SWIG addLinksFromNumpy fast path.
      .def("addLinksFromArray",
           [](InfomapWrapper& im, nb::ndarray<const uint64_t, nb::ndim<2>, nb::c_contig> links) {
             if (links.shape(1) != 2)
               throw std::runtime_error("links array must have shape (n, 2)");
             const uint64_t* p = links.data();
             for (size_t i = 0; i < links.shape(0); ++i)
               im.addLink(static_cast<unsigned int>(p[2 * i]),
                          static_cast<unsigned int>(p[2 * i + 1]), 1.0);
           })
      .def("getModules", &InfomapWrapper::getModules,
           nb::arg("level") = 1, nb::arg("states") = false)
      .def("getNames", nb::overload_cast<>(&InfomapWrapper::getNames, nb::const_))
      .def("haveModules", [](const InfomapWrapper& im) { return im.haveModules(); })
      .def("haveMemory", [](const InfomapWrapper& im) { return im.haveMemory(); })
      .def("codelength", [](const InfomapWrapper& im) { return im.codelength(); })
      .def("numTopModules", [](const InfomapWrapper& im) { return im.numTopModules(); })
      .def("maxTreeDepth", &InfomapWrapper::maxTreeDepth)
      .def("writeTree",
           [](InfomapWrapper& im, const std::string& filename, bool states) {
             im.writeTree(filename, states);
           },
           nb::arg("filename"), nb::arg("states") = false)
      // The spike's centerpiece: one C++ traversal, then zero-copy NumPy
      // views. The dict holds one ndarray per column; each view shares the
      // same shared_ptr<NodeData> owner.
      .def("get_node_data",
           [](InfomapWrapper& im, int level, bool states) {
             auto data = std::make_shared<infomap::NodeData>(im.getNodeData(level, states));
             nb::dict out;
             out["node_id"] = columnView(data, data->node_id);
             out["state_id"] = columnView(data, data->state_id);
             out["module_id"] = columnView(data, data->module_id);
             out["flow"] = columnView(data, data->flow);
             out["depth"] = columnView(data, data->depth);
             out["layer_id"] = columnView(data, data->layer_id);
             out["child_index"] = columnView(data, data->child_index);
             out["modular_centrality"] = columnView(data, data->modular_centrality);
             out["path_flat"] = columnView(data, data->path_flat);
             out["path_len"] = columnView(data, data->path_len);
             return out;
           },
           nb::arg("level") = 1, nb::arg("states") = false);
}
