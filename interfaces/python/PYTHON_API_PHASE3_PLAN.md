# Python API 2.0 — Phase 3 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Run every verification. Commit per logical step. STOP + report if blocked.

**Goal:** Eliminate the two hand-maintained ~80-parameter signature lists in `Infomap.__init__`/`Infomap.run` by generating thin, explicitly-typed wrappers into a marker block in `_facade.py` from the parameter catalog, add a `--check` freshness guard wired into the existing build flow, and add a canonical frozen `Settings` alias.

**Architecture:** Hand-write the real construction/run logic as private `_init_from_options`/`_run_from_options` impl methods (the current bodies, moved verbatim). GENERATE thin explicit `__init__`/`run` wrappers — full keyword list + NumPy docstring, body delegates to the impl — into a `# === BEGIN/END generated ===` marker block in `_facade.py`. The generated wrapper order must be **byte-identical** to today's facade signatures (a hand-curated order that is NOT derivable from the catalog), so the generator is driven by an explicit ordered name list (`_FACADE_OPTION_ORDER`) validated against the catalog field set, with types/defaults/docs looked up from the catalog. `InfomapOptions` becomes `frozen`; `Settings` is added as its canonical public alias.

**Tech Stack:** Python 3.14, dataclasses, the parameter catalog pipeline (`scripts/parameter_catalog.py` + `scripts/generate_binding_options.py` driven by `./Infomap --print-json-parameters`), pytest, ruff, make.

---

## Background / locked facts (verified during planning)

These were confirmed by inspection before writing this plan — trust them:

1. **Baseline is green:** `286 passed, 20 skipped`; `ruff check` clean; binding-options freshness clean.
2. **`run` == `__init__` order**, with `initial_partition` inserted at position 1 (right after `args`). `run` has no extra options beyond `__init__`.
3. **The facade signature order is hand-curated** and does NOT equal the catalog `grouped()` order, the catalog raw declaration order, or `_options.py` field order. It must be reproduced exactly (gate: `test_public_surface` pins the exact signature string of `__init__`/`run`, both defined in the OWNED module `infomap._facade`).
4. **`pretty=False`** is a facade-only deprecated no-op param (in `test_signature_catalog.BACKWARD_COMPAT_EXTRA`). It is NOT in `InfomapOptions`/the catalog. It sits in the signature right after `silent` (Output group). It must remain in the generated signature, accepted-and-ignored.
5. **`include_self_links`** IS an `InfomapOptions` field (deprecated alias, default `None`) but sits in a different position in the facade (after `weight_threshold`, before `no_self_links`) than in `_options.py` (first Input field). Reproduce the facade position.
6. **No code mutates an `InfomapOptions` instance** anywhere (`from_mapping`/`to_kwargs`/`to_args` are read-only; no `options.x =` assignments). Freezing is safe. Dataclass already uses `slots=True`.
7. **The generator already builds**: the `InfomapOptions` dataclass (typed fields + NumPy docstring via `param.python_doc_type()` / `param.python_doc_description()` / `wrap_doc()`), and `_construct_args` (full param list). Reuse `wrap_doc`, `param.python_type()`, `param.python_default_expr()`, `param.python_doc_type()`, `param.python_doc_description()`.
8. **The build binary** `./Infomap` is not present in the worktree root; `make build-binding-options` depends on `build-native` and will build it. A prebuilt copy exists at `/Users/anton/kod/infomap/Infomap` (main checkout, identical source) usable for ad-hoc analysis. The compiled `.so` for the python tests is already current — no SWIG regen, no `.so` rebuild needed for the test suite.
9. **The exact facade option order (with `pretty`)** to reproduce, in `__init__` after `self, args` (and `run` additionally inserts `initial_partition` after `args`):

```
cluster_data, no_infomap, skip_adjust_bipartite_flow, bipartite_teleportation,
weight_threshold, include_self_links, no_self_links, node_limit,
matchable_multilayer_ids, assign_to_neighbouring_module, meta_data,
meta_data_rate, meta_data_unweighted, tree, ftree, clu, verbosity_level,
silent, pretty, out_name, no_file_output, clu_level, output,
hide_bipartite_nodes, print_all_trials, no_overwrite, print_config_fingerprint,
timing_json, summary_json, manifest_json, memory_report, two_level, flow_model,
directed, recorded_teleportation, use_node_weights_as_flow, to_nodes,
teleportation_probability, regularized, regularization_strength,
entropy_corrected, entropy_correction_strength, markov_time,
variable_markov_time, variable_markov_damping, variable_markov_min_scale,
preferred_number_of_modules, preferred_number_of_levels,
preferred_number_of_levels_strength, multilayer_relax_rate,
multilayer_relax_limit, multilayer_relax_limit_up, multilayer_relax_limit_down,
multilayer_relax_by_jsd, multilayer_relax_to_self, seed, num_trials,
core_loop_limit, core_level_limit, tune_iteration_limit,
core_loop_codelength_threshold, tune_iteration_relative_threshold,
fast_hierarchical_solution, prefer_modular_solution, inner_parallelization,
parallel_trials, converge, num_threads, threads, trial_offset, trial_results,
no_final_output, num_random_moves, max_degree_for_random_moves
```

