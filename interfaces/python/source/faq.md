# FAQ & common pitfalls

Short answers to the questions that trip people up most often. Each answer
links to the chapter that explains it properly.

## Getting data in

### How do I load my graph into Infomap?

Hand any of these to `infomap.run(...)`: a NetworkX or python-igraph graph, a
SciPy sparse adjacency matrix, a `(2, E)` edge index, an edge list, or a network
file. For non-default loading, build a `Network` with `Network.from_networkx`,
`from_igraph`, `from_scipy_sparse_matrix`, or the `add_*` verbs. The quickest
path for a NetworkX graph is `infomap.run(g)` (or `infomap.find_communities(g)`
for a `list[set]`). Bundled example networks live in {mod}`infomap.datasets`.
See {doc}`Building a network <working-with-infomap/inputs>`.

## Results and determinism

### Why do different seeds give slightly different partitions?

Infomap's search is stochastic: each trial starts from a different random node
order and can settle in a different local optimum, so changing `seed=` can
change the partition. Repeated runs with the same seed are identical (the
default is `seed=123`). Run several trials with `num_trials=` and Infomap
keeps the lowest-codelength result. See {doc}`The map equation <concepts/the-map-equation>`.

### How many trials should I use, and how do I know the result is reliable?

For exploration, 1–5 trials; for results you publish, use `num_trials=20` or
more (or `converge=True`). To gauge reliability, run several single-trial fits
and compare codelengths: a tight spread means the partition is stable, a wide
spread signals degeneracy. See {doc}`Running Infomap and tuning options <working-with-infomap/running-and-options>`.

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
their own module if naming it shortens the overall description, so an unexpected
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

### How do I export results to Gephi or to `.tree` / `.clu` files?

For Gephi, annotate the graph with `infomap.to_networkx(result)` (or `to_igraph`)
and write it with `nx.write_graphml` / `nx.write_gexf`. For the native formats,
the stateful `Infomap` writes them with `im.write_tree(path)` and
`im.write_clu(path)`: `.clu` is one row per node, `.tree` carries the full
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

Declare state nodes on a `Network` with `add_state_node(state_id, physical_id)`
and link them with `add_link`. Memory lets a physical node appear in overlapping
modules. See {doc}`Memory and state networks <flow-models/memory-and-state>`.

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

### My sparse network gives lots of tiny modules: how do I avoid overfitting?

Standard Infomap overfits sparse or incomplete data. Pass `regularized=True`
(tune with `regularization_strength=`) for a Bayesian prior that yields fewer,
more reliable modules. See {doc}`Incomplete data and regularization <robustness/incomplete-data>`.

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

Yes: the `infomap.graphrag` adapter reads entity/relationship tables (columns `id`,
`title`, `source`, `target`, `weight`) and returns a community hierarchy. Use
`run_graphrag_communities(...)` (omit `output_dir` to stay in memory). See
{doc}`GraphRAG tables <workflows/graphrag>`.

### How do I make Infomap use only the cores my scheduler allocated?

The default `num_threads="auto"` already resolves the budget from, in priority
order, `INFOMAP_NUM_THREADS`, `SLURM_CPUS_PER_TASK`, `OMP_NUM_THREADS`, the
process cpuset, and finally the hardware. Set `num_threads=` to a positive
integer to override. See {doc}`Running at scale (HPC) <workflows/hpc>`.

### How do I run many trials across cluster jobs and combine them?

Give each job a non-overlapping range with `trial_offset=` / `trial_results=`, then merge
the shard files with `python -m infomap.merge` (it keeps the best codelength). See
{doc}`Running at scale (HPC) <workflows/hpc>`.

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
