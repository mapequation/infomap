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

namespace infomap {
    %template(InfomapConfigInfomapBase) InfomapConfig<InfomapBase>;
}

/* Parse the header file to generate wrappers */
%include "src/core/InfomapBase.h"
