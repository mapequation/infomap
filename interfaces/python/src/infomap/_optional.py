from __future__ import annotations

import importlib
from typing import Any

_pandas: Any = None
_pandas_loaded = False


def _require(package: str, *, feature: str, extra: str | None) -> Any:
    """Import ``package`` or raise an ImportError with a uniform install hint.

    The shared guard behind the ``require_*`` helpers, so every optional
    dependency reports the same message shape. ``extra`` names the infomap
    extra that provides the package; ``None`` falls back to a bare pip hint
    for packages without a dedicated extra.
    """
    try:
        return importlib.import_module(package)
    except ImportError as exc:
        hint = (
            f'`python -m pip install "infomap[{extra}]"`'
            if extra
            else f"`python -m pip install {package}`"
        )
        raise ImportError(
            f"The {package.partition('.')[0]!r} package is required for "
            f"{feature}. Install it with {hint}."
        ) from exc


def require_networkx(feature: str = "NetworkX support") -> Any:
    """Return the networkx module, raising a uniform ImportError if absent."""
    return _require("networkx", feature=feature, extra="networkx")


def require_igraph(feature: str = "igraph support") -> Any:
    """Return the igraph module, raising a uniform ImportError if absent."""
    return _require("igraph", feature=feature, extra="igraph")


def require_scipy_sparse(feature: str = "SciPy sparse matrix support") -> Any:
    """Return scipy.sparse, raising a uniform ImportError if absent."""
    return _require("scipy.sparse", feature=feature, extra="scipy")


def require_numpy(feature: str = "array support") -> Any:
    """Return the numpy module, raising a uniform ImportError if absent.

    There is no numpy-only extra, so the hint is a bare pip install.
    """
    return _require("numpy", feature=feature, extra=None)


def require_pyarrow(feature: str = "Parquet support") -> Any:
    """Return the pyarrow module, raising a uniform ImportError if absent."""
    return _require("pyarrow", feature=feature, extra="graphrag")


def require_pandas(feature: str = "DataFrame support", *, extra: str = "pandas") -> Any:
    """Return the pandas module, raising a uniform ImportError if absent.

    Layered on :func:`get_pandas` so the lazy, cached import (and its macOS
    OpenMP rationale) stays in one place.
    """
    pandas = get_pandas()
    if pandas is None:
        _require("pandas", feature=feature, extra=extra)
    return pandas


def get_pandas() -> Any:
    """Return the pandas module, or ``None`` if pandas is not installed.

    Imported lazily on first call and cached. Importing pandas eagerly at
    module load pulls in numpy, which on macOS loads a second OpenMP runtime
    and aborts ``import infomap`` / the CLI with "OMP: Error #15" in
    environments (e.g. conda) that ship their own ``libomp``. pandas is an
    optional dependency, only needed by the DataFrame accessors, so it must
    not be imported until one of them is actually called.
    """
    global _pandas, _pandas_loaded
    if not _pandas_loaded:
        _pandas_loaded = True
        try:
            import pandas as pd
        except ImportError:
            pd = None
        _pandas = pd
    return _pandas
