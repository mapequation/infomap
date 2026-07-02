API reference
=============

.. currentmodule:: infomap

The main entry point is :func:`run`, which returns an immutable
:class:`Result`. Build inputs with :class:`Network`, capture configuration with
:class:`Options`, and convert results to graph objects with :func:`to_networkx`
and :func:`to_igraph`. The stateful :class:`Infomap` class composes these for
incremental, repeated-run workflows.

.. list-table::
   :widths: 28 72
   :header-rows: 0

   * - :doc:`functions`
     - :func:`run`, the graph-package helpers :func:`find_communities` and
       :func:`find_igraph_communities`, and information-theoretic primitives.
   * - :doc:`network`
     - :class:`Network`, the first-class input builder.
   * - :doc:`result`
     - :class:`Result`, the immutable run snapshot, and :class:`TreeNode`.
   * - :doc:`options`
     - :class:`Options` for reusable configuration and :class:`MultilayerNode`
       for multilayer link inputs.
   * - :doc:`export`
     - :func:`to_networkx` / :func:`to_igraph` and the GraphML / GEXF writers.
   * - :doc:`infomap`
     - The stateful :class:`Infomap` class with all configuration and run
       methods; new code usually wants :func:`run` instead.
   * - :doc:`iterators`
     - :class:`InfoNode` and the tree-walking iterators returned by
       :meth:`Infomap.tree`, :meth:`Infomap.leaf_modules`, and friends.
   * - :doc:`integrations`
     - Scanpy (:func:`infomap.tl.infomap`), GraphRAG, and the distributed-trial
       merge tool.
   * - :doc:`datasets`
     - The bundled example networks, one loader per network
       (:func:`infomap.datasets.two_triangles` and friends).

.. toctree::
   :hidden:
   :maxdepth: 1

   functions
   network
   result
   options
   export
   infomap
   iterators
   integrations
   datasets
