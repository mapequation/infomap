# CLI Workflows

Use this reference when the user wants native Infomap commands, file-based analysis, batch jobs, or wrapper-independent output.

## Sources to inspect

- `infomap --help` or `Infomap --help`, depending on the installed binary name.
- `infomap -hh` or `Infomap -hh` for advanced options.
- `infomap --print-json-parameters` or `Infomap --print-json-parameters` for current parameter names.
- The published user guide at `https://www.mapequation.org/infomap/`.
- Source checkout files only when the user is actually working inside an Infomap repository.

## Basic shape

The native command shape is:

```bash
Infomap network_file out_directory [options]
```

Installed Python packages usually provide an `infomap` entry point; native builds may expose `Infomap`. Verify which binary is on `PATH` before giving exact commands. In shell examples, use `INFOMAP_BIN` when the binary name is unknown.

## Reproducible command pattern

For examples in answers, `--num-trials 20` is a good research default. For local validation that the command and input format work, run a smoke test with `--num-trials 1` first. Ask before launching commands on large files, many trials, multiple seeds, parameter sweeps, or jobs likely to take more than a few minutes.

```bash
INFOMAP_BIN="${INFOMAP_BIN:-infomap}"
mkdir -p results/karate
"$INFOMAP_BIN" data/karate.net results/karate \
  --flow-model undirected \
  --seed 123 \
  --num-trials 20 \
  --tree \
  --clu

"$INFOMAP_BIN" --version > results/karate/infomap-version.txt
"$INFOMAP_BIN" --print-json-parameters > results/karate/infomap-parameters.json
```

Weighted directed edge-list pattern:

```bash
INFOMAP_BIN="${INFOMAP_BIN:-infomap}"
mkdir -p results/transitions
"$INFOMAP_BIN" data/transitions.tsv results/transitions \
  --directed \
  --seed 123 \
  --num-trials 20 \
  --tree \
  --clu \
  --silent

{
  printf 'binary=%s\n' "$INFOMAP_BIN"
  printf '%s\n' "$INFOMAP_BIN data/transitions.tsv results/transitions --directed --seed 123 --num-trials 20 --tree --clu --silent"
  "$INFOMAP_BIN" --version
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum data/transitions.tsv
  else
    shasum -a 256 data/transitions.tsv
  fi
  "$INFOMAP_BIN" --print-json-parameters
} > results/transitions/run-log.txt
```

For a weighted link list, ensure the input columns and delimiter match the current supported format. The usual edge-list shape is source, target, and optional weight; inspect the installed help or user guide before giving format-specific guarantees.

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
- For state, multilayer, metadata, or bipartite formats, inspect installed help, published docs, or user-provided examples before writing a full command.
- Choose `--clu` when the user needs one top-level module id per node.
- Choose `--tree` when the user needs hierarchy, flow, names, or downstream parsing.
- Choose `--ftree` when Network Navigator or aggregated module links are needed.
