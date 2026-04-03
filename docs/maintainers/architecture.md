# Architecture

This document defines ownership boundaries for Infomap's supported surfaces.
It is intended for maintainers who need to understand which files are sources
of truth, which outputs are generated, and which products are public versus
internal-supported.

## Primary runtime authority

`src/` is the only implementation authority for Infomap runtime behavior:

- algorithms and optimization behavior
- parsing and file IO semantics
- CLI option semantics and output formats
- metadata exposed to wrappers and tests

The CLI binary, Python wrapper, JavaScript worker, C++ test suite, and Docker
images are all projections of this C++ core. They should not redefine runtime
behavior independently.

## Supported surfaces

Public-supported surfaces:

- CLI binary (`Infomap`)
- Python package (`infomap`)
- JavaScript worker package (`@mapequation/infomap`)
- published Python documentation site

Internal-supported surfaces:

- React helpers (`@mapequation/infomap-react`)
- parser helpers (`@mapequation/infomap-parser`)
- notebook image and Python-oriented Docker images

Internal-supported means the surface still matters and should keep building, but
it is not a primary public contract for release planning.

## Source-of-truth rules

Use these ownership rules when making changes:

- `src/`
  - runtime semantics, option behavior, output semantics, and C++ APIs
- `interfaces/python/source/`
  - documentation source for the published Python docs
- `docs/`
  - committed generated Sphinx output; do not hand-edit generated HTML or JS
- `interfaces/js/generated/`
  - generated metadata consumed by the JS package build
- `test/fixtures/`
  - shared regression inputs and expected outputs for tests

Generated outputs should have an explicit generator and verification path. They
should not be treated as hand-maintained source files.

## Build ownership

The repository intentionally uses more than one tool, but their roles are
distinct:

- `make`
  - primary maintainer UX for native builds, wrapper builds, packaging, and
    common verification
- `cmake`
  - C++ test and instrumentation build path
- `swig`
  - Python wrapper generation layer
- `webpack` and `em++`
  - JavaScript worker packaging layer

If a change touches multiple build surfaces, keep the configuration policy
consistent across them rather than re-encoding the same logic separately.

## Documentation and release expectations

- `README.rst` is user-facing source content and feeds the committed docs site
- `README.md` is the npm package README for the public JavaScript package
- `BUILD.md` describes local maintainer build/test workflows
- `RELEASING.md` describes maintainer release policy

When these documents disagree, fix the source document rather than patching
generated output by hand.

## Docker support policy

Docker remains supported, but not every image has the same role:

- Supported:
  - CLI image
  - notebook image
- Internal-supported:
  - Python build/test image
  - compatibility or investigation images

Supported images should stay aligned with current Make-based build workflows and
should have at least smoke coverage in CI over time.
