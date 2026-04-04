# Performance And Memory Optimization Plan

> **For Claude:** Review this plan as a skeptical C++ systems/performance engineer. Default review mode is owner + red team, not consensus writing.

**Goal:** Improve runtime and memory behavior of the current pointer-backed Infomap implementation without reviving the additive CSR rollout as the default path.

**Architecture:** Keep `InfoNode`/tree ownership as the production model. Use the CSR branch only as evidence: benchmark infrastructure was worth keeping, additive CSR could win narrowly on one hot path, but ownership/lifetime bugs and large always-live node state are more promising optimization targets. Work in bounded slices with explicit stop/replan gates.

**Tech Stack:** C++, Make/CMake, native benchmark harness from PR #399, sanitizer/nightly learnings from PR #400, `claude -p` for non-interactive plan review.

---

## 1. Findings That Constrain This Plan

- The CSR experiment proved that measurement and conformance work were high-value, but it did **not** prove that additive CSR is the right general memory strategy.
- The merged native benchmark harness already gives the minimum reusable signal:
  - `read_input_sec`
  - `run_sec`
  - `total_sec`
  - `peak_rss_bytes`
  - `sizeof(InfoNode)` and `sizeof(InfoEdge)`
- The most valuable correctness findings from the CSR branch were ownership/lifetime issues:
  - `generateSubNetwork(...)` was mutating caller-visible `InfoNode.index`
  - collapsed child chains were not cleaned up consistently
  - sanitizer coverage surfaced bugs that would dominate any micro-optimization win on larger higher-order workloads
- This plan therefore optimizes the **current** pointer-backed design first:
  - ownership/lifetime correctness
  - `InfoNode` footprint and field residency
  - `InfoEdge` / adjacency overhead in the existing model
  - hot pointer-backed algorithm paths
- Explicit non-goals until later evidence reopens them:
  - broader CSR continuation
  - module-level CSR
  - higher-order CSR
  - replacing the tree/hierarchy model

## 2. Success Criteria And Stop Conditions

### Working benchmark policy

- All performance and memory measurements in this plan use a release build. Do not use sanitizer or debug builds for benchmark claims.
- Fast local slice verification:
  - `make build-native`
  - relevant focused test target
  - `make bench-native NATIVE_BENCHMARK_PROFILE=smoke NATIVE_BENCHMARK_REPEATS=1`
- Mandatory checkpoint benchmark:
  - `make bench-native NATIVE_BENCHMARK_PROFILE=baseline NATIVE_BENCHMARK_REPEATS=3`
- Mandatory pre-merge benchmark for a claimed memory/perf checkpoint:
  - `make bench-native NATIVE_BENCHMARK_PROFILE=full NATIVE_BENCHMARK_REPEATS=3`
- Mandatory benchmark cases to discuss in every checkpoint note:
  - first-order small: `twotriangles`, `ninetriangles`, `modular_w`, `modular_wd`
  - first-order large: `sparse_100k`, `ring_of_cliques_100k`
  - higher-order/state: `states`, `multilayer`, `state_ring_5k`
  - if a change specifically targets higher-order memory, extend the harness or run an explicit larger state-network case before claiming success

### Runtime interpretation

- Before any optimization claim, characterize baseline variance by running:
  - `make bench-native NATIVE_BENCHMARK_PROFILE=baseline NATIVE_BENCHMARK_REPEATS=10`
- Compute per-case coefficient of variation from that run and record it in the plan PR.
- Treat sub-2% movement as noise only if the measured coefficient of variation supports that threshold; otherwise use `2 x CV` as the minimum meaningful-change threshold for that case.
- A workstream may claim a runtime win only if:
  - `median_run_sec` improves by at least 5% on one intended large workload, or
  - `median_run_sec` improves by at least 3% on both `sparse_100k` and `ring_of_cliques_100k`
- No checkpoint may regress `median_run_sec` by more than 3% on `sparse_100k` or `ring_of_cliques_100k` unless the checkpoint is explicitly memory-first and the tradeoff is documented.

### Memory interpretation

