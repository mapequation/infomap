%module infomap

%{
/* Includes the header in the wrapper code */
#include "src/core/StateNetwork.h"
%}

%include "std_string.i"

%include "std_vector.i"

namespace std {
    %template(vector_uint) std::vector<unsigned int>;
    %template(vector_double) std::vector<double>;
}

%ignore infomap::StateNetwork::LinkInput;
%ignore infomap::StateNetwork::addLinksBulk;

/* Parse the header file to generate wrappers */
%include "src/core/StateNetwork.h"
