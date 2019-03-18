%module infomap

%{
/* Includes the header in the wrapper code */
#include "src/core/MapEquation.h"
%}

%include "NodeBase.i"

/* Parse the header file to generate wrappers */
%include "src/core/MapEquation.h"
