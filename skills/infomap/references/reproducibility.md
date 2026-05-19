# Reproducibility and Reporting

Use this reference when the user asks for a method section, provenance checklist, run logging, result interpretation, or publication-ready workflow.

## Minimum run record

Record:

- Infomap version and interface: CLI, Python package, or R package.
- Input source, preprocessing steps, node/link definitions, and whether links are weighted or directed.
- Flow model or directedness choice.
- Non-default options, especially `two_level`, state/multilayer/metadata/bipartite settings, and output options.
- `seed` and `num_trials`.
- Output files or objects used for downstream analysis.
- Number of top modules and codelength for the reported run.

## Recommended defaults

- Set `seed` for repeatability.
- Use more than one trial for serious analyses; `num_trials=20` is a practical default in examples, but large or time-sensitive runs may need adjustment.
- Separate smoke checks from research runs: use small inputs or one trial to validate setup, then ask before launching long runs, sweeps, repeated seeds, or notebook executions.
- Save both machine-readable results and human-readable notes.
- Preserve mappings from internal ids to original node labels.
- Keep state-level and physical-node-level outputs separate when higher-order or multilayer data are involved.

## Runtime planning

Use these local benchmark results only as order-of-magnitude guidance. They were measured on macOS arm64 with Infomap 2.10.1, `seed=123`, no file output, and wall-clock time around `read_file + run`. Python used the package API from a virtual environment; R includes one-shot `Rscript` startup overhead.

| Network | Size | States | Links | Trials | CLI | Python | R |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| state ring | 0.47 MB | 10k | 20k | 1 | 0.25s | 1.06s | 0.60s |
| state ring | 0.47 MB | 10k | 20k | 20 | 2.07s | 2.32s | 5.87s |
| sparse graph | 0.61 MB | 0 | 50k | 1 | 0.24s | 0.60s | 0.44s |
| sparse graph | 0.61 MB | 0 | 50k | 20 | 3.17s | 3.64s | 4.15s |
| first-order edge list | 16.9 MB | 0 | 1M | 1 | 3.80s | 4.11s | 8.22s |
| first-order edge list | 16.9 MB | 0 | 1M | 5 | 16.68s | 17.55s | 41.40s |
| state network | 19.2 MB | 400k | 800k | 1 | 10.62s | 11.51s | 26.49s |

Practical gating:

- Run without asking for tiny examples, smoke checks, and single-trial runs on networks around 1M links or less when the user asked for execution.
- Ask before running `num_trials=20` on million-link or state-heavy networks unless the user explicitly requested a research run.
- Ask before parameter sweeps, repeated seeds, full notebooks, Docker/Jupyter startup, multilayer/state networks beyond these sizes, or inputs where size is unknown.

## Method-section checklist

An English method paragraph should usually state:

- what the network represents;
- whether links are directed and/or weighted;
- which Infomap interface and version were used;
- the key options, including flow model when relevant;
- `seed` and `num_trials`;
- whether a two-level or multilevel solution was used;
- which output was analyzed: top-level modules, hierarchy paths, state nodes, physical nodes, or exported graph attributes.

## Template method text

Use this as a starting point and adapt it to the actual run:

```text
We detected flow-based communities with Infomap using the map equation framework. The network nodes represented [entities], and links represented [interaction or flow definition]. Links were treated as [directed/undirected] and [weighted/unweighted]. We ran Infomap [version/interface] with seed 123 and 20 trials, keeping the solution with the shortest codelength. We analyzed [top-level modules / the multilevel hierarchy / state-level assignments] and recorded the input data, non-default options, codelength, and module assignments for reproducibility.
```

## Result interpretation

- `codelength` summarizes compression quality for the chosen model and run.
- `num_top_modules` is the number of top-level modules.
- Hierarchical output paths encode nested modules; do not collapse them unless the user wants top-level labels.
- State nodes can allow the same physical node to appear in different modules; report whether assignments are state-level or merged to physical nodes.

## Codelength and one-module results

When Infomap returns one top module, the reported `codelength` is normally equal to `one_level_codelength`. That equality does not diagnose the cause by itself; it says the selected result is the non-modular baseline.

- `one_level_codelength` is the non-modular baseline: one codebook for the whole network flow.
- `codelength` is the total description length of the selected partition.
- For modular results, `codelength = index_codelength + module_codelength`.
- `module_codelength` alone is not the value to compare against `one_level_codelength`; the index codebook also costs bits.

Infomap may collapse to one module when candidate modular partitions do not improve the total codelength over the one-level solution. This usually means that, under the current network representation and flow model, modules do not compress the flow better than a one-level description. It is not automatically an error.

If the user expected multiple modules, check:

- **Input semantics**: weights should mean stronger flow or interaction, not distances or dissimilarities unless transformed.
- **Direction and flow model**: verify directedness, observed-flow assumptions, and `flow_model`/`--directed` choices.
- **Network construction**: check that ids, delimiters, isolated nodes, duplicate links, self-links, and component structure match the intended graph.
- **Search stability**: rerun with a fixed `seed`, then increase `num_trials` or compare several seeds to rule out a poor local optimum.
- **Supplied partition diagnostic**: initialize from a candidate partition with `--cluster-data` and run `--no-infomap` to calculate its codelength without optimization. This helps test whether the expected partition compresses flow better than the one-level baseline.
- **Resolution scale**: for exploratory sensitivity analysis, try lower `markov_time` values to look for smaller-scale modules; this is a modeling choice and needs extra thought before being used in a reported result.
- **Preferred module count**: `preferred_number_of_modules` / `--preferred-number-of-modules` can explore what partitions near a target count look like; this is also a modeling choice, not neutral evidence that the data contain that many modules.
- **Representation**: if the data have layers, sequence memory, metadata, or bipartite structure, model those explicitly instead of flattening away the signal.
- **Expectation check**: if no tested representation improves on the one-level codelength, report the one-module result rather than forcing a partition.

Avoid presenting forced module counts as evidence of community structure. Non-default Markov time and preferred module counts can be useful for sensitivity analysis, but they change the model and must be reported.