The exact default + type for each name is in `_options.py` (lines 329–405) except `pretty` (`pretty=False`, no type annotation in the wrapper — match current rendering: the current facade renders bare `pretty=False`).

## File Structure

- **Modify** `interfaces/python/src/infomap/_facade.py`
  - Move the `__init__` body → new `_init_from_options(self, args, options)`.
  - Move the `run` body → new `_run_from_options(self, args, initial_partition, options)`.
  - Replace the two hand-listed `__init__`/`run` methods with a generated marker block.
  - Add `Settings` import + `__all__` entry.
- **Modify** `scripts/generate_binding_options.py`
  - Add `FACADE_OUT` path constant + `_FACADE_OPTION_ORDER` curated list + a `generate_facade(catalog)` function that renders ONLY the marker block, and a marker-aware splice (read existing file, replace between markers).
  - Wire `FACADE_OUT` into `outputs()`, `write_outputs()` (marker splice), and `check_outputs()`.
- **Modify** `interfaces/python/src/infomap/_options.py` (generated)
  - `@dataclass(frozen=True, slots=True)`; add `Settings = InfomapOptions`.
- **Modify** `interfaces/python/src/infomap/__init__.py` — export `Settings`.
- **Modify** `test/python/test_signature_catalog.py` — keep enforcing signature↔catalog; it passes trivially after the change. Add a note that freshness is also guarded by the generator `--check`.
- **No change** to SWIG, C++, CMake.

---

## Task 1: Add `Settings` alias and make `InfomapOptions` frozen (generator + regenerate)

`_options.py` is generated; edit the GENERATOR, then regenerate. Doing Settings/frozen first keeps later tasks focused on the facade.

**Files:**
- Modify: `scripts/generate_binding_options.py` (the `generate_python` function, dataclass decorator + trailing alias)
- Regenerate: `interfaces/python/src/infomap/_options.py`
- Modify: `interfaces/python/src/infomap/__init__.py` (export `Settings`)
- Test: `test/python/test_settings_alias.py` (new)

- [ ] **Step 1: Write the failing test**

Create `test/python/test_settings_alias.py`:

```python
"""Settings is the canonical public name for the frozen options dataclass.

InfomapOptions stays as a back-compat alias of the same class.
"""

import dataclasses

import pytest

import infomap
from infomap import Settings, InfomapOptions


@pytest.mark.fast
def test_settings_is_infomap_options():
    assert Settings is InfomapOptions


@pytest.mark.fast
def test_settings_is_frozen():
    assert dataclasses.fields(Settings)  # is a dataclass
    s = Settings(num_trials=5)
    with pytest.raises(dataclasses.FrozenInstanceError):
        s.num_trials = 10


@pytest.mark.fast
def test_settings_exported_in_package_all():
    assert "Settings" in infomap.__all__


@pytest.mark.fast
def test_settings_roundtrips_through_facade():
    s = Settings(num_trials=3, two_level=True)
    im = infomap.Infomap.from_options(s, args=None)
    im.add_link(1, 2)
    result = im.run_with_options(s)
    assert result is not None
```

- [ ] **Step 2: Run test to verify it fails**

Run: `.venv/bin/python -m pytest test/python/test_settings_alias.py -q`
Expected: FAIL — `ImportError: cannot import name 'Settings'` (and frozen assertion would fail too).

- [ ] **Step 3: Edit the generator — frozen dataclass + Settings alias**

In `scripts/generate_binding_options.py`, `generate_python`, change the dataclass decorator line in the emitted source from:

```python
            "@dataclass(slots=True)",
```

to:

```python
            "@dataclass(frozen=True, slots=True)",
```

Then, in the same function, locate the block that emits `_OPTION_FIELD_NAMES` (the list entry `"_OPTION_FIELD_NAMES = tuple(field.name for field in fields(InfomapOptions))",`). Immediately AFTER that line (still inside the same `lines.extend([...])` literal, before the blank lines that precede `def _construct_args(`), insert these two list items:

```python
            "",
            "",
            "# Settings is the canonical public name; InfomapOptions stays as a back-compat alias.",
            "Settings = InfomapOptions",
```

Concretely, the emitted tail goes from:

```python
            "_OPTION_FIELD_NAMES = tuple(field.name for field in fields(InfomapOptions))",
            "",
            "",
            "def _construct_args(",
```

to:

```python
            "_OPTION_FIELD_NAMES = tuple(field.name for field in fields(InfomapOptions))",
            "",
            "",
            "# Settings is the canonical public name; InfomapOptions stays as a back-compat alias.",
            "Settings = InfomapOptions",
            "",
            "",
            "def _construct_args(",
```

- [ ] **Step 4: Regenerate and inspect `_options.py`**

