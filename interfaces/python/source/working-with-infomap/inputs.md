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

{bdg-success-line}`How-to`

```{admonition} At a glance
:class: tip
Hand your network to {func}`infomap.run` however it already lives in memory:
NetworkX, igraph, a SciPy sparse matrix, an edge index, or raw link tuples. For
non-default loading, build it explicitly with a {class}`~infomap.Network`.
```

## Run from the format you already have

Your network might be a NetworkX graph from a scraping step, a SciPy sparse
adjacency matrix from a machine-learning pipeline, or a DataFrame from a SQL
query. {func}`infomap.run` dispatches on the type of its argument, builds the
same internal flow network whatever the source, and returns an immutable
{class}`~infomap.Result`. The partition from a NetworkX graph is therefore
numerically identical to the partition from a SciPy matrix built on the same
edges.

When you need to control *how* a graph is read, a different edge-weight
attribute or explicit directedness, build the network first with a
{class}`~infomap.Network` and run that.

## Input routes at a glance

| Your data | One call | Explicit builder |
|---|---|---|
| NetworkX `Graph` / `DiGraph` | `infomap.run(g)` | `Network.from_networkx(g, weight=…)` |
| igraph `Graph` | `infomap.run(g)` | `Network.from_igraph(g, edge_weights=…)` |
| SciPy sparse adjacency | `infomap.run(A)` | `Network.from_scipy_sparse_matrix(A, directed=…)` |
| `(2, E)` edge index | `infomap.run(edge_index)` | `Network.from_edge_index(ei, …)` |
| link rows (NumPy array / tuples) | `infomap.run(rows)` | `Network().add_links(rows)` |
| pandas `DataFrame` of links | `infomap.run(df.to_numpy())` | `Network().add_links(df.to_numpy())` |
| network file | `infomap.run("graph.net")` | `Network.from_file(path)` |

Keyword arguments to {func}`infomap.run` configure the *engine* (``seed``,
``num_trials``, ``markov_time``, …). Arguments that govern *how the input is
read* belong to the ``Network.from_*`` constructors; passing one to
{func}`infomap.run` raises with a pointer to the right constructor rather than
silently building a different graph. For a NetworkX or igraph graph the
directedness is read from the graph object itself, so no ``directed`` argument
is needed. For a SciPy matrix or edge index, pass ``directed=`` to the matching
``from_*`` constructor; the two routes default differently, so it is worth
spelling out (see Pitfalls).

