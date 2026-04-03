# InfoNode/CSR Refactor Plan

## Summary

Refactor the internal `InfoNode`-centric model by **keeping the hierarchy/tree pointer-backed** and introducing a separate abstraction only for the **active optimization graph**. The goal is still lower memory usage and better performance, but the first CSR-backed milestone should target the hot leaf-level optimization path, not the entire tree and not full backend parity across every feature combination on day one.

This change should proceed in stages:

1. lock down current behavior with stronger conformance tests and measurement
2. remove known coupling/precondition bugs that block a clean abstraction
3. split active-graph payload/state from `InfoNode`
4. introduce CSR adjacency for the leaf-level active graph
5. retain pointer-backed fallback for features that are not yet migrated

This plan intentionally narrows the abstraction boundary compared to the previous draft:

- **do not** abstract the tree/output layer up front
- **do** abstract the active graph used by `InfomapOptimizer` and the objective functions
- **do not** assume module-level or recursive graphs should immediately move to CSR

## Architecture And Implementation Changes

### 1. Keep the tree pointer-backed

The current tree is not just a presentation structure. It is mutated during:

- consolidation
- hard partition restore
- fine/coarse tuning
- super-module discovery
- recursive sub-Infomap construction
- final output and iterator traversal

The refactor should therefore keep `InfoNode` and the parent/child/sibling linked structure as the hierarchy representation for now.

Consequences:

- output code and iterators stay pointer-based initially
- `owner`, `setInfomap()`, `disposeInfomap()`, `physicalNodes`, `stateNodes`, and `metaCollection` remain on the hierarchy nodes
- tree rewrite operations such as `collapseChildren`, `expandChildren`, `replaceWithChildren`, and `replaceChildrenWithGrandChildren` remain unchanged until after the active-graph refactor proves out

### 2. Abstract only the active optimization graph

Add a new internal active-graph layer used by the optimizer and objectives. This layer must be compile-time based, not runtime-polymorphic, to avoid virtual dispatch in the hot loop.

Internal concepts to introduce:

- `ActiveNodeId`: stable dense integer id for the current active graph
- `ActiveNodePayload`: read-mostly per-node values such as flow data, state/physical/layer ids, and any metadata needed by the current objective
- `ActiveNodeState`: read-write optimization state such as module assignment and dirty flags
- `ActiveAdjacency`: backend-specific adjacency access
- `ActiveGraphContext<Backend>`: the active graph object handed to the optimizer/objective layer

Backend policy:

- the optimizer/objective code is templated on the backend or on backend-provided accessors
- no virtual dispatch in edge traversal or node-state access
- the first backend is a pointer-backed compatibility backend over existing `InfoNode`/`InfoEdge`
- the second backend is CSR

This active-graph layer is the only new abstraction boundary that should be introduced in the first implementation.

### 3. Treat consolidation as an explicit backend boundary

`consolidateModules(...)` is both a tree mutation and a graph rebuild. The plan must treat it as the bridge point between hierarchy and active graph.

Rules:

- consolidation continues to create pointer-backed module `InfoNode`s and attach children exactly as today
- after consolidation, the next active graph is materialized from the new top-level hierarchy state
- backend choice is made at active-graph materialization time, not inside the hierarchy rewrite itself

Default behavior for the first CSR rollout:

- leaf-level active graphs may use CSR
- module-level graphs created after consolidation remain pointer-backed unless measurement proves CSR is worth the rebuild cost

### 4. Make the prerequisite cleanup explicit

Before any storage refactor starts, fix current coupling that would invalidate the abstraction work:

- `generateSubNetwork(InfoNode&)` must stop clobbering `node.index` as a side-channel for mapping cloned edges
- replace that temporary mutation with a local pointer-to-index map or equivalent explicit lookup
- document `node.index` and `node.dirty` semantics as optimization-state fields, not general scratch storage
- document OpenMP move-loop assumptions: adjacency is read-only during a move round; membership/state arrays are the only mutated hot-path storage

These are prerequisites, not follow-up cleanup.

## Implementation Phases

### Phase 0: Baseline, prerequisites, and parity net

#### 0a. Measure current baseline

Add a native benchmark/memory harness before changing storage.

Metrics:

- total wall time for `readInputData + initNetwork + run`
- partition-only wall time
- per-phase time where possible:
  - initial leaf optimization
  - consolidation/materialization
  - module-level optimization
  - recursive/sub-Infomap work
