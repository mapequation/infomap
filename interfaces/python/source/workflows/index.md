# Workflows & integrations

How Infomap fits into the wider ecosystem and how to run it on real workloads.
For *which* method to choose in the first place, see
{doc}`/concepts/choosing-a-method`.

- {doc}`Scanpy and AnnData <scanpy>`: cluster single-cell neighbour graphs with
  `infomap.tl.infomap`, the Leiden-style entry point for the scanpy workflow.
- {doc}`GraphRAG tables <graphrag>`: run Infomap on the entity and relationship
  tables a GraphRAG pipeline produces, and get community tables back.
- {doc}`Running at scale (HPC) <hpc>`: scheduler-aware threading, trial sharding
  across cluster jobs, and merging the results.

```{toctree}
:hidden:
:maxdepth: 1

scanpy
graphrag
hpc
```
