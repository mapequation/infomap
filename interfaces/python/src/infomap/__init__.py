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

try:
    from ._facade import __all__ as _FACADE_ALL
    from ._facade import *  # noqa: F401,F403
    from ._facade import _construct_args as _construct_args
    __all__.extend(_FACADE_ALL)
except ImportError:
    # Allow setuptools to read package metadata before the extension is built.
    pass
