# Building

## Commit style

This repository uses [Conventional Commits](https://www.conventionalcommits.org).
Release automation relies on commit types such as `fix`, `feat`, `docs`, `build`
and `test`, with optional scopes like `js` and `python`.

Examples:

- `fix: Off by one error in output`
- `fix(python): Correct iterator materialization`
- `docs: Update macOS build prerequisites`

## Release process

Maintainer release flow now lives in [RELEASING.md](RELEASING.md).
Maintainer architecture and ownership rules live in
[docs/maintainers/architecture.md](docs/maintainers/architecture.md).
This document only covers local build and verification.

## Native build

Compile the C++ binary from the repository root:

```bash
make build-native
```

Builds use all detected logical cores by default. Use `JOBS=1` to back off to
a single-job build when debugging or reducing local machine load:

```bash
make build-native JOBS=1
```

On macOS with Homebrew `libomp`, use:

```bash
PATH="/opt/homebrew/bin:$PATH" \
CXXFLAGS="-I/opt/homebrew/opt/libomp/include" \
LDFLAGS="-L/opt/homebrew/opt/libomp/lib" \
make build-native
```

Use `make help` to see the grouped target surface, and `make doctor` to inspect
tool availability plus the active build flags on the current machine.

## JavaScript package

Building requires [Emscripten](https://emscripten.org/docs/getting_started/downloads.html).

```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
cd /path/to/infomap
npm ci
make build-js-metadata
make build-js
make test-js-metadata
make test-js
```

`make build-js-metadata` refreshes the tracked parameter and changelog metadata
under `interfaces/js/generated/`.
`make build-js` builds the worker and bundles the public npm package from those
tracked metadata files into `dist/npm/package/`.
`interfaces/js/README.md` is the source README for the npm package. The
root-level `README.md` is generated/secondary package output and is not the
hand-maintained source of truth.
`make test-js-metadata` regenerates metadata in a temp directory and verifies
that the committed files are fresh.
`make test-js` packs the npm package and validates the local example against the
packed artifact.

The sibling `interfaces/js/react` and `interfaces/js/parser` packages are
internal-supported and are not part of the primary local workflow in this
document. `make test-js-internal` smoke-builds them in CI.

## Python package

Building the Python package requires Python packaging tooling and Sphinx.
Only SWIG maintainer tasks such as regenerating tracked wrapper outputs require
SWIG itself.

On macOS with Homebrew:

```bash
brew install sphinx-doc libomp
```

Build and test locally:

```bash
PATH="/opt/homebrew/bin:$PATH" \
CXXFLAGS="-I/opt/homebrew/opt/libomp/include" \
LDFLAGS="-L/opt/homebrew/opt/libomp/lib" \
python -m pip install -r requirements_dev.txt

PATH="/opt/homebrew/bin:$PATH" \
CXXFLAGS="-I/opt/homebrew/opt/libomp/include" \
LDFLAGS="-L/opt/homebrew/opt/libomp/lib" \
make build-python

make dev-python-install
make test-python
```

`make build-python` builds a wheel from the repo root using the tracked SWIG
artifacts under `interfaces/python/generated/` and
`interfaces/python/src/infomap/_swig.py`. If those tracked files need to be
refreshed, install SWIG and run:

```bash
make build-python-swig
make test-python-swig-freshness
```

The current automated macOS wheel build in CI/release uses
`MACOSX_DEPLOYMENT_TARGET=15.0`. If you need broader compatibility than the
published wheel currently provides, verify that policy before changing the
release pipeline.

Build source and wheel distributions locally when needed:

```bash
make release-python-dist
```

Generate the Python documentation locally:

```bash
make build-docs
```

This writes the static site into `docs/`.

Verify that the committed generated docs are fresh without rewriting `docs/`:

```bash
make test-docs
```

The documentation source lives under `interfaces/python/source/`. The committed
`docs/` tree contains the generated Sphinx site plus checked-in maintainer
content under `docs/automation/`, `docs/maintainers/`, and `docs/plans/`. Only
the generated subset is refreshed by `make build-docs` and checked by
`make test-docs`; do not hand-edit generated HTML or JS.

Docker remains supported, but it is a secondary maintainer surface:

- supported images:
  - CLI image (`docker/infomap.Dockerfile`)
  - notebook image (`docker/notebook.Dockerfile`)
- internal-supported images:
  - Python build/test image (`docker/python.Dockerfile`)
  - Ubuntu compatibility image (`docker/ubuntu.Dockerfile`)
  - RStudio image (`docker/rstudio.Dockerfile`)

Use Docker Compose v2 commands if you are invoking the local compose file, for
example `docker compose run --rm infomap`.

Advanced or internal targets such as `R`, `R-build`, and `docker-*` still exist
in the Makefile, but they are no longer part of the primary development
surface.