Run: `cd /Users/anton/kod/infomap/.claude/worktrees/python-api-improvement && PATH="/opt/homebrew/bin:$PATH" make build-binding-options`
Expected: `Updated interfaces/python/src/infomap/_options.py` (and R/TS unchanged content but rewritten).
Then confirm with: `.venv/bin/python - <<'PY'
import infomap._options as o, dataclasses
print("frozen:", o.InfomapOptions.__dataclass_params__.frozen)
print("Settings is InfomapOptions:", o.Settings is o.InfomapOptions)
PY`
Expected: `frozen: True` and `Settings is InfomapOptions: True`.

If `make build-binding-options` fails because `./Infomap` must be built (`build-native`), let it build — this is the existing C++ CLI, no source change. If the native build itself fails for an unrelated reason, STOP and report.

- [ ] **Step 5: Export `Settings` from the package**

In `interfaces/python/src/infomap/__init__.py`, the facade is imported via `from ._facade import *`. `Settings` will be exported once it is in `_facade.__all__` (Task added in this step). For now, add the import + `__all__` wiring in `_facade.py`:

In `interfaces/python/src/infomap/_facade.py`, change the `_options` import:

```python
from ._options import (
    InfomapOptions,
    _construct_args,
)
```

to:

```python
from ._options import (
    InfomapOptions,
    Settings,
    _construct_args,
)
```

and add `"Settings"` to the `__all__` list in `_facade.py` (keep alphabetical-ish ordering near `"Result"`):

```python
__all__ = [
    *_BINDINGS_ALL,
    "Infomap",
    "InfomapOptions",
    "MultilayerNode",
    "Result",
    "Settings",
    "entropy",
    "find_communities",
    "find_igraph_communities",
    "main",
    "perplexity",
    "plogp",
]
```

- [ ] **Step 6: Run the test to verify it passes**

Run: `.venv/bin/python -m pytest test/python/test_settings_alias.py -q`
Expected: PASS (4 passed).

- [ ] **Step 7: Run the public-surface + signature guards (no regression yet)**

Run: `.venv/bin/python -m pytest test/python/test_public_surface.py test/python/test_signature_catalog.py test/python/test_wrapper_args.py -q`
Expected: PASS. (Adding `Settings` to `__all__` does not change the `Infomap` class surface; `test_public_surface` inspects `Infomap` members only.)

- [ ] **Step 8: Ruff**

Run: `.venv/bin/ruff check interfaces/python/src/infomap scripts test/python`
Expected: `All checks passed!`

- [ ] **Step 9: Commit**

```bash
git add scripts/generate_binding_options.py interfaces/python/src/infomap/_options.py interfaces/python/src/infomap/_facade.py test/python/test_settings_alias.py
git commit -m "feat(python): frozen Settings alias for InfomapOptions"
```

---

## Task 2: Split facade bodies into impl methods (no signature change yet)

Pure refactor: move the `__init__`/`run` bodies into private impl methods, and have the existing hand-written `__init__`/`run` delegate. This must be behavior-preserving and keep both signatures byte-identical. Done as its own commit so the generator step (Task 3) only swaps the wrapper text.

**Files:**
- Modify: `interfaces/python/src/infomap/_facade.py` (`__init__` ~94–197, `run` ~1329–1444)

- [ ] **Step 1: Add `_init_from_options` impl method**

In `_facade.py`, immediately ABOVE the current `def __init__(self, ...)` method, add:

```python
    def _init_from_options(self, args, options):
        self._core = Core(_package_construct_args()(args, **options.to_kwargs()))
        self._network = _NetworkBuilder(self._core)
        self._multilayer = _MultilayerBuilder(self._core)
        self.node_id_to_label = {}
        # Run-generation token: incremented on every run(). A Result stamps the
        # generation it was created in; node-level access on a stale Result
        # (engine re-ran since) raises instead of reading freed memory. The C++
        # result tree is rebuilt on every run() (design §7).
        self._generation = 0
        self._result = None
```

- [ ] **Step 2: Make the existing `__init__` body delegate to the impl**

Replace the BODY of the current `__init__` (everything after its docstring, i.e. the two lines
`options = InfomapOptions.from_mapping(locals())` ... through `self._result = None`) with just:

```python
        options = InfomapOptions.from_mapping(locals())
        self._init_from_options(args, options)
```

Leave the `def __init__(self, ...):` signature and docstring untouched.

- [ ] **Step 3: Add `_run_from_options` impl method**

In `_facade.py`, immediately ABOVE the current `def run(self, ...)` method, add:

```python
    def _run_from_options(self, args, initial_partition, options):
        args = _package_construct_args()(args, **options.to_kwargs())

        if initial_partition is not None:
            with self._initial_partition(initial_partition):
                self._core.run(args)
        else:
            self._core.run(args)

        # Stamp a fresh Result with the new generation. The C++ result tree is
        # rebuilt on every run(), so any previously returned Result becomes
        # stale for node-level access (its eager scalars stay valid).
        self._generation += 1
        self._result = Result(self, generation=self._generation)
        return self._result
```