- peak RSS / peak resident memory
- internal storage accounting:
  - bytes in hierarchy nodes
  - bytes in edge storage
  - bytes in active payload/state storage
  - bytes per node
  - bytes per edge

Benchmark corpus:

- repo fixtures:
  - `examples/networks/twotriangles.net`
  - `examples/networks/ninetriangles.net`
  - `examples/networks/modular_w.net`
  - `examples/networks/modular_wd.net`
  - `examples/networks/states.net`
  - `examples/networks/multilayer.net`
  - `examples/networks/bipartite.net`
- generated graphs:
  - sparse first-order graphs at roughly `10k`, `100k`, and `1M` nodes with average degree around `10`
  - clustered ring-of-cliques or planted-partition graphs at the same scales
  - one deterministic larger higher-order/state-network workload

Also record:

- measured `sizeof(InfoNode)` and `sizeof(InfoEdge)` in the benchmark/diagnostic harness
- edge-traversal throughput and, where feasible, cache-miss counters on Linux

#### 0b. Fix prerequisites

Required code cleanup before storage changes:

- replace the `generateSubNetwork(...)` `node.index = childIndex` side-channel with explicit mapping
- add regression coverage for sub-network edge cloning correctness
- codify module-assignment semantics so later SoA arrays preserve current read/write ordering

#### 0c. Expand the conformance suite

Add missing tests before introducing CSR:

1. hierarchy mutation invariants
   - `replaceChildrenWithGrandChildren`
   - `replaceWithChildren`
   - `collapseChildren` / `expandChildren`
   - `releaseChildren`
   - repeated remove/rebuild cycles
2. output parity
   - `.tree`
   - `.clu`
   - JSON
   - Pajek / state-network output
   - state vs physical output for memory networks
3. recursive hierarchy behavior
   - fine tune
   - coarse tune
   - super-module search
   - repeated rerun after hierarchy reconstruction
4. higher-order parity
   - `states.net`
   - `multilayer.net`
   - `matchableMultilayerIds`
   - `iterTreePhysical`
   - `iterLeafNodesPhysical`
   - `getModules(states=true/false)`
5. hard partition behavior
   - `.clu` hard/soft
   - `.tree` import
   - restore after optimization
6. metadata behavior
   - minimal `MetaMapEquation` fixture
   - stable module-level metadata aggregation after moves/consolidation
7. OpenMP parity
   - sequential and parallel move loops produce equivalent results for fixed seeds / deterministic fixtures

Phase 0 exit criteria:

- all current tests remain green
- the new conformance suite is green
- baseline benchmark JSON exists and is committed or archived for comparison

### Phase 1: Split active payload/state from `InfoNode`

Introduce a dense active-graph data model without changing adjacency yet.

Add:

- read-mostly arrays for flow/payload data
- read-write arrays for optimization state:
  - module assignment
  - dirty flags
- explicit mapping between active-node id and hierarchy `InfoNode*`

Important design rule:

- separate read-mostly payload from read-write optimization state
- do not treat them as one generic “node payload” blob

At this phase:

- edge traversal still uses current `InfoEdge*` adjacency
- consolidation still builds pointer-backed module nodes
- module-level active graphs remain pointer-backed
- higher-order/meta/hard-partition all stay on the current pointer backend

Purpose:

- prove the active-graph abstraction
- de-risk backend templating
- isolate optimizer/objective code from direct `InfoNode` field access

Phase 1 exit criteria:

- correctness parity with the pointer baseline
- measurable storage/accounting visibility for active payload/state
- no unacceptable regression in total runtime on the benchmark corpus

### Phase 2: Add CSR adjacency for leaf-level active graphs

Implement a CSR backend for the initial leaf-level active graph only.

Storage shape:

- out-edge CSR:
  - offsets
  - targets
  - edge weights/flows
- in-edge support:
  - either transposed CSR
  - or an explicit incoming index structure

Backend behavior:

- the leaf-level active graph can be materialized as CSR
- the optimizer/objective layer uses template-based backend access
- the move loop mutates only active-node state arrays
- CSR adjacency remains read-only during a move round

OpenMP design requirement:

- document exactly which arrays are shared read-only
- document how module-assignment state is updated in the parallel path
- preserve current critical-section/validation semantics

Deliberate scope limit:

- after `consolidateModules`, the next module-level active graph stays pointer-backed by default
- recursive sub-Infomap and special feature paths also stay pointer-backed unless explicitly migrated later

Phase 2 exit criteria:

