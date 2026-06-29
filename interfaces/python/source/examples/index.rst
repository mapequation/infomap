Notebook examples
=================

These examples are rendered from the Jupyter notebooks in
``examples/notebooks``. They are published as regular documentation pages so
they can be searched, linked, and read without opening Jupyter.

Choose a notebook
-----------------

- :doc:`quickstart` — first end-to-end Infomap workflow in Python.
- :doc:`options-guide` — what every Infomap option is for, with runnable
  examples and a complete reference table.
- :doc:`compare-infomap-louvain-networkx` — compare Infomap and Louvain with
  NetworkX when your graph already lives in NetworkX.
- :doc:`compare-infomap-louvain-leiden-igraph` — compare Infomap, Louvain, and
  Leiden when you want igraph-native clustering objects.
- :doc:`compare-infomap-scanpy-workflow` — compare Infomap and Leiden in an
  AnnData and Scanpy-style workflow.
- :doc:`run-infomap-on-graphrag-tables` — run Infomap on GraphRAG-style
  entity and relationship tables and compare with Leiden.
- :doc:`run-infomap-on-hpc` — native CLI recipes for scheduler-aware HPC runs
  and Python shard merging.
- :doc:`benchmark-performance` — estimate run time and memory from network size,
  threads, trials, and hierarchy depth, with empirical scaling laws.

Additional tutorial notebooks
-----------------------------

The notebook source tree also includes companion material for
`Community Detection with the Map Equation and Infomap: Theory and Applications`_:

- the two-level map equation;
- the two-level search phase and solution landscapes;
- memory, multilayer, temporal, and multi-body network models;
- networks with metadata, bipartite structure, and incomplete data;
- map equation centrality, similarity, bioregions, and model selection with
  correlational data.

Those source notebooks are available in
`examples/notebooks <https://github.com/mapequation/infomap/tree/master/examples/notebooks>`_.
Some require external research code or additional data-processing packages and
are not rendered in this first docs set.

.. _`Community Detection with the Map Equation and Infomap: Theory and Applications`: https://doi.org/10.1145/3779648

Run locally
-----------

From an Infomap source checkout:

.. code-block:: bash

    python -m pip install -e '.[notebooks]'
    cd examples/notebooks
    jupyter lab

Source notebooks
----------------

- `quickstart.ipynb <https://github.com/mapequation/infomap/blob/master/examples/notebooks/quickstart.ipynb>`_
- `options-guide.ipynb <https://github.com/mapequation/infomap/blob/master/examples/notebooks/options-guide.ipynb>`_
- `compare-infomap-louvain-networkx.ipynb <https://github.com/mapequation/infomap/blob/master/examples/notebooks/compare-infomap-louvain-networkx.ipynb>`_
- `compare-infomap-louvain-leiden-igraph.ipynb <https://github.com/mapequation/infomap/blob/master/examples/notebooks/compare-infomap-louvain-leiden-igraph.ipynb>`_
- `compare-infomap-scanpy-workflow.ipynb <https://github.com/mapequation/infomap/blob/master/examples/notebooks/compare-infomap-scanpy-workflow.ipynb>`_
- `run-infomap-on-graphrag-tables.ipynb <https://github.com/mapequation/infomap/blob/master/examples/notebooks/run-infomap-on-graphrag-tables.ipynb>`_
- `run-infomap-on-hpc.ipynb <https://github.com/mapequation/infomap/blob/master/examples/notebooks/run-infomap-on-hpc.ipynb>`_
- `benchmark-performance.ipynb <https://github.com/mapequation/infomap/blob/master/examples/notebooks/benchmark-performance.ipynb>`_

.. toctree::
   :hidden:
   :maxdepth: 1

   quickstart
   options-guide
   compare-infomap-louvain-networkx
   compare-infomap-louvain-leiden-igraph
   compare-infomap-scanpy-workflow
   run-infomap-on-graphrag-tables
   run-infomap-on-hpc
   benchmark-performance
