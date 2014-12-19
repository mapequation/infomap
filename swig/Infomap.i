%module infomap

%{
/* Includes the header in the wrapper code */
#include "src/Infomap.h"
#include "src/utils/Date.h"
#include "src/io/Config.h"
#include "src/infomap/Network.h"
#include "src/io/HierarchicalNetwork.h"
%}

%include "std_string.i"

%include "Date.i"
%include "Config.i"
%include "Network.i"
%include "HierarchicalNetwork.i"

/* Parse the header file to generate wrappers */
%include "src/Infomap.h"
%include "src/utils/Date.h"
%include "src/io/Config.h"
%include "src/infomap/Network.h"
%include "src/io/HierarchicalNetwork.h"