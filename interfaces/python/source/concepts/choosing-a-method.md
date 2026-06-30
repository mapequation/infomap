---
jupytext:
  text_representation:
    format_name: myst
kernelspec:
  display_name: Python 3
  language: python
  name: python3
---

# Choosing a method: Infomap, Louvain, and Leiden

```{admonition} In one sentence
:class: tip
Infomap, Louvain, and Leiden each define "community" differently: Infomap finds
modules that trap random-walk flow, while Louvain and Leiden maximise modularity.
On directed or flow-structured networks those definitions diverge in ways that
matter for your analysis.
```

## Motivation

If you search for "community detection" in any network-science paper you will
find Louvain and Leiden cited alongside Infomap. All three are fast,
widely-used, and return a partition of your nodes. So which one should you use,
and how do you know when they agree?

The short answer: the right choice depends on what you mean by "community." If
your network represents real flows (web traffic, information spreading, transport
routes, citation chains), Infomap's flow-based objective is the natural match. If
your network captures static pairwise similarity and you want groups that are
dense internally relative to a null model, Louvain and Leiden fit better. On many
undirected networks the three methods agree closely. On directed networks, or
networks with strong flow asymmetry, they can produce qualitatively different
partitions.

Running all three and comparing their output is a useful sanity check regardless
of which you ultimately prefer. Agreement across objectives is evidence of
robust community structure; disagreement highlights ambiguous regions worth
examining.

## Intuition

**What Infomap optimises.** Infomap compresses a description of a random walk on
your network (the map equation). A module is real when a random walker entering
it tends to stay for several steps before leaving. Tight modules compress well;
loose ones do not. The method responds to edge direction and edge weights,
because they control where the random walk goes.

**What Louvain and Leiden optimise.** Both are usually run to maximise
*modularity*, a score that measures whether the fraction of within-community
edges exceeds the fraction you would expect from a random graph with the same
degree sequence. The standard modularity null model is undirected, so by default
both ignore edge direction. Leiden is in fact a general optimiser: it maximises
whichever quality function you hand it, and can instead optimise a
resolution-limit-free objective such as the Constant Potts Model. It also adds a
refinement step that guarantees internally connected communities and finds better
optima than Louvain's greedy moves {cite:p}`traag2019leiden`. This chapter uses
the modularity objective for both, the most common default.

**When they diverge.** On a directed graph where flow is asymmetric (say a web
subgraph where one cluster mostly *sends* links to another), Infomap picks up the
source cluster as a separate module, because the random walker rarely walks
*back* into it. Modularity ignores flow direction unless you pass a directed null
model, so it can miss this asymmetry. The *modularity resolution limit* is
another source of divergence: modularity merges small but well-defined
communities in large graphs regardless of how cohesive they are. The map equation
has a resolution limit too, but a much weaker one, so it resolves structure that
modularity blends (shown below).

## Theory

Modularity for a partition $\mathsf{C}$ is

$$
Q = \frac{1}{2m} \sum_{i,j} \left[ A_{ij} - \frac{k_i k_j}{2m} \right]
\delta(c_i, c_j),
$$

where $A_{ij}$ is the adjacency matrix, $k_i$ is degree, $m$ is the total
number of edges, and $\delta(c_i, c_j)$ is 1 when nodes $i$ and $j$ belong
to the same community. Louvain maximises $Q$ by greedy node moves; Leiden
adds a refinement phase that guarantees every returned community is internally
connected.

The map equation $L(\mathsf{M})$ (see {doc}`/concepts/the-map-equation` for the
full derivation) is instead

$$
L(\mathsf{M}) =
  q_{\curvearrowright} H(\mathcal{Q})
  + \sum_{i=1}^{m} p_{\circlearrowright}^i H(\mathcal{P}^i),
$$

where the two terms are the average coding cost for *crossing* module boundaries
and *staying inside* modules respectively. {cite:t}`rosvall2008maps` showed that
on networks where edges encode real flow, the map equation recovers communities
that reflect the flow dynamics, which a static-density score like modularity does
not capture.

:::{toggle}
**Resolution limit and scale**

