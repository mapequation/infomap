"""Functional entry point: :func:`infomap.run`.

``run`` is the one-call front door to community detection. It accepts any
supported network representation, runs Infomap with the given settings, and
returns an immutable :class:`infomap.result.Result`.

Dispatch by input type:

- :class:`infomap.Network` -> run the already-built network in place (the
  ``Network`` owns its ``Core``; we render the settings, drive the engine, and
  stamp a ``Result`` bound to the ``Network``).
- ``networkx.Graph`` / ``igraph.Graph`` / SciPy sparse matrix / a ``(2, E)``
  edge index (ndarray or tensor) / a ``str``/``Path`` network file / an iterable
  of ``(u, v[, w])`` links -> build an :class:`infomap.Infomap`, load via the
  matching adapter, then ``im.run()``.

The settings are passed positionally as ``settings`` (a :class:`Settings`
instance or mapping) and/or as keyword ``**overrides``; overrides win.
"""

from __future__ import annotations

from collections.abc import Iterable, Mapping
from pathlib import Path
from typing import Any


def _resolve_settings(settings: Any, overrides: dict) -> dict:
    """Merge a ``Settings``/mapping with keyword overrides into a kwargs dict."""
    if settings is None:
        resolved: dict = {}
    elif isinstance(settings, Mapping):
        resolved = dict(settings)
    else:
        # Settings / InfomapOptions instance.
        to_kwargs = getattr(settings, "to_kwargs", None)
        if not callable(to_kwargs):
            raise TypeError(
                "settings must be a Settings/InfomapOptions instance, a mapping, "
                "or None"
            )
        resolved = to_kwargs()
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
        import igraph as ig
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


def run(
    input: Any,
    *,
    settings: Any = None,
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
    settings : Settings, InfomapOptions, mapping, or None, optional
        Base configuration. Keyword ``overrides`` take precedence.
    initial_partition : mapping, optional
        Initial module assignment for this run only.
    **overrides
        Individual Infomap settings (CLI-flag keyword arguments) that override
        ``settings``.

    Returns
    -------
    Result
        An immutable snapshot of the run.

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

    resolved = _resolve_settings(settings, overrides)

    # 1. An already-built Network: run it in place.
    if isinstance(input, Network):
        if initial_partition is not None:
            raise TypeError(
                "initial_partition is not supported when running a Network; set "
                "it on the Network's engine or use an Infomap instance instead."
            )
        return input.run(settings=resolved)

    # 2. A network file path.
    if isinstance(input, (str, Path)):
        im = Infomap(**resolved)
        im.read_file(str(input))
        return im.run(initial_partition=initial_partition)

    # 3. A networkx graph.
    if _is_networkx_graph(input):
        im = Infomap(**resolved)
        im.add_networkx_graph(input)
        return im.run(initial_partition=initial_partition)

    # 4. An igraph graph.
    if _is_igraph_graph(input):
        im = Infomap(**resolved)
        im.add_igraph_graph(input)
        return im.run(initial_partition=initial_partition)

    # 5. A SciPy sparse adjacency matrix.
    if _is_scipy_sparse(input):
        im = Infomap(**resolved)
        im.add_scipy_sparse_matrix(input)
        return im.run(initial_partition=initial_partition)

    # 6. A (2, E) edge index (ndarray or tensor).
    if _is_edge_index(input):
        im = Infomap(**resolved)
        im.add_edge_index(input)
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
