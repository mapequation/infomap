"""The shared optional-dependency guards in infomap._optional.

Every ``require_*`` helper must raise an ImportError following the same
message template when its package is missing, including the install hint
(the infomap extra when one exists, a bare pip install otherwise).
"""

from __future__ import annotations

import importlib

import pytest
from infomap import _optional

pytestmark = pytest.mark.fast


def _block_import(monkeypatch, blocked: str):
    real_import_module = importlib.import_module

    def fake_import_module(name, *args, **kwargs):
        if name == blocked or name.split(".")[0] == blocked.split(".")[0]:
            raise ImportError(f"No module named {name!r}")
        return real_import_module(name, *args, **kwargs)

    monkeypatch.setattr(_optional.importlib, "import_module", fake_import_module)


@pytest.mark.parametrize(
    ("helper", "package", "match"),
    [
        (
            _optional.require_networkx,
            "networkx",
            r"'networkx' package is required for NetworkX support\. "
            r'Install it with `python -m pip install "infomap\[networkx\]"`\.',
        ),
        (
            _optional.require_igraph,
            "igraph",
            r"'igraph' package is required for igraph support\. "
            r'Install it with `python -m pip install "infomap\[igraph\]"`\.',
        ),
        (
            _optional.require_scipy_sparse,
            "scipy.sparse",
            r"'scipy' package is required for SciPy sparse matrix support\. "
            r'Install it with `python -m pip install "infomap\[scipy\]"`\.',
        ),
        (
            _optional.require_pyarrow,
            "pyarrow",
            r"'pyarrow' package is required for Parquet support\. "
            r'Install it with `python -m pip install "infomap\[graphrag\]"`\.',
        ),
        (
            _optional.require_numpy,
            "numpy",
            # No numpy extra exists, so the hint is a bare pip install.
            r"'numpy' package is required for array support\. "
            r"Install it with `python -m pip install numpy`\.",
        ),
    ],
)
def test_require_helpers_raise_uniform_import_errors(
    monkeypatch, helper, package, match
):
    _block_import(monkeypatch, package)
    with pytest.raises(ImportError, match=match):
        helper()


def test_require_pandas_raises_uniform_import_error(monkeypatch):
    _block_import(monkeypatch, "pandas")
    monkeypatch.setattr(_optional, "get_pandas", lambda: None)
    with pytest.raises(
        ImportError,
        match=r"'pandas' package is required for DataFrame support\. "
        r'Install it with `python -m pip install "infomap\[pandas\]"`\.',
    ):
        _optional.require_pandas()


def test_require_helpers_pass_through_feature_text(monkeypatch):
    _block_import(monkeypatch, "networkx")
    with pytest.raises(ImportError, match="required for frobnication support"):
        _optional.require_networkx("frobnication support")
