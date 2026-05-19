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
- Save both machine-readable results and human-readable notes.
- Preserve mappings from internal ids to original node labels.
- Keep state-level and physical-node-level outputs separate when higher-order or multilayer data are involved.

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
