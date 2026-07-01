---
jupytext:
  text_representation:
    format_name: myst
kernelspec:
  display_name: Python 3
  language: python
  name: python3
---

# Reading Infomap through Louvain and Leiden

{bdg-info-line}`Concept`

```{admonition} In one sentence
:class: tip
If you already use Louvain or Leiden, you can read Infomap through the same lens:
all three partition a network, but Infomap optimises the compression of flow
where Louvain and Leiden optimise modularity.
```

## Same job, a different objective

Louvain and Leiden are the community-detection methods most researchers meet
first, and both return a partition of your nodes just as Infomap does. This page
is a translation, not a benchmark: it maps Infomap onto ideas you already have,
so you can read its output with the intuition you built on modularity.

One difference explains the rest, and it is the *objective*. Louvain and Leiden
score a partition by **modularity**: whether more edges fall inside groups than a
random graph with the same degrees would predict. Infomap scores it by the **map
equation**: how few bits describe a random walk on the network under that
partition (see {doc}`/concepts/the-map-equation`). Modularity asks how the network
is wired; the map equation asks how it is used.

Running both is a useful sanity check. Where the two lenses agree, the grouping is
robust to the choice of objective; where they differ, you have found a region
whose grouping depends on what you mean by "community", which is worth knowing.

## What each objective optimises

**Modularity (Louvain, Leiden).** Both are usually run to maximise modularity, a
static-density score against a degree-preserving null model. That null model is
undirected, so by default both ignore edge direction. Leiden is a general
optimiser: it maximises whichever quality function you give it, adds a refinement
step that guarantees internally connected communities, and reaches better optima
than Louvain's greedy moves {cite:p}`traag2019leiden`. This page uses the
modularity objective for both, the most common default.

**The map equation (Infomap).** Infomap compresses a description of a random walk
(see {doc}`/concepts/the-map-equation`). A group is worth naming when a walker
entering it tends to stay for several steps before leaving. Because the walk
follows edge direction and weight, Infomap reads those where an undirected
modularity null model does not.

## Modularity and the map equation

Modularity for a partition $\mathsf{C}$ is

$$
Q = \frac{1}{2m} \sum_{i,j} \left[ A_{ij} - \frac{k_i k_j}{2m} \right]
\delta(c_i, c_j),
$$

where $A_{ij}$ is the adjacency matrix, $k_i$ is degree, and $m$ is the number of
edges. The map equation $L(\mathsf{M})$ is instead

$$
L(\mathsf{M}) =
  q_{\curvearrowright} H(\mathcal{Q})
  + \sum_{i=1}^{m} p_{\circlearrowright}^i H(\mathcal{P}^i),
$$

the average cost of coding a random walk: bits for *crossing* module boundaries
plus bits for *moving inside* them. One reads structure off edge density; the
other off the dynamics the edges carry.

:::{toggle}
**A note on the resolution limit**

Modularity has a resolution limit: it tends not to separate communities smaller
than roughly $\sqrt{2m}$ edges, however cohesive they are
{cite:p}`fortunato2007resolution`. Leiden can sidestep it with a
resolution-limit-free objective such as the Constant Potts Model, or by tuning its
`resolution_parameter`. The map equation has a resolution limit too, weaker but
not absent ({doc}`/concepts/hierarchy-and-the-multilevel-map` derives the bound);
tune its scale with `markov_time` or `preferred_number_of_modules`. These are
properties of the objectives, not verdicts on them.
:::

## The same network through three lenses

The clearest way to build intuition is to look at one network under each lens.

```{code-cell} python
import networkx as nx
import infomap
import igraph as ig

# A network with clear community structure.
g = nx.planted_partition_graph(4, 12, 0.7, 0.05, seed=123)

# Infomap: compress the flow
result = infomap.run(g, two_level=True, seed=123, num_trials=10, silent=True)
infomap_mods = result.modules()

# Louvain and Leiden: maximise modularity
louvain = nx.community.louvain_communities(g, seed=123)
louvain_mods = {n: c for c, comm in enumerate(louvain) for n in comm}

ig_g = ig.Graph(edges=list(g.edges()), n=g.number_of_nodes(), directed=False)
leiden = ig_g.community_leiden(objective_function="modularity", n_iterations=10)
leiden_mods = {n: m for n, m in enumerate(leiden.membership)}

print(f"Infomap (flow):        {len(set(infomap_mods.values()))} groups")
print(f"Louvain (modularity):  {len(louvain)} groups")
print(f"Leiden  (modularity):  {len(leiden)} groups")
```

```{code-cell} python
import matplotlib.pyplot as plt
from myst_nb import glue
from docs_viz import draw_partition

fig, axes = plt.subplots(1, 3, figsize=(11, 3.6), layout="constrained")
for ax, (name, mods) in zip(
    axes,
    [("Infomap (flow)", infomap_mods),
     ("Louvain (modularity)", louvain_mods),
     ("Leiden (modularity)", leiden_mods)],
):
    draw_partition(g, mods, ax=ax)
    ax.set_title(name, fontsize=11)
fig.suptitle("The same network through three lenses", fontsize=12)
glue("fig-choosing-a-method", fig, display=False)
plt.close(fig)
```

```{glue:figure} fig-choosing-a-method
One network partitioned by each objective. On clearly separated structure the
flow lens and the modularity lens tend to carve it the same way; the group counts
print above. Colours identify groups within a panel, not across panels.
```

On well-separated, undirected structure like this the objectives largely coincide:
compressing the flow and maximising within-group density point at the same groups.
That is the common case, and the agreement is reassuring rather than surprising.

## Where the lenses read a network differently

Two situations are worth flagging to a Louvain or Leiden user, because they are
where Infomap's answer will differ, and why:

- **Directed flow.** Infomap follows edge direction; the standard modularity null
  model does not. On a network with real flow asymmetry, a citation cascade or a
  web subgraph that mostly *sends* links one way, the random walk concentrates
  where undirected edge density alone does not, so a directed Infomap run reads
  the structure differently {cite:p}`rosvall2008maps`.
- **Scale.** The two objectives have different resolution limits (see the toggle
  above), so on a network with many small groups they need not agree on how finely
  to divide it. Neither is more correct; they optimise different things.

Neither point is a benchmark. They are the two places where "community by flow"
and "community by density" genuinely mean different things, so knowing your
network tells you which lens to trust.

## API pointers

- {func}`infomap.run` returns an immutable {class}`~infomap.Result`; pass
  `directed=True` for directed-flow treatment and `two_level=True` for a flat
  partition.
- {func}`infomap.find_communities` returns a NetworkX-style `list[set]`, and
  {func}`infomap.find_igraph_communities` returns an `igraph.VertexClustering`, so
  an Infomap result plugs into igraph's own inspection and plotting utilities
  alongside Louvain and Leiden.

## Going deeper

- {doc}`/concepts/the-map-equation` derives why Infomap minimises description
  length rather than maximising modularity.
- Companion notebooks: `examples/notebooks/compare-infomap-louvain-networkx.ipynb`
  and `examples/notebooks/compare-infomap-louvain-leiden-igraph.ipynb` show the
  side-by-side workflow with NetworkX and igraph.
- {cite:t}`traag2019leiden` introduces Leiden and shows Louvain can return
  internally disconnected communities.
- {cite:t}`fortunato2007resolution` is the original proof of modularity's
  resolution limit.
- {cite:t}`rosvall2008maps` contrasts the map equation and modularity on flow and
  non-flow networks.
- {cite:t}`smiljanic2026survey`, §3, treats the two in the broader survey.
