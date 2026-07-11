"""Infomap: network community detection with the map equation.

Quick start
-----------
Call :func:`infomap.run` on a network and read the immutable :class:`Result`::

    import infomap

    result = infomap.run(graph, seed=123, num_trials=20)
    result.codelength        # scalar metrics are properties (no parentheses)
    result.modules()         # collections are methods -> {node_id: module_id}
    result.to_dataframe()    # per-node table: node_id, module_id, flow, path, name
    result.summary()         # {metric: value} scalar row, one per run for a sweep table

``run`` accepts a NetworkX or igraph graph, a SciPy sparse matrix, a ``(2, E)``
edge index, a network file path, an iterable of ``(u, v[, w])`` links, or a
prebuilt :class:`Network`. The bundled example networks in
:mod:`infomap.datasets` need no graph library, e.g.
``infomap.run(infomap.datasets.two_triangles())``.

Options
-------
Only ``seed``, ``num_trials``, ``two_level``, ``directed`` and ``markov_time``
are keyword arguments on :func:`run`. Carry every other engine option (for
example ``regularized`` or ``flow_model``) via :class:`Options`::

    from infomap import Options

    infomap.run(graph, options=Options(regularized=True, flow_model="directed"))

``inspect.getdoc(infomap.Options)`` is the full, one-entry-per-option parameter
reference. When the *input* needs non-default reading (a different weight
attribute, explicit directedness, a state or multilayer layout), build it with
:meth:`Network.from_networkx` / ``from_igraph`` / ``from_scipy_sparse_matrix`` /
``from_edge_index`` / ``from_file`` and pass the result to ``run``.

Building incrementally and legacy code
--------------------------------------
The stateful :class:`Infomap` class still works as a builder; read results off
the :class:`Result` that ``im.run()`` returns, not off the instance. Instance
result accessors such as ``im.modules`` and ``im.codelength`` are deprecated and
leave in 3.0 -- and note the shape shift: ``im.modules`` is a property while
``result.modules()`` is a method.

Logging
-------
The Python API is quiet by default. Call :func:`enable_log` to route the engine
log through the standard :mod:`logging` module
(``infomap.enable_log(logging.DEBUG)`` for more detail).

See https://mapequation.org/infomap-python-docs/ for the full documentation.
"""

# The error taxonomy is pure Python and importable without the compiled
# bindings, so it lives outside the guarded block below.
from .errors import (
    __all__ as _ERRORS_ALL,
    InfomapError as InfomapError,
    NetworkParseError as NetworkParseError,
    NotRunError as NotRunError,
    StaleResultError as StaleResultError,
)
from ._version import (
    __author__ as __author__,
    __classifiers__ as __classifiers__,
    __description__ as __description__,
    __email__ as __email__,
    __homepage__ as __homepage__,
    __issues__ as __issues__,
    __keywords__ as __keywords__,
    __license__ as __license__,
    __repo__ as __repo__,
    __url__ as __url__,
    __version__ as __version__,
)

_VERSION_ALL = [
    "__author__",
    "__classifiers__",
    "__description__",
    "__email__",
    "__homepage__",
    "__issues__",
    "__keywords__",
    "__license__",
    "__repo__",
    "__url__",
    "__version__",
]

__all__ = list(_VERSION_ALL)
__all__.extend(_ERRORS_ALL)

# The distributed-trial merge tool lives in the pure-Python ``infomap.merge``
# module (use ``from infomap.merge import merge_trial_results`` or the CLI
# ``python -m infomap.merge``). It is intentionally NOT imported here so the
# ``python -m infomap.merge`` entry point runs without a re-import warning and
# stays usable even before the compiled bindings are built.

try:
    from ._facade import __all__ as _FACADE_ALL
    from ._facade import *  # noqa: F401,F403
    from ._options import _construct_args as _construct_args

    from . import datasets as datasets
    from . import io as io
    from . import tl as tl

    __all__.extend(_FACADE_ALL)
    __all__.append("datasets")
    __all__.append("io")
    __all__.append("tl")
except ImportError:
    # Allow setuptools to read package metadata before the extension is built.
    pass
