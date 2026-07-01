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

```{admonition} In one sentence
:class: tip
Once Infomap has partitioned your network, you want to *see* the structure and
*save* it. This chapter covers the docs visualisation helper, the in-memory
graph export (`to_networkx` / `to_igraph` for GraphML and GEXF), and the native
`.tree` / `.clu` formats written by the stateful Infomap.
```

## Motivation

Running Infomap is only half the job. The partition lives in memory on the
:class:`~infomap.Result`, but a mapping from node ids to module ids is hard to
reason about on its own. You need two things:

1. **A picture**: a network layout where every module gets a distinct colour, so
   you can see at a glance whether the detected communities match your intuition
   about the data.
2. **Persistent output**: files your collaborators, downstream scripts, and
   visualisation tools can open without re-running the algorithm.

The cleanest export turns the result into an annotated graph with
:func:`infomap.to_networkx` or :func:`infomap.to_igraph`, which you then write
with the graph library's own writer (GraphML, GEXF). The stateful
:class:`~infomap.Infomap` writes the engine's native text formats, `.tree` and
`.clu`.

## Intuition

Think of the partition as a layer of paint over your network. The visualisation
helper applies that paint: each module gets one colour, nodes sit where a spring
layout places them, and the edges fade into the background so the clusters stand
out.

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

## A worked example

We use the Zachary karate club throughout this chapter (34 people, 78
friendships, a well-known fission into factions) because it is small enough to
explore interactively and produces a legible figure.

### Run Infomap

```{code-cell} python
import networkx as nx
import infomap

g = nx.karate_club_graph()

result = infomap.run(g, two_level=True, seed=123, num_trials=10, silent=True)
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

Each colour represents one top-level module. The spring layout pulls tightly
connected groups together, so the colour boundaries usually line up with the
visual gaps in the drawing.

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

The helper is intentionally short (~40 lines). For production figures, copy
`interfaces/python/source/_ext/docs_viz.py` and adapt the palette, layout
algorithm, edge styling, and node labels to your taste.

### Export to GraphML and GEXF

For a single file bundling the network topology and the Infomap result,
:func:`infomap.to_networkx` returns a copy of the graph annotated with the
partition, which NetworkX then writes:

```{code-cell} python
import os
import tempfile
from infomap import to_networkx

tmp = tempfile.mkdtemp()

annotated = to_networkx(result)   # adds infomap_module, infomap_path, flow, ...
graphml_path = os.path.join(tmp, "karate.graphml")
gexf_path = os.path.join(tmp, "karate.gexf")
nx.write_graphml(annotated, graphml_path)
nx.write_gexf(annotated, gexf_path)

print(f"GraphML: {os.path.getsize(graphml_path):,} bytes")
print(f"GEXF:    {os.path.getsize(gexf_path):,} bytes")
```

The annotated graph carries these Infomap node attributes:

| Attribute | Value |
|---|---|
| `infomap_module` | Top-level module id (string) |
| `infomap_path` | Colon-separated path, e.g. `"1"` or `"2:3"` |
| `infomap_level_1`, `infomap_level_2`, … | One attribute per hierarchy level |
| `flow` | Stationary visit frequency |

Open the GraphML file in Gephi, select **Appearance → Nodes → Partition**, and
choose `infomap_module` to colour the graph by community.

```{code-cell} python
with open(graphml_path) as f:
    for line in f:
        if "infomap_module" in line:
            print(line.strip())
            break
```

For igraph users, :func:`infomap.to_igraph` returns the same annotation on an
`igraph.Graph`.

### Export to .tree and .clu

The stateful :class:`~infomap.Infomap` writes the engine's native text formats:
build one, run it, and call its `write_*` methods.
These formats feed the mapequation.org Network Navigator and alluvial diagrams.

```{code-cell} python
im = infomap.Infomap(two_level=True, seed=123, num_trials=10, silent=True)
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
:func:`infomap.find_communities` writes the module attribute directly onto the
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

## API pointers

**Visualisation**

- {func}`docs_viz.draw_partition` draws a module-coloured spring layout and
  accepts an existing `ax` for multi-panel figures.

**In-memory graph export**

- {func}`infomap.to_networkx` returns a NetworkX copy annotated with
  `infomap_module`, `infomap_path`, per-level ids, and `flow`; write it with
  `nx.write_graphml` / `nx.write_gexf`.
- {func}`infomap.to_igraph` returns the same annotation on an `igraph.Graph`.
- {func}`infomap.export.write_graphml`, {func}`infomap.export.write_gexf`, and
  {func}`infomap.export.annotate_networkx_graph` are convenience wrappers around
  that pattern.

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
- {cite:t}`smiljanic2026survey`, §4, sets flow-based community detection against
  modularity-based methods.
- The `.ftree` format adds intra-module link flows and is the basis for alluvial
  diagrams on mapequation.org.
