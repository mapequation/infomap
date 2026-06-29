# Concepts

These chapters teach the ideas behind Infomap from the ground up, in reading
order. Start here if you want to understand *why* Infomap finds the communities
it does before running it on your own data.

- {doc}`Flow and random walks <flow-and-random-walks>`: what "flow" means and why
  Infomap models a random walker; the foundation everything else rests on.
- {doc}`The map equation <the-map-equation>`: the objective Infomap minimises,
  codelength, the bits needed to describe that walk, and the partition that makes
  it shortest.
- {doc}`Hierarchy and the multilevel map equation <hierarchy-and-the-multilevel-map>`:
  how Infomap discovers nested structure to any depth, with no resolution
  parameter to tune.
- {doc}`State nodes and higher-order flow <state-nodes-and-higher-order-flow>`: the
  one mechanism behind the memory, multilayer, and temporal models; read it before
  the Flow models section.
- {doc}`Choosing a method <choosing-a-method>`: Infomap versus Louvain and Leiden,
  and when the flow-based and modularity-based views diverge.

```{toctree}
:hidden:
:maxdepth: 1

flow-and-random-walks
the-map-equation
hierarchy-and-the-multilevel-map
state-nodes-and-higher-order-flow
choosing-a-method
```
