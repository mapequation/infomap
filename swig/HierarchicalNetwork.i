%module infomap

%{
/* Includes the header in the wrapper code */
#include "src/io/HierarchicalNetwork.h"
%}

%include "std_string.i"

/* Parse the header file to generate wrappers */
#include "src/io/HierarchicalNetwork.h"