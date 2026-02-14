%module infomap

%{
/* Includes the header in the wrapper code */
#include "src/io/Network.h"
%}

%include "std_string.i"
%include "StateNetwork.i"

/* Parse the header file to generate wrappers */
%include "src/io/Network.h"

#ifdef SWIGPYTHON
%extend infomap::Network
{
	%insert("python") %{

		@property
		def is_multilayer_network(self):
			"""True if the network have multiple layers.

			Returns
			-------
			bool
				Is multilayer network
			"""
			return self.isMultilayerNetwork()

		def add_multilayer_node(self, state_id, layer_id, node_id, weight=1.0):
			"""Add a multilayer node with predefined state id.

			Parameters
			----------
			state_id : int
				The unique id for the state node
			layer_id : int
				The layer of the state node
			node_id : int
				The physical node id of the state node
			weight : float
				The optional weight of the state node, default 1.0
			
			Returns
			-------
			int
				The state id
			"""
			return self.addMultilayerNode(state_id, layer_id, node_id, weight)

		def add_multilayer_state_link(self, source_state_id, source_layer_id, source_node_id, target_state_id, target_layer_id, target_node_id, weight=1.0):
			"""Add a multilayer link with predefined state ids.

			Parameters
			----------
			source_state_id : int
				The unique id for the source state node
			source_layer_id : int
				The layer of the source state node
			source_node_id : int
				The physical node id of the source state node
			target_state_id : int
				The unique id for the target state node
			target_layer_id : int
				The layer of the target state node
			target_node_id : int
				The physical node id of the target state node
			weight : float
				The optional weight of the link, default 1.0
			
			Returns
			-------
			int
				The state id
			"""
			return self.addMultilayerLink(source_state_id, source_layer_id, source_node_id, target_state_id, target_layer_id, target_node_id, weight)

	%}
}
#endif