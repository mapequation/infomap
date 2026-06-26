"""Functional entry point: :func:`infomap.run`.

``run`` is the one-call front door to community detection. It accepts any
supported network representation, runs Infomap with the given options, and
returns an immutable :class:`infomap.result.Result`.

Dispatch by input type:

- :class:`infomap.Network` -> run the already-built network in place (the
  ``Network`` owns its ``Core``; we render the options, drive the engine, and
  stamp a ``Result`` bound to the ``Network``).
- ``networkx.Graph`` / ``igraph.Graph`` / SciPy sparse matrix / a ``(2, E)``
  edge index (ndarray or tensor) / a ``str``/``Path`` network file / an iterable
  of ``(u, v[, w])`` links -> build an :class:`infomap.Infomap`, load via the
  matching adapter, then ``im.run()``.

The options are passed as the keyword ``options`` (an :class:`Options`
instance or mapping) and/or as keyword ``**overrides``; overrides win.
"""

from __future__ import annotations

from collections.abc import Iterable, Mapping
from pathlib import Path
from typing import Any


def _resolve_options(options: Any, overrides: dict) -> dict:
    """Merge an ``Options``/mapping with keyword overrides into a kwargs dict."""
    if options is None:
        resolved: dict = {}
    elif isinstance(options, Mapping):
        resolved = dict(options)
    else:
        # Options instance. ``to_kwargs`` is reached via a
        # duck-typed getattr on an untyped object; ``callable()`` narrows it to a
        # ``-> object`` callable, so type its kwargs-dict result as Any (it
        # returns a kwargs dict by contract).
        to_kwargs = getattr(options, "to_kwargs", None)
        if not callable(to_kwargs):
            raise TypeError(
                "options must be an Options instance, a mapping, "
                "or None"
            )
        kwargs: Any = to_kwargs()
        resolved = kwargs
    resolved.update(overrides)
    return resolved


def _is_networkx_graph(obj: Any) -> bool:
    # Duck-typed, matching the networkx adapter: avoid importing networkx.
    return (
        hasattr(obj, "nodes")
        and hasattr(obj, "edges")
        and hasattr(obj, "is_directed")
        and callable(getattr(obj, "is_directed", None))
    )


def _is_igraph_graph(obj: Any) -> bool:
    try:
        import igraph as ig  # pyright: ignore[reportMissingImports]  # optional dep, no stubs
    except ImportError:
        return False
    return isinstance(obj, ig.Graph)


def _is_scipy_sparse(obj: Any) -> bool:
    try:
        import scipy.sparse as sparse
    except ImportError:
        return False
    return bool(sparse.issparse(obj))


def _is_edge_index(obj: Any) -> bool:
    """A ``(2, E)`` ndarray or tensor of integer node ids."""
    # Tensors expose detach(); ndarrays expose shape + dtype.
    shape = getattr(obj, "shape", None)
    if shape is None:
        return False
    if len(shape) != 2 or shape[0] != 2:
        return False
    # Distinguish from a 2-row plain Python list (handled as an edge iterable):
    # require an array/tensor-like object (numpy/torch carry a dtype).
    return hasattr(obj, "dtype")


# Adapter-only keyword arguments per input type. run() forwards keywords to the
# *engine* (Infomap options); these instead configure how the *input* is read,
# so they live on the matching Network.from_* constructor. Listing them lets
# run() reject a misdirected keyword with a clear pointer -- instead of a bare
# "unexpected keyword" TypeError, or (for `directed` on scipy/edge_index inputs)
# silently building a different graph than the user asked for.
_ADAPTER_KWARGS = {
    "file": ("Network.from_file", frozenset({"accumulate"})),
    "networkx": (
        "Network.from_networkx",
        frozenset({"weight", "node_id", "layer_id", "multilayer_inter_intra_format"}),
    ),
    "igraph": (
        "Network.from_igraph",
        frozenset(
            {
                "edge_weights",
                "vertex_weights",
                "node_id",
                "layer_id",
                "multilayer_inter_intra_format",
            }
        ),
    ),
    "scipy": (
        "Network.from_scipy_sparse_matrix",
        frozenset({"directed", "weighted", "node_ids"}),
    ),
    "edge_index": (
        "Network.from_edge_index",
        frozenset({"edge_weight", "num_nodes", "directed", "node_ids"}),
    ),
}


def _reject_adapter_kwargs(kind: str, user_keys: set) -> None:
    constructor, adapter_kwargs = _ADAPTER_KWARGS[kind]
    misdirected = sorted(adapter_kwargs & user_keys)
    if not misdirected:
        return
    names = ", ".join(repr(name) for name in misdirected)
    raise TypeError(
        f"infomap.run() forwards keyword arguments to the engine, but {names} "
        f"configure how the {kind} input is read, not the engine. Build the "
        f"network explicitly and run it, e.g. "
        f"infomap.run({constructor}(..., {misdirected[0]}=...), num_trials=10). "
        f"run() builds input with default settings; Network.from_* is the path "
        f"for non-default input."
    )