- For non-structural slices, require at least neutral `peak_rss_bytes_max` on `baseline`.
- For structural memory workstreams (`InfoNode` slimming, ownership cleanup, edge/storage changes), require one of:
  - at least 5% peak RSS reduction on one large first-order case and no worse than neutral on the other
  - or a clear higher-order/state-network RSS reduction on the targeted workload
- Do **not** claim overall success from benchmark-only accounting or `sizeof(...)` changes alone; claim success only from measured benchmark RSS on the target cases.

### Required correctness gates

- Ownership-sensitive changes must pass:
  - `make test-native JOBS=1`
  - `make test-sanitizers JOBS=1`
- Reuse/lifecycle-sensitive changes must also include at least one rerun pattern:
  - repeated `run()` on the same instance
  - hard partition / restore path
  - recursive or subnetwork rebuild path if touched

### Stop / replan rules

- Stop and update this plan before implementation continues if:
  - a proposed slice requires changing the tree representation itself
  - benchmark evidence shows the chosen workstream is noise-only after two bounded slices
  - a memory reduction requires behavior-breaking API or output changes
  - sanitizer work exposes a broader ownership model problem rather than a bounded fix
- If two consecutive bounded optimization slices fail to produce measurable movement on the targeted metric, pause that workstream and re-rank the backlog before more edits.

## 3. Phased Workstreams

### Phase 0: Measurement And Correctness Baseline

**Purpose:** establish a credible post-correctness optimization baseline with concrete missing measurements filled in.

Known gaps on `master` at plan start:
- no recorded variance characterization for `baseline`
- no required profiling artifact for CPU hotspots
- no required allocation-profile artifact for memory hotspots
- no larger higher-order/state-network benchmark beyond the small current fixtures
- no repeated-run benchmark or explicit retained-memory measurement path

Tasks:
1. Rebase or merge the known correctness fixes that this plan relies on before recording the optimization baseline:
   - PR #398
   - PR #400
2. Record the release-build benchmark baseline in the plan PR:
   - `smoke`
   - `baseline`
   - selected `full` cases if runtime permits
3. Run and record baseline variance:
   - `make bench-native NATIVE_BENCHMARK_PROFILE=baseline NATIVE_BENCHMARK_REPEATS=10`
   - compute CV for the large target cases and use that to validate or revise the numeric thresholds above
4. Produce a CPU hotspot profile for the large first-order cases:
   - Linux preferred: `perf record` + `perf report`
   - if Linux `perf` is unavailable locally, use the closest available profiler and document the substitution explicitly
5. Produce an allocation profile for the same cases:
   - Linux preferred: `heaptrack`
   - if `heaptrack` is unavailable locally, document the blocker and defer Phase 2/3 ranking until allocation evidence exists
6. Record the current node budget before any `InfoNode` slimming decisions:
   - current `sizeof(InfoNode)`
   - current `sizeof(InfoEdge)`
   - `num_nodes * sizeof(InfoNode)` vs `peak_rss_bytes_max` for at least `sparse_100k` and `ring_of_cliques_100k`
7. If higher-order memory work is intended in Phase 2, add one larger state-network benchmark case before Phase 2 starts.
8. Add one repeated-run or retained-memory benchmark/check path if the current harness cannot already exercise it.

Exit gate:
- PR #398 and PR #400 equivalent fixes are present in the working base
- baseline benchmark artifacts exist
- baseline variance is recorded
- CPU and allocation profiles exist for the large first-order cases
- the node-budget-to-RSS ratio is recorded for the large first-order cases
- sanitizer and rerun expectations are fixed for later phases

Checkpoint status (2026-04-04): `complete`
- prerequisite lifecycle fixes from the PR #398 / PR #400 line are present in the branch
- baseline and variance artifacts are recorded in `build/benchmarks/phase0-baseline-r10.json` and `build/benchmarks/phase0-baseline-r10.md`
- large-case CPU/allocation artifacts are recorded in `build/benchmarks/phase0-prof/` using macOS substitutes (`sample` and `heap`) because Linux `perf`/`heaptrack` were unavailable locally
- node-budget ratios are recorded for `sparse_100k` and `ring_of_cliques_100k`; `InfoNode` residency was about `10.6%` and `5.7%` of peak RSS respectively
- repeated-run benchmark coverage was added, and the refreshed `build/benchmarks/phase0-smoke-iter2-rerun.json` artifact is stable across same-instance reruns for the smoke cases
- targeted lifecycle/partition sanitizer coverage is green via `make test-sanitizers ... -R "infomap_cpp_(lifecycle|partition)_tests"`

