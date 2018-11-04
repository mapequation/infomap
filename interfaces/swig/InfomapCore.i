%module infomap

%{
/* Includes the header in the wrapper code */
#include "src/core/InfomapCore.h"
// SWIG strips namespaces, so include infomap in global namespace in wrapper code
using namespace infomap;
%}

%include "std_string.i"
%include "InfomapBase.i"
%include "Config.i"
%include "MapEquation.i"
%include "MemMapEquation.i"
%include "MetaMapEquation.i"

/* Parse the header file to generate wrappers */
%include "src/core/InfomapCore.h"
