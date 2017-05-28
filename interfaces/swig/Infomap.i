%module infomap

%{
/* Includes the header in the wrapper code */
#include "src/Infomap.h"
// SWIG strips namespaces, so include infomap in global namespace in wrapper code
using namespace infomap;
%}

%include "std_string.i"
%include "InfomapTypes.i"
%include "Config.i"
%include "InfomapIterator.i"

/* Parse the header file to generate wrappers */
%include "src/Infomap.h"
