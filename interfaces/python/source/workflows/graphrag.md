---
jupytext:
  text_representation:
    format_name: myst
kernelspec:
  display_name: Python 3
  language: python
  name: python3
---

# GraphRAG tables

```{admonition} In one sentence
:class: tip
The `infomap.graphrag` adapter reads the entity and relationship Parquet tables
produced by a GraphRAG pipeline, runs Infomap on the resulting weighted graph,
and returns GraphRAG-style community tables ready to drop into a retrieval
pipeline.
```

## Motivation

GraphRAG pipelines turn a document corpus into a knowledge graph: an *entities*
table of named concepts and a *relationships* table of weighted co-occurrences.
The communities detected over this graph become the unit of retrieval: an LLM
summarises each community, and those summaries are indexed for
question-answering.

The default community detector in most GraphRAG implementations is Leiden, run
with a modularity objective. Infomap takes a different approach: it finds the partition
that minimises the code length of a random walk over the weighted graph (see
{doc}`/concepts/the-map-equation`). On knowledge graphs where edge weights represent
co-occurrence frequency, flow is a natural proxy for semantic proximity, and
Infomap's compression objective rewards tight topical clusters. The adapter in
`infomap.graphrag` handles the column mapping, node-id translation, hierarchy
extraction, and Parquet output so you do not have to.

## Intuition

Think of the entity graph as a map of ideas. A random reader picking up a
document about "Alpha" will next likely encounter "Beta" and "Gamma" before
jumping to the "Delta" side of the graph. Infomap encodes that tendency: it
groups entities that a conceptual random walk visits together into the same
community. Communities that trap the walk well compress into short codewords;
entities that bridge communities carry higher description cost.

The adapter translates this result into GraphRAG's own table schema so the
downstream summarisation and retrieval steps need no changes.

## Theory

Infomap minimises the map equation

$$
L(\mathsf{M}) =
  q_{\curvearrowright} H(\mathcal{Q})
  + \sum_{i=1}^{m} p_{\circlearrowright}^i H(\mathcal{P}^i)
$$

over all partitions $\mathsf{M}$ of the entity graph. The first term is the cost
of naming module crossings; the second is the cost of naming nodes within
modules. Infomap uses edge weights directly as flow volumes, so a heavy
co-occurrence link between two entities makes them harder to separate. See
{doc}`/concepts/the-map-equation` for the full derivation.

For the typical undirected, weighted knowledge graph produced by GraphRAG, no
directed-flow or teleportation assumptions are needed. Infomap operates in
undirected mode by default when all edges are symmetric, which is the standard
output of a co-occurrence extraction step.

## A worked example

### Build a synthetic GraphRAG dataset

The `infomap.graphrag` adapter expects two Parquet tables (or two DataFrames)
with specific column names:

| Table | Required columns | Notes |
|---|---|---|
| `entities` | `id`, `title` | `id` must be unique; `title` used as endpoint key by default |
| `relationships` | `source`, `target`, `weight` | `source`/`target` match entity `title` values; `id` column for relationship ids |

Here you build a minimal example: two tight triangles (Alpha–Beta–Gamma and
Delta–Epsilon–Zeta) linked by one weak bridge edge. The two communities are
obvious by design, so you can verify the result at a glance.

```{code-cell} python
import pandas as pd

entities = pd.DataFrame({
    "id":    ["a",     "b",    "c",     "d",     "e",       "f"],
    "title": ["Alpha", "Beta", "Gamma", "Delta", "Epsilon", "Zeta"],
})

relationships = pd.DataFrame({
    "id":     ["ab",   "bc",    "ca",    "de",    "ef",      "fd",    "cd"],
    "source": ["Alpha","Beta",  "Gamma", "Delta", "Epsilon", "Zeta",  "Gamma"],
    "target": ["Beta", "Gamma", "Alpha", "Epsilon","Zeta",   "Delta", "Delta"],
    "weight": [2.0,    2.0,     2.0,     3.0,     3.0,      3.0,     1.0],
})

print(entities.to_string(index=False))
print()
print(relationships.to_string(index=False))
```

Write the tables to a temporary directory so `run_graphrag_communities` can read
them from disk, which mirrors real pipeline usage.

```{code-cell} python
import tempfile
from pathlib import Path

work_dir = Path(tempfile.mkdtemp(prefix="infomap-graphrag-"))
input_dir = work_dir / "input"
input_dir.mkdir()

entities.to_parquet(input_dir / "entities.parquet")
relationships.to_parquet(input_dir / "relationships.parquet")
```

### Inspect the node-id mapping

`read_graphrag` translates entity titles to the integer node ids that Infomap
expects. You can call it directly to preview the mapping before running the full
pipeline, which helps when debugging column-name mismatches.

```{code-cell} python
from infomap.graphrag import read_graphrag

graph = read_graphrag(entities, relationships)

relationships.assign(
    source_node=graph.sources,
    target_node=graph.targets,
)[["source", "target", "source_node", "target_node", "weight"]]
```

### Run Infomap

`run_graphrag_communities` reads the Parquet files from `input_dir`, builds the
weighted graph, runs Infomap, and returns a `GraphRAGRunResult` with two tables:
`nodes` (one row per entity) and `communities` (one row per detected community at
each hierarchy level).

