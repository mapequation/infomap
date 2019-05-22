%module infomap

%{
/* Includes the header in the wrapper code */
#include "src/core/Node.h"
%}

%include "NodeBase.i"
%include "FlowData.i"

/* Parse the header file to generate wrappers */
%include "src/core/Node.h"

namespace infomap {
    %template(NodeFlowData) Node<FlowData>;
    %template(NodeFlowDataInt) Node<FlowDataInt>;
}
