# Building

This file covers the local build and verification paths that maintainers should
use most often. Release policy lives in [RELEASING.md](RELEASING.md). Ownership
and source-of-truth rules live in [ARCHITECTURE.md](ARCHITECTURE.md).

## General notes

- Use `make help` to see the maintained target surface.
- Use `make doctor` to inspect tool availability and active build flags.
- Builds use all detected logical cores by default; add `JOBS=1` when you want
  a serial build.
- The repository uses
  [Conventional Commits](https://www.conventionalcommits.org).

Examples:

- `fix: correct output formatting`
- `fix(python): restore iterator behavior`
- `docs: refresh build instructions`

## Native CLI

Build the C++ binary from the repository root:

```bash
make build-native
```

On macOS with Homebrew `libomp`, use:

```bash
PATH="/opt/homebrew/bin:$PATH" \
CXXFLAGS="-I/opt/homebrew/opt/libomp/include" \
LDFLAGS="-L/opt/homebrew/opt/libomp/lib" \
make build-native
```

The compiled binary is written to `./Infomap`.

## Python package

Building the Python package requires [SWIG](https://www.swig.org/), Python
packaging tooling, and Sphinx.

On macOS with Homebrew:

```bash
brew install swig sphinx-doc libomp
```

Install development dependencies, build the extension, then run the full Python
verification path:

```bash
python -m pip install -e '.[test,docs,examples,release]'

PATH="/opt/homebrew/bin:$PATH" \
CXXFLAGS="-I/opt/homebrew/opt/libomp/include" \
LDFLAGS="-L/opt/homebrew/opt/libomp/lib" \
make build-python

make dev-python-install
make test-python
```

More targeted checks are available when you only need one slice:

- `make test-python-unit`
- `make test-python-doctest`
- `make test-python-examples`

Build source and wheel distributions locally when needed:

```bash
make release-python-dist
```

## JavaScript worker package

The JavaScript package requires
[Emscripten](https://emscripten.org/docs/getting_started/downloads.html) and
Node.js.

Typical local flow:

```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install 5.0.5
./emsdk activate 5.0.5
source ./emsdk_env.sh
cd /path/to/infomap
npm ci
make build-js-metadata
make build-js
make test-js-metadata
make test-js
```

Keep local maintainer builds on the same emsdk version that CI uses: `5.0.5`.

What each target does:

- `make build-js-metadata` refreshes the tracked metadata under
  `interfaces/js/generated/`
- `make build-js` builds the worker and stages the public npm package in
  `dist/npm/package/`
- `make test-js-metadata` verifies that the tracked metadata is fresh
- `make test-js` packs the npm package and smoke-tests the packaged example

`interfaces/js/README.md` is the source README for the public npm package.
`@mapequation/infomap/react` is exported from the main package.

## Documentation

Building the Python package requires Python packaging tooling and Sphinx.
Only SWIG maintainer tasks such as regenerating tracked wrapper outputs require
SWIG itself.

The Python docs source lives under `interfaces/python/source/`.
`README.rst` provides the landing-page content reused in the generated docs.

On macOS with Homebrew:

```bash
brew install sphinx-doc libomp
```

`make build-python` builds a wheel from the repo root using the tracked SWIG
artifacts under `interfaces/python/generated/` and
`interfaces/python/src/infomap/_swig.py`. If those tracked files need to be
refreshed, use SWIG 4.4.1 and run:

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

After the normal Python setup from the section above, refresh the committed
Sphinx output in `docs/` with:

```bash
make build-docs
```

Verify that the committed generated output is fresh without rewriting `docs/`
with:

```bash
make test-docs
```

Do not hand-edit generated HTML, JavaScript, or `_sources` files under `docs/`.

## Docker

Docker is supported, but it is a smaller maintainer surface than the native,
Python, and JavaScript build paths.

Supported images:

- CLI image: `docker/infomap.Dockerfile`
- Notebook image: `docker/notebook.Dockerfile`

Internal-supported images:

- Python build/test image: `docker/python.Dockerfile`
- Ubuntu compatibility image: `docker/ubuntu.Dockerfile`
- RStudio image: `docker/rstudio.Dockerfile`

Use Docker Compose v2 commands with the local compose file, for example:

```bash
docker compose run --rm infomap
```
