# Python Workflows

Use this reference when the user works in Python, notebooks, NetworkX, python-igraph, SciPy, edge-index, AnnData, Scanpy, pandas, or graph export workflows.

## The recommended API shape (2.x, stable into 3.0)

Prefer this shape unless the user is maintaining older code:

- `infomap.run(input, *, options=None, seed=..., num_trials=..., ...)` — the one-call front door. `input` can be a NetworkX/igraph graph, a SciPy sparse matrix, a `(2, E)` edge index, a file path, or an iterable of `(u, v[, w])` links. Returns an immutable `Result`.
- `infomap.Network.from_networkx / from_igraph / from_scipy_sparse_matrix / from_edge_index / from_file` — build a network explicitly when the input needs non-default reading (a different weight attribute, explicit directedness, a state/multilayer layout), then `network.run(options=...)`.
- `infomap.Options(...)` — a reusable dataclass-style configuration; the canonical carrier for engine options. Pass it as `infomap.run(input, options=Options(...))`.
- `infomap.Result` — the immutable result. Read scalars as **properties** (`result.codelength`, `result.num_top_modules`) and collections as **methods** (`result.modules()`, `result.nodes()`, `result.tree()`, `result.to_dataframe()`). Exceptions worth memorizing: `result.names` / `result.state_names` / `result.codelengths` are collection-valued **properties** (no parentheses — calling `result.names()` raises a plain `TypeError`, `'dict' object is not callable`), and `result.effective_num_modules(depth)` is a scalar read as a **method** because it takes a depth (unlike the property-shaped `result.effective_num_top_modules`).

The stateful `infomap.Infomap` class still works and is a fine builder for incremental construction, but read results off the `Result` that `im.run()` returns — not off the instance. Instance accessors such as `im.modules`, `im.nodes`, `im.codelength`, and `im.get_modules()` are deprecated and leave in 3.0; note also that `im.modules`/`im.nodes` are *properties* while `result.modules()`/`result.nodes()` are *methods*.

## Options and the 3.0 transition

Five options are first-class keyword arguments across `infomap.run()` / `Infomap()` / `Infomap.run()`: **`seed`, `num_trials`, `two_level`, `directed`, `markov_time`**. The functional `infomap.run()` and `Network.run()` also accept any other engine option (for example `regularized`, `flow_model`, `teleportation_probability`, `num_threads`, `multilayer_relax_rate`, `entropy_corrected`) as a bare keyword — it forwards to `Options` — so there is no friction for a one-off. The ~70 explicit advanced keywords on the *stateful* `Infomap()` / `Infomap.run()` signatures are pending-deprecated and move off those signatures in 3.0; `Options` is the recommended reusable, validated carrier and the parameter reference. This five-name set lives in the code, not this skill: confirm it from `inspect.signature(infomap.run)` (the explicit keyword parameters) or the published `api/deprecations` reference rather than trusting a copied list.

```python
import infomap
from infomap import Options

result = infomap.run(
    graph,
    options=Options(regularized=True, flow_model="directed"),
    seed=123,
    num_trials=20,
)
```

Prefer `infomap.run(g, options=Options(regularized=True))` when the configuration is reused across runs or you want it validated up front; a bare `infomap.run(g, regularized=True)` is fine for a one-off.

The `options=` carrier is accepted on the stateful `Infomap()` and `Infomap.run()` as well, not just `infomap.run()` and `Network.run()` — e.g. `im.run(options=Options(regularized=True))` — so the stateful builder has the same warning-free path (a bare keyword set to a non-default value still overrides the carrier).

`directed` is a direct engine kwarg only for graph, file, and link-iterable inputs. For a SciPy sparse matrix or a `(2, E)` edge index it names the *input adapter's* orientation, so `infomap.run(A, directed=True)` raises `TypeError`; build the network explicitly with `Network.from_scipy_sparse_matrix(A, directed=True)` / `Network.from_edge_index(ei, directed=True)`, or pass `options=Options(flow_model="directed")`.

The Python API is quiet by default. Do not reach for `silent=` (also going away); to see the engine log, attach a handler to the `infomap` logger, e.g. `infomap.enable_log()`.

## Authority for current syntax

This skill is not the API manual. Confirm names and signatures against the installed package before giving runnable code — but read the right surface:

```python
import inspect, importlib.metadata as md
import infomap

print(md.version("infomap"))
print(inspect.signature(infomap.run))       # the functional front door
print(inspect.getdoc(infomap.Options))      # the canonical parameter reference — one clean entry per option
print(inspect.getdoc(infomap.Result))       # how to read results
```

