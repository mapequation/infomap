Quick start
===========

This page shows the smallest useful calls: run Infomap on a graph and read the
result. For worked examples with plotting, dataframe inspection, and export,
see :doc:`working-with-infomap/index`.

Run on a graph
--------------

:func:`infomap.run` accepts a network and returns an immutable
:class:`~infomap.Result`:

.. code-block:: python

    import networkx as nx
    import infomap

    graph = nx.karate_club_graph()
    result = infomap.run(graph, seed=123, num_trials=20, silent=True)

    print(result.num_top_modules)   # 3
    print(result.codelength)        # 4.0874 bits per step
    print(result.modules())         # {node_id: module_id}

The same call accepts a NetworkX or igraph graph, a SciPy sparse matrix, a
``(2, E)`` edge index, a network file path, or an iterable of ``(u, v[, w])``
links. Keyword arguments configure the engine: ``seed``, ``num_trials``,
``directed`` for a directed flow model, and so on.

Build a network step by step
-----------------------------

When you assemble a network incrementally rather than from an existing graph,
use :class:`~infomap.Network`. Its ``add_*`` verbs mirror the link and node
input of the algorithm, and :func:`infomap.run` takes the built network
directly:

.. code-block:: python

    from infomap import Network, run

    net = Network()
    net.add_link(0, 1)
    net.add_link(1, 2)
    net.add_link(2, 0)
    result = run(net, silent=True)

    print(result.codelength)

For non-default loading from a graph library, the ``from_*`` constructors take
the adapter options that :func:`infomap.run` does not, such as a different edge
weight attribute or explicit directedness:

.. code-block:: python

    from infomap import Network, run

    net = Network.from_networkx(graph, weight="capacity")
    result = run(net, num_trials=20, silent=True)

Reusable configuration
----------------------

Capture a configuration once as an :class:`~infomap.Options` and reuse it across
runs. Keyword arguments on :func:`infomap.run` take precedence over the bound
options:

.. code-block:: python

    from infomap import Options, run

    options = Options(num_trials=20, seed=123, silent=True)
    result = run(graph, options=options)

Read the result
---------------

A :class:`~infomap.Result` reports scalar metrics as properties and collections
as methods with defaults:

.. code-block:: python

    result.codelength            # float, bits per step
    result.num_top_modules       # int
    result.num_levels            # depth of the hierarchy

    result.modules()             # {node_id: module_id} at the top level
    result.modules(depth=2)      # one level deeper
    for node in result.nodes():  # per-node views
        node.node_id, node.module_id, node.flow

    result.to_dataframe(["node_id", "module_id", "flow"])

Explore interactively
---------------------

The ``infomap-shell`` command opens a Python shell with Infomap imported and
ready; see :doc:`installation` for details.

The stateful Infomap class
--------------------------

Existing code that builds an :class:`infomap.Infomap` instance, calls ``add_*``,
and then ``run()`` keeps working unchanged: ``run()`` now returns the same
:class:`~infomap.Result`. New code should prefer :func:`infomap.run` for
one-shot use and :class:`~infomap.Network` for incremental construction.

Next steps
----------

- :doc:`concepts/index` explains *why* Infomap finds the communities it does:
  flow, the map equation, and hierarchy.
- :doc:`working-with-infomap/index` covers every input format, the options worth
  tuning, and how to read, visualise, and export results.
- :doc:`flow-models/index` covers richer networks: memory, multilayer, temporal,
  metadata, and bipartite.
