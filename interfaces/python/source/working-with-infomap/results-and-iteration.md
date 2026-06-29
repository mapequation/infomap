---
jupytext:
  text_representation:
    format_name: myst
kernelspec:
  display_name: Python 3
  language: python
  name: python3
---

# Reading results and iterating

```{admonition} In one sentence
:class: tip
After `im.run()`, Infomap exposes summary statistics as attributes, per-node
assignments through `get_modules()` and `im.nodes`, and tabular results through
`to_dataframe()`: three views of the same partition, each richer than the last.
```

## Motivation

Running Infomap is one step; making use of the result is another. The partition
Infomap produces is richer than a simple list of labels: every node carries a
flow value that quantifies how much of the random walk visits it, and the full
hierarchical tree encodes nested module structure that a flat assignment vector
cannot represent. Knowing which access pattern to reach for (attribute, dict,
node iterator, or DataFrame) makes downstream analysis faster and less error-prone.

This chapter walks through the complete result surface: the summary numbers that
tell you whether the run succeeded, the flat assignment dict that slots into most
visualisation and clustering workflows, the node-by-node iterator that gives you
flow alongside module membership, and the DataFrame export that connects Infomap
results to pandas. It also explains why a single run may not be enough and when
you should look at several trials rather than just the best one.

## Intuition

Think of Infomap's output as three concentric layers of detail.

The **outermost layer** is a handful of scalars: codelength, number of top
modules, number of levels. These answer "did it find something?" without touching
individual nodes. They are cheap to inspect, log, and compare across runs.

The **middle layer** is a mapping from node id to module id, the partition
itself. `get_modules()` returns this as a plain Python dict, `{node_id: module_id}`.
Everything that expects a node-colouring can consume it directly.

The **inner layer** is per-node flow. Each node's `data.flow` records the
stationary probability of a random walk visiting it under the best partition.
Nodes with high flow are influential hubs; nodes with low flow are peripheral.
This layer lets you rank nodes within a module, build summaries weighted by
importance, and flag nodes that sit near module boundaries.

The DataFrame accessor `to_dataframe()` materialises all three layers into a
single tabular object that you can filter, group, merge, and export.

## Theory

Infomap minimises the map equation $L(\mathsf{M})$ over many independent
optimisation trials. Each trial starts from a different random partition and
runs a greedy hill-climbing search. The search is stochastic and the solution
landscape is degenerate (many partitions reach near-identical codelengths with
different node assignments), so running several trials lowers the risk of
returning a local minimum {cite:p}`calatayud2019solution`.

Infomap keeps only the partition with the lowest codelength across all trials.
That partition is what you see in `im.codelength`, `im.get_modules()`, and
`im.nodes`. The implication is practical: for exploratory work a single trial
is fine, but for results you intend to publish or act on, use at least
`num_trials=10` and prefer `num_trials=20` or more on networks with weak or
diffuse community structure.

:::{toggle}
**The solution landscape and degeneracy**

