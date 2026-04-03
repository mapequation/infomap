# InfoNode/CSR Refactor Plan

## Summary

Refactor the internal `InfoNode`-centric model by **keeping the hierarchy/tree pointer-backed** and introducing a separate abstraction only for the **active optimization graph**. The goal is still lower memory usage and better performance, but the first CSR-backed milestone should target the hot leaf-level optimization path, not the entire tree and not full backend parity across every feature combination on day one.

This change should proceed in stages:

1. lock down current behavior with stronger conformance tests and measurement
2. remove known coupling/precondition bugs that block a clean abstraction
3. split active-graph flow payload from `InfoNode`
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
- `ActiveNodeState`: optimization state for CSR-backed paths such as module assignment and dirty flags
- `ActiveAdjacency`: backend-specific adjacency access
- `ActiveGraphContext<Backend>`: the active graph object handed to the optimizer/objective layer

Backend policy:

- the optimizer/objective code is templated on the backend or on backend-provided accessors
- no virtual dispatch in edge traversal or node-state access
- the first backend is a pointer-backed compatibility backend over existing `InfoNode`/`InfoEdge`
- the second backend is CSR

This active-graph layer is the only new abstraction boundary that should be introduced in the first implementation.

### 2b. Define the active-graph lifecycle explicitly

Every migrated path must follow the same lifecycle:

1. **materialize**
   - build the active graph from the current hierarchy state
   - establish dense active-node ids and any payload arrays
2. **optimize**
   - run the move loop and objective updates against the active graph backend
   - treat backend-owned optimization state as authoritative during this phase
3. **sync back**
   - write any state needed by downstream tree logic back to hierarchy `InfoNode`s
   - this must happen before consolidation, tuning, or output code reads tree state again

Fields that may require sync-back once they are migrated off `InfoNode`:

- module assignment
- dirty state if any downstream logic depends on it
- any payload values that the tree/objective/output code later expects to read directly from `InfoNode`

This lifecycle is a correctness invariant. No later phase should rely on implicit aliasing between active-graph storage and `InfoNode`.

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
- audit current `node.index` usages and document each role by lifecycle phase:
  - optimization-time module assignment
  - tree-time scratch/indexing uses
  - output- or traversal-related temporary uses
- document `node.dirty` semantics as optimization-state, not general scratch storage
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

#### 0c. Expand the conformance suite in two tiers

##### Tier 1: must-have before Phase 1

1. hierarchy mutation invariants
   - `replaceChildrenWithGrandChildren`
   - `replaceWithChildren`
   - `collapseChildren` / `expandChildren`
   - `releaseChildren`
   - repeated remove/rebuild cycles
2. first-order partition parity
   - current C++/Python regressions remain green
   - add direct regression for `generateSubNetwork(...)` edge cloning correctness
3. OpenMP parity
   - sequential and parallel move loops produce equivalent results for fixed seeds / deterministic fixtures
4. consolidation bridge correctness
   - module-level edge weights equal the sum of inter-module leaf-edge weights after `consolidateModules(...)`

##### Tier 2: must-have before Phase 2

1. output parity
   - `.tree`
   - `.clu`
   - JSON
   - Pajek / state-network output
   - state vs physical output for memory networks
2. recursive hierarchy behavior
   - fine tune
   - coarse tune
   - super-module search
   - repeated rerun after hierarchy reconstruction
3. higher-order parity
   - `states.net`
   - `multilayer.net`
   - `matchableMultilayerIds`
   - `iterTreePhysical`
   - `iterLeafNodesPhysical`
   - `getModules(states=true/false)`
4. hard partition behavior
   - `.clu` hard/soft
   - `.tree` import
   - restore after optimization
5. metadata behavior
   - minimal `MetaMapEquation` fixture
   - stable module-level metadata aggregation after moves/consolidation
6. sync-back and rollback invariants
   - post-optimization sync-back preserves downstream tree semantics
   - `restoreConsolidatedOptimizationPointIfNoImprovement(...)` remains safe with split active-graph state

Phase 0 exit criteria:

- all current tests remain green
- Tier 1 conformance tests are green
- baseline benchmark JSON exists and is committed or archived for comparison

### Phase 1: Introduce active-graph lifecycle and payload infrastructure

Introduce the active-graph lifecycle, active-node id mapping, and read-mostly payload infrastructure without changing adjacency or moving module assignment off `InfoNode` yet.

Add:

- read-mostly arrays for flow/payload data
- explicit mapping between active-node id and hierarchy `InfoNode*`

Important design rule:

- separate read-mostly payload from read-write optimization state
- Phase 1 only migrates the read-mostly payload part
- `InfoNode.index` and `InfoNode.dirty` remain authoritative in Phase 1

Objective/backend rule for Phase 1:

- do **not** attempt to template the existing objective virtual hierarchy on backend yet
- keep objective entry points compatible with the current `InfoNode&`-based design
- introduce lightweight payload access helpers/views as needed, but do not force a CRTP/policy rewrite of the objective hierarchy in Phase 1
- the explicit template-based backend dimension starts when adjacency moves in Phase 2

