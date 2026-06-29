# Working with Infomap

The practical core: build a network from your data, run Infomap, then read,
visualise, and export the result. These chapters are task-oriented; for the ideas
behind them see {doc}`/concepts/index`, and for the smallest possible example see
{doc}`/quickstart`.

- {doc}`Inputs <inputs>`: every way to get your data in: NetworkX, igraph, SciPy
  sparse matrices, edge lists, and the raw API.
- {doc}`Running Infomap <running-and-options>`: the few options that matter, with
  rules of thumb for trials, seeds, directedness, and resolution.
- {doc}`Reading results and iterating <results-and-iteration>`: modules, flow, the
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
