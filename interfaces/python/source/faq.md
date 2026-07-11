# FAQ & common pitfalls

Short answers to the questions that trip people up most often, each linking to
the chapter that covers it in full.

## Getting data in

### How do I load my graph into Infomap?

Hand any of these to `infomap.run(...)`: a NetworkX or python-igraph graph, a
SciPy sparse adjacency matrix, a `(2, E)` edge index, an edge list, or a network
file. For non-default loading, build a `Network` with `Network.from_networkx`,
`from_igraph`, `from_scipy_sparse_matrix`, or the `add_*` verbs. The quickest
path for a NetworkX graph is `infomap.run(g)` (or `infomap.find_communities(g)`
for a `list[set]`). Bundled example networks live in {mod}`infomap.datasets`.
See {doc}`Building a network <working-with-infomap/inputs>`.

### Why does `infomap.run(A, directed=True)` raise `TypeError` for a matrix or edge index?

For a SciPy sparse matrix or a `(2, E)` edge index, `directed` names the *input
adapter's* edge orientation, not the engine's flow model, so `run()` rejects it
rather than silently build a different graph. Build the network explicitly with
`Network.from_scipy_sparse_matrix(A, directed=True)` or
`Network.from_edge_index(ei, directed=True)`, or ask for the directed flow model
with `options=infomap.Options(flow_model="directed")`. For a graph, file, or
edge list, `infomap.run(g, directed=True)` works directly. See
{doc}`Building a network <working-with-infomap/inputs>`.

## Using the Python API

### Why does `result.codelength()` raise `TypeError: 'float' object is not callable`?

On a `Result`, scalar metrics are **properties** and collections are
**methods**: read `result.codelength` and `result.num_top_modules` *without*
parentheses, and call `result.modules()`, `result.nodes()`, `result.tree()`, and
`result.to_dataframe()` *with* them. (On the stateful `Infomap` instance those
same names are properties -- another reason to read results off the returned
`Result`, not the instance.) See
{doc}`Reading the Result <working-with-infomap/results-and-iteration>`.

### Why does `result.modules` give a bound method, or `result.modules().items` fail?

Collections are **methods** -- call `result.modules()`, not `result.modules`.
Accessing it without parentheses returns the method object, so
`result.modules.items()` raises `AttributeError: 'function' object has no
attribute 'items'`. And `result.modules()` returns a `{node_id: module_id}`
dict: iterate `result.modules().items()`. Code ported from the stateful
instance can also raise `TypeError: cannot unpack non-iterable int object` --
its `modules` was a *property* that yielded `(node_id, module_id)` pairs, but
iterating the `Result`'s dict yields keys, so unpack `result.modules().items()`
instead. See {doc}`Reading the Result <working-with-infomap/results-and-iteration>`.

### How do I see Infomap's progress or engine log in Python?

The Python API is quiet by default. Call `infomap.enable_log()` once to route the
engine log through the standard `logging` module
(`infomap.enable_log(logging.DEBUG)` for more detail), and `infomap.disable_log()`
to stop. Prefer this over the legacy `silent` keyword, which is pending-deprecated
and leaves the API in 3.0. One caveat: `enable_log()` covers the `Infomap` class
and `infomap.run(...)` on an edge list, graph, or file path, but a pre-built
`Network` (including the bundled {mod}`infomap.datasets`) runs silently for its
whole lifetime, so `enable_log()` does not capture its run (a `UserWarning` says
as much) -- run through `Infomap(...)` or pass the raw input to `infomap.run` for
those. See
{doc}`Running Infomap and tuning options <working-with-infomap/running-and-options>`.

### What does `infomap.find_communities` return, and how does it differ from `infomap.run`?

The finders return the partition in the graph's own labels: `find_communities`
a NetworkX-style `list[set]` of node labels, `find_igraph_communities` an
`igraph.VertexClustering` (keyed by vertex index). `infomap.run` instead returns
a {class}`~infomap.Result` with the codelength, hierarchy, dataframe, and
exporters. All three default to `num_trials=1`, so raise it -- `num_trials=` (or
the `trials=` alias on the finders) -- for research runs. See
{doc}`Reading the Result <working-with-infomap/results-and-iteration>`.

## Results and determinism

### Why do different seeds give slightly different partitions?

Infomap's search is stochastic: each trial starts from a different random node
order and can settle in a different local optimum, so changing `seed=` can
change the partition. Repeated runs with the same seed are identical (the
default is `seed=123`). Run several trials with `num_trials=` and Infomap
keeps the lowest-codelength result. See {doc}`The map equation <concepts/the-map-equation>`.

### How many trials should I use, and how do I know the result is reliable?

For exploration, one to about five trials; `num_trials=10` for most
analyses; and `num_trials=20` or more (or `options=Options(converge=True)`) for
results you publish. To gauge reliability, run several single-trial fits and compare
codelengths: a tight spread means the partition is stable, a wide spread signals
degeneracy. See {doc}`Running Infomap and tuning options <working-with-infomap/running-and-options>`.

