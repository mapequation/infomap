# Python API Phase 5 — Close the SWIG Instance Leak

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Bite-sized steps; run every verification; commit per logical step; STOP + report if blocked.

**Goal:** Remove the transitional `Infomap.__getattr__` forward (in `_facade.py`) that leaks undocumented SWIG access on `im`, after rerouting every internal caller to the sanctioned `obj._core.<name>` boundary.

**Architecture:** `Infomap` composes a SWIG core at `self._core`. The `__getattr__` at `_facade.py:720` forwards any unknown attribute into `self._core`, keeping undocumented SWIG calls (`setDirected`, `flowModelIsSet`, `haveModules`, …) working at runtime while invisible to `dir()`/inspect. Phase 5 reroutes the handful of internal callers to address `._core` explicitly, then deletes the forward — the deliberate, endorsed break of UNDOCUMENTED SWIG access on `im`. Public facade members (`network`, `add_link`, `run`, `codelength`, `num_nodes`, …) are unaffected.

**Tech Stack:** Pure Python. No C++/SWIG recompile, no regen. Verify with `.venv/bin/python -m pytest test/python` (read counts from JUnit XML only) and `ruff`.

---

## Ground truth established during reconnaissance (do not re-derive)

- Baseline suite (before any change): `tests="323" errors="0" failures="0" skipped="20"` (JUnit XML).
- The 7 forwarded names live ONLY on `_core`, never as real facade members:
  `stateInput`, `multilayerInput`, `isMultilayerNetwork`, `numLeafNodes`, `flowModelIsSet`, `setDirected`, `haveModules`. So each must be rerouted to `obj._core.<name>`.
- `network` IS a real facade property (`Infomap.__dict__`, pinned in `test/python/public_surface_baseline.json` line 202). Leave `infomap.network.X` calls untouched.
- `result.py:175` (`core.haveMemory()` where `core = engine._core`) and `_results.py:1062` (`self._core.haveMemory()`) already go through `_core`. No change.
- `test_public_surface.py:36` skips any name starting with `_`. `__getattr__` is a dunder → excluded from the pinned surface. Removing it MUST NOT change the baseline. Do not regenerate the baseline.
- Freshness targets exist: `mk/python.mk:90` (`test-python-swig-freshness`, pure `--check`) and `mk/test.mk:67` (`test-binding-options-freshness`, depends on `build-native`; `./Infomap` binary already present so it is a no-op). We change zero generated/SWIG/C++ inputs.

### Authoritative test-count command (the wrapper mangles pytest stdout — trust ONLY the XML)

```bash
.venv/bin/python -m pytest test/python --junitxml=/tmp/p5.xml -q >/tmp/p5_stdout.txt 2>&1; echo "exit=$?"
grep -oE '<testsuite [^>]*>' /tmp/p5.xml
```

---

## Call sites that rely on the forward (the complete list)

