from __future__ import annotations

from collections.abc import Sequence
from math import log2
from typing import TYPE_CHECKING, Any

from ._optional import get_pandas


if TYPE_CHECKING:
    from ._core import Core


pandas = get_pandas()
_DEFAULT_DATAFRAME_COLUMNS = ("path", "flow", "name", "node_id")
_DEFAULT_TO_DATAFRAME_COLUMNS = ("node_id", "module_id", "flow", "path", "name")
_DATAFRAME_COLUMN_ALIASES = {"community": "module_id"}


def plogp(p):
    return (x * log2(x) if x > 0 else 0 for x in p)


def entropy(p):
    return -sum(plogp(p))


def perplexity(p):
    return 2 ** entropy(p)


def _to_dataframe_sort_columns(sort, dataframe_columns):
    if sort is True:
        sort_columns = [
            column
            for column in ("module_id", "community", "node_id")
            if column in dataframe_columns
        ]
        return sort_columns or list(dataframe_columns)

    if isinstance(sort, str):
        sort_columns = [sort]
    else:
        sort_columns = list(sort)

    missing = [column for column in sort_columns if column not in dataframe_columns]
    if missing:
        available = ", ".join(dataframe_columns)
        raise ValueError(
            f"Cannot sort by unknown DataFrame column {missing[0]!r}. "
            f"Available columns: {available}."
        )

    return sort_columns