def run(
    input: Any,
    *,
    options: Any = None,
    initial_partition: Any = None,
    **overrides: Any,
):
    """Run Infomap on ``input`` and return a :class:`Result`.

    Parameters
    ----------
    input : Network, networkx.Graph, igraph.Graph, scipy sparse matrix, \
            (2, E) array/tensor, str or Path, or iterable of links
        The network to partition. See the module docstring for the dispatch
        table.
    options : Options, mapping, or None, optional
        Base configuration. Keyword ``overrides`` take precedence.
    initial_partition : mapping, optional
        Initial module assignment for this run only.
    **overrides
        Individual Infomap options (CLI-flag keyword arguments) that override
        ``options``. These configure the *engine*, not how the input is read.

    Returns
    -------
    Result
        An immutable snapshot of the run.

    Notes
    -----
    Keyword arguments go to the engine; the input adapters always build with
    their defaults (e.g. networkx reads the ``"weight"`` edge attribute, a
    SciPy matrix is treated as undirected). For non-default input building -- a
    different weight attribute, explicit directedness, a state/multilayer
    layout -- build the network first and run it::

        infomap.run(Network.from_networkx(g, weight="capacity"), num_trials=10)
        infomap.run(Network.from_scipy_sparse_matrix(A, directed=True))

    Passing an adapter argument to ``run()`` directly (``run(g, weight=...)``,
    ``run(A, directed=True)``) raises with a pointer to the matching
    ``Network.from_*`` constructor, rather than silently ignoring it or building
    a different graph.

    Examples
    --------
    One call from an iterable of ``(u, v[, w])`` links to a :class:`Result`:

    >>> from infomap import run
    >>> result = run(
    ...     [(1, 2), (1, 3), (2, 3), (4, 5), (4, 6), (5, 6), (3, 4)],
    ...     silent=True,
    ... )
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
    """
    # Imported lazily to avoid an import cycle with the facade, which exposes
    # this function as ``infomap.run``.
    from ._facade import Infomap
    from .network import Network

    resolved = _resolve_options(options, overrides)
    # Keys the user actually supplied (kwargs + a mapping `options`), used to
    # reject adapter-only arguments below. An Options *instance* is excluded on
    # purpose: its to_kwargs() carries every field (e.g. directed=None) the user
    # never set, which would false-positive the guard.
    user_keys = set(overrides)
    if isinstance(options, Mapping):
        user_keys |= set(options)

    # 1. An already-built Network: run it in place.
    if isinstance(input, Network):
        return input.run(options=resolved, initial_partition=initial_partition)

    # 2. A network file path.
    if isinstance(input, (str, Path)):
        _reject_adapter_kwargs("file", user_keys)
        im = Infomap(**resolved)
        im.read_file(str(input))
        return im.run(initial_partition=initial_partition)

    # 3. A networkx graph.
    if _is_networkx_graph(input):
        _reject_adapter_kwargs("networkx", user_keys)
        im = Infomap(**resolved)
        im._add_networkx_graph_impl(input)
        return im.run(initial_partition=initial_partition)

    # 4. An igraph graph.
    if _is_igraph_graph(input):
        _reject_adapter_kwargs("igraph", user_keys)
        im = Infomap(**resolved)
        im._add_igraph_graph_impl(input)
        return im.run(initial_partition=initial_partition)

    # 5. A SciPy sparse adjacency matrix.
    if _is_scipy_sparse(input):
        _reject_adapter_kwargs("scipy", user_keys)
        im = Infomap(**resolved)
        im._add_scipy_sparse_matrix_impl(input)
        return im.run(initial_partition=initial_partition)

    # 6. A (2, E) edge index (ndarray or tensor).
    if _is_edge_index(input):
        _reject_adapter_kwargs("edge_index", user_keys)
        im = Infomap(**resolved)
        im._add_edge_index_impl(input)
        return im.run(initial_partition=initial_partition)

    # 7. An iterable of (u, v[, w]) links.
    if isinstance(input, Iterable):
        im = Infomap(**resolved)
        im.add_links(input)
        return im.run(initial_partition=initial_partition)

    raise TypeError(
        f"Unsupported input type for infomap.run: {type(input).__name__}. "
        "Pass a Network, networkx/igraph graph, SciPy sparse matrix, a (2, E) "
        "edge index, a file path, or an iterable of (u, v[, w]) links."
    )
