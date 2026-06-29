---
jupytext:
  text_representation:
    format_name: myst
kernelspec:
  display_name: Python 3
  language: python
  name: python3
---

# Hierarchy and the multilevel map equation

```{admonition} In one sentence
:class: tip
The multilevel map equation lets Infomap discover how many levels of nested
structure your network contains, with no resolution parameter to tune.
```

## Motivation

Real networks rarely organise into one flat layer of modules. Scientific
disciplines hold sub-fields that hold research topics. Cities hold
neighbourhoods that hold blocks. Biological systems nest by definition: cells
inside tissues inside organs inside organisms. A plain two-level partition, one
index codebook over one set of leaf modules, sees only one cross-section of that
structure at a time.

This limit carries a measurable information-theoretic cost. {cite:t}`kawamoto2015resolution` showed that any two-level method has a *resolution limit*: modules below a
certain size (scaling exponentially with their internal link count) go missing,
because merging them into a larger module shortens the two-level description. In
a nested network the bind is plain. The two-level method must choose between
resolving the fine-grained modules and capturing the coarse super-groups; it
cannot hold both at once.

The multilevel map equation lifts the limit by allowing nested index codebooks
to any depth. It reads the *right number of levels* off the data: it adds a level
only when that level shortens the total description, so the depth of the output
tree reflects real structure rather than an analyst's choice. Infomap runs in
multilevel mode by default, so you opt out, not in.

## Intuition

Think of the two-level map as a street address with two parts: *city* and
*street*. That works for a small country. For the whole world you need at least
*country → city → street*, and for a large nation *country → state → city →
street*. Each extra level earns its place only when travellers stay inside the
smaller unit often enough that the added name pays for itself in shorter
directions overall.

The multilevel map equation applies the same logic to random-walk descriptions.
Each level of hierarchy is a set of nested index codebooks. When a walker stays
inside a module for a long time, giving that module its own sub-codebook for the
nodes it visits pays off. The same objective, description length, decides whether
to add another sub-level: Infomap keeps adding levels until none shortens the
description, then stops. No resolution parameter, no manual depth.

The result is a tree of modules. Each leaf is a network node. Internal tree
nodes are modules at different granularities. You can read off the coarse
super-group membership at depth 1, the fine-grained clique membership at
depth 2, and so on.

## Theory

The two-level map equation (see the [map equation chapter](the-map-equation.md))
uses one index codebook to describe inter-module movements and one codebook
per module to describe intra-module node visits:

$$
L(\mathsf{M}) =
  q_{\curvearrowright} H(\mathcal{Q})
  + \sum_{i=1}^{m} p_{\circlearrowright}^i H(\mathcal{P}^i).
$$

The hierarchical extension {cite:p}`rosvall2011multilevel` replaces the single
index codebook with a *tree* of nested index codebooks. Each module $i$ at
the top level has its own sub-index codebook $\mathcal{Q}^i$ for directing
the random walker among its sub-modules, and those sub-modules may have
their own sub-sub-index codebooks, and so on. The total description length
becomes a recursive sum:

$$
L(\mathsf{M}) =
  q_{\curvearrowright} H(\mathcal{Q})
  + \sum_{i=1}^{m} L(\mathsf{M}^i),
$$

where each sub-map $\mathsf{M}^i$ contributes

$$
L(\mathsf{M}^i) =
  q_{\circlearrowright}^i H(\mathcal{Q}^i)
  + \sum_{j=1}^{m^i} L(\mathsf{M}^{ij}),
$$

and the recursion bottoms out at the leaf level with

$$
L(\mathsf{M}^{ij\ldots k}) =
  p_{\circlearrowright}^{ij\ldots k} H(\mathcal{P}^{ij\ldots k}).
$$

The point is that $q_{\circlearrowright}^i$, the rate at which the walker uses
module $i$'s sub-index codebook, depends only on the *local* traffic within
module $i$, not on the global network. That is why the resolution limit relaxes
so far: whether to split module $i$ further is judged against the traffic inside
$i$, not against a global cut size that grows with the whole network (Kawamoto &
Rosvall 2015, §IV).

Infomap searches for the hierarchical partition that minimises the total $L$
using a fast stochastic recursive algorithm: it first generates top-level
modules, then recursively applies the same procedure inside each module to
find sub-modules, adding or removing levels wherever the description
shortens. The search converges to a locally optimal hierarchical tree with
no depth parameter required.

:::{toggle}
**Resolution limit: why two-level fails and hierarchy helps**

{cite:t}`kawamoto2015resolution` derived the resolution limit of the two-level map
equation analytically. For an undirected network with cut size $C$ (total
number of links crossing module boundaries), the two-level method can fail
to detect a module with $l_c$ internal links when

$$
\frac{4^{l_c}}{l_c + 1} \lesssim C.
$$

Contrast this with modularity, where the limit scales as $\sqrt{L}$ with
total link count $L$. The map equation's dependence on the cut size $C$
rather than $L$ is already much less restrictive, but it does not vanish.

In the *hierarchical* map equation the analogous update is evaluated with
the effective network size equal to the super-module plus its boundary
links, not the full graph. Because a super-module is much smaller than the
full network, the cut size against which a fine-level split is judged is
tiny, and fine-level modules that would be swallowed at the two-level stage
are correctly resolved. For networks with a pronounced nested structure the
resolution limit of the hierarchical method is effectively eliminated
(Kawamoto & Rosvall 2015, §IV).
:::

## A worked example

