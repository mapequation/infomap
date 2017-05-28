%module infomap

%{
/* Includes the header in the wrapper code */
#include "src/core/InfoNode.h"
%}

%include "FlowData.i"
%include "std_string.i"

/* Parse the header file to generate wrappers */
%include "src/core/InfoNode.h"