Two one-shot helpers return native types instead of a
{class}`~infomap.Result`: {func}`infomap.find_communities` (a NetworkX-style
``list[set]``) and {func}`infomap.find_igraph_communities` (an
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
print(f"Assignment: {result.modules()}")     # {node_id: module_id}
```

**Non-integer node labels** (strings, compound keys) work out of the box. The
loader maps them to internal integers. For string labels, the cleanest way to
read assignments back in your own labels is the ``"name"`` column of the result
DataFrame; for other label types (tuples, frozensets), use
{class}`~infomap.Network` and its label mapping as shown below:

```{code-cell} python
g_str = nx.Graph([("alice", "bob"), ("bob", "carol"), ("dave", "eve")])

result = infomap.run(g_str, two_level=True, seed=123, silent=True)
print(result.to_dataframe(["name", "module_id"]).to_string(index=False))
```

When you need the integer-to-label mapping itself, build a
{class}`~infomap.Network`: its {attr}`~infomap.Network.node_id_to_label` records
it.

```{code-cell} python
from infomap import Network, run

net = Network.from_networkx(g_str)
result = run(net, seed=123, silent=True)

named = {net.node_id_to_label[nid]: mid for nid, mid in result.modules().items()}
print("Named assignments:", named)
```

**Directed graphs**: pass a `DiGraph` and the adapter selects the directed-flow
model with teleportation automatically. **Weighted edges** are read from the
``"weight"`` attribute by default; for a different attribute, build with
``Network.from_networkx(g, weight="capacity")`` and run the network.

**One-shot in NetworkX style**: {func}`infomap.find_communities` returns a
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

{func}`infomap.find_igraph_communities` is the one-shot variant; it returns an
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
directly. {func}`infomap.run` accepts CSR, CSC, COO, and the other SciPy sparse
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

How the matrix is read is set on the constructor: ``directed=True`` reads
``A[i, j]`` as an edge from row ``i`` to column ``j``, and ``weighted=False``
ignores the stored values. Here with the defaults spelled out:

```{code-cell} python
net = Network.from_scipy_sparse_matrix(A, directed=False, weighted=True)
result = run(net, two_level=True, seed=123, num_trials=5, silent=True)
print(f"via Network: {result.num_top_modules} modules")
```

Pass ``node_ids=`` to give the matrix rows external ids; the mapping is stored
on {attr}`~infomap.Network.node_id_to_label`, the same pattern as
``from_networkx``.

## Edge lists: pandas, NumPy, tuples

A weighted edge list, rows of ``(source, target, weight)``, runs directly. An
iterable of tuples is the same input in one call:
``infomap.run([(0, 1, 1.0), (1, 2, 1.0), ...])``. Pandas DataFrames convert in
one call with ``to_numpy()``:

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
codes first (``pandas.factorize``) and keep the ``uniques`` array it returns as
your reverse mapping. This route registers no node names, so the result
DataFrame's ``"name"`` column cannot recover the labels.

```{admonition} Edge index vs link rows
:class: note
A two-row *integer* array is read as a ``(2, E)`` edge index (the PyG / GNN
convention) and, following that convention, defaults to a **directed** flow
model. Pass ``directed=False`` to `Network.from_edge_index` if your edge index
lists an undirected graph. Rows of ``(source, target, weight)`` have a
float column and are read as weighted links. For an explicit edge index with
its own options, use
`Network.from_edge_index(edge_index, edge_weight=..., directed=...)`.
```

## Network files

Infomap reads its native Pajek-style `.net` files (and plain link lists)
directly from a path. The example below is the two-triangles network that
ships with Infomap as `examples/networks/twotriangles.net`: triangles A–B–C
and D–E–F, joined by the single link C–D. Named vertices come back on the
result's nodes.

```{code-cell} python
import tempfile
from pathlib import Path

import infomap

content = """*Vertices
1 "A"
2 "B"
3 "C"
4 "D"
5 "E"
6 "F"
*Edges
1 2
1 3
2 3
3 4
4 5
4 6
5 6
"""
path = Path(tempfile.mkdtemp()) / "twotriangles.net"
path.write_text(content)

result = infomap.run(str(path), two_level=True, seed=123, num_trials=5, silent=True)
print(f"file route: {result.num_top_modules} modules, {result.codelength:.4f} bits/step")
for node in result.nodes():
    print(f"  {node.name}: module {node.module_id}")
```

`Network.from_file(path)` is the two-step form when you want to inspect or
extend the network before running. The
[input-format reference](https://www.mapequation.org/infomap/#Input) on
mapequation.org documents every section a `.net` file can carry, including
`*States`, `*Bipartite`, and `*Multilayer`. The reference example for each
format also ships pre-loaded in {mod}`infomap.datasets`
(`infomap.datasets.two_triangles()` returns this very network, ready to run).

## JSON network input

Infomap also reads the `infomap-network-json` v1.0 format, a small JSON input
format that is convenient to produce from Python, notebooks, and web tools. The
format is detected by content, so a `.json` file is handed to
{func}`infomap.run` (or {meth}`~infomap.Network.from_file`) like any other
network file. A minimal document needs only `format`, `version`, and `edges`:

```json
{
  "format": "infomap-network-json",
  "version": "1.0",
  "edges": [{ "source": 1, "target": 2, "weight": 1.0 }]
}
```

The same two-triangles network runs straight from a JSON file:

```{code-cell} python
import json
import tempfile
from pathlib import Path

import infomap

network = {
    "format": "infomap-network-json",
    "version": "1.0",
    "edges": [
        {"source": 1, "target": 2},
        {"source": 1, "target": 3},
        {"source": 2, "target": 3},
        {"source": 3, "target": 4},
        {"source": 4, "target": 5},
        {"source": 4, "target": 6},
        {"source": 5, "target": 6},
    ],
}
path = Path(tempfile.mkdtemp()) / "twotriangles.json"
path.write_text(json.dumps(network))

result = infomap.run(str(path), two_level=True, seed=123, num_trials=5, silent=True)
print(f"json route: {result.num_top_modules} modules, {result.codelength:.4f} bits/step")
```

`type` defaults to `standard`; the other values are `bipartite` (requires
`bipartiteStartId`), `multilayer` (with `multilayer` set to `full`, `intra`, or
`intra-inter` and `layers` on each edge), and `state` (edges are state ids, with
an optional `states` array). Parsing is order- and emitter-independent, so key
order (for example alphabetically sorted output) does not matter.

Two optional conveniences replace separate input files:

- `nodes[].meta` embeds one-dimensional integer metadata. Its presence enables
  metadata coding exactly like `--meta-data` or
  {meth}`~infomap.Infomap.set_meta_data` (it is not a passive annotation). An
  external `--meta-data` file still composes with a JSON network and overrides
  the embedded values, with a warning.
- `nodes[].path` (and `states[].path` for state networks) embeds an initial
  partition: `[3]` is equivalent to a `.clu` module and `[1, 2]` to a `.tree`
  path. `--cluster-data` overrides it, with a warning.

All ids are non-negative integers; integral-valued doubles such as `10.0` are
accepted but `1.5` is rejected. Edge weights are passed to the same core network
builder as the text formats, so weights `<= 0` are ignored (not an error); node
and state weights must be non-negative. The JSON Schema in
`test/schemas/json/infomap-network-json.schema.json` is the normative
specification of what the parser accepts.

## Building incrementally with Network

When you assemble a network programmatically, read a custom file format, or wire
up a handful of edges, use {class}`~infomap.Network` directly. Its ``add_*``
verbs return the network, so calls chain, and {func}`infomap.run` takes the
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
an optional label; `add_link` creates any node it references
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

## Pitfalls

- **Read-time options belong on the constructor, not the run.** `weight`,
  `edge_weights`, `directed`, and `weighted` live on the `Network.from_*`
  constructors; {func}`infomap.run` raises if you pass one, naming the
  constructor to use.
- **SciPy and edge-index routes default to different directedness.** A SciPy
  sparse matrix is read as *undirected* unless you pass `directed=True`, while a
  `(2, E)` integer edge index follows the PyG convention and is read as
  *directed*. NetworkX and igraph instead take directedness from the graph
  object. Spell the flag out when it matters.
- **A two-row integer array is an edge index, not two links.** Rows of
  `(source, target, weight)` need a float weight column to be read as links; a
  bare 2×E integer array is interpreted as a `(2, E)` edge index (and so as
  directed). Use `Network.from_edge_index(..., directed=False)` if that is not
  what you meant.
- **The raw edge-list route registers no node names.** Running a NumPy array or
  tuples directly stores no labels, so the result's `"name"` column cannot
  recover string ids. Convert labels with `pandas.factorize` first and keep the
  `uniques` array as your reverse mapping, or build a {class}`~infomap.Network`
  (whose {attr}`~infomap.Network.node_id_to_label` records the mapping). Source
  and target columns must hold integers.

## API pointers

- {func}`infomap.run` dispatches on the input type and returns an immutable
  {class}`~infomap.Result`.
- {class}`infomap.Network` builds a network explicitly. Its constructors
  {meth}`~infomap.Network.from_networkx`, {meth}`~infomap.Network.from_igraph`,
  {meth}`~infomap.Network.from_scipy_sparse_matrix`,
  {meth}`~infomap.Network.from_edge_index`, and
  {meth}`~infomap.Network.from_file` take the adapter options that
  {func}`infomap.run` does not.
- {meth}`infomap.Network.add_node`, {meth}`infomap.Network.add_link`, and
  {meth}`infomap.Network.add_links` build a network incrementally.
- {func}`infomap.find_communities` and {func}`infomap.find_igraph_communities`
  are one-shot helpers returning a NetworkX ``list[set]`` and an
  ``igraph.VertexClustering`` respectively.
- {meth}`infomap.Result.modules` returns `{node_id: module_id}`;
  {meth}`infomap.Result.to_dataframe` returns node id, module id, flow, path,
  and name as a DataFrame.

## Going deeper

- **Full API reference**: {doc}`/api/index` gives complete signatures for every
  entry point above.
- **Richer network representations**: {doc}`/flow-models/index` covers data with
  memory, layers, or explicit state nodes.
- The survey (§4) treats flow modelling in depth, including undirected versus
  directed links {cite:p}`smiljanic2026survey`.
