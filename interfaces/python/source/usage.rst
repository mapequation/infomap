Usage
=====

This page covers the most common usage patterns: reusable options, the
NetworkX- and igraph-style entry points, state and multilayer networks, and a
short migration guide for users coming from other community-detection
packages.

Reusable options
----------------

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
---------------

:func:`infomap.find_communities` provides a NetworkX-style entry point that
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
and node attributes are stored on the graph. See the NetworkX documentation
for `community algorithms`_, `Louvain communities`_, and `setting node
attributes`_.

.. _community algorithms: https://networkx.org/documentation/stable/reference/algorithms/community.html
.. _Louvain communities: https://networkx.org/documentation/stable/reference/algorithms/generated/networkx.algorithms.community.louvain.louvain_communities.html
.. _setting node attributes: https://networkx.org/documentation/stable/reference/generated/networkx.classes.function.set_node_attributes.html

:meth:`infomap.Infomap.add_networkx_graph` remains available when you need
direct control over the :class:`infomap.Infomap` instance. It accepts
standard, state, and multilayer NetworkX graphs. Non-integer node labels are
mapped to stable internal ids in first-seen order and returned as a mapping:

.. code-block:: python

    import networkx as nx
    from infomap import Infomap

    graph = nx.Graph([("a", "b"), ("a", "c")])
    im = Infomap(silent=True)
    mapping = im.add_networkx_graph(graph)
    im.run()

State and multilayer networks
-----------------------------

State networks are inferred from a ``phys_id`` node attribute, and multilayer
networks additionally use ``layer_id``. :func:`infomap.find_communities`
returns a partition of the graph's own nodes, which are state nodes for these
inputs:

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
-------------

:func:`infomap.find_igraph_communities` provides an igraph-native entry point
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

:meth:`infomap.Infomap.add_igraph_graph` remains available when you need
direct control over the :class:`infomap.Infomap` instance. It accepts
standard, state, and multilayer igraph graphs. The igraph vertex index is
used as the state/internal id, while non-numeric ``phys_id`` labels in state
and multilayer graphs are mapped to stable internal physical ids:

.. code-block:: python

    graph = ig.Graph(edges=[(0, 1), (0, 2)])
    graph.vs["name"] = ["state-a", "state-b", "state-a-next"]
    graph.vs["phys_id"] = ["a", "b", "a"]
    graph.vs["layer_id"] = [1, 1, 2]

    im = infomap.Infomap(silent=True)
    im.add_igraph_graph(graph)
    im.run()

python-igraph also includes ``Graph.community_infomap()``. The ``infomap``
package API is useful when you want the current Infomap package
implementation, Infomap-specific options, or state/multilayer import, while
still receiving an igraph-native ``VertexClustering`` result. See the
python-igraph documentation for `Graph.community_infomap`_ and
`VertexClustering`_.

.. _Graph.community_infomap: https://python.igraph.org/en/main/api/igraph.Graph.html
.. _VertexClustering: https://python.igraph.org/en/main/api/igraph.VertexClustering.html

Migration guide for NetworkX and igraph users
---------------------------------------------

NetworkX community functions usually return a partition such as ``list[set]``.
Use :func:`infomap.find_communities` for that style. Pass ``weight`` as the
edge attribute name, pass ``None`` for unweighted edges, and use
``module_attribute`` or ``flow_attribute`` when you want values written back
to graph nodes. Directed ``DiGraph`` inputs are detected automatically unless
you already set an Infomap flow model.

python-igraph community functions usually return a ``VertexClustering``. Use
:func:`infomap.find_igraph_communities` for that style. Pass ``edge_weights``
as an igraph edge attribute name, an explicit sequence, or ``None`` for
unweighted edges. Directed igraph graphs are detected automatically, and
optional ``module_attribute`` and ``flow_attribute`` arguments write results
back to vertices.

Use :meth:`infomap.Infomap.add_networkx_graph` or
:meth:`infomap.Infomap.add_igraph_graph` instead of the one-shot helpers when
you need state networks, multilayer networks, explicit result iteration,
dataframe export, or several runs with different Infomap options.

See the :doc:`api/index` reference for the full surface.
