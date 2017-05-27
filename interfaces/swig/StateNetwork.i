%module infomap

%{
/* Includes the header in the wrapper code */
#include "src/core/StateNetwork.h"
%}

%include "std_string.i"

/* Parse the header file to generate wrappers */
%include "src/core/StateNetwork.h"