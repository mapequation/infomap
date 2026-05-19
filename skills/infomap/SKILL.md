---
name: infomap
description: Use when helping researchers use Infomap, the map equation, or the Infomap CLI, Python package, or R package for community detection workflows, including reproducible analysis, method selection, notebooks, and multilayer, memory/state, metadata, bipartite, or flow-model choices.
---

# Infomap Research Workflows

Use this skill to help researchers run, explain, adapt, or troubleshoot Infomap analyses with the CLI, Python package, R package, and survey companion notebooks.

## First classify the task

Identify the user's mode before answering:

- **Analysis planning**: choose a network representation, flow model, interface, outputs, and reproducibility checklist. Read `references/method-selection.md` and usually `references/reproducibility.md`.
- **Code or command generation**: choose the smallest practical interface, then read `references/cli.md`, `references/python.md`, or `references/r.md`.
- **Notebook workflow**: read `references/notebooks.md`; also read `references/python.md` when converting notebook ideas into scripts.
- **Result interpretation or method text**: read `references/reproducibility.md`; read interface-specific references only if output fields or APIs matter.
- **Usage troubleshooting**: inspect current repo docs/source or local help first, then read the relevant interface reference.

## Choose the interface

- Prefer **Python** for notebooks, scripts, NetworkX, python-igraph, SciPy sparse matrices, edge-index data, AnnData/Scanpy workflows, tabular outputs, and GraphML/GEXF export.
- Prefer **R** for R-native analysis, R igraph workflows, R Markdown-style reports, and users who want `cluster_infomap()` or the R6 `Infomap` API.
- Prefer **CLI** for file-based workflows, batch jobs, shell pipelines, native output files, or wrapper-independent runs.

When the user already chose an interface, stay there unless a different interface is clearly necessary.

## Source rules

- Treat repo docs and source as the authority for current syntax. For CLI details, inspect `./Infomap --help`, `./Infomap -hh`, or `./Infomap --print-json-parameters` when available.
- For Python details, inspect `interfaces/python/source/`, `examples/python/`, or package API docs in the repo before giving version-sensitive advice.
- For R details, inspect `interfaces/R/infomap/README.md`, `interfaces/R/infomap/R/`, and `examples/R/`.
- Use the survey article as a decision guide for representation, flow modeling, higher-order networks, metadata, bipartite networks, incomplete data, and applications. Do not quote long passages from it.

## Default research standards

- Make examples reproducible by default: include `seed`, use meaningful `num_trials`, record Infomap version, input provenance, non-default options, and output artifacts.
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
