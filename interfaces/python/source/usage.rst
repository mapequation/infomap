API usage patterns
==================

This page is a reference for Python API patterns: reusable options,
NetworkX- and igraph-style entry points, sparse inputs, state and multilayer
networks, and migration notes for users coming from other
community-detection packages.

For single-cell analysis workflows using AnnData or Scanpy, see
:doc:`scanpy`.

For GraphML and GEXF files with module assignments attached to nodes, see
:doc:`export`.

For end-to-end notebooks, see :doc:`examples/index`. The notebooks own the
worked comparisons against Louvain and Leiden.

Which API should I use?
-----------------------

- Use :func:`infomap.find_communities` when you have a NetworkX-style graph
  and want a partition.
- Use :class:`infomap.Infomap` when you need hierarchy, flows, detailed
  options, output files, or repeated runs.
- Use :func:`infomap.find_igraph_communities` when you want an
  igraph-native ``VertexClustering``.
- Use :func:`infomap.tl.infomap` when your graph is stored in AnnData.
- Use the native ``infomap`` CLI for large file-based runs, batch jobs, and
  HPC workflows.

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

Inspecting state
----------------

``repr(im)`` shows a compact snapshot that is useful in a Python shell,
notebook, debugger, or failed test output:

.. code-block:: python

    from infomap import Infomap

    im = Infomap(silent=True)
    im.add_link(1, 2)
    im
    # Infomap(nodes=2, links=1, physical_nodes=2, state_nodes=0, status='not run')

    im.run()
    im
    # Infomap(nodes=2, links=1, physical_nodes=2, state_nodes=0, status='run', multilayer_network=False, levels=2, top_modules=1, codelength=1.0)

Use :meth:`infomap.Infomap.summary` when you want the full inspection data as a
dictionary:

.. code-block:: python

    im.summary()
    # {
    #     "nodes": 2,
    #     "links": 1,
    #     "physical_nodes": 2,
    #     "state_nodes": 0,
    #     "status": "run",
    #     ...
    # }

NetworkX API semantics
----------------------

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
and node attributes are stored on the graph. For a worked comparison with
Louvain, see :doc:`examples/compare-infomap-louvain-networkx`.

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

SciPy sparse matrices
---------------------

Use :meth:`infomap.Infomap.from_scipy_sparse_matrix` when your graph is
already stored as a SciPy sparse adjacency matrix. CSR, CSC and COO matrices
and sparse arrays are accepted. The adapter imports SciPy lazily, so SciPy is
only required when this API is used.

.. code-block:: python

    import scipy.sparse as sp
    from infomap import Infomap

    adjacency = sp.csr_matrix([
        [0, 1, 0],
        [1, 0, 2],
        [0, 2, 0],
    ])

    im = Infomap.from_scipy_sparse_matrix(adjacency, directed=False, args="--silent")
    im.run()
    modules = im.get_modules()

For unweighted graphs, pass ``weighted=False`` to treat every nonzero matrix
entry as weight ``1.0``:

.. code-block:: python

    im = Infomap.from_scipy_sparse_matrix(adjacency, weighted=False, args="--silent")

For directed graphs, pass ``directed=True``. Then ``A[i, j]`` is interpreted
as a directed edge from row ``i`` to column ``j``:

.. code-block:: python

    directed_adjacency = sp.coo_matrix([
        [0, 1],
        [0, 0],
    ])
    im = Infomap.from_scipy_sparse_matrix(
        directed_adjacency,
        directed=True,
        args="--silent",
    )

Custom node ids can be supplied in matrix row order. The adapter returns a
mapping from Infomap's internal integer ids to those external ids, matching
the pattern used by :meth:`infomap.Infomap.add_networkx_graph`:

.. code-block:: python

    adjacency = sp.coo_matrix([
        [0, 3],
        [3, 0],
    ])
    im = Infomap(silent=True)
    mapping = im.add_scipy_sparse_matrix(adjacency, directed=False, node_ids=["gene_a", "gene_b"])
    im.run()
    modules = {mapping[node_id]: module_id for node_id, module_id in im.get_modules().items()}
    assert set(modules) == {"gene_a", "gene_b"}

For undirected input, symmetric entries are de-duplicated: if both ``A[i, j]``
and ``A[j, i]`` exist, one undirected link is added. If the two entries have
different weights, the larger weight is used. For large graphs, prefer CSR or
COO input to avoid unnecessary conversion work.

edge_index and graph ML workflows
---------------------------------