Prefer `inspect.getdoc(infomap.Options)` over `inspect.signature(infomap.Infomap)` for the parameter reference. The stateful `Infomap` and `Infomap.run` signatures still list ~70 keyword arguments whose docstrings are mostly per-parameter deprecation notes (options relocating to `Options` in 3.0), so the raw signature does not reveal which options are permanent. (The *functional* `inspect.signature(infomap.run)` is already clean — the five common keywords plus the structural `options`/`initial_partition` — so don't expect ~70 there.) `Options` lists every engine option once with a clean per-option docstring, without the ~70-way signature clutter.

Use the published Python docs at `https://mapequation.org/infomap-python-docs/` when internet access is available. If inspecting from an Infomap source checkout, verify `infomap.__file__` so Python has not imported repo-local sources by accident.

## Choose the Python entry point

- One-call partitioning from a graph, file, matrix, edge index, or link iterable: `infomap.run(input, seed=..., num_trials=...)`.
- Just the partition in the graph's own node labels: `infomap.find_communities(graph, seed=..., num_trials=...)` returns a NetworkX-style `list[set]` of node labels — not a `{node: module}` dict. Its igraph counterpart `infomap.find_igraph_communities` returns an `igraph.VertexClustering` (keyed by vertex index), not the same shape. Both accept a `trials` alias for `num_trials` and, like `run`, default to `num_trials=1` — raise it for research runs.
- Non-default input building (weights, directedness, state/multilayer): `Network.from_*(...)` then `.run(options=Options(...))`.
- Encoding node metadata: read it from a clu-format file via `Options(meta_data=..., meta_data_rate=...)`, or set it per node on the stateful builder with `im.set_meta_data(node_id, category)` (confirm the current spelling from installed help).
- Bipartite input: declare the second node type with the `Network` bipartite start id (e.g. `net.bipartite_start_id`); `Options(hide_bipartite_nodes=True)` projects that type out of the written `result.write_*` output.
- Export the partition attached to a graph (GraphML/GEXF and other graph-library formats): `result.to_networkx(module_attribute="infomap_module")` / `result.to_igraph(...)` attach `infomap_module`/`infomap_path`/`flow` node attributes; write with the graph library's own writer (e.g. `networkx.write_graphml(result.to_networkx(...), path)`). There is no native GraphML/GEXF writer, so this is the path for that capability.
- AnnData/Scanpy observation graphs: `infomap.tl.infomap(adata, key_added="infomap")` writes labels to `adata.obs[key_added]` and run metadata to `adata.uns[key_added]` (confirm the helper is exposed in the installed package).

## Generating code

- Prefer the functional `run(...)` returning a `Result`; read scalars as properties and collections as methods.
- Prefer `Options` for advanced options (reusable, validated, the reference); the functional `infomap.run()` / `Network.run()` also accept them as bare keywords. The five common keywords are listed under *Options and the 3.0 transition* above — verify against `inspect.signature(infomap.run)`, do not re-derive from memory.
- Make runs reproducible: set `seed` (default 123) and a meaningful `num_trials` (default 1) for research runs; use `num_trials=1` for smoke tests. For a no-dependency smoke run use a bundled network, e.g. `infomap.run(infomap.datasets.two_triangles(), num_trials=1)`.
- Record package version, `infomap.__file__`, graph source, directed/weighted choice, seed, trials, non-default options, and output artifacts.
- For state or multilayer output, say whether results are state-level (`result.nodes(states=True)`) or physical-node-level, and preserve any label→internal-id mapping.

## Minimal pattern

```python
import infomap

result = infomap.run(graph, seed=123, num_trials=20)   # graph, file path, matrix, edge index, or links
print(result.codelength, result.num_top_modules)       # scalars: properties (no parentheses)
modules = result.modules()                              # {node_id: module_id} (method: parentheses)
df = result.to_dataframe()                              # per-node table: node_id, module_id, flow, path, name
row = result.summary()                                  # {metric: value} scalar row (one per run; keys = property names)
```

For a graph with non-integer (e.g. string) labels, the keys of `result.modules()` are Infomap **internal ids**, not the original labels. Recover labels with `result.names.get(nid, nid)` (the map is empty — an identity no-op — when labels are already integers), or the `name` column of `result.to_dataframe()`, or use `infomap.find_communities(graph, ...)`, which returns communities keyed by the graph's own labels.

Confirm exact method names and option coverage against the installed package and published docs for the user's version; do not copy version-sensitive examples verbatim.

## Writing native output files

To write the mapequation.org native files in Python, call the writers on the `Result` (partition artifacts) or the `Network` (input serializations) — not the option flags:

- `result.write_tree(path)` — `.tree`
- `result.write_flow_tree(path)` — `.ftree`
- `result.write_clu(path, depth=1)` — `.clu` at a chosen hierarchy depth
- `network.write_pajek(path)` — Pajek serialization of the input network
- `network.write_state_network(path)` — the internal state/multilayer network

The `tree`, `ftree`, `clu`, `clu_level`, `output`, `out_name`, `no_file_output`, `print_all_trials`, and `no_overwrite` **option flags are inert on the Python library surface**: setting them via `Options` writes no file and only emits a `UserWarning` (they act only through the raw `args` escape hatch together with an output directory). Write from the `Result`/`Network` instead.

## Sweeps (many runs → one table)

There is no sweep helper; users write their own loop. Do not invent one. The blessed pattern is a `Result` per run plus `result.summary()` (a `{metric: value}` dict keyed by the `Result` property names) collected into a tidy one-row-per-run `pandas.DataFrame` — the scikit-learn `pd.DataFrame(records)` idiom:

```python
import infomap, pandas as pd

records = []
for markov_time in (0.75, 1.0, 1.5):                    # or: for graph in graphs / for seed in seeds
    r = infomap.run(graph, markov_time=markov_time, seed=123, num_trials=20)
    records.append({"markov_time": markov_time, **r.summary()})   # add the swept params yourself

sweep = pd.DataFrame(records)                           # columns: markov_time + codelength, num_top_modules, ...
```

`result.to_series()` is the pandas-native single row (`pd.DataFrame([r.to_series() for r in results])`). `summary()`/`to_series()` read only the eager scalars, so they never raise on a re-run; they omit the lazy `effective_num_*`, the per-trial `codelengths`, and `meta_codelength` — add those columns explicitly if a sweep needs them.

Do not confuse `result.summary()` (this method, on a `Result`) with `im.summary()` on the stateful `Infomap` instance. The instance method is a network/run *state card* — it works before a run and keys module counts with the short card names (`status`, `top_modules`, `levels`), not the `Result` property names. For a sweep table, always collect `Result.summary()` (`num_top_modules`, `num_levels`, …).
