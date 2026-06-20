%module infomap

%{
/* Includes the header in the wrapper code */
#include "src/core/InfomapBase.h"
%}

%include "InfomapConfig.i"
%include "Config.i"
%include "InfoNode.i"
%include "InfomapIterator.i"
%include "Network.i"

%include "std_map.i"

namespace std {
    %template(map_uint_uint) std::map<unsigned int, unsigned int>;
    %template(vector_double) std::vector<double>;
}
namespace infomap {
    %template(InfomapConfigInfomapBase) InfomapConfig<InfomapBase>;
}

/* Cooperative interruption (issue #412) is a native C++/embedder seam only;
   keep it out of the generated language bindings. A function-pointer callback
   does not wrap meaningfully through SWIG, and host bindings drive cancellation
   through their own bridge (see the R/Python notes on the issue). */
%ignore infomap::InterruptCallback;
%ignore infomap::InterruptionError;
%ignore infomap::InfomapBase::setInterruptHandler;
%ignore infomap::InfomapBase::clearInterruptHandler;
%ignore infomap::InfomapBase::hasInterruptHandler;
%ignore infomap::InfomapBase::requestInterrupt;
%ignore infomap::InfomapBase::interruptRequested;

/* Parse the header file to generate wrappers */
%include "src/core/InfomapBase.h"
