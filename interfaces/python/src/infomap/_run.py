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
from typing import TYPE_CHECKING, Any

from ._options import _warn_advanced_tier_kwargs

if TYPE_CHECKING:
    from .result import Result


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


def _explicit_option_kwargs(options: Any) -> dict:
    """The option fields a carrier sets away from their defaults.

    The graph finders (:func:`infomap.find_communities` and
    :func:`infomap.find_igraph_communities`) fold an ``options=`` carrier into
    their own engine defaults (``silent`` and ``no_file_output`` on,
    ``num_trials`` = 10). Taking only the fields the caller actually changed --
    rather than the carrier's full ``to_kwargs()`` -- keeps a carrier left at
    its defaults from silently turning those finder defaults off. A mapping is
    returned as-is (its keys are explicit by construction); ``None`` yields an
    empty dict.
    """
    if options is None:
        return {}
    if isinstance(options, Mapping):
        return dict(options)
    to_kwargs = getattr(options, "to_kwargs", None)
    if not callable(to_kwargs):
        raise TypeError("options must be an Options instance, a mapping, or None")
    from ._options import _OPTION_DEFAULTS

    kwargs: Any = to_kwargs()
    return {
        name: value
        for name, value in kwargs.items()
        if value != getattr(_OPTION_DEFAULTS, name)
    }


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
    """A ``(2, E)`` ndarray or tensor of **integer** node ids.

    The integer-dtype requirement disambiguates from a weighted NumPy link
    matrix (rows of ``(source, target, weight)``), which carries a float
    weight column and so is routed to ``add_links`` instead. Edge-index node
    ids are always integers.
    """
    shape = getattr(obj, "shape", None)
    if shape is None or len(shape) != 2 or shape[0] != 2:
        return False
    dtype = getattr(obj, "dtype", None)
    if dtype is None:
        return False
    kind = getattr(dtype, "kind", None)
    if kind is not None:  # numpy / array-api dtype: 'i'/'u' integer, 'f' float
        return kind in "iu"
    # torch dtypes have no ``.kind``; exclude floating/complex by name.
    return "int" in str(dtype).lower()


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
        frozenset(
            {
                "weight",
                "node_id",
                "layer_id",
                "multilayer_inter_intra_format",
                "meta_attribute",
            }
        ),
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
                "meta_attribute",
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
    message = (
        f"infomap.run() forwards keyword arguments to the engine, but {names} "
        f"configure how the {kind} input is read, not the engine. Build the "
        f"network explicitly and run it, e.g. "
        f"infomap.run({constructor}(..., {misdirected[0]}=...), num_trials=10). "
        f"run() builds input with default settings; Network.from_* is the path "
        f"for non-default input."
    )
    if "directed" in misdirected:
        # `directed` names the adapter's edge orientation, not the engine's
        # flow model -- steer users away from that collision explicitly.
        message += (
            " (Here 'directed' sets the adapter's edge orientation; for the "
            "engine's directed flow model pass flow_model='directed' via "
            "Options.)"
        )
    raise TypeError(message)


# Sentinel for the common-tier keyword parameters below: it lets run() tell "the
# caller passed this option" from "left at its default", so a supplied value
# overrides the ``options`` carrier while an untouched one defers to it. Typed
# ``Any`` so the honest per-parameter annotations (``seed: int`` ...) still
# type-check against this shared default; the ``<unset>`` repr keeps
# ``inspect.signature(infomap.run)`` readable for IDE/agent introspection.
class _Unset:
    __slots__ = ()

    def __repr__(self) -> str:
        return "<unset>"


_UNSET: Any = _Unset()


