"""Back-compat shim: ``infomap.graphrag`` moved to :mod:`infomap.tl.graphrag`.

This module is kept so existing code (``from infomap.graphrag import ...``)
keeps working. Prefer importing from :mod:`infomap.tl.graphrag` going forward;
this shim may be removed in a future major release.
"""

from __future__ import annotations

from .tl.graphrag import (
    GraphRAGGraph as GraphRAGGraph,
    GraphRAGRunResult as GraphRAGRunResult,
    read_graphrag as read_graphrag,
    run_graphrag_communities as run_graphrag_communities,
    write_graphrag_communities as write_graphrag_communities,
)

__all__ = [
    "GraphRAGGraph",
    "GraphRAGRunResult",
    "read_graphrag",
    "run_graphrag_communities",
    "write_graphrag_communities",
]
