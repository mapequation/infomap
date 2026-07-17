"""Back-compat shim: ``infomap.export`` moved to :mod:`infomap.io.export`.

This module is kept so existing code (``from infomap.export import ...``) keeps
working. Prefer importing from :mod:`infomap.io.export` going forward; this shim
may be removed in a future major release.
"""

from __future__ import annotations

from .io.export import (
    annotate_igraph_graph as annotate_igraph_graph,
)
from .io.export import (
    annotate_networkx_graph as annotate_networkx_graph,
)
from .io.export import (
    to_igraph as to_igraph,
)
from .io.export import (
    to_networkx as to_networkx,
)
from .io.export import (
    write_gexf as write_gexf,
)
from .io.export import (
    write_graphml as write_graphml,
)

__all__ = [
    "annotate_igraph_graph",
    "annotate_networkx_graph",
    "to_igraph",
    "to_networkx",
    "write_gexf",
    "write_graphml",
]