### Why does `seed=0` raise an error?

Infomap requires a seed ≥ 1; use any positive integer for reproducible runs.
See {doc}`Running Infomap and tuning options <working-with-infomap/running-and-options>`.

### What does `result.codelength` measure?

It is the value of the map equation: the average number of bits per step needed
to describe a random walk under the best code for the partition Infomap found.
Lower means a more compressive, and usually more meaningful, partition.
Compare it to `result.one_level_codelength` (no modules) to see how much structure
Infomap found. See {doc}`The map equation <concepts/the-map-equation>`.

## Number and size of modules

### Why does Infomap find more (or fewer) modules than I expected?

Infomap minimises a flow-based description length, not a target count or a
sociological ground truth. Boundary nodes where the random walker mixes can form
their own module when doing so shortens the overall description, so an unexpected
module count is not necessarily an error. See
{doc}`The map equation <concepts/the-map-equation>`.

### Two-level or multilevel: when should I pass `two_level=True`?

Pass `two_level=True` for a flat partition (every node in exactly one top
module). Omit it (the default) to let Infomap discover hierarchical structure.
See {doc}`The map equation <concepts/the-map-equation>`
and {doc}`Hierarchy and the multilevel map equation <concepts/hierarchy-and-the-multilevel-map>`.

### Does Infomap choose the hierarchy depth for me, and how do I read each level?

Yes: multilevel is the default, and Infomap adds levels only when they shorten
the description. Read a given level with `result.modules(depth=k)`
(`k=1` is the coarsest, `depth=-1` the finest). Check `result.num_levels`.
See {doc}`Hierarchy and the multilevel map equation <concepts/hierarchy-and-the-multilevel-map>`.

## Flow and directed networks

### What is "flow" in Infomap's output?

Flow is each node's stationary visit frequency for the random walk Infomap uses
to model how information moves; it sums to 1 across nodes. Communities are
regions where flow lingers. See {doc}`Flow and random walks <concepts/flow-and-random-walks>`.

### Why does Infomap teleport on directed networks?

A directed walk can get trapped in sinks, leaving the stationary distribution
ill-defined. Teleportation restores ergodicity; Infomap uses an unrecorded
scheme so the artificial jumps don't inflate apparent cross-module traffic.
See {doc}`Flow and random walks <concepts/flow-and-random-walks>`.

## Output and export

### What's the difference between `result.modules()` and `result.to_dataframe()`?

`result.modules()` returns a lightweight `{node_id: module_id}` dict, ideal for
colouring or feeding another algorithm. `result.to_dataframe([...])` returns a
pandas DataFrame with node id, module id, flow, and hierarchical path, better for
filtering, grouping, and export. See {doc}`Reading the Result <working-with-infomap/results-and-iteration>`.

### Why are `result.modules()` keys integers instead of my node labels?

`infomap.run(graph)` maps non-integer node labels to internal integer ids, so
`result.modules()` and `node.node_id` are keyed by those ids. The mapping is kept
on `result.names` (`{internal_id: label}`). Note it is **empty when your labels
are already integers** (contiguous or not) -- then the ids *are* your labels, so
there is nothing to map. The `result.names.get(n, n)` idiom below therefore works
either way (it falls back to the id when `names` has no entry):

```python
named = {result.names.get(n, n): m for n, m in result.modules().items()}
```

`result.to_dataframe()` already includes a `name` column, and
`infomap.find_communities(graph)` returns a `list[set]` in your original labels.
See {doc}`Building a network <working-with-infomap/inputs>`.

### How do I export results to Gephi or to `.tree` / `.clu` files?

For Gephi, annotate the graph with `infomap.to_networkx(result)` (or `to_igraph`)
and write it with `nx.write_graphml` / `nx.write_gexf`. For the native formats,
write straight from the `Result` with `result.write_tree(path)` and
`result.write_clu(path)`: `.clu` is one row per node, `.tree` carries the full
hierarchy. See {doc}`Visualising and exporting <working-with-infomap/visualizing-and-exporting>`.

## Flow models and representations

### Why does `result.modules()` raise "Cannot get modules on higher-order network without states"?

Multilayer and memory networks partition *state nodes*, so a physical node can
belong to several modules. Call `result.modules(states=True)`, then map state
nodes back to physical nodes (and layers) via `result.nodes(states=True)`. See
{doc}`Multilayer networks <flow-models/multilayer>` and {doc}`Memory and state networks <flow-models/memory-and-state>`.

### What multilayer relax rate should I use?

The default `multilayer_relax_rate=0.15` suits many networks; higher rates couple
layers more (toward the aggregate), lower rates keep layers independent. If you
have real inter-layer links, add them with `add_multilayer_inter_link` and the
relax rate is ignored. See {doc}`Multilayer networks <flow-models/multilayer>`.

