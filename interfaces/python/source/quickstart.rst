Quick start
===========

Import the package with:

.. code-block:: python

    import infomap

A simple example:

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

For direct control over Infomap-specific options and result access:

.. code-block:: python

    from infomap import Infomap

    # Command line flags can be provided as a string
    im = Infomap("--two-level --directed")

    # Add weight as optional third argument
    im.add_link(0, 1)
    im.add_link(0, 2)
    im.add_link(0, 3)
    im.add_link(1, 0)
    im.add_link(1, 2)
    im.add_link(2, 1)
    im.add_link(2, 0)
    im.add_link(3, 0)
    im.add_link(3, 4)
    im.add_link(3, 5)
    im.add_link(4, 3)
    im.add_link(4, 5)
    im.add_link(5, 4)
    im.add_link(5, 3)

    # Run the search
    im.run()

    print(f"Found {im.num_top_modules} modules with codelength: {im.codelength}")

    print("Result")
    print("\n#node module")
    for node in im.tree:
        if node.is_leaf:
            print(node.node_id, node.module_id)

Recommended researcher workflow
-------------------------------

For notebooks and scripts, start from the graph package you already use,
choose a seed, run multiple trials, then store both the partition and the
settings used to produce it:

.. code-block:: python

    import networkx as nx
    import infomap

    graph = nx.karate_club_graph()
    communities = infomap.find_communities(
        graph,
        weight="weight",
        seed=123,
        num_trials=20,
        module_attribute="module_id",
        flow_attribute="flow",
    )

    print(f"{len(communities)} communities")
    print("Report: Infomap, seed=123, num_trials=20, weight='weight'")

When you need tabular results, use :meth:`infomap.Infomap.to_dataframe` after
running an explicit :class:`infomap.Infomap` instance:

.. code-block:: python

    from infomap import Infomap

    im = Infomap(silent=True, seed=123, num_trials=20)
    im.add_networkx_graph(graph)
    im.run()
    results = im.to_dataframe(
        columns=["node_id", "community", "flow"],
        index="node_id",
        sort=["community", "flow"],
    )

``seed`` initializes the random number generator. ``num_trials`` controls how
many independent optimization trials are run before Infomap keeps the best
solution. For reproducible reports, record the package version, graph input,
edge weight field, ``seed``, ``num_trials``, and non-default Infomap options.
