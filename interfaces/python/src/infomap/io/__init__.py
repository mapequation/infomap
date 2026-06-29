"""I/O adapters for Infomap.

Network writers (``writers``), graph-library adapters (``networkx``,
``igraph``, ``scipy``, ``edge_index``) and result export helpers (``export``)
live here. These modules are imported lazily by the facade and the graph
adapters keep their optional third-party imports inside functions, so importing
``infomap`` never requires networkx / igraph / scipy / pandas to be installed.
"""
