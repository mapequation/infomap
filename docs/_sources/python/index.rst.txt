:orphan:

Infomap Python API
==================

Infomap is a network clustering algorithm based on the `Map equation`_.

.. _Map equation: https://www.mapequation.org/publications.html#Rosvall-Axelsson-Bergstrom-2009-Map-equation

API Reference
-------------
.. toctree::
   :maxdepth: 2

   infomap

Installation
------------
Infomap is available on `PyPI`_. Install it with::

    pip install infomap

Upgrade an existing installation with::

    pip install --upgrade infomap

.. _PyPI: https://pypi.org/project/infomap/

Usage
-----

Command line
^^^^^^^^^^^^

Installing the Python package also provides an ``infomap`` executable.
To verify that the CLI is available, run::

    infomap -v

Command line usage is as follows::

    infomap [options] network_data destination

For a list of available options, run::

    infomap --help


Python package
^^^^^^^^^^^^^^

Import the package with::

    import infomap

A simple example:

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

Reusable options
""""""""""""""""

Use :class:`infomap.InfomapOptions` when you want to reuse the same
configuration across multiple runs:

.. code-block:: python

    from infomap import Infomap, InfomapOptions

    options = InfomapOptions(
        two_level=True,
        directed=True,
        silent=True,
        num_trials=20,
    )

    im = Infomap.from_options(options)
    im.add_link(0, 1)
    im.add_link(1, 2)
    im.run()

    # Reuse the same settings on another instance or another run
    im2 = Infomap(silent=True)
    im2.add_link(2, 3)
    im2.run_with_options(options)

NetworkX graphs
"""""""""""""""

``infomap.find_communities()`` provides a NetworkX-style entry point that
returns communities as a ``list[set]`` partition using the original graph node
labels. It is duck-typed and does not import NetworkX, so NetworkX remains an
optional user-installed package:

.. code-block:: python

    import networkx as nx
    import infomap

    graph = nx.Graph([("a", "b"), ("a", "c")])
    communities = infomap.find_communities(graph, weight="weight")

Directed graphs are detected from the graph object:

.. code-block:: python

    graph = nx.DiGraph([("a", "b"), ("b", "c")])
    communities = infomap.find_communities(graph)

To annotate a graph for drawing or downstream NetworkX workflows, pass a node
attribute name:

.. code-block:: python

    infomap.find_communities(graph, module_attribute="community")

This follows NetworkX community conventions where algorithms return partitions
and node attributes are stored on the graph. See the NetworkX documentation for
`community algorithms <https://networkx.org/documentation/stable/reference/algorithms/community.html>`__,
`Louvain communities <https://networkx.org/documentation/stable/reference/algorithms/generated/networkx.algorithms.community.louvain.louvain_communities.html>`__,
and
`setting node attributes <https://networkx.org/documentation/stable/reference/generated/networkx.classes.function.set_node_attributes.html>`__.

``Infomap.add_networkx_graph()`` remains available when you need direct control
over the ``Infomap`` instance. It accepts standard, state, and multilayer
NetworkX graphs. Non-integer node labels are mapped to stable internal ids in
first-seen order and returned as a mapping:

.. code-block:: python

    import networkx as nx
    from infomap import Infomap

    graph = nx.Graph([("a", "b"), ("a", "c")])
    im = Infomap(silent=True)
    mapping = im.add_networkx_graph(graph)
    im.run()

State and multilayer networks
"""""""""""""""""""""""""""""

State networks are inferred from a ``phys_id`` node attribute, and multilayer
networks additionally use ``layer_id``. ``find_communities()`` returns a
partition of the graph's own nodes, which are state nodes for these inputs:

.. code-block:: python

    import networkx as nx
    from infomap import Infomap

    graph = nx.Graph()
    graph.add_node("state-a", phys_id="a", layer_id=1)
    graph.add_node("state-b", phys_id="b", layer_id=1)
    graph.add_node("state-a-next", phys_id="a", layer_id=2)
    graph.add_edge("state-a", "state-b")
    graph.add_edge("state-a", "state-a-next")

    im = Infomap(silent=True)
    im.add_networkx_graph(graph)
    im.run()

For the NetworkX-style wrapper:

.. code-block:: python

    communities = infomap.find_communities(graph)

igraph graphs
"""""""""""""

``infomap.find_igraph_communities()`` provides an igraph-native entry point
without making igraph a required dependency. It imports igraph only when the
igraph-specific API is used and returns an ``igraph.VertexClustering`` with a
``codelength`` attribute, matching python-igraph community conventions:

.. code-block:: python

    import igraph as ig
    import infomap

    graph = ig.Graph.TupleList([("a", "b"), ("a", "c")], directed=False)
    communities = infomap.find_igraph_communities(graph, trials=10)
    communities.codelength

Use ``edge_weights`` the same way as python-igraph's community functions:

.. code-block:: python

    graph.es["weight"] = [2.0, 1.0]
    communities = infomap.find_igraph_communities(graph, edge_weights="weight")

Directed igraph graphs are detected from the graph object:

.. code-block:: python

    graph = ig.Graph.TupleList([("a", "b"), ("b", "c")], directed=True)
    communities = infomap.find_igraph_communities(graph)

To annotate an igraph graph for plotting or downstream igraph workflows, pass
vertex attribute names:

.. code-block:: python

    infomap.find_igraph_communities(
        graph,
        module_attribute="community",
        flow_attribute="flow",
    )

``Infomap.add_igraph_graph()`` remains available when you need direct control
over the ``Infomap`` instance. It accepts standard, state, and multilayer
igraph graphs. The igraph vertex index is used as the state/internal id, while
non-numeric ``phys_id`` labels in state and multilayer graphs are mapped to
stable internal physical ids:

.. code-block:: python

    graph = ig.Graph(edges=[(0, 1), (0, 2)])
    graph.vs["name"] = ["state-a", "state-b", "state-a-next"]
    graph.vs["phys_id"] = ["a", "b", "a"]
    graph.vs["layer_id"] = [1, 1, 2]

    im = infomap.Infomap(silent=True)
    im.add_igraph_graph(graph)
    im.run()

python-igraph also includes ``Graph.community_infomap()``. The
``infomap`` package API is useful when you want the current Infomap package
implementation, Infomap-specific options, or state/multilayer import, while
still receiving an igraph-native ``VertexClustering`` result. See the
python-igraph documentation for
`Graph.community_infomap <https://python.igraph.org/en/main/api/igraph.Graph.html>`__
and
`VertexClustering <https://python.igraph.org/en/main/api/igraph.VertexClustering.html>`__.

Please read the :doc:`/python/infomap` reference to learn more.

.. * :ref:`genindex`
