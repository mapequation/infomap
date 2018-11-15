%module infomap

%include "exceptions.i"

%{
/* Includes the header in the wrapper code */
#include "src/Infomap.h"
// SWIG strips namespaces, so include infomap in global namespace in wrapper code
using namespace infomap;
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
}

#ifdef SWIGPYTHON
%extend infomap::Infomap
{
	// overwrite getModules to convert swig map proxy object to pure dict
	%insert("python") %{
        def getModules(self, level=1, states=False):
            return dict(_infomap.Infomap_getModules(self, level, states))
    %}
}
#endif

/* Parse the header file to generate wrappers */
%include "src/Infomap.h"
