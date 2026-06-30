---
jupytext:
  text_representation:
    format_name: myst
kernelspec:
  display_name: Python 3
  language: python
  name: python3
---

# Building a network

```{admonition} In one sentence
:class: tip
Hand your network to :func:`infomap.run` however it already lives in memory:
NetworkX, igraph, a SciPy sparse matrix, an edge index, or raw link tuples. For
non-default loading, build it explicitly with a :class:`~infomap.Network`.
```

## Motivation

Community detection sits at the end of a data pipeline, not the beginning.
Your network might be a NetworkX graph from a scraping step, a SciPy sparse
adjacency matrix from a machine-learning preprocessing step, or a DataFrame from
a SQL query. Converting everything to a common intermediate format costs time,
introduces bugs, and hides the original structure of your data.

:func:`infomap.run` meets you where you are. It dispatches on the type of its
argument, builds the same internal flow network whatever the source, and returns
an immutable :class:`~infomap.Result`. So the partition from a NetworkX graph is
numerically identical to the partition from a SciPy matrix built on the same
edges.

When you need to control *how* a graph is read, a different edge-weight
attribute or explicit directedness, build the network first with a
:class:`~infomap.Network` and run that.

## Input routes at a glance

| Your data | One call | Explicit builder |
|---|---|---|
| NetworkX `Graph` / `DiGraph` | `infomap.run(g)` | `Network.from_networkx(g, weight=…)` |
| igraph `Graph` | `infomap.run(g)` | `Network.from_igraph(g, edge_weights=…)` |
| SciPy sparse adjacency | `infomap.run(A)` | `Network.from_scipy_sparse_matrix(A, directed=…)` |
| `(2, E)` edge index | `infomap.run(edge_index)` | `Network.from_edge_index(ei, …)` |
| link rows (pandas / NumPy / tuples) | `infomap.run(rows)` | `Network().add_links(rows)` |
| network file | `infomap.run("graph.net")` | `Network.from_file(path)` |

Keyword arguments to :func:`infomap.run` configure the *engine* (``seed``,
``num_trials``, ``directed``, …). Arguments that govern *how the input is read*
belong to the ``Network.from_*`` constructors; passing one to :func:`infomap.run`
raises with a pointer to the right constructor rather than silently building a
different graph.

Two library-idiomatic one-shot helpers return native types instead of a
:class:`~infomap.Result`: :func:`infomap.find_communities` (a NetworkX-style
``list[set]``) and :func:`infomap.find_igraph_communities` (an
``igraph.VertexClustering``).

## NetworkX

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

result = infomap.run(g, two_level=True, seed=123, num_trials=5, silent=True)

print(f"Modules:    {result.num_top_modules}")
print(f"Codelength: {result.codelength:.4f} bits/step")
print(f"Modules:    {result.modules()}")     # {node_id: module_id}
```

**Non-integer node labels** (strings, compound keys) work out of the box. The
loader maps them to internal integers; the cleanest way to read assignments back
in your own labels is the ``"name"`` column of the result DataFrame:

```{code-cell} python
g_str = nx.Graph([("alice", "bob"), ("bob", "carol"), ("dave", "eve")])

result = infomap.run(g_str, two_level=True, seed=123, silent=True)
print(result.to_dataframe(["name", "module_id"]).to_string(index=False))
```

When you need the integer-to-label mapping itself, build a
:class:`~infomap.Network`: its :attr:`~infomap.Network.node_id_to_label` records
it.

```{code-cell} python
from infomap import Network, run

net = Network.from_networkx(g_str)
result = run(net, seed=123, silent=True)

named = {net.node_id_to_label[nid]: mid for nid, mid in result.modules().items()}
print("Named assignments:", named)
```

**Directed graphs**: pass a `DiGraph` and add ``directed=True`` to use a
directed-flow model with teleportation. **Weighted edges** are read from the
``"weight"`` attribute by default; for a different attribute, build with
``Network.from_networkx(g, weight="capacity")`` and run the network.

**One-shot in NetworkX style**: :func:`infomap.find_communities` returns a
``list[set]`` in your original labels, for quick exploration:

```{code-cell} python
communities = infomap.find_communities(g, two_level=True, seed=123, num_trials=5)
print("Communities:", communities)
```

## python-igraph

```{code-cell} python
import igraph as ig

