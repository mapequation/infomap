---
jupytext:
  text_representation:
    format_name: myst
kernelspec:
  display_name: Python 3
  language: python
  name: python3
---

# Inputs: building networks

```{admonition} In one sentence
:class: tip
Infomap accepts your network however it already lives in memory: NetworkX,
igraph, SciPy sparse matrices, pandas edge-lists, or raw link tuples. You skip
the conversion boilerplate and go straight to analysis.
```

## Motivation

Community detection sits at the end of a data pipeline, not the beginning.
Your network might be stored as a NetworkX graph from a scraping step, a SciPy
sparse adjacency matrix from a machine-learning preprocessing step, or a pandas
DataFrame from a SQL query. Converting everything to a common intermediate
format costs time, introduces bugs, and hides the original structure of your
data.

Infomap's Python interface meets you where you are. Each major graph
representation has a dedicated entry point. Under the hood they all build
the same internal network and run the same algorithm, so the partition you
get from a NetworkX graph is numerically identical to the partition you would
get from a SciPy matrix built from the same edges.

This chapter shows you how to load each representation, what each entry point
controls, and how to verify that two routes give the same answer.

## Input routes at a glance

| Your data | Entry point | Returns |
|---|---|---|
| NetworkX `Graph` / `DiGraph` | `im.add_networkx_graph(g)` | `{int: label}` mapping |
| NetworkX graph (one-shot) | `infomap.find_communities(g)` | `list[set]` partition |
| python-igraph `Graph` | `im.add_igraph_graph(g)` | `{int: label}` mapping |
| igraph graph (one-shot) | `infomap.find_igraph_communities(g)` | `VertexClustering` |
| SciPy sparse adjacency | `im.add_scipy_sparse_matrix(A)` | `{int: label}` mapping |
| pandas edge-list `DataFrame` | `im.add_links(df[...].to_numpy())` | (none) |
| Raw tuples / integers | `im.add_link(u, v, weight)` | (none) |

All entry points that return a mapping give you a dict from Infomap's internal
zero-based integer ids to your original node labels, so you can translate
results back to your domain. When your node labels are already integers
starting from zero the mapping is an identity and you can ignore it.

## Route 1: NetworkX

NetworkX is the most common graph library in Python data science. Infomap
wraps it with two entry points:

- **`infomap.find_communities(g)`** is the one-shot helper. It takes any
  NetworkX-compatible graph, runs Infomap with sensible defaults, and returns a
  `list[set]` of communities in your original node labels. Reach for it during
  quick exploration.
- **`im.add_networkx_graph(g)`** is the instance method. It loads the graph into
  an `Infomap` instance you control, for when you need to tune parameters, run
  multiple trials, inspect codelengths, or read hierarchical results.

```{code-cell} python
import networkx as nx
import infomap

# A small two-community graph: two triangles joined by a weak bridge
g = nx.Graph()
g.add_weighted_edges_from([
    (0, 1, 1.0), (1, 2, 1.0), (2, 0, 1.0),   # triangle A
    (3, 4, 1.0), (4, 5, 1.0), (5, 3, 1.0),   # triangle B
    (2, 3, 0.5),                               # weak bridge
])

# One-shot helper: returns a list of sets using original node labels
communities = infomap.find_communities(
    g,
    two_level=True, seed=123, num_trials=5,
)
print("Communities:", communities)
```

```{code-cell} python
# Full control: construct an Infomap instance and load the graph
im = infomap.Infomap(two_level=True, seed=123, num_trials=5, silent=True)
mapping = im.add_networkx_graph(g)   # returns {internal_id: original_label}
im.run()

modules = im.get_modules()           # {internal_id: module_id}
print(f"Modules:     {im.num_top_modules}")
print(f"Codelength:  {im.codelength:.4f} bits/step")
print(f"Mapping:     {mapping}")
print(f"Assignments: {modules}")
```

**Non-integer node labels** (strings, compound keys) work out of the box.
`add_networkx_graph` maps them to stable internal integers and returns the
reverse mapping so you can translate assignments back:

```{code-cell} python
g_str = nx.Graph([("alice", "bob"), ("bob", "carol"), ("dave", "eve")])

im2 = infomap.Infomap(two_level=True, seed=123, silent=True)
mapping2 = im2.add_networkx_graph(g_str)
im2.run()

result = {mapping2[nid]: mid for nid, mid in im2.get_modules().items()}
print("Named assignments:", result)
```

**Directed graphs** are detected automatically: pass a `DiGraph` and Infomap
uses a directed-flow model (with teleportation) without any extra flag.

**Weighted edges** are read from the `"weight"` edge attribute by default.
Pass `weight=None` to treat every edge as weight 1, or pass a different
attribute name.

## Route 2: python-igraph

igraph offers two entry points that mirror the igraph community-detection
convention:

- **`infomap.find_igraph_communities(g)`** returns an `igraph.VertexClustering`
  with a `.codelength` attribute, matching igraph's own community functions.
