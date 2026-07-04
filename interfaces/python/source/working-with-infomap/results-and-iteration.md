---
jupytext:
  text_representation:
    format_name: myst
kernelspec:
  display_name: Python 3
  language: python
  name: python3
---

# Reading the result

{bdg-success-line}`How-to`

```{admonition} At a glance
:class: tip
`infomap.run` returns an immutable {class}`~infomap.Result`: summary statistics
as properties, per-node assignments through `modules()` and `nodes()`, and
tabular output through `to_dataframe()`.
```

## What the Result holds

A {class}`~infomap.Result` holds more than labels. Each node carries a flow
value, the share of the random walk that visits it, and the hierarchical tree
records nested modules that a flat assignment vector cannot.

This chapter covers the summary statistics, the assignment dict, the per-node
view with flow, and the DataFrame export. It also explains why a single run may
not be enough.

## Three layers of detail

Think of the {class}`~infomap.Result` as three concentric layers of detail.

The **outermost layer** is a handful of scalars: codelength, number of top
modules, number of levels. They answer "did it find something?" without touching
individual nodes, and are cheap to inspect, log, and compare across runs.

The **middle layer** is the partition itself, a mapping from node id to module
id. `result.modules()` returns it as a plain dict, `{node_id: module_id}`.
Anything that expects a node-colouring can consume it directly.

The **inner layer** is per-node flow. Each node's `flow` records the stationary
probability of a random walk visiting it under the best partition. High-flow
nodes are influential hubs; low-flow nodes are peripheral. This layer lets you
rank nodes within a module or build flow-weighted summaries.

`result.to_dataframe()` materialises all three layers into one tabular object you
can filter, group, merge, and export.

## Why more than one trial

Infomap keeps only the partition with the lowest codelength across the
`num_trials` independent searches, and that partition is what the
{class}`~infomap.Result` reports through `codelength`, `modules()`, and
`nodes()`. A single trial can settle in a local minimum, so one run is fine for
exploration but results you publish or act on warrant more; see
{doc}`Running Infomap </working-with-infomap/running-and-options>` for why a
single trial gets stuck and the full `num_trials` rule of thumb. The rest of this
chapter is about *reading and comparing* the partition you get back.

:::{toggle}
**The solution landscape and degeneracy**

Even for networks with clear community structure, Infomap produces a cloud of
near-optimal partitions with similar codelengths but meaningfully different node
assignments {cite:p}`calatayud2019solution`. The variance of that
cloud grows as community structure weakens (higher mixing parameter $\mu$ in LFR
benchmarks). For networks with $\mu \le 0.2$, 50 trials are usually enough for the
solution landscape to be "complete", so further trials rarely reveal a different
partition. Noisier networks can need 200 to 1000 trials. If you need to
characterise the degeneracy rather than just find the best partition, consider
the dedicated [solution-landscape tooling](https://github.com/mapequation/solution-landscape).
:::

## Reading a karate-club result

The examples below use the Zachary karate club graph, a 34-node network with
well-known community structure.

### Run Infomap

```{code-cell} python
import networkx as nx
import infomap
import pandas as pd

g = nx.karate_club_graph()

result = infomap.run(g, two_level=True, seed=123, num_trials=10, silent=True)
```

### The Result object

A {class}`~infomap.Result` is an immutable snapshot of one run. `run()` captures
the scalar metrics when it returns. Node-level collections are materialised
lazily on first access and then cached. The surface follows one convention:

- **Scalars are properties**: `result.codelength`, `result.num_top_modules`.
- **Collections are methods, with defaults**: `result.modules(depth=1)`,
  `result.nodes()`, `result.to_dataframe()`.

A `Result` from an earlier run of a reused stateful {class}`~infomap.Infomap`
raises if you read its node data after a later `run()`. The eagerly captured
scalar properties stay readable (the lazily computed `effective_num_*`
properties must have been read at least once before the re-run).

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

Module ids are positive integers with no guaranteed ordering, but the numbering
is stable across calls for the same run.

For hierarchical results with more than two levels, pass `depth=k` to slice the
tree at depth $k$. Level 1 gives top modules; level 2 gives sub-modules, and so
on down to the finest module level, `result.num_levels - 1` (the leaf nodes
themselves occupy the last level).

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
Each node is an immutable view; the attributes you reach for most are `node_id`,
`module_id`, and `flow`, alongside `path`, `name`, and `layer_id`. See
{class}`~infomap.TreeNode` for the full set (including `state_id`, `state_name`,
`depth`, `child_index`, and `modular_centrality`):

```{code-cell} python
print(f"{'node_id':>8}  {'module_id':>10}  {'flow':>10}")
print("-" * 35)
for node in result.nodes():
    print(f"{node.node_id:>8}  {node.module_id:>10}  {node.flow:>10.6f}")
```

`flow` is the stationary visit probability described above; see
{doc}`/concepts/flow-and-random-walks` for how it is computed.

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

`draw_partition` is a small helper that ships with these docs (in
`_ext/docs_viz.py`), not part of the `infomap` package; see
{doc}`visualizing-and-exporting` for the full treatment of plotting and export.

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

The layout respects the network's edge structure: tightly connected nodes appear
close together, and the colour boundaries show where the random walk crosses
between modules.

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
degenerate solution landscape where more trials are warranted. The karate club's
community structure is pronounced enough that every trial finds the same
codelength; large noisy networks spread wider.

## Pitfalls

- **A `Result` goes stale if its network runs again.** Reading `nodes()` or
  `modules()` on a `Result` after a later `run()` on the same network raises;
  finish reading a result before you re-run. The eagerly captured scalar
  properties stay readable.
- **Module ids carry no order or meaning.** They are stable within one run but
  arbitrary across runs; compare partitions with a metric, not id equality.
- **`modules()` returns the top level by default.** Pass `depth=-1` for the
  finest level of a multilevel result.

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
  follows the solution-landscape clustering approach {cite:p}`calatayud2019solution`.
- The survey (§4) covers the multilevel map equation and hierarchical community
  detection in full {cite:p}`smiljanic2026survey`.
