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
