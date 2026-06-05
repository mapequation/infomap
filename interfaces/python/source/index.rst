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
or jump to :doc:`quickstart` for the smallest Python API examples.

For Jupyter, start with the flagship
`quickstart notebook <https://github.com/mapequation/infomap/blob/master/examples/notebooks/quickstart.ipynb>`_.
It shows the notebook-native Infomap result summary, dataframe inspection, and
a copyable static network partition helper.

Choose a path
-------------

- New to Infomap in Python? Start with :doc:`quickstart`.
- Working in NetworkX, igraph, sparse matrices, or multilayer graphs? Use
  :doc:`usage`.
- Working with AnnData or Scanpy? Use :doc:`scanpy`.
- Need GraphML or GEXF files? Use :doc:`export`.
- Learning from executable workflows? Use :doc:`examples/index`.
- Running many native trials on a cluster? Use
  :doc:`examples/run-infomap-on-hpc`.
- Need exact signatures and return types? Use :doc:`api/index`.

Core pages
----------

- :doc:`installation` — install the package, optional integrations, and the
  ``infomap`` CLI entry point.
- :doc:`quickstart` — small Python API examples for first runs and result
  access.
- :doc:`usage` — API patterns for common graph containers and result access.
- :doc:`examples/index` — executed notebooks for practical Infomap workflows
  and survey companion material.

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
   examples/index
   usage
   export
   scanpy
   api/index
