# InfoNode/CSR Refactor Plan

## Summary

Refactor the internal `InfoNode`-centric model into two explicit layers instead of trying to make one object do everything:

1. a **hierarchy layer** for tree/module ownership, traversal, output, and sub-Infomap ownership
2. a **hot active-graph layer** for optimization-time node state and adjacency, with a pointer-backed backend first and a CSR backend second

This is the only realistic way to get full feature parity without touching the whole codebase repeatedly. Today `InfoNode` is simultaneously tree node, graph vertex, aggregation container, and owner for recursive sub-Infomaps; it is also large enough to matter materially for memory (`sizeof(InfoNode)=336`, `sizeof(InfoEdge)=32`, excluding allocator/vector overhead). The refactor should therefore **split storage concerns first**, then switch the hot active graph to CSR behind a compatibility layer.

This plan assumes the target is **full feature parity directly**, not a first-order-only milestone.

## Architecture And Implementation Changes

### 1. Introduce a storage abstraction boundary before changing storage

Add an internal abstraction layer with stable handles, not raw `InfoNode*`, for the optimization hot path.

Internal types to introduce:

- `NodeHandle` / `ModuleHandle`: stable integer handles
- `NodePayload`: flow/index/state/physical/layer/meta fields needed by objectives and movement logic
- `ActiveGraphView`: the flat optimization graph interface used by `InfomapOptimizer`, `MapEquation`, `MemMapEquation`, and `MetaMapEquation`
- `HierarchyView`: parent/child/path/leaf/module traversal interface used by `InfomapBase`, iterators, and output
- `AdjacencyRange`: iterable out/in-neighbor view that hides whether storage is pointer vectors or CSR arrays

Rules:

- `InfomapOptimizer*` and objective code stop iterating raw `InfoNode::outEdges()/inEdges()` directly and instead consume `ActiveGraphView`
- output and iterators stop depending on concrete sibling pointers directly and instead consume `HierarchyView`
- `InfoNode` remains temporarily as the pointer-backed implementation of `HierarchyView`
- the first backend behind `ActiveGraphView` is a compatibility backend over current `InfoNode`/`InfoEdge`
- the second backend is CSR

This is the key scope control mechanism: once those call sites depend on views/handles, the CSR work is localized to the new store plus a few bridge files instead of re-editing the entire tree every time.

### 2. Separate tree storage from hot graph storage

Do **not** make the whole Infomap tree CSR in the first real cutover. Instead:

- keep a logical tree/hierarchy store for:
  - parent/child/module relationships
  - `calculatePath()`
  - iterators
  - output
  - `owner` / sub-Infomap recursion
  - hard-partition restore
- move the hot active network to a compact store:
  - dense node arrays for payload
  - CSR adjacency arrays for out-edges
  - either transposed CSR or computed in-edge index for in-neighbor iteration
  - explicit module-membership arrays instead of walking sibling lists during optimization

The bridge between them is:

- leaf/module handles map back to hierarchy nodes
- hierarchy operations can materialize the next active graph view when `setActiveNetworkFromLeafs()` or `setActiveNetworkFromChildrenOfRoot()` is called
- subtree/subnetwork generation builds an `ActiveGraphView` from the hierarchy layer instead of cloning linked nodes/edges directly

This makes the first meaningful performance win land in the real hot path while preserving current feature semantics.

### 3. Refactor in four implementation phases

#### Phase A: Baseline, parity harness, and abstraction scaffolding

- Add `ActiveGraphView` and `HierarchyView` interfaces plus a pointer-backed adapter over current `InfoNode`
- Convert optimizer/objective code to read through the views without changing behavior
- Keep all runtime behavior pointer-backed at this phase
- Add backend-selection plumbing that is internal-only:
  - default backend = current pointer backend
  - CSR backend selectable only from tests/benchmarks or a non-public build-time/internal hook

#### Phase B: Build a full conformance test net before CSR

Add missing regression tests first so the storage swap has a safety cage.

Required new coverage:

- tree rewrite invariants
  - `replaceChildrenWithGrandChildren`
  - `replaceWithChildren`
  - `collapseChildren` / `expandChildren`
  - `releaseChildren`
  - sanity after repeated remove/rebuild cycles
