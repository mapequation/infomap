"""Renamed keyword parameters on the graph-input surface (#791 §2).

``node_id`` and ``node_ids`` read as the singular and plural of one parameter
but named two unrelated things: the *attribute name* to read a physical node
id from (``from_networkx`` / ``from_igraph`` / the finders) and an explicit
*sequence of labels* for matrix rows (``from_scipy_sparse_matrix`` /
``from_edge_index``). From 2.16 the attribute-name parameters are
``node_id_attribute`` / ``layer_id_attribute`` -- the ``*_attribute`` suffix the
same signatures already use for ``meta_attribute`` -- and the label sequence is
``node_labels``. The old spellings keep working through 2.x and leave in 3.0.
"""

from __future__ import annotations

import warnings
from typing import Any

from ._options import LEGACY_SURFACE_WARNING, _external_stacklevel


def renamed_keyword(
    method: str,
    *,
    new_name: str,
    new_value: Any,
    old_name: str,
    old_value: Any,
    default: Any,
) -> Any:
    """Return the value of a renamed keyword, announcing the old spelling.

    ``old_value`` is ``None`` unless the caller typed the deprecated keyword.
    A typed old spelling emits the legacy tier's ``DeprecationWarning`` at the
    caller's line and wins over the new parameter's *default*; typing both
    spellings with the new one set away from its default is a ``ValueError``,
    since picking one silently would hide a migration mistake.
    """
    if old_value is None:
        return new_value
    if new_value is not None and new_value != default:
        raise ValueError(
            f"{method}() got both {new_name}={new_value!r} and the deprecated "
            f"{old_name}={old_value!r}; pass only {new_name}."
        )
    warnings.warn(
        f"'{old_name}' is deprecated on {method}() and leaves in 3.0; pass "
        f"{new_name}={old_value!r} instead.",
        LEGACY_SURFACE_WARNING,
        stacklevel=_external_stacklevel(),
    )
    return old_value
