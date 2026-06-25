# Phase 5b — io/ + tl/ reorg, remove package-level SWIG re-exports, strict import guard

Branch: `python-api-core-boundary` (Phases 1–5 done, green: 326 tests, 0 failures, 20 skipped).
Pure Python; no recompile/regen. Verify ONLY via JUnit XML.

Worktree (all ops rooted here):
`/Users/anton/kod/infomap/.claude/worktrees/python-api-improvement`

## Verification commands
- Suite: `.venv/bin/python -m pytest test/python -q --junitxml=/tmp/p5b.xml` then `grep -oE '<testsuite [^>]*>' /tmp/p5b.xml`
- Lint: `ruff check interfaces/python/src/infomap test/python`
- Freshness: `make test-python-swig-freshness && make test-binding-options-freshness`
- Surface: confirm `git status` shows no diff to `test/python/public_surface_baseline.json`

## Task A — remove package-level SWIG re-exports + strict guard (commit A)
1. `interfaces/python/src/infomap/_facade.py`: delete `from ._bindings import *`,
   `from ._bindings import __all__ as _BINDINGS_ALL`, and `*_BINDINGS_ALL,` from `__all__`.
   (Grounded: `_facade.py` uses no star-imported SWIG name; `MultilayerNode` stays local.)
2. Fix consumers of removed package-level names:
   - `test/python/test_wrapper_args.py:67` `infomap_module.InfomapWrapper` → import `from infomap._bindings import InfomapWrapper`.
   - `test/python/test_api.py:312` same pattern. (Both monkeypatch the class object; same object regardless of import path.)
   - Grep confirmed no other `infomap.<SwigName>` / `from infomap import <SwigName>` consumers.
3. Extend `test/python/test_no_swig_leak.py`: scan every `*.py` under
   `interfaces/python/src/infomap/` and assert ONLY `_core.py` and `_bindings.py` import
   `_swig`/`_bindings`. Also assert `not hasattr(infomap, "InfomapWrapper")`.
4. Gate: suite green, ruff clean. Commit A.

## Task B — move writers + graph adapters into io/ (commit B)
Create `interfaces/python/src/infomap/io/__init__.py`. `git mv`:
- `_writers.py` → `io/writers.py`
- `_networkx.py` → `io/networkx.py`
- `_igraph.py` → `io/igraph.py`
- `_scipy.py` → `io/scipy.py`
- `_edge_index.py` → `io/edge_index.py`
- `export.py` → `io/export.py`

Fixups:
- `_facade.py` imports → `from .io.writers ...`, `from .io.networkx ...`, etc.
- Inside moved modules, relative `from ._facade import Infomap` → `from .._facade import Infomap`
  (`networkx.py`, `igraph.py`, `tl` unaffected here). `export.py` uses no internal import.
  Lazy/optional imports stay lazy (inside functions / TYPE_CHECKING).
- Back-compat shim: new top-level `interfaces/python/src/infomap/export.py` re-exporting from
  `infomap.io.export` with a deprecation note (test_export.py imports `from infomap.export import ...`).
- Surface-lock: in `test/python/test_public_surface.py` `OWNED_MODULES`, replace `infomap._writers`
  with `infomap.io.writers`. (NOTE: stale `_network_builder`/`_multilayer_builder` already absent;
  current set is `_facade, _results, _writers, network, _summary`.) Baseline JSON must NOT change.
- Gate: suite green, ruff clean, `test_public_surface` passes with baseline JSON unchanged
  (confirm `git status` clean for the baseline). Commit B.

## Task C — move tl + graphrag into tl/ (commit C)
Create `interfaces/python/src/infomap/tl/` package. `git mv`:
- `tl.py` → `tl/__init__.py` (preserves `infomap.tl.infomap(...)`)
- `graphrag.py` → `tl/graphrag.py`

Fixups:
- `tl/__init__.py` line 130: `from ._facade import Infomap` → `from .._facade import Infomap`.
- `tl/graphrag.py` line 589: `from infomap import Infomap` (absolute) → unchanged.
- Shim: new top-level `interfaces/python/src/infomap/graphrag.py` re-exporting
  `read_graphrag, write_graphrag_communities, run_graphrag_communities, _output_paths`
  (and `__all__`) from `infomap.tl.graphrag`. `test_graphrag.py` imports these (incl private).
- `merge.py`, `shell.py` stay (`python -m infomap.merge` / `.shell`).
- `__init__.py` `from . import tl as tl` keeps working (now a package).
- Gate: suite green (watch test_anndata, test_graphrag), ruff clean. Commit C.

## Hard gates (all must pass, show output)
- JUnit: `failures="0" errors="0"`, tests ≥ 326 (+ new guard assertions).
- ruff clean.
- `make test-python-swig-freshness` + `make test-binding-options-freshness` clean.
- `test_public_surface` passes, baseline JSON unchanged.
- Main checkout `/Users/anton/kod/infomap` untouched (only 3 untracked PYTHON_API_*.md docs).

## Out of scope (Phase 6)
Conversion API surface redesign (canonical `infomap.to_networkx(...)`). 5b only MOVES export.py.
