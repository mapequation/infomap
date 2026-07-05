Top-level functions
===================

.. currentmodule:: infomap

The functional API and its building blocks. Most work goes through :func:`run`;
the rest are graph-package convenience wrappers and standalone primitives.

Running Infomap
---------------

.. autofunction:: run

Graph-package entry points
--------------------------

.. autofunction:: find_communities
.. autofunction:: find_igraph_communities

Information-theoretic primitives
--------------------------------

The building blocks behind the map equation, exposed for standalone use.

.. autofunction:: entropy
.. autofunction:: perplexity
.. autofunction:: plogp

Build and CLI entry points
--------------------------

.. autofunction:: build_info
.. autofunction:: main
