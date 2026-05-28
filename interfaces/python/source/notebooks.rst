Tutorial notebooks
==================

Infomap includes a set of Jupyter notebooks for readers who want a more
guided, exploratory introduction to the map equation and Infomap workflows.
The notebooks were created as companion material for the paper
`Community Detection with the Map Equation and Infomap: Theory and Applications`_.

Start with the flagship quickstart notebook:
`quickstart.ipynb <https://github.com/mapequation/infomap/blob/master/examples/notebooks/quickstart.ipynb>`_.
It shows the notebook-native Infomap result summary, a copyable static network
partition helper, dataframe inspection, and export paths for further analysis.

They are useful when you want to:

- connect the mathematical ideas in the map equation to executable examples;
- inspect small networks, partitions, flows, and codelengths step by step;
- adapt examples for research scripts, teaching material, or data-science
  analysis notebooks.

.. _`Community Detection with the Map Equation and Infomap: Theory and Applications`: https://doi.org/10.1145/3779648

What's included
---------------

The notebooks include:

- a flagship Python quickstart for Jupyter-native Infomap workflows;
- comparison tutorials for NetworkX, igraph, and Scanpy-style workflows;
- companion material for the survey article.

The numbered survey notebooks cover:

- the two-level map equation;
- the two-level search phase and solution landscapes;
- memory, multilayer, temporal, and multi-body network models;
- networks with metadata, bipartite structure, and incomplete data;
- map equation centrality, similarity, bioregions, and model selection with
  correlational data.

The notebook source is available in
`examples/notebooks <https://github.com/mapequation/infomap/tree/master/examples/notebooks>`_.

Comparison tutorials
--------------------

The unnumbered workflow notebooks show how to run Infomap next to common
community-detection tools without following the article section numbering:

- `compare-infomap-louvain-leiden-networkx.ipynb <https://github.com/mapequation/infomap/blob/master/examples/notebooks/compare-infomap-louvain-leiden-networkx.ipynb>`_
  compares :func:`infomap.find_communities` with NetworkX Louvain and Leiden
  when Leiden is available.
- `compare-infomap-louvain-leiden-igraph.ipynb <https://github.com/mapequation/infomap/blob/master/examples/notebooks/compare-infomap-louvain-leiden-igraph.ipynb>`_
  compares :func:`infomap.find_igraph_communities` with python-igraph
  Louvain and Leiden.
- `compare-infomap-scanpy-workflow.ipynb <https://github.com/mapequation/infomap/blob/master/examples/notebooks/compare-infomap-scanpy-workflow.ipynb>`_
  shows :func:`infomap.tl.infomap` in an AnnData and Scanpy-style workflow.

Run with Docker
---------------

The easiest way to explore the notebooks is the published notebook image:

.. code-block:: bash

    docker run --rm -p 8888:8888 \
        ghcr.io/mapequation/infomap:notebook \
        start.sh jupyter lab

The image includes Infomap, the tutorial notebooks, and the Python packages
used by the notebooks. It opens in the notebook workspace by default.

If you want to save notebooks or outputs to a host directory, mount it as a
separate workspace path:

.. code-block:: bash

    docker run --rm -p 8888:8888 \
        -v "$(pwd)":/home/jovyan/work/local \
        ghcr.io/mapequation/infomap:notebook \
        start.sh jupyter lab

Run from a source checkout
--------------------------

From an Infomap source checkout, install the notebook dependencies and start
JupyterLab:

.. code-block:: bash

    python -m pip install -e '.[notebooks]'
    cd examples/notebooks
    jupyter lab

Some notebooks call external research code or need additional data-processing
packages. Those notebooks are still included as examples, but may require
extra setup described in the notebook itself.

Related material
----------------

- :doc:`quickstart` for the shortest Python API example.
- :doc:`usage` for reusable Python API patterns.
- `Infomap user guide <https://www.mapequation.org/infomap/>`_ for command
  line options, input formats, and algorithm documentation.
