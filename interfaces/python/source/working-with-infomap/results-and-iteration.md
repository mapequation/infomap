---
jupytext:
  text_representation:
    format_name: myst
kernelspec:
  display_name: Python 3
  language: python
  name: python3
---

# Reading the Result

{bdg-success-line}`How-to`

```{admonition} In one sentence
:class: tip
`infomap.run` returns an immutable :class:`~infomap.Result`: summary statistics
as properties, per-node assignments through `modules()` and `nodes()`, and
tabular output through `to_dataframe()`, three views of the same partition, each
richer than the last.
```

## Which accessor to reach for

Running Infomap is one step; making use of the result is another. The partition
is richer than a list of labels: every node carries a flow value that quantifies
how much of the random walk visits it, and the full hierarchical tree encodes
nested module structure that a flat assignment vector cannot represent. Knowing
which accessor to reach for (property, dict, node view, or DataFrame) makes
downstream analysis faster and less error-prone.

This chapter walks through the complete result surface: the summary numbers that
tell you whether the run succeeded, the flat assignment dict that slots into most
visualisation and clustering workflows, the node-by-node view that gives you flow
alongside module membership, and the DataFrame export that connects Infomap to
pandas. It also explains why a single run may not be enough.

## Three layers of detail

Think of the :class:`~infomap.Result` as three concentric layers of detail.

The **outermost layer** is a handful of scalars: codelength, number of top
modules, number of levels. They answer "did it find something?" without touching
individual nodes, and are cheap to inspect, log, and compare across runs.

The **middle layer** is the partition itself, a mapping from node id to module
id. `result.modules()` returns it as a plain dict, `{node_id: module_id}`.
Anything that expects a node-colouring can consume it directly.

The **inner layer** is per-node flow. Each node's `flow` records the stationary
probability of a random walk visiting it under the best partition. High-flow
nodes are influential hubs; low-flow nodes are peripheral. This layer lets you
rank nodes within a module, build flow-weighted summaries, and flag nodes near
module boundaries.

`result.to_dataframe()` materialises all three layers into one tabular object you
can filter, group, merge, and export.

## Why more than one trial

Infomap minimises the map equation $L(\mathsf{M})$ over many independent
optimisation trials. Each trial starts from a different random partition and runs
a greedy hill-climbing search. The search is stochastic and the solution
landscape is degenerate (many partitions reach near-identical codelengths with
different node assignments), so running several trials lowers the risk of
returning a local minimum {cite:p}`calatayud2019solution`.

Infomap keeps only the partition with the lowest codelength across all trials.
That partition is what the :class:`~infomap.Result` reports through
`codelength`, `modules()`, and `nodes()`. The implication is practical: for
exploratory work a single trial is fine, but for results you intend to publish or
act on, use at least `num_trials=10` and prefer `num_trials=20` or more on
networks with weak or diffuse community structure.

:::{toggle}
**The solution landscape and degeneracy**

