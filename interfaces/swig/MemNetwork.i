%module infomap

%{
/* Includes the header in the wrapper code */
#include "src/infomap/MemNetwork.h"
%}

%include "std_map.i"

// Instantiate templates used
namespace std {
   %template(StateNodeMap) map<StateNode, double>;
}

/* Parse the header file to generate wrappers */
#include "src/infomap/MemNetwork.h"