### Phase 1: Ownership And Lifetime Fixes

**Purpose:** remove correctness/lifetime issues that can swamp performance or memory results.

Primary slice candidates, in order:
1. remaining cleanup paths that can leave `InfoNode` or edge-owned state live after collapse/restore/reinit
2. repeated-run and reinit correctness on the same `Infomap` instance
3. hard-partition and subnetwork lifecycle correctness where memory can stay retained unexpectedly
4. any ownership bug newly exposed by the Phase 0 repeated-run and allocation evidence

Rules:
- Each slice must be framed as a bounded ownership hypothesis, not a generic cleanup.
- Prefer sanitizer-visible fixes over speculative refactors.
- Do not broaden into tree redesigns.

Exit gate:
- targeted ownership fixes are green under sanitizer
- no known lifecycle bug remains on touched paths
- benchmark regressions stay within the runtime/memory guardrails above

### Phase 2: `InfoNode` Footprint Reduction

**Purpose:** attack the largest always-live memory structure directly.

Entry condition:
- the Phase 0 node-budget-to-RSS ratio is recorded
- proceed only if `InfoNode` residency is still large enough to plausibly move RSS in a meaningful way

Primary slice candidates, in order:
1. audit `InfoNode` fields into buckets:
   - always-live and hot
   - always-live but cold
   - higher-order-only
   - metadata/output-only
   - recursive/sub-Infomap-only
2. externalize or lazily allocate the coldest fields first, especially fields already treated as externally managed in copy/assignment behavior
3. reduce padding/alignment waste only after field residency is improved

Rules:
- Prefer field residency changes over bit-packing tricks.
- Do not mix semantic changes with layout changes in the same slice.
- Any `InfoNode` layout change must be benchmarked on both first-order and higher-order workloads.
- If Phase 0 shows `num_nodes * sizeof(InfoNode)` is not a substantial share of peak RSS on the target workloads, pause this phase and re-rank the backlog before editing `InfoNode`.

Exit gate:
- at least one measurable RSS win on a targeted workload
- no >3% runtime loss on the large first-order cases unless explicitly accepted as a higher-order memory tradeoff

### Phase 3: Current Adjacency / Edge Overhead

**Purpose:** reduce overhead in the existing pointer-backed adjacency model without introducing additive backends.

Entry condition:
- CPU or allocation profiling must identify edge/adjacency/storage overhead as a meaningful contributor

Primary slice candidates, in order:
1. identify always-live edge-side allocations or duplicate adjacency ownership that can be removed safely
2. reduce construction or retention overhead in subnetwork/module rebuild paths
3. optimize the hottest pointer-based traversal path only after measurement identifies it clearly

Rules:
- No new backend abstraction for this phase.
- No module-level CSR revival through the back door.
- Keep the data model pointer-backed and localize each slice to a measurable hotspot.
- If Phase 0/2 evidence shows `InfoNode` dominates the memory budget, Phase 3 is optional and may be skipped.

Exit gate:
- a targeted benchmark win or memory reduction on the measured hotspot
- no correctness regression in consolidation, hard partition, or recursive behavior

### Phase 4: Hot Algorithm Paths

**Purpose:** optimize the pointer-backed implementation after the structural memory work has been exhausted or stabilized.

Primary slice candidates:
- move-loop bookkeeping
- objective-function data access
- consolidation rebuild cost
- repeated-pass allocations in current pointer-backed code

Rules:
- Only start this phase after at least one of the earlier phases has been executed and benchmarked.
- Each slice must be justified by current benchmark/profiling evidence, not by intuition from the CSR branch.

Exit gate:
- meaningful runtime movement on the intended large workloads
- neutral or improved memory behavior on `baseline`

## 4. Agent Review And Execution Loop

### Default collaboration model

