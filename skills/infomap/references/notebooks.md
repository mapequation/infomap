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

Published notebook image:

```bash
docker run --rm -p 8888:8888 \
  ghcr.io/mapequation/infomap:notebook \
  start.sh jupyter lab
```

From a source checkout, only when the user has the repository locally:

```bash
python -m pip install -e '.[notebooks]'
cd examples/notebooks
jupyter lab
```

Do not start a dev server or Jupyter session unless the user asks.
