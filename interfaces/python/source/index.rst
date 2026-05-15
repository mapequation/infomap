Infomap Python API
==================

Infomap is a network clustering algorithm based on the `Map equation`_. This
site documents the Python package: installation, a quick start, usage patterns
for NetworkX, igraph and multilayer graphs, and the full API reference.

.. _Map equation: https://www.mapequation.org/publications.html#Rosvall-Axelsson-Bergstrom-2009-Map-equation

Quick start
-----------

.. code-block:: bash

    pip install infomap

.. code-block:: python

    import networkx as nx
    import infomap

    graph = nx.karate_club_graph()
    communities = infomap.find_communities(graph, seed=123, num_trials=20)

    print(communities)

Continue to :doc:`installation` for optional integrations and shell completion,
or jump to :doc:`quickstart` for the first end-to-end example.

Where to go next
----------------

- :doc:`installation` — install the package, optional integrations, and the
  ``infomap`` CLI entry point.
- :doc:`quickstart` — the shortest path from ``pip install`` to a partition.
- :doc:`usage` — NetworkX, igraph, state and multilayer networks, reusable
  options, and a migration guide for users of other community-detection
  packages.
- :doc:`export` — write Infomap module assignments to GraphML and GEXF for
  visualization tools.
- :doc:`scanpy` — run Infomap on AnnData graphs in Scanpy-style single-cell
  workflows.
- :doc:`api/index` — full API reference, split into top-level functions, the
  :class:`infomap.Infomap` class, options, and tree iterators.

External resources
------------------

- `Infomap user guide <https://www.mapequation.org/infomap/>`_ — algorithm,
  options, and file formats.
- `PyPI project <https://pypi.org/project/infomap/>`_
- `GitHub repository <https://github.com/mapequation/infomap>`_ —
  source, issues, and discussions.
- `CHANGELOG <https://github.com/mapequation/infomap/blob/master/CHANGELOG.md>`_

.. toctree::
   :hidden:
   :maxdepth: 2

   installation
   quickstart
   usage
   export
   scanpy
   api/index