At this phase:

- edge traversal still uses current `InfoEdge*` adjacency
- consolidation still builds pointer-backed module nodes
- module-level active graphs remain pointer-backed
- higher-order/meta/hard-partition all stay on the current pointer backend

Purpose:

- prove the materialize -> optimize -> sync-back lifecycle
- establish active-node id <-> `InfoNode*` mapping
- build payload arrays as structural proof that active-graph materialization works
- de-risk later backend work without changing the hot-path consumer contract yet

Performance expectation:

- Phase 1 is primarily an architecture/de-risking step
- it is not expected to deliver major speedups on its own
- hot-path performance is expected to remain effectively unchanged
- acceptable outcome is parity with baseline within a small regression budget
- the optimizer and objective hot path remain unchanged in Phase 1; the new lifecycle/payload infrastructure exists alongside the current hot-path consumers without replacing them yet

Phase 1 exit criteria:

- correctness parity with the pointer baseline
- measurable storage/accounting visibility for active payload/state
- total runtime within roughly `+/-5%` of baseline on benchmark cases
- no new correctness regressions in Tier 1 conformance tests

### Phase 2: Add CSR adjacency for leaf-level active graphs

Implement a CSR backend for the initial leaf-level active graph only. This is also the phase where optimization state can move off `InfoNode` because neighbor positions become directly available from CSR targets.

Storage shape:

- out-edge CSR:
  - offsets
  - targets
  - edge weights/flows
- in-edge support:
  - either transposed CSR
  - or an explicit incoming index structure
- optimization-state arrays:
  - module assignment
  - dirty flags

Backend requirements:

- a `CSRBackend` for migrated leaf-level active graphs
- a `PointerBackend` compatibility adapter that wraps current `InfoNode*` / `InfoEdge*` access behind the same accessor concept
- the inner templated optimizer methods must work against both backends so pointer-backed module-level graphs and fallback paths keep using the same logic

Backend behavior:

- the leaf-level active graph can be materialized as CSR
- the optimizer hot path can now use template-based backend access for adjacency/state reads
- the move loop mutates only active-node state arrays
- CSR adjacency remains read-only during a move round
- for first-order and biased objectives, the objective interface can remain `InfoNode&`-based because the objective logic reads stable flow payload on `InfoNode`, while module-assignment and dirty-state changes stay inside the optimizer/backend layer until sync-back
- the `recordedTeleportation` optimizer branch is an explicit Phase 2 change site because it currently reads `current.index`
- `initPartition()` is also an explicit Phase 2 change site because it seeds module assignment and dirty state for the active graph
- `moveNodeToPredefinedModule()` and `moveActiveNodesToPredefinedModules()` are part of the Phase 2 optimizer/backend scope because fine-tuning seeds optimization through the same neighbor-module access pattern as the main move loop
- fine-tuning should be treated as split scope:
  - hierarchy/module bookkeeping stays pointer-backed
  - the leaf-level optimization portion is CSR-eligible when the active graph is leaf-level

OpenMP design requirement:

- document exactly which arrays are shared read-only
- document how module-assignment state is updated in the parallel path
- preserve current critical-section/validation semantics

Deliberate scope limit:

- after `consolidateModules`, the next module-level active graph stays pointer-backed by default
- recursive sub-Infomap and special feature paths also stay pointer-backed unless explicitly migrated later
- Phase 2 should start undirected-first if that materially reduces implementation size
- directed CSR support can follow within Phase 2 once transpose/in-edge support is validated

Phase 2 exit criteria:

- pointer and CSR backends produce identical partitions/codelengths on first-order fixtures
- benchmark shows at least a measurable win on large first-order sparse graphs
- first-order Tier 2 conformance tests are green on both pointer and CSR backends for migrated paths
- non-first-order Tier 2 tests remain green on the unchanged pointer-backed paths
- no more than roughly `3%` total-runtime regression on non-migrated or fallback paths

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
- active-graph sync-back invariants before downstream tree consumers run
- `recordedTeleportation` parity on migrated CSR paths
- `BiasedMapEquation` parity if it rides along with the first-order CSR rollout
- `moveActiveNodesToPredefinedModules()` parity on migrated CSR paths

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
- fine-tune time
- coarse-tune time
- consolidation count per trial
- module graph size distribution after each consolidation step
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
- Phase 1 should be judged as an architectural milestone, not a performance milestone
- Phase 2 must show at least about `10%` peak-RSS reduction and about `5%` partition-phase speedup on `100k+` first-order sparse graphs before CSR expands further
- Phase 3 extensions must justify themselves case-by-case

### Backend templating default

Preferred implementation strategy:

- keep the outer optimizer type as `InfomapOptimizer<Objective>`
- template only the hot inner methods on the backend/accessor type
- select the backend once per call, then run the inner templated move loop

This keeps runtime selection logic and binary growth more contained than templating the entire optimizer class on both objective and backend.

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
