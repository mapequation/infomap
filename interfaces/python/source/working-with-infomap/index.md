# Working with Infomap

The practical core: build a network from your data, run Infomap, then read,
visualise, and export the result. These chapters are task-oriented; for the ideas
behind them see {doc}`/concepts/index`, and for the smallest possible example see
{doc}`/quickstart`.

- {doc}`Building a network <inputs>`: every way to get your data in with
  {func}`infomap.run` and {class}`~infomap.Network`: NetworkX, igraph, SciPy
  sparse matrices, edge indices, edge lists, and incremental construction.
- {doc}`Running Infomap <running-and-options>`: the few options that matter, with
  rules of thumb for trials, seeds, directedness, and resolution.
- {doc}`Reading the Result <results-and-iteration>`: the immutable
  {class}`~infomap.Result` surface: scalar metrics, modules, per-node flow, the
  hierarchical tree, and a pandas DataFrame of assignments.
- {doc}`Visualising and exporting <visualizing-and-exporting>`: draw the partition
  and write `.tree`, `.clu`, GraphML, or GEXF for Gephi and other tools.

```{toctree}
:hidden:
:maxdepth: 1

inputs
running-and-options
results-and-iteration
visualizing-and-exporting
```
