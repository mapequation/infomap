---
jupytext:
  text_representation:
    format_name: myst
kernelspec:
  display_name: Python 3
  language: python
  name: python3
---

# Visualising and exporting

{bdg-success-line}`How-to`

```{admonition} At a glance
:class: tip
This chapter covers the docs visualisation helper, the in-memory
graph export (`to_networkx` / `to_igraph` for GraphML and, via NetworkX, GEXF),
and the native `.tree` / `.clu` formats written by the stateful Infomap.
```

## A picture and a file

The partition lives in memory on the {class}`~infomap.Result`. This chapter
covers two ways to get it out:

1. **A picture**: a network layout where every module gets a distinct colour.
2. **Persistent output**: files that other tools and scripts can open without
   re-running the algorithm.

The in-memory export turns the result into an annotated graph with
{func}`infomap.to_networkx` or {func}`infomap.to_igraph`, which you then write
with the graph library's own writer (GraphML for both; GEXF is NetworkX-only). The stateful
{class}`~infomap.Infomap` writes the engine's native text formats, `.tree` and
`.clu`.

## Export formats

The export formats take different cuts through the same result:

- `to_networkx` / `to_igraph` attach the module assignment, hierarchical path,
  and flow as node attributes inside the graph object, ready for GraphML or GEXF.
  Open the file in Gephi and colour nodes by `infomap_module` for an interactive
  view of the same partition.
- `.tree` is the full hierarchical tree. Every node appears with its path from
  the root (`1:3` means module 1, position 3 within it), its flow, its name, and
  its id. It is the most informative single-file summary.
- `.clu` is the simplest format: node id, module, and flow per line. It loads
  with pandas or numpy in one line.
- `.ftree` is a `.tree` plus the intra-module link flows. It is what the
  Network Navigator reads to lay out and explore the modules.

## Colour and export the karate club

The examples below use the Zachary karate club (34 people, 78 friendships).

### Run Infomap

```{code-cell} python
import networkx as nx
import infomap

g = nx.karate_club_graph()

result = infomap.run(g, two_level=True, seed=123, num_trials=10)
modules = result.modules()           # {node_id: module_id}

print(f"Nodes: {g.number_of_nodes()}, Edges: {g.number_of_edges()}")
print(f"Top-level modules: {result.num_top_modules}")
print(f"Codelength: {result.codelength:.4f} bits/step")
```

### Visualise the partition

The docs ship a small helper, `draw_partition`, that handles module colouring in
one call. It lives in the docs source tree and is importable in all executed
documentation pages.

```{code-cell} python
import matplotlib.pyplot as plt
from myst_nb import glue

from docs_viz import draw_partition

# Marker area scales with flow, so a node's radius grows as the square root of
# how often the random walk visits it.
flow = {node.node_id: node.flow for node in result.nodes()}
fig = draw_partition(g, modules, flow=flow)
glue("fig-visualizing-and-exporting", fig, display=False)
plt.close(fig)
```

```{glue:figure} fig-visualizing-and-exporting
The example network coloured by top-level module. The spring layout pulls
tightly connected nodes together, so the colour boundaries line up with the
visual gaps in the drawing.
```

`draw_partition` accepts several optional keyword arguments that are useful when
you adapt the pattern in your own code. `seed` fixes the layout, `node_size` sets
the base marker area, and `flow` scales each marker so its radius grows as the
square root of the node's flow:

```python
from docs_viz import draw_partition
import matplotlib.pyplot as plt

fig, axes = plt.subplots(1, 2, figsize=(10, 5))
draw_partition(g, modules, seed=7, ax=axes[0], node_size=200, flow=flow)
axes[0].set_title("seed=7")
draw_partition(g, modules, seed=99, ax=axes[1], flow=flow)
axes[1].set_title("seed=99")
fig.tight_layout()
```

For production figures, copy
`interfaces/python/source/_ext/docs_viz.py` and adapt it.

### Export to GraphML and GEXF

For a single file bundling the network topology and the Infomap result,
{func}`infomap.to_networkx` (also available as
{meth}`result.to_networkx() <infomap.Result.to_networkx>`) builds a *new*
NetworkX graph from the result: nodes are the result's (state) nodes, keyed by
`state_id` and carrying the Infomap node `name` plus the partition attributes,
and edges come from the partitioned network. NetworkX then writes it:

```{code-cell} python
import os
import tempfile
from infomap import to_networkx

tmp = tempfile.mkdtemp()

annotated = to_networkx(result)   # new graph with infomap_module, infomap_path, flow, ...
graphml_path = os.path.join(tmp, "karate.graphml")
gexf_path = os.path.join(tmp, "karate.gexf")
nx.write_graphml(annotated, graphml_path)
nx.write_gexf(annotated, gexf_path)

print(f"GraphML: {os.path.getsize(graphml_path):,} bytes")
print(f"GEXF:    {os.path.getsize(gexf_path):,} bytes")
```

The exported graph carries these Infomap node attributes:

