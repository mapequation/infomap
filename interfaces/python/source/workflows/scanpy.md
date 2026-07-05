---
jupytext:
  text_representation:
    format_name: myst
kernelspec:
  display_name: Python 3
  language: python
  name: python3
---

# Scanpy and AnnData

{bdg-primary-line}`Workflow`

```{admonition} At a glance
:class: tip
Pull Infomap straight into your single-cell analysis pipeline: point it at the
kNN connectivity graph Scanpy already built, get back AnnData-compatible
cluster labels, and compare with Leiden in a few lines.
```

## Infomap in a single-cell pipeline

Single-cell RNA-seq pipelines, Scanpy chief among them, cluster cells by
building a $k$-nearest-neighbour graph in PCA-reduced space and then running a
graph-partitioning algorithm. Leiden, the standard default, optimises a
modularity-style objective with a tunable resolution parameter; Infomap optimises
the map equation instead. {doc}`/concepts/choosing-a-method` compares the two.

Both algorithms take the same weighted neighbour graph as input and produce a
partition of observations as output, so swapping in Infomap means calling a
different function. The `infomap.tl.infomap()` function follows Scanpy `tl`
conventions: it reads `adata.obsp["connectivities"]` by default, writes a
pandas categorical column to `adata.obs`, and records run metadata in
`adata.uns`.

Beyond the high-level helper, the sparse matrix underlying any `adata.obsp`
slot is a standard SciPy CSR matrix; you can hand it directly to
`infomap.run()` and use the full Result API (hierarchical modules, codelength,
multiple trials) without any AnnData involvement.

## The neighbour graph as flow

The neighbour graph Scanpy builds is a network: cells are nodes, and each edge
carries a connectivity weight reflecting how similar two cells are in PCA space.
Infomap reads it as a flow network and partitions it by compression, so densely
connected cell groups become modules (see {doc}`/concepts/the-map-equation`).

This flow-centric view often agrees with Leiden, but the two can diverge:
Leiden's partition depends on its resolution parameter, and the connectivity
graph may be asymmetric. Running both and comparing is a useful sanity check,
especially for datasets where cluster sizes vary widely.

## Cluster three cell populations

### Setup: synthetic AnnData with three clusters

The example is fully synthetic and small so the build completes in a few
seconds. Three tight Gaussian blobs in 10-dimensional space give Scanpy's
neighbour graph a clear community structure that both algorithms should recover.

```{code-cell} python
import warnings
warnings.filterwarnings("ignore", message="IProgress not found.*")
warnings.filterwarnings("ignore", category=FutureWarning)

import numpy as np
import pandas as pd
import scipy.sparse as sp
from sklearn.datasets import make_blobs

import scanpy as sc
import infomap

# Three-cluster synthetic dataset: 150 cells, 10 PCA-like features
X, truth = make_blobs(
    n_samples=150,
    centers=3,
    n_features=10,
    cluster_std=1.2,
    random_state=42,
)

adata = sc.AnnData(X.astype(np.float32))
adata.obs["true_label"] = pd.Categorical([str(l) for l in truth])

print(adata)
print("True cluster sizes:", adata.obs["true_label"].value_counts().to_dict())
```

### Build the kNN graph

`sc.pp.neighbors` computes a weighted $k$-nearest-neighbour graph and stores two
sparse matrices in `adata.obsp`:

- **`connectivities`**: symmetrised connectivity weights used by clustering tools.
- **`distances`**: raw distances, not used for clustering.

```{code-cell} python
sc.pp.neighbors(adata, n_neighbors=15, random_state=42)

print("obsp keys:", list(adata.obsp.keys()))
A = adata.obsp["connectivities"]
print(f"Connectivity matrix: {A.shape}, {A.nnz} non-zeros")
```

### Cluster with Leiden and Infomap

Both algorithms read the same connectivity matrix. The `infomap.tl.infomap`
helper mirrors the Scanpy `tl` calling convention and writes results directly
into `adata`.

```{code-cell} python
# Leiden: Scanpy's standard community detection
sc.tl.leiden(
    adata,
    key_added="leiden",
    random_state=42,
    flavor="igraph",
    n_iterations=2,
    directed=False,
)

# Infomap: reads adata.obsp["connectivities"] by default
infomap.tl.infomap(
    adata,
    key_added="infomap",
    seed=123,
    num_trials=5,
)

print("Leiden modules: ", adata.obs["leiden"].nunique())
print("Infomap modules:", adata.obs["infomap"].nunique())
print("\nInfomap run metadata:")
for k, v in adata.uns["infomap"].items():
    print(f"  {k}: {v}")
```

The `codelength` in `adata.uns["infomap"]` is the map equation value for the
best partition Infomap found; a lower codelength means the partition compresses
the random walk more efficiently (see {doc}`/concepts/the-map-equation`).