```{code-cell} python
from infomap.graphrag import run_graphrag_communities

result = run_graphrag_communities(
    input_dir=input_dir,
    silent=True,
    seed=123,
    num_trials=5,
)

print(f"Map equation codelength: {result.infomap.codelength:.4f} bits/step")
print(f"Top-level communities:   {result.infomap.num_top_modules}")
```

### Per-entity community assignments

The `nodes` table maps every entity to its module. The `module_path` column
encodes the full position in the hierarchy (a list of module ids from the root
to the leaf), and `flow` is the stationary probability of the random walk
visiting that entity, a natural measure of entity centrality within its
community.

```{code-cell} python
result.nodes[["entity_title", "module_id", "module_path", "level", "flow"]]
```

### GraphRAG-style community table

The `communities` table matches GraphRAG's expected schema. Each row is one
community: a unique `id`, the `level` in the hierarchy (0 = top), `size`, the
list of `entity_ids` the community contains, and the list of `relationship_ids`
whose both endpoints fall within the community.

```{code-cell} python
result.communities[[
    "id", "level", "title", "size",
    "entity_ids", "relationship_ids", "parent", "children",
]]
```

Infomap separates the two triangles into community `infomap-1`
(Alpha/Beta/Gamma) and `infomap-2` (Delta/Epsilon/Zeta). The weak bridge edge
between Gamma and Delta carries too little flow to merge the groups.

### Visualise the partition

Build a NetworkX graph from the relationship table and pass it to
`draw_partition` with the module assignments keyed by entity title.

```{code-cell} python
import matplotlib.pyplot as plt
from myst_nb import glue

import networkx as nx
from docs_viz import draw_partition

g = nx.Graph()
for _, row in relationships.iterrows():
    g.add_edge(row["source"], row["target"], weight=row["weight"])

title_to_module = dict(
    zip(result.nodes["entity_title"], result.nodes["module_id"])
)
title_to_flow = dict(
    zip(result.nodes["entity_title"], result.nodes["flow"])
)

fig = draw_partition(g, title_to_module, flow=title_to_flow)
glue("fig-graphrag", fig, display=False)
plt.close(fig)
```

```{glue:figure} fig-graphrag
The GraphRAG entity graph coloured by community. Infomap separates the two
tightly linked entity groups; the thin bridge between them carries too
little flow to merge them.
```

The two colour groups match the triangle communities. The thin bridge edge
between Gamma and Delta connects the two clusters but does not pull them
together, because the flow through it is weak next to the internal triangle
edges.

### Write output to disk

If you need the Parquet tables on disk (for downstream summarisation or
ingestion into a vector store), pass `output_dir` to `run_graphrag_communities`.
The function writes `infomap_nodes.parquet`, `communities.parquet`, and an
`infomap_run.json` summary to the output directory.

```{code-cell} python
output_dir = work_dir / "output"

result_with_output = run_graphrag_communities(
    input_dir=input_dir,
    output_dir=output_dir,
    silent=True,
    seed=123,
    num_trials=5,
)

list(output_dir.iterdir())
```

You can also write the tables separately using `write_graphrag_communities` if
you have already run Infomap and just want to export:

```{code-cell} python
from infomap.graphrag import write_graphrag_communities

nodes, communities = write_graphrag_communities(
    result_with_output.infomap,
    graph=result_with_output.graph,
    output=output_dir / "communities_alt.parquet",
)
communities[["id", "level", "size", "entity_ids"]]
```

## API pointers

- {func}`infomap.graphrag.read_graphrag` translates entity/relationship
  DataFrames or Parquet paths into a `GraphRAGGraph` with mapped node ids. Key
  parameters: `entity_id_col`, `entity_title_col`, `source_col`, `target_col`,
  `weight_col`, `endpoint_col` (`"title"` or `"id"`).
- {func}`infomap.graphrag.run_graphrag_communities` is the one-call pipeline: it
  reads tables, runs Infomap, and optionally writes outputs. It returns a
  `GraphRAGRunResult` with `.infomap`, `.graph`, `.nodes`, and `.communities`.
- {func}`infomap.graphrag.write_graphrag_communities` writes a pre-run Infomap
  result to disk as GraphRAG-compatible Parquet tables.
- {class}`infomap.graphrag.GraphRAGGraph` holds the entity/relationship tables,
  the edge arrays, and the bidirectional entity-id ↔ node-id mappings.
- {class}`infomap.graphrag.GraphRAGRunResult` bundles the `Infomap` object, the
  `GraphRAGGraph`, and the two output DataFrames.
- {meth}`infomap.Infomap.codelength` and {attr}`infomap.Infomap.num_top_modules`
  report the quality and structure of the solution after a run.

## Going deeper

- Companion notebook: `examples/notebooks/run-infomap-on-graphrag-tables.ipynb`
  extends this example with a Leiden comparison and a retrieval-pipeline walkthrough.
- {doc}`/working-with-infomap/inputs` covers all the ways to feed data into Infomap.
- {doc}`/concepts/the-map-equation` is the objective `run_graphrag_communities` minimises.
- {cite:t}`edge2024graphrag` introduced the GraphRAG pipeline and its
  community-hierarchy approach.
