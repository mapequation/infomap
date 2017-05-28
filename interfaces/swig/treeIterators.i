%module infomap

%{
/* Includes the header in the wrapper code */
#include "src/core/treeIterators.h"
%}

/* Parse the header file to generate wrappers */
%include "src/core/treeIterators.h"