We construct a toy network with a clear two-level hierarchy and let Infomap
find it automatically. The network has four cliques of five nodes each,
arranged in two super-groups: cliques A1 and A2 are densely connected to
each other (forming super-group A), cliques B1 and B2 are densely connected
to each other (forming super-group B), and only a single bridge link connects
the two super-groups.

### Build the network

```{code-cell} python
import networkx as nx
import infomap

G = nx.Graph()

def add_clique(G, nodes):
    """Add all edges of a complete subgraph."""
    for i, u in enumerate(nodes):
        for v in nodes[i + 1:]:
            G.add_edge(u, v)

# Four cliques of five nodes each
A1 = list(range(0, 5))
A2 = list(range(5, 10))
B1 = list(range(10, 15))
B2 = list(range(15, 20))

for clique in [A1, A2, B1, B2]:
    add_clique(G, clique)

# Dense inter-clique edges within each super-group
for i in range(3):
    G.add_edge(A1[i], A2[i])   # A1 ↔ A2
for i in range(3):
    G.add_edge(B1[i], B2[i])   # B1 ↔ B2

# Single sparse bridge between super-groups
G.add_edge(A1[0], B1[0])

print(f"Nodes: {G.number_of_nodes()}, Edges: {G.number_of_edges()}")
```

### Run multilevel Infomap

Infomap runs in multilevel mode by default; no flag required. Do **not** pass
`two_level=True`, which restricts the output to a single flat level and stops
Infomap from finding deeper structure.

```{code-cell} python
im = infomap.Infomap(silent=True, num_trials=10, seed=123)
for u, v in G.edges():
    im.add_link(u, v)
im.run()

print(f"Tree depth (num_levels): {im.num_levels}")
print(f"Top-level modules:       {im.num_top_modules}")
print(f"Map equation codelength: {im.codelength:.4f} bits/step")
```

Infomap determined that this network has **3 levels** entirely on its own:
a root, two coarse super-groups, and four fine cliques underneath them.
No depth argument was passed.

### Read module assignments at each level

```{code-cell} python
# Coarsest level (depth 1): the two super-groups A and B
modules_l1 = im.get_modules(depth_level=1)

# Finer level (depth 2): the four individual cliques
modules_l2 = im.get_modules(depth_level=2)

print("Level-1 assignment (super-groups):")
print(modules_l1)

print("\nLevel-2 assignment (cliques):")
print(modules_l2)
```

`get_modules(depth_level=1)` returns the coarsest grouping: two modules of ten
nodes each, matching super-groups A and B. `get_modules(depth_level=2)` returns
the finer partition, four modules of five nodes each, one per clique. Pass
`depth_level=-1` for the finest (leaf) level whatever the tree depth.

### Visualise both levels side by side

```{code-cell} python
from myst_nb import glue

import matplotlib.pyplot as plt
from docs_viz import draw_partition

fig, axes = plt.subplots(1, 2, figsize=(9, 4), layout="constrained")

# Same flow in both panels; only the level of the grouping differs.
flow = {n.node_id: n.data.flow for n in im.nodes}

draw_partition(G, modules_l1, ax=axes[0], flow=flow)
axes[0].set_title("Level 1: super-groups", fontsize=11)

draw_partition(G, modules_l2, ax=axes[1], flow=flow)
axes[1].set_title("Level 2: cliques", fontsize=11)

fig.suptitle("Two levels of hierarchy, discovered automatically", fontsize=12)
glue("fig-hierarchy-and-the-multilevel-map", fig, display=False)
plt.close(fig)
```

```{glue:figure} fig-hierarchy-and-the-multilevel-map
The same nested network at two levels of the hierarchy Infomap discovers.
Left: the two coarse super-groups. Right: the four fine cliques nested
inside them. Both panels share one layout, so the levels read against each
other.
```

The left panel shows the two super-groups: all ten nodes in super-group A
share one colour, all ten in super-group B share another. The right panel
reveals the finer structure: each clique gets its own colour. Both views are
consistent descriptions of the *same* Infomap result, read at different depths
of the output tree.

This matches the theoretical picture from {cite:t}`rosvall2011multilevel`: the
network has dense intra-clique flow, moderately dense intra-super-group flow
(clique pairs), and sparse inter-super-group flow (the single bridge). Three
levels of nested index codebooks capture exactly those three scales, and the
hierarchical map equation confirms that the three-level description is more
compressed than either a flat two-level partition or a one-level description.

## API pointers

- {attr}`infomap.Infomap.num_levels` gives the tree depth Infomap discovered
  (alias `max_depth`), so you can read the number of levels without walking the
  tree.
- {attr}`infomap.Infomap.num_top_modules` counts the modules at level 1, the
  coarsest partition.
- {meth}`infomap.Infomap.get_modules` returns a `{node_id: module_id}` dict.
  Its `depth_level` selects the level:
  - `depth_level=1` (default) gives the top modules.
  - `depth_level=2, 3, …` give progressively finer levels.
  - `depth_level=-1` gives the leaf (finest) modules.
- {meth}`infomap.Infomap.get_effective_num_modules` gives the flow-weighted
  effective number of modules at a `depth_level`, which helps when module sizes
  are uneven and a plain count overstates them.
- Constructor argument `two_level=True` restricts the search to two levels.
  **Leave it off** for the full hierarchical result.

## Going deeper

- {cite:t}`smiljanic2026survey`, §3, covers the multilevel derivation and worked
  examples.
- {cite:t}`rosvall2011multilevel` is the source paper for the multilevel map equation.
- {cite:t}`kawamoto2015resolution` derives the resolution limit it relaxes.