- output parity
  - `.tree`
  - `.clu`
  - JSON/network-link output
  - state vs physical output for higher-order input
- recursive/sub-Infomap parity
  - coarse tune
  - fine tune
  - super-module search
  - repeated multi-trial reruns after hierarchy rebuilds
- higher-order parity
  - `states.net`
  - `multilayer.net`
  - `matchableMultilayerIds`
  - physical aggregation and `iterTreePhysical`
  - `getModules(states=true/false)` correctness
- hard partition parity
  - `.clu` hard/soft
  - `.tree` import
  - restore after optimization and after repeated trials
- meta-data parity
  - minimal meta-data fixture with `MetaMapEquation`
  - stable module-level meta aggregation after moves/consolidation
- determinism/parity checks
  - same top modules
  - same hierarchy depth
  - same codelength/index codelength within tolerance
  - same output files after normalization where appropriate

Also add backend A/B tests:

- pointer backend and CSR backend must produce identical results on the same fixture set
- these tests must run inside the C++ suite, not only through Python

#### Phase C: Implement the CSR backend for the active graph

Implement a CSR-backed `ActiveGraphView` for:

- first-order networks
- state networks
- module-level aggregated active networks
- regenerated subnetwork views

Storage requirements:

- dense arrays for payload
- CSR for out-neighbors
- efficient in-neighbor access, either:
  - second CSR transpose, or
  - a separate incoming index structure
- explicit module-membership arrays and flow aggregates replacing pointer chasing in the hot loop

The optimizer/objective layer must not know which backend it is using.

#### Phase D: Complete full feature parity and remove accidental pointer coupling

Finish parity for the features most likely to block the rollout:

- hard partition restore path
- recursive sub-Infomap ownership/coarse tune/super modules
- higher-order physical aggregation
- meta-data aggregation
- output/iterator traversal over the abstract hierarchy interface

Only once the full parity suite passes against CSR should CSR be allowed as the default backend.

### 4. Define the file boundary up front

The refactor should concentrate changes primarily in:

- the new storage/view layer
- `InfomapBase`
- `InfomapOptimizer*` and objective classes
- iterator/output adapters

The rest of the code should see:

- handles/views instead of concrete `InfoNode` storage details
- no direct knowledge of CSR layout

Explicit non-goal:

- no second pass of sweeping edits across the whole repo after the abstraction phase

## Test Plan And Required New Coverage

### C++ correctness additions required before cutover

Extend the current C++ suite with these new groups:

1. **Hierarchy mutation invariants**
   - child/sibling counts remain correct after every tree rewrite
   - path/depth/module numbering remain stable
   - leaf coverage is preserved after collapse/expand/replace cycles
   - no orphan nodes or parent mismatches after hard-partition restore

2. **Output parity tests**
   - golden or normalized comparisons for `.tree`, `.clu`, JSON, and Pajek/state-network outputs
   - first-order and higher-order fixtures
   - physical-vs-state outputs for memory networks

3. **Higher-order and physical-node behavior**
   - `iterTreePhysical`
   - `iterLeafNodesPhysical`
   - `getModules(states=true/false)`
   - `states.net` and `multilayer.net` parity
   - deterministic state-id behavior with `matchableMultilayerIds`

4. **Recursive hierarchy behavior**
   - coarse tune / fine tune / super-module passes produce stable trees
   - repeated `run()` on the same instance after hierarchy reconstruction
   - multi-trial best-tree restore still works with the abstraction

5. **Meta-data behavior**
   - module meta aggregation stays identical before/after moves and consolidation
   - meta codelength parity on a minimal fixture

6. **Backend conformance**
   - identical partitions, level counts, and codelengths for pointer vs CSR backends on the same fixtures
   - same failure behavior on malformed parser inputs

### Acceptance criteria for test readiness

Before CSR becomes default:

- all existing C++ and Python regression suites remain green
- new backend-conformance C++ tests are green
- output parity tests are green
- sanitizer runs are green for the CSR backend
- pointer backend remains available until CSR parity is complete

## Performance And Memory Measurement

