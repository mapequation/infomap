from __future__ import annotations

from typing import Any

_pandas: Any = None
_pandas_loaded = False


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
