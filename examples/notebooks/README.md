# Infomap tutorial notebooks

Start with [`quickstart.ipynb`](quickstart.ipynb). It is the flagship Python
notebook for running Infomap in Jupyter, inspecting the notebook-native result
summary, drawing a small readable partition, and exporting results for further
analysis.

Then read [`options-guide.ipynb`](options-guide.ipynb) for what every Infomap
option is for, with runnable examples and a complete reference table.

Most numbered notebooks accompany [Community Detection with the Map Equation
and Infomap: Theory and Applications](https://doi.org/10.1145/3779648).
Descriptive, unnumbered notebooks cover practical Python workflows that are
not tied to the article section numbering.

They live in the main Infomap repository so API changes, dependency drift, and
broken examples are caught by the normal maintenance workflow.

## Maintenance tiers

The tier for each notebook is listed in `notebooks.toml`.

- `smoke`: deterministic notebooks that run in pull-request CI for notebook
  and Python API changes, and in full notebook CI.
- `full`: reproducible notebooks that run in the scheduled full notebook CI.
- `manual`: notebooks that currently require external repositories, compiled
  helper tools, large geospatial dependencies, or other setup outside CI.

## Running tests

From the repository root:

```bash
python -m pip install -e '.[notebooks]'
make test-python-notebooks-smoke
make test-python-notebooks-full
```

The tests use `nbmake`, so they check that notebooks execute successfully
without comparing plot output byte-for-byte.
