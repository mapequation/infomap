"""The single boundary to the SWIG-compiled Infomap engine.

`Core` is a transparent proxy: it owns the one `InfomapWrapper` instance and
forwards every unknown attribute to it. Every other module in the package talks
to `Core`, never to `InfomapWrapper` directly, so the binding can later be
swapped (nanobind/pybind11) by rewriting only this file. Phase 1 preserves
behaviour exactly -- typed accessors and bulk extraction arrive in Phase 2.

The two module-level engine functions that are part of the public package
surface (``build_info`` and ``run`` — the ``python -m infomap`` CLI driver) are
re-exported here so the rest of the package reaches them through this single
boundary instead of importing the SWIG layer directly.
"""

from __future__ import annotations

from typing import Any

from .errors import _translate_engine_errors

from ._bindings import InfomapWrapper  # noqa: F401  (the only engine import)
from ._bindings import build_info as build_info  # module-level engine function
from ._bindings import run as run  # module-level engine function (CLI driver)

# Documented tree-walking iterator/node types returned by ``Infomap.tree`` /
# ``leaf_modules`` and the physical variants (source/api/iterators.rst). They are
# public API, so they're re-exported through this single boundary -- like
# ``build_info``/``run`` above -- for the facade to surface, rather than letting
# any other module import the SWIG layer. Raw/undocumented SWIG containers
# (vector_*, Config, StateNetwork, FlowModel, FlowData, ...) are deliberately
# NOT re-exported.
from ._bindings import InfoNode as InfoNode
from ._bindings import InfomapIterator as InfomapIterator
from ._bindings import InfomapIteratorPhysical as InfomapIteratorPhysical
from ._bindings import InfomapLeafIterator as InfomapLeafIterator
from ._bindings import InfomapLeafIteratorPhysical as InfomapLeafIteratorPhysical
from ._bindings import InfomapLeafModuleIterator as InfomapLeafModuleIterator


class Core:
    def __init__(self, args):
        # Argument/config errors ("Unrecognized option: ...", option
        # conflicts) are thrown while the wrapper parses ``args``.
        with _translate_engine_errors():
            self._im = InfomapWrapper(args)

    def get_node_data(self, level=1, states=False):
        """Single-traversal bulk node data over the result tree.

        Returns the SWIG ``NodeData`` snapshot of parallel columns
        (``node_id``, ``state_id``, ``module_id``, ``flow``, ``depth``,
        ``layer_id``, ``child_index``) plus the CSR-encoded ragged path
        (``path_flat`` + ``path_len``). The sanctioned bulk-extraction entry
        point for ``Result``; mirrors the iterator choice of the legacy
        per-node accessors.
        """
        return self._im.getNodeData(level, states)

    def __getattr__(self, name: str) -> Any:
        # Only called for attributes missing on Core itself. Guard against
        # recursion before _im exists (e.g. during unpickling/__new__).
        # The ``Any`` return models the dynamic forwarding proxy honestly: the
        # SWIG instance is untyped, so any forwarded ``core.<x>`` access is Any.
        if name == "_im":
            raise AttributeError(name)
        return getattr(self._im, name)


def apply_initial_partition(core: Any, module_ids) -> bool:
    """Set an initial partition on ``core`` for the next run.

    Accepts ``{node_or_state_id: module_id}`` (integers) or, for a multilayer
    network, ``{(layer_id, node_id): module_id}``. Returns ``True`` when the
    multilayer form was applied. Shared by ``Infomap.initial_partition`` and
    ``Network.run`` so the validation/dispatch lives in one place.
    """
    if module_ids is None:
        module_ids = {}
    if module_ids and any(isinstance(key, tuple) for key in module_ids):
        if not all(isinstance(key, tuple) for key in module_ids):
            raise ValueError(
                "initial_partition keys must be either all integers "
                "(node/state ids) or all (layer_id, node_id) tuples, not mixed"
            )
        if not all(len(key) == 2 for key in module_ids):
            raise ValueError(
                "multilayer initial_partition keys must be (layer_id, node_id) "
                "2-tuples (or MultilayerNode)"
            )
        layer_ids, node_ids, modules = [], [], []
        for (layer_id, node_id), module in module_ids.items():
            layer_ids.append(int(layer_id))
            node_ids.append(int(node_id))
            modules.append(int(module))
        core.setMultilayerInitialPartition(layer_ids, node_ids, modules)
        return True
    core.setInitialPartition(module_ids)
    return False
