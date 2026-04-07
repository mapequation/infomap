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

## Verification

Run the smallest sufficient verification for the changed surface:

- `src/` changes: at least `make build-native`
- Python wrapper or packaging changes: `make build-python` and `make test-python`
- JavaScript worker or package changes: `npm ci` plus `make build-js` or `make test-js`
- docs-only text changes: no code build unless docs output freshness is affected; then run `make test-docs`
- workflow or release changes: run the smallest relevant local smoke check and say what remains unverified

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

## Escalation

Stop and hand off when:

- the work spans multiple major surfaces
- the verification path is unavailable in the current environment
- the fix appears to require architectural redesign instead of a bounded patch
- the behavior change cannot be validated with a small scoped check
- the issue points to algorithm correctness, cross-platform divergence, or memory behavior
