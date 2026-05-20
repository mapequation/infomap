# Notebook Workflows

Use this reference when the user mentions Jupyter, tutorial notebooks, survey companion notebooks, exploratory analysis, teaching material, or adapting notebook examples related to the survey article (`https://doi.org/10.1145/3779648`).

## Sources to inspect

- Published Python docs and notebook page.
- The installed or published notebook image `ghcr.io/mapequation/infomap:notebook`.
- The public notebook source on GitHub when internet access is available.
- Source checkout files only when the user is actually working inside an Infomap repository.

## When notebooks are the right path

Use notebooks when the user wants to:

- connect the map equation concepts to executable examples;
- inspect small networks, partitions, flows, and codelengths step by step;
- adapt survey examples for a paper, teaching material, or exploratory analysis;
- compare Infomap with Louvain, Leiden, Scanpy, or other workflow tools.

Use scripts instead when the user wants batch runs, CI, parameter sweeps, or stable pipeline execution.

Notebook runtimes can grow quickly when examples download data, build layouts, compare methods, or run many trials. Do not start Docker, Jupyter, full notebook execution, parameter sweeps, or long comparison notebooks unless the user asks. For validation, prefer running one small extracted cell or a reduced toy example first.

## Notebook topics

The survey companion notebooks cover:

- two-level map equation and two-level search;
- solution landscapes;
- memory, multilayer, temporal, and multi-body network models;
- metadata, bipartite, and incomplete-data networks;
- map equation centrality, similarity, bioregions, and model selection with correlational data.

Comparison notebooks cover NetworkX, python-igraph, and Scanpy-style workflows.

## Adaptation workflow

When helping adapt a notebook:

1. Identify the notebook section that matches the research data or question.
2. Preserve the conceptual structure, but replace toy data with the user's data loading cell.
3. Add a reproducibility cell with package versions, input path or data checksum, seed, `num_trials`, and non-default options.
4. Store outputs outside the notebook when they are needed later: CSV tables, GraphML/GEXF, `.tree`, `.clu`, or figures.
5. Convert repeated analysis into functions or scripts once the exploratory workflow stabilizes.

## Running notebooks

Do not start Docker, Jupyter, or full notebook execution unless the user asks. Before giving launch commands, verify the current notebook image, installation extras, or source-checkout path from the published docs, package metadata, or the user's local repository. For validation, prefer extracting one small cell into a temporary script or reduced notebook and record the package version, inputs, seed, options, and outputs.