edges = [(0, 1), (1, 2), (2, 0), (3, 4), (4, 5), (5, 3), (2, 3)]
g_ig = ig.Graph(n=6, edges=edges)

result = infomap.run(g_ig, two_level=True, seed=123, num_trials=5, silent=True)
print(f"igraph route: {result.num_top_modules} modules, {result.codelength:.4f} bits/step")
```

For a weighted igraph graph, name the edge-weight attribute through the builder:

```{code-cell} python
g_ig_w = ig.Graph(n=6, edges=edges)
g_ig_w.es["weight"] = [1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.5]

net = Network.from_igraph(g_ig_w, edge_weights="weight")
result = run(net, two_level=True, seed=123, num_trials=5, silent=True)
print(f"igraph (weighted): {result.num_top_modules} modules, {result.codelength:.4f} bits/step")
```

:func:`infomap.find_igraph_communities` is the one-shot variant; it returns an
``igraph.VertexClustering`` with a ``.codelength`` attribute, matching igraph's
own community functions.

```{admonition} igraph already has Graph.community_infomap()
:class: note
python-igraph ships a bundled Infomap under `Graph.community_infomap()`. The
`infomap` package is useful when you want the current Infomap release,
Infomap-specific options (multilayer, state networks, convergence control), or a
`VertexClustering` from it.
```

## SciPy sparse adjacency matrix

Graph ML pipelines and spectral methods often produce sparse adjacency matrices
directly. :func:`infomap.run` accepts CSR, CSC, COO, and the other SciPy sparse
formats and treats the matrix as undirected:

```{code-cell} python
import scipy.sparse as sp

# The same two-triangle adjacency as a symmetric COO matrix
rows = [0, 1, 1, 2, 2, 0,  3, 4, 4, 5, 5, 3,  2, 3]
cols = [1, 0, 2, 1, 0, 2,  4, 3, 5, 4, 3, 5,  3, 2]
data = [1.0] * 12 + [0.5, 0.5]
A = sp.coo_matrix((data, (rows, cols)), shape=(6, 6))

result = infomap.run(A, two_level=True, seed=123, num_trials=5, silent=True)
print(f"SciPy route: {result.num_top_modules} modules, {result.codelength:.4f} bits/step")
```

To read the matrix as **directed** (``A[i, j]`` is an edge from row ``i`` to
column ``j``) or to ignore the stored values (``weighted=False``), build with the
constructor and run the network:

```{code-cell} python
net = Network.from_scipy_sparse_matrix(A, directed=False, weighted=True)
result = run(net, two_level=True, seed=123, num_trials=5, silent=True)
print(f"via Network: {result.num_top_modules} modules")
```

`node_ids` gives external ids in matrix-row order; the mapping is stored on
:attr:`~infomap.Network.node_id_to_label`, the same pattern as
``from_networkx``.

## Edge lists: pandas, NumPy, tuples

A weighted edge list, rows of ``(source, target, weight)``, runs directly.
Pandas DataFrames convert in one call with ``to_numpy()``:

```{code-cell} python
import pandas as pd

edges_df = pd.DataFrame({
    "source": [0, 1, 2, 3, 4, 5, 2],
    "target": [1, 2, 0, 4, 5, 3, 3],
    "weight": [1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.5],
})