Use :meth:`infomap.Infomap.from_edge_index` when your graph is stored in the
PyTorch Geometric-style ``edge_index`` format. The input has shape
``(2, num_edges)``, with source node ids in row 0 and target node ids in row
1. NumPy arrays, Python lists and PyTorch tensors are accepted. PyTorch is
optional; tensor inputs are converted with ``detach().cpu().numpy()`` only
when provided. NumPy is required for edge-index conversion; install it with
``python -m pip install numpy`` if it is not already available.

.. code-block:: python

    import numpy as np
    from infomap import Infomap

    edge_index = np.array([
        [0, 1, 1, 2],
        [1, 0, 2, 1],
    ])

    im = Infomap.from_edge_index(edge_index, directed=False, args="--silent")
    im.run()
    modules = im.get_modules()

Pass ``edge_weight`` for weighted graphs:

.. code-block:: python

    edge_weight = np.array([1.0, 1.0, 3.5, 3.5])
    im = Infomap.from_edge_index(
        edge_index,
        edge_weight=edge_weight,
        directed=False,
        args="--silent",
    )

PyTorch tensors can be passed directly when PyTorch is installed:

.. code-block:: python

    import torch
    from infomap import Infomap

    edge_index = torch.tensor([
        [0, 1, 2],
        [1, 2, 0],
    ])
    im = Infomap.from_edge_index(edge_index, directed=True, args="--silent")

PyTorch Geometric ``Data`` objects are not required. Pass their attributes to
the edge-index API:

.. code-block:: python

    im = Infomap.from_edge_index(
        data.edge_index,
        edge_weight=data.edge_weight,
        num_nodes=data.num_nodes,
        args="--silent",
    )

Pass ``num_nodes`` to preserve isolated nodes that do not appear in
``edge_index``. For undirected input, reverse duplicate edges are
de-duplicated; if the two entries have different weights, the larger weight is
used. Custom ``node_ids`` follow the same mapping pattern as
:meth:`infomap.Infomap.add_networkx_graph`:

.. code-block:: python

    im = Infomap(silent=True)
    mapping = im.add_edge_index(edge_index, node_ids=["gene_a", "gene_b", "gene_c"])
    im.run()
    modules = {mapping[node_id]: module_id for node_id, module_id in im.get_modules().items()}

State and multilayer networks
-----------------------------

State networks are inferred from a ``node_id`` node attribute, and multilayer
networks additionally use ``layer_id``. :func:`infomap.find_communities`
returns a partition of the graph's own nodes, which are state nodes for these
inputs:

.. code-block:: python

    import networkx as nx
    from infomap import Infomap

    graph = nx.Graph()
    graph.add_node("state-a", node_id="a", layer_id=1)
    graph.add_node("state-b", node_id="b", layer_id=1)
    graph.add_node("state-a-next", node_id="a", layer_id=2)
    graph.add_edge("state-a", "state-b")
    graph.add_edge("state-a", "state-a-next")

    im = Infomap(silent=True)
    im.add_networkx_graph(graph)
    im.run()

For the NetworkX-style wrapper:

.. code-block:: python

    communities = infomap.find_communities(graph)

Multilayer initial partitions
-----------------------------

For a multilayer network you can supply a pre-clustering keyed by physical
identity, using ``(layer_id, node_id)`` tuples (or
:class:`infomap.MultilayerNode`) instead of the internally generated state ids:

.. code-block:: python

    from infomap import Infomap, MultilayerNode

    im = Infomap(silent=True)
    im.add_multilayer_intra_link(1, 1, 2)
    im.add_multilayer_intra_link(2, 1, 3)
    im.initial_partition = {(1, 1): 0, MultilayerNode(2, 1): 1}
    im.run()

The keys are resolved to state ids when the network is built. On the command
line the equivalent ``--cluster-data`` file is a ``.clu`` with a
``# node_id layer_id module`` header (or a ``.tree`` with a
``# path flow name node_id layer_id`` header); without such a header the file is
read as before (state ids).

igraph API semantics
--------------------

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
used as the state/internal id, while non-numeric ``node_id`` labels in state
and multilayer graphs are mapped to stable internal physical ids:

.. code-block:: python

    graph = ig.Graph(edges=[(0, 1), (0, 2)])
    graph.vs["name"] = ["state-a", "state-b", "state-a-next"]
    graph.vs["node_id"] = ["a", "b", "a"]
    graph.vs["layer_id"] = [1, 1, 2]

    im = infomap.Infomap(silent=True)
    im.add_igraph_graph(graph)
    im.run()

python-igraph also includes ``Graph.community_infomap()``. The ``infomap``
package API is useful when you want the current Infomap package
implementation, Infomap-specific options, or state/multilayer import, while
still receiving an igraph-native ``VertexClustering`` result. For a worked
comparison with Louvain and Leiden, see
:doc:`examples/compare-infomap-louvain-leiden-igraph`.

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
