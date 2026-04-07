# Architecture

This document describes the ownership boundaries for Infomap's maintained
surfaces. Use it to answer three questions quickly:

1. Which file is the source of truth?
2. Which outputs are generated?
3. Which surfaces are public versus internal-supported?

## Runtime authority

`src/` is the only authority for Infomap runtime behavior:

- algorithm and optimization behavior
- parsing and file IO semantics
- CLI option semantics and output formats
- metadata exposed to wrappers and tests

The CLI, Python package, JavaScript worker, tests, and Docker images are all
projections of this core. They should not redefine runtime behavior
independently.

## Supported surfaces

Public-supported:

- CLI binary (`Infomap`)
- Python package (`infomap`)
- JavaScript worker package (`@mapequation/infomap`)
- React hook subpath (`@mapequation/infomap/react`)
- published Python documentation site

Internal-supported:

- secondary Docker images used for compatibility or maintainer workflows

Internal-supported means the surface still matters and should keep building, but
it is not a primary public contract for release planning.

## Source-of-truth rules

- `src/`
  - runtime semantics, option behavior, output semantics, and C++ APIs
- `README.rst`
  - repository landing-page text and the landing-page content reused in the
    generated Python docs
- `interfaces/python/source/`
  - Python docs source beyond the landing page
- `interfaces/js/README.md`
  - README source for the public npm package
- `interfaces/js/generated/`
  - tracked metadata consumed by the JS package build
- `ARCHITECTURE.md`, `BUILD.md`, `RELEASING.md`, and `AGENTS.md`
  - hand-maintained maintainer documentation and repo working rules
- `docs/`
  - generated Python docs output for GitHub Pages
- `test/fixtures/`
  - shared regression inputs and expected outputs for tests

Generated outputs should always have a clear generator and verification path.
They are not hand-maintained source files.

For the Python docs site:

- `make build-docs` refreshes the committed generated output in `docs/`
- `make test-docs` rebuilds in a temp directory and compares that output with
  the committed generated subset of `docs/`

Do not hand-edit generated HTML, JavaScript, or `_sources` files under `docs/`.

## Build ownership

The repository intentionally uses several tools, each with a narrow role:

- `make`
  - primary maintainer entry point for builds, packaging, and common verification
- `cmake`
  - C++ tests and instrumentation builds
- `swig`
  - Python wrapper generation layer
- `rollup` and `em++`
  - JavaScript worker packaging layer

If a change touches multiple build surfaces, keep the policy aligned across
them instead of re-encoding the same logic separately.

## Documentation expectations

- `README.rst` is the main general-purpose README in this repository
- `interfaces/js/README.md` is the npm-package README
- `BUILD.md` describes local maintainer build and verification commands
- `RELEASING.md` describes the release flow
- `AGENTS.md` captures repo-local working rules for small, safe changes

When documents disagree, update the owning source rather than the generated
output.

## Docker support policy

Docker remains supported, but not every image has the same role.

Supported images:

- CLI image (`docker/infomap.Dockerfile`)
- notebook image (`docker/notebook.Dockerfile`)

Internal-supported images:

- Python build/test image (`docker/python.Dockerfile`)
- Ubuntu compatibility image (`docker/ubuntu.Dockerfile`)
- RStudio image (`docker/rstudio.Dockerfile`)

Supported images should stay aligned with the current Make-based workflows and
retain smoke coverage in CI.