### Add a native benchmark/memory harness

The current repo only has a small Python timing harness. That is not enough for this refactor.

Add a native benchmark path that reports:

- total wall time for `readInputData + initNetwork + run`
- partition-only wall time for the optimization phase
- peak RSS / peak resident memory
- internal storage accounting:
  - bytes in node payload arrays
  - bytes in adjacency arrays
  - bytes in hierarchy storage
  - bytes per node
  - bytes per edge

Implementation shape:

- one native benchmark executable or script-driven native harness
- JSON output committed as CI artifact
- same cases run against pointer backend and CSR backend

### Benchmark corpus

Use both real repo fixtures and deterministic generated graphs.

Repo fixtures:

- `examples/networks/twotriangles.net`
- `examples/networks/ninetriangles.net`
- `examples/networks/modular_w.net`
- `examples/networks/modular_wd.net`
- `examples/networks/states.net`
- `examples/networks/multilayer.net`

Generated workloads:

- large sparse first-order graph
- ring-of-cliques / SBM-style clustered graph
- larger deterministic state/higher-order graph

### Success metrics

Track both absolute and relative deltas:

- wall time
- partition-only wall time
- peak RSS
- bytes per node
- bytes per edge

Recommended success thresholds for calling the refactor worthwhile:

- first-order sparse graphs: at least `20%` lower peak RSS and `15%` lower partition-phase runtime
- no more than `5%` total-runtime regression on higher-order/meta/hard-partition cases while parity is being preserved
- zero correctness regressions in partitions, hierarchy depth, outputs, or codelength tolerance

If CSR improves partition time but not total runtime, report both numbers explicitly rather than claiming an overall win.

## Current Blockers And High-Risk Features

These are the current features that are genuine blockers or likely to force temporary fallback to the pointer backend:

### Hard blockers for a clean CSR rollout

- higher-order/state-network handling:
  - `haveMemory()`
  - `physicalNodes`
  - `stateNodes`
  - physical-node iterators/output
- hard partition restore:
  - `clusterDataIsHard`
  - `restoreHardPartition`
  - `collapseChildren` / `expandChildren` / `replaceWithChildren`
- recursive sub-Infomap ownership:
  - `owner`
  - `setInfomap()` / `disposeInfomap()`
  - coarse tune / super-module hierarchy rebuilds
- meta-data aggregation:
  - `metaCollection`
  - `MetaMapEquation`
- output and iterator contracts that assume pointer-linked siblings and parent chains

### Not blockers, but correctness-sensitive during the refactor

- `recordedTeleportation`
- `regularized`
- `variableMarkovTime*`
- precomputed flow input
- `matchableMultilayerIds`

Policy for these:

- they must be parity-tested
- they should not be used to justify pointer-only escape hatches unless a real storage-coupling issue is proven

### Features that may need temporary backend fallback

If full parity cannot be achieved immediately, the only acceptable temporary fallback is:

- keep the abstraction in place
- route the affected feature through the pointer-backed implementation internally
- do not remove public support or change user-visible semantics

That fallback is acceptable for:

- hard partition restore
- higher-order physical aggregation
- meta-data objective
- recursive super/coarse tune passes

It is **not** acceptable to silently drop feature support.

## Public/Internal Interface Changes

No intended public CLI/Python/JS behavior changes.

Internal changes that should exist in the final design:

- `NodeHandle`-style stable identifiers
- `ActiveGraphView` and `HierarchyView`
- backend-selectable active graph store
- internal benchmark hook for pointer-vs-CSR A/B runs

The backend selector should remain internal until CSR is proven correct and beneficial.

## Assumptions And Defaults

- This is a high-risk internal refactor and should be executed as a staged effort, with frequent checkpoints.
- Full feature parity is required before CSR becomes the default backend.
- The first real performance win should come from CSR in the active optimization graph, not from rewriting the entire hierarchy tree into CSR immediately.
- `InfoNode` can remain as a compatibility-backed hierarchy implementation during the transition; the goal is to stop it from being the only storage model, not necessarily to delete it on day one.
- The refactor is not considered successful without native runtime and memory measurements proving the gain.