Modularity has a resolution limit: communities smaller than roughly
$\sqrt{2m}$ edges tend to be merged regardless of how internally cohesive they
are {cite:p}`fortunato2007resolution`. Leiden inherits this when it maximises
modularity, but escapes it with a resolution-limit-free objective such as the
Constant Potts Model, or by tuning its `resolution_parameter`. The map equation
has its own resolution limit, but a much weaker one: it depends on a module's cut
size rather than the total link count, so it resolves far smaller modules
({doc}`/concepts/hierarchy-and-the-multilevel-map` derives the bound). It does not
vanish, so very small, loosely connected modules can still be absorbed when a
shorter description exists without them; tune the scale with `markov_time` or
`preferred_number_of_modules`.
:::

## A worked example

We use `nx.planted_partition_graph`, which generates a network with four
equally-sized communities whose ground truth is known by construction.

### Build the graph

```{code-cell} python
import networkx as nx
import infomap
import igraph as ig
from sklearn.metrics import adjusted_mutual_info_score, normalized_mutual_info_score

# Four planted communities of 10 nodes each.
# p_in=0.7: 70 % chance of an edge within a community.
# p_out=0.05: 5 % chance across communities.
g = nx.planted_partition_graph(4, 10, 0.7, 0.05, seed=123)
print(f"Nodes: {g.number_of_nodes()}, Edges: {g.number_of_edges()}")

# Ground truth: nodes 0–9 → group 0, 10–19 → group 1, etc.
true_labels = [n // 10 for n in sorted(g.nodes())]
```

### Run all three methods

```{code-cell} python
# ── Infomap ──────────────────────────────────────────────────────────────────
result = infomap.run(g, two_level=True, seed=123, num_trials=10, silent=True)

modules = result.modules()                         # {node_id: module_id}
infomap_vec = [modules[n] for n in sorted(g.nodes())]
print(f"Infomap:  {result.num_top_modules} modules   codelength {result.codelength:.4f} bits/step")

# ── Louvain (NetworkX) ───────────────────────────────────────────────────────
louvain_communities = nx.community.louvain_communities(g, weight=None, seed=123)
louvain_labels = {}
for cid, comm in enumerate(louvain_communities):
    for node in comm:
        louvain_labels[node] = cid
louvain_vec = [louvain_labels[n] for n in sorted(g.nodes())]
print(f"Louvain:  {len(louvain_communities)} communities")

# ── Leiden (igraph) ──────────────────────────────────────────────────────────
ig_g = ig.Graph(edges=list(g.edges()), n=g.number_of_nodes(), directed=False)
leiden = ig_g.community_leiden(objective_function="modularity", n_iterations=10)
leiden_vec = leiden.membership
print(f"Leiden:   {len(leiden)} communities   modularity {leiden.modularity:.4f}")
```

### Compare with ground truth

Adjusted Mutual Information (AMI) and Normalised Mutual Information (NMI) are
standard label-comparison metrics from information theory. Both equal 1.0 for
a perfect match with the ground truth and 0 for chance-level agreement. AMI
corrects for the fact that a random partition with many small communities
inflates NMI.

```{code-cell} python
import pandas as pd

rows = []
for name, vec in [("Infomap", infomap_vec), ("Louvain", louvain_vec), ("Leiden", leiden_vec)]:
    rows.append({
        "Method": name,
        "Communities": len(set(vec)),
        "AMI vs truth": round(adjusted_mutual_info_score(true_labels, vec), 3),
        "NMI vs truth": round(normalized_mutual_info_score(true_labels, vec), 3),
    })

pd.DataFrame(rows).set_index("Method")
```

On this clean benchmark all three methods recover the four planted communities
exactly (AMI = NMI = 1.0). That is the common case: when the structure is
well-separated and undirected, the flow-based and modularity-based objectives
coincide. Agreement across objectives like this is a useful signal that the
partition is robust rather than an artefact of one method's bias.

The methods part ways in two regimes: when edge direction carries real flow
asymmetry, and when the graph is large enough for the modularity resolution limit
to bite. The section below demonstrates the resolution limit and explains the
directed case.

