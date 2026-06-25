"""Immutable :class:`Result` returned by :meth:`Infomap.run`.

A ``Result`` is a *snapshot*, not a live handle. Cheap scalar metrics
(codelength, module counts, codelength components) are captured eagerly when
``run()`` returns. The expensive node/tree/dataframe data is materialized
**lazily on first access** by a single C++ traversal
(``_core.get_node_data``), guarded by a run-generation token: the C++ result
tree is destroyed and rebuilt on every ``run()`` (design §7), so reading node
data from a ``Result`` whose ``Infomap`` has re-run since raises a clear error
instead of touching freed memory.

The §9 conventions apply to the new surface: scalars are properties,
collections are methods with defaults (``modules(depth=1, states=False)``).
Output is byte-identical to the legacy ``Infomap`` accessors (parity is a gate).
"""

from __future__ import annotations

from collections.abc import Sequence
from typing import TYPE_CHECKING, Any

from ._optional import get_pandas
from ._results import _to_dataframe_sort_columns

if TYPE_CHECKING:
    from ._facade import Infomap


pandas = get_pandas()

_DEFAULT_TO_DATAFRAME_COLUMNS = ("node_id", "module_id", "flow", "path", "name")
_DATAFRAME_COLUMN_ALIASES = {"community": "module_id"}

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
    """A lightweight, immutable view over a single leaf row of a ``Result``.

    This is *our* type, not a SWIG object: attribute access reads from the
    snapshot arrays the ``Result`` already materialized.
    """

    __slots__ = (
        "node_id",
        "state_id",
        "module_id",
        "flow",
        "depth",
        "layer_id",
        "child_index",
        "path",
        "name",
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
        path: tuple,
        name: Any,
    ) -> None:
        object.__setattr__(self, "node_id", node_id)
        object.__setattr__(self, "state_id", state_id)
        object.__setattr__(self, "module_id", module_id)
        object.__setattr__(self, "flow", flow)
        object.__setattr__(self, "depth", depth)
        object.__setattr__(self, "layer_id", layer_id)
        object.__setattr__(self, "child_index", child_index)
        object.__setattr__(self, "path", path)
        object.__setattr__(self, "name", name)

    def __setattr__(self, name: str, value: Any) -> None:
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

    Returned by :meth:`Infomap.run` (and the functional layer) and used to back
    the legacy on-instance accessors. Holds eager scalars; node/tree/dataframe
    data is materialized lazily and guarded against the engine being re-run.
    """

    __slots__ = (
        "_engine",
        "_generation",
        "_names",
        "_have_memory",
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
        "_snapshots",
        "__weakref__",
    )

    def __init__(self, engine: "Infomap", *, generation: int) -> None:
        core = engine._core
        object.__setattr__(self, "_engine", engine)
        object.__setattr__(self, "_generation", generation)
        # Names are needed for the "name" column; capture eagerly (cheap dict).
        object.__setattr__(self, "_names", dict(core.getNames()))
        object.__setattr__(self, "_have_memory", core.haveMemory())
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
        # Lazy node-data cache, keyed by (level, states).
        object.__setattr__(self, "_snapshots", {})

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

    def _snapshot(self, level: int, states: bool) -> _Snapshot:
        if self._engine._generation != self._generation:
            raise _StaleResultError(
                "stale Result: the Infomap instance was re-run since this "
                "Result was created; node-level data is no longer available "
                "(the C++ result tree is rebuilt on every run())."
            )
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
        """The meta codelength (meta entropy times meta data rate)."""
        return self._meta_codelength

    @property
    def have_memory(self) -> bool:
        """True for multilayer and memory networks."""
        return self._have_memory

    @property
    def names(self) -> dict:
        """All node names, as ``{node_id: name}``."""
        return dict(self._names)

    # -- collection accessors (§9: methods with defaults) -------------------

    def modules(self, depth: int = 1, *, states: bool = False) -> dict:
        """Map ``node_id`` (or ``state_id`` when ``states``) to ``module_id``.

        Equivalent to the legacy ``Infomap.get_modules(depth, states)``: for a
        higher-order (multilayer/memory) network, requesting physical-node
        modules without ``states`` is ambiguous and raises, mirroring the C++
        ``getModules`` guard.
        """
        if self._have_memory and not states:
            raise RuntimeError(
                "Cannot get modules on higher-order network without states."
            )
        snapshot = self._snapshot(depth, states)
        ids = snapshot.state_id if states else snapshot.node_id
        return dict(zip(ids, snapshot.module_id))

    def nodes(self, depth: int = 1, *, states: bool = False):
        """Iterate leaf :class:`TreeNode` views, depth first from the root."""
        snapshot = self._snapshot(depth, states)
        names = self._names
        for i in range(len(snapshot)):
            node_id = snapshot.node_id[i]
            yield TreeNode(
                node_id=node_id,
                state_id=snapshot.state_id[i],
                module_id=snapshot.module_id[i],
                flow=snapshot.flow[i],
                depth=snapshot.depth[i],
                layer_id=snapshot.layer_id[i],
                child_index=snapshot.child_index[i],
                path=snapshot.path[i],
                name=names.get(node_id, node_id),
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
        """A pandas DataFrame of the leaf nodes.

        Byte-identical to the legacy ``Infomap.to_dataframe``. Default columns
        ``("node_id", "module_id", "flow", "path", "name")``; ``"community"``
        is an alias for ``"module_id"``; ``"name"`` is resolved via the node
        name map (falling back to the integer ``node_id``).
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

        snapshot = self._snapshot(level, states)
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

    def _column(
        self, resolved: str, requested: str, snapshot: _Snapshot, names: dict
    ) -> list:
        if resolved == "name":
            return [names.get(nid, nid) for nid in snapshot.node_id]
        if resolved in _SNAPSHOT_COLUMNS:
            return list(getattr(snapshot, resolved))
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
                }
            )
        )
        raise ValueError(
            f"Unknown DataFrame column {requested!r}. "
            f"Available columns include: {available}."
        )