result = infomap.run(
    edges_df[["source", "target", "weight"]].to_numpy(),
    two_level=True, seed=123, num_trials=5, silent=True,
)
print(f"pandas route: {result.num_top_modules} modules, {result.codelength:.4f} bits/step")
```

Source and target columns must hold integers. Convert string labels to integer
codes first (``pandas.factorize``), then read names back through the result
DataFrame or your own reverse mapping. An equally valid one-call form is an
iterable of tuples: ``infomap.run([(0, 1, 1.0), (1, 2, 1.0), ...])``.

```{admonition} Edge index vs link rows
:class: note
A two-row *integer* array is read as a ``(2, E)`` edge index (the PyG / GNN
convention). Rows of ``(source, target, weight)`` have a float column and are
read as weighted links. For an explicit edge index with its own options, use
`Network.from_edge_index(edge_index, edge_weight=..., directed=...)`.
```

## Building incrementally with Network

When you assemble a network programmatically, read a custom file format, or wire
up a handful of edges, use :class:`~infomap.Network` directly. Its ``add_*``
verbs return the network, so calls chain, and :func:`infomap.run` takes the
built network:

```{code-cell} python
from infomap import Network, run

net = Network()
net.add_node(0, name="Alice")
net.add_node(1, name="Bob")
net.add_node(2, name="Carol")
net.add_node(3, name="Dave")
net.add_node(4, name="Eve")
net.add_node(5, name="Frank")

net.add_link(0, 1); net.add_link(1, 2); net.add_link(2, 0)   # triangle A
net.add_link(3, 4); net.add_link(4, 5); net.add_link(5, 3)   # triangle B
net.add_link(2, 3, 0.5)                                       # weak bridge

result = run(net, two_level=True, seed=123, num_trials=5, silent=True)
print(result.to_dataframe(["node_id", "name", "module_id", "flow"]).to_string(index=False))
```

`add_node(node_id, name=None, teleportation_weight=None)` registers a node and
an optional label; a node referenced only by `add_link` is created
automatically. `add_link(source, target, weight=1.0)` adds one link, and
`add_links(rows)` adds many from an iterable or a NumPy ``(n, 2)`` / ``(n, 3)``
array. The flow model (directed or undirected) is an engine option on the run,
not a property of individual links.

## Same graph, same partition

Every route builds the same internal flow network. The codelength is identical
across them:

```{code-cell} python
runs = {
    "NetworkX":     infomap.run(g, two_level=True, seed=123, num_trials=5, silent=True),
    "SciPy sparse": infomap.run(A, two_level=True, seed=123, num_trials=5, silent=True),
    "edge list":    infomap.run(
        edges_df[["source", "target", "weight"]].to_numpy(),
        two_level=True, seed=123, num_trials=5, silent=True,
    ),
}
for route, res in runs.items():
    print(f"  {res.codelength:.4f}  {route}")
```

To draw the partition or write it to disk, see
{doc}`visualizing-and-exporting`.

## API pointers

- {func}`infomap.run` is the one-call front door; it dispatches on the input
  type and returns an immutable :class:`~infomap.Result`.
- {class}`infomap.Network` builds a network explicitly. Its constructors
  {meth}`~infomap.Network.from_networkx`, {meth}`~infomap.Network.from_igraph`,
  {meth}`~infomap.Network.from_scipy_sparse_matrix`,
  {meth}`~infomap.Network.from_edge_index`, and
  {meth}`~infomap.Network.from_file` take the adapter options that
  :func:`infomap.run` does not.
- {meth}`infomap.Network.add_node`, {meth}`infomap.Network.add_link`, and
  {meth}`infomap.Network.add_links` build a network incrementally.
- {func}`infomap.find_communities` and {func}`infomap.find_igraph_communities`
  are one-shot helpers returning a NetworkX ``list[set]`` and an
  ``igraph.VertexClustering`` respectively.
- {meth}`infomap.Result.modules` returns `{node_id: module_id}`;
  {meth}`infomap.Result.to_dataframe` returns node id, module id, flow, and name
  as a DataFrame.

## Going deeper

- **Full API reference**: {doc}`/api/index` gives complete signatures for every
  entry point above.
- **Richer network representations**: {doc}`/flow-models/index` covers data with
  memory, layers, or explicit state nodes.
- {cite:t}`smiljanic2026survey`, §4, treats flow modelling in depth, including
  undirected versus directed links.
