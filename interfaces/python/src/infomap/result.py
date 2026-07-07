"""Immutable :class:`Result` returned by :meth:`Infomap.run`.

Cheap scalar metrics (codelength, module counts, codelength components) are
captured eagerly when ``run()`` returns, so they stay valid forever. The two
kinds of node-level access differ:

- ``modules()`` / ``nodes()`` / ``to_dataframe()`` are true **snapshots** --
  materialized lazily on first access by a single C++ traversal
  (``_core.get_node_data``) and cached as plain Python data.
- ``tree()`` / ``leaf_modules()`` return **live engine iterators**, not
  snapshots, but each is wrapped so the generation is re-checked before every
  step.

Both kinds are guarded by a run-generation token: the C++ result tree is
destroyed and rebuilt on every ``run()`` (design §7), so reading node-level data
from a ``Result`` whose ``Infomap`` has re-run since raises a clear error
instead of touching freed memory.

The §9 conventions apply to the new surface: scalars are properties,
collections are methods with defaults (``modules(depth=1, states=False)``).
Output is byte-identical to the legacy ``Infomap`` accessors (parity is a gate).
"""

from __future__ import annotations

from collections.abc import Iterator, Sequence
from typing import TYPE_CHECKING, Any

from ._optional import require_pandas
from ._results import (
    _DATAFRAME_COLUMN_ALIASES,
    _DEFAULT_TO_DATAFRAME_COLUMNS,
    _to_dataframe_sort_columns,
    _unknown_dataframe_column_error,
    perplexity,
)

if TYPE_CHECKING:
    import igraph  # pyright: ignore[reportMissingImports]  # optional dep, no stubs
    import networkx
    import pandas

    from ._facade import Infomap

# build_result is internal plumbing shared by Infomap.run and Network.run;
# only the result types are public API.
__all__ = ["Result", "TreeNode"]

# Columns served directly from the NodeData snapshot. "name" and "module_id"
# (the "community" alias) are resolved separately.
_SNAPSHOT_COLUMNS = (
    "node_id",
    "state_id",
    "module_id",
    "flow",
    "depth",
    "layer_id",
    "child_index",
    "modular_centrality",
    "path",
)


class _StaleResultError(RuntimeError):
    """Raised when node-level data is read from a Result after a re-run."""


def build_result(engine: Any) -> "Result":
    """Bump the engine's run-generation token and return a fresh ``Result``.

    The single place that stamps a ``Result`` with the new generation, shared by
    :meth:`Infomap._run_from_options` and :meth:`Network.run`. The C++ result
    tree is rebuilt on every ``run()``, so any previously returned ``Result``
    becomes stale for node-level access (its eager scalars stay valid).

    The engine only needs the minimal contract a ``Result`` reads: a mutable
    ``_generation`` integer and a ``_core`` handle.
    """
    engine._generation += 1
    return Result(engine, generation=engine._generation)


class TreeNode:
    """A lightweight, immutable view over a single leaf node of a ``Result``.

    Yielded by :meth:`Result.nodes`. A plain Python object: attribute access
    reads from the snapshot data the ``Result`` already materialized, without
    touching the underlying C++ engine, so it stays valid after a re-run.

    Attributes
    ----------
    node_id : int
        The physical node id.
    state_id : int
        The state node id. Equals ``node_id`` for first-order networks.
    module_id : int
        The module id at the depth level the nodes were requested for.
    flow : float
        The node flow (the fraction of flow the node receives).
    depth : int
        The depth of the node in the tree (number of levels below the root;
        equals ``len(path)``).
    layer_id : int
        The layer id for multilayer networks, otherwise ``0``.
    child_index : int
        The zero-based index of the node among its parent module's children.
    modular_centrality : float
        A flow-based centrality score of the node within its parent module.
    path : tuple of int
        The tree path from the root as a tuple of one-based child indices
        (the colon-separated path in tree output files).
    name : str or int
        The physical node name, or ``node_id`` if the node is unnamed.
    state_name : str or int
        The state-node name for higher-order networks, falling back to
        ``name`` when no state name is set.
    """

    __slots__ = (
        "node_id",
        "state_id",
        "module_id",
        "flow",
        "depth",
        "layer_id",
        "child_index",
        "modular_centrality",
        "path",
        "name",
        "state_name",
    )

    def __init__(
        self,
        *,
        node_id: int,
        state_id: int,
        module_id: int,
        flow: float,
        depth: int,
        layer_id: int,
        child_index: int,
        modular_centrality: float,
        path: tuple,
        name: Any,
        state_name: Any,
    ) -> None:
        object.__setattr__(self, "node_id", node_id)
        object.__setattr__(self, "state_id", state_id)
        object.__setattr__(self, "module_id", module_id)
        object.__setattr__(self, "flow", flow)
        object.__setattr__(self, "depth", depth)
        object.__setattr__(self, "layer_id", layer_id)
        object.__setattr__(self, "child_index", child_index)
        object.__setattr__(self, "modular_centrality", modular_centrality)
        object.__setattr__(self, "path", path)
        object.__setattr__(self, "name", name)
        object.__setattr__(self, "state_name", state_name)

    def __setattr__(self, name: str, value: Any) -> None:
        raise AttributeError("TreeNode is immutable")

    def __delattr__(self, name: str) -> None:
        raise AttributeError("TreeNode is immutable")

    def __repr__(self) -> str:
        return (
            f"TreeNode(node_id={self.node_id}, module_id={self.module_id}, "
            f"flow={self.flow!r}, path={self.path!r})"
        )


