# AGENTS.md

## Mission

Keep changes small, source-aware, and easy to verify.

Default priorities:

1. identify the smallest affected surface
2. edit the real source-of-truth file
3. run the smallest useful verification
4. state clearly what was verified and what was not

Do not bundle unrelated cleanup into the same change.

## Repo Map

- `src/`: C++ runtime behavior, algorithms, parsing, IO, and CLI semantics
- `interfaces/python/`: Python package sources, tracked SWIG outputs, and Sphinx source
- `interfaces/R/`: R package skeleton (`infomap/`) and tracked SWIG-generated R outputs (`generated/`)
- `interfaces/js/`: JavaScript package sources, legacy parser package, and TypeScript sources
- `interfaces/swig/`: SWIG interface files shared by Python and R bindings
- `test/`: Python-facing regression tests and fixtures
- `examples/python/`: executable Python examples used by `make test-python`
- `examples/R/`: executable R examples used by `make test-r-examples`
- `docs/`: committed generated Python docs output plus hand-maintained maintainer docs
- `.github/workflows/`: CI, docs verification, release, and packaging workflows

## Source Of Truth

- `src/` owns runtime behavior
- `README.rst` is the main repository and Python-docs landing-page source
- `interfaces/js/README.md` is the source README for the public npm package
- `interfaces/python/source/` owns the published Python docs source
- `interfaces/python/generated/` and `interfaces/python/src/infomap/_swig.py` are tracked Python wrapper outputs
- `interfaces/R/infomap/` owns the R package skeleton (`R/`, `DESCRIPTION`, `tests/`, `man/`)
- `interfaces/R/generated/` are tracked SWIG-generated R outputs; refresh with `make build-r-swig`
- `docs/` generated HTML, JS, and `_sources` files are build output; do not hand-edit them

When two documents disagree, fix the source document and regenerate derived
output instead of patching the generated copy by hand.

## Bootstrap From Clean Clone

Start from a task branch, not `master`:

```bash
git clone https://github.com/mapequation/infomap.git
cd infomap
git switch -c fix/<short-task-name>
```

Baseline tools:

- Python 3.11 or newer; use `python3` on macOS when `python` is unavailable
- R 4.0 or newer for the R package; CI exercises both `release` and `oldrel`
- Node.js 20 for the JavaScript worker package
- `gcc` or `clang` with a working C++ toolchain
- `swig` 4.4.1 only for refreshing tracked Python or R wrapper outputs
- `em++` from Emscripten 5.0.5 for JavaScript worker builds

Common local setup:

```bash
python3 -m pip install -e '.[test,docs,examples,release]'
npm ci
make doctor
make build-native
```

On macOS with Homebrew:

```bash
brew install libomp swig
PATH="/opt/homebrew/bin:$PATH" make doctor
```

For Python wrapper refreshes, use SWIG 4.4.1 to match CI. For JavaScript
worker work, activate Emscripten 5.0.5 before running `make build-js` or
`make test-js`.

## Verification

Run the smallest sufficient verification for the changed surface:

- `src/` changes: at least `make build-native`
- Python wrapper or packaging changes: `make build-python` and `make test-python`
- R wrapper or packaging changes: `make test-r` (R CMD check) plus
  `make test-r-examples`. Refresh tracked SWIG outputs with
  `make build-r-swig` and verify with `make test-r-swig-freshness`.
  Regenerate Rd/NAMESPACE with
  `Rscript -e 'roxygen2::roxygenise("interfaces/R/infomap")'`
  after installing with `R CMD INSTALL --with-keep.source`.
- JavaScript worker or package changes: `npm ci` plus `make build-js` or `make test-js`
- docs-only text changes: no code build unless docs output freshness is affected; then run `make test-docs`
- workflow or release changes: run the smallest relevant local smoke check and say what remains unverified

Approximate local runtimes vary by machine and cache state:

