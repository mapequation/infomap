---
name: infomap
description: Use when helping someone USE Infomap as an analysis tool — running the map equation for community detection via the Infomap CLI, Python package, R package, or notebooks; choosing a network representation or flow model (multilayer, memory/state, metadata, bipartite); reproducible analysis; result interpretation; or method selection. NOT for developing the Infomap software itself: editing its source (src/, the C++/SWIG bindings, CMake, CI, vendored libs) or building/debugging/testing the codebase is ordinary software development, not an Infomap analysis task.
---

# Infomap Research Workflows

Use this skill to help researchers run, explain, adapt, or troubleshoot Infomap analyses with the CLI, Python package, R package, and survey companion notebooks.

## When NOT to use this skill

This skill is for *using* Infomap to analyze networks. It is **not** for developing Infomap itself. If the task is editing, building, profiling, debugging, or testing the Infomap source — the C++ in `src/`, the SWIG Python/R bindings, CMake/CI, vendored libraries, the CLI parameter catalog, or the repo's own test suite — that is ordinary software development; do not use this skill, even when working inside the Infomap repository. The presence of Infomap source files or CLI/option names in the task is not a trigger; the trigger is a user analyzing a network with Infomap.

## First classify the task

Identify the user's mode before answering:

- **Analysis planning**: choose a network representation, flow model, interface, outputs, and reproducibility checklist. Read `references/method-selection.md` and usually `references/reproducibility.md`.
- **Code or command generation**: choose the smallest practical interface, then read `references/cli.md`, `references/python.md`, or `references/r.md`.
- **Notebook workflow**: read `references/notebooks.md`; also read `references/python.md` when converting notebook ideas into scripts.
- **Result interpretation or method text**: read `references/reproducibility.md`; read `references/faq.md` for common interpretation traps.
- **Usage troubleshooting**: inspect the installed package help, CLI help, or online user docs first, then read `references/faq.md` and the relevant interface reference.

## Choose the interface

- Prefer **Python** for notebooks, scripts, NetworkX, python-igraph, SciPy sparse matrices, edge-index data, AnnData/Scanpy workflows, tabular outputs, and GraphML/GEXF export.
- Prefer **R** for R-native analysis, R igraph workflows, R Markdown-style reports, and users who want `cluster_infomap()` or the R6 `Infomap` API.
- Prefer **CLI** for file-based workflows, batch jobs, shell pipelines, native output files, or wrapper-independent runs.

When the user already chose an interface, stay there unless a different interface is clearly necessary.

## Source rules

- Do not assume the user has an Infomap source checkout. Most users will only have the CLI, Python package, R package, or notebook image installed.
- Treat installed help and published docs as the normal authority for current syntax. Use source files only when the user is working inside an Infomap checkout or explicitly provides repo files.
- For CLI details, inspect the available binary with `infomap --help`, `Infomap --help`, advanced help, or `--print-json-parameters` when available.
- For Python details, prefer installed package help (`help(...)`), `inspect.signature(...)`, package metadata, published Python docs, and package examples available to the user.
- For R details, prefer installed help (`utils::help(..., package = "infomap")`), `args(...)`, `packageVersion("infomap")`, and package exports.
- Do not copy version-sensitive examples from this skill as if they were authoritative. Generate runnable code only after checking the installed interface or published docs for the user's version.
- Use the survey article as a decision guide for representation, flow modeling, higher-order networks, metadata, bipartite networks, incomplete data, and applications: `https://doi.org/10.1145/3779648`. Do not quote long passages from it.

## Default research standards

- Make examples reproducible by default: include `seed`, use meaningful `num_trials`, record Infomap version, input provenance, non-default options, and output artifacts.
- Avoid starting expensive runs without consent. Use tiny smoke examples or `num_trials=1` for validation; read `references/reproducibility.md` for rough runtime hints before running large networks, notebook images, parameter sweeps, or many trials.
- Distinguish top-level module assignments from hierarchical paths, and distinguish physical nodes from state nodes for higher-order or multilayer inputs.
- Explain `two_level` versus multilevel only as much as the task needs.
- Do not block users with broad method-validity warnings unless they ask for method comparison.

## Reference map

- `references/method-selection.md`: survey-based guide from research problem to Infomap model.
- `references/cli.md`: file-based CLI workflows and native outputs.
- `references/python.md`: Python package workflows and integrations.
- `references/r.md`: R package workflows and igraph interop.
- `references/notebooks.md`: survey companion notebooks and Jupyter adaptation.
- `references/reproducibility.md`: reporting, provenance, and method-section checklist.
- `references/faq.md`: common troubleshooting patterns distilled from Infomap Discussions.
