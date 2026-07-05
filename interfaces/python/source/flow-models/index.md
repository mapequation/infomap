# Flow models & representations

These chapters apply the same map equation to richer ways of turning your data
into flow — flow that carries memory, spans layers, tracks time, follows node
attributes, or crosses two node types. The first three build on **state nodes**;
if you have not met them yet, read
{doc}`/concepts/state-nodes-and-higher-order-flow` first.

- {doc}`Memory and state networks <memory-and-state>`: flow that depends on where
  the walker came from.
- {doc}`Multilayer networks <multilayer>`: a node living in several layers
  (relationship types) at once; the home of the inter-layer relax rate.
- {doc}`Temporal networks <temporal>`: multilayer networks whose layers are time
  windows.
- {doc}`Networks with metadata <metadata>`: folding node attributes into the
  objective.
- {doc}`Bipartite networks <bipartite>`: two node types, and how declaring the
  boundary changes the flow model.

```{toctree}
:hidden:
:maxdepth: 1

memory-and-state
multilayer
temporal
metadata
bipartite
```