- Codex owns the plan and later the implementation branch.
- Claude acts as an explicit red-team reviewer.
- Do **not** use symmetric voting or consensus writing as the default. A single owner plus skeptical review produces clearer dispositions and less plan drift.
- Optional one-time exception: if the first draft is visibly underconstrained, run one early “two drafts + synthesis” round, then return to owner + red team.

### Non-interactive `claude -p` review loop

The review loop is for **plan readiness**, not for every implementation slice. After the plan is marked ready, benchmark gates and stop conditions govern the implementation work.

The review loop should run from shell without human-in-the-loop between rounds.

Prerequisite:
- local `claude` CLI must be installed and authenticated before the planning phase can be declared complete
- default to normal `claude -p`, not `--bare`
- use `--bare` only if the environment is intentionally configured for API-key-only auth
- if normal `claude -p` is unavailable or returns an auth/setup error, stop at plan-only state and fix the toolchain before implementation starts

Round structure:
1. Codex updates this Markdown draft.
2. Codex invokes `claude -p` with:
   - the full current Markdown
   - a compact CSR findings summary
   - repo and PR context
   - an explicit skeptical-review rubric
3. Claude returns:
   - `Findings`
   - `Recommended Next Slices`
   - `Experiments Worth Running`
   - `Likely Dead Ends`
   - `Benchmark/Measurement Critique`
   - `Ready / Not Ready`
4. Codex updates the Markdown and records dispositions for material findings:
   - `accepted`
   - `rejected`
   - `deferred`
5. Repeat until the readiness gate below is met.

The prompt should always include:
- this plan file
- CSR findings from PR #397
- benchmark/test extraction from PR #399
- `generateSubNetwork(...)` side-channel fix from PR #398
- sanitizer/nightly ownership and CI learnings from PR #400
- current benchmark capabilities and known benchmark blind spots

Suggested shell pattern:

```bash
PROMPT_FILE=$(mktemp)
cat > "$PROMPT_FILE" <<'EOF'
<review prompt with repo context and the full plan text>
EOF

claude -p --output-format text < "$PROMPT_FILE"
```

Track each review round in the branch by updating this plan with:
- the round date
- Claude verdict: `Ready` or `Not Ready`
- short dispositions for each material finding

### Readiness gate before implementation begins

Do not start implementation on this branch until:
- local `claude -p` is runnable and authenticated
- Claude returns `Ready` with no unresolved major planning gaps
- Codex also judges that benchmark policy, work ordering, and stop conditions are decision-complete
- the next implementation slice can be named without making a new architectural decision

## 5. First Implementation Slice After Plan Approval

When the readiness gate is passed, the first implementation slice is the first incomplete task in Phase 0.

Default tie-break:
- choose the smallest slice that improves decision quality for the rest of the branch, not the largest possible optimization.

## 6. Explicit Assumptions

- This branch is intentionally used for both planning and implementation.
- The first commits on the branch remain plan-only.
- PR #399 benchmark infrastructure is the measurement starting point.
- PR #397 is treated as evidence that additive CSR is not the default next path.
- Large memory wins are more likely to come from ownership/lifetime and `InfoNode` residency work than from new adjacency abstractions.

## 7. Plan Review Log

### Round 1 — 2026-04-04

- Claude verdict: `Not Ready`
- Accepted findings:
  - add explicit CPU and allocation profiling to Phase 0
  - ground `InfoNode` slimming behind a measured node-budget-to-RSS ratio
  - characterize benchmark variance before treating 2%/3%/5% movements as meaningful
  - make it explicit that the `claude -p` loop governs plan readiness, not every implementation slice
- Deferred:
  - a second red-team round after the revised draft is stable
- Current status:
  - plan branch created
  - plan document exists
  - implementation has **not** started yet because the readiness gate is not yet passed

### Round 2 — 2026-04-04

- Claude verdict: `Ready`
- Accepted findings:
  - no remaining P1 or P2 planning gaps
  - profiling, node-budget grounding, and variance-based thresholds are now explicit enough for implementation
- Current status:
  - plan readiness gate is passed
  - implementation may start with the first incomplete task in Phase 0
