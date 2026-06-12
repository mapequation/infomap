Quick start
===========

This page shows the smallest useful Python API calls. For a worked notebook
with plotting, dataframe inspection, and export, see
:doc:`examples/quickstart`.

Run on a graph
--------------

Use :func:`infomap.find_communities` when you already have a NetworkX-style
graph and only need top-level assignments:

.. code-block:: python

    import networkx as nx
    import infomap

    graph = nx.karate_club_graph()
    communities = infomap.find_communities(
        graph,
        seed=123,
        num_trials=20,
    )

    print(communities)

Pass ``weight=None`` for unweighted edges. Pass ``module_attribute`` when you
want the assignments written back to graph nodes.

Use the Infomap class
---------------------

Use :class:`infomap.Infomap` when you need Infomap-specific options, result
inspection, hierarchy access, or repeated runs:

.. code-block:: python

    from infomap import Infomap

    im = Infomap(silent=True, seed=123, num_trials=20)
    im.add_link(0, 1)
    im.add_link(1, 2)
    im.add_link(2, 0)
    im.run()

    print(im.codelength)
    print(im.get_modules())

For tabular output:

.. code-block:: python

    results = im.to_dataframe(
        columns=["node_id", "module_id", "flow"],
        index="node_id",
    )

Record ``seed``, ``num_trials``, input provenance, and non-default options for
reproducible reports.

Explore interactively
---------------------

Start a Python shell with Infomap preloaded:

.. code-block:: bash

    infomap-shell

This opens a shell with ``im = Infomap()`` ready to use. Run
``options()`` to inspect options and ``summary()`` to inspect the current
network or result.

Next steps
----------

- :doc:`usage` covers NetworkX, igraph, SciPy sparse matrices, edge-index,
  state networks, and multilayer networks.
- :doc:`examples/index` contains executed workflow notebooks.
- :doc:`api/index` is the full API reference.