- **`im.add_igraph_graph(g)`** loads the graph into an `Infomap` instance you
  control.

```{code-cell} python
import igraph as ig

# Same two-triangle topology as above, built as an igraph Graph
edges = [(0, 1), (1, 2), (2, 0), (3, 4), (4, 5), (5, 3), (2, 3)]
g_ig = ig.Graph(n=6, edges=edges)

# One-shot helper: returns VertexClustering with .codelength
vc = infomap.find_igraph_communities(
    g_ig,
    trials=5, seed=123, silent=True, two_level=True,
)
print("Membership:  ", vc.membership)
print("Codelength:  ", round(vc.codelength, 4))
print("Num clusters:", len(vc))
```

```{code-cell} python
# Full control with a weighted igraph graph
g_ig_w = ig.Graph(n=6, edges=edges)
g_ig_w.es["weight"] = [1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.5]

im3 = infomap.Infomap(two_level=True, seed=123, num_trials=5, silent=True)
im3.add_igraph_graph(g_ig_w, edge_weights="weight")
im3.run()

print(f"igraph (weighted): {im3.num_top_modules} modules, {im3.codelength:.4f} bits/step")
```

```{admonition} igraph already has Graph.community_infomap()
:class: note
python-igraph ships a bundled Infomap implementation under
`Graph.community_infomap()`. The `infomap` package entry point is useful when
you want the current Infomap release, Infomap-specific options (multilayer,
state networks, convergence control), or a `VertexClustering` from it.
```

## Route 3: SciPy sparse adjacency matrix

Graph ML pipelines and spectral methods often produce sparse adjacency matrices
directly. `im.add_scipy_sparse_matrix(A)` accepts CSR, CSC, COO, and all other
SciPy sparse formats:

```{code-cell} python
import scipy.sparse as sp
import numpy as np

# Build the same two-triangle adjacency as a symmetric COO matrix
rows = [0, 1, 1, 2, 2, 0,  3, 4, 4, 5, 5, 3,  2, 3]
cols = [1, 0, 2, 1, 0, 2,  4, 3, 5, 4, 3, 5,  3, 2]
data = [1.0] * 12 + [0.5, 0.5]    # bridge has weight 0.5 in both directions
A = sp.coo_matrix((data, (rows, cols)), shape=(6, 6))

im4 = infomap.Infomap(two_level=True, seed=123, num_trials=5, silent=True)
im4.add_scipy_sparse_matrix(A, directed=False)
im4.run()

print(f"SciPy route: {im4.num_top_modules} modules, {im4.codelength:.4f} bits/step")
print(f"Modules:     {im4.get_modules()}")
```

Key parameters:

- `directed=False` (default) treats the matrix as symmetric (undirected flow).
  Pass `directed=True` and `A[i, j]` becomes a directed edge from row `i` to
  column `j`.
- `weighted=True` (default) uses the matrix values as weights. Pass
  `weighted=False` to treat every nonzero entry as weight 1.
- `node_ids` gives external ids in matrix-row order; the method returns a reverse
  mapping (the same pattern as `add_networkx_graph`).

There is also a class-method constructor `Infomap.from_scipy_sparse_matrix(A,
directed=..., silent=True)` that creates the instance and loads the matrix
in one call, useful for one-shot scripts.

## Route 4: pandas edge-list DataFrame

Pandas DataFrames are a natural home for relational or tabular graph data
imported from CSV, SQL, or data-lake sources. Infomap has no `from_dataframe`
constructor, but `im.add_links` accepts any NumPy 2-D array, and
`DataFrame.to_numpy()` converts in one call:

```{code-cell} python
import pandas as pd

# Edge list as a DataFrame (as you might load from a CSV or SQL query)
edges_df = pd.DataFrame({
    "source": [0, 1, 2, 3, 4, 5, 2],
    "target": [1, 2, 0, 4, 5, 3, 3],
    "weight": [1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.5],
})

im5 = infomap.Infomap(two_level=True, seed=123, num_trials=5, silent=True)
im5.add_links(edges_df[["source", "target", "weight"]].to_numpy())
im5.run()

print(f"pandas route: {im5.num_top_modules} modules, {im5.codelength:.4f} bits/step")
```

`add_links` expects an array of shape `(n_edges, 2)` (unweighted) or
`(n_edges, 3)` (weighted, third column is weight). The source and target
columns must hold integers; convert string labels to integer codes before
calling:

```{code-cell} python
# String labels: convert with pandas Categorical
edges_str = pd.DataFrame({
    "source": ["alice", "bob", "carol", "dave", "eve", "frank", "carol"],
    "target": ["bob", "carol", "alice", "eve", "frank", "dave", "dave"],
    "weight": [1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.5],
})

# Map labels to integers
codes, labels = pd.factorize(
    pd.concat([edges_str["source"], edges_str["target"]]).unique()
)
label_to_int = {label: i for i, label in enumerate(labels)}

edges_int = edges_str.copy()
edges_int["source"] = edges_str["source"].map(label_to_int)
edges_int["target"] = edges_str["target"].map(label_to_int)

im6 = infomap.Infomap(two_level=True, seed=123, num_trials=5, silent=True)
im6.add_links(edges_int[["source", "target", "weight"]].to_numpy())
im6.run()

# Translate back
int_to_label = {v: k for k, v in label_to_int.items()}
named_modules = {int_to_label[nid]: mid for nid, mid in im6.get_modules().items()}
print("Named modules:", named_modules)
```

