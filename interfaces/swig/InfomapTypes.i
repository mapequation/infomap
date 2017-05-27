%module infomap

%{
/* Includes the header in the wrapper code */
#include "src/core/InfomapTypes.h"
%}

%include "Config.i"
%include "MapEquation.i"
%include "MemMapEquation.i"
%include "InfomapOptimizer.i"

// Instantiate templates used
namespace infomap {
%template(MapEquationInfomapOptimizer) InfomapOptimizer<MapEquation>;
%template(MemMapEquationInfomapOptimizer) InfomapOptimizer<MemMapEquation>;
}

/* Parse the header file to generate wrappers */
%include "src/core/InfomapTypes.h"