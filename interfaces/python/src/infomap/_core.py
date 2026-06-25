"""The single boundary to the SWIG-compiled Infomap engine.

`Core` is a transparent proxy: it owns the one `InfomapWrapper` instance and
forwards every unknown attribute to it. Every other module in the package talks
to `Core`, never to `InfomapWrapper` directly, so the binding can later be
swapped (nanobind/pybind11) by rewriting only this file. Phase 1 preserves
behaviour exactly -- typed accessors and bulk extraction arrive in Phase 2.
"""

from __future__ import annotations

from ._bindings import InfomapWrapper  # noqa: F401  (the only engine import)


class Core:
    def __init__(self, args):
        self._im = InfomapWrapper(args)

    def __getattr__(self, name):
        # Only called for attributes missing on Core itself. Guard against
        # recursion before _im exists (e.g. during unpickling/__new__).
        if name == "_im":
            raise AttributeError(name)
        return getattr(self._im, name)
