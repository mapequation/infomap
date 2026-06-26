"""Shared array/result helpers for the graph-library adapters.

This module holds the pieces that more than one adapter needs, so the
``scipy`` / ``edge_index`` / ``networkx`` / ``igraph`` adapters stay in sync:

* :func:`undirected_edge_items` -- the one undirected dedup policy used by the
  ``scipy`` and ``edge_index`` adapters.
* :func:`community_node_data` -- the single, non-deprecated way the community
  helpers read leaf state nodes back out of a finished run.
"""

from __future__ import annotations

from collections.abc import Iterator
from typing import Any, NamedTuple


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
