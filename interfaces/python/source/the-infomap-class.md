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

{func}`infomap.run` is the canonical entry point. It accepts any input --
including a prebuilt {class}`~infomap.Network` or a stateful
{class}`~infomap.Infomap` instance -- so `net.run(**kw)` and `im.run(**kw)` are
thin conveniences equivalent to `infomap.run(net, **kw)` /
`infomap.run(im, **kw)`. All three take the same keywords (the five common-tier
options directly, everything else via `options=`) and return the same
{class}`~infomap.Result`.

```{code-cell} python
from infomap import Infomap

im = Infomap(seed=123, num_trials=10)
im.add_link(0, 1)
im.add_link(1, 2)
im.add_link(2, 0)

result = im.run()          # returns an immutable Result
print(result.codelength)
print(result.modules())
```

`im.run()` returns the same {class}`~infomap.Result` the functional API returns.
The on-instance result accessors (`im.get_modules()`, `im.codelength`,
`im.nodes`) still work and are backed by that result, but they are **deprecated
and leave in 3.0** -- read the equivalently named members off the returned
{class}`~infomap.Result` instead. Mind the shape shift: `im.modules` is a
property, while `result.modules()` is a method. These accessors emit a
silent-by-default `PendingDeprecationWarning` (surface it with `-W`); the
migration table below maps each one across.

## Migrating to the functional API

Each stateful pattern has a direct functional or `Network` equivalent:

| Task | Stateful `Infomap` | Functional / `Network` |
|---|---|---|
| Run a graph | `im = Infomap(**opts); im.add_networkx_graph(g); im.run()` | `result = infomap.run(g, **opts)` |
| Build incrementally | `im.add_node(...)`, `im.add_link(...)`, then `im.run()` | `net = Network(); net.add_node(...); net.add_link(...)`, then `infomap.run(net)` |
| Reusable configuration | repeat the keyword arguments | `options = Options(**opts)`, then `infomap.run(g, options=options)` |
| Top-level modules | `im.get_modules()` | `result.modules()` |
| Iterate `(node_id, module_id)` pairs | `for k, v in im.modules:` | `for k, v in result.modules().items():` |
| Modules at level *k* | `im.get_modules(depth_level=k)` | `result.modules(depth=k)` |
| State-node modules | `im.get_modules(states=True)` | `result.modules(states=True)` |
| Per-node flow | `for n in im.nodes: n.data.flow` | `for n in result.nodes(states=True): n.flow` (`im.nodes` iterates state nodes; plain `result.nodes()` gives physical nodes, identical for first-order networks) |
| DataFrame | `im.to_dataframe([...])` | `result.to_dataframe([...])` |
| Scalar metrics | `im.codelength`, `im.num_top_modules` | `result.codelength`, `result.num_top_modules` |
| Graph-file export | `infomap.io.export.write_graphml(graph, im, path)` | `nx.write_graphml(result.to_networkx(), path)` |

The two consistent shifts: building a network is a {class}`~infomap.Network`
(or a direct {func}`infomap.run` call) rather than the stateful instance, and
reading results goes through the immutable {class}`~infomap.Result`
(see {doc}`/working-with-infomap/results-and-iteration`).

## Migrating deprecated keyword arguments

Advanced engine keywords still work on `Infomap()` and `infomap.run()` in 2.x,
but they are pending-deprecated and leave those signatures in 3.0 (issue #741):
passing one directly emits a (default-silent) `PendingDeprecationWarning`. Each
falls into one of three groups, with a sanctioned replacement:

| Deprecated keyword | Where it moves |
|---|---|
| Advanced tuning flags -- `regularized`, `core_loop_limit`, `flow_model`, and the like | carry them on the {class}`~infomap.Options` object: `run(g, options=Options(regularized=True))` |
| Output-artifact flags -- `no_file_output`, `tree`, `clu`, `out_name` | write from the {class}`~infomap.Result` instead: `result.write_tree(path)`, `result.write_clu(path)`, or `network.write_pajek(path)` |
| Console flags -- `silent`, `verbosity_level` | use logging: `infomap.enable_log()` for the engine log, and `infomap.enable_log(logging.DEBUG)` to raise its verbosity |

The `options` carrier accepts an {class}`~infomap.Options` instance or a plain
mapping, and is accepted on `Infomap()`, {meth}`Infomap.run`,
{meth}`Network.run`, and {func}`infomap.run` alike — for example
`im.run(options=Options(regularized=True))` gives the stateful builder the same
warning-free path. A bare keyword argument set to a non-default value still
overrides the carrier, and only bare keyword arguments typed directly on the call
are flagged, so the common options (`seed`, `num_trials`, `two_level`,
`directed`, `markov_time`) stay on the signature and are never deprecated.

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
