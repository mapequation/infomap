---
jupytext:
  text_representation:
    format_name: myst
kernelspec:
  display_name: Python 3
  language: python
  name: python3
---

# Bipartite networks

{bdg-warning-line}`Flow model`

```{admonition} At a glance
:class: tip
A bipartite network has two node types with links only between them. Declare
the boundary with `bipartite_start_id` and Infomap models the alternating
random walk directly, returning modules that mix both node types.
```

## Two node types, links between them

Many networks connect two distinct kinds of entities: users rating movies,
species visiting flowers, authors writing papers. Links go exclusively
*between* the two types, never within them. This is a bipartite network.

A common workaround is to project one type onto the other, replacing
user–item links with user–user links whenever two users share an item.
The projection densifies the network and conflates distinct interaction
patterns into weighted cliques. Running Infomap on the bipartite network
itself avoids that loss: the walk alternates between the types, and the
communities keep both types together.

## How Infomap handles the two types

You assign integer node ids so that all type-A nodes come first and all
type-B nodes start at a threshold id, and declare that threshold by setting
`bipartite_start_id`. Infomap then models the alternating walk with a
two-step encoding: each move A → B → A counts as one step between type-A
nodes, and by default the type-B nodes' visit rates are folded onto the
type-A side. The codelength therefore describes the flow among the type-A
nodes, with the type-B nodes acting as the contexts that route it. Both
types still receive module assignments.

```{admonition} Which type goes first?
:class: note
Put the nodes whose communities you care about below the threshold: their
flow is what the code describes. If you also write native output files with
the stateful {class}`~infomap.Infomap`, `hide_bipartite_nodes=True` drops the
other type from those files (it does not filter the in-memory
{class}`~infomap.Result`).
```

Two engine options change this treatment, passed via
{class}`~infomap.Options` (e.g. `run(net, options=Options(skip_adjust_bipartite_flow=True))`):
`skip_adjust_bipartite_flow=True` keeps flow on the type-B nodes so both types
are coded, and `bipartite_teleportation=True` makes a directed run teleport with
bipartite-aware jumps instead of the default two-step unipartite scheme.

## Users and items in shared clusters

The scenario: four users and four items. Users 0 and 1 strongly prefer
items 4 and 5; users 2 and 3 strongly prefer items 6 and 7. A few weak
cross-cluster links add noise. We would like Infomap to recover the two
preference clusters, each containing two users and two items.

### Set up the network

```{code-cell} python
import infomap
from infomap import Network, run

# Node-id convention:
#   Users  → ids 0, 1, 2, 3         (type A: "left")
#   Items  → ids 4, 5, 6, 7         (type B: "right")
#
# All type-B ids must be >= BIPARTITE_START.
BIPARTITE_START = 4

net = Network()

# Declare the bipartite boundary: everything from id 4 onwards is type B.
net.bipartite_start_id = BIPARTITE_START

# Cluster 1: users {0,1} prefer items {4,5}
net.add_link(0, 4, 3.0)
net.add_link(0, 5, 2.0)
net.add_link(1, 4, 2.0)
net.add_link(1, 5, 3.0)

# Cluster 2: users {2,3} prefer items {6,7}
net.add_link(2, 6, 3.0)
net.add_link(2, 7, 2.0)
net.add_link(3, 6, 2.0)
net.add_link(3, 7, 3.0)

# Weak cross-cluster signal
net.add_link(0, 6, 0.2)
net.add_link(2, 4, 0.2)

print(f"Bipartite start id: {net.bipartite_start_id}")
print(f"Nodes with id >= {BIPARTITE_START} are treated as type-B (items).")
```

### Run and inspect modules

```{code-cell} python
result = run(net, two_level=True, num_trials=10, seed=123)

modules = result.modules()   # {node_id: module_id}

print(f"Bipartite start id:      {net.bipartite_start_id}")
print(f"Top-level modules found: {result.num_top_modules}")
print(f"Codelength (bits/step):  {result.codelength:.4f}\n")

for node_id, module_id in sorted(modules.items()):
    kind = "user" if node_id < BIPARTITE_START else "item"
    print(f"  node {node_id:2d} ({kind:4s})  →  module {module_id}")
```

Each module mixes the two types: users {0, 1} group with items {4, 5}, and
users {2, 3} with items {6, 7}, even though no user–user or item–item links
exist.

### With and without the declaration

Declaring the boundary changes the *flow model*, not the data. Run the same
links without setting `bipartite_start_id`, and Infomap treats them as an
ordinary unipartite network instead:

