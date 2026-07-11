Quick start
===========

This page shows the smallest useful calls: run Infomap on a graph and read the
result. For worked examples with plotting, dataframe inspection, and export,
see :doc:`working-with-infomap/index`.

Cheat sheet
-----------

The whole surface at a glance (each line is expanded below):

.. code-block:: python

    import infomap
    from infomap import Options

    # input: a networkx/igraph graph, SciPy sparse matrix, (2, E) edge index,
    # file path, iterable of (u, v[, w]) links, or a prebuilt Network
    result = infomap.run(graph, seed=123, num_trials=20)

    result.codelength              # scalar metrics are PROPERTIES (no parentheses)
    result.num_top_modules
    result.modules()               # collections are METHODS -> {node_id: module_id}
    result.to_dataframe()          # per-node table: node_id, module_id, flow, path, name
    result.write_clu("out.clu")    # write .clu/.tree/.ftree from the Result

    # advanced engine options ride Options, not bare keywords:
    infomap.run(graph, options=Options(regularized=True, flow_model="directed"))

Common pitfalls, each answered in the :doc:`FAQ <faq>`:

- **Scalars are properties, collections are methods.** ``result.codelength``
  (no ``()``) but ``result.modules()`` (with ``()``); the exceptions are
  ``result.names`` / ``state_names`` / ``codelengths`` (properties).
- **Only five options are direct keywords** on :func:`infomap.run`: ``seed``,
  ``num_trials``, ``two_level``, ``directed``, ``markov_time``. Carry every other
  engine option via :class:`~infomap.Options` (see :doc:`/api/deprecations`).
  (``options`` and ``initial_partition`` are structural arguments to ``run``, not
  engine options, so they are keywords too but are not part of "the five".)
- **Output flags are inert on the library surface.** ``Options(tree=True)``
  writes nothing; write from the ``Result`` (``write_tree`` / ``write_clu``) or
  the ``Network`` (``write_pajek``).
- **A matrix or edge index is not a graph.** ``infomap.run(A, directed=True)``
  raises; build it with ``Network.from_scipy_sparse_matrix(A, directed=True)``.
  (A *dense* adjacency matrix is not a supported input either -- convert it with
  ``scipy.sparse.csr_matrix(A)``.)
- **Result keys are internal ids, not graph labels.** For a graph with
  non-integer labels, ``result.modules()`` keys are Infomap's internal ids;
  recover labels with ``result.names.get(nid, nid)``, the ``name`` column of
  ``to_dataframe()``, or ``infomap.find_communities`` (keyed by labels).

Run on a graph
--------------

:func:`infomap.run` accepts a network and returns an immutable
:class:`~infomap.Result`:

.. code-block:: python

    import networkx as nx
    import infomap

    graph = nx.karate_club_graph()
    result = infomap.run(graph, seed=123, num_trials=20)

    print(result.num_top_modules)   # e.g. 3
    print(result.codelength)        # e.g. ~4.09 bits per step
    print(result.modules())         # {node_id: module_id}

The same call accepts a NetworkX or igraph graph, a SciPy sparse matrix, a
``(2, E)`` edge index, a network file path, or an iterable of ``(u, v[, w])``
links. Five common options are direct keyword arguments — ``seed``,
``num_trials``, ``two_level``, ``directed`` (a directed flow model), and
``markov_time``; carry every other engine option (``regularized``,
``flow_model``, ``teleportation_probability``, …) through
:class:`~infomap.Options`, as shown under `Reusable configuration`_ below.

Without any graph library installed, the bundled example networks in
:mod:`infomap.datasets` work directly:

.. code-block:: python

    result = infomap.run(infomap.datasets.two_triangles())

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
    result = run(net)

    print(result.codelength)

The ``from_*`` constructors take the adapter options that :func:`infomap.run`
does not, such as a different edge-weight attribute or explicit directedness:

.. code-block:: python

    from infomap import Network, run

    net = Network.from_networkx(graph, weight="capacity")
    result = run(net, num_trials=20)

Reusable configuration
----------------------

Capture a configuration once as an :class:`~infomap.Options` and reuse it across
runs. Keyword arguments on :func:`infomap.run` take precedence over the bound
options:

.. code-block:: python

    from infomap import Options, run

    options = Options(num_trials=20, seed=123)
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

``to_dataframe`` and ``to_series`` need pandas, an optional dependency: install
it with ``pip install "infomap[pandas]"`` (a missing install raises an
``ImportError`` that says exactly this). ``result.modules()`` and
``result.nodes()`` need no extra dependency.

Explore interactively
---------------------

The ``infomap-shell`` command opens a Python shell with Infomap imported and
ready; see :doc:`installation` for details.

The stateful Infomap class
--------------------------

Existing code that builds an :class:`infomap.Infomap` instance, calls ``add_*``,
and then ``run()`` keeps working unchanged: ``run()`` now returns the same
:class:`~infomap.Result`. For when to choose each entry point and the full
migration guide, see :doc:`the-infomap-class`.

Next steps
----------

- :doc:`concepts/index` explains *why* Infomap finds the communities it does:
  flow, the map equation, and hierarchy.
- :doc:`working-with-infomap/index` covers every input format, the options worth
  tuning, and how to read, visualise, and export results.
- :doc:`flow-models/index` covers richer networks: memory, multilayer, temporal,
  metadata, and bipartite.