- [ ] **Step 4: Make the existing `run` body delegate to the impl**

Replace the BODY of the current `run` (everything after its docstring — the `options = ...` line through `return self._result`) with:

```python
        options = InfomapOptions.from_mapping(locals())
        return self._run_from_options(args, initial_partition, options)
```

Leave the `def run(self, ...):` signature and docstring untouched.

- [ ] **Step 5: Run the full suite + guards**

Run: `.venv/bin/python -m pytest test/python -q`
Expected: `286 passed` (+ Task 1's 4 = `290 passed`), `20 skipped`. In particular `test_public_surface`, `test_signature_catalog`, `test_wrapper_args`, and `_facade.py` doctests pass unchanged.

- [ ] **Step 6: Ruff**

Run: `.venv/bin/ruff check interfaces/python/src/infomap`
Expected: `All checks passed!`

- [ ] **Step 7: Commit**

```bash
git add interfaces/python/src/infomap/_facade.py
git commit -m "refactor(python): split __init__/run into impl methods"
```

---

## Task 3: Generate the `__init__`/`run` wrappers into a marker block

Extend the generator to emit the two thin wrappers and splice them into `_facade.py` between markers, then replace the hand-written wrappers with the generated block.

**Files:**
- Modify: `scripts/generate_binding_options.py`
- Modify: `interfaces/python/src/infomap/_facade.py` (replace hand-written `__init__`/`run` with marker block)
- Test: `test/python/test_facade_generated_block.py` (new)

- [ ] **Step 1: Add the curated order list + facade-render helpers to the generator**

In `scripts/generate_binding_options.py`, after the `*_OUT` path constants (after `TS_OUT = ...`), add:

```python
FACADE_OUT = Path("interfaces/python/src/infomap/_facade.py")

FACADE_BEGIN = (
    "    # === BEGIN generated: Infomap option signatures "
    "(scripts/generate_binding_options.py) ==="
)
FACADE_END = "    # === END generated ==="

# Hand-curated keyword order for the public Infomap.__init__ / Infomap.run
# signatures. This order is intentionally NOT the catalog order; it is the
# long-standing public API order and is part of the frozen public surface
# (test/python/test_public_surface.py pins the exact signature string).
# Catalog params supply each name's type, default, and docstring; this list
# only fixes their ORDER and where the facade-only `pretty` no-op sits.
# `_validate_facade_order` guards that this stays in sync with the catalog.
_FACADE_OPTION_ORDER = (
    "cluster_data", "no_infomap", "skip_adjust_bipartite_flow",
    "bipartite_teleportation", "weight_threshold", "include_self_links",
    "no_self_links", "node_limit", "matchable_multilayer_ids",
    "assign_to_neighbouring_module", "meta_data", "meta_data_rate",
    "meta_data_unweighted", "tree", "ftree", "clu", "verbosity_level",
    "silent", "pretty", "out_name", "no_file_output", "clu_level", "output",
    "hide_bipartite_nodes", "print_all_trials", "no_overwrite",
    "print_config_fingerprint", "timing_json", "summary_json", "manifest_json",
    "memory_report", "two_level", "flow_model", "directed",
    "recorded_teleportation", "use_node_weights_as_flow", "to_nodes",
    "teleportation_probability", "regularized", "regularization_strength",
    "entropy_corrected", "entropy_correction_strength", "markov_time",
    "variable_markov_time", "variable_markov_damping",
    "variable_markov_min_scale", "preferred_number_of_modules",
    "preferred_number_of_levels", "preferred_number_of_levels_strength",
    "multilayer_relax_rate", "multilayer_relax_limit",
    "multilayer_relax_limit_up", "multilayer_relax_limit_down",
    "multilayer_relax_by_jsd", "multilayer_relax_to_self", "seed",
    "num_trials", "core_loop_limit", "core_level_limit",
    "tune_iteration_limit", "core_loop_codelength_threshold",
    "tune_iteration_relative_threshold", "fast_hierarchical_solution",
    "prefer_modular_solution", "inner_parallelization", "parallel_trials",
    "converge", "num_threads", "threads", "trial_offset", "trial_results",
    "no_final_output", "num_random_moves", "max_degree_for_random_moves",
)

# Facade-only parameters that are NOT catalog options (rendered verbatim).
# `pretty` is a deprecated no-op kept for backward compatibility.
_FACADE_ONLY_PARAMS = {
    "pretty": {"default": "False", "doc_type": None, "doc": None},
}
```

- [ ] **Step 2: Add a validator and a per-parameter renderer**

Still in `generate_binding_options.py`, add module-level helper functions (place them just above `def generate_python`):

```python
def _validate_facade_order(catalog: ParameterCatalog) -> None:
    catalog_names = set()
    for group in GROUPS:
        if group == "Input":
            catalog_names.add("include_self_links")
        for param in catalog.grouped()[group]:
            catalog_names.add(param.name("python"))
    ordered = set(_FACADE_OPTION_ORDER) - set(_FACADE_ONLY_PARAMS)
    missing = catalog_names - ordered
    extra = ordered - catalog_names
    if missing or extra:
        raise RuntimeError(
            "scripts/generate_binding_options.py:_FACADE_OPTION_ORDER drifted "
            "from the parameter catalog. Add new params to the curated order "
            "(and decide their position), and remove deleted ones.\n"
            f"  in catalog, missing from order: {sorted(missing)}\n"
            f"  in order, not in catalog: {sorted(extra)}"
        )


def _facade_param_index(catalog: ParameterCatalog) -> dict:
    index = {}
    include_self_links = catalog.binding_only_entry("python", "include_self_links")
    for group in GROUPS:
        for param in catalog.grouped()[group]:
            index[param.name("python")] = {
                "type": param.python_type(),
                "default": param.python_default_expr(),
                "doc_type": param.python_doc_type(),
                "doc": param.python_doc_description(),
            }
    index["include_self_links"] = {
        "type": include_self_links.type,
        "default": include_self_links.default,
        "doc_type": "bool, optional",
        "doc": (
            "Deprecated. Self-links are included by default; use "
            "no_self_links=True to exclude them."
        ),
    }
    return index
```

Notes:
- `include_self_links.type` renders as `bool | None` and `.default` as `None` (verified: `_options.py` shows `include_self_links: bool | None = None`).
- The current facade renders `include_self_links=None` WITHOUT a type annotation. To stay byte-identical to the frozen public surface (which is computed from the *runtime* signature, where annotations don't appear in `str(inspect.signature(...))`), annotations are cosmetic for the gate. BUT the current source has bare `param=default` for every param (no annotations). MATCH that: render `name=default` (no `: type`) in the wrapper signature. Keep types only in the docstring. This is what Step 3 does.

- [ ] **Step 3: Add `generate_facade(catalog)` rendering the marker block**

Add this function (above `outputs`):

```python
def _render_facade_signature(names, index, indent="        "):
    lines = []
    for name in names:
        if name in _FACADE_ONLY_PARAMS:
            default = _FACADE_ONLY_PARAMS[name]["default"]
        else:
            default = index[name]["default"]
        lines.append(f"{indent}{name}={default},")
    return lines


def _render_facade_docstring_params(names, index):
    lines = []
    for name in names:
        if name in _FACADE_ONLY_PARAMS:
            continue
        info = index[name]
        lines.append(f"        {name} : {info['doc_type']}")
        lines.extend(wrap_doc(info["doc"], "            "))
    return lines


def generate_facade(catalog: ParameterCatalog) -> str:
    _validate_facade_order(catalog)
    index = _facade_param_index(catalog)
    names = list(_FACADE_OPTION_ORDER)

    lines = [FACADE_BEGIN]
    # ---- __init__ ----
    lines.append("    def __init__(")
    lines.append("        self,")
    lines.append("        args=None,")
    lines.extend(_render_facade_signature(names, index))
    lines.append("    ):")
    lines.append('        """Create a new Infomap instance.')
    lines.append("")
    lines.append("        Keyword arguments mirror the Infomap CLI flags. Use")
    lines.append(
        "        :class:`Settings` (alias :class:`InfomapOptions`) for a reusable"
    )
    lines.append("        configuration object and the full parameter reference.")
    lines.append("")
    lines.append("        Parameters")
    lines.append("        ----------")
    lines.append("        args : str, optional")
    lines.extend(
        wrap_doc(
            "Raw Infomap arguments to prepend before rendered keyword options.",
            "            ",
        )
    )
    lines.extend(_render_facade_docstring_params(names, index))
    lines.append('        """')
    lines.append("        options = InfomapOptions.from_mapping(locals())")
    lines.append("        self._init_from_options(args, options)")
    lines.append("")
    # ---- run ----
    lines.append("    def run(")
    lines.append("        self,")
    lines.append("        args=None,")
    lines.append("        initial_partition=None,")
    lines.extend(_render_facade_signature(names, index))
    lines.append("    ):")
    lines.append('        """Run Infomap.')
    lines.append("")
    lines.append("        Keyword arguments mirror the Infomap CLI flags. Use")
    lines.append(
        "        :class:`Settings` for the full parameter reference and"
    )
    lines.append(
        "        :meth:`run_with_options` when reusing a saved configuration."
    )
    lines.append("")
    lines.append("        Parameters")
    lines.append("        ----------")
    lines.append("        args : str, optional")
    lines.extend(
        wrap_doc(
            "Raw Infomap arguments to prepend before rendered keyword options.",
            "            ",
        )
    )
    lines.append("        initial_partition : dict, optional")
    lines.extend(
        wrap_doc(
            "Initial partition to use for this run only. See "
            "initial_partition.",
            "            ",
        )
    )
    lines.extend(_render_facade_docstring_params(names, index))
    lines.append("")
    lines.append("        See Also")
    lines.append("        --------")
    lines.append("        initial_partition")
    lines.append('        """')
    lines.append("        options = InfomapOptions.from_mapping(locals())")
    lines.append(
        "        return self._run_from_options(args, initial_partition, options)"
    )
    lines.append(FACADE_END)
    return "\n".join(lines)
```

- [ ] **Step 4: Add marker-splice write + check for the facade**

Add a helper near `write_outputs`:

```python
def _splice_marker_block(path: Path, block: str) -> str:
    text = path.read_text(encoding="utf-8")
    begin_idx = text.find(FACADE_BEGIN)
    end_idx = text.find(FACADE_END)
    if begin_idx == -1 or end_idx == -1:
        raise RuntimeError(
            f"Marker block not found in {path}. Expected lines:\n"
            f"  {FACADE_BEGIN}\n  {FACADE_END}"
        )
    end_idx += len(FACADE_END)
    return text[:begin_idx] + block + text[end_idx:]
```

Then change `outputs()`, `write_outputs()`, and `check_outputs()` so the facade is handled as a *marker splice*, not a whole-file overwrite. Concretely:

- In `outputs(infomap_bin)`, after building `catalog`, also compute `facade_block = generate_facade(catalog)` and return a SEPARATE structure. To keep the existing dict-of-whole-files contract for `_options.py`/R/TS, return a tuple `(generated, facade_block)`:

```python
def outputs(infomap_bin: Path):
    overrides = load_overrides()
    catalog = ParameterCatalog(load_parameters(infomap_bin), overrides)
    generated = {
        PYTHON_OUT: generate_python(catalog),
        R_OUT: generate_r(catalog),
        TS_OUT: generate_ts(catalog),
    }
    facade_block = generate_facade(catalog)
    return generated, facade_block
```

- Update `write_outputs` to also splice the facade:

```python
def write_outputs(generated, facade_block, output_root: Path) -> None:
    for rel_path, text in generated.items():
        path = output_root / rel_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
    facade_path = output_root / FACADE_OUT
    facade_path.write_text(
        _splice_marker_block(facade_path, facade_block), encoding="utf-8"
    )
```

- Update `check_outputs` to also verify the facade block is fresh:

```python
def check_outputs(generated, facade_block, output_root: Path) -> int:
    failures = []
    for rel_path, text in generated.items():
        path = output_root / rel_path
        if not path.exists() or path.read_text(encoding="utf-8") != text:
            failures.append(rel_path)
    facade_path = output_root / FACADE_OUT
    if not facade_path.exists():
        failures.append(FACADE_OUT)
    else:
        spliced = _splice_marker_block(facade_path, facade_block)
        if facade_path.read_text(encoding="utf-8") != spliced:
            failures.append(FACADE_OUT)
    if not failures:
        print("Tracked binding option outputs are fresh.")
        return 0
    if "GITHUB_ACTIONS" in __import__("os").environ:
        print(
            "::error title=Tracked binding option outputs are stale::"
            "Run make build-binding-options and commit the updated files."
        )
    print("Tracked binding option outputs are stale:")
    for path in failures:
        print(f"  {path}")
    print()
    print("Run: make build-binding-options")
    return 1
```

- Update `main()` to unpack the tuple:

```python
def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--infomap-bin", default="./Infomap")
    parser.add_argument("--output-root", default=".")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    generated, facade_block = outputs(Path(args.infomap_bin))
    output_root = Path(args.output_root)
    if args.check:
        return check_outputs(generated, facade_block, output_root)
    write_outputs(generated, facade_block, output_root)
    for path in generated:
        print(f"Updated {path}")
    print(f"Updated {FACADE_OUT} (generated signature block)")
    return 0
```

Note: this removed the `sys.platform and` truthiness guard around the GITHUB_ACTIONS check (it was always-true noise). `sys` is still imported and used by `raise SystemExit(main())`-adjacent code; leave the `import sys` in place (ruff would flag it only if fully unused — `check_outputs` no longer uses `sys`, but `main`/module may). VERIFY with ruff in Step 8; if `sys` becomes unused, remove the import.

- [ ] **Step 5: Insert the marker block into `_facade.py` (replace hand-written wrappers)**

In `interfaces/python/src/infomap/_facade.py`, DELETE the entire hand-written `def __init__(self, ...): ... self._init_from_options(args, options)` method AND the entire hand-written `def run(self, ...): ... return self._run_from_options(...)` method, replacing BOTH with a single placeholder marker block at the `__init__` location (the `run` method moves up into the block, which is fine — it stays inside the class):

```python
    # === BEGIN generated: Infomap option signatures (scripts/generate_binding_options.py) ===
    # Run `make build-binding-options` to regenerate this block.
    # === END generated ===
```

Important details:
- The `__getattr__`, `__repr__`, `summary`, `_repr_html_`, classmethods, etc. that currently sit BETWEEN `__init__` and `run` must stay where they are. So: replace ONLY the `__init__` def with the placeholder marker block (keep the intervening methods), and separately DELETE the `run` def (its logic now lives in `_run_from_options`, and the generated `run` is emitted inside the marker block at the `__init__` site).
- This means after Step 5 the class has BOTH `__init__` and `run` defined inside the single marker block (the generator emits both consecutively). That is intentional and matches `generate_facade` (it emits `__init__` then `run` back-to-back).
- The `_init_from_options`/`_run_from_options` impls (added in Task 2) stay OUTSIDE the marker block.

- [ ] **Step 6: Regenerate to populate the block**

Run: `cd /Users/anton/kod/infomap/.claude/worktrees/python-api-improvement && PATH="/opt/homebrew/bin:$PATH" make build-binding-options`
Expected: `Updated interfaces/python/src/infomap/_facade.py (generated signature block)`.

- [ ] **Step 7: Verify byte-identical signatures against the frozen baseline**

Run:
```bash
.venv/bin/python - <<'PY'
import inspect, json
from pathlib import Path
from infomap import Infomap
base = json.loads(Path("test/python/public_surface_baseline.json").read_text())
live_run = str(inspect.signature(Infomap.run))
live_init = str(inspect.signature(Infomap.__init__))
print("run matches baseline:", base["run"]["signature"] == live_run)
# __init__ is private; assert run+init order parity instead
init_no_self = list(inspect.signature(Infomap.__init__).parameters)[1:]
run_no_self = list(inspect.signature(Infomap.run).parameters)[1:]
print("init order == run order minus initial_partition:",
      init_no_self == [p for p in run_no_self if p != "initial_partition"])
PY
```
Expected: `run matches baseline: True` and the order-parity line `True`.

If `run matches baseline` is False, diff the live vs baseline string to find the first differing token, fix `_FACADE_OPTION_ORDER` / `_FACADE_ONLY_PARAMS` / default rendering, regenerate, re-check. Do NOT regenerate the baseline — the surface must stay frozen.

- [ ] **Step 8: Add a generated-block test**

Create `test/python/test_facade_generated_block.py`:

```python
"""Guard: the generated Infomap.__init__/run signature block stays present and
in sync with the parameter catalog. The byte-level freshness is enforced by
`scripts/generate_binding_options.py --check` (wired into
`make test-binding-options-freshness`); this test guards the runtime invariants.
"""

import inspect
from pathlib import Path

import pytest

from infomap import Infomap
from infomap._options import _OPTION_FIELD_NAMES

FACADE = Path(__file__).resolve().parents[2] / (
    "interfaces/python/src/infomap/_facade.py"
)
BEGIN = "# === BEGIN generated: Infomap option signatures"
END = "# === END generated ==="

NON_OPTION_INIT = {"self", "args", "pretty"}
NON_OPTION_RUN = {"self", "args", "initial_partition", "pretty"}


@pytest.mark.fast
def test_marker_block_present():
    text = FACADE.read_text(encoding="utf-8")
    assert BEGIN in text
    assert END in text
    assert text.index(BEGIN) < text.index(END)


@pytest.mark.fast
def test_init_options_match_catalog():
    params = set(inspect.signature(Infomap.__init__).parameters) - NON_OPTION_INIT
    assert params == set(_OPTION_FIELD_NAMES)


@pytest.mark.fast
def test_run_options_match_catalog():
    params = set(inspect.signature(Infomap.run).parameters) - NON_OPTION_RUN
    assert params == set(_OPTION_FIELD_NAMES)
```

- [ ] **Step 9: Run the full suite**

Run: `.venv/bin/python -m pytest test/python -q`
Expected: green, count = `286 + 4 (Task 1) + 3 (Task 3) = 293 passed`, `20 skipped`. Pay attention to `test_public_surface`, `test_signature_catalog`, `test_wrapper_args`, `test_parameter_catalog`, and `_facade.py` doctests.

- [ ] **Step 10: Freshness checks (the hard gate)**

Run:
```bash
cd /Users/anton/kod/infomap/.claude/worktrees/python-api-improvement
PATH="/opt/homebrew/bin:$PATH" make test-binding-options-freshness
PATH="/opt/homebrew/bin:$PATH" make test-python-swig-freshness
```
Expected: both print fresh/clean and exit 0. (`test-binding-options-freshness` now also covers the `_facade.py` block.)

- [ ] **Step 11: Idempotency check**

Run `make build-binding-options` a SECOND time, then `git diff --stat`.
Expected: no changes (generation is idempotent). If the facade block churns on re-run, the splice or rendering is non-deterministic — fix before committing.

- [ ] **Step 12: Ruff**

Run: `.venv/bin/ruff check interfaces/python/src/infomap test/python scripts`
Expected: `All checks passed!` (If `import sys` in the generator is now unused, remove it.)

- [ ] **Step 13: Commit (generator + facade together — they must move in lockstep)**

```bash
git add scripts/generate_binding_options.py interfaces/python/src/infomap/_facade.py test/python/test_facade_generated_block.py
git commit -m "feat(python): generate Infomap.__init__/run signatures from catalog"
```

---

## Task 4: Confirm the signature↔catalog invariant stays enforced; doc note

`test_signature_catalog.py` now passes trivially (the wrappers are generated FROM the catalog). Keep it as a second, independent guard and add a one-line note. Update `PYTHON_API_2.0_DESIGN.md` §3.3/§7 only if it claims the signatures are hand-maintained.

**Files:**
- Modify: `test/python/test_signature_catalog.py` (docstring note only)
- Modify (if needed): `interfaces/python/PYTHON_API_2.0_DESIGN.md`

- [ ] **Step 1: Add a note to `test_signature_catalog.py`**

Append to the module docstring (after the existing text):

```python
"""... (existing text) ...

Phase 3: the __init__/run signatures are now GENERATED into a marker block in
_facade.py from this same catalog (scripts/generate_binding_options.py), so this
guard is belt-and-suspenders: byte-level freshness is enforced by
``make test-binding-options-freshness``; this test enforces the runtime
set-equality invariant independently of the generator.
"""
```

(Adjust to merge cleanly with the existing docstring rather than duplicating the `"""`.)

- [ ] **Step 2: Check the design doc for stale claims**

Run: `grep -n "hand-listed\|hand-maintained\|~80\|two hand" interfaces/python/PYTHON_API_2.0_DESIGN.md`
If any line claims the signatures are hand-maintained, update it to say they are generated into a marker block in `_facade.py` by `scripts/generate_binding_options.py`. If no such claim exists, skip the edit.

- [ ] **Step 3: Full suite + ruff (final regression)**

Run:
```bash
.venv/bin/python -m pytest test/python -q
.venv/bin/ruff check interfaces/python/src/infomap test/python scripts
```
Expected: `293 passed, 20 skipped`; ruff clean.

- [ ] **Step 4: Commit**

```bash
git add test/python/test_signature_catalog.py interfaces/python/PYTHON_API_2.0_DESIGN.md
git commit -m "docs(python): note generated signatures in catalog guards"
```

(If the design doc needed no edit, only stage the test file.)

---

## Task 5: Final verification gate (no commit — evidence for the report)

- [ ] **Step 1: Clean regenerate from scratch**

```bash
cd /Users/anton/kod/infomap/.claude/worktrees/python-api-improvement
PATH="/opt/homebrew/bin:$PATH" make build-binding-options
git diff --stat   # expect: no changes (everything already committed & fresh)
```

- [ ] **Step 2: All gates**

```bash
PATH="/opt/homebrew/bin:$PATH" make test-binding-options-freshness   # clean
PATH="/opt/homebrew/bin:$PATH" make test-python-swig-freshness       # clean
.venv/bin/python -m pytest test/python -q                            # >=293 passed
.venv/bin/ruff check interfaces/python/src/infomap test/python scripts
```

- [ ] **Step 3: Public surface unchanged + Settings additive**

```bash
.venv/bin/python -m pytest test/python/test_public_surface.py -q
.venv/bin/python -c "import infomap; print('Settings' in infomap.__all__)"   # True
```

- [ ] **Step 4: Capture the generated block snippet for the report**

```bash
sed -n '/BEGIN generated/,/def __init__/p' interfaces/python/src/infomap/_facade.py | head -20
git log --oneline -5
```

---

## Self-Review

- **Spec coverage:**
  - Generate `__init__`/`run` into a marker block — Task 3.
  - Wrapper+impl split — Task 2 (impls) + Task 3 (generated wrappers).
  - `--check` for the facade block, wired into `build-binding-options`/freshness — Task 3 Step 4 + Step 10 (`test-binding-options-freshness` already calls `--check`).
  - `Settings` frozen alias, exported, back-compat `InfomapOptions` — Task 1.
  - `test_signature_catalog` kept + invariant enforced — Task 3 Step 8 (new test) + Task 4.
  - Byte-identical call sites / frozen public surface — Task 3 Step 7 + Step 11.
  - SWIG untouched — verified by `test-python-swig-freshness` (Task 3 Step 10).
- **Placeholder scan:** none — every code step shows the exact code; the curated order list is fully spelled out.
- **Type/name consistency:** `_init_from_options(self, args, options)` and `_run_from_options(self, args, initial_partition, options)` are defined in Task 2 and called by the generated wrappers in Task 3 with the same names. `generate_facade` / `_splice_marker_block` / `outputs` tuple-return are consistent across Steps 3–4. `Settings`/`InfomapOptions`/`_OPTION_FIELD_NAMES` names match `_options.py`.

## Risks / STOP conditions

- If `make build-binding-options` cannot build the native `./Infomap` for an unrelated reason, STOP and report (the generator needs `--print-json-parameters`).
- If the generated `run` signature does not match `public_surface_baseline.json` byte-for-byte after tuning the order list, STOP rather than regenerating the baseline — the frozen surface is a hard gate.
- If freezing `InfomapOptions` surfaces a hidden mutation (no such site found during planning), STOP and report the offending call site instead of un-freezing.
