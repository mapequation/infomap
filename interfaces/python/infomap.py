from collections import namedtuple
from contextlib import contextmanager

MultilayerNode = namedtuple("MultilayerNode", "layer_id, node_id")


class Infomap(InfomapWrapper):
    def __init__(self, parameters=""):
        super().__init__(parameters)

    def read_file(self, filename, accumulate=True):
        """Read network data from file.

        Parameters
        ----------
        filename : str
        accumulate : bool, optional
            If the network data should be accumulated to already added
            nodes and links (default True).
        """
        super().readInputData(filename, accumulate)

    def add_node(self, node_id, name="", teleportation_weight=None):
        """Add a node.

        Parameters
        ----------
        node_id : int
        name : str, optional
        teleportation_weight : float, optional
            Used for teleporting between layers in multilayer networks.
        """
        if (len(name) and teleportation_weight):
            super().addNode(node_id, name, teleportation_weight)
        elif (len(name) and not teleportation_weight):
            super().addNode(node_id, name)
        elif (teleportation_weight and not len(name)):
            super().addNode(node_id, teleportation_weight)
        else:
            super().addNode(node_id)

    def add_nodes(self, nodes):
        """Add several nodes.

        See add_node

        Parameters
        ----------
        nodes : iterable
            Iterable of tuples on the form (node_id, [name], [teleportation_weight])
        """
        for node in nodes:
            self.add_node(*node)

    def set_name(self, node_id, name):
        """Set the name of a node.

        Creates nodes if a node with the supplied node id does not exist.
        This is useful to create empty physical node in a state network.

        Parameters
        ----------
        node_id : int
        name : str
        """
        super().addName(node_id, name)

    def set_names(self, names):
        """Set names to several nodes at once.

        See add_node.

        Parameters
        ----------
        names : dict
            dict with node ids as keys and names as values
        """
        for node_id, name in names:
            self.set_name(node_id, name)

    def set_meta_data(self, node_id, meta_category):
        """Set meta data to a node.

        Parameters
        ----------
        node_id : int
        meta_category : int
        """
        self.network.addMetaData(node_id, meta_category)

    def add_state_node(self, state_id, node_id):
        """Add a state node.

        Parameters
        ----------
        state_id : int
        node_id : int
            Id of the physical node the state node should be added to.
        """
        super().addStateNode(state_id, node_id)

    def add_state_nodes(self, state_nodes):
        """Add several state nodes.

        See add_state_node

        Parameters
        ----------
        state_nodes : iterable
            Iterable of tuples of the form (state_id, node_id)
        """
        for node in state_nodes:
            self.add_state_node(*node)

    def add_link(self, source_id, target_id, weight=1.0):
        """Add a link.

        Parameters
        ----------
        source_id : int
        target_id : int
        weight : float, optional
        """
        super().addLink(source_id, target_id, weight)

    def add_links(self, links):
        """Add several links.

        See add_link

        Parameters
        ----------
        links : iterable
            Iterable of tuples of int of the form (source_id, target_id, [weight])
        """
        for link in links:
            self.add_link(*link)

    def remove_link(self, source_id, target_id):
        """Remove a link.

        Parameters
        ----------
        source_id : int
        target_id : int
        """
        self.network.removeLink(source_id, target_id)

    def remove_links(self, links):
        """Remove several links.

        See remove_link

        Parameters
        ----------
        links : iterable
            Iterable of tuples of the form (source_id, target_id)
        """
        for link in links:
            self.remove_link(*link)

    def add_multilayer_link(
            self,
            source_multilayer_node,
            target_multilayer_node,
            weight=1.0):
        """Add a multilayer link.

        Adds a link between layers in a multilayer network.

        Parameters
        ----------
        source_multilayer_node : tuple of int, or MultilayerNode
            If passed a tuple, it should be of the format (layer_id, node_id).
        target_multilayer_node : tuple of int, or MultilayerNode
            If passed a tuple, it should be of the format (layer_id, node_id).
        weight : float, optional
        """
        source_layer_id, source_node_id = source_multilayer_node
        target_layer_id, target_node_id = target_multilayer_node
        super().addMultilayerLink(source_layer_id,
                                  source_node_id,
                                  target_layer_id,
                                  target_node_id,
                                  weight)

    def add_multilayer_links(self, links):
        """Add several multilayer links.

        See add_multilayer_link

        Parameters
        ----------
        links : iterable
            Iterable of tuples of the form (source_node, target_node, [weight])
        """
        for link in links:
            self.add_multilayer_link(*link)

    @property
    def bipartite_start_id(self):
        """Get the bipartite start id."""
        return self.network.bipartiteStartId

    @bipartite_start_id.setter
    def bipartite_start_id(self, start_id):
        """Set the bipartite start id.

        Parameters
        ----------
        start_id : int
            The node id where the second category starts.
        """
        super().setBipartiteStartId(start_id)

    @property
    def initial_partition(self):
        """Get the initial partition."""
        return super().getInitialPartition()

    @initial_partition.setter
    def initial_partition(self, module_ids):
        """Set the initial partition.

        This is a initial configuration of nodes into modules where Infomap
        will start the optimizer.

        Parameters
        ----------
        module_ids : dict
            dict of module ids to node ids describing the module assignments
            of each node.
        """
        if module_ids is None:
            module_ids = {}
        super().setInitialPartition(module_ids)

    @contextmanager
    def _initial_partition(self, partition):
        old_partition = self.initial_partition
        try:
            self.initial_partition = partition
            yield
        finally:
            self.initial_partition = old_partition

    def run(self, args="", initial_partition=None):
        """Run Infomap.

        Parameters
        ----------
        args : string
            Space delimited parameter list (see Infomap documentation).
        initial_partition : dict, optional
            Initial partition to start optimizer from (see initial_partition).
        """
        if initial_partition:
            with self._initial_partition(initial_partition):
                super().run(args)
        else:
            super().run(args)

    def get_modules(self, depth_level=1, states=False):
        return super().getModules(depth_level, states)

    def get_multilevel_modules(self, states=False):
        return super().getMultilevelModules(states)

    @property
    def modules(self):
        return self.get_modules().items()

    def tree(self):
        return super().iterTree()

    @property
    def leaf_modules(self):
        return super().iterLeafModules()

    @property
    def leaf_nodes(self):
        return super().iterLeafNodes()

    @property
    def nodes(self):
        """ Alias of leaf_nodes """
        return self.leaf_nodes

    @property
    def physical_tree(self):
        return super().iterTreePhysical()

    @property
    def physical_leaf_modules(self):
        return super().iterLeafModulesPhysical()

    @property
    def physical_leaf_nodes(self):
        return super().iterLeafNodesPhysical()

    @property
    def num_top_modules(self):
        return super().numTopModules()

    @property
    def max_depth(self):
        return super().maxTreeDepth()

    @property
    def codelength(self):
        """Total (hierarchical) codelength"""
        return super().codelength()

    @property
    def index_codelength(self):
        """Two-level index codelength"""
        return super().getIndexCodelength()

    @property
    def module_codelength(self):
        """Two-level module codelength"""
        return super().getModuleCodelength()

    @property
    def meta_codelength(self):
        """Meta codelength (meta entropy times meta data rate)"""
        return super().getMetaCodelength()

    @property
    def meta_entropy(self):
        """Meta entropy (unweighted by meta data rate)"""
        return super().getMetaCodelength(True)

    @property
    def network(self):
        return super().network()

    def write_clu(self, filename="", states=False, depth_level=1):
        return self.writeClu(filename, states, depth_level)

    def write_tree(self, filename="", states=False):
        return self.writeTree(filename, states)

    def write_flow_tree(self, filename="", states=False):
        return self.writeFlowTree(filename, states)

    def set_no_infomap(self, no_infomap=True):
        """Set wether the optimizer should run or not.

        Parameters
        ----------
        no_infomap : boolean, optional
        """
        return self.setNoInfomap(no_infomap)
