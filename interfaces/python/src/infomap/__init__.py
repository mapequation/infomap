"""Infomap: network community detection with the map equation.

Quick start
-----------
Call :func:`infomap.run` on a network and read the immutable :class:`Result`::

    import infomap

    result = infomap.run(graph, seed=123, num_trials=20)
    result.codelength        # metrics are properties (read without parentheses)
    result.modules()         # call a method to slice -> {node_id: module_id}
    result.to_dataframe()    # per-node table: node_id, module_id, flow, path, name
    result.summary()         # {metric: value} scalar row, one per run for a sweep table

``run`` accepts a NetworkX or igraph graph, a SciPy sparse matrix, a ``(2, E)``
edge index, a network file path, an iterable of ``(u, v[, w])`` links, or a
prebuilt :class:`Network`. The bundled example networks in
:mod:`infomap.datasets` need no graph library, e.g.
``infomap.run(infomap.datasets.two_triangles())``.

Options
-------
``seed``, ``num_trials``, ``two_level``, ``directed`` and ``markov_time`` are
first-class keyword arguments on :func:`run`. Any other engine option (for
example ``regularized`` or ``flow_model``) can be passed as a keyword too --
it is forwarded to :class:`Options` -- but for a reusable or validated
configuration prefer :class:`Options`::

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
from ._version import (
    __author__ as __author__,
)
from ._version import (
    __classifiers__ as __classifiers__,
)
from ._version import (
    __description__ as __description__,
)
from ._version import (
    __email__ as __email__,
)
from ._version import (
    __homepage__ as __homepage__,
)
from ._version import (
    __issues__ as __issues__,
)
from ._version import (
    __keywords__ as __keywords__,
)
from ._version import (
    __license__ as __license__,
)
from ._version import (
    __repo__ as __repo__,
)
from ._version import (
    __url__ as __url__,
)
from ._version import (
    __version__ as __version__,
)
from .errors import (
    InfomapError as InfomapError,
)
from .errors import (
    NetworkParseError as NetworkParseError,
)
from .errors import (
    NotRunError as NotRunError,
)
from .errors import (
    StaleResultError as StaleResultError,
)
from .errors import (
    __all__ as _ERRORS_ALL,
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
    from . import datasets as datasets
    from . import io as io
    from . import tl as tl
    from ._facade import *  # noqa: F401,F403
    from ._facade import __all__ as _FACADE_ALL
    from ._options import _construct_args as _construct_args

    __all__.extend(_FACADE_ALL)
    __all__.append("datasets")
    __all__.append("io")
    __all__.append("tl")
except ImportError:
    # Allow setuptools to read package metadata before the extension is built.
    pass
