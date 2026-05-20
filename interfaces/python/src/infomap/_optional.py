from __future__ import annotations

from typing import Any

try:
    import pandas as _pandas
except ImportError:
    _pandas = None


def get_pandas() -> Any:
    return _pandas
