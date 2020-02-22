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


#ifdef SWIGPYTHON
%extend infomap::InfoNode
{
	%insert("python") %{

		@property
		def node_id(self):
			"""Get the physical node id."""
			return self.physicalId

		@property
		def state_id(self):
			"""Get the state id of the node."""
			return self.stateId
		
		@property
		def layer_id(self):
			"""Get the layer id of a multilayer node."""
			return self.layerId
		
		@property
		def child_degree(self):
			"""The number of children"""
			return self.childDegree()
		
		@property
		def is_leaf(self):
			"""True if the node has no children"""
			return self.isLeaf()
		
		@property
		def is_leaf_module(self):
			"""True if the node has children but no grandchildren"""
			return self.isLeafModule()
		
		@property
		def is_root(self):
			"""True if the node has no parent"""
			return self.isRoot()
		
		@property
		def meta_data(self):
			"""Meta data (on first dimension if more)"""
			return self.getMetaData()
		
		def get_meta_data(self, dimension = 0):
			"""Get meta data on a specific dimension"""
			return self.getMetaData(dimension)
	%}
}
#endif