```{code-cell} python
net_std = Network()
for u, v, w in [
    (0, 4, 3.0), (0, 5, 2.0), (1, 4, 2.0), (1, 5, 3.0),
    (2, 6, 3.0), (2, 7, 2.0), (3, 6, 2.0), (3, 7, 3.0),
    (0, 6, 0.2), (2, 4, 0.2),
]:
    net_std.add_link(u, v, w)
result_std = run(net_std, two_level=True, num_trials=10, seed=123)

print(f"unipartite run: {result_std.num_top_modules} modules, L={result_std.codelength:.4f}")
print(f"bipartite run:  {result.num_top_modules} modules, L={result.codelength:.4f}")
```

The two codelengths are not directly comparable, because the runs describe
different walks: the bipartite run codes only the two-step flow between
users, the unipartite run codes every node. On this small network both find
the same two clusters; on larger networks the declaration changes the flow
each node carries and can move module boundaries.

### Visualise

Lay the two node types out as two columns, users on the left and items on the
right, so the community structure reads as horizontal bands.

```{code-cell} python
import matplotlib.pyplot as plt
from myst_nb import glue

import networkx as nx
from docs_viz import draw_partition

# Build a NetworkX graph matching the Infomap network above
G = nx.Graph()
G.add_nodes_from(range(8))
for u, v, w in [
    (0, 4, 3.0), (0, 5, 2.0), (1, 4, 2.0), (1, 5, 3.0),
    (2, 6, 3.0), (2, 7, 2.0), (3, 6, 2.0), (3, 7, 3.0),
    (0, 6, 0.2), (2, 4, 0.2),
]:
    G.add_edge(u, v, weight=w)

# Two columns: users left, items right. The default bipartite run folds item
# flow onto the users, so items would draw as near-invisible dots if sized by
# flow; size every node the same and let colour carry the module structure.
pos = {u: (0.0, 3 - u) for u in range(4)}
pos.update({v: (2.2, 3 - (v - 4)) for v in range(4, 8)})

fig, ax = plt.subplots(figsize=(5.4, 4.2))
draw_partition(G, modules, ax=ax, pos=pos, with_labels=True, node_size=620)
ax.text(0.0, 3.7, "users", ha="center", color="0.35", weight="bold")
ax.text(2.2, 3.7, "items", ha="center", color="0.35", weight="bold")
ax.set_xlim(-0.6, 2.8)
ax.set_ylim(-0.6, 4.0)
fig.tight_layout()
glue("fig-bipartite", fig, display=False)
plt.close(fig)
```

```{glue:figure} fig-bipartite
The bipartite network as two columns, users on the left and items on the right,
coloured by module. Each module is a band spanning both types: users {0, 1} with
items {4, 5}, users {2, 3} with items {6, 7}. The thin diagonals are the two
weak cross-cluster links.
```

## API pointers

Declare the split by setting {attr}`~infomap.Network.bipartite_start_id` (or
{attr}`~infomap.Infomap.bipartite_start_id`) before running, and link opposite
types with {meth}`~infomap.Network.add_link`; {meth}`~infomap.Result.modules`
covers both types. The bipartite engine options (`hide_bipartite_nodes`,
`skip_adjust_bipartite_flow`, `bipartite_teleportation`) are in the Options
table below.

## Options

| Option | Where | Effect |
|---|---|---|
| `bipartite_start_id` | {class}`~infomap.Network` attribute | First node id of the type-B block (the second type); declares the network bipartite |
| `hide_bipartite_nodes` | {class}`~infomap.Infomap` (file-writing runs) | Omit the type-B nodes from written output files (the in-memory result keeps both types) |
| `skip_adjust_bipartite_flow` | {class}`~infomap.Options` → {func}`infomap.run` | Keep flow on the type-B nodes instead of folding it onto type-A |
| `bipartite_teleportation` | {class}`~infomap.Options` → {func}`infomap.run` | On directed runs, teleport with bipartite-aware jumps instead of the two-step unipartite scheme |

## Going deeper

- {cite:t}`blocker2020bipartite` analyse coding schemes that use the node-type
  information explicitly, giving each module separate codebooks per type, and
  quantify the compression gains across 21 real bipartite networks.
- The survey (§6.2) covers bipartite networks {cite:p}`smiljanic2026survey`; its
  companion notebook `examples/notebooks/6.2 Bipartite Networks.ipynb` applies
  both coding schemes to a real weighted plant–ant web (Fonseca & Ganade 1996).