| Attribute | Value |
|---|---|
| `infomap_module` | Top-level module id (`int`) |
| `infomap_path` | Colon-separated tree path ending in the node's position within its module, e.g. `"1:1"` or `"2:3:4"` (`str`) |
| `infomap_level_1`, `infomap_level_2`, … | One attribute per path component (the last is the node's position within its final module) (`int`) |
| `flow` | Stationary visit frequency (`float`) |

GraphML and GEXF store the types alongside the values, so module ids and flows
come back as numbers when the file is read — Gephi's numeric colour scales and
sorting work directly.

Because the graph is rebuilt from the result, your original graph's node
attributes are *not* carried over. To keep them, annotate your own graph in
place with {func}`infomap.find_communities` (see below) or
{func}`infomap.io.export.annotate_networkx_graph`.

Open the GraphML file in Gephi, select **Appearance → Nodes → Partition**, and
choose `infomap_module` to colour the graph by community.

```{code-cell} python
with open(graphml_path) as f:
    for line in f:
        if "infomap_module" in line:
            print(line.strip())
            break
```

For igraph users, {func}`infomap.to_igraph` returns the same annotation on an
`igraph.Graph`.

### Export to .tree and .clu

The stateful {class}`~infomap.Infomap` writes the engine's native text formats:
build one, run it, and call its `write_*` methods.
These formats feed the mapequation.org tools: `.ftree` opens in the Network
Navigator, and `.tree`/`.clu` load in the alluvial diagram generator and other
scripts.

```{code-cell} python
im = infomap.Infomap(two_level=True, seed=123, num_trials=10)
im.add_networkx_graph(g)
im.run()

tree_path = os.path.join(tmp, "karate.tree")   # full hierarchy with flow
clu_path = os.path.join(tmp, "karate.clu")      # flat node_id, module, flow
im.write_tree(tree_path)
im.write_clu(clu_path)

print(f"Wrote {os.path.getsize(tree_path):,} byte tree file")
print(f"Wrote {os.path.getsize(clu_path):,} byte clu file")
```

Read the first few data lines of each file to see the format:

```{code-cell} python
def data_lines(path, n=6):
    """Return the first n non-comment lines of a file."""
    lines = []
    with open(path) as f:
        for line in f:
            if not line.startswith("#"):
                lines.append(line.rstrip())
            if len(lines) == n:
                break
    return lines

print("=== .tree (path  flow  name  node_id) ===")
for line in data_lines(tree_path):
    print(line)

print()
print("=== .clu  (node_id  module  flow) ===")
for line in data_lines(clu_path):
    print(line)
```

The `.tree` path column uses colon-separated integers: `1:3` means the third
node listed inside top module 1. For hierarchical runs with more than two levels
you will see paths like `2:1:4`. The `.clu` format uses the top-level module
number only; pass `depth_level` to `write_clu` to report a different level.
`im.write_flow_tree` writes a `.ftree` file that adds intra-module link flows.

### The find_communities shortcut

For a one-liner where you only need the NetworkX graph annotated in place,
{func}`infomap.find_communities` writes the module attribute directly onto the
graph and returns the community sets. Call a NetworkX writer yourself afterwards:

```python
import networkx as nx
import infomap

g = nx.karate_club_graph()
communities = infomap.find_communities(
    g,
    module_attribute="infomap_module",
    seed=123, num_trials=10, silent=True,
)
nx.write_graphml(g, "karate_infomap.graphml")
print(f"Found {len(communities)} communities")
```

### Clean up

```{code-cell} python
import shutil
shutil.rmtree(tmp)
print("Temp directory removed.")
```

## Pitfalls

- **Alluvial diagrams live on mapequation.org, not in this package.** Write a
  `.ftree` and open it in the Network Navigator; the docs helper draws static
  module-coloured layouts only.
- **`.clu` records the top level.** Pass `depth_level` to `write_clu` for a
  deeper level; `.tree` / `.ftree` carry the full hierarchy.
- **`to_networkx` builds a new graph, not a copy of yours.** Its nodes are the
  result's (state) nodes keyed by `state_id`, so your original graph is left
  untouched: its attributes are *not* carried into the export. Use
  {func}`infomap.find_communities` or
  {func}`infomap.io.export.annotate_networkx_graph` to annotate your own graph
  instead.

## API pointers

**Visualisation**

- `draw_partition` (the docs helper in `_ext/docs_viz.py`) draws a
  module-coloured spring layout and accepts an existing `ax` for multi-panel
  figures.

**In-memory graph export**

- {func}`infomap.to_networkx` builds a NetworkX graph from the result's (state)
  nodes, annotated with `infomap_module`, `infomap_path`, per-level ids, and
  `flow` (all strings); write it with `nx.write_graphml` / `nx.write_gexf`.
- {func}`infomap.to_igraph` builds the same graph as an `igraph.Graph`.
- {func}`infomap.io.export.write_graphml`, {func}`infomap.io.export.write_gexf`,
  and {func}`infomap.io.export.annotate_networkx_graph` instead annotate *your
  own* graph with the partition. They take a post-run stateful
  {class}`~infomap.Infomap`, not a {class}`~infomap.Result`. (`write_gexf`
  supports NetworkX only.)

**Native engine formats** (written by the stateful {class}`~infomap.Infomap`)

- {meth}`infomap.Infomap.write_tree` writes a `.tree` file with the hierarchical
  path, flow, name, and node id for every node.
- {meth}`infomap.Infomap.write_clu` writes a `.clu` flat table; pass
  `depth_level` to choose the level.
- {meth}`infomap.Infomap.write_flow_tree` writes a `.ftree` file that adds
  intra-module link flows.

## Going deeper

- {doc}`/api/index` gives the complete export signatures with every keyword
  argument.
- The survey (§4) sets flow-based community detection against modularity-based
  methods {cite:p}`smiljanic2026survey`.
- The `.ftree` format adds intra-module link flows and is the basis for alluvial
  diagrams on mapequation.org.
