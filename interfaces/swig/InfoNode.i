%module infomap

%{
/* Includes the header in the wrapper code */
#include "src/core/InfoNode.h"
%}

%include "FlowData.i"
%include "std_string.i"
%include "std_vector.i"

namespace std {
    %template(vector_uint) std::vector<unsigned int>;
}

/* Parse the header file to generate wrappers */
%include "src/core/InfoNode.h"
