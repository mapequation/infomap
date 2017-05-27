%module infomap

%{
/* Includes the header in the wrapper code */
#include "src/core/InfomapBase.h"
%}

%include "InfomapConfig.i"
%include "Config.i"

namespace infomap {
%template(InfomapBaseInfomapConfig) InfomapConfig<InfomapBase>;
}

/* Parse the header file to generate wrappers */
%include "src/core/InfomapBase.h"