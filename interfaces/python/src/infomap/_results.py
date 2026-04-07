from math import log2

from ._optional import get_pandas


pandas = get_pandas()
_DEFAULT_DATAFRAME_COLUMNS = ("path", "flow", "name", "node_id")


def plogp(p):
    return (x * log2(x) if x > 0 else 0 for x in p)


def entropy(p):
    return -sum(plogp(p))


def perplexity(p):
    return 2 ** entropy(p)


class _LeafIterWrapper:
    def __init__(self, tree_iterator):
        self.it = tree_iterator

    def __iter__(self):
        return self

    def __next__(self):
        self.it.stepForward()
        while not self.it.isEnd() and not self.it.isLeaf():
            self.it.stepForward()
        if not self.it.isEnd():
            return self.it
        raise StopIteration


class _InfomapResultsMixin:
    def get_modules(self, depth_level=1, states=False):
        """Get a dict with node ids as keys and module ids as values for a given depth in the hierarchical tree.

        ::

            Level                            Root

              0                               ┌─┐
                                    ┌─────────┴─┴────────┐
                                    │                    │
                                    │                    │
                                    │                    │
                              Path  │  Module      Path  │  Module
              1                  1 ┌┼┐ 1              2 ┌┼┐ 2
                               ┌───┴─┴───┐          ┌───┴─┴───┐
                               │         │          │         │
                               │         │          │         │
                               │         │          │         │
                               │         │          │         │
              2             1 ┌┼┐ 1   2 ┌┼┐ 2    1 ┌┼┐ 3   2 ┌┼┐ 3
                          ┌───┴─┴───┐   └─┴────┐   └─┘       └─┘
                          │         │          │
                          │         │          │    ▲         ▲
                          │         │          │    └────┬────┘
                          │         │          │         │
              3        1 ┌┼┐     2 ┌┼┐      1 ┌┼┐
                         └─┘       └─┘        └─┘  ◄───  Leaf-nodes


        Path to the left of the nodes. Depth dependent module ids to the right.
        The five leaf-nodes are network-nodes. All other tree-nodes are modules.

        For example:

        The left-most node on level 3 has path 1:1:1 and belong to module 1 on level 1.

        The right-most node on level 2 has path 2:2 and belong to module 2 on level 1
        which is renamed to module 3 on level 2 as we have more modules in total on this level.

        Assuming the nodes are labelled 1-5 from left to right, then the first three nodes
        are in module 1, and the last two nodes are in module 2::

            > im.get_modules(depth_level=1)
            {1: 1, 2: 1, 3: 1, 4: 2, 5: 2}

        However, at level 2, the first two nodes are in module 1, the third node in module 2,
        and the last two nodes are in module 3::

            > im.get_modules(depth_level=2)
            {1: 1, 2: 1, 3: 2, 4: 3, 5: 3}


        Examples
        --------

        >>> from infomap import Infomap
        >>> im = Infomap(silent=True)
        >>> im.read_file("twotriangles.net")
        >>> im.run()
        >>> im.get_modules()
        {1: 1, 2: 1, 3: 1, 4: 2, 5: 2, 6: 2}

        >>> from infomap import Infomap
        >>> im = Infomap(silent=True)
        >>> im.read_file("states.net")
        >>> im.run()
        >>> im.get_modules(states=True)
        {1: 1, 2: 1, 3: 1, 4: 2, 5: 2, 6: 2}


        Notes
        -----
        In a higher-order network, a physical node (defined by ``node_id``)
        may partially exist in multiple modules. However, the ``node_id``
        can not exist multiple times as a key in the node-to-module map,
        so only one occurrence of a physical node will be retrieved.
        To get all states, use ``get_modules(states=True)``.

        Parameters
        ----------
        depth_level : int, optional
            The level in the hierarchical tree. Set to ``1`` (default) to
            return the top modules (coarsest level). Set to ``2`` for second
            coarsest level etc. Set to ``-1`` to return the bottom level
            modules (finest level). Default ``1``.
        states : bool, optional
            For higher-order networks, if ``states`` is True, it will return
            state node ids. Otherwise it will return physical node ids,
            merging state nodes with same ``node_id`` if they are in the same
            module. Note that the same physical node may end up on different
            paths in the tree.
            Default ``false``.

        Returns
        -------
        dict of int
            Dict with node ids as keys and module ids as values.
        """
        return super().getModules(depth_level, states)

    def get_multilevel_modules(self, states=False):
        """Get a dict with node ids as keys and a tuple of module ids as values.
        Each position in the tuple corresponds to a depth in the hierarchical tree,
        with the first level being the top level.

        See Also
        --------
        get_modules


        Examples
        --------

        >>> from infomap import Infomap
        >>> im = Infomap(silent=True, num_trials=10)
        >>> im.read_file("ninetriangles.net")
        >>> im.run()
        >>> for modules in sorted(im.get_multilevel_modules().values()):
        ...     print(modules)
        (1, 1)
        (1, 1)
        (1, 1)
        (1, 2)
        (1, 2)
        (1, 2)
        (1, 3)
        (1, 3)
        (1, 3)
        (2, 4)
        (2, 4)
        (2, 4)
        (2, 5)
        (2, 5)
        (2, 5)
        (2, 6)
        (2, 6)
        (2, 6)
        (3, 7)
        (3, 7)
        (3, 7)
        (4, 8)
        (4, 8)
        (4, 8)
        (5, 9)
        (5, 9)
        (5, 9)

        >>> from infomap import Infomap
        >>> im = Infomap(silent=True)
        >>> im.read_file("states.net")
        >>> im.run()
        >>> for node, modules in im.get_multilevel_modules(states=True).items():
        ...     print(node, modules)
        1 (1,)
        2 (1,)
        3 (1,)
        4 (2,)
        5 (2,)
        6 (2,)


        Notes
        -----
        In a higher-order network, a physical node (defined by ``node_id``)
        may partially exist in multiple modules. However, the ``node_id``
        can not exist multiple times as a key in the node-to-module map,
        so only one occurrence of a physical node will be retrieved.
        To get all states, use ``get_multilevel_modules(states=True)``.

        Parameters
        ----------
        states : bool, optional
            For higher-order networks, if ``states`` is True, it will return
            state node ids. Otherwise it will return physical node ids,
            merging state nodes with same ``node_id`` if they are in the same
            module. Note that the same physical node may end up on different
            paths in the tree.
            Default ``false``.

        Returns
        -------
        dict of list of int
            Dict with node ids as keys and tuple of module ids as values.
        """
        return super().getMultilevelModules(states)

    @property
    def modules(self):
        """A view of the top-level modules, mapping
        ``node_id`` to ``module_id``.

        Notes
        -----
        In a higher-order network, a physical node (defined by ``node_id``)
        may partially exist in multiple modules. However, the ``node_id``
        can not exist multiple times as a key in the node-to-module map,
        so only one occurrence of a physical node will be retrieved.
        To get all states, use ``get_modules(states=True)``.

        Examples
        --------
        >>> from infomap import Infomap
        >>> im = Infomap(silent=True, num_trials=5)
        >>> im.read_file("twotriangles.net")
        >>> im.run()
        >>> for node_id, module_id in im.modules:
        ...     print(node_id, module_id)
        ...
        1 1
        2 1
        3 1
        4 2
        5 2
        6 2

        See Also
        --------
        get_modules

        Yields
        -------
        tuple of int, int
            An iterator of ``(node_id, module_id)`` pairs.
        """
        return self.get_modules(depth_level=1, states=False).items()

    @property
    def multilevel_modules(self):
        """A view of the multilevel modules, mapping
        ``node_id`` to a tuple of ``module_id``.

        Notes
        -----
        In a higher-order network, a physical node (defined by ``node_id``)
        may partially exist in multiple modules. However, the ``node_id``
        can not exist multiple times as a key in the node-to-module map,
        so only one occurrence of a physical node will be retrieved.
        To get all states, use ``get_multilevel_modules(states=True)``.

        See Also
        --------
        get_multilevel_modules

        Yields
        -------
        tuple of (int, tuple of int)
            An iterator of ``(node_id, (module_ids...)`` pairs.
        """
        return self.get_multilevel_modules().items()

    def get_tree(self, depth_level=1, states=False):
        """A view of the hierarchical tree, iterating
        over the modules as well as the leaf-nodes.

        Parameters
        ----------
        depth_level : int, optional
            The module level returned by ``iterator.module_id``. Set to ``1``
            (default) to return the top modules (coarsest level). Set to ``2``
            for second coarsest level etc. Set to ``-1`` to return the bottom
            level modules (finest level).
        states : bool, optional
            For higher-order networks, if ``states`` is True, it will iterate
            over state nodes. Otherwise it will iterate over physical nodes,
            merging state nodes with same ``node_id`` if they are in the same
            module. Note that the same physical node may end up on different
            paths in the tree.
            Default ``false``.

        Notes
        ----
        For higher-order networks, each node is represented by a set of state
        nodes with the same ``node_id``, where each state node represents a
        different constraint on the random walker. This enables
        overlapping modules, where state nodes with the same ``node_id`` end up
        in different modules. However, the state nodes with the same
        ``node_id`` within each module are only visible as one (partial)
        physical node (if ``states = False``).

        Returns
        -------
        InfomapIterator or InfomapIteratorPhysical
            An iterator over each node in the tree, depth first from the root
        """
        if self.have_memory and not states:
            return super().iterTreePhysical(depth_level)
        return super().iterTree(depth_level)

    def get_nodes(self, depth_level=1, states=False):
        """A view of the nodes in the hierarchical tree, iterating depth first
        from the root.

        Parameters
        ----------
        depth_level : int, optional
            The module level returned by ``iterator.module_id``. Set to ``1``
            (default) to return the top modules (coarsest level). Set to ``2``
            for second coarsest level etc. Set to ``-1`` to return the bottom
            level modules (finest level). Default ``1``.
        states : bool, optional
            For higher-order networks, if ``states`` is True, it will iterate
            over state nodes. Otherwise it will iterate over physical nodes,
            merging state nodes with same ``node_id`` if they are in the same
            module. Note that the same physical node may end up on different
            paths in the tree.
            See notes on ``physical_tree``. Default ``false``.

        Notes
        -----
        For higher-order networks, each node is represented by a set of state
        nodes with the same ``node_id``, where each state node represents a
        different constraint on the random walker. This enables
        overlapping modules, where state nodes with the same ``node_id`` end up
        in different modules. However, the state nodes with the same
        ``node_id`` within each module are only visible as one (partial)
        physical node (if ``states = False``).

        Returns
        -------
        InfomapIterator or InfomapIteratorPhysical
            An iterator over each node in the tree, depth first from the root
        """
        if self.have_memory and not states:
            # super().iterLeafNodesPhysical(depth_level) is unreliable in python
            return _LeafIterWrapper(super().iterTreePhysical(depth_level))
        return super().iterLeafNodes(depth_level)

    @property
    def tree(self):
        """A view of the hierarchical tree, iterating
        over the modules as well as the leaf-nodes.

        Convenience method for ``get_tree(depth_level=1, states=True)``.

        See Also
        --------
        get_tree
        InfomapIterator

        Returns
        -------
        InfomapIterator
            An iterator over each node in the tree, depth first from the root
        """
        return self.get_tree(depth_level=1, states=True)

    @property
    def physical_tree(self):
        """A view of the hierarchical tree, iterating
        over the modules as well as the leaf-nodes.
        All state nodes with the same ``node_id`` are merged to one physical node.

        Convenience method for ``get_tree(depth_level=1, states=False)``.

        See Also
        --------
        get_tree
        InfomapIteratorPhysical

        Returns
        -------
        InfomapIteratorPhysical
            An iterator over each physical node in the tree, depth first from
            the root
        """
        return self.get_tree(depth_level=1, states=False)

    @property
    def leaf_modules(self):
        """A view of the leaf modules, i.e. the bottom modules containing leaf nodes.

        See Also
        --------
        get_modules
        InfomapLeafModuleIterator

        Returns
        -------
        InfomapLeafModuleIterator
            An iterator over each leaf module in the tree, depth first from the
            root
        """
        return super().iterLeafModules()

    @property
    def nodes(self):
        """A view of the nodes in the hierarchical tree, iterating depth first
        from the root.

        Convenience method for ``get_nodes(depth_level=1, states=True)``.

        See Also
        --------
        get_nodes
        InfomapLeafIterator

        Returns
        -------
        InfomapLeafIterator
            An iterator over each leaf node in the tree, depth first from the
            root
        """
        return self.get_nodes(depth_level=1, states=True)

    @property
    def physical_nodes(self):
        """A view of the nodes in the hierarchical tree, iterating depth first
        from the root.
        All state nodes with the same ``node_id`` are merged to one physical node.

        Convenience method for ``get_nodes(depth_level=1, states=False)``.

        See Also
        --------
        get_nodes
        InfomapLeafIteratorPhysical

        Returns
        -------
        InfomapLeafIteratorPhysical
            An iterator over each physical leaf node in the tree, depth first
            from the root
        """
        return self.get_nodes(depth_level=1, states=False)

    def get_dataframe(self, columns=None, *, states=True, depth_level=1):
        """Get a Pandas DataFrame with the selected columns.

        Examples
        --------

        >>> from infomap import Infomap
        >>> im = Infomap(silent=True)
        >>> im.read_file("twotriangles.net")
        >>> im.run()
        >>> im.get_dataframe()
             path      flow name  node_id
        0  (1, 1)  0.214286    C        3
        1  (1, 2)  0.142857    A        1
        2  (1, 3)  0.142857    B        2
        3  (2, 1)  0.214286    D        4
        4  (2, 2)  0.142857    E        5
        5  (2, 3)  0.142857    F        6
        >>> im.get_dataframe(columns=["node_id", "module_id"])
           node_id  module_id
        0        3          1
        1        1          1
        2        2          1
        3        4          2
        4        5          2
        5        6          2


        See Also
        --------
        InfoNode
        InfomapLeafIterator
        InfomapLeafIteratorPhysical

        Parameters
        ----------
        columns : list(str), optional
            A list of columns that should be extracted from each node.
            Must be available as an attribute of ``InfoNode``,
            ``InfomapLeafIterator`` (for state nodes),
            or ``InfomapLeafIteratorPhysical``.
            One exception to this is ``"name"`` which is looked up internally.
            Default ``["path", "flow", "name", "node_id"]``.
        states : bool, optional
            Use state-node iterators when ``True`` and physical-node iterators
            when ``False``. Default ``True``.
        depth_level : int, optional
            Depth level passed to :meth:`get_nodes`. Default ``1``.

        Raises
        ------
        ImportError
            If the pandas package is not available.
        AttributeError
            If a column name is not available as an ``InfoNode`` attribute.

        Returns
        -------
        pandas.DataFrame
            A DataFrame containing the selected columns.
        """
        if pandas is None:
            raise ImportError("Cannot import package 'pandas'")

        if columns is None:
            columns = _DEFAULT_DATAFRAME_COLUMNS

        nodes = self.get_nodes(depth_level=depth_level, states=states)

        return pandas.DataFrame(
            [
                [
                    getattr(node, attr)
                    if attr != "name"
                    else self.get_name(node.node_id, default=node.node_id)
                    for attr in columns
                ]
                for node in nodes
            ],
            columns=columns,
        )

    def get_name(self, node_id, default=None):
        """Get the name of a node.

        Notes
        -----
        If the node name is an empty string,
        the ``default`` will be returned.

        See Also
        --------
        set_name
        names

        Parameters
        ----------
        node_id : int
        default : str, optional
            The return value if the node name is missing, default ``None``

        Returns
        -------
        str
            The node name if it exists, else the ``default``.
        """
        name = super().getName(node_id)
        if name == "":
            return default
        return name

    def get_names(self):
        """Get all node names.

        See Also
        --------
        names
        get_name

        Returns
        -------
        dict of string
            A dict with node ids as keys and node names as values.
        """
        return super().getNames()

    @property
    def names(self):
        """Get all node names.

        Short-hand for ``get_names``.

        See Also
        --------
        get_names
        get_name

        Returns
        -------
        dict of string
            A dict with node ids as keys and node names as values.
        """
        return super().getNames()

    def get_links(self, data="weight"):
        """A view of the currently assigned links and their weights or flow.

        The sources and targets are state ids when we have a
        state or multilayer network.

        Examples
        --------

        >>> from infomap import Infomap
        >>> im = Infomap(silent=True)
        >>> im.read_file("twotriangles.net")
        >>> im.run()
        >>> for link in im.get_links():
        ...     print(link)
        (1, 2, 1.0)
        (1, 3, 1.0)
        (2, 3, 1.0)
        (3, 4, 1.0)
        (4, 5, 1.0)
        (4, 6, 1.0)
        (5, 6, 1.0)
        >>> for link in im.get_links(data="flow"):
        ...     print(link)
        (1, 2, 0.14285714285714285)
        (1, 3, 0.14285714285714285)
        (2, 3, 0.14285714285714285)
        (3, 4, 0.14285714285714285)
        (4, 5, 0.14285714285714285)
        (4, 6, 0.14285714285714285)
        (5, 6, 0.14285714285714285)


        See Also
        --------
        links
        flow_links

        Parameters
        ----------
        data : str
            The kind of data to return, one of ``"weight"`` or ``"flow"``.
            Default ``"weight"``.

        Returns
        -------
        tuple of int, int, float
            An iterator of source, target, weight/flow tuples.
        """
        if data not in ("weight", "flow"):
            raise RuntimeError('data must one of "weight" or "flow"')

        return (
            (source, target, value)
            for (source, target), value in self.getLinks(data != "weight").items()
        )

    @property
    def links(self):
        """A view of the currently assigned links and their weights.

        The sources and targets are state ids when we have a
        state or multilayer network.

        Examples
        --------

        >>> from infomap import Infomap
        >>> im = Infomap(silent=True)
        >>> im.read_file("twotriangles.net")
        >>> im.run()
        >>> for link in im.links:
        ...     print(link)
        (1, 2, 1.0)
        (1, 3, 1.0)
        (2, 3, 1.0)
        (3, 4, 1.0)
        (4, 5, 1.0)
        (4, 6, 1.0)
        (5, 6, 1.0)


        See Also
        --------
        flow_links

        Returns
        -------
        tuple of int, int, float
            An iterator of source, target, weight tuples.
        """
        return self.get_links()

    @property
    def flow_links(self):
        """A view of the currently assigned links and their flow.

        The sources and targets are state ids when we have a
        state or multilayer network.

        Examples
        --------

        >>> from infomap import Infomap
        >>> im = Infomap(silent=True)
        >>> im.read_file("twotriangles.net")
        >>> im.run()
        >>> for link in im.flow_links:
        ...     print(link)
        (1, 2, 0.14285714285714285)
        (1, 3, 0.14285714285714285)
        (2, 3, 0.14285714285714285)
        (3, 4, 0.14285714285714285)
        (4, 5, 0.14285714285714285)
        (4, 6, 0.14285714285714285)
        (5, 6, 0.14285714285714285)


        See Also
        --------
        links

        Returns
        -------
        tuple of int, int, float
            An iterator of source, target, flow tuples.
        """
        return self.get_links(data="flow")

    @property
    def network(self):
        """Get the internal network."""
        return super().network()

    @property
    def num_nodes(self):
        """The number of state nodes if we have a higher order network, or the
        number of physical nodes.

        See Also
        --------
        num_physical_nodes

        Returns
        -------
        int
            The number of nodes
        """
        return self.network.numNodes()

    @property
    def num_links(self):
        """The number of links.

        Returns
        -------
        int
            The number of links
        """
        return self.network.numLinks()

    @property
    def num_physical_nodes(self):
        """The number of physical nodes.

        See Also
        --------
        num_nodes

        Returns
        -------
        int
            The number of nodes
        """
        return self.network.numPhysicalNodes()

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
    def num_non_trivial_top_modules(self):
        """Get the number of non-trivial top modules in the tree

        A trivial module is a module with either one or all nodes within.

        Returns
        -------
        int
            The number of non-trivial top modules
        """
        return super().numNonTrivialTopModules()

    @property
    def num_leaf_modules(self):
        """Get the number of leaf modules in the tree

        Returns
        -------
        int
            The number of leaf modules
        """
        num_leaf_modules = 0
        for _ in self.leaf_modules:
            num_leaf_modules += 1
        return num_leaf_modules

    def get_effective_num_modules(self, depth_level=1):
        """The flow weighted effective number of modules.

        Measured as the perplexity of the module flow distribution.

        Parameters
        ----------
        depth_level : int, optional
            The module level returned by ``iterator.depth``.
            Set to ``1`` (default) to return the top modules (coarsest level).
            Set to ``2`` for second coarsest level etc. Set to ``-1`` to return
            the bottom level modules (finest level).

        Returns
        -------
        float
            The effective number of modules
        """
        return perplexity(
            [
                module.flow
                for module in self.get_tree(depth_level)
                if depth_level == -1
                and module.is_leaf_module
                or module.depth == depth_level
            ]
        )

    @property
    def effective_num_top_modules(self):
        """The flow weighted effective number of top modules.

        Measured as the perplexity of the module flow distribution.

        Returns
        -------
        float
            The effective number of top modules
        """
        return self.get_effective_num_modules(depth_level=1)

    @property
    def effective_num_leaf_modules(self):
        """The flow weighted effective number of leaf modules.

        Measured as the perplexity of the module flow distribution.

        Returns
        -------
        float
            The effective number of top modules
        """
        return self.get_effective_num_modules(depth_level=-1)

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
    def num_levels(self):
        """Get the max depth of the hierarchical tree.
        Alias of ``max_depth``.

        See Also
        --------
        max_depth

        Returns
        -------
        int
            The max depth
        """
        return self.max_depth

    @property
    def have_memory(self):
        """Returns true for multilayer and memory networks.

        Returns
        -------
        bool
            True if the network is a multilayer or memory network.
        """
        return super().haveMemory()

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
    def codelengths(self):
        """Get the total (hierarchical) codelength for each trial.

        See Also
        --------
        codelength

        Returns
        -------
        tuple of float
            The codelengths for each trial
        """
        return super().codelengths()

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
        """Get the total codelength of the modules.

        The module codelength is defined such that
        ``codelength = index_codelength + module_codelength``

        For a hierarchical solution, the module codelength
        is the sum of codelengths for each top module.

        See Also
        --------
        codelength
        index_codelength

        Returns
        -------
        float
            The module codelength
        """
        return super().getModuleCodelength()

    @property
    def one_level_codelength(self):
        """Get the one-level codelength.

        See Also
        --------
        codelength

        Returns
        -------
        float
            The one-level codelength
        """
        return super().getOneLevelCodelength()

    @property
    def relative_codelength_savings(self):
        """Get the relative codelength savings.

        This is defined as the reduction in codelength
        relative to the non-modular one-level solution::

            S_L = 1 - L / L_1

        where ``L`` is the ``codelength`` and ``L_1``
        the ``one_level_codelength``.

        See Also
        --------
        codelength
        one_level_codelength

        Returns
        -------
        float
            The relative codelength savings
        """
        return super().getRelativeCodelengthSavings()

    @property
    def entropy_rate(self):
        """Get the entropy rate of the network.

        The entropy rate is an indication of the sparsity of a network.
        A higher entropy rate corresponds to a densely connected network.

        Notes
        -----
        This value is only accessible after running the optimizer (``im.run()``).

        Examples
        --------

        >>> from infomap import Infomap
        >>> im = Infomap(silent=True, no_infomap=True)
        >>> im.read_file("twotriangles.net")
        >>> im.run()
        >>> f"{im.entropy_rate:.5f}"
        '1.25070'


        Returns
        -------
        float
            The entropy rate
        """
        return super().getEntropyRate()

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
