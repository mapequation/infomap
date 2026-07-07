Integrations
============

Adapters that connect Infomap to other tools: single-cell analysis with Scanpy,
GraphRAG entity tables, and merging distributed trial runs.

Scanpy / AnnData
----------------

.. autofunction:: infomap.tl.infomap

GraphRAG
--------

.. currentmodule:: infomap.tl.graphrag

These names are also re-exported from the ``infomap.tl`` namespace, e.g.
``infomap.tl.read_graphrag``.

.. autofunction:: read_graphrag
.. autofunction:: run_graphrag_communities
.. autofunction:: write_graphrag_communities

.. autoclass:: GraphRAGGraph
   :members:

.. autoclass:: GraphRAGRunResult
   :members:

Distributed trials
------------------

.. currentmodule:: infomap.merge

.. autofunction:: merge_trial_results

.. autoclass:: MergeSummary
   :members:

.. autoexception:: MergeError
