"""Public I/O namespace for Infomap.

Network writers (``writers``), graph-library adapters (``networkx``,
``igraph``, ``scipy``, ``edge_index``) and result export helpers (``export``)
live here; reach them as ``infomap.io.export`` and so on. The graph adapters
keep their optional third-party imports inside functions, so importing
``infomap`` never requires networkx / igraph / scipy / pandas to be installed.

The result-export helpers are also re-exported at this namespace level (e.g.
``infomap.io.to_networkx``, ``infomap.io.write_graphml``), mirroring the
``infomap.tl`` convenience namespace. ``io.export`` has no module-scope
third-party imports, so this keeps ``import infomap`` dependency-free.
"""

from __future__ import annotations

from .export import (
    annotate_igraph_graph as annotate_igraph_graph,
    annotate_networkx_graph as annotate_networkx_graph,
    to_igraph as to_igraph,
    to_networkx as to_networkx,
    write_gexf as write_gexf,
    write_graphml as write_graphml,
)

# A curated __all__ so ``from infomap.io import *`` and tooling see only the
# public export helpers, not the submodules or their imported names.
__all__ = [
    "annotate_igraph_graph",
    "annotate_networkx_graph",
    "to_igraph",
    "to_networkx",
    "write_gexf",
    "write_graphml",
]
