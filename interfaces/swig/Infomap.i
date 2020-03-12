%module infomap

%include "exceptions.i"

%{
/* Includes the header in the wrapper code */
#include "src/Infomap.h"
// SWIG strips namespaces, so include infomap in global namespace in wrapper code
using namespace infomap;
%}

// Makes it possible to use for example numpy.int64 as link weights
%begin %{
#define SWIG_PYTHON_CAST_MODE
%}

%include "std_string.i"
%include "Config.i"
%include "InfomapCore.i"

%include "std_map.i"
%include "std_vector.i"

namespace std {
    %template(vector_uint) std::vector<unsigned int>;
    %template(map_uint_uint) std::map<unsigned int, unsigned int>;
    %template(map_uint_vector_uint) std::map<unsigned int, std::vector<unsigned int>>;
    %template(map_uint_string) std::map<unsigned int, std::string>;
}

#ifdef SWIGPYTHON
%extend infomap::InfomapWrapper
{
	// overrides to convert swig map proxy object to pure dict
	%insert("python") %{
        def getModules(self, level=1, states=False):
            return dict(_infomap.InfomapWrapper_getModules(self, level, states))

        def getNames(self):
            return dict(_infomap.InfomapWrapper_getNames(self))
    %}
}
#endif

/* Parse the header file to generate wrappers */
%include "src/Infomap.h"

#ifdef SWIGPYTHON
%pythoncode "interfaces/python/infomap.py"
#endif