def run(
    input: Any,
    *,
    options: Any = None,
    seed: int = _UNSET,
    num_trials: int = _UNSET,
    two_level: bool = _UNSET,
    directed: bool | None = _UNSET,
    markov_time: float = _UNSET,
    initial_partition: Any = None,
    **overrides: Any,
) -> "Result":
    """Run Infomap on ``input`` and return a :class:`Result`.

    Parameters
    ----------
    input : Network, networkx.Graph, igraph.Graph, scipy sparse matrix, \
            (2, E) array/tensor, str or Path, or iterable of links
        The network to partition. See the module docstring for the dispatch
        table.
    options : Options, mapping, or None, optional
        Base configuration. Any keyword argument below takes precedence.
    seed : int, optional
        Random-number-generator seed for reproducible results (default 123).
    num_trials : int, optional
        Number of independent trials to run; the best solution is kept
        (default 1).
    two_level : bool, optional
        Optimize a two-level partition instead of the default multilevel
        hierarchy.
    directed : bool, optional
        Treat links as directed (shorthand for ``flow_model="directed"``).
        For a graph, file, or link-iterable input this is an engine flag; for a
        SciPy sparse matrix or a ``(2, E)`` edge index it names the input
        adapter's orientation instead and is rejected here -- build the network
        with ``Network.from_scipy_sparse_matrix(..., directed=True)`` /
        ``Network.from_edge_index(..., directed=True)``, or pass
        ``options=Options(flow_model="directed")``.
    markov_time : float, optional
        Scale link flow to change the cost of moving between modules; higher
        values yield fewer modules.
    initial_partition : mapping, optional
        Initial module assignment for this run only.
    **overrides
        Any other Infomap engine option, as a keyword argument. These
        advanced-tier options still work but are pending-deprecated on this
        signature and leave it in 3.0 (issue #741); carry them via ``options``
        (an :class:`Options` instance or a mapping) for the sanctioned,
        warning-free path. They configure the *engine*, not how the input is
        read.

    Returns
    -------
    Result
        An immutable snapshot of the run.

    Notes
    -----
    The Python API is quiet by default. To see the engine log, attach a
    handler to the ``infomap`` logger with :func:`infomap.enable_log`
    (``infomap.enable_log(logging.DEBUG)`` for more detail); the legacy
    ``silent=`` keyword is pending-deprecated and leaves the signature in 3.0.
    The ``infomap`` command-line interface keeps its verbose default.

    Advanced engine options passed as bare keyword arguments (for example a
    direct ``regularized=True``) are pending-deprecated on this signature and
    leave it in 3.0 (issue #741); carry them via ``options`` instead -- either an
    :class:`Options` instance or a plain mapping -- which is the sanctioned,
    warning-free path. The common options (``seed``, ``num_trials``,
    ``two_level``, ``directed``, ``markov_time``) stay on the signature.

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

    A 2-row *integer* array/tensor is read as a ``(2, E)`` edge index. A weighted
    link matrix (rows of ``(source, target, weight)``) has a float column and is
    read as link rows; to pass integer link rows explicitly, use a list of
    tuples or ``Network().add_links(...)``.

    Examples
    --------
    One call from an iterable of ``(u, v[, w])`` links to a :class:`Result`:

    >>> from infomap import run
    >>> result = run([(1, 2), (1, 3), (2, 3), (4, 5), (4, 6), (5, 6), (3, 4)])
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

    # The five common-tier options are first-class keyword parameters so they
    # show up in inspect.signature(infomap.run) for IDE/agent introspection;
    # every other engine option rides **overrides. Collect the common ones the
    # caller actually supplied (still at _UNSET means "defer to options").
    common = {
        name: value
        for name, value in (
            ("seed", seed),
            ("num_trials", num_trials),
            ("two_level", two_level),
            ("directed", directed),
            ("markov_time", markov_time),
        )
        if value is not _UNSET
    }

    resolved = _resolve_options(options, {**overrides, **common})
    # Advanced-tier keywords are pending-deprecated on the run() signature and
    # move off it in 3.0 (issue #741). Warn only on bare **overrides typed
    # directly on this call; the common-tier parameters above never warn, and an
    # Options or mapping `options` carrier is the sanctioned path and stays
    # silent. The helper's caller-frame check sees the user's frame here (frame 2
    # is run()'s caller), so an in-package caller of run() is not flagged and the
    # later Infomap(**resolved) -- reached from inside the package -- does not
    # double-warn.
    if overrides:
        _warn_advanced_tier_kwargs(overrides, "init")
    # Keys the user actually supplied (kwargs + common-tier params + a mapping
    # `options`), used to reject adapter-only arguments below. An Options
    # *instance* is excluded on purpose: its to_kwargs() carries every field
    # (e.g. directed=None) the user never set, which would false-positive the
    # guard.
    user_keys = set(overrides) | set(common)
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
