%module infomap

%{
/* Includes the header in the wrapper code */
#include "src/core/FlowData.h"
%}

/* Parse the header file to generate wrappers */
%include "src/core/FlowData.h"