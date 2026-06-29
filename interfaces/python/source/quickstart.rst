Quick start
===========

This page shows the smallest useful Python API calls. For worked examples with
plotting, dataframe inspection, and export, see
:doc:`working-with-infomap/index`.

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

The ``infomap-shell`` command opens a Python shell with ``im = Infomap()``
preloaded, plus ``options()`` and ``summary()`` helpers; see
:doc:`installation` for details.

Next steps
----------

- :doc:`concepts/index` explains *why* Infomap finds the communities it does:
  flow, the map equation, and hierarchy.
- :doc:`working-with-infomap/index` covers every input format, the options worth
  tuning, and how to read, visualise, and export results.
- :doc:`flow-models/index` covers richer networks: memory, multilayer, temporal,
  metadata, bipartite, and hypergraphs.
