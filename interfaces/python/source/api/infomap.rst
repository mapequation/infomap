Infomap class
=============

.. currentmodule:: infomap

:class:`Infomap` is the stateful entry point for the whole workflow: build a
network, run the search, then read the partition back. Its remaining strength is
building a network incrementally, one link at a time:

.. code-block:: python

   im = infomap.Infomap(num_trials=10, seed=42)  # 1. configure
   im.add_link(0, 1)                             # 2. build incrementally
   im.add_link(1, 2)
   result = im.run()                             # 3. search
   result.modules()                              # 4. read

Everything else on this page refines one of those four steps. The tables below
group the members by purpose; the full reference, with signatures and
docstrings, follows underneath.

.. note::

   ``run()`` returns a :class:`Result`; read metrics and modules from it. The
   on-instance result accessors below (``get_modules``, ``modules``,
   ``codelength``, …) are **deprecated and leave in 3.0** -- read the
   equivalently named members off the returned :class:`Result` instead. Mind the
   shape shift: ``im.modules`` is a property, while ``result.modules()`` is a
   method. These accessors emit a silent-by-default ``PendingDeprecationWarning``
   (surface it with ``-W``); see :doc:`/the-infomap-class` for the full
   migration table. For
   one-shot use, prefer :func:`infomap.run`; the stateful class remains the way
   to build incrementally and to write the native output files.

Building a network
------------------

The graph-library and matrix adapters below are deprecated; load graphs with
:func:`infomap.run` or the :class:`Network` ``from_*`` classmethods instead (see
the note above). Only :meth:`~Infomap.read_file`, which reads a native network
file, is current.

.. autosummary::

   ~Infomap.read_file
   ~Infomap.add_networkx_graph
   ~Infomap.add_igraph_graph
   ~Infomap.add_scipy_sparse_matrix
   ~Infomap.from_scipy_sparse_matrix
   ~Infomap.add_edge_index
   ~Infomap.from_edge_index

**Links and nodes directly:**

.. autosummary::

   ~Infomap.add_link
   ~Infomap.add_links
   ~Infomap.add_node
   ~Infomap.add_nodes
   ~Infomap.remove_link
   ~Infomap.remove_links

**Multilayer and state-node inputs:**

.. autosummary::

   ~Infomap.add_state_node
   ~Infomap.add_state_nodes
   ~Infomap.add_multilayer_intra_link
   ~Infomap.add_multilayer_intra_links
   ~Infomap.add_multilayer_inter_link
   ~Infomap.add_multilayer_inter_links
   ~Infomap.add_multilayer_link
   ~Infomap.add_multilayer_links

**Names, metadata, and setup:**

.. autosummary::

   ~Infomap.set_name
   ~Infomap.set_names
   ~Infomap.set_meta_data
   ~Infomap.bipartite_start_id
   ~Infomap.initial_partition

Running Infomap
---------------

.. autosummary::

   ~Infomap.run
   ~Infomap.run_with_options
   ~Infomap.from_options

Reading the partition
---------------------

Except for :attr:`~Infomap.network`, these accessors mirror the
:class:`Result` API and are deprecated (they leave in 3.0). Read them off the
:class:`Result` that ``run()`` returns -- e.g. ``result.modules()`` rather than
``im.get_modules()``.

.. autosummary::

   ~Infomap.get_modules
   ~Infomap.modules
   ~Infomap.get_multilevel_modules
   ~Infomap.multilevel_modules
   ~Infomap.get_nodes
   ~Infomap.nodes
   ~Infomap.physical_nodes
   ~Infomap.leaf_modules
   ~Infomap.get_tree
   ~Infomap.tree
   ~Infomap.physical_tree
   ~Infomap.get_links
   ~Infomap.links
   ~Infomap.flow_links
   ~Infomap.network
   ~Infomap.to_dataframe
   ~Infomap.get_dataframe
   ~Infomap.get_name
   ~Infomap.get_names
   ~Infomap.names
   ~Infomap.state_names
   ~Infomap.get_state_names

Solution metrics
----------------

Most of these metrics are also available (and preferred) on the
:class:`Result` that ``run()`` returns; those on-instance copies are deprecated
and leave in 3.0.

.. autosummary::

   ~Infomap.codelength
   ~Infomap.codelengths
   ~Infomap.index_codelength
   ~Infomap.module_codelength
   ~Infomap.meta_codelength
   ~Infomap.one_level_codelength
   ~Infomap.relative_codelength_savings
   ~Infomap.num_top_modules
   ~Infomap.num_leaf_modules
   ~Infomap.num_non_trivial_top_modules
   ~Infomap.num_levels
   ~Infomap.max_depth
   ~Infomap.num_nodes
   ~Infomap.num_links
   ~Infomap.num_physical_nodes
   ~Infomap.effective_num_top_modules
   ~Infomap.effective_num_leaf_modules
   ~Infomap.get_effective_num_modules
   ~Infomap.entropy_rate
   ~Infomap.meta_entropy
   ~Infomap.have_memory
   ~Infomap.elapsed_time
   ~Infomap.summary

Writing output
--------------

.. autosummary::

   ~Infomap.write
   ~Infomap.write_clu
   ~Infomap.write_tree
   ~Infomap.write_flow_tree
   ~Infomap.write_state_network
   ~Infomap.write_json
   ~Infomap.write_newick
   ~Infomap.write_pajek
   ~Infomap.write_csv

Full reference
--------------

.. autoclass:: Infomap
   :members:
   :inherited-members:
