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
*save* it. This chapter covers the docs visualisation helper, two native file
formats (`.tree` and `.clu`), and how to annotate and export your graph as
GraphML or GEXF for Gephi, Cytoscape, and similar tools.
```

## Motivation

Running Infomap is only half the job. The partition lives in memory as a
mapping from node ids to module ids, but that dictionary is hard to reason
about on its own. You need two things:

1. **A picture**: a network layout where every module gets a distinct colour, so
   you can see at a glance whether the detected communities match your intuition
   about the data.
2. **Persistent output**: files your collaborators, downstream scripts, and
   visualisation tools can open without re-running the algorithm.

Infomap supports several export formats. The two native text formats, `.tree`
and `.clu`, are compact and easy to parse. To embed the partition inside a full
graph file (topology plus attributes in one document), the Python package
provides helpers that annotate NetworkX or igraph graphs before writing them as
GraphML or GEXF.

## Intuition

Think of the partition as a layer of paint over your network. The visualization
helper applies that paint: each module gets one colour from a palette, nodes sit
where a spring layout places them, and the edges fade into the background so the
clusters stand out.

The export formats take different cuts through the same result:

- `.tree` is the full hierarchical tree. Every node appears with its path from
  the root (`1:3` means module 1, position 3 within it), its flow, its name, and
  its id. It is the most informative single-file summary.
- `.clu` is the simplest format: three columns per data line, namely node id,
  module, and flow. It loads with pandas or numpy in one line.
- **GraphML / GEXF** attach the module assignment as a node attribute inside a
  standard graph interchange file. Open it in Gephi and colour nodes by
  `infomap_module` for an interactive view of the same partition.

## A worked example

We use the Zachary karate club throughout this chapter (34 people, 78
friendships, a well-known fission into factions) because it is small enough to
explore interactively and produces a legible figure.

### Build the network and run Infomap

```{code-cell} python
import networkx as nx
import infomap

g = nx.karate_club_graph()

im = infomap.Infomap(two_level=True, seed=123, num_trials=10, silent=True)
mapping = im.add_networkx_graph(g)   # keep mapping for export helpers
im.run()

modules = im.get_modules()           # {node_id: module_id}
print(f"Nodes: {g.number_of_nodes()}, Edges: {g.number_of_edges()}")
print(f"Top-level modules: {im.num_top_modules}")
print(f"Codelength: {im.codelength:.4f} bits/step")
```

### Visualise the partition

The docs ship a small helper, `draw_partition`, that handles module colouring in
one call. You do not need to install anything extra; it lives in the docs source
tree and is importable in all executed documentation pages.

```{code-cell} python
import matplotlib.pyplot as plt
from myst_nb import glue

from docs_viz import draw_partition

# Marker area scales with flow, so a node's radius grows as the square root of
# how often the random walk visits it.
flow = {n.node_id: n.data.flow for n in im.nodes}
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

### Export to .tree and .clu

The two native text formats write directly from the `Infomap` object. Use
`tempfile` so nothing lands in the repo.

```{code-cell} python
import os
import tempfile

tmp = tempfile.mkdtemp()

# .tree: full hierarchy with flow values
tree_path = os.path.join(tmp, "karate.tree")
im.write_tree(tree_path)

# .clu: flat table with node_id, module, flow
clu_path = os.path.join(tmp, "karate.clu")
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
node listed inside top module 1. For hierarchical runs with more than two
levels you will see paths like `2:1:4`. The `.clu` format uses the top-level
module number only; combine with `--clu-level` (or the `depth_level` argument
to `write_clu`) to report a different level.

### Export to GraphML and GEXF

When you need a single file that bundles both the network topology and the
Infomap result, use the helpers in `infomap.export`. They annotate a copy of
your graph with module attributes and then call NetworkX's native writers.

```{code-cell} python
from infomap.export import write_graphml, write_gexf

graphml_path = os.path.join(tmp, "karate.graphml")
gexf_path    = os.path.join(tmp, "karate.gexf")

write_graphml(g, im, graphml_path, node_mapping=mapping)
write_gexf(g,   im, gexf_path,    node_mapping=mapping)

print(f"GraphML: {os.path.getsize(graphml_path):,} bytes → {graphml_path}")
print(f"GEXF:    {os.path.getsize(gexf_path):,} bytes → {gexf_path}")
```

Both files include three Infomap-specific node attributes by default:

| Attribute | Value |
|---|---|
| `infomap_module` | Top-level module id (string) |
| `infomap_path` | Colon-separated path, e.g. `"1"` or `"2:3"` |
| `infomap_level_1`, `infomap_level_2`, … | One attribute per hierarchy level |

Open the GraphML file in Gephi, select **Appearance → Nodes → Partition**, and
choose `infomap_module` to colour the graph by community.

Verify that the attribute appears in the XML:

```{code-cell} python
with open(graphml_path) as f:
    for line in f:
        if "infomap" in line:
            print(line.rstrip())
            break
```

#### Using the `find_communities` shortcut

For a one-liner workflow where you only need the NetworkX graph annotated
in-place, `infomap.find_communities` writes the module attribute directly onto
the graph and then returns the list of community sets. You can call
`nx.write_graphml` yourself afterwards:

```python
import networkx as nx
import infomap

g = nx.karate_club_graph()
communities = infomap.find_communities(
    g,
    module_attribute="infomap_module",
    seed=123,
    num_trials=10,
    silent=True,
)

nx.write_graphml(g, "karate_infomap.graphml")
nx.write_gexf(g,   "karate_infomap.gexf")

print(f"Found {len(communities)} communities")
```

This is convenient for exploratory work. For reusable pipelines, the explicit
`im.add_networkx_graph` / `im.run` / `write_graphml` pattern gives you more
control (re-running with different options, inspecting codelength, accessing
hierarchical paths).

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

**Native file formats**

- {meth}`infomap.Infomap.write_tree` writes a `.tree` file with the hierarchical
  path, flow, name, and node id for every node.
- {meth}`infomap.Infomap.write_clu` writes a `.clu` flat table; `depth_level=1`
  (default) gives top modules, higher values give sub-modules.
- {meth}`infomap.Infomap.write_flow_tree` writes a `.ftree` file that adds
  intra-module link flows for flow-based downstream analysis.

**Graph interchange formats**

- {func}`infomap.export.write_graphml` annotates and writes a GraphML file from a
  NetworkX or igraph graph.
- {func}`infomap.export.write_gexf` does the same for GEXF (NetworkX graphs only).
- {func}`infomap.export.annotate_networkx_graph` annotates and returns a graph
  copy without writing a file, for when you want to call a NetworkX writer
  yourself or inspect the attributes.
- {func}`infomap.find_communities` is the one-call shortcut that annotates the
  graph in place via `module_attribute` and returns community sets.

## Going deeper

- {doc}`/api/index` gives the complete export function signatures with every
  keyword argument.
- {cite:t}`smiljanic2026survey`, §4, sets flow-based community detection against
  modularity-based methods.
- The `.ftree` format (written by `im.write_flow_tree`) adds intra-module link
  flows and is the basis for alluvial diagrams on mapequation.org.
