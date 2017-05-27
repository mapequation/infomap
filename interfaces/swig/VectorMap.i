%module infomap

%{
/* Includes the header in the wrapper code */
#include "src/utils/VectorMap.h"
%}

/* Parse the header file to generate wrappers */
%include "src/utils/VectorMap.h"