---
jupytext:
  text_representation:
    format_name: myst
kernelspec:
  display_name: Python 3
  language: python
  name: python3
---

# The stateful Infomap class

{bdg-success-line}`How-to`

```{admonition} At a glance
:class: tip

The {class}`infomap.Infomap` class is the stateful entry point: build a network
with the `add_*` verbs, then call `run()` for an immutable
{class}`~infomap.Result`. New code should prefer {func}`infomap.run` for one-shot
use and {class}`~infomap.Network` for incremental construction; existing
`Infomap` code keeps working essentially unchanged (see
[Removed accessors](#removed-accessors) for the few exceptions).
```

## When to use it

The functional {func}`infomap.run` and the {class}`~infomap.Network` builder
cover most needs. Reach for the stateful {class}`~infomap.Infomap` class when you
want to keep one configured object around and run it repeatedly, or when you are
maintaining code written against the original API. Internally it composes a
`Network` and an `Options` over the same engine boundary, so its building verbs
and its results are identical to the functional path.

```{code-cell} python
from infomap import Infomap

im = Infomap(silent=True, seed=123, num_trials=10)
im.add_link(0, 1)
im.add_link(1, 2)
im.add_link(2, 0)

result = im.run()          # returns an immutable Result
print(result.codelength)
print(result.modules())
```

`im.run()` returns the same {class}`~infomap.Result` the functional API returns.
The on-instance accessors (`im.get_modules()`, `im.codelength`, `im.nodes`) still
work and are backed by that result.

## Migrating to the functional API

Each stateful pattern has a direct functional or `Network` equivalent:

| Task | Stateful `Infomap` | Functional / `Network` |
|---|---|---|
| Run a graph | `im = Infomap(**opts); im.add_networkx_graph(g); im.run()` | `result = infomap.run(g, **opts)` |
| Build incrementally | `im.add_node(...)`, `im.add_link(...)`, then `im.run()` | `net = Network(); net.add_node(...); net.add_link(...)`, then `infomap.run(net)` |
| Reusable configuration | repeat the keyword arguments | `options = Options(**opts)`, then `infomap.run(g, options=options)` |
| Top-level modules | `im.get_modules()` | `result.modules()` |
| Modules at level *k* | `im.get_modules(depth_level=k)` | `result.modules(depth=k)` |
| State-node modules | `im.get_modules(states=True)` | `result.modules(states=True)` |
| Per-node flow | `for n in im.nodes: n.data.flow` | `for n in result.nodes(states=True): n.flow` (`im.nodes` iterates state nodes; plain `result.nodes()` gives physical nodes, identical for first-order networks) |
| DataFrame | `im.to_dataframe([...])` | `result.to_dataframe([...])` |
| Scalar metrics | `im.codelength`, `im.num_top_modules` | `result.codelength`, `result.num_top_modules` |
| Graph-file export | `infomap.io.export.write_graphml(graph, im, path)` | `nx.write_graphml(infomap.to_networkx(result), path)` |

The two consistent shifts: building a network is a {class}`~infomap.Network`
(or a direct {func}`infomap.run` call) rather than the stateful instance, and
reading results goes through the immutable {class}`~infomap.Result` whose scalars
are properties and whose collections are methods with defaults.

## Removed accessors

The redesign dropped a few camelCase passthroughs. Their replacements:

| Removed | Use instead |
|---|---|
| `im.isBipartite()` | set {attr}`~infomap.Infomap.bipartite_start_id`; there is no separate status accessor |
| `im.setBipartiteStartId(n)` | `im.bipartite_start_id = n` (a property) |
| `im.haveMetaData()` | declare with {meth}`~infomap.Infomap.set_meta_data`; read {attr}`~infomap.Result.meta_entropy` |
| `im.numMetaDataDimensions` | one metadata dimension is supported; there is no count accessor |

## See also

- {doc}`/quickstart` leads with the functional API.
- {doc}`/api/infomap` is the full reference for the stateful class.
- {doc}`/api/index` covers {func}`infomap.run`, {class}`infomap.Network`,
  {class}`infomap.Result`, and {class}`infomap.Options`.
