# Python Workflows

Use this reference when the user works in Python, notebooks, NetworkX, python-igraph, SciPy, edge-index, AnnData, Scanpy, pandas, or graph export workflows.

## Sources to inspect

- `interfaces/python/source/quickstart.rst`
- `interfaces/python/source/usage.rst`
- `interfaces/python/source/scanpy.rst`
- `interfaces/python/source/export.rst`
- `examples/python/`
- `test/python/` for exact behavior when docs are not enough

## Choose the Python entry point

- Use `infomap.find_communities(...)` for NetworkX-style workflows where the user wants a partition and original node labels.
- Use `Infomap(...)` when the user needs codelength, flow, hierarchical paths, output files, state/multilayer handling, repeated runs, or tabular results.
- Use `InfomapOptions` when the same options should be reused across runs or documented as a structured configuration.
- Use `infomap.tl.infomap(adata, ...)` for AnnData/Scanpy-style observation graphs.

## Common patterns

NetworkX quick path:

```python
import networkx as nx
import infomap

graph = nx.karate_club_graph()
communities = infomap.find_communities(
    graph,
    weight="weight",
    seed=123,
    num_trials=20,
    module_attribute="infomap_module",
)
```

Explicit run with tabular output:

```python
from infomap import Infomap

im = Infomap(silent=True, seed=123, num_trials=20)
mapping = im.add_networkx_graph(graph)
im.run()

nodes = im.to_dataframe(
    columns=["node_id", "module_id", "flow"],
    index="node_id",
    sort=["module_id", "flow"],
)
```

SciPy sparse matrix:

```python
from infomap import Infomap

im = Infomap.from_scipy_sparse_matrix(
    adjacency,
    directed=False,
    args="--silent --seed 123 --num-trials 20",
)
im.run()
modules = im.get_modules()
```

Edge-index data:

```python
from infomap import Infomap

im = Infomap.from_edge_index(
    edge_index,
    edge_weight=edge_weight,
    directed=False,
    args="--silent --seed 123 --num-trials 20",
)
im.run()
```

Scanpy-style AnnData:

```python
import infomap

infomap.tl.infomap(
    adata,
    key_added="infomap",
    directed=False,
    use_weights=True,
    seed=123,
    num_trials=20,
)
```

## State and multilayer reminders

- NetworkX state networks use a `node_id` node attribute to map states to physical nodes.
- Multilayer NetworkX graphs additionally use `layer_id`.
- For state or multilayer output, be explicit whether results should be state-level or physical-node-level.
- Preserve and report mappings when non-integer labels are converted to internal ids.

## Export and reporting

- Use `to_dataframe()` when a paper or pipeline needs node-level tables.
- Use `get_modules()` for simple module mappings.
- Use `infomap.export.write_graphml` or `write_gexf` for NetworkX visualization workflows.
- Record package version, graph source, directed/weighted choice, seed, `num_trials`, and non-default options.
