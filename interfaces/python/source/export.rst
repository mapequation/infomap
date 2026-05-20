Export GraphML and GEXF
=======================

Infomap can write module assignments back to graph objects before exporting
them to common visualization formats. This keeps the graph topology and the
Infomap result in one file, so tools such as Gephi and Cytoscape can color or
filter nodes by community without a separate join step.

NetworkX GraphML and GEXF
-------------------------

For reusable :class:`infomap.Infomap` workflows, keep the mapping returned by
:meth:`infomap.Infomap.add_networkx_graph`. The export helpers use it to align
Infomap's internal integer ids with the original NetworkX node labels:

.. code-block:: python

    import networkx as nx
    from infomap import Infomap
    from infomap.export import write_gexf, write_graphml

    graph = nx.karate_club_graph()

    im = Infomap(silent=True, num_trials=20, seed=123)
    mapping = im.add_networkx_graph(graph)
    im.run()

    write_graphml(graph, im, "karate_infomap.graphml", node_mapping=mapping)
    write_gexf(graph, im, "karate_infomap.gexf", node_mapping=mapping)

The helpers copy the graph by default, annotate the copy, and pass it to
NetworkX's native writers. Use ``copy=False`` when you explicitly want the
attributes written to the graph object you pass in.

If you only need top-level modules, the NetworkX-style entry point can write
the node attribute directly. Then call NetworkX's writer yourself:

.. code-block:: python

    import networkx as nx
    import infomap

    graph = nx.karate_club_graph()
    infomap.find_communities(
        graph,
        module_attribute="infomap_module",
        seed=123,
        num_trials=20,
    )

    nx.write_graphml(graph, "karate_infomap.graphml")
    nx.write_gexf(graph, "karate_infomap.gexf")

igraph GraphML
--------------

For python-igraph, use :meth:`infomap.Infomap.add_igraph_graph` and
:func:`infomap.export.write_graphml`. The helper annotates vertex attributes
and uses python-igraph's native GraphML writer:

.. code-block:: python

    import igraph as ig
    from infomap import Infomap
    from infomap.export import write_graphml

    graph = ig.Graph.Famous("Zachary")

    im = Infomap(silent=True, num_trials=20, seed=123)
    im.add_igraph_graph(graph)
    im.run()

    write_graphml(graph, im, "karate_infomap.graphml")

GEXF export is available for NetworkX graphs only. python-igraph does not
provide a native GEXF writer, and Infomap does not convert igraph graphs
through NetworkX implicitly because that adds overhead and can change ids or
attributes in surprising ways.

Written attributes
------------------

By default, exported nodes receive string-valued attributes for broad
compatibility with visualization tools:

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Attribute
     - Meaning
   * - ``infomap_module``
     - Top-level module assignment.
   * - ``infomap_path``
     - Colon-separated hierarchical module path, such as ``"3:12"``.
   * - ``infomap_level_1``, ``infomap_level_2``, ...
     - One attribute per hierarchy level.

Set ``module_attribute`` or ``path_attribute`` to change the main attribute
names. Set ``include_hierarchy=False`` when you only want the top-level module
attribute.

Visualization workflow
----------------------

Open the exported GraphML or GEXF file in Gephi or Cytoscape and style nodes
by the ``infomap_module`` attribute. For hierarchical runs, use
``infomap_path`` to identify full paths or ``infomap_level_N`` attributes to
color one hierarchy level at a time.

The native ``infomap`` CLI does not read or write GraphML/GEXF in this
release. Use the Python workflow above when you need these interchange
formats.