def _to_dataframe_column_getter(requested, resolved, names, state_names):
    if resolved == "name":
        return lambda node: names.get(node.node_id, node.node_id)

    if resolved == "state_name":
        return lambda node: state_names.get(node.state_id) or names.get(
            node.node_id, node.node_id
        )

    def get_column_value(node):
        try:
            return getattr(node, resolved)
        except AttributeError as exc:
            available = ", ".join(
                sorted(
                    {
                        *_DEFAULT_TO_DATAFRAME_COLUMNS,
                        "child_index",
                        "community",
                        "depth",
                        "layer_id",
                        "modular_centrality",
                        "state_id",
                        "state_name",
                    }
                )
            )
            raise ValueError(
                f"Unknown DataFrame column {requested!r}. "
                f"Available columns include: {available}."
            ) from exc

    return get_column_value


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
    # Attributes provided by the composing ``Infomap`` host. Declared under
    # TYPE_CHECKING so pyright resolves these mixin accesses without affecting
    # runtime (annotations only; the host supplies the real values).
    if TYPE_CHECKING:
        _core: Core
        network: Any  # the untyped SWIG network instance

    # -- internal, non-deprecated implementations -------------------------------
    # The deprecated public accessors below delegate to these shared internals
    # so that internal callers can reach the same data without going through a
    # deprecated public accessor.

    def _get_modules_impl(self, depth_level=1, states=False):
        return self._core.getModules(depth_level, states)

    def _get_multilevel_modules_impl(self, states=False):
        return self._core.getMultilevelModules(states)

    def _get_tree_impl(self, depth_level=1, states=False):
        if self._core.haveMemory() and not states:
            return self._core.iterTreePhysical(depth_level)
        return self._core.iterTree(depth_level)

    def _get_nodes_impl(self, depth_level=1, states=False):
        if self._core.haveMemory() and not states:
            # iterLeafNodesPhysical is unreliable in python; filter the tree.
            return _LeafIterWrapper(self._core.iterTreePhysical(depth_level))
        return self._core.iterLeafNodes(depth_level)

    def _get_links_impl(self, data="weight"):
        if data not in ("weight", "flow"):
            raise RuntimeError('data must one of "weight" or "flow"')
        return (
            (source, target, value)
            for (source, target), value in self._core.getLinks(
                data != "weight"
            ).items()
        )

    def _get_name_impl(self, node_id, default=None):
        name = self._core.getName(node_id)
        if name == "":
            return default
        return name

    def _get_names_impl(self):
        return self._core.getNames()

    def _get_state_names_impl(self):
        return self._core.getStateNames()

    def _leaf_modules_impl(self):
        return self._core.iterLeafModules()

    def _num_leaf_modules_impl(self):
        return sum(1 for _ in self._leaf_modules_impl())

    def _max_depth_impl(self):
        return self._core.maxTreeDepth()

    def _get_effective_num_modules_impl(self, depth_level=1):
        return perplexity(
            [
                module.flow
                for module in self._get_tree_impl(depth_level)
                if depth_level == -1
                and module.is_leaf_module
                or module.depth == depth_level
            ]
        )

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
        >>> _ = im.run()
        >>> im.get_modules()
        {1: 1, 2: 1, 3: 1, 4: 2, 5: 2, 6: 2}

        >>> from infomap import Infomap
        >>> im = Infomap(silent=True)
        >>> im.read_file("states.net")
        >>> _ = im.run()
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

        .. deprecated::
            Use ``result = im.run(); result.modules(depth, states=states)``.
        """
        return self._get_modules_impl(depth_level, states)

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
        >>> _ = im.run()
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
        >>> _ = im.run()
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

        .. deprecated::
            Use ``result = im.run(); result.multilevel_modules(states=states)``.
        """
        return self._get_multilevel_modules_impl(states)

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
        >>> _ = im.run()
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

        .. deprecated::
            Use ``result = im.run(); result.modules()``.
        """
        return self._get_modules_impl(depth_level=1, states=False).items()

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

        .. deprecated::
            Use ``result = im.run(); result.multilevel_modules()``.
        """
        return self._get_multilevel_modules_impl().items()

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

        .. deprecated::
            Use ``result = im.run(); result.tree(depth, states=states)``.
        """
        return self._get_tree_impl(depth_level, states)

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

        .. deprecated::
            Use ``result = im.run(); result.nodes(depth, states=states)``.
        """
        return self._get_nodes_impl(depth_level, states)

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

        .. deprecated::
            Use ``result = im.run(); result.tree(states=True)``.
        """
        return self._get_tree_impl(depth_level=1, states=True)

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

        .. deprecated::
            Use ``result = im.run(); result.tree(states=False)``.
        """
        return self._get_tree_impl(depth_level=1, states=False)

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

        .. deprecated::
            Use ``result = im.run(); result.leaf_modules()``.
        """
        return self._leaf_modules_impl()

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

        .. deprecated::
            Use ``result = im.run(); result.nodes(states=True)``.
        """
        return self._get_nodes_impl(depth_level=1, states=True)

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

        .. deprecated::
            Use ``result = im.run(); result.nodes(states=False)``.
        """
        return self._get_nodes_impl(depth_level=1, states=False)

    def get_dataframe(
        self,
        columns: Sequence[str] | None = None,
        *,
        states: bool = True,
        depth_level: int = 1,
    ) -> Any:
        """Get a Pandas DataFrame with the selected columns.

        .. deprecated::
            Use ``result = im.run(); result.to_dataframe(...)``.

        Examples
        --------

        >>> from infomap import Infomap
        >>> im = Infomap(silent=True)
        >>> im.read_file("twotriangles.net")
        >>> _ = im.run()
        >>> im.to_dataframe(columns=["path", "flow", "name", "node_id"], states=True)
             path      flow name  node_id
        0  (1, 1)  0.214286    C        3
        1  (1, 2)  0.142857    A        1
        2  (1, 3)  0.142857    B        2
        3  (2, 1)  0.214286    D        4
        4  (2, 2)  0.142857    E        5
        5  (2, 3)  0.142857    F        6
        >>> im.to_dataframe(columns=["node_id", "module_id"], states=True)
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
            If the pandas package is not available. Install it with
            ``python -m pip install "infomap[pandas]"``.
        AttributeError
            If a column name is not available as an ``InfoNode`` attribute.

        Returns
        -------
        pandas.DataFrame
            A DataFrame containing the selected columns.
        """

        if pandas is None:
            raise ImportError(
                "Cannot import package `pandas`. Install it with "
                '`python -m pip install "infomap[pandas]"`.'
            )

        if columns is None:
            columns = _DEFAULT_DATAFRAME_COLUMNS

        nodes = self._get_nodes_impl(depth_level=depth_level, states=states)

        return pandas.DataFrame(
            [
                [
                    getattr(node, attr)
                    if attr != "name"
                    else self._get_name_impl(node.node_id, default=node.node_id)
                    for attr in columns
                ]
                for node in nodes
            ],
            columns=columns,
        )

    def to_dataframe(
        self,
        columns: Sequence[str] | None = None,
        *,
        states: bool = False,
        level: int = 1,
        index: str | bool | None = None,
        sort: bool | str | Sequence[str] = False,
        depth_level: int | None = None,
    ) -> Any:
        """Get a pandas-friendly DataFrame with Infomap results.

        Compared with :meth:`get_dataframe`, this method defaults to physical
        nodes and includes ``module_id`` for analysis workflows.

        Parameters
        ----------
        columns : sequence of str, optional
            Columns to include. ``"community"`` is accepted as an alias for
            ``"module_id"``. ``"name"`` resolves the physical node name; the
            opt-in ``"state_name"`` resolves the per-state-node name for a
            higher-order network (falling back to the physical name, then
            ``node_id``). Default
            ``["node_id", "module_id", "flow", "path", "name"]``.
        states : bool, optional
            Use state-node iterators when ``True`` and physical-node iterators
            when ``False``. Default ``False``.
        level : int, optional
            Depth level passed to :meth:`get_nodes`. Default ``1``.
        index : str, bool, or None, optional
            Column to set as the DataFrame index. Use ``False`` or ``None`` to
            keep the default RangeIndex.
        sort : bool, str, or sequence of str, optional
            Sort by one or more columns. Use ``True`` to sort by
            ``["module_id", "node_id"]`` when available. Default ``False``.
        depth_level : int, optional
            Backward-compatible alias for ``level``.

        .. deprecated::
            Use ``result = im.run(); result.to_dataframe(...)``.
        """

        if pandas is None:
            raise ImportError(
                "Cannot import package `pandas`. Install it with "
                '`python -m pip install "infomap[pandas]"`.'
            )

        if depth_level is not None:
            level = depth_level
        if columns is None:
            columns = _DEFAULT_TO_DATAFRAME_COLUMNS

        requested_columns = list(columns)
        resolved_columns = [
            _DATAFRAME_COLUMN_ALIASES.get(column, column)
            for column in requested_columns
        ]
        # "state_name" falls back to the physical name, so it needs both maps.
        need_names = bool({"name", "state_name"} & set(resolved_columns))
        names = self._get_names_impl() if need_names else None
        state_names = (
            self._get_state_names_impl() if "state_name" in resolved_columns else None
        )
        column_getters = [
            _to_dataframe_column_getter(requested, resolved, names, state_names)
            for requested, resolved in zip(
                requested_columns, resolved_columns, strict=True
            )
        ]
        data = {column: [] for column in requested_columns}

        for node in self._get_nodes_impl(depth_level=level, states=states):
            for column, getter in zip(requested_columns, column_getters, strict=True):
                data[column].append(getter(node))

        dataframe = pandas.DataFrame(data, columns=requested_columns)

        if sort:
            sort_columns = _to_dataframe_sort_columns(sort, dataframe.columns)
            dataframe = dataframe.sort_values(sort_columns).reset_index(drop=True)

        if index not in (None, False):
            dataframe = dataframe.set_index(index)

        return dataframe

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

        .. deprecated::
            Use ``result = im.run(); result.names.get(node_id)``.
        """
        return self._get_name_impl(node_id, default)

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

        .. deprecated::
            Use ``result = im.run(); result.names``.
        """
        return self._get_names_impl()

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

        .. deprecated::
            Use ``result = im.run(); result.names``.
        """
        return self._get_names_impl()

    def get_state_names(self):
        """Get all state-node names.

        Populated for higher-order (state/memory) networks whose ``*States``
        section names the state nodes; empty otherwise. Physical node names are
        available separately via :meth:`get_names`.

        See Also
        --------
        get_names
        state_names

        Returns
        -------
        dict of string
            A dict with state ids as keys and state-node names as values.

        .. deprecated::
            Use ``result = im.run(); result.state_names``.
        """
        return self._get_state_names_impl()

    @property
    def state_names(self):
        """Get all state-node names.

        Short-hand for ``get_state_names``.

        See Also
        --------
        get_state_names
        names

        Returns
        -------
        dict of string
            A dict with state ids as keys and state-node names as values.

        .. deprecated::
            Use ``result = im.run(); result.state_names``.
        """
        return self._get_state_names_impl()

    def get_links(self, data="weight"):
        """A view of the currently assigned links and their weights or flow.

        The sources and targets are state ids when we have a
        state or multilayer network.

        Examples
        --------

        >>> from infomap import Infomap
        >>> im = Infomap(silent=True)
        >>> im.read_file("twotriangles.net")
        >>> _ = im.run()
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

        .. deprecated::
            Use ``result = im.run(); result.links(data=data)``.
        """
        return self._get_links_impl(data)

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
        >>> _ = im.run()
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

        .. deprecated::
            Use ``result = im.run(); result.links()``.
        """
        return self._get_links_impl()

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
        >>> _ = im.run()
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

        .. deprecated::
            Use ``result = im.run(); result.links(data="flow")``.
        """
        return self._get_links_impl(data="flow")

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

        .. deprecated::
            Use ``result = im.run(); result.num_top_modules``.
        """
        return self._core.numTopModules()

    @property
    def num_non_trivial_top_modules(self):
        """Get the number of non-trivial top modules in the tree

        A trivial module is a module with either one or all nodes within.

        Returns
        -------
        int
            The number of non-trivial top modules

        .. deprecated::
            Use ``result = im.run(); result.num_non_trivial_top_modules``.
        """
        return self._core.numNonTrivialTopModules()

    @property
    def num_leaf_modules(self):
        """Get the number of leaf modules in the tree

        Returns
        -------
        int
            The number of leaf modules

        .. deprecated::
            Use ``result = im.run(); result.num_leaf_modules``.
        """
        return self._num_leaf_modules_impl()

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

        .. deprecated::
            Use ``result = im.run(); result.effective_num_modules(depth)``.
        """
        return self._get_effective_num_modules_impl(depth_level)

    @property
    def effective_num_top_modules(self):
        """The flow weighted effective number of top modules.

        Measured as the perplexity of the module flow distribution.

        Returns
        -------
        float
            The effective number of top modules

        .. deprecated::
            Use ``result = im.run(); result.effective_num_top_modules``.
        """
        return self._get_effective_num_modules_impl(depth_level=1)

    @property
    def effective_num_leaf_modules(self):
        """The flow weighted effective number of leaf modules.

        Measured as the perplexity of the module flow distribution.

        Returns
        -------
        float
            The effective number of top modules

        .. deprecated::
            Use ``result = im.run(); result.effective_num_leaf_modules``.
        """
        return self._get_effective_num_modules_impl(depth_level=-1)

    @property
    def max_depth(self):
        """Get the max depth of the hierarchical tree.

        Returns
        -------
        int
            The max depth

        .. deprecated::
            Use ``result = im.run(); result.max_depth``.
        """
        return self._max_depth_impl()

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

        .. deprecated::
            Use ``result = im.run(); result.num_levels``.
        """
        return self._max_depth_impl()

    @property
    def have_memory(self):
        """Returns true for multilayer and memory networks.

        Returns
        -------
        bool
            True if the network is a multilayer or memory network.

        .. deprecated::
            Use ``result = im.run(); result.have_memory``.
        """
        return self._core.haveMemory()

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

        .. deprecated::
            Use ``result = im.run(); result.index_codelength``.
        """
        return self._core.getIndexCodelength()

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

        .. deprecated::
            Use ``result = im.run(); result.module_codelength``.
        """
        return self._core.getModuleCodelength()

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

        .. deprecated::
            Use ``result = im.run(); result.one_level_codelength``.
        """
        return self._core.getOneLevelCodelength()

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

        .. deprecated::
            Use ``result = im.run(); result.relative_codelength_savings``.
        """
        return self._core.getRelativeCodelengthSavings()

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
        >>> _ = im.run()
        >>> f"{im.entropy_rate:.5f}"
        '1.25070'


        Returns
        -------
        float
            The entropy rate

        .. deprecated::
            Use ``result = im.run(); result.entropy_rate``.
        """
        return self._core.getEntropyRate()

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

        .. deprecated::
            Use ``result = im.run(); result.meta_codelength``.
        """
        return self._core.getMetaCodelength()

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

        .. deprecated::
            Use ``result = im.run(); result.meta_entropy``.
        """
        return self._core.getMetaCodelength(True)
