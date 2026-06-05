Using Infomap with AnnData and Scanpy
=====================================

Use :func:`infomap.tl.infomap` to run Infomap directly on an AnnData
observation graph. The function follows Scanpy ``tl`` conventions: it reads a
sparse graph from ``adata.obsp``, stores categorical cluster labels in
``adata.obs``, and records run metadata in ``adata.uns``.

Infomap does not import Scanpy. It only requires an AnnData-compatible object
with ``obs``, ``obsp``, ``uns`` and ``obs_names``.

This is the reference page for AnnData graph selection and result storage. For
an end-to-end Scanpy-style comparison with Leiden and UMAP plots, see
:doc:`examples/compare-infomap-scanpy-workflow`.

Minimal AnnData example
-----------------------

.. code-block:: python

    import anndata as ad
    import pandas as pd
    import scipy.sparse as sp
    import infomap

    adata = ad.AnnData(obs=pd.DataFrame(index=["cell-a", "cell-b", "cell-c"]))
    adata.obsp["connectivities"] = sp.csr_matrix([
        [0, 1, 0],
        [1, 0, 1],
        [0, 1, 0],
    ])

    infomap.tl.infomap(adata)

    adata.obs["infomap"]
    adata.uns["infomap"]

Using Scanpy neighbor graphs
----------------------------

In Scanpy workflows, nearest-neighbor graphs are commonly created with
``sc.pp.neighbors``. By default, Infomap uses ``adata.obsp["connectivities"]``,
which is the graph used by Scanpy clustering tools.

.. code-block:: python

    import scanpy as sc
    import infomap

    adata = sc.datasets.pbmc3k_processed()
    sc.pp.neighbors(adata)

    infomap.tl.infomap(adata, key_added="infomap")

If the neighbor graph was stored under a custom key, pass ``neighbors_key`` and
Infomap will resolve the graph through
``adata.uns[neighbors_key]["connectivities_key"]``:

.. code-block:: python

    sc.pp.neighbors(adata, key_added="neighbors_pca")
    infomap.tl.infomap(adata, neighbors_key="neighbors_pca")

Graph selection
---------------

Use ``obsp`` to select a graph directly from ``adata.obsp``:

.. code-block:: python

    infomap.tl.infomap(adata, obsp="connectivities")

Use ``adjacency`` when you already have a sparse matrix object:

.. code-block:: python

    infomap.tl.infomap(adata, adjacency=adata.obsp["connectivities"])

For single-cell clustering, ``connectivities`` is usually the right default.
``distances`` stores neighbor distances rather than graph affinities, so it is
not used unless you explicitly pass ``obsp="distances"``.

Directed and weighted graphs
----------------------------

By default, Infomap treats the graph as undirected and weighted. For a directed
observation graph, pass ``directed=True``; then ``A[i, j]`` is interpreted as a
link from observation ``i`` to observation ``j``. Pass ``use_weights=False`` to
treat every nonzero sparse entry as weight ``1.0``.

.. code-block:: python

    infomap.tl.infomap(
        adata,
        directed=False,
        use_weights=True,
        key_added="infomap",
    )

Results and lower-level access
------------------------------

``adata.obs[key_added]`` is a pandas categorical column with string module
labels, matching common Scanpy clustering output. ``adata.uns[key_added]``
contains the selected graph key, Infomap options, number of modules and
codelength.

For hierarchical module assignments, flows, custom output files or repeated
runs with the same graph, use the lower-level :class:`infomap.Infomap` API with
:meth:`infomap.Infomap.add_scipy_sparse_matrix`. The AnnData helper keeps the
default output compact for interactive single-cell analysis.

See also
--------

- :doc:`examples/compare-infomap-scanpy-workflow` for a worked comparison with
  Leiden in a Scanpy-style workflow.
- :doc:`usage` for sparse matrix and lower-level :class:`infomap.Infomap`
  patterns.