### Production source (`interfaces/python/src/infomap/`)
- `_networkx.py:179-180` — `infomap.flowModelIsSet`, `infomap.setDirected(True)`
- `_igraph.py:122-123` — `infomap.flowModelIsSet`, `infomap.setDirected(True)`
- `_scipy.py:94-95` — `infomap.flowModelIsSet`, `infomap.setDirected(True)`
- `_edge_index.py:153-154` — `infomap.flowModelIsSet`, `infomap.setDirected(True)`
- `_summary.py:44-48` — `im.stateInput`, `im.multilayerInput`, `im.isMultilayerNetwork()`, `im.numLeafNodes()`
  (NOTE: `stateInput`/`multilayerInput` were NOT in the spec's named list but the broad grep found them; they live only on `_core` and must be rerouted.)
- `graphrag.py:541` — `im.haveModules()`
- `export.py:36` — `infomap.haveModules()`

Leave alone: every `infomap.network.X` call (public property), and all snake_case facade members.

### Test fakes that stand in for `Infomap` and must mirror the `_core` boundary
When production reroutes to `obj._core.<name>`, fakes passed to (or monkeypatched as) that production code must expose those names via `_core`, or the rerouted call fails with `AttributeError`. The uniform minimal fix is to add a `_core` property returning the fake itself:

```python
    @property
    def _core(self):
        return self
```

Fakes needing this:
- `test/python/test_shell.py` — `FakeInfomap` (line 7): `_summary_data` reads `stateInput`, `multilayerInput`, `isMultilayerNetwork()`, `numLeafNodes()`.
- `test/python/test_networkx.py` — inner `FakeInfomap` (line 62): rerouted `_networkx.py` reads `flowModelIsSet`/`setDirected`.
- `test/python/test_igraph.py` — inner `FakeInfomap` (line 120), `Recorder` (line 160), `_Recorder` (line 250): rerouted `_igraph.py`.
- `test/python/test_edge_index.py` — `Recorder` (line 30): rerouted `_edge_index.py`.

NOT needed (use real `Infomap`, which has `_core`): `test_scipy.py`, `test_graphrag.py`, `test_export.py`, `test_anndata.py` (all build real instances via `make_infomap` or `Infomap(...)`).

---

## Task 1: Reroute production call sites to `._core`

**Files:**
- Modify: `interfaces/python/src/infomap/_networkx.py:179-180`
- Modify: `interfaces/python/src/infomap/_igraph.py:122-123`
- Modify: `interfaces/python/src/infomap/_scipy.py:94-95`
- Modify: `interfaces/python/src/infomap/_edge_index.py:153-154`
- Modify: `interfaces/python/src/infomap/_summary.py:44-48`
- Modify: `interfaces/python/src/infomap/graphrag.py:541`
- Modify: `interfaces/python/src/infomap/export.py:36`

- [ ] **Step 1: Edit `_networkx.py`**

```python
    if not infomap._core.flowModelIsSet and g.is_directed():
        infomap._core.setDirected(True)
```

- [ ] **Step 2: Edit `_igraph.py`**

```python
    if not infomap._core.flowModelIsSet and g.is_directed():
        infomap._core.setDirected(True)
```

- [ ] **Step 3: Edit `_scipy.py`**

```python
    if not infomap._core.flowModelIsSet and directed:
        infomap._core.setDirected(True)
```

- [ ] **Step 4: Edit `_edge_index.py`**

```python
    if not infomap._core.flowModelIsSet and directed:
        infomap._core.setDirected(True)
```

- [ ] **Step 5: Edit `_summary.py` (all four reads)**

```python
            "state_input": im._core.stateInput,
            "multilayer_input": im._core.multilayerInput,
            "multilayer_network": im._core.isMultilayerNetwork(),
```
and
```python
            "leaf_nodes": im._core.numLeafNodes(),
```

- [ ] **Step 6: Edit `graphrag.py`**

```python
    if not im._core.haveModules():
```

- [ ] **Step 7: Edit `export.py`**

```python
    if not infomap._core.haveModules():
```

- [ ] **Step 8: Update the four test fakes to expose `_core`**

Add this property to each of: `test_shell.py::FakeInfomap`, `test_networkx.py` inner `FakeInfomap`, `test_igraph.py` inner `FakeInfomap` / `Recorder` / `_Recorder`, `test_edge_index.py::Recorder`:

```python
    @property
    def _core(self):
        return self
```

(Place it after the `flowModelIsSet`/attribute declarations within each class. `fake._core.flowModelIsSet` then resolves to the fake's own class attr, and `fake._core.setDirected(...)` calls the fake's own method, so existing assertions on `.directed`/`.loaded`/etc. still hold.)

- [ ] **Step 9: Run the full suite (forward still present — everything must stay green)**

```bash
.venv/bin/python -m pytest test/python --junitxml=/tmp/p5.xml -q >/tmp/p5_stdout.txt 2>&1; echo "exit=$?"
grep -oE '<testsuite [^>]*>' /tmp/p5.xml
```
Expected: `failures="0" errors="0" tests="323"`.

- [ ] **Step 10: Ruff clean**

```bash
ruff check interfaces/python/src/infomap test/python
```
Expected: `All checks passed!`

- [ ] **Step 11: Commit**

```bash
git add interfaces/python/src/infomap test/python
git commit -m "refactor(python): route internal SWIG calls through _core boundary"
```

---

## Task 2: Remove `__getattr__` and add the guard test

**Files:**
- Modify: `interfaces/python/src/infomap/_facade.py:720-728` (delete the method)
- Create: `test/python/test_no_swig_leak.py`

- [ ] **Step 1: Write the guard test (failing first — file does not exist yet, so it cannot pass against the still-present forward)**

`test/python/test_no_swig_leak.py`:

```python
"""Guard: the transitional SWIG-forwarding ``__getattr__`` is gone.

Phase 5 removed ``Infomap.__getattr__`` (the forward into ``self._core``).
Undocumented SWIG names must no longer resolve on an ``Infomap`` instance,
while the documented public surface keeps working.
"""

import pytest

from infomap import Infomap


def test_infomap_defines_no_getattr_forward():
    assert "__getattr__" not in Infomap.__dict__


def test_undocumented_swig_name_raises_attribute_error():
    im = Infomap(silent=True)
    with pytest.raises(AttributeError):
        im.setDirected


def test_documented_surface_still_works():
    im = Infomap(silent=True)
    im.add_link(0, 1)
    im.run()
    assert im.codelength is not None
    assert im.network is not None
    assert im.num_nodes == 2
```

- [ ] **Step 2: Run the new test to confirm it FAILS while `__getattr__` is still present**

```bash
.venv/bin/python -m pytest test/python/test_no_swig_leak.py --junitxml=/tmp/p5_guard.xml -q >/tmp/p5_guard_stdout.txt 2>&1; echo "exit=$?"
grep -oE '<testsuite [^>]*>' /tmp/p5_guard.xml
```
Expected: at least one failure. `test_infomap_defines_no_getattr_forward` fails (forward present); `test_undocumented_swig_name_raises_attribute_error` fails (forward resolves `im.setDirected`).

- [ ] **Step 3: Delete `__getattr__` from `_facade.py`**

Remove exactly this block (lines 720-728, plus the blank line separating it):

```python
    def __getattr__(self, name):
        # Transitional: forward not-yet-migrated SWIG calls (e.g. the io
        # adapters' setDirected/flowModelIsSet/network) to Core. Removed in
        # Phase 5 once adapters route through Core. Keeps undocumented calls
        # working at runtime while they are absent from the discoverable
        # (dir()/inspect) surface.
        if name == "_core":
            raise AttributeError(name)
        return getattr(self._core, name)
```

- [ ] **Step 4: Run the new guard test — now PASSES**

```bash
.venv/bin/python -m pytest test/python/test_no_swig_leak.py --junitxml=/tmp/p5_guard.xml -q >/tmp/p5_guard_stdout.txt 2>&1; echo "exit=$?"
grep -oE '<testsuite [^>]*>' /tmp/p5_guard.xml
```
Expected: `failures="0" errors="0" tests="3"`.

- [ ] **Step 5: Run the full suite — green with the new test counted**

```bash
.venv/bin/python -m pytest test/python --junitxml=/tmp/p5.xml -q >/tmp/p5_stdout.txt 2>&1; echo "exit=$?"
grep -oE '<testsuite [^>]*>' /tmp/p5.xml
```
Expected: `failures="0" errors="0" tests="326"` (323 + 3 new). Tests ≥ 324.

- [ ] **Step 6: Ruff clean**

```bash
ruff check interfaces/python/src/infomap test/python
```
Expected: `All checks passed!`

- [ ] **Step 7: Public surface unchanged (baseline NOT regenerated)**

```bash
.venv/bin/python -m pytest test/python/test_public_surface.py -q >/tmp/p5_surface.txt 2>&1; echo "exit=$?"
git status --porcelain test/python/public_surface_baseline.json
```
Expected: test passes; baseline file shows no modification.

- [ ] **Step 8: Freshness gates**

```bash
make test-python-swig-freshness
make test-binding-options-freshness
```
Expected: both succeed (we changed zero generated/SWIG/C++ inputs).

- [ ] **Step 9: Commit**

```bash
git add interfaces/python/src/infomap/_facade.py test/python/test_no_swig_leak.py
git commit -m "refactor(python): remove transitional __getattr__ SWIG forward + add leak guard"
```

---

## Hard gates (all must pass; show output in the final report)
- Suite JUnit XML: `failures="0" errors="0"`, tests ≥ 324.
- `ruff check interfaces/python/src/infomap test/python` clean.
- `make test-python-swig-freshness` and `make test-binding-options-freshness` clean.
- `test_public_surface` passes; baseline unchanged (no git diff on `public_surface_baseline.json`).
- `im.network` untouched and still works.

## Out of scope (DEFER to Phase 5b)
- Moving `_writers.py`/adapters into `io/`; `tl/`/`graphrag`/`export` reorg.
- Removing package-level `from ._bindings import *` re-exports and the strict import guard. `_facade.py` keeps importing `_bindings`.

## STOP conditions
- Any gate red after the relevant step → STOP and report with the exact failing output. Do not paraphrase pytest stdout; cite the JUnit XML `<testsuite>` line.
