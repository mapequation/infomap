Export helpers
==============

In-memory converters
--------------------

.. currentmodule:: infomap

:func:`to_networkx` and :func:`to_igraph` build a graph from a
:class:`~infomap.Result`, annotated with the partition (``infomap_module``,
``infomap_path``, per-level ids, and ``flow``), ready for the graph library's
own writer.

.. autofunction:: to_networkx
.. autofunction:: to_igraph

File writers
------------

.. currentmodule:: infomap.io.export

These helpers accept a post-run stateful :class:`~infomap.Infomap` instance or
the :class:`~infomap.Result` it returned. They live in the public
``infomap.io`` namespace; ``infomap.export`` is a back-compatibility alias for
:mod:`!infomap.io.export`.

.. autofunction:: annotate_networkx_graph
.. autofunction:: annotate_igraph_graph
.. autofunction:: write_graphml
.. autofunction:: write_gexf
