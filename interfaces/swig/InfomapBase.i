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

%ignore infomap::InfomapBase::setInterruptHandler;
%ignore infomap::InfomapBase::clearInterruptHandler;
%ignore infomap::InfomapBase::setInterruptPollingPeriod;
%ignore infomap::InfomapBase::hasInterruptHandler;
%ignore infomap::InterruptionError;

/* Parse the header file to generate wrappers */
%include "src/core/InfomapBase.h"
