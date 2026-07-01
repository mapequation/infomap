---
jupytext:
  text_representation:
    format_name: myst
kernelspec:
  display_name: Python 3
  language: python
  name: python3
# Chapter authoring template: COPY this file, do not edit it in place.
# Fill every section. Keep prose digestible: intuition and the runnable
# example come BEFORE heavy math. Put derivations behind togglebuttons.
---

# {Chapter title}

```{admonition} In one sentence
:class: tip
{One-sentence "why you care".}
```

## Motivation

{2–4 short paragraphs: the real problem this representation/feature solves.}

## Intuition

{Plain-language mental model, before any equation.}

## Theory

{The minimum needed to use it well, grounded in the cited paper(s).}

:::{toggle}
**Full derivation**

{Heavy math here, hidden by default so the page breathes.}
:::

## A worked example

```{code-cell} python
# Self-contained, runs at build time. Uses its own small dataset: # NOT a copy of the matching survey notebook.
import infomap
# ...
```

```{code-cell} python
from docs_viz import draw_partition
fig = draw_partition(graph, modules)
fig  # rendered inline
```

## API pointers

- {:py:func:`...` / :py:class:`...` links into the API reference.}

## Going deeper

- Survey article, §{N}: <https://doi.org/10.1145/3779648>
- Companion notebook: {`examples/notebooks/<name>.ipynb` link}
- Source paper(s): {citation}
