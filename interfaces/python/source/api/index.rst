API reference
=============

.. currentmodule:: infomap

The :mod:`infomap` package exposes a small set of top-level helpers, a
configuration dataclass, the main :class:`Infomap` class, and a family of
tree iterators returned when walking the resulting partition.

.. list-table::
   :widths: 30 70
   :header-rows: 0

   * - :doc:`functions`
     - Top-level helpers: :func:`find_communities`,
       :func:`find_igraph_communities`, and information-theoretic primitives.
   * - :doc:`export`
     - GraphML and GEXF export helpers for graph-package workflows.
   * - :doc:`infomap`
     - The main :class:`Infomap` class with all configuration and run methods.
   * - :doc:`options`
     - :class:`InfomapOptions` for reusable configuration and
       :class:`MultilayerNode` for multilayer link inputs.
   * - :doc:`iterators`
     - :class:`InfoNode` and the tree-walking iterators returned by
       :attr:`Infomap.tree`, :attr:`Infomap.leaf_modules`, and friends.

.. toctree::
   :hidden:
   :maxdepth: 1

   functions
   export
   infomap
   options
   iterators
