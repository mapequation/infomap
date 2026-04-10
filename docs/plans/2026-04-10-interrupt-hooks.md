# Interrupt Hooks Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add cooperative interruption hooks to long-running Infomap runs with clean unwind and no noticeable performance regression when no hook is configured.

**Architecture:** Keep the API narrow and core-owned. Store a lightweight callback pointer and polling state on `InfomapBase`, propagate that state to sub- and super-Infomap instances, and signal interruption with a dedicated exception that naturally unwinds existing RAII-managed state. Poll only at explicit safe points and throttle checks inside hot loops so the default no-callback path stays effectively unchanged.

**Tech Stack:** C++ core runtime, doctest C++ tests, existing native benchmark harness

---

### Task 1: Add core interrupt API and exception

**Files:**
- Modify: `src/core/InfomapBase.h`
- Modify: `src/Infomap.h`

**Step 1: Add the failing test skeleton**

Add a lifecycle test that configures an interrupt hook on `InfomapWrapper`, calls `run()`, and expects a dedicated interruption exception.

**Step 2: Run test to verify it fails**

Run: `make test-native CTEST_ARGS='-R infomap_cpp_lifecycle_tests'`
Expected: FAIL because the interrupt API and exception type do not exist yet.

**Step 3: Write minimal implementation**

Add:
- a dedicated exception type for interruption
- a callback type alias based on a raw function pointer plus `void*`
- setters/getters for the interrupt handler
- a cheap `isInterruptedEnabled()` helper and polling counter configuration

Expose the new API through `InfomapWrapper` by inheritance or thin wrapper methods.

**Step 4: Run test to verify it still fails for polling**

Run: `make test-native CTEST_ARGS='-R infomap_cpp_lifecycle_tests'`
Expected: FAIL because the hook exists but is not polled yet.

**Step 5: Commit**

```bash
git add src/core/InfomapBase.h src/Infomap.h test/cpp/test_infomap_wrapper.cpp
git commit -m "feat: add interrupt hook api"
```

### Task 2: Poll at safe top-level boundaries

**Files:**
- Modify: `src/core/InfomapBase.h`
- Modify: `src/core/InfomapBase.cpp`

**Step 1: Wire polling helpers**

Add helper methods on `InfomapBase`:
- unconditional safe-point poll
- throttled poll for hot loops
- state reset before each top-level run

**Step 2: Add polling at coarse safe points**

Poll:
- before and after major phases in `run(Network&)`
- at each trial boundary
- before recursive/super-module/sub-module sub-runs

Use the dedicated exception for abort.

**Step 3: Run targeted lifecycle tests**

Run: `make test-native CTEST_ARGS='-R infomap_cpp_lifecycle_tests'`
Expected: the interruption test now passes for a top-level safe point.

**Step 4: Commit**

```bash
git add src/core/InfomapBase.h src/core/InfomapBase.cpp test/cpp/test_infomap_wrapper.cpp
git commit -m "feat: add safe-point interruption polling"
```

### Task 3: Add throttled polling to optimization loops

**Files:**
- Modify: `src/core/InfomapOptimizer.h`

**Step 1: Add polling in core optimization control flow**

Poll:
- once per `optimizeActiveNetwork()` loop
- throttled inside `tryMoveEachNodeIntoBestModule()`

Do not call the external callback from OpenMP worker iterations in the first version. Keep parallel mode responsive at loop boundaries only.

**Step 2: Extend test coverage**

Add a test that interrupts after several polls so execution enters the optimizer before aborting.

**Step 3: Run targeted tests**

Run: `make test-native CTEST_ARGS='-R infomap_cpp_(lifecycle|partition)_tests'`
Expected: PASS

**Step 4: Commit**

```bash
git add src/core/InfomapOptimizer.h test/cpp/test_infomap_wrapper.cpp
git commit -m "feat: poll interrupt hooks in optimizer loops"
```

### Task 4: Add polling to flow-iteration loops

**Files:**
- Modify: `src/utils/FlowCalculator.cpp`
- Modify: `src/utils/FlowCalculator.h`
- Modify: `src/core/InfomapBase.cpp` if needed for call-site plumbing

**Step 1: Thread interrupt context into flow calculation**

Pass a lightweight polling callable or owning `InfomapBase` pointer into flow calculation without broad API churn.

**Step 2: Poll once per PageRank-style iteration**

Apply polling in:
- `powerIterate()`
- the regularized directed flow loop
- any equivalent iterative flow path not already covered by `powerIterate()`

**Step 3: Add or extend flow/lifecycle tests**

Add a targeted test that interrupts during flow calculation, or document why top-level/lifecycle coverage is the smallest sufficient check.

**Step 4: Run targeted tests**

Run: `make test-native CTEST_ARGS='-R infomap_cpp_(lifecycle|flow)_tests'`
Expected: PASS

**Step 5: Commit**

```bash
git add src/utils/FlowCalculator.cpp src/utils/FlowCalculator.h src/core/InfomapBase.cpp test/cpp/test_infomap_wrapper.cpp test/cpp/test_flow.cpp
git commit -m "feat: add interruption polling to flow iterations"
```

### Task 5: Verify cleanup and no-hook behavior

**Files:**
- Modify: `test/cpp/test_infomap_wrapper.cpp`

**Step 1: Add cleanup/regression tests**

Cover:
- interruption throws the dedicated exception
- a later run without interruption still works
- no-hook behavior still matches existing sanity checks

**Step 2: Run the narrow regression suite**

Run: `make test-native CTEST_ARGS='-R infomap_cpp_(lifecycle|flow|partition)_tests'`
Expected: PASS

**Step 3: Commit**

```bash
git add test/cpp/test_infomap_wrapper.cpp test/cpp/test_flow.cpp
git commit -m "test: cover interruption cleanup paths"
```

### Task 6: Performance regression check

**Files:**
- No source changes required unless regression is found

**Step 1: Build native and benchmark baseline**

Run: `make build-native`

Run: `make bench-native NATIVE_BENCHMARK_REPEATS=5 NATIVE_BENCHMARK_PROFILE=baseline NATIVE_BENCHMARK_OUTPUT=build/benchmarks/native-before.json`

**Step 2: Benchmark the changed branch**

Run: `make bench-native NATIVE_BENCHMARK_REPEATS=5 NATIVE_BENCHMARK_PROFILE=baseline NATIVE_BENCHMARK_OUTPUT=build/benchmarks/native-after.json`

**Step 3: Compare results**

Inspect summary or JSON deltas. If the no-hook path moves materially, first reduce polling frequency in hot loops before considering broader redesign.

**Step 4: Final commit if tuning was needed**

```bash
git add <tuned-files>
git commit -m "perf: reduce interrupt polling overhead"
```
