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
- `interfaces/js/`: JavaScript package sources, legacy parser package, and TypeScript sources
- `test/`: Python-facing regression tests and fixtures
- `examples/python/`: executable Python examples used by `make test-python`
- `docs/`: committed generated Python docs output plus hand-maintained maintainer docs
- `.github/workflows/`: CI, docs verification, release, and packaging workflows

## Source Of Truth

- `src/` owns runtime behavior
- `README.rst` is the main repository and Python-docs landing-page source
- `interfaces/js/README.md` is the source README for the public npm package
- `interfaces/python/source/` owns the published Python docs source
- `interfaces/python/generated/` and `interfaces/python/src/infomap/_swig.py` are tracked Python wrapper outputs
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
- Node.js 20 for the JavaScript worker package
- `gcc` or `clang` with a working C++ toolchain
- `swig` only for refreshing tracked Python wrapper outputs
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
- JavaScript worker or package changes: `npm ci` plus `make build-js` or `make test-js`
- docs-only text changes: no code build unless docs output freshness is affected; then run `make test-docs`
- workflow or release changes: run the smallest relevant local smoke check and say what remains unverified

Approximate local runtimes vary by machine and cache state:

- `make build-native`: about 1-2 minutes on a clean checkout
- `make build-python`: about 2-4 minutes after Python dependencies are installed
- `make test-python`: about 3-6 minutes after the package is built and installed
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

- Do not improvise around SWIG generation or Python packaging
- Do not improvise around JS worker generation or Emscripten
- Do not hand-edit generated docs output in `docs/`
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
- SWIG drift: only refresh tracked Python wrapper outputs with SWIG 4.4.1, then
  run `make test-python-swig-freshness`.
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
