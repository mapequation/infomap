# CLI Workflows

Use this reference when the user wants native Infomap commands, file-based analysis, batch jobs, or wrapper-independent output.

## Authority for current syntax

Do not treat this skill as the CLI manual. Before giving runnable commands, inspect the installed binary:

```bash
INFOMAP_BIN="${INFOMAP_BIN:-infomap}"
command -v "$INFOMAP_BIN" || command -v Infomap
"$INFOMAP_BIN" --version
"$INFOMAP_BIN" --help
"$INFOMAP_BIN" -hh
"$INFOMAP_BIN" --print-json-parameters
```

If `infomap` is not found, try `Infomap`. Installed Python packages often provide `infomap`; native builds may expose `Infomap`.

Use the published user guide at `https://www.mapequation.org/infomap/` for file formats and examples. Use source checkout files only when the user is actually working inside an Infomap repository.

## Stable command shape

The stable CLI pattern is:

```bash
Infomap network_file out_directory [options]
```

When the binary name is uncertain, write examples with `INFOMAP_BIN` and tell the agent to verify current option names from help before running.

## Generating commands

- Prefer CLI for existing network files, batch jobs, shell pipelines, native output files, or wrapper-independent reproducibility.
- For validation, use tiny inputs or one trial first. Ask before launching large files, many trials, multiple seeds, parameter sweeps, notebooks, or runs likely to take more than a few minutes.
- Make research commands record the binary version, exact command, input checksum, non-default options, and output directory.
- Use `--print-json-parameters` when available to log the installed version's actual parameter names and defaults.
- For edge lists, state/multilayer input, metadata, bipartite data, and output formats, verify the installed help or published guide before making format-specific claims.

Portable checksum snippet for run logs:

```bash
if command -v sha256sum >/dev/null 2>&1; then
  sha256sum data/input.tsv
else
  shasum -a 256 data/input.tsv
fi
```

## Output choice prompts

- Ask whether the user needs one module id per node, a hierarchy, state-level output, physical-node output, Network Navigator input, or machine-readable tables.
- Choose native output formats only after checking current help, because option names and writer behavior are version-sensitive.
- When a user cites an old command from a discussion or paper, translate it through current `--help`, `-hh`, and `--print-json-parameters` rather than copying it verbatim.
