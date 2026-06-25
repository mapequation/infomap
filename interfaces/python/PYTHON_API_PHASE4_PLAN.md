# Python API Phase 4 — first-class `Network`

Status as of start: Phases 1–3 DONE and green on branch `python-api-core-boundary`
(313 tests, 0 failures / 0 errors / 20 skipped). `Infomap` composes a `Core`;
`run()` returns an immutable `Result`; signatures generated from the catalog;
`Settings` frozen.

## Goal (locked, non-breaking structural extraction)

Unify the two internal builders (`_NetworkBuilder` + `_MultilayerBuilder`) into
one **public** `infomap.Network` class that holds a `Core` and exposes the core
building verbs. `Infomap` composes a `Network` over its own settings-configured
`Core` (shared instance) and delegates its building verbs to it. The existing
`im.network` SWIG property and the pinned `Infomap` public surface stay
byte-identical.

Design ref: `PYTHON_API_2.0_DESIGN.md` §3.6 (`Network` — the input builder,
first-class). DEFER to Phase 6: graph-library adapters, `from_*`, functional
`run(net)` / `Infomap(net)`, and running a standalone `Network`. A standalone
`Network()` is a builder only for now.

## What `Network` exposes

Constructor: `Network(core=None)` — if `core` is None create
`Core("--silent --no-file-output")`; when given, wrap the shared one.

Mutators (return `self`, fluent):
`add_node`/`add_nodes`, `add_state_node`/`add_state_nodes`,
`set_name`/`set_names`, `add_link`/`add_links`, `remove_link`/`remove_links`,
`add_multilayer_link`/`add_multilayer_links`,
`add_multilayer_intra_link`/`add_multilayer_intra_links`,
`add_multilayer_inter_link`/`add_multilayer_inter_links`,
`remove_multilayer_link`, `set_meta_data`, `read_file`.

Properties: `bipartite_start_id` (get/set), `num_nodes`, `num_links`.

Internals reused unchanged: `_network_input.add_bulk_links` and the unpackers
(`first_order_unpacker`, `flat_multilayer_unpacker`, `paired_multilayer_unpacker`).
Bulk routing must stay: list -> `self._core.addLinks`, numpy ->
`self._core.addLinksFromNumpy2D` (and the multilayer equivalents), so the
`test_bulk_construction_routing` spy (which patches `InfomapWrapper`) still passes.

`num_nodes`/`num_links` read `self._core.network().numNodes()/numLinks()` (same
source as the `Infomap` result-mixin properties, which read via `im.network`).

## Steps (bite-sized, commit per logical step, verify each)

### Step 1 — create `network.py` with the unified `Network` class
- New `interfaces/python/src/infomap/network.py` containing `Network`.
- Mutators return `self`. Single-item methods reuse the existing builder logic
  verbatim (only difference vs builders: return `self` not the raw C++ return).
- Export `Network` from `_facade.__all__` so it reaches `infomap.Network` via the
  package re-export (`__init__` already does `from ._facade import *`). Import
  `Network` into `_facade` for that re-export.
- Verify: `ruff check` clean on the new file; `python -c "from infomap import Network"`.
- Commit: `feat(python): add first-class Network builder unifying the two internal builders`.

### Step 2 — compose `Network` in `Infomap`, delegate building verbs
- In `_init_from_options`, replace
  `self._network = _NetworkBuilder(self._core)` + `self._multilayer =
  _MultilayerBuilder(self._core)` with `self._network = Network(core=self._core)`.
- Re-route the multilayer facade verbs from `self._multilayer.X` to
  `self._network.X` (the unified class now owns them). Single-layer verbs already
  call `self._network.X` — unchanged.
- Keep `im.network` property, `num_nodes`/`num_links` (result mixin), and all
  docstrings/signatures untouched -> public surface byte-identical.
- Verify: `test_public_surface` passes (byte-identical), `test_collaborators`,
  `test_bulk_construction_routing`, full suite green.
- Commit: `refactor(python): Infomap composes the unified Network and delegates building verbs`.

### Step 3 — retire the two old builder modules
- Delete `_network_builder.py` and `_multilayer_builder.py` (logic fully moved).
- Update `test_collaborators.py` (imports `_NetworkBuilder`/`_MultilayerBuilder`
  and asserts `isinstance(im._network, _NetworkBuilder)` /
  `isinstance(im._multilayer, _MultilayerBuilder)`): assert against `Network`,
  drop the `_multilayer` attribute assertion (now unified into `_network`).
- Update `OWNED_MODULES` in `test_public_surface.py`: replace the two retired
  module names with `infomap.network` (so any `Network`-defined methods that end
  up on the `Infomap` surface stay signature-pinned — though delegation means the
  surface members remain defined in `_facade`, so practically the baseline is
  unaffected; the swap keeps the owned-module set accurate). Re-run
  `test_public_surface`; the baseline must NOT change.
- Verify: `ruff check`, full suite green, freshness checks clean.
- Commit: `refactor(python): retire _network_builder/_multilayer_builder, unified into Network`.

### Step 4 — add `test/python/test_network.py`
- (a) Build a graph via a standalone `Network`, build the same via `Infomap`'s
  delegated verbs, run both, assert codelength + module counts match.
  (Standalone `Network` is a builder only; to run it, hand its `Core` to an
  `Infomap`-equivalent run path — but Phase 4 keeps run on `Infomap`. So: build
  the standalone `Network` with its own default `Core`, then drive the run
  through that same `Core`. If a standalone `Network` cannot be run without the
  deferred functional entry, the parity test instead constructs an `Infomap`,
  takes `im._network` (a `Network`) and a second standalone `Network` wrapping a
  fresh `Infomap`'s core, builds identically on both, and asserts equal results
  via each owning `Infomap`. STOP + report if neither is expressible without
  exceeding scope.)
- (b) `Network` mutators return `self` (chaining).
- (c) `num_nodes`/`num_links` correct after building.
- Verify: new tests pass; full suite >= 313 (+ new tests); ruff clean.
- Commit: `test(python): add test_network.py covering standalone/delegated parity, fluent, counts`.

## Hard gates (all before final report)
- Full suite green via JUnit XML: `failures="0" errors="0"`, tests >= 313 + new.
- `ruff check interfaces/python/src/infomap test/python` clean.
- `make test-binding-options-freshness` + `make test-python-swig-freshness` clean
  (no generated/SWIG/binding-options outputs touched).
- `test_public_surface` passes; `Infomap` surface byte-identical; `im.network`
  untouched. Baseline regenerated ONLY if the change is provably additive +
  intended (expectation: no regeneration needed).

## Risk / STOP conditions
- If composing `Network` over the shared `Core` changes `num_nodes`/`num_links`
  or any pinned signature -> STOP + report.
- If standalone-`Network` parity (Step 4a) cannot be tested without the deferred
  functional run entry -> use the in-`Infomap` parity variant; if even that
  requires scope beyond the lock -> STOP + report.
