"""Shared array/result helpers for the graph-library adapters.

This module holds the pieces that more than one adapter needs, so the
``scipy`` / ``edge_index`` / ``networkx`` / ``igraph`` adapters stay in sync:

* :func:`undirected_edge_items` -- the one undirected dedup policy used by the
  ``scipy`` and ``edge_index`` adapters.
* :func:`community_node_data` -- the single, non-deprecated way the community
  helpers read leaf state nodes back out of a finished run.
* :func:`require_modules` -- the shared "did Infomap actually run?" guard used by
  the export and GraphRAG helpers before they read results.
"""

from __future__ import annotations

import math
from collections.abc import Iterator
from typing import Any, NamedTuple


def require_modules(infomap: Any) -> None:
    """Raise if ``infomap`` has no module assignments yet (i.e. has not run)."""
    if not infomap._core.haveModules():
        raise ValueError(
            "Infomap results are not available. Run Infomap before exporting."
        )


def undirected_edge_items(
    triples: Iterator[tuple[int, int, float]],
) -> Iterator[tuple[int, int, float]]:
    """Collapse a stream of directed ``(source, target, weight)`` triples to an
    undirected edge list.

    Each edge is canonicalised to ``(min(i, j), max(i, j))`` and, when both
    ``(i, j)`` and ``(j, i)`` are present, the **maximum** of the colliding
    weights is kept.

    The scipy adapter feeds this generator a coo matrix that has already had
    ``sum_duplicates()`` applied, so repeated *same-direction* ``(i, j)`` entries
    are summed first; this helper then dedups across the two directions keeping
    the max. That "sum within a direction, then max across directions" policy is
    intentional and shared by both callers.
    """
    edges: dict[tuple[int, int], float] = {}
    for source, target, weight in triples:
        source_id = int(source)
        target_id = int(target)
        edge = (
            (source_id, target_id)
            if source_id <= target_id
            else (target_id, source_id)
        )
        if edge in edges:
            edges[edge] = max(edges[edge], weight)
        else:
            edges[edge] = weight

    yield from ((source, target, weight) for (source, target), weight in edges.items())


class CommunityNode(NamedTuple):
    """A leaf state node read back from a finished run."""

    state_id: int
    module_id: int
    flow: float


def apply_node_meta_data(infomap: Any, id_meta_pairs: Any) -> dict:
    """Set per-node Infomap meta data, encoding arbitrary category values to
    integers (stable first-seen order) so non-integer labels -- cell types,
    string classes -- work directly.

    ``id_meta_pairs`` yields ``(internal_id, raw_category)``; entries whose
    category is ``None`` or NaN are skipped. Returns the ``{raw_category: code}``
    encoding (callers may ignore it -- the meta map equation only uses which
    nodes share a category, not the integer values).
    """
    encoding: dict[Any, int] = {}
    for internal_id, raw in id_meta_pairs:
        if raw is None or (isinstance(raw, float) and math.isnan(raw)):
            continue
        code = encoding.get(raw)
        if code is None:
            code = len(encoding)
            encoding[raw] = code
        infomap.set_meta_data(internal_id, code)
    return encoding


def community_node_data(infomap: Any) -> Iterator[CommunityNode]:
    """Yield ``(state_id, module_id, flow)`` for each leaf state node.

    Reads through the engine core's bulk ``get_node_data(1, True)`` snapshot --
    the same non-deprecated path :mod:`infomap.io.export` uses -- rather than the
    doc-deprecated ``Infomap.nodes`` property. Keeping one convention across the
    integration layer means the community helpers and the exporters agree on how
    results are read.
    """
    node_data = infomap._core.get_node_data(1, True)
    state_ids = list(node_data.state_id)
    module_ids = list(node_data.module_id)
    flows = list(node_data.flow)
    for state_id, module_id, flow in zip(state_ids, module_ids, flows, strict=True):
        yield CommunityNode(int(state_id), int(module_id), float(flow))