{cite:t}`calatayud2019solution` showed that even for networks with clear community
structure, Infomap produces a cloud of near-optimal partitions with
similar codelengths but meaningfully different node assignments. The variance of
that cloud grows as community structure weakens (higher mixing parameter $\mu$
in LFR benchmarks). For networks with $\mu \le 0.2$, 50 trials are usually
enough for the solution landscape to be "complete", so further trials rarely
reveal a different partition. Noisier networks can need 200 to 1000 trials. If you need to characterise the
degeneracy rather than just find the best partition, consider the dedicated
[solution-landscape tooling](https://github.com/mapequation/solution-landscape).
:::

## A worked example

We use the Zachary karate club graph throughout: small enough to inspect
interactively, familiar enough to judge the result, and a benchmark with known
community structure.

### Setup: run Infomap

```{code-cell} python
import networkx as nx
import infomap
import pandas as pd

g = nx.karate_club_graph()

im = infomap.Infomap(two_level=True, seed=123, num_trials=10, silent=True)
im.add_networkx_graph(g)
im.run()
```

### Summary statistics

The three most useful summary attributes are available immediately after `run()`.

```{code-cell} python
print(f"Codelength:          {im.codelength:.4f} bits/step")
print(f"One-level baseline:  {im.one_level_codelength:.4f} bits/step")
print(f"Compression gain:    {im.relative_codelength_savings * 100:.1f}%")
print(f"Top-level modules:   {im.num_top_modules}")
print(f"Hierarchy levels:    {im.num_levels}")
```

`im.codelength` is the map equation value $L(\mathsf{M}^*)$ for the best
partition found. `im.one_level_codelength` is the cost with all nodes in a
single module, the natural baseline for judging how much structure Infomap
found. `im.relative_codelength_savings` gives the
fractional gap between the two; a larger number means stronger and more
compressible community structure.

`im.num_top_modules` is the number of top-level modules. `im.num_levels`
is the depth of the hierarchical tree; a value of 2 means one level of
modules above the leaves, which is the standard two-level result.

### Getting assignments: `get_modules()`

```{code-cell} python
modules = im.get_modules()   # {node_id: top-level module_id}

print(f"Type:  {type(modules)}")
print(f"First 8 entries: {dict(list(modules.items())[:8])}")
print(f"Unique modules: {sorted(set(modules.values()))}")
```

`get_modules()` returns a plain dict mapping each node's integer id to its
top-level module id. Module ids are positive integers with no guaranteed
ordering, but the numbering is stable across calls for the same run.

For hierarchical results with more than two levels, pass `depth_level=k` to
slice the tree at depth $k$. Level 1 gives top modules; level 2 gives
sub-modules; and so on up to `im.num_levels`.

```{code-cell} python
# Demonstrate depth_level on a network with genuine three-level hierarchy.
# Build three clusters, each with three dense sub-communities.
g_hier = nx.Graph()
for cluster in range(3):
    for sub in range(3):
        base = cluster * 30 + sub * 10
        for i in range(base, base + 10):
            for j in range(i + 1, base + 10):
                g_hier.add_edge(i, j)
        g_hier.add_edge(base, cluster * 30 + ((sub + 1) % 3) * 10)
    g_hier.add_edge(cluster * 30, ((cluster + 1) % 3) * 30)

im_hier = infomap.Infomap(seed=123, num_trials=10, silent=True)
im_hier.add_networkx_graph(g_hier)
im_hier.run()

print(f"Hierarchy levels: {im_hier.num_levels}")
top_mods   = im_hier.get_modules(depth_level=1)
sub_mods   = im_hier.get_modules(depth_level=2)
print(f"Top-level modules (depth 1): {sorted(set(top_mods.values()))}")
print(f"Sub-modules      (depth 2): {sorted(set(sub_mods.values()))}")
```

### Iterating over nodes

When you need flow alongside module membership, iterate over `im.nodes`.
Each node object exposes `node_id`, `module_id`, and `data.flow`.

```{code-cell} python
print(f"{'node_id':>8}  {'module_id':>10}  {'flow':>10}")
print("-" * 35)
for node in im.nodes:
    print(f"{node.node_id:>8}  {node.module_id:>10}  {node.data.flow:>10.6f}")
```

`data.flow` is the stationary visit probability under the random-walk model.
Nodes with higher flow are more central to the network's dynamics. You can use
this to rank nodes within a module or to weight aggregations.

### Converting to a pandas DataFrame

`to_dataframe()` is the most convenient entry point for downstream analysis.
Pass a list of columns you want; valid names include `node_id`, `module_id`,
`flow`, and `path`.

```{code-cell} python
df = im.to_dataframe(columns=["node_id", "module_id", "flow", "path"])
print(f"Shape: {df.shape}")
df.head(8)
```

The `path` column encodes position in the hierarchical tree as a tuple. For a
two-level result each path is `(top_module_id, position_in_module)`. For deeper
hierarchies the tuple grows accordingly, and its first element is the top-level
module.

With the DataFrame in hand, you can compute per-module summaries in a single
`groupby`:

```{code-cell} python
module_summary = (
    df.groupby("module_id", as_index=False)
    .agg(
        nodes=("node_id", "count"),
        total_flow=("flow", "sum"),
    )
    .sort_values("total_flow", ascending=False)
    .reset_index(drop=True)
)
module_summary
```

`total_flow` is the fraction of the random walk that flows through each module.
Modules with high total flow are the dominant structural features; modules with
low flow are peripheral communities that the walker rarely visits.

### Visualising the partition

```{code-cell} python
import matplotlib.pyplot as plt
from myst_nb import glue

from docs_viz import draw_partition

flow = dict(zip(df["node_id"], df["flow"]))
fig = draw_partition(g, modules, flow=flow)
glue("fig-results-and-iteration", fig, display=False)
plt.close(fig)
```

```{glue:figure} fig-results-and-iteration
The example network coloured by the modules read back from the result
object. The same assignment feeds the dataframe and the per-module iteration
shown above.
```

Each colour corresponds to one top-level module. The layout respects the
network's edge structure: tightly connected nodes appear close together, and the
colour boundaries reveal where the random walk is most likely to cross between
modules.

### Comparing trials and inspecting degeneracy

Infomap runs `num_trials` independent searches and keeps only the best
partition. You can inspect how consistent the result is by running multiple
independent `Infomap` instances and comparing their codelengths.

```{code-cell} python
codelengths = []
for seed in range(1, 11):
    im_trial = infomap.Infomap(two_level=True, seed=seed, num_trials=1, silent=True)
    im_trial.add_networkx_graph(g)
    im_trial.run()
    codelengths.append(im_trial.codelength)

print(f"Codelengths across 10 single-trial runs:")
print(f"  min:    {min(codelengths):.6f}")
print(f"  max:    {max(codelengths):.6f}")
print(f"  range:  {max(codelengths) - min(codelengths):.6f}")
print(f"Best (10-trial run): {im.codelength:.6f}")
```

A tight spread means most trials recover the same strong community structure. A
wide spread, or a best-of-10 value below the single-trial minimum, signals a
degenerate solution landscape where more trials are warranted. The karate club
shows a small spread because its community structure is pronounced; large noisy
networks spread wider, and the spread itself tells you something.

## API pointers

- {attr}`infomap.Infomap.codelength` is $L(\mathsf{M}^*)$, the map equation
  value for the best partition.
- {attr}`infomap.Infomap.one_level_codelength` is the baseline cost with all
  nodes in a single module.
- {attr}`infomap.Infomap.relative_codelength_savings` is the fractional gain
  $(L_\text{one} - L^*) / L_\text{one}$.
- {attr}`infomap.Infomap.num_top_modules` counts the top-level modules in the
  best partition.
- {attr}`infomap.Infomap.num_levels` is the depth of the hierarchical tree.
- {meth}`infomap.Infomap.get_modules` returns a `{node_id: module_id}` dict and
  accepts `depth_level=k` for hierarchy slicing.
- {attr}`infomap.Infomap.nodes` is an iterable of node objects with `node_id`,
  `module_id`, and `data.flow`.
- {meth}`infomap.Infomap.to_dataframe` exports a table with columns `node_id`,
  `module_id`, `flow`, `path`.

## Going deeper

- {doc}`/api/index` is the complete attribute and method reference for
  `infomap.Infomap`.
- `examples/notebooks/4.3 Solution Landscape.ipynb` explores partition degeneracy,
  the clustering approach from {cite:t}`calatayud2019solution`, and the
  `converge=True` option for early stopping.
- {cite:t}`smiljanic2026survey`, §4, covers the multilevel map equation and
  hierarchical community detection in full.