### Compare the two labelings

Because both columns are categorical strings stored in `adata.obs`, a simple
cross-tabulation reveals how much the two partitions agree.

```{code-cell} python
ct = pd.crosstab(
    adata.obs["infomap"],
    adata.obs["leiden"],
    rownames=["Infomap"],
    colnames=["Leiden"],
)
ct
```

A near-diagonal table, one Infomap module per Leiden module with no cells split
across modules, is a sign that the two algorithms are finding the same
structure. On messier real datasets you will see more off-diagonal entries,
which can guide interpretation: cells that switch label between algorithms tend
to sit at cluster boundaries.

### Using the lower-level sparse-matrix API

If you want hierarchical modules, flow values, or finer control over Infomap
options, skip the `tl` helper and use the core API directly. The connectivity
matrix is a standard SciPy CSR matrix that you can pass straight to
`infomap.run()`.

```{code-cell} python
A = adata.obsp["connectivities"]

result = infomap.run(A, silent=True, seed=123, num_trials=5)

print(f"Modules:    {result.num_top_modules}")
print(f"Codelength: {result.codelength:.4f} bits/step")

# Write labels back into adata.obs
modules = result.modules()  # {node_index: module_id}
adata.obs["infomap_lowlevel"] = pd.Categorical(
    [str(modules[i]) for i in range(adata.n_obs)]
)

adata.obs[["infomap", "infomap_lowlevel"]].value_counts()
```

The two agree because the `tl` helper and the core API call the same engine with
the same options; the helper only adds the AnnData bookkeeping.

### Visualise with UMAP

Scanpy's UMAP embedding uses the same neighbour graph and gives a 2-D view of
cell layout. Colouring by ground truth, Infomap, and Leiden side by side is the
standard visual QC step.

```{code-cell} python
sc.tl.umap(adata, random_state=42)
sc.pl.umap(
    adata,
    color=["true_label", "infomap", "leiden"],
    wspace=0.4,
    show=True,
)
```

## Pitfalls

- **Scanpy is not a dependency of `infomap`.** `infomap.tl.infomap` reads an
  AnnData you already built. Install the AnnData support with
  `pip install "infomap[anndata]"`, and install Scanpy itself separately.
- **Results follow the neighbour graph.** They depend on your `sc.pp.neighbors`
  settings (`n_neighbors`, the representation), so build that graph deliberately.
- **Compare with another method.** Cross-tabulate against Leiden; the cells that
  switch labels between the two are where the objectives disagree.

## API pointers

- {func}`infomap.tl.infomap` is the AnnData-aware helper. It reads
  `adata.obsp["connectivities"]` by default and accepts `neighbors_key`, `obsp`,
  and `adjacency` to point at a different graph. It writes a categorical column
  to `adata.obs[key_added]` and metadata to `adata.uns[key_added]`.
- {func}`infomap.run` partitions any SciPy sparse matrix directly (use
  {meth}`infomap.Network.from_scipy_sparse_matrix` for non-default loading); reach
  for it when you need hierarchical output or flow values.
- {meth}`infomap.Result.modules` returns `{node_index: module_id}` and accepts
  `depth` for sub-module assignments.
- {attr}`infomap.Result.codelength` is the map equation value for the best
  partition; it is also at `adata.uns[key_added]["codelength"]` after
  `tl.infomap`.
- {attr}`infomap.Result.num_top_modules` is the number of top-level modules.

Key `infomap.tl.infomap` keyword arguments:

| Argument | Default | Purpose |
|---|---|---|
| `key_added` | `"infomap"` | Column name in `adata.obs` / key in `adata.uns` |
| `obsp` | `None` (uses `"connectivities"`) | Select a specific `adata.obsp` slot |
| `neighbors_key` | `None` | Resolve graph via `adata.uns[neighbors_key]` |
| `adjacency` | `None` | Pass a sparse matrix directly |
| `directed` | `False` | Treat graph as directed |
| `use_weights` | `True` | Use edge weights; `False` for unweighted |
| `seed` | Infomap's `123` | Random seed forwarded to Infomap |
| `num_trials` | Infomap's `1` | Independent restarts; more trials give a more stable partition |

## Going deeper

- Companion notebook: `examples/notebooks/compare-infomap-scanpy-workflow.ipynb`
  gives a fuller comparison with AMI/NMI scoring and graph-selection notes.
- {doc}`/working-with-infomap/inputs` shows how `Network.from_scipy_sparse_matrix`
  handles directed, undirected, and weighted graphs.
- {doc}`/concepts/the-map-equation` is the objective Infomap minimises.
- The map equation's source paper {cite:p}`rosvall2008maps`.