After a run you can export results straight back to a DataFrame with
`im.to_dataframe(columns=["node_id", "module_id", "flow", "name"])`.

## Route 5: raw / low-level API

The low-level API (`im.add_node` and `im.add_link`) helps when you build the
network programmatically, read a custom file format, or wire up a handful of
edges in a script:

```{code-cell} python
im7 = infomap.Infomap(two_level=True, seed=123, num_trials=5, silent=True)

# Add nodes with human-readable names (optional, for labelling)
im7.add_node(0, name="Alice")
im7.add_node(1, name="Bob")
im7.add_node(2, name="Carol")
im7.add_node(3, name="Dave")
im7.add_node(4, name="Eve")
im7.add_node(5, name="Frank")

# Add links; weight defaults to 1.0 if omitted
im7.add_link(0, 1);  im7.add_link(1, 2);  im7.add_link(2, 0)   # triangle A
im7.add_link(3, 4);  im7.add_link(4, 5);  im7.add_link(5, 3)   # triangle B
im7.add_link(2, 3, 0.5)                                         # weak bridge

im7.run()

# Export with names via to_dataframe
df = im7.to_dataframe(columns=["node_id", "name", "module_id", "flow"])
print(df.to_string(index=False))
```

**`add_node(node_id, name=None, teleportation_weight=None)`** registers a node
and optionally attaches a human-readable label and teleportation weight. If you
call `add_link` for a node that doesn't exist
yet, it is created automatically with no name.

**`add_link(source_id, target_id, weight=1.0)`** adds a single directed or
undirected link (the flow model is determined by the `directed` constructor
argument, not by individual link calls).

**`add_links(links)`** is the vectorised variant. It accepts an iterable of
`(source, target)` or `(source, target, weight)` tuples, or a NumPy array of
shape `(n, 2)` or `(n, 3)`. Prefer it over a loop when adding many links.

## Comparing routes: same graph, same partition

All five routes build the same internal flow network. You can verify this by
checking that the codelength is identical across routes:

```{code-cell} python
codelengths = {
    "NetworkX (add_networkx_graph)": im.codelength,
    "SciPy sparse (add_scipy_sparse_matrix)": im4.codelength,
    "pandas (add_links)": im5.codelength,
    "raw API (add_link)": im7.codelength,
}

for route, L in codelengths.items():
    print(f"  {L:.4f}  {route}")
```

```{admonition} Why the igraph result differs
:class: note
The igraph route (`im3.codelength`) comes out lower here because that graph
carries explicit `es["weight"]` values (0.5 and 1.0), which rescale the flow
relative to the unweighted graph passed to `find_igraph_communities`. Build the
graphs with the same weights and the codelengths match.
```

To draw the partition or write it to disk, see
{doc}`visualizing-and-exporting`.

## API pointers

- {meth}`infomap.Infomap.add_networkx_graph` loads any NetworkX graph; it
  handles weights, directed graphs, and state or multilayer graphs.
- {func}`infomap.find_communities` is the one-shot NetworkX entry point; it
  returns `list[set]` in your original node labels.
- {meth}`infomap.Infomap.add_igraph_graph` loads any python-igraph graph.
- {func}`infomap.find_igraph_communities` is the one-shot igraph entry point; it
  returns an `igraph.VertexClustering`.
- {meth}`infomap.Infomap.add_scipy_sparse_matrix` loads a SciPy sparse adjacency
  matrix, and is also available as the class-method constructor
  {meth}`infomap.Infomap.from_scipy_sparse_matrix`.
- {meth}`infomap.Infomap.add_links` loads links in bulk from a NumPy array or
  iterable; combine it with `DataFrame.to_numpy()` for pandas.
- {meth}`infomap.Infomap.add_link` adds a single link (source, target, weight).
- {meth}`infomap.Infomap.add_node` adds a node with an optional name.
- {meth}`infomap.Infomap.get_modules` retrieves top-level module assignments as
  `{node_id: module_id}` after a run.
- {meth}`infomap.Infomap.to_dataframe` exports results (node id, module id,
  flow, name) as a pandas DataFrame.

## Going deeper

- **Full API reference**: {doc}`/api/index` gives complete method signatures,
  parameters, and return types for every entry point above.
- **Richer network representations**: {doc}`/flow-models/index` covers data with
  memory, layers, or explicit state nodes.
- {cite:t}`smiljanic2026survey`, §4, treats flow modelling in depth, including
  undirected versus directed links.
