%module infomap

%{
/* Includes the header in the wrapper code */
#include "src/core/PartitionQueue.h"
%}

/* Parse the header file to generate wrappers */
%include "src/core/PartitionQueue.h"