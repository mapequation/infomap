"""Cross-check the public API surface against the Sphinx API reference.

Two directions:

1. Every object documented in ``interfaces/python/source/api/*.rst`` (via
   autodoc directives or autosummary entries) must resolve against the
   installed package — catching stale docs after a rename or removal.
2. Every name a public ``__all__`` exports must be documented somewhere in
   the API reference — catching public-but-undocumented additions.
   ``autoclass :members:`` counts as covering the class's methods, so class
   members are not required to have per-method entries.
"""

from __future__ import annotations

import importlib
import re
from pathlib import Path

import infomap
import infomap.merge
import pytest

pytestmark = pytest.mark.fast

DOCS_API = (
    Path(__file__).resolve().parents[2] / "interfaces" / "python" / "source" / "api"
)

_MODULE = re.compile(r"^\.\.\s+(?:currentmodule|module)::\s+(\S+)\s*$")
_AUTODOC = re.compile(
    r"^\.\.\s+auto(?:function|class|exception|method|attribute|data)::\s+(\S+)\s*$"
)
_AUTOSUMMARY = re.compile(r"^\.\.\s+autosummary::\s*$")
_AUTOSUMMARY_ENTRY = re.compile(r"^\s{3,}~?([\w.]+)\s*$")


def _documented_names() -> set[tuple[str, str]]:
    """Collect ``(module, dotted_name)`` for every documented object."""
    if not DOCS_API.is_dir():
        pytest.skip("docs source not available (wheel/sdist test run)")

    entries: set[tuple[str, str]] = set()
    for rst in sorted(DOCS_API.glob("*.rst")):
        module = "infomap"
        in_autosummary = False
        for line in rst.read_text(encoding="utf-8").splitlines():
            module_match = _MODULE.match(line)
            if module_match:
                module = module_match.group(1)
                in_autosummary = False
                continue
            if _AUTOSUMMARY.match(line):
                in_autosummary = True
                continue
            autodoc_match = _AUTODOC.match(line)
            if autodoc_match:
                in_autosummary = False
                entries.add((module, autodoc_match.group(1)))
                continue
            if in_autosummary:
                if line.strip().startswith(":"):
                    continue  # autosummary options like :toctree:
                entry_match = _AUTOSUMMARY_ENTRY.match(line)
                if entry_match:
                    entries.add((module, entry_match.group(1)))
                elif line.strip():
                    in_autosummary = False
    assert entries, "no autodoc entries found -- has the rst layout changed?"
    return entries


def _resolve(module: str, dotted: str):
    """Resolve a documented name to an object, or raise AssertionError."""
    # Fully qualified names (e.g. ``infomap.tl.infomap``) resolve on their
    # own; bare or attribute names resolve relative to the current module.
    candidates = [dotted] if dotted.startswith("infomap") else []
    candidates.append(f"{module}.{dotted}")
    for candidate in candidates:
        parts = candidate.split(".")
        for split in range(len(parts), 0, -1):
            try:
                obj = importlib.import_module(".".join(parts[:split]))
            except ImportError:
                continue
            try:
                for attr in parts[split:]:
                    obj = getattr(obj, attr)
            except AttributeError:
                break
            return obj
    raise AssertionError(f"documented name does not resolve: {module} :: {dotted}")


def test_every_documented_name_resolves():
    for module, dotted in sorted(_documented_names()):
        _resolve(module, dotted)


def test_public_surfaces_are_documented():
    documented = _documented_names()
    # Both the last path component and the full dotted form count: a class
    # documented as ``infomap.tl.infomap`` covers the exported ``infomap``.
    documented_names = {dotted.split(".")[-1] for _, dotted in documented}
    documented_modules = {module for module, _ in documented}

    surfaces = {
        "infomap": [
            name
            for name in infomap.__all__
            if not (name.startswith("__") and name.endswith("__"))
        ],
        "infomap.merge": list(infomap.merge.__all__),
        "infomap.io.export": list(infomap.io.export.__all__),
        "infomap.tl": list(infomap.tl.__all__),
        "infomap.datasets": list(infomap.datasets.__all__),
    }

    missing = []
    for surface, names in surfaces.items():
        for name in names:
            if name in documented_names:
                continue
            # A namespace (io/tl/datasets) counts as documented when the API
            # reference documents objects from inside it.
            qualified = (
                f"{surface}.{name}" if surface != "infomap" else f"infomap.{name}"
            )
            if any(module.startswith(qualified) for module in documented_modules):
                continue
            missing.append(f"{surface}.{name}")

    assert not missing, (
        "public names missing from the API reference "
        f"(interfaces/python/source/api/): {missing}"
    )
