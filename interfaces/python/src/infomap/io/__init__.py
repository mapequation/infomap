"""Public I/O namespace for Infomap.

Network writers (``writers``), graph-library adapters (``networkx``,
``igraph``, ``scipy``, ``edge_index``) and result export helpers (``export``)
live here; reach them as ``infomap.io.export`` and so on. The graph adapters
keep their optional third-party imports inside functions, so importing
``infomap`` never requires networkx / igraph / scipy / pandas to be installed.
"""
