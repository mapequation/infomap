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

Engine log
----------

The engine log becomes Python log records on the ``"infomap"`` logger when
that logger has handlers; see the routing rules in
:doc:`/working-with-infomap/running-and-options`.

.. autofunction:: enable_log
.. autofunction:: disable_log

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
