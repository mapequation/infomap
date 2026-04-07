%module infomap

%{
/* Includes the header in the wrapper code */
#include "src/io/Config.h"
%}

%include "std_string.i"

%ignore infomap::Config::startDate;
%ignore infomap::Config::additionalInput;
%ignore infomap::Config::parsedOptions;

/* Parse the header file to generate wrappers */
%include "src/io/Config.h"
