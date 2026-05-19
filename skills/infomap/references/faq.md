# FAQ and Troubleshooting

Use this reference when the user asks why a result looks surprising, why a run is slow, how to get an output field, or how to translate advice from old Infomap discussions to current versions. The entries below are distilled from high-signal GitHub Discussions and checked against current CLI/Python/R option names where possible.

## One module or fewer modules than expected

Start from `references/reproducibility.md` for codelength interpretation. A one-module result usually means the current representation and flow model did not find a modular description shorter than the one-level baseline.

Practical checks:

- Confirm weights mean stronger interaction/flow, not distance or dissimilarity.
- Confirm directionality and `--flow-model` / `--directed`.
- Run more trials or compare seeds if the network is stochastic or hard to optimize.
- For dense or aggregated networks, test whether filtering weak links with `--weight-threshold` changes the signal.
- For scale sensitivity, lower `--markov-time` / `markov_time` to explore smaller modules, but treat this as a modeling choice.
- To bias the search toward a target granularity, consider `--preferred-number-of-modules` / `preferred_number_of_modules`, but treat it as a modeling choice and report it.
- If the data can be stratified by context, model layers/state nodes instead of flattening.
- To evaluate an expected partition, use `--cluster-data PARTITION --no-infomap`, or Python `initial_partition` with `run(no_infomap=True)`.

Source discussions: [#241](https://github.com/mapequation/infomap/discussions/241), [#340](https://github.com/mapequation/infomap/discussions/340).

## Reproducibility differs across binaries or machines

Use `seed` and `num_trials`, and record the Infomap version, interface, build, options, and input. A seed is still the right starting point, but do not promise bit-identical results across all binaries, platforms, standard libraries, and OpenMP/build configurations. If exact reproduction matters, use the same Infomap version and build environment, and compare codelengths and partitions rather than assuming labels or node order are stable.

Recent/current Infomap builds are known to expose a standard-library based random mapping path. Keep the cross-platform caveat unless the installed version's docs or source explicitly document portable seeded permutations.

Source discussion: [#524](https://github.com/mapequation/infomap/discussions/524).

## Slow runs or low CPU usage

Check `infomap --version` / `Infomap --version` for OpenMP support. Some phases can be single-threaded even when the binary supports OpenMP. For Python, reading a network file with `im.read_file(...)` is usually much faster than adding many links in Python loops. Also check memory pressure: swapping can make runs look CPU-light and extremely slow.

For very large graphs, use runtime planning in `references/reproducibility.md`, start with one trial, and ask before launching many trials or parameter sweeps.

Source discussion: [#333](https://github.com/mapequation/infomap/discussions/333).

## Overlapping modules

Do not suggest the old `--overlapping` workflow for current Infomap. For ordinary first-order networks, Infomap returns hard assignments. To model overlap, represent the data as multilayer, memory, or raw state networks so one physical node can have multiple state nodes in different modules.

Source discussions: [#61](https://github.com/mapequation/infomap/discussions/61), [#228](https://github.com/mapequation/infomap/discussions/228).

## Multilayer output looks duplicated or confusing

In multilayer/state networks, one physical node may appear as multiple state nodes. A physical node can therefore appear in multiple modules across layers or contexts. For Python, iterate `im.nodes` and inspect `node.node_id`, `node.layer_id`, `node.state_id`, and `node.module_id`. For R, use `as.data.frame(im, states = TRUE)` or result node tables when available.

If output should distinguish layers, request state-level output. If output should merge physical nodes, say so explicitly and explain how merging affects interpretation.

Source discussions: [#267](https://github.com/mapequation/infomap/discussions/267), [#272](https://github.com/mapequation/infomap/discussions/272).

## Multilayer `--cluster-data` or initial partitions

For multilayer/state networks, initial partitions usually need state-level identifiers, not only physical node ids. When possible, write a state-level `.clu` from a known run and use that as the shape/template. Current CLI help says `--cluster-data` accepts a `.clu`, `.tree`, or `.ftree`, and `--no-infomap` can calculate codelength without optimization.

For Python, `im.initial_partition = {...}` or `im.run(initial_partition=..., no_infomap=True)` can evaluate a supplied partition. For CLI/R, use the current `cluster_data` and `no_infomap` option names.

Source discussions: [#329](https://github.com/mapequation/infomap/discussions/329), [#340](https://github.com/mapequation/infomap/discussions/340).

## CLI flags changed from old versions

When reproducing old commands, check `infomap -hh` or `Infomap -hh`. In current help, self-links are included by default and `-k` / `--include-self-links` is deprecated; use `--no-self-links` to exclude them. Current Infomap does not require zero-based indexing flags for ordinary non-consecutive ids.

Source discussions: [#138](https://github.com/mapequation/infomap/discussions/138), [#350](https://github.com/mapequation/infomap/discussions/350).

## What does flow mean, and where are Huffman codes?

Flow means random-walk visit rates used by the map equation to describe system-wide interdependencies. Infomap does not need to materialize actual Huffman codewords for each node in normal use; it uses flows to calculate the lower bound on expected codelength. In tree output, the flow is included as a column; use it for interpretation rather than trying to recover literal codewords.

Source discussions: [#177](https://github.com/mapequation/infomap/discussions/177), [#337](https://github.com/mapequation/infomap/discussions/337).

## Output formats and Network Navigator

Use `--ftree` or `--output ftree` when Network Navigator or aggregated links between modules are needed. In Python/R APIs, prefer the current writer/helper names from installed help. Do not assume `write_pajek` is a general export of the original user-facing multilayer input; check whether the desired output should be `.tree`, `.ftree`, `.clu`, `.csv`, `.json`, `states`, or graph-package export.

Source discussions: [#330](https://github.com/mapequation/infomap/discussions/330), [#283](https://github.com/mapequation/infomap/discussions/283).

## Install and wrapper confusion

Prefer the official `infomap` Python/R packages and current docs when users hit wrapper-specific issues from CDlib, old Conda packages, or old Python APIs. For Python API errors from old examples, inspect the installed package version and current object signatures. For source installs, modern Infomap requires a C++ toolchain with the language standard supported by the package version.

Source discussions: [#105](https://github.com/mapequation/infomap/discussions/105), [#171](https://github.com/mapequation/infomap/discussions/171), [#180](https://github.com/mapequation/infomap/discussions/180).

## Metadata beyond simple categories

For categorical metadata in current Infomap, use the installed metadata options or programmatic metadata APIs. For non-discrete or more elaborate metadata-dependent encoding, treat it as an advanced modeling problem rather than a simple option toggle; look for current research code and paper context before giving implementation advice.

Source discussion: [#343](https://github.com/mapequation/infomap/discussions/343).