- `make build-native`: about 1-2 minutes on a clean checkout
- `make build-python`: about 2-4 minutes after Python dependencies are installed
- `make test-python`: about 3-6 minutes after the package is built and installed
- `make test-r`: about 1-3 minutes after R dependencies are installed
- `make build-js`: about 2-4 minutes with `em++` already on `PATH`
- `make test-js`: about 3-6 minutes after JavaScript dependencies and browsers are installed
- `make test-docs`: about 1-3 minutes after Python docs dependencies are installed

Targeted checks:

- Single Python test: `make test-python-unit PYTEST_ARGS='test/python/test_x.py::test_y'`
- Python marker subset: `make test-python-unit PYTEST_ARGS='-m "not slow and not perf"'`
- Single JavaScript unit test: `npm run test:unit -- -t "name pattern"`
- JavaScript typecheck only: `npm run typecheck`

## Environment

- Verify tool availability before use: `python`, `node`, `swig`, `em++`, and the compiler toolchain
- On some local macOS setups, Homebrew tools may need `PATH="/opt/homebrew/bin:$PATH"`
- Never develop directly on `master`; create or use a task-specific branch

## Do Not Guess

- Do not improvise around SWIG generation, Python packaging, or R packaging
- Do not improvise around JS worker generation or Emscripten
- Do not hand-edit generated docs output in `docs/`
- Do not hand-edit `interfaces/R/generated/` or `interfaces/R/infomap/man/`;
  regenerate with `make build-r-swig` and `roxygen2::roxygenise(...)`
  respectively
- Do not make release or publishing changes casually
- Do not treat algorithmic, numerical, determinism, or memory issues as routine cleanup

## Common Pitfalls

- macOS OpenMP: Homebrew `libomp` may be installed but not found by the
  compiler. Use `PATH="/opt/homebrew/bin:$PATH"` and the `CXXFLAGS`/`LDFLAGS`
  recipe in `BUILD.md`, or disable OpenMP with `OPENMP=0` for local smoke
  builds.
- Stale Python extension: after changing C++ extension sources, SWIG
  interfaces, or tracked wrapper outputs, rerun `make build-python` and
  `make dev-python-install` before Python tests.
- SWIG drift: only refresh tracked Python or R wrapper outputs with SWIG
  4.4.1, then run the matching freshness check
  (`make test-python-swig-freshness` or `make test-r-swig-freshness`).
- R 4.6 SWIG patch: `scripts/generate_r_swig.py` post-processes the SWIG
  output to replace `SET_S4_OBJECT` and read-only `CHARACTER_POINTER`
  assignments removed in R 4.6, and adds `R_useDynamicSymbols(dll, FALSE)`
  to `R_init_infomap`. Tagged for removal once SWIG > 4.4.1 emits R
  4.6-clean code (upstream PR `swig/swig#3411`).
- R6 method discovery: roxygen2 only documents R6 methods defined inside
  the `R6::R6Class()` body. Adding methods later via
  `InfomapClass$set("public", ...)` produces working code that does not
  appear in `?InfomapClass`. Define methods inline.
- macOS R workaround: `mk/r.mk` writes a temporary Makevars pinning
  `/usr/bin/clang++` so the compiled R `.so` is libc++-compatible with
  Homebrew R. `make doctor` flags missing `/usr/bin/clang++`.
- Emscripten environment: `make build-js` and `make test-js` require `em++` from
  Emscripten 5.0.5 on `PATH`; `npm ci` alone is not enough.

## Repository Settings

Private vulnerability reporting, branch protection, required checks, stale
approval dismissal, and linear-history requirements are GitHub repository
settings. They cannot be verified from tracked files alone; note any manual
settings checks in the pull request when they affect the change.

## Escalation

Stop and hand off when:

- the work spans multiple major surfaces
- the verification path is unavailable in the current environment
- the fix appears to require architectural redesign instead of a bounded patch
- the behavior change cannot be validated with a small scoped check
- the issue points to algorithm correctness, cross-platform divergence, or memory behavior
