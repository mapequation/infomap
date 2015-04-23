%module infomap

%{
/* Includes the header in the wrapper code */
#include "src/infomap/Network.h"
%}

%include "std_string.i"
%include "std_vector.i"

// Instantiate templates used by Network
namespace std {
   %template(StringVector) vector<string>;
}

/* Parse the header file to generate wrappers */
#include "src/infomap/Network.h"