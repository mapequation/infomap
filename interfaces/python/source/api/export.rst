Export helpers
==============

In-memory converters
--------------------

.. currentmodule:: infomap

:func:`to_networkx` and :func:`to_igraph` return a copy of the graph annotated
with the partition (``infomap_module``, ``infomap_path``, per-level ids, and
``flow``), ready for the graph library's own writer.

.. autofunction:: to_networkx
.. autofunction:: to_igraph

File writers
------------

.. currentmodule:: infomap.export

.. autofunction:: annotate_networkx_graph
.. autofunction:: annotate_igraph_graph
.. autofunction:: write_graphml
.. autofunction:: write_gexf
