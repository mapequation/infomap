# CLI Workflows

Use this reference when the user wants native Infomap commands, file-based analysis, batch jobs, or wrapper-independent output.

## Sources to inspect

- `./Infomap --help`
- `./Infomap -hh` for advanced options
- `./Infomap --print-json-parameters` for current parameter names
- `README.rst`
- `examples/networks/`
- `src/io/Config.cpp` and `src/io/Network.cpp` only when help text is not enough

## Basic shape

The native command shape is:

```bash
Infomap network_file out_directory [options]
```

From a source checkout, examples may use `./Infomap`. Installed Python packages may provide an `infomap` entry point. Verify which binary is on `PATH` before giving exact commands.

## Reproducible command pattern

```bash
mkdir -p results/karate
./Infomap data/karate.net results/karate \
  --flow-model undirected \
  --seed 123 \
  --num-trials 20 \
  --tree \
  --clu

./Infomap --version > results/karate/infomap-version.txt
./Infomap --print-json-parameters > results/karate/infomap-parameters.json
```

Weighted directed edge-list pattern:

```bash
mkdir -p results/transitions
./Infomap data/transitions.tsv results/transitions \
  --directed \
  --seed 123 \
  --num-trials 20 \
  --tree \
  --clu \
  --silent

{
  printf '%s\n' './Infomap data/transitions.tsv results/transitions --directed --seed 123 --num-trials 20 --tree --clu --silent'
  ./Infomap --version
  shasum -a 256 data/transitions.tsv
  ./Infomap --print-json-parameters
} > results/transitions/run-log.txt
```

For a weighted link list, ensure the input columns and delimiter match the current supported format. The usual edge-list shape is source, target, and optional weight; inspect the user guide, examples, or parser tests before giving format-specific guarantees.

Use `--silent` for scripts when console progress is not needed. Use `-v`, `-vv`, or `-vvv` when diagnosing a run.

## Common options

- `--flow-model undirected|directed|undirdir|outdirdir|rawdir|precomputed`: choose how Infomap derives flow from links. Verify current choices from help.
- `--directed`: shorthand for directed flow modeling.
- `--two-level`: optimize a two-level partition instead of the default multilevel hierarchy.
- `--seed INTEGER`: set the random number generator seed.
- `--num-trials INTEGER`: run independent trials and keep the best result.
- `--tree`: write hierarchy output.
- `--ftree`: write hierarchy plus aggregated links for Network Navigator.
- `--clu`: write top-level module ids for each node.
- `--cluster-data PATH`: read an initial partition or hierarchy.
- `--no-infomap`: skip optimization, useful for codelength or non-modular statistics with supplied cluster data.

## Input and output guidance

- Infomap assumes link-list input unless a file starts with a Pajek-style heading.
- Use weighted input when weights carry interaction strength or observed flow frequency.
- Use directed options when edge orientation is part of the process.
- For state, multilayer, metadata, or bipartite formats, inspect current docs/examples before writing a full command.
- Choose `--clu` when the user needs one top-level module id per node.
- Choose `--tree` when the user needs hierarchy, flow, names, or downstream parsing.
- Choose `--ftree` when Network Navigator or aggregated module links are needed.