- pointer and CSR backends produce identical partitions/codelengths on first-order fixtures
- benchmark shows real benefit on large first-order sparse graphs
- no correctness regressions in the conformance suite

### Phase 3: Extend CSR to selected nontrivial active-graph cases

Only after Phase 2 is proven useful, evaluate and selectively extend CSR to:

- higher-order/state-network active graphs
- recursive sub-Infomap leaf-level active graphs
- possibly some module-level active graphs, but only if measured rebuild cost is justified

Do not assume all of these are worth migrating.

Explicit policy:

- if a feature path has poor ROI or high complexity, keep it on the pointer backend behind the active-graph abstraction
- supported fallback is acceptable as long as user-visible behavior does not change

Features most likely to remain pointer-backed longest:

- hard partition restore
- metadata objective
- physical aggregation on hierarchy nodes
- super/coarse tune paths that constantly rebuild hierarchy state

Phase 3 exit criteria:

- selected extensions are benchmarked and justified individually
- fallback matrix is documented explicitly

### Phase 4: Re-evaluate broader migration or permanent hybrid design

After measurements and parity results, decide whether to:

- keep a permanent hybrid architecture:
  - pointer-backed hierarchy
  - CSR for selected active graphs
- or expand CSR further where it pays off

This phase is decision-oriented, not assumed.

## Test Plan And Acceptance Criteria

### Backend parity requirements

For any path migrated to CSR:

- same top-module partition
- same number of levels
- same `codelength` / `index codelength` within tolerance
- same normalized output artifacts where output is part of the contract
- same failure behavior on malformed input

### Required backend comparison tests

Add direct pointer-vs-CSR A/B tests in the C++ suite for:

- first-order leaf-level optimization
- repeated multi-trial reruns
- sub-network generation where applicable
- OpenMP vs sequential parity for migrated backends

### Sanitizer and stress requirements

For every migrated path:

- normal C++ suite green
- sanitizer suite green
- at least one repeated stress/rerun case under sanitizers

## Performance And Memory Goals

### What success looks like

Do not judge success by a single wall-time number.

Track:

- total runtime
- partition-only runtime
- CSR materialization/rebuild time
- peak RSS
- bytes per node
- bytes per edge
- edge-traversal throughput

Recommended interpretation:

- first-order large sparse graphs should show a clear memory win and a measurable partition-phase speedup
- if CSR improves leaf-level partition time but total runtime barely moves, report that honestly rather than calling it a universal speedup
- module-level CSR is only justified if rebuild cost is amortized by subsequent optimization rounds

### Measurement gates per phase

- Phase 1 must not meaningfully regress large first-order graphs while introducing payload/state arrays
- Phase 2 must show a measurable benefit on large first-order sparse graphs before CSR expands further
- Phase 3 extensions must justify themselves case-by-case

## Current Blockers And High-Risk Features

### Explicit blockers for the first CSR milestone

- hard partition restore:
  - `clusterDataIsHard`
  - `restoreHardPartition`
  - collapsed/expanded children rewrites
- recursive hierarchy rebuilding:
  - `coarseTune`
  - `fineTune`
  - super-module search
  - `owner` / `setInfomap()` / `disposeInfomap()`
- metadata aggregation:
  - `metaCollection`
  - `MetaMapEquation`
- higher-order physical aggregation:
  - `physicalNodes`
  - `stateNodes`
  - physical iterators and outputs

These should remain pointer-backed until explicitly migrated.

### Correctness-sensitive but not first-class blockers

- `recordedTeleportation`
- `regularized`
- `variableMarkovTime*`
- precomputed flow input
- `matchableMultilayerIds`
- bipartite handling

These must be parity-tested for any migrated backend.

## Public/Internal Interface Changes

No intended public CLI, Python, or JS behavior changes.

Internal changes that should exist after the first successful milestone:

- active-node ids and explicit active-graph materialization
- dense payload/state arrays for the active graph
- template-based backend selection for optimizer/objective code
- CSR adjacency for the leaf-level active graph
- explicit pointer-backed fallback for non-migrated feature paths

## Assumptions And Defaults

- This remains a high-risk internal refactor and should be executed behind tight parity and measurement gates.
- The pointer-backed tree is not the primary performance problem; the hot active graph is.
- A permanent hybrid design is acceptable if it gives the desired speed/memory improvements with lower risk.
- Full user-visible feature support remains the goal, but full CSR coverage across all internal paths is **not** required for the first successful milestone.