class _Snapshot:
    """Per-(level, states) materialized columns from a single C++ traversal."""

    __slots__ = (
        "node_id",
        "state_id",
        "module_id",
        "flow",
        "depth",
        "layer_id",
        "child_index",
        "modular_centrality",
        "path",
    )

    def __init__(self, node_data) -> None:
        self.node_id = list(node_data.node_id)
        self.state_id = list(node_data.state_id)
        self.module_id = list(node_data.module_id)
        self.flow = list(node_data.flow)
        self.depth = list(node_data.depth)
        self.layer_id = list(node_data.layer_id)
        self.child_index = list(node_data.child_index)
        self.modular_centrality = list(node_data.modular_centrality)
        # Rebuild ragged path tuples from CSR (flat values + per-node lengths).
        path_flat = list(node_data.path_flat)
        path_len = list(node_data.path_len)
        paths = []
        offset = 0
        for length in path_len:
            paths.append(tuple(path_flat[offset : offset + length]))
            offset += length
        self.path = paths

    def __len__(self) -> int:
        return len(self.node_id)


class Result:
    """Immutable snapshot of an Infomap run.

    Returned by :meth:`Infomap.run` (and the functional :func:`infomap.run`) and
    used to back the legacy on-instance accessors. Holds eager O(1) scalars;
    tree-derived metrics (the ``effective_num_*`` modules) and
    node/tree/dataframe data are materialized lazily and guarded against the
    engine being re-run.

    Read scalars as **properties** and collections as **methods** (with
    defaults):

    Examples
    --------
    Run Infomap and read the partition off the ``Result``:

    >>> from infomap import Infomap
    >>> im = Infomap(silent=True)
    >>> im.add_links(((1, 2), (1, 3), (2, 3), (4, 5), (4, 6), (5, 6), (3, 4)))
    >>> result = im.run()
    >>> result.num_top_modules
    2
    >>> for node_id, module_id in sorted(result.modules().items()):
    ...     print(node_id, module_id)
    1 1
    2 1
    3 1
    4 2
    5 2
    6 2

    Per-node views via :meth:`nodes` (a physical-node default; pass
    ``states=True`` for state nodes):

    >>> for node in sorted(result.nodes(), key=lambda n: n.node_id):
    ...     print(node.node_id, node.module_id, f"{node.flow:.4f}")
    1 1 0.1429
    2 1 0.1429
    3 1 0.2143
    4 2 0.2143
    5 2 0.1429
    6 2 0.1429

    A ``Result`` is a snapshot, not a live handle: re-running the same
    ``Infomap`` rebuilds the C++ result tree, so node-level access on the old
    ``Result`` raises. Eager scalars (``codelength``, ``num_top_modules``, ...)
    stay valid.
    """

    __slots__ = (
        "_engine",
        "_generation",
        "_names",
        "_state_names",
        "_have_memory",
        "_directed",
        "_codelength",
        "_num_top_modules",
        "_num_non_trivial_top_modules",
        "_num_levels",
        "_num_nodes",
        "_num_links",
        "_index_codelength",
        "_module_codelength",
        "_one_level_codelength",
        "_relative_codelength_savings",
        "_entropy_rate",
        "_meta_codelength",
        "_codelengths",
        "_meta_entropy",
        "_elapsed_time",
        "_num_leaf_modules",
        "_effective_cache",
        "_snapshots",
        "__weakref__",
    )

    def __init__(self, engine: "Infomap", *, generation: int) -> None:
        core = engine._core
        object.__setattr__(self, "_engine", engine)
        object.__setattr__(self, "_generation", generation)
        # Names are needed for the "name" column; capture eagerly (cheap dict).
        object.__setattr__(self, "_names", dict(core.getNames()))
        # State-node names ({state_id: name}) for the optional "state_name"
        # column; empty for first-order networks. Captured eagerly alongside the
        # physical names so a Result stays a self-contained snapshot.
        object.__setattr__(self, "_state_names", dict(core.getStateNames()))
        object.__setattr__(self, "_have_memory", core.haveMemory())
        # Captured eagerly so a Result is a stable artifact: exporters read the
        # directedness off the snapshot, not the live engine. Derived from the
        # effective flow model, not the --directed CLI bool, so
        # ``flow_model="directed"`` (and the datasets loaders that bake it)
        # export directed graphs just like ``directed=True`` does.
        object.__setattr__(self, "_directed", not core.isUndirectedFlow())
        # Eager scalar metrics (O(1), no tree traversal).
        object.__setattr__(self, "_codelength", core.codelength())
        object.__setattr__(self, "_num_top_modules", core.numTopModules())
        object.__setattr__(
            self, "_num_non_trivial_top_modules", core.numNonTrivialTopModules()
        )
        object.__setattr__(self, "_num_levels", core.maxTreeDepth())
        object.__setattr__(self, "_num_nodes", core.network().numNodes())
        object.__setattr__(self, "_num_links", core.network().numLinks())
        object.__setattr__(self, "_index_codelength", core.getIndexCodelength())
        object.__setattr__(self, "_module_codelength", core.getModuleCodelength())
        object.__setattr__(
            self, "_one_level_codelength", core.getOneLevelCodelength()
        )
        object.__setattr__(
            self,
            "_relative_codelength_savings",
            core.getRelativeCodelengthSavings(),
        )
        object.__setattr__(self, "_entropy_rate", core.getEntropyRate())
        object.__setattr__(self, "_meta_codelength", core.getMetaCodelength())
        object.__setattr__(self, "_meta_entropy", core.getMetaCodelength(True))
        object.__setattr__(self, "_codelengths", tuple(core.codelengths()))
        object.__setattr__(self, "_elapsed_time", core.elapsedTime())
        # Tree-derived scalars are captured eagerly while the C++ result tree is
        # fresh (mirroring the legacy Infomap accessors over self._core).
        object.__setattr__(
            self, "_num_leaf_modules", sum(1 for _ in core.iterLeafModules())
        )
        # The effective-number-of-modules metrics each require a full Python-side
        # tree traversal, so they are computed lazily on first access and cached,
        # keyed by depth (see ``_effective_num_modules_cached``). Computing them
        # eagerly here roughly doubled run() wall-clock for callers that never
        # read them.
        object.__setattr__(self, "_effective_cache", {})
        # Lazy node-data cache, keyed by (level, states).
        object.__setattr__(self, "_snapshots", {})

    @staticmethod
    def _tree_iterator(core, depth: int, states: bool):
        """Mirror :meth:`Infomap.get_tree`: physical tree for higher-order
        networks unless ``states`` is requested."""
        if core.haveMemory() and not states:
            return core.iterTreePhysical(depth)
        return core.iterTree(depth)

    def _effective_num_modules_cached(self, depth: int) -> float:
        """Lazily compute and cache the effective number of modules at ``depth``.

        Each computation walks the result tree, so it is deferred until first
        access and memoized. The generation guard fires only if the bound engine
        was re-run *before* the first read at this depth; a value read while the
        Result was fresh stays valid afterwards (snapshot semantics).
        """
        cache = self._effective_cache
        if depth not in cache:
            self._check_generation()
            cache[depth] = self._compute_effective_num_modules(
                self._engine._core, depth
            )
        return cache[depth]

    @classmethod
    def _compute_effective_num_modules(cls, core, depth: int) -> float:
        """Mirror legacy ``Infomap.get_effective_num_modules`` over ``core``."""
        return perplexity(
            [
                module.flow
                for module in cls._tree_iterator(core, depth, False)
                if (depth == -1 and module.is_leaf_module)
                or module.depth == depth
            ]
        )

    def __setattr__(self, name: str, value: Any) -> None:
        raise AttributeError("Result is immutable")

    def __delattr__(self, name: str) -> None:
        raise AttributeError("Result is immutable")

    def __repr__(self) -> str:
        return (
            f"Result(codelength={self._codelength!r}, "
            f"num_top_modules={self._num_top_modules!r}, "
            f"num_levels={self._num_levels!r})"
        )

    # -- lazy node-data extraction ------------------------------------------

    def _check_generation(self) -> None:
        """Guard live C++ tree access against a re-run of the bound engine."""
        if self._engine._generation != self._generation:
            raise _StaleResultError(
                "stale Result: the Infomap instance was re-run since this "
                "Result was created; node-level data is no longer available "
                "(the C++ result tree is rebuilt on every run())."
            )

    def _guard_iteration(self, iterator):
        """Wrap a live SWIG iterator so the run generation is re-checked before
        every step, not only when the iterator was acquired.

        ``tree()`` / ``leaf_modules()`` hand back live engine iterators rather
        than materialized snapshots. Without this guard, acquiring one and then
        re-running the engine before consuming it would walk a rebuilt C++ tree;
        instead the first step raises :class:`_StaleResultError`.
        """
        iterator = iter(iterator)
        while True:
            self._check_generation()
            try:
                item = next(iterator)
            except StopIteration:
                return
            yield item

    def _snapshot(self, level: int, states: bool) -> _Snapshot:
        self._check_generation()
        key = (level, states)
        snapshot = self._snapshots.get(key)
        if snapshot is None:
            node_data = self._engine._core.get_node_data(level, states)
            snapshot = _Snapshot(node_data)
            self._snapshots[key] = snapshot
        return snapshot

    # -- scalar properties (§9) ---------------------------------------------

    @property
    def codelength(self) -> float:
        """The total (hierarchical) codelength."""
        return self._codelength

    @property
    def num_top_modules(self) -> int:
        """The number of top modules in the tree."""
        return self._num_top_modules

    @property
    def num_non_trivial_top_modules(self) -> int:
        """The number of non-trivial top modules (size not 1 or all)."""
        return self._num_non_trivial_top_modules

    @property
    def num_levels(self) -> int:
        """The max depth of the hierarchical tree."""
        return self._num_levels

    @property
    def max_depth(self) -> int:
        """Alias of :attr:`num_levels`."""
        return self._num_levels

    @property
    def num_nodes(self) -> int:
        """The number of (state) nodes."""
        return self._num_nodes

    @property
    def num_links(self) -> int:
        """The number of links."""
        return self._num_links

    @property
    def index_codelength(self) -> float:
        """The two-level index codelength."""
        return self._index_codelength

    @property
    def module_codelength(self) -> float:
        """The total codelength of the modules."""
        return self._module_codelength

    @property
    def one_level_codelength(self) -> float:
        """The one-level codelength."""
        return self._one_level_codelength

    @property
    def relative_codelength_savings(self) -> float:
        """The relative codelength savings ``1 - L / L_1``."""
        return self._relative_codelength_savings

    @property
    def entropy_rate(self) -> float:
        """The entropy rate of the network."""
        return self._entropy_rate

    @property
    def meta_codelength(self) -> float:
        """The meta codelength (meta entropy times metadata rate)."""
        return self._meta_codelength

    @property
    def meta_entropy(self) -> float:
        """The meta entropy (unweighted by the metadata rate)."""
        return self._meta_entropy

    @property
    def codelengths(self) -> tuple:
        """The total (hierarchical) codelength for each trial."""
        return self._codelengths

    @property
    def elapsed_time(self) -> float:
        """The elapsed run time in seconds."""
        return self._elapsed_time

    @property
    def num_leaf_modules(self) -> int:
        """The number of leaf modules (bottom modules containing leaf nodes)."""
        return self._num_leaf_modules

    @property
    def effective_num_top_modules(self) -> float:
        """The flow-weighted effective number of top modules.

        Measured as the perplexity of the top-module flow distribution.
        Computed lazily on first access (see :meth:`effective_num_modules`).
        """
        return self._effective_num_modules_cached(1)

    @property
    def effective_num_leaf_modules(self) -> float:
        """The flow-weighted effective number of leaf modules.

        Measured as the perplexity of the leaf-module flow distribution.
        Computed lazily on first access (see :meth:`effective_num_modules`).
        """
        return self._effective_num_modules_cached(-1)

    @property
    def have_memory(self) -> bool:
        """True for multilayer and memory networks."""
        return self._have_memory

    @property
    def names(self) -> dict[int, str]:
        """All node names, as ``{node_id: name}``.

        Returns a copy; mutating it does not affect the :class:`Result`.
        """
        return dict(self._names)

    @property
    def state_names(self) -> dict[int, str]:
        """All state-node names, as ``{state_id: name}``.

        Returns a copy; mutating it does not affect the :class:`Result`.

        Populated for higher-order (state/memory) networks whose ``*States``
        section names the state nodes; empty otherwise. Physical node names are
        available separately via :attr:`names`.
        """
        return dict(self._state_names)

    # -- collection accessors (§9: methods with defaults) -------------------

    def modules(self, depth: int = 1, *, states: bool = False) -> dict[int, int]:
        """Map ``node_id`` (or ``state_id`` when ``states``) to ``module_id``.

        Equivalent to the legacy ``Infomap.get_modules(depth, states)``: for a
        higher-order (multilayer/memory) network, requesting physical-node
        modules without ``states`` is ambiguous and raises, mirroring the C++
        ``getModules`` guard.

        Parameters
        ----------
        depth : int, optional
            The depth in the hierarchical tree of the reported module ids.
            ``1`` (default) is the top (coarsest) level; ``-1`` the bottom
            (finest) level.
        states : bool, optional
            Key the mapping by ``state_id`` when ``True``, by ``node_id``
            when ``False`` (the default).

        Returns
        -------
        dict of int to int
            ``{node_or_state_id: module_id}`` for every leaf node.

        Raises
        ------
        RuntimeError
            For a higher-order network when ``states`` is ``False``.

        See Also
        --------
        multilevel_modules : Module ids for every level at once.
        tree : Walk the full hierarchical tree.
        """
        if self._have_memory and not states:
            # RuntimeError (not ValueError) on purpose: the legacy
            # Infomap.get_modules path surfaces the C++ std::runtime_error
            # thrown by getModules (src/Infomap.h), and parity tests pin the
            # error type across both surfaces.
            raise RuntimeError(
                "Cannot get modules on higher-order network without states."
            )
        snapshot = self._snapshot(depth, states)
        ids = snapshot.state_id if states else snapshot.node_id
        return dict(zip(ids, snapshot.module_id))

    def nodes(self, depth: int = 1, *, states: bool = False) -> Iterator[TreeNode]:
        """Iterate leaf :class:`TreeNode` views, depth first from the root.

        Parameters
        ----------
        depth : int, optional
            The module level reported by ``node.module_id``. ``1`` (default)
            is the top (coarsest) level; ``-1`` the bottom (finest).
        states : bool, optional
            Iterate state nodes when ``True``. When ``False`` (the default),
            iterate physical nodes, merging state nodes with the same
            ``node_id`` if they are in the same module; the same physical
            node may then appear on different paths in the tree.

        Yields
        ------
        TreeNode
            An immutable snapshot view per leaf node.
        """
        snapshot = self._snapshot(depth, states)
        names = self._names
        state_names = self._state_names
        for i in range(len(snapshot)):
            node_id = snapshot.node_id[i]
            name = names.get(node_id, node_id)
            yield TreeNode(
                node_id=node_id,
                state_id=snapshot.state_id[i],
                module_id=snapshot.module_id[i],
                flow=snapshot.flow[i],
                depth=snapshot.depth[i],
                layer_id=snapshot.layer_id[i],
                child_index=snapshot.child_index[i],
                modular_centrality=snapshot.modular_centrality[i],
                path=snapshot.path[i],
                name=name,
                state_name=state_names.get(snapshot.state_id[i]) or name,
            )

    def tree(self, depth: int = 1, *, states: bool = False):
        """Iterate the hierarchical tree, modules and leaf nodes alike, depth
        first from the root.

        Equivalent to the legacy ``Infomap.get_tree(depth, states)``. For a
        higher-order (multilayer/memory) network the physical tree is used
        unless ``states`` is requested, mirroring the legacy semantics.

        Parameters
        ----------
        depth : int, optional
            The module level reported by ``iterator.module_id``. ``1``
            (default) is the top (coarsest) level; ``-1`` the bottom (finest).
        states : bool, optional
            Iterate over state nodes when ``True``, physical nodes when
            ``False`` (the default).

        Returns
        -------
        generator
            Yields each node in the tree (the live ``InfomapIterator`` /
            ``InfomapIteratorPhysical`` positions), depth first from the root.
            The iterator is generation-guarded: consuming it after the bound
            engine re-runs raises instead of walking a rebuilt C++ tree.
        """
        self._check_generation()
        return self._guard_iteration(
            self._tree_iterator(self._engine._core, depth, states)
        )

    def multilevel_modules(
        self, *, states: bool = False
    ) -> dict[int, tuple[int, ...]]:
        """Map ``node_id`` (or ``state_id`` when ``states``) to a tuple of
        ``module_id``, one per level from the top down.

        Equivalent to the legacy ``Infomap.get_multilevel_modules(states)``.

        Parameters
        ----------
        states : bool, optional
            Key the mapping by ``state_id`` when ``True``, by ``node_id``
            when ``False`` (the default).

        Returns
        -------
        dict of int to tuple of int
            ``{node_or_state_id: (top_module_id, ..., leaf_module_id)}``.

        See Also
        --------
        modules : Module ids for a single level.
        """
        self._check_generation()
        return self._engine._core.getMultilevelModules(states)

    def leaf_modules(self):
        """Iterate the leaf modules (bottom modules containing leaf nodes),
        depth first from the root.

        Equivalent to the legacy ``Infomap.leaf_modules`` iterator.

        Returns
        -------
        generator
            Yields each leaf module (the live ``InfomapLeafModuleIterator``
            positions). The iterator is generation-guarded: consuming it after
            the bound engine re-runs raises instead of walking a rebuilt tree.
        """
        self._check_generation()
        return self._guard_iteration(self._engine._core.iterLeafModules())

    def links(self, data: str = "weight") -> Iterator[tuple[int, int, float]]:
        """Iterate the partitioned links with their weight or flow.

        Equivalent to the legacy ``Infomap.get_links(data)``. The sources and
        targets are state ids for a state or multilayer network.

        Parameters
        ----------
        data : str, optional
            The kind of value to return, one of ``"weight"`` or ``"flow"``.
            Default ``"weight"``.

        Returns
        -------
        generator of (int, int, float)
            ``(source, target, weight_or_flow)`` tuples. The generator is
            generation-guarded like :meth:`tree`: consuming it after the bound
            engine re-runs raises instead of reading the rebuilt engine.
        """
        if data not in ("weight", "flow"):
            raise ValueError('data must be one of "weight" or "flow"')
        self._check_generation()
        flow = data != "weight"

        def _link_items():
            for (source, target), value in self._engine._core.getLinks(flow).items():
                yield (source, target, value)

        return self._guard_iteration(_link_items())

    def effective_num_modules(self, depth: int = 1) -> float:
        """Return the flow-weighted effective number of modules at ``depth``.

        Measured as the perplexity of the module flow distribution. Equivalent
        to the legacy ``Infomap.get_effective_num_modules(depth)``.

        Parameters
        ----------
        depth : int, optional
            The module level. ``1`` (default) is the top (coarsest) level;
            ``-1`` the bottom (leaf-module) level.
        """
        return self._effective_num_modules_cached(depth)

    def to_dataframe(
        self,
        columns: Sequence[str] | None = None,
        *,
        states: bool = False,
        depth: int | None = None,
        index: str | bool | None = None,
        sort: bool | str | Sequence[str] = False,
        level: int | None = None,
        depth_level: int | None = None,
    ) -> "pandas.DataFrame":
        """Return a pandas DataFrame of the leaf nodes.

        Byte-identical to the legacy ``Infomap.to_dataframe``.

        Parameters
        ----------
        columns : sequence of str, optional
            Columns to include. ``"community"`` is accepted as an alias for
            ``"module_id"``. ``"name"`` resolves the physical node name
            (falling back to the integer ``node_id``); the opt-in
            ``"state_name"`` resolves the per-state-node name for a
            higher-order network (falling back to the physical name, then
            ``node_id``); see :attr:`state_names`. Default
            ``["node_id", "module_id", "flow", "path", "name"]``.
        states : bool, optional
            Use state nodes when ``True`` and physical nodes when ``False``
            (the default).
        depth : int, optional
            The module level reported by ``module_id``, as in
            :meth:`modules`. ``1`` (default) is the top (coarsest) level;
            ``-1`` the bottom (finest).
        index : str, bool, or None, optional
            Column to set as the DataFrame index. Use ``False`` or ``None``
            (the default) to keep the default RangeIndex.
        sort : bool, str, or sequence of str, optional
            Sort by one or more columns. Use ``True`` to sort by
            ``["module_id", "node_id"]`` when available. Default ``False``.
        level : int, optional
            .. deprecated::
                Alias for ``depth``.
        depth_level : int, optional
            .. deprecated::
                Alias for ``depth``.

        Returns
        -------
        pandas.DataFrame
            One row per leaf node, with the requested columns.

        Raises
        ------
        ImportError
            If pandas is not installed.
        ValueError
            If an unknown column is requested, or if ``depth`` and one of its
            aliases are given conflicting values.
        """
        pandas = require_pandas("the DataFrame accessors")

        supplied = {
            name: value
            for name, value in (
                ("depth", depth),
                ("level", level),
                ("depth_level", depth_level),
            )
            if value is not None
        }
        if len(set(supplied.values())) > 1:
            raise ValueError(
                f"Conflicting values for the tree depth: {supplied!r}. "
                "Pass only `depth` (`level` and `depth_level` are deprecated "
                "aliases)."
            )
        resolved_depth = next(iter(supplied.values()), 1)

        if columns is None:
            columns = _DEFAULT_TO_DATAFRAME_COLUMNS

        requested_columns = list(columns)
        resolved_columns = [
            _DATAFRAME_COLUMN_ALIASES.get(column, column)
            for column in requested_columns
        ]

        snapshot = self._snapshot(resolved_depth, states)
        names = self._names

        data = {}
        for requested, resolved in zip(requested_columns, resolved_columns):
            data[requested] = self._column(resolved, requested, snapshot, names)

        dataframe = pandas.DataFrame(data, columns=requested_columns)

        if sort:
            sort_columns = _to_dataframe_sort_columns(sort, dataframe.columns)
            dataframe = dataframe.sort_values(sort_columns).reset_index(drop=True)

        if index not in (None, False):
            dataframe = dataframe.set_index(index)

        return dataframe

    def to_networkx(
        self,
        *,
        module_attribute: str | None = "infomap_module",
        path_attribute: str | None = "infomap_path",
        include_hierarchy: bool = True,
        flow_attribute: str | None = "flow",
    ) -> "networkx.Graph":
        """Build a NetworkX graph of the partitioned network.

        Equivalent to :func:`infomap.to_networkx`; see it for the parameters
        and the attribute scheme.

        Returns
        -------
        networkx.Graph or networkx.DiGraph
            ``DiGraph`` when the partitioned network is directed, else
            ``Graph``.
        """
        from .io.export import to_networkx

        return to_networkx(
            self,
            module_attribute=module_attribute,
            path_attribute=path_attribute,
            include_hierarchy=include_hierarchy,
            flow_attribute=flow_attribute,
        )

    def to_igraph(
        self,
        *,
        module_attribute: str | None = "infomap_module",
        path_attribute: str | None = "infomap_path",
        include_hierarchy: bool = True,
        flow_attribute: str | None = "flow",
    ) -> "igraph.Graph":
        """Build a python-igraph graph of the partitioned network.

        Equivalent to :func:`infomap.to_igraph`; see it for the parameters
        and the attribute scheme.

        Returns
        -------
        igraph.Graph
            Directed when the partitioned network is directed, else
            undirected.
        """
        from .io.export import to_igraph

        return to_igraph(
            self,
            module_attribute=module_attribute,
            path_attribute=path_attribute,
            include_hierarchy=include_hierarchy,
            flow_attribute=flow_attribute,
        )

    def _column(
        self, resolved: str, requested: str, snapshot: _Snapshot, names: dict
    ) -> list:
        if resolved == "name":
            return [names.get(nid, nid) for nid in snapshot.node_id]
        if resolved == "state_name":
            state_names = self._state_names
            return [
                state_names.get(sid) or names.get(nid, nid)
                for sid, nid in zip(snapshot.state_id, snapshot.node_id)
            ]
        if resolved in _SNAPSHOT_COLUMNS:
            return list(getattr(snapshot, resolved))
        raise _unknown_dataframe_column_error(requested)