{cite:t}`calatayud2019solution` showed that even for networks with clear community
structure, Infomap produces a cloud of near-optimal partitions with similar
codelengths but meaningfully different node assignments. The variance of that
cloud grows as community structure weakens (higher mixing parameter $\mu$ in LFR
benchmarks). For networks with $\mu \le 0.2$, 50 trials are usually enough for the
solution landscape to be "complete", so further trials rarely reveal a different
partition. Noisier networks can need 200 to 1000 trials. If you need to
characterise the degeneracy rather than just find the best partition, consider
the dedicated [solution-landscape tooling](https://github.com/mapequation/solution-landscape).
:::

## Reading a karate-club result

We use the Zachary karate club graph throughout: small enough to inspect
interactively, familiar enough to judge the result, and a benchmark with known
community structure.

### Run Infomap

```{code-cell} python
import networkx as nx
import infomap
import pandas as pd

g = nx.karate_club_graph()

result = infomap.run(g, two_level=True, seed=123, num_trials=10, silent=True)
```

### The Result object

A :class:`~infomap.Result` is an immutable snapshot of one run. `run()` captures
the scalar metrics the moment it returns, so they stay valid forever. Node-level
collections are materialised lazily on first access and then cached. The two
kinds of access read differently, and the surface follows one convention:

- **Scalars are properties**: `result.codelength`, `result.num_top_modules`.
- **Collections are methods, with defaults**: `result.modules(depth=1)`,
  `result.nodes()`, `result.to_dataframe()`.

Because the snapshot is bound to the run that produced it, a `Result` from an
earlier run of a reused stateful :class:`~infomap.Infomap` raises if you read its
node data after a later `run()`, rather than quietly returning data from a
rebuilt tree. The eager scalars stay readable either way.

### Summary statistics

The most useful summary properties are available immediately:

```{code-cell} python
print(f"Codelength:          {result.codelength:.4f} bits/step")
print(f"One-level baseline:  {result.one_level_codelength:.4f} bits/step")
print(f"Compression gain:    {result.relative_codelength_savings * 100:.1f}%")
print(f"Top-level modules:   {result.num_top_modules}")
print(f"Hierarchy levels:    {result.num_levels}")
```

`result.codelength` is the map equation value $L(\mathsf{M}^*)$ for the best
partition found. `result.one_level_codelength` is the cost with all nodes in a
single module, the natural baseline for judging how much structure Infomap found.
`result.relative_codelength_savings` gives the fractional gap between the two; a
larger number means stronger, more compressible community structure.

`result.num_top_modules` is the number of top-level modules.
`result.num_levels` is the depth of the hierarchical tree; a value of 2 means one
level of modules above the leaves, the standard two-level result.

### Getting assignments: `modules()`

```{code-cell} python
modules = result.modules()   # {node_id: top-level module_id}

print(f"Type:  {type(modules)}")
print(f"First 8 entries: {dict(list(modules.items())[:8])}")
print(f"Unique modules: {sorted(set(modules.values()))}")
```

`result.modules()` returns a plain dict mapping each node's integer id to its
top-level module id. Module ids are positive integers with no guaranteed
ordering, but the numbering is stable across calls for the same run.

For hierarchical results with more than two levels, pass `depth=k` to slice the
tree at depth $k$. Level 1 gives top modules; level 2 gives sub-modules, and so
on up to `result.num_levels`.

```{code-cell} python
# Three clusters, each with three dense sub-communities.
g_hier = nx.Graph()
for cluster in range(3):
    for sub in range(3):
        base = cluster * 30 + sub * 10
        for i in range(base, base + 10):
            for j in range(i + 1, base + 10):
                g_hier.add_edge(i, j)
        g_hier.add_edge(base, cluster * 30 + ((sub + 1) % 3) * 10)
    g_hier.add_edge(cluster * 30, ((cluster + 1) % 3) * 30)

result_hier = infomap.run(g_hier, seed=123, num_trials=10, silent=True)

print(f"Hierarchy levels: {result_hier.num_levels}")
top_mods = result_hier.modules(depth=1)
sub_mods = result_hier.modules(depth=2)
print(f"Top-level modules (depth 1): {sorted(set(top_mods.values()))}")
print(f"Sub-modules      (depth 2): {sorted(set(sub_mods.values()))}")
```

### Iterating over nodes

When you need flow alongside module membership, iterate over `result.nodes()`.
Each node is an immutable view exposing `node_id`, `module_id`, and `flow` (plus
`path`, `name`, and `layer_id`):

```{code-cell} python
print(f"{'node_id':>8}  {'module_id':>10}  {'flow':>10}")
print("-" * 35)
for node in result.nodes():
    print(f"{node.node_id:>8}  {node.module_id:>10}  {node.flow:>10.6f}")
```

`flow` is the stationary visit probability under the random-walk model. Nodes
with higher flow are more central to the network's dynamics, so you can use it to
rank nodes within a module or to weight aggregations.

### Converting to a pandas DataFrame

`result.to_dataframe()` is the most convenient entry point for downstream
analysis. Pass the columns you want; valid names include `node_id`, `module_id`,
`flow`, `path`, and `name`.

```{code-cell} python
df = result.to_dataframe(columns=["node_id", "module_id", "flow", "path"])
print(f"Shape: {df.shape}")
df.head(8)
```

The `path` column encodes position in the hierarchical tree as a tuple. For a
two-level result each path is `(top_module_id, position_in_module)`. For deeper
hierarchies the tuple grows accordingly, and its first element is the top-level
module.

With the DataFrame in hand, per-module summaries are a single `groupby`:

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
The example network coloured by the modules read off the result. The same
assignment feeds the DataFrame and the per-node iteration above; marker area
scales with each node's flow.
```

Each colour corresponds to one top-level module. The layout respects the
network's edge structure: tightly connected nodes appear close together, and the
colour boundaries reveal where the random walk is most likely to cross between
modules.

### Comparing trials and inspecting degeneracy

Infomap runs `num_trials` independent searches and keeps only the best partition.
You can inspect how consistent the result is by running several single-trial
searches from different seeds and comparing their codelengths:

```{code-cell} python
codelengths = [
    infomap.run(g, two_level=True, seed=seed, num_trials=1, silent=True).codelength
    for seed in range(1, 11)
]

print("Codelengths across 10 single-trial runs:")
print(f"  min:    {min(codelengths):.6f}")
print(f"  max:    {max(codelengths):.6f}")
print(f"  range:  {max(codelengths) - min(codelengths):.6f}")
print(f"Best (10-trial run): {result.codelength:.6f}")
```

A tight spread means most trials recover the same strong community structure. A
wide spread, or a best-of-10 value below the single-trial minimum, signals a
degenerate solution landscape where more trials are warranted. The karate club
shows a small spread because its community structure is pronounced; large noisy
networks spread wider, and the spread itself tells you something.

## API pointers

- {attr}`infomap.Result.codelength` is $L(\mathsf{M}^*)$, the map equation value
  for the best partition.
- {attr}`infomap.Result.one_level_codelength` is the baseline cost with all nodes
  in a single module.
- {attr}`infomap.Result.relative_codelength_savings` is the fractional gain
  $(L_\text{one} - L^*) / L_\text{one}$.
- {attr}`infomap.Result.num_top_modules` counts the top-level modules.
- {attr}`infomap.Result.num_levels` is the depth of the hierarchical tree.
- {meth}`infomap.Result.modules` returns a `{node_id: module_id}` dict and
  accepts `depth=k` for hierarchy slicing.
- {meth}`infomap.Result.nodes` yields per-node views with `node_id`,
  `module_id`, and `flow`.
- {meth}`infomap.Result.to_dataframe` exports a table with columns `node_id`,
  `module_id`, `flow`, `path`, `name`.

## Going deeper

- {doc}`/api/index` is the complete property and method reference for
  {class}`infomap.Result`.
- `converge=True` treats `num_trials` as a cap and stops once the best codelength
  plateaus; pair it with a high cap on networks whose degeneracy you have not yet
  characterised. The [solution-landscape tooling](https://github.com/mapequation/solution-landscape)
  follows the clustering approach from {cite:t}`calatayud2019solution`.
- {cite:t}`smiljanic2026survey`, §4, covers the multilevel map equation and
  hierarchical community detection in full.
