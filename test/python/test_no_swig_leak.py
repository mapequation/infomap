"""Guard: the SWIG engine is reachable ONLY through the ``_core`` boundary.

Phase 5 removed ``Infomap.__getattr__`` (the forward into ``self._core``), so
undocumented SWIG names no longer resolve on an ``Infomap`` instance.

Phase 5b removed the package-level wildcard SWIG re-export (``from ._bindings
import *`` in ``_facade.py``), so raw/undocumented SWIG types (``InfomapWrapper``,
``FlowData``, ``vector_*``, ``Config``, ...) are no longer attributes of the
``infomap`` package. The *documented* tree-walking types (``InfoNode`` and the
iterators, ``source/api/iterators.rst``) are deliberately kept — re-exported
through ``_core`` — and are pinned by ``test_public_types.py``.

The import guard below enforces the architectural invariant: only ``_core.py``
and ``_bindings.py`` may import the ``_swig`` / ``_bindings`` modules. Every
other module must talk to the engine through ``Core``.
"""

import ast
from pathlib import Path

import pytest

import infomap
from infomap import Infomap

PACKAGE_DIR = Path(infomap.__file__).parent

# Only these modules are allowed to touch the SWIG layer. ``_swig.py`` is the
# generated SWIG module itself (it imports the ``_infomap`` C extension); the
# hand-written boundary is ``_core.py`` -> ``_bindings.py`` -> ``_swig.py``.
_ALLOWED_ENGINE_IMPORTERS = {"_core.py", "_bindings.py", "_swig.py"}
# Module names that constitute the SWIG boundary, including the raw C extension.
_ENGINE_MODULES = {"_swig", "_bindings", "_infomap"}


def _names_an_engine_module(value: str) -> bool:
    return any(part in _ENGINE_MODULES for part in value.split("."))


def _imports_engine_module(source: str) -> bool:
    """True if the module reaches the SWIG layer by any import mechanism.

    Covers static ``import``/``from`` forms plus dynamic
    ``importlib.import_module("infomap._swig")`` and ``__import__("...")`` calls
    with a string-literal module name.
    """
    tree = ast.parse(source)
    for node in ast.walk(tree):
        if isinstance(node, ast.ImportFrom):
            # ``from ._swig import X`` / ``from infomap._bindings import X``
            if _names_an_engine_module(node.module or ""):
                return True
            # ``from . import _swig`` / ``from . import _bindings``
            for alias in node.names:
                if alias.name in _ENGINE_MODULES:
                    return True
        elif isinstance(node, ast.Import):
            # ``import infomap._swig`` etc.
            for alias in node.names:
                if _names_an_engine_module(alias.name):
                    return True
        elif isinstance(node, ast.Call):
            # ``importlib.import_module("infomap._swig")`` / ``__import__("...")``
            func = node.func
            func_name = (
                func.attr
                if isinstance(func, ast.Attribute)
                else func.id
                if isinstance(func, ast.Name)
                else ""
            )
            if func_name in {"import_module", "__import__"} and node.args:
                first = node.args[0]
                if (
                    isinstance(first, ast.Constant)
                    and isinstance(first.value, str)
                    and _names_an_engine_module(first.value)
                ):
                    return True
    return False


def test_only_core_and_bindings_import_the_swig_layer():
    offenders = []
    for path in sorted(PACKAGE_DIR.glob("**/*.py")):
        if path.name in _ALLOWED_ENGINE_IMPORTERS:
            continue
        if _imports_engine_module(path.read_text(encoding="utf-8")):
            offenders.append(str(path.relative_to(PACKAGE_DIR)))
    assert offenders == [], (
        "These modules import the SWIG layer directly; route them through "
        f"infomap._core instead: {offenders}"
    )


def test_package_does_not_re_export_raw_swig_types():
    # Phase 5b: the package-level ``from ._bindings import *`` re-export is gone,
    # so the raw/undocumented SWIG types stay hidden. (The documented iterator
    # types -- InfoNode etc. -- ARE re-exported; see test_public_types.py.)
    assert not hasattr(infomap, "InfomapWrapper")
    assert not hasattr(infomap, "FlowData")
    assert not hasattr(infomap, "Config")


def test_infomap_defines_no_getattr_forward():
    assert "__getattr__" not in Infomap.__dict__


def test_undocumented_swig_name_raises_attribute_error():
    im = Infomap(silent=True)
    with pytest.raises(AttributeError):
        im.setDirected


def test_documented_surface_still_works():
    im = Infomap(silent=True)
    im.add_link(0, 1)
    im.run()
    assert im.codelength is not None
    assert im.network is not None
    assert im.num_nodes == 2
