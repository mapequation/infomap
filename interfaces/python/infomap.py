from collections import namedtuple
from contextlib import contextmanager

MultilayerNode = namedtuple("MultilayerNode", "layer_id, node_id")


class Infomap(InfomapWrapper):
    """Infomap

    This class is a thin wrapper around Infomap C++ compiled with SWIG.

    Examples
    --------
    Create an instance and add nodes and links:

    >>> import infomap
    >>> im = infomap.Infomap()
    >>> im.add_node(1)
    >>> im.add_node(2)
    >>> im.add_link(1, 2)
    >>> im.run()
    >>> im.codelength
    1.0


    Create an instance and read a network file:

    >>> import infomap
    >>> im = infomap.Infomap()
    >>> im.read_file("ninetriangles.net")
    >>> im.run("-N5")
    >>> im.codelength
    3.4841898804052187


    For more examples, see the examples directory.
    """

    def __init__(self, args=None):
        """Create a new Infomap instance.

        Parameters
        ----------
        args : str, optional
            Space delimited parameter list (see Infomap documentation).
        """
        if args is None:
            args = ""
        super().__init__(args)

    def read_file(self, filename, accumulate=True):
        """Read network data from file.

        Parameters
        ----------
        filename : str
        accumulate : bool, optional
            If the network data should be accumulated to already added
            nodes and links (default True).
        """
        return super().readInputData(filename, accumulate)

    def add_node(self, node_id, name=None, teleportation_weight=None):
        """Add a node.

        Notes
        -----
        Creates a state node for internal use.
        If you want to create empty named nodes, use `set_name` instead.

        See Also
        --------
        set_name

        Parameters
        ----------
        node_id : int
        name : str, optional
        teleportation_weight : float, optional
            Used for teleporting between layers in multilayer networks.
        """
        if name is None:
            name = ""

        if (len(name) and teleportation_weight is not None):
            return super().addNode(node_id, name, teleportation_weight)
        elif (len(name) and teleportation_weight is None):
            return super().addNode(node_id, name)
        elif (not len(name) and teleportation_weight is not None):
            return super().addNode(node_id, teleportation_weight)

        return super().addNode(node_id)

    def add_nodes(self, nodes):
        """Add several nodes.

        See Also
        --------
        add_node

        Parameters
        ----------
        nodes : iterable of tuples or iterable of int
            Iterable of tuples on the form (node_id, [name], [teleportation_weight])
        """
        for node in nodes:
            if isinstance(node, int):
                self.add_node(node)
            else:
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
        if name is None:
            name = ""
        return super().addName(node_id, name)

    def set_names(self, names):
        """Set names to several nodes at once.

        See Also
        --------
        set_name

        Parameters
        ----------
        names : dict of str
            Dict with node ids as keys and names as values
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
        return self.network.addMetaData(node_id, meta_category)

    def add_state_node(self, state_id, node_id):
        """Add a state node.

        Notes
        -----
        If a physical node with id node_id does not exist, it will be created.
        If you want to name the physical node, use `set_name`.

        Parameters
        ----------
        state_id : int
        node_id : int
            Id of the physical node the state node should be added to.
        """
        return super().addStateNode(state_id, node_id)

    def add_state_nodes(self, state_nodes):
        """Add several state nodes.

        See Also
        --------
        add_state_node

        Parameters
        ----------
        state_nodes : iterable of tuples
            Iterable of tuples of the form (state_id, node_id)
        """
        for node in state_nodes:
            self.add_state_node(*node)

    def add_link(self, source_id, target_id, weight=1.0):
        """Add a link.

        Notes
        -----
        If the source or target nodes does not exist, they will be created.

        Parameters
        ----------
        source_id : int
        target_id : int
        weight : float, optional
        """
        return super().addLink(source_id, target_id, weight)

    def add_links(self, links):
        """Add several links.

        See Also
        --------
        add_link

        Parameters
        ----------
        links : iterable of tuples
            Iterable of tuples of int of the form (source_id, target_id, [weight])
        """
        for link in links:
            self.add_link(*link)

    def remove_link(self, source_id, target_id):
        """Remove a link.

        Notes
        -----
        Removing links will not remove nodes if they become disconnected.

        Parameters
        ----------
        source_id : int
        target_id : int
        """
        return self.network.removeLink(source_id, target_id)

    def remove_links(self, links):
        """Remove several links.

        See Also
        --------
        remove_link

        Parameters
        ----------
        links : iterable of tuples
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

        Examples
        --------
        Usage with tuples:

        >>> import infomap
        >>> im = infomap.Infomap()
        >>> source_multilayer_node = (0, 1) # layer_id, node_id
        >>> target_multilayer_node = (1, 2) # layer_id, node_id
        >>> im.add_multilayer_link(source_multilayer_node, target_multilayer_node)
        None


        Usage with MultilayerNode

        >>> from infomap import Infomap, MultilayerNode
        >>> im = Infomap()
        >>> source_multilayer_node = MultilayerNode(layer_id=0, node_id=1)
        >>> target_multilayer_node = MultilayerNode(layer_id=1, node_id=2)
        >>> im.add_multilayer_link(source_multilayer_node, target_multilayer_node)
        None

        """
        source_layer_id, source_node_id = source_multilayer_node
        target_layer_id, target_node_id = target_multilayer_node
        return super().addMultilayerLink(source_layer_id,
                                         source_node_id,
                                         target_layer_id,
                                         target_node_id,
                                         weight)

    def add_multilayer_links(self, links):
        """Add several multilayer links.

        See Also
        --------
        add_multilayer_link

        Parameters
        ----------
        links : iterable of tuples
            Iterable of tuples of the form (source_node, target_node, [weight])
        """
        for link in links:
            self.add_multilayer_link(*link)

    @property
    def bipartite_start_id(self):
        """Get or set the bipartite start id.

        Parameters
        ----------
        start_id : int
            The node id where the second node type starts.

        Returns
        -------
        int
            The node id where the second node type starts.
        """
        return self.network.bipartiteStartId

    @bipartite_start_id.setter
    def bipartite_start_id(self, start_id):
        return super().setBipartiteStartId(start_id)

    @property
    def initial_partition(self):
        """Get or set the initial partition.

        This is a initial configuration of nodes into modules where Infomap
        will start the optimizer.

        Notes
        -----
        The initial partition is saved between runs.
        If you want to use an initial partition for one run only,
        use `run(initial_partition=partition)`.

        Parameters
        ----------
        module_ids : dict of int, or None
            Dict with node ids as keys and module ids as values.

        Returns
        -------
        dict of int
            Dict with node ids as keys and module ids as values.
        """
        return super().getInitialPartition()

    @initial_partition.setter
    def initial_partition(self, module_ids):
        if module_ids is None:
            module_ids = {}
        return super().setInitialPartition(module_ids)

    @contextmanager
    def _initial_partition(self, partition):
        # Internal use only
        # Store the possiply set initial partition
        # and use the new initial partition for one run only.
        old_partition = self.initial_partition
        try:
            self.initial_partition = partition
            yield
        finally:
            self.initial_partition = old_partition

    def run(self, args=None, initial_partition=None):
        """Run Infomap.

        Parameters
        ----------
        args : str, optional
            Space delimited parameter list (see Infomap documentation).
        initial_partition : dict, optional
            Initial partition to start optimizer from (see initial_partition).

        See Also
        --------
        initial_partition
        """
        if args is None:
            args = ""

        if initial_partition:
            with self._initial_partition(initial_partition):
                return super().run(args)

        return super().run(args)

    def get_modules(self, depth_level=1, states=False):
        """Get the modules for a given depth in the hierarchical tree.

        Parameters
        ----------
        depth_level : int, optional
            The level in the hierarchical tree (default 1).
        states : bool, optional
            Include state nodes (default False).

        Returns
        -------
        dict of int
            Dict with node ids as keys and module ids as values.
        """
        return super().getModules(depth_level, states)

    def get_multilevel_modules(self, states=False):
        """Get all the modules.

        Parameters
        ----------
        states : bool, optional
            Include state nodes (default False).

        Returns
        -------
        dict of list of int
            Dict with node ids as keys and list of module ids as values.
        """
        return super().getMultilevelModules(states)

    @property
    def modules(self):
        """A view of the (top-level) modules

        See Also
        --------
        get_modules

        Yields
        -------
        dict_items
            An iterator of (node_id, module_id) pairs.
        """
        return self.get_modules().items()

    def tree(self):
        """A view of the tree

        Yields
        -------
        InfomapIterator
            An iterator over each node in the tree, depth first from the root
        """
        return super().iterTree()

    @property
    def leaf_modules(self):
        """A view of the leaf modules, i.e. the bottom modules containing leaf nodes.

        Yields
        -------
        InfomapLeafIterator
            An iterator over each leaf module in the tree, depth first from the root
        """
        return super().iterLeafModules()

    @property
    def leaf_nodes(self):
        """A view of the leaf nodes (network nodes)

        Yields
        -------
        InfomapLeafIterator
            An iterator over each leaf node in the tree, depth first from the root
        """
        return super().iterLeafNodes()

    @property
    def nodes(self):
        """Alias of leaf_nodes.

        See Also
        --------
        leaf_nodes
        """
        return self.leaf_nodes

    @property
    def physical_tree(self):
        """A view of the tree where that state nodes of the same node_id
        are merged to one physical node

        Notes
        -----
        In a physical view, state nodes of the same physical node are merged to
        a (partial) physical node if they belong to the same module. As state
        nodes of the same physical node can be part of different modules, the
        same physical node_id may exist on multiple tips of the tree. In that
        case, the flow value of each leaf node in the tree is the sum of the flow
        values of the state nodes with the same physical node_id.

        Yields
        -------
        InfomapIteratorPhysical
            An iterator over each physical node in the tree, depth first from the root
        """
        return super().iterTreePhysical()

    @property
    def physical_leaf_nodes(self):
        """A view of the physical leaf nodes (network nodes)

        Notes
        -----
        In a physical view, state nodes of the same physical node are merged to
        a (partial) physical node if they belong to the same module. As state
        nodes of the same physical node can be part of different modules, the
        same physical node_id may exist on multiple tips of the tree. In that
        case, the flow value of each leaf node in the tree is the sum of the flow
        values of the state nodes with the same physical node_id.

        Yields
        -------
        InfomapLeafIteratorPhysical
            An iterator over each physical leaf node in the tree, depth first from the root
        """
        return super().iterLeafNodesPhysical()

    @property
    def num_top_modules(self):
        """Get the number of top modules in the tree

        Returns
        -------
        int
            The number of top modules
        """
        return super().numTopModules()

    @property
    def max_depth(self):
        """Get the max depth of the hierarchical tree.

        Returns
        -------
        int
            The max depth
        """
        return super().maxTreeDepth()

    @property
    def codelength(self):
        """Get the total (hierarchical) codelength.

        See Also
        --------
        index_codelength
        module_codelength

        Returns
        -------
        float
            The codelength
        """
        return super().codelength()

    @property
    def index_codelength(self):
        """Get the two-level index codelength.

        See Also
        --------
        codelength
        module_codelength

        Returns
        -------
        float
            The two-level index codelength
        """
        return super().getIndexCodelength()

    @property
    def module_codelength(self):
        """Get the two-level module codelength.

        See Also
        --------
        codelength
        index_codelength

        Returns
        -------
        float
            The two-level module codelength
        """
        return super().getModuleCodelength()

    @property
    def meta_codelength(self):
        """Get the meta codelength.

        This is the meta entropy times the meta data rate.

        See Also
        --------
        meta_entropy

        Returns
        -------
        float
            The meta codelength
        """
        return super().getMetaCodelength()

    @property
    def meta_entropy(self):
        """Get the meta entropy (unweighted by meta data rate).

        See Also
        --------
        meta_codelength

        Returns
        -------
        float
            The meta entropy
        """
        return super().getMetaCodelength(True)

    @property
    def network(self):
        """Get the internal network."""
        return super().network()

    def write_clu(self, filename="", states=False, depth_level=1):
        """Write result to a clu file.

        See Also
        --------
        write_tree
        write_flow_tree

        Parameters
        ----------
        filename : str
        states : bool, optional
            If the state nodes should be included (default False).
        depth_level : int, optional
            The depth in the hierarchical tree to write.
        """
        return self.writeClu(filename, states, depth_level)

    def write_tree(self, filename="", states=False):
        """Write result to a tree file.

        See Also
        --------
        write_clu
        write_flow_tree

        Parameters
        ----------
        filename : str
        states : bool, optional
            If the state nodes should be included (default False).
        """
        return self.writeTree(filename, states)

    def write_flow_tree(self, filename="", states=False):
        """Write result to a ftree file.

        See Also
        --------
        write_clu
        write_tree

        Parameters
        ----------
        filename : str
        states : bool, optional
            If the state nodes should be included (default False).
        """
        return self.writeFlowTree(filename, states)

    _no_infomap = False

    @property
    def no_infomap(self):
        """Set wether the optimizer should run or not.

        Parameters
        ----------
        no_infomap : bool
        """
        return self._no_infomap

    @no_infomap.setter
    def no_infomap(self, no_infomap):
        self._no_infomap = no_infomap
        super().setNoInfomap(no_infomap)
