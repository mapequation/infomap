import sys
from collections import namedtuple
from contextlib import contextmanager

from ._bindings import *  # noqa: F401,F403
from ._networkx import add_networkx_graph as _add_networkx_graph
from ._options import (
    InfomapOptions,
    _DEFAULT_CORE_LOOP_CODELENGTH_THRESHOLD,
    _DEFAULT_META_DATA_RATE,
    _DEFAULT_MULTILAYER_RELAX_RATE,
    _DEFAULT_SEED,
    _DEFAULT_TELEPORTATION_PROB,
    _DEFAULT_TUNE_ITER_RELATIVE_THRESHOLD,
    _DEFAULT_VERBOSITY_LEVEL,
    _construct_args,
)
from ._results import _InfomapResultsMixin
from ._results import entropy as entropy  # noqa: F401
from ._results import perplexity as perplexity  # noqa: F401
from ._results import plogp as plogp  # noqa: F401
from ._writers import _InfomapWritersMixin


def _package_construct_args():
    package_module = sys.modules.get(__package__)
    if package_module is None:
        return _construct_args
    return getattr(package_module, "_construct_args", _construct_args)


MultilayerNode = namedtuple("MultilayerNode", "layer_id, node_id")


class Infomap(_InfomapResultsMixin, _InfomapWritersMixin, InfomapWrapper):  # noqa: F405
    """Infomap

    This class is a thin wrapper around Infomap C++ compiled with SWIG.

    Examples
    --------
    Create an instance and add nodes and links:

    >>> from infomap import Infomap
    >>> im = Infomap(silent=True)
    >>> im.add_node(1)
    >>> im.add_node(2)
    >>> im.add_link(1, 2)
    >>> im.run()
    >>> im.codelength
    1.0


    Create an instance and read a network file:

    >>> from infomap import Infomap
    >>> im = Infomap(silent=True, num_trials=10)
    >>> im.read_file("ninetriangles.net")
    >>> im.run()
    >>> tol = 1e-4
    >>> abs(im.codelength - 3.4622731375264144) < tol
    True


    For more examples, see the examples directory.
    """

    def __init__(
        self,
        args=None,
        # input
        cluster_data=None,
        no_infomap=False,
        skip_adjust_bipartite_flow=False,
        bipartite_teleportation=False,
        weight_threshold=None,
        include_self_links=None,
        no_self_links=False,
        node_limit=None,
        matchable_multilayer_ids=None,
        assign_to_neighbouring_module=False,
        meta_data=None,
        meta_data_rate=_DEFAULT_META_DATA_RATE,
        meta_data_unweighted=False,
        # output
        tree=False,
        ftree=False,
        clu=False,
        verbosity_level=_DEFAULT_VERBOSITY_LEVEL,
        silent=False,
        out_name=None,
        no_file_output=False,
        clu_level=None,
        output=None,
        hide_bipartite_nodes=False,
        print_all_trials=False,
        # algorithm
        two_level=False,
        flow_model=None,
        directed=None,
        recorded_teleportation=False,
        use_node_weights_as_flow=False,
        to_nodes=False,
        teleportation_probability=_DEFAULT_TELEPORTATION_PROB,
        regularized=False,
        regularization_strength=1.0,
        entropy_corrected=False,
        entropy_correction_strength=1.0,
        markov_time=1.0,
        variable_markov_time=False,
        variable_markov_damping=1.0,
        preferred_number_of_modules=None,
        multilayer_relax_rate=_DEFAULT_MULTILAYER_RELAX_RATE,
        multilayer_relax_limit=-1,
        multilayer_relax_limit_up=-1,
        multilayer_relax_limit_down=-1,
        multilayer_relax_by_jsd=False,
        # accuracy
        seed=_DEFAULT_SEED,
        num_trials=1,
        core_loop_limit=10,
        core_level_limit=None,
        tune_iteration_limit=None,
        core_loop_codelength_threshold=_DEFAULT_CORE_LOOP_CODELENGTH_THRESHOLD,
        tune_iteration_relative_threshold=_DEFAULT_TUNE_ITER_RELATIVE_THRESHOLD,
        fast_hierarchical_solution=None,
        prefer_modular_solution=False,
        inner_parallelization=False,
    ):
        """Create a new Infomap instance.

        Keyword arguments mirror the Infomap CLI flags. Use
        :class:`InfomapOptions` for a reusable configuration object and the full
        parameter reference.

        Parameters
        ----------
        args : str, optional
            Raw Infomap arguments to prepend before rendered keyword options.
        """
        options = InfomapOptions.from_mapping(locals())
        super().__init__(_package_construct_args()(args, **options.to_kwargs()))

    @classmethod
    def from_options(cls, options, args=None):
        """Create an :class:`Infomap` instance from :class:`InfomapOptions`."""
        if not isinstance(options, InfomapOptions):
            raise TypeError("options must be an InfomapOptions instance")
        return cls(args=args, **options.to_kwargs())

    # ----------------------------------------
    # Input
    # ----------------------------------------

    def read_file(self, filename, accumulate=True):
        """Read network data from file.

        Parameters
        ----------
        filename : str
        accumulate : bool, optional
            If the network data should be accumulated to already added
            nodes and links. Default ``True``.
        """
        return super().readInputData(filename, accumulate)

    def add_node(self, node_id, name=None, teleportation_weight=None):
        """Add a node.

        See Also
        --------
        set_name
        add_nodes

        Parameters
        ----------
        node_id : int
        name : str, optional
        teleportation_weight : float, optional
            Used for teleporting between layers in multilayer networks.
        """
        if name is None:
            name = ""

        if len(name) and teleportation_weight is not None:
            return super().addNode(node_id, name, teleportation_weight)
        elif len(name) and teleportation_weight is None:
            return super().addNode(node_id, name)
        elif not len(name) and teleportation_weight is not None:
            return super().addNode(node_id, teleportation_weight)

        return super().addNode(node_id)

    def add_nodes(self, nodes):
        """Add nodes.

        See Also
        --------
        add_node

        Examples
        --------
        Add nodes

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> im.add_nodes(range(4))


        Add named nodes

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> nodes = (
        ...     (1, "Node 1"),
        ...     (2, "Node 2"),
        ...     (3, "Node 3")
        ... )
        >>> im.add_nodes(nodes)
        >>> im.names
        {1: 'Node 1', 2: 'Node 2', 3: 'Node 3'}


        Add named nodes with teleportation weights

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> nodes = (
        ...     (1, "Node 1", 0.5),
        ...     (2, "Node 2", 0.2),
        ...     (3, "Node 3", 0.8)
        ... )
        >>> im.add_nodes(nodes)
        >>> im.names
        {1: 'Node 1', 2: 'Node 2', 3: 'Node 3'}

        Add named nodes using dict

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> nodes = {
        ...     1: "Node 1",
        ...     2: "Node 2",
        ...     3: "Node 3"
        ... }
        >>> im.add_nodes(nodes)
        >>> im.names
        {1: 'Node 1', 2: 'Node 2', 3: 'Node 3'}


        Add named nodes with teleportation weights using dict

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> nodes = {
        ...     1: ("Node 1", 0.5),
        ...     2: ("Node 2", 0.2),
        ...     3: ("Node 3", 0.8)
        ... }
        >>> im.add_nodes(nodes)
        >>> im.names
        {1: 'Node 1', 2: 'Node 2', 3: 'Node 3'}


        Parameters
        ----------
        nodes : iterable of tuples or iterable of int or dict of int: str or dict of int: tuple of (str, float)
            Iterable of tuples on the form
            ``(node_id, [name], [teleportation_weight])``
        """
        try:
            for node, attr in nodes.items():
                if isinstance(attr, str):
                    self.add_node(node, attr)
                else:
                    self.add_node(node, *attr)
        except AttributeError:
            for node in nodes:
                if isinstance(node, int):
                    self.add_node(node)
                else:
                    self.add_node(*node)

    def add_state_node(self, state_id, node_id):
        """Add a state node.

        Notes
        -----
        If a physical node with id node_id does not exist, it will be created.
        If you want to name the physical node, use ``set_name``.

        Parameters
        ----------
        state_id : int
        node_id : int
            Id of the physical node the state node should be added to.
        """
        return super().addStateNode(state_id, node_id)

    def add_state_nodes(self, state_nodes):
        """Add state nodes.

        See Also
        --------
        add_state_node

        Examples
        --------
        With tuples

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> states = (
        ...     (1, 1),
        ...     (2, 1),
        ...     (3, 2)
        ... )
        >>> im.add_state_nodes(states)


        With dict

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> states = {
        ...     1: 1,
        ...     2: 1,
        ...     3: 2
        ... }
        >>> im.add_state_nodes(states)


        Parameters
        ----------
        state_nodes : iterable of tuples or dict of int: int
            Iterable of tuples of the form ``(state_id, node_id)``
            or dict of the form ``{state_id: node_id}``.
        """
        try:
            for node in state_nodes.items():
                self.add_state_node(*node)
        except AttributeError:
            for node in state_nodes:
                self.add_state_node(*node)

    def set_name(self, node_id, name):
        """Set the name of a node.

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

        Examples
        --------
        With tuples

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> names = (
        ...     (1, "Node 1"),
        ...     (2, "Node 2")
        ... )
        >>> im.set_names(names)
        >>> im.names
        {1: 'Node 1', 2: 'Node 2'}


        With dict

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> names = {
        ...     1: "Node 1",
        ...     2: "Node 2"
        ... }
        >>> im.set_names(names)
        >>> im.names
        {1: 'Node 1', 2: 'Node 2'}


        See Also
        --------
        set_name, names

        Parameters
        ----------
        names : iterable of tuples or dict of int: str
            Iterable of tuples on the form ``(node_id, name)``
            or dict of the form ``{node_id: name}``.
        """
        try:
            for name in names.items():
                self.set_name(*name)
        except AttributeError:
            for name in names:
                self.set_name(*name)

    def add_link(self, source_id, target_id, weight=1.0):
        """Add a link.

        Notes
        -----
        If the source or target nodes does not exist, they will be created.

        See Also
        --------
        remove_link

        Parameters
        ----------
        source_id : int
        target_id : int
        weight : float, optional
        """
        return super().addLink(source_id, target_id, weight)

    def add_links(self, links):
        """Add several links.

        Examples
        --------

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> links = (
        ...     (1, 2),
        ...     (1, 3)
        ... )
        >>> im.add_links(links)


        See Also
        --------
        add_link
        remove_link

        Parameters
        ----------
        links : iterable of tuples
            Iterable of tuples of int of the form
            ``(source_id, target_id, [weight])``
        """
        for link in links:
            self.add_link(*link)

    def remove_link(self, source_id, target_id):
        """Remove a link.

        Notes
        -----
        Removing links will not remove nodes if they become disconnected.

        See Also
        --------
        add_link

        Parameters
        ----------
        source_id : int
        target_id : int
        """
        return self.network.removeLink(source_id, target_id)

    def remove_links(self, links):
        """Remove several links.

        Examples
        --------

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> links = (
        ...     (1, 2),
        ...     (1, 3)
        ... )
        >>> im.add_links(links)
        >>> im.remove_links(links)
        >>> im.num_links
        0


        See Also
        --------
        remove_link

        Parameters
        ----------
        links : iterable of tuples
            Iterable of tuples of the form ``(source_id, target_id)``
        """
        for link in links:
            self.remove_link(*link)

    def add_multilayer_link(
        self, source_multilayer_node, target_multilayer_node, weight=1.0
    ):
        """Add a multilayer link.

        Adds a link between layers in a multilayer network.

        Examples
        --------
        Usage with tuples:

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> source_multilayer_node = (0, 1) # layer_id, node_id
        >>> target_multilayer_node = (1, 2) # layer_id, node_id
        >>> im.add_multilayer_link(source_multilayer_node, target_multilayer_node)


        Usage with MultilayerNode

        >>> from infomap import Infomap, MultilayerNode
        >>> im = Infomap()
        >>> source_multilayer_node = MultilayerNode(layer_id=0, node_id=1)
        >>> target_multilayer_node = MultilayerNode(layer_id=1, node_id=2)
        >>> im.add_multilayer_link(source_multilayer_node, target_multilayer_node)

        Notes
        -----
        This is the full multilayer format that supports both undirected
        and directed links. Infomap will not make any changes to the network.

        Parameters
        ----------
        source_multilayer_node : tuple of int, or MultilayerNode
            If passed a tuple, it should be of the format
            ``(layer_id, node_id)``.
        target_multilayer_node : tuple of int, or MultilayerNode
            If passed a tuple, it should be of the format
            ``(layer_id, node_id)``.
        weight : float, optional

        """
        source_layer_id, source_node_id = source_multilayer_node
        target_layer_id, target_node_id = target_multilayer_node
        return super().addMultilayerLink(
            source_layer_id, source_node_id, target_layer_id, target_node_id, weight
        )

    def add_multilayer_intra_link(
        self, layer_id, source_node_id, target_node_id, weight=1.0
    ):
        """Add an intra-layer link.

        Adds a link within a layer in a multilayer network.

        Examples
        --------

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> im.add_multilayer_intra_link(1, 1, 2)
        >>> im.add_multilayer_intra_link(1, 2, 3)
        >>> im.add_multilayer_intra_link(2, 1, 3)
        >>> im.add_multilayer_intra_link(2, 3, 4)

        Notes
        -----
        This multilayer format requires a directed network, so if
        the directed flag is not present, it will add all links
        also in their opposite direction to transform the undirected
        input to directed. If no inter-layer links are added, Infomap
        will simulate those by relaxing the random walker's constraint
        to its current layer. The final state network will be generated
        on run, which will clear the temporary data structure that holds
        the provided intra-layer links.

        Parameters
        ----------
        layer_id : int
        source_node_id : int
        target_node_id : int
        weight : float, optional


        """
        return super().addMultilayerIntraLink(
            layer_id, source_node_id, target_node_id, weight
        )

    def add_multilayer_inter_link(
        self, source_layer_id, node_id, target_layer_id, weight=1.0
    ):
        """Add an inter-layer link.

        Adds a link between two layers in a multilayer network.
        The link is specified through a shared physical node, but
        that jump will not be recorded so Infomap will spread out
        this link to the next possible steps for the random walker
        in the target layer.


        Notes
        -----
        This multilayer format requires a directed network, so if
        the directed flag is not present, it will add all links
        also in their opposite direction to transform the undirected
        input to directed. If no inter-layer links are added, Infomap
        will simulate these by relaxing the random walker's constraint
        to its current layer. The final state network will be generated
        on run, which will clear the temporary data structure that holds
        the provided inter-layer links.

        Examples
        --------

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> im.add_multilayer_inter_link(1, 1, 2)
        >>> im.add_multilayer_inter_link(1, 2, 2)
        >>> im.add_multilayer_inter_link(2, 1, 1)
        >>> im.add_multilayer_inter_link(2, 3, 1)

        Parameters
        ----------
        source_layer_id : int
        node_id : int
        target_layer_id : int
        weight : float, optional

        """
        return super().addMultilayerInterLink(
            source_layer_id, node_id, target_layer_id, weight
        )

    def add_multilayer_links(self, links):
        """Add several multilayer links.

        Examples
        --------

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> links = (
        ...     ((0, 1), (1, 2)),
        ...     ((0, 3), (1, 2))
        ... )
        >>> im.add_multilayer_links(links)


        See Also
        --------
        add_multilayer_link

        Parameters
        ----------
        links : iterable of tuples
            Iterable of tuples of the form
            ``(source_node, target_node, [weight])``
        """
        for link in links:
            self.add_multilayer_link(*link)

    def remove_multilayer_link(self):
        raise NotImplementedError(
            "Removing multilayer links is not supported by the Python API."
        )

    def set_meta_data(self, node_id, meta_category):
        """Set meta data to a node.

        Examples
        --------

        >>> from infomap import Infomap
        >>> im = Infomap(silent=True, num_trials=10)
        >>> im.add_links((
        ...     (1, 2), (1, 3), (2, 3),
        ...     (3, 4),
        ...     (4, 5), (4, 6), (5, 6)
        ... ))
        >>> im.set_meta_data(1, 0)
        >>> im.set_meta_data(2, 0)
        >>> im.set_meta_data(3, 1)
        >>> im.set_meta_data(4, 1)
        >>> im.set_meta_data(5, 0)
        >>> im.set_meta_data(6, 0)
        >>> im.run(meta_data_rate=0)
        >>> im.num_top_modules
        2
        >>> im.run(meta_data_rate=2)
        >>> im.num_top_modules
        3


        Parameters
        ----------
        node_id : int
        meta_category : int
        """
        return self.network.addMetaData(node_id, meta_category)

    def add_networkx_graph(
        self,
        g,
        weight="weight",
        phys_id="phys_id",
        layer_id="layer_id",
        multilayer_inter_intra_format=True,
    ):
        """Add a NetworkX graph.

        Uses weighted links if present on the `weight` attribute.
        Treats the graph as a state network if the `phys_id` attribute
        is present and as a multilayer network if also the `layer_id`
        attribute is present on the nodes.

        Examples
        --------

        >>> import networkx as nx
        >>> from infomap import Infomap
        >>> G = nx.Graph([("a", "b"), ("a", "c")])
        >>> im = Infomap(silent=True)
        >>> mapping = im.add_networkx_graph(G)
        >>> mapping
        {0: 'a', 1: 'b', 2: 'c'}
        >>> im.run()
        >>> for node in im.nodes:
        ...     print(node.node_id, node.module_id, node.flow, mapping[node.node_id])
        0 1 0.5 a
        1 1 0.25 b
        2 1 0.25 c

        Usage with a state network

        >>> import networkx as nx
        >>> from infomap import Infomap
        >>> G = nx.Graph()
        >>> G.add_node("a", phys_id=1)
        >>> G.add_node("b", phys_id=2)
        >>> G.add_node("c", phys_id=3)
        >>> G.add_node("d", phys_id=1)
        >>> G.add_node("e", phys_id=4)
        >>> G.add_node("f", phys_id=5)
        >>> G.add_edge("a", "b")
        >>> G.add_edge("a", "c")
        >>> G.add_edge("b", "c")
        >>> G.add_edge("d", "e")
        >>> G.add_edge("d", "f")
        >>> G.add_edge("e", "f")
        >>> im = Infomap(silent=True)
        >>> mapping = im.add_networkx_graph(G)
        >>> mapping
        {0: 'a', 1: 'b', 2: 'c', 3: 'd', 4: 'e', 5: 'f'}
        >>> im.run()
        >>> for node in im.nodes:
        ...     print(node.state_id, node.node_id, node.module_id, node.flow)
        0 1 1 0.16666666666666666
        1 2 1 0.16666666666666666
        2 3 1 0.16666666666666666
        3 1 2 0.16666666666666666
        4 4 2 0.16666666666666666
        5 5 2 0.16666666666666666

        Usage with a multilayer network

        >>> import networkx as nx
        >>> from infomap import Infomap
        >>> G = nx.Graph()
        >>> G.add_node(11, phys_id=1, layer_id=1)
        >>> G.add_node(21, phys_id=2, layer_id=1)
        >>> G.add_node(22, phys_id=2, layer_id=2)
        >>> G.add_node(32, phys_id=3, layer_id=2)
        >>> G.add_edge(11, 21, weight=2)
        >>> G.add_edge(22, 32)
        >>> im = Infomap(silent=True)
        >>> mapping = im.add_networkx_graph(G)
        >>> im.run()
        >>> for node in sorted(im.nodes, key=lambda n: n.state_id):
        ...     print(node.state_id, node.module_id, f"{node.flow:.2f}", node.node_id, node.layer_id)
        11 1 0.28 1 1
        21 1 0.28 2 1
        22 2 0.22 2 2
        32 2 0.22 3 2

        Notes
        -----
        Transforms non-int labels to unique int ids.
        Assumes that all nodes are of the same type.
        If node type is string, they are added as names
        to Infomap.
        If the NetworkX graph is directed (``nx.DiGraph``), and no flow
        model has been specified in the constructor, this method
        sets the ``directed`` flag to ``True``.

        Parameters
        ----------
        g : nx.Graph
            A NetworkX graph.
        weight : str, optional
            Key to look up link weight in edge data if present. Default
            ``"weight"``.
        phys_id : str, optional
            Node attribute for physical node ids, implying a state network.
        layer_id : str, optional
            Node attribute for layer ids, implying a multilayer network.
        multilayer_inter_intra_format : bool, optional
            Use intra/inter format to simulate inter-layer links. Default
            ``True``.

        Returns
        -------
        dict
            Dict with the internal node ids as keys and original labels as
            values.
        """
        return _add_networkx_graph(
            self,
            g,
            weight=weight,
            phys_id=phys_id,
            layer_id=layer_id,
            multilayer_inter_intra_format=multilayer_inter_intra_format,
        )

    # ----------------------------------------
    # Run
    # ----------------------------------------

    @property
    def bipartite_start_id(self):
        """Get or set the bipartite start id.

        Examples
        --------

        >>> from infomap import Infomap
        >>> im = Infomap(silent=True, num_trials=10)
        >>> im.add_node(1, "Left 1")
        >>> im.add_node(2, "Left 2")
        >>> im.bipartite_start_id = 3
        >>> im.add_node(3, "Right 3")
        >>> im.add_node(4, "Right 4")
        >>> im.add_link(1, 3)
        >>> im.add_link(1, 4)
        >>> im.add_link(2, 4)
        >>> im.run()
        >>> tol = 1e-4
        >>> abs(im.codelength - 0.9182958340544896) < tol
        True


        Parameters
        ----------
        start_id : int
            The node id where the second node type starts.

        Returns
        -------
        int
            The node id where the second node type starts.
        """
        return self.network.bipartiteStartId()

    @bipartite_start_id.setter
    def bipartite_start_id(self, start_id):
        super().setBipartiteStartId(start_id)

    @property
    def initial_partition(self):
        """Get or set the initial partition.

        This is a initial configuration of nodes into modules where Infomap
        will start the optimizer.

        Examples
        --------

        >>> from infomap import Infomap
        >>> im = Infomap(silent=True)
        >>> im.add_node(1)
        >>> im.add_node(2)
        >>> im.add_node(3)
        >>> im.add_node(4)
        >>> im.add_link(1, 2)
        >>> im.add_link(1, 3)
        >>> im.add_link(2, 3)
        >>> im.add_link(2, 4)
        >>> im.initial_partition = {
        ...     1: 0,
        ...     2: 0,
        ...     3: 1,
        ...     4: 1
        ... }
        >>> im.run(no_infomap=True)
        >>> tol = 1e-4
        >>> abs(im.codelength - 3.4056390622295662) < tol
        True


        Notes
        -----
        The initial partition is saved between runs.
        If you want to use an initial partition for one run only,
        use ``run(initial_partition=partition)``.

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
        super().setInitialPartition(module_ids)

    @contextmanager
    def _initial_partition(self, partition):
        # Internal use only
        # Store the possibly set initial partition
        # and use the new initial partition for one run only.
        old_partition = self.initial_partition
        try:
            self.initial_partition = partition
            yield
        finally:
            self.initial_partition = old_partition

    def run(
        self,
        args=None,
        initial_partition=None,
        # input
        cluster_data=None,
        no_infomap=False,
        skip_adjust_bipartite_flow=False,
        bipartite_teleportation=False,
        weight_threshold=None,
        include_self_links=None,
        no_self_links=False,
        node_limit=None,
        matchable_multilayer_ids=None,
        assign_to_neighbouring_module=False,
        meta_data=None,
        meta_data_rate=_DEFAULT_META_DATA_RATE,
        meta_data_unweighted=False,
        # output
        tree=False,
        ftree=False,
        clu=False,
        verbosity_level=_DEFAULT_VERBOSITY_LEVEL,
        silent=False,
        out_name=None,
        no_file_output=False,
        clu_level=None,
        output=None,
        hide_bipartite_nodes=False,
        print_all_trials=False,
        # algorithm
        two_level=False,
        flow_model=None,
        directed=None,
        recorded_teleportation=False,
        use_node_weights_as_flow=False,
        to_nodes=False,
        teleportation_probability=_DEFAULT_TELEPORTATION_PROB,
        regularized=False,
        regularization_strength=1.0,
        entropy_corrected=False,
        entropy_correction_strength=1.0,
        markov_time=1.0,
        variable_markov_time=False,
        variable_markov_damping=1.0,
        preferred_number_of_modules=None,
        multilayer_relax_rate=_DEFAULT_MULTILAYER_RELAX_RATE,
        multilayer_relax_limit=-1,
        multilayer_relax_limit_up=-1,
        multilayer_relax_limit_down=-1,
        multilayer_relax_by_jsd=False,
        # accuracy
        seed=_DEFAULT_SEED,
        num_trials=1,
        core_loop_limit=10,
        core_level_limit=None,
        tune_iteration_limit=None,
        core_loop_codelength_threshold=_DEFAULT_CORE_LOOP_CODELENGTH_THRESHOLD,
        tune_iteration_relative_threshold=_DEFAULT_TUNE_ITER_RELATIVE_THRESHOLD,
        fast_hierarchical_solution=None,
        prefer_modular_solution=False,
        inner_parallelization=False,
    ):
        """Run Infomap.

        Keyword arguments mirror the Infomap CLI flags. Use
        :class:`InfomapOptions` for the full parameter reference and
        :meth:`run_with_options` when reusing a saved configuration.

        Parameters
        ----------
        args : str, optional
            Raw Infomap arguments to prepend before rendered keyword options.
        initial_partition : dict, optional
            Initial partition to use for this run only. See
            :attr:`initial_partition`.

        See Also
        --------
        initial_partition
        """
        options = InfomapOptions.from_mapping(locals())
        args = _package_construct_args()(args, **options.to_kwargs())

        if initial_partition is not None:
            with self._initial_partition(initial_partition):
                return super().run(args)

        return super().run(args)

    def run_with_options(self, options, *, args=None, initial_partition=None):
        """Run Infomap using a reusable :class:`InfomapOptions` instance."""
        if not isinstance(options, InfomapOptions):
            raise TypeError("options must be an InfomapOptions instance")
        return self.run(
            args=args,
            initial_partition=initial_partition,
            **options.to_kwargs(),
        )

def main():
    import sys

    args = " ".join(sys.argv[1:])
    conf = Config(args, True)  # noqa: F405
    im = Infomap(conf)
    im.run()


if __name__ == "__main__":
    main()