### Visualise the Infomap partition

```{code-cell} python
import matplotlib.pyplot as plt
from docs_viz import draw_partition
from myst_nb import glue

flow = {n.node_id: n.flow for n in result.nodes()}
fig = draw_partition(g, modules, flow=flow)
glue("fig-choosing-a-method", fig, display=False)
plt.close(fig)
```

```{glue:figure} fig-choosing-a-method
The benchmark network coloured by Infomap's partition. The chapter scores
this against Louvain and Leiden on the same graph with AMI and NMI.
```

Each colour is one Infomap module. The four planted communities appear as
distinct colour groups, with the handful of cross-community edges visible
between them.

## When results diverge most

Agreement on a clean benchmark is the common case. The objectives part ways in
two documented regimes.

**The resolution limit.** Modularity cannot resolve communities below a size set
by the whole graph. On a ring of many small cliques it merges adjacent cliques,
however clearly separated they are {cite:p}`fortunato2007resolution`:

```{code-cell} python
ring = nx.ring_of_cliques(15, 4)   # 15 cliques of 4 nodes, joined in a ring

infomap_n = infomap.run(
    ring, two_level=True, seed=123, num_trials=20, silent=True
).num_top_modules

louvain_n = len(nx.community.louvain_communities(ring, seed=123))

ring_ig = ig.Graph(edges=list(ring.edges()), n=ring.number_of_nodes(), directed=False)
leiden_n = len(ring_ig.community_leiden(objective_function="modularity", n_iterations=20))

print(f"Planted cliques     : 15")
print(f"Infomap             : {infomap_n} modules")
print(f"Louvain (modularity): {louvain_n} modules")
print(f"Leiden  (modularity): {leiden_n} modules")
```

Infomap recovers all 15 cliques; Louvain and Leiden, both maximising modularity,
merge them into 9. Because *both* modularity optimisers merge, the cause is the
modularity objective itself, not the search quality, a better optimiser does not
help. Leiden escapes it by switching to a resolution-limit-free objective such as
the Constant Potts Model, or by raising its `resolution_parameter`; the map
equation's own limit is far weaker, so it keeps the cliques apart.

**Directed flow.** Infomap follows edge direction; the standard modularity null
model does not. On small, balanced graphs the two still agree, but on genuinely
flow-asymmetric networks, a web subgraph that mostly *sends* links one way or a
citation cascade, the directed random walk concentrates where undirected edge
density does not, and the partitions diverge {cite:p}`rosvall2008maps`.

## API pointers

- {class}`infomap.Infomap` is the main class; keyword arguments configure the
  options. Pass `directed=True` for directed-flow treatment and `two_level=True`
  to restrict to a flat partition.
- {func}`infomap.find_communities` is a convenience wrapper that returns a
  `list[set]` of communities in NetworkX partition style.
- {func}`infomap.find_igraph_communities` is the igraph equivalent, returning a
  `VertexClustering` so Infomap results plug into igraph's modularity and
  plotting utilities.
- `sklearn.metrics.adjusted_mutual_info_score` and `normalized_mutual_info_score`
  are label-comparison metrics for partition evaluation; both accept flat integer
  label vectors.

## Going deeper

- {doc}`/concepts/the-map-equation` explains why Infomap minimises description
  length rather than maximising modularity, with the full two-level code
  derivation.
- Companion notebooks: `examples/notebooks/compare-infomap-louvain-networkx.ipynb`
  (NetworkX workflow, GraphML/GEXF export) and
  `examples/notebooks/compare-infomap-louvain-leiden-igraph.ipynb` (igraph-native
  `VertexClustering` and the `find_igraph_communities` helper).
- {cite:t}`rosvall2008maps` compares the map equation and modularity on flow and
  non-flow networks.
- {cite:t}`traag2019leiden` introduces Leiden and shows Louvain can return
  internally disconnected communities.
- {cite:t}`fortunato2007resolution` is the original proof of modularity's
  resolution limit.
- {cite:t}`smiljanic2026survey`, §3, compares the map equation and modularity in
  the broader survey.
