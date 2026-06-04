# Python Workflows

Use this reference when the user works in Python, notebooks, NetworkX, python-igraph, SciPy, edge-index, AnnData, Scanpy, pandas, or graph export workflows.

## Authority for current syntax

Do not treat this skill as the Python API manual. Before giving runnable code, inspect the installed package:

```python
import inspect
import importlib.metadata as md
import infomap

print(md.version("infomap"))
print(infomap.__file__)
print(inspect.signature(infomap.Infomap))
print(inspect.getdoc(infomap.Infomap))
print(inspect.signature(infomap.find_communities))
print(inspect.getdoc(infomap.find_communities))
```

For optional helpers, check availability before using them:

```python
hasattr(infomap, "InfomapOptions")
hasattr(infomap, "tl") and hasattr(infomap.tl, "infomap")
```

If inspection is run from an Infomap source checkout, verify `infomap.__file__` so Python has not imported repo-local sources by accident. If needed, run from another working directory or a clean environment. Use the published Python docs at `https://mapequation.org/infomap-python-docs/` when internet access is available.

## Choose the Python entry point

- Prefer a high-level community helper when the user has a NetworkX-style graph and only needs a partition with original labels. Verify the helper signature from the installed package.
- Prefer the `Infomap` class when the user needs codelength, flow, hierarchy paths, output files, state/multilayer handling, repeated runs, or tabular results.
- Prefer reusable option objects only if the installed package exposes them and the workflow benefits from a structured configuration.
- Prefer the AnnData/Scanpy helper only if the installed package exposes it and the user is working with observation graphs.

## Generating code

- Generate code after inspecting signatures or docs for the installed version.
- Keep examples small. Use one trial for smoke tests; use a meaningful `num_trials` value for research runs only after runtime is acceptable.
- Record package version, `infomap.__file__`, graph source, directed/weighted choice, seed, trials, non-default options, and output artifacts.
- Preserve mappings when labels are converted to internal integer ids.
- For state or multilayer output, state whether results are state-level or physical-node-level.

## Minimal patterns

For simple graph partitioning, use the installed high-level helper if available. For richer results, instantiate `Infomap`, add/read the network with installed methods, run, then extract modules, codelength, and node/state tables using methods confirmed by `help(...)` or `inspect`.

Avoid copying long API examples from this skill. The exact method names, signatures, and helper coverage should come from the installed package and published docs.
