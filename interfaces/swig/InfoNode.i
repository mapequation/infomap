%module infomap

%{
/* Includes the header in the wrapper code */
#include "src/core/InfoNode.h"
%}

%include "iterators.i"
%include "treeIterators.i"
%include "infomapIterators.i"
%include "FlowData.i"
%include "InfoEdge.i"

/* Parse the header file to generate wrappers */
%include "src/core/InfoNode.h"