### How do I model a network with memory (higher-order flow)?

Declare state nodes on a `Network` with `add_state_node(state_id, node_id)`,
where `node_id` is the physical node, and link them with `add_link`. Memory lets a
physical node appear in overlapping modules, so read the partition with
`result.modules(states=True)`. See {doc}`Memory and state networks <flow-models/memory-and-state>`.

### How do I cluster a time-varying (temporal) network?

Represent each time window as a layer of a multilayer network
(`add_multilayer_intra_link` per window) and let the relax rate carry community
identity across time. See {doc}`Temporal networks <flow-models/temporal>`.

### My codelength goes up when I set `meta_data_rate > 0`: is that wrong?

No. `result.codelength` reports the *combined* objective the search minimises:
the map equation plus `meta_data_rate` times the metadata codelength. The
search trades higher topological codelength for
attribute-homogeneous modules, and the metadata term adds to the reported
value. See {doc}`Networks with metadata <flow-models/metadata>`.

### How do I tell Infomap my network is bipartite?

Number the nodes so the second type starts at a fixed id, set
`net.bipartite_start_id = start_id` on your `Network`, then `infomap.run(net)`.
See {doc}`Bipartite networks <flow-models/bipartite>`.

### Can I run Infomap directly on a hypergraph?

Not directly: encode the hypergraph as a network first (e.g. a bipartite
incidence network with `bipartite_start_id` set). The representation you choose
changes the communities found. See {doc}`Bipartite networks <flow-models/bipartite>`.

## Robustness and reliability

### My sparse network gives lots of tiny modules: how do I avoid overfitting?

Standard Infomap overfits sparse or incomplete data. Pass
`options=infomap.Options(regularized=True)` (tune with `regularization_strength=`)
for a Bayesian prior that yields fewer, more reliable modules. See {doc}`Incomplete data and regularization <robustness/incomplete-data>`.

## Workflows and integrations

### How does Infomap relate to Louvain and Leiden?

All three partition a network, but Infomap optimises the compression of flow while
Louvain and Leiden maximise modularity (density against a null model). They tend to
agree on clear structure and read a network differently when flow direction or
scale matters. See {doc}`Reading Infomap through Louvain and Leiden <concepts/choosing-a-method>`.

### How does Infomap relate to statistical inference methods such as stochastic block models?

They answer different questions. Infomap asks how well a partition compresses
the flow of a random walk on the network you observed; inference methods ask how
plausibly a generative model explains the observed edges, and they bring their
own machinery for model selection. For that family of methods,
[graph-tool's documentation](https://graph-tool.skewed.de/) is a good starting
point.

### How do I use Infomap inside a Scanpy / AnnData workflow?

After `sc.pp.neighbors(adata)`, call `infomap.tl.infomap(adata, key_added="infomap")`.
It clusters `adata.obsp["connectivities"]` and writes labels to `adata.obs` (and a
run-metadata dict, including the codelength, to `adata.uns["infomap"]`). See
{doc}`Scanpy and AnnData <workflows/scanpy>`.

### Can I run Infomap on GraphRAG entity/relationship tables?

Yes: the `infomap.tl.graphrag` adapter reads entity/relationship tables (columns `id`,
`title`, `source`, `target`, `weight`) and returns a community hierarchy. Use
`run_graphrag_communities(...)` (omit `output_dir` to stay in memory). See
{doc}`GraphRAG tables <workflows/graphrag>`.

### How do I make Infomap use only the cores my scheduler allocated?

By default (`num_threads` unset, i.e. the engine's `auto` mode) the thread
budget resolves from, in priority order, `INFOMAP_NUM_THREADS`,
`SLURM_CPUS_PER_TASK`, `OMP_NUM_THREADS`, the process cpuset, and finally the
hardware. Override it by carrying `num_threads` via `infomap.Options` (or set
`INFOMAP_NUM_THREADS`). See {doc}`Running at scale (HPC) <workflows/hpc>`.

### How do I run many trials across cluster jobs and combine them?

Give each job a non-overlapping range with `trial_offset` and write its shard
with `trial_results` (carried via `infomap.Options`, or as CLI flags), then
merge the shard files with `python -m infomap.merge` (it keeps the best
codelength). See {doc}`Running at scale (HPC) <workflows/hpc>`.

## Method and citation

### Is the map equation the same as modularity?

No. Modularity counts link weights against a random-wiring null model; the map
equation compresses random-walk trajectories. They agree when flow concentrates
in modules and diverge otherwise (notably on directed source–sink networks).
See {doc}`The map equation <concepts/the-map-equation>`.

### How do I cite Infomap?

Cite both the 2008 PNAS paper (the map equation) and the MapEquation software
package (the implementation and version, for reproducibility). Cite the survey
{cite:p}`smiljanic2026survey` as well when your work builds on the wider
framework. BibTeX and guidance on citing specific extensions are in
{doc}`How to cite Infomap <citing>`.
