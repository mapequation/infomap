%module infomap

%{
/* Includes the header in the wrapper code */
#include "src/io/Config.h"
%}

%include "std_string.i"

%ignore infomap::flowModelNames;
%ignore infomap::parseFlowModel;

/* Parse the header file to generate wrappers */
%include "src/io/Config.h"
