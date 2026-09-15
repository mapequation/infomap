"""The tree-level selector shared by the ``Result`` readers and writers.

``level`` is the canonical name (#789): ``1`` is the top, coarsest level and
``-1`` the bottom, finest one. ``depth`` was the selector through 2.15, but the
same word is :attr:`TreeNode.depth`, a node's distance from the root, so
``result.nodes(depth=1)`` yielded nodes whose ``.depth`` is ``2``. ``depth`` and
the older ``depth_level`` stay as aliases through 2.x and leave in 3.0.
"""

from __future__ import annotations

import warnings

from ._options import LEGACY_SURFACE_WARNING, _external_stacklevel

_ALIASES = ("depth", "depth_level")


def resolve_level(
    method: str,
    level: int | None,
    depth: int | None = None,
    depth_level: int | None = None,
) -> int:
    """Return the selected level, announcing a deprecated alias when typed.

    ``level`` wins nothing by itself: passing two spellings with different
    values is a ``ValueError``, since silently picking one would hide a
    migration mistake. Equal values are accepted. A typed alias emits the legacy
    tier's ``DeprecationWarning`` -- the keyword keeps working and leaves in 3.0,
    like an advanced-tier keyword -- attributed to the caller's line. ``method``
    is the bare method name: ``write_clu`` is reached from ``Result`` and from
    the stateful ``Infomap`` writers alike, so the message names no receiver.
    """
    supplied = {
        name: value
        for name, value in (
            ("level", level),
            ("depth", depth),
            ("depth_level", depth_level),
        )
        if value is not None
    }
    if len(set(supplied.values())) > 1:
        raise ValueError(
            f"Conflicting values for the tree level: {supplied!r}. Pass only "
            "`level` (`depth` and `depth_level` are deprecated aliases)."
        )
    for alias in _ALIASES:
        if alias in supplied:
            warnings.warn(
                f"'{alias}' is deprecated as the level selector on {method}() "
                f"and leaves in 3.0; pass level={supplied[alias]!r} instead. "
                "(TreeNode.depth keeps its meaning: a node's distance from the "
                "root.)",
                LEGACY_SURFACE_WARNING,
                stacklevel=_external_stacklevel(),
            )
    return next(iter(supplied.values()), 1)
