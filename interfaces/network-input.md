# Network input contract

This document describes the shared network input behavior that the Python, R,
and JavaScript bindings should preserve. The C++ `InfomapWrapper` stays a thin
runtime-agnostic layer over the core network methods; each binding adapts its
host language data structures to the same shapes before calling C++ or writing
an Infomap network text format.

## Common rules

- Node, layer, state, source, and target ids are numeric values converted to
  unsigned integer ids by the native layer.
- Link weights are numeric values. When a weight is omitted, bindings use
  `1.0`.
- Links can be added before their endpoint nodes exist; the native network
  creates missing nodes as needed.
- Duplicate links and self-links are accepted by the binding adapters. Native
  parsing and configuration decide aggregation, weight-threshold filtering, and
  whether self-links are included in a run.
- Binding adapters should reject malformed container shapes before native calls
  when practical, with errors that name the invalid input family.

## First-order links

Python accepts iterables of `(source_id, target_id[, weight])` and NumPy arrays
with 2 or 3 numeric columns. First-order NumPy arrays must use 32-bit or 64-bit
numeric values.

R accepts lists of `c(source, target[, weight])` and 2- or 3-column matrices or
data frames whose source, target, and weight columns are numeric.

JavaScript accepts objects with `links: [{ source, target, weight? }]`, optional
`nodes`, and optional `bipartiteStartId`. `stringifyNetwork()` validates that
serialized ids and weights are finite numbers.

## State nodes

Python and R accept state nodes as `(state_id, node_id)` pairs or mappings from
state id to physical node id. JavaScript state networks use
`states: [{ stateId, id, name? }]` plus required `nodes` and `links` arrays.

## Multilayer links

Python accepts full multilayer links as
`((source_layer, source_node), (target_layer, target_node)[, weight])` or NumPy
arrays with 4 or 5 numeric columns:
`source_layer, source_node, target_layer, target_node[, weight]`.

R accepts the same paired list shape and 4- or 5-column matrices/data frames.

JavaScript full multilayer networks use
`links: [{ sourceLayer, source, targetLayer, target, weight? }]`.

## Intra- and inter-layer links

Intra-layer links use `layer, source_node, target_node[, weight]`.
Inter-layer links use `source_layer, node, target_layer[, weight]`. Python
accepts iterable rows and NumPy arrays with 3 or 4 numeric columns. R accepts
list rows and 3- or 4-column matrices/data frames. JavaScript uses
`intra: [{ layerId, source, target, weight? }]` and optional
`inter: [{ sourceLayer, id, targetLayer, weight? }]`.
