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
make build-js
make test-js
```

`make build-js` builds the worker and bundles the public npm package.
`make test-js` packs the npm package and validates the local example against the
packed artifact.

The sibling `interfaces/js/react` and `interfaces/js/parser` packages are
internal-supported and are not part of the primary local workflow in this
document.

## Python package

Building requires [SWIG](http://swig.org), Python packaging tooling, and Sphinx.

On macOS with Homebrew:

```bash
brew install swig sphinx-doc libomp
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

Build source and wheel distributions locally when needed:

```bash
make release-python-dist
```

Generate the Python documentation locally:

```bash
make build-docs
```

This writes the static site into `docs/`.

The documentation source lives under `interfaces/python/source/`. The committed
`docs/` tree is generated output and should be refreshed via the documented
build path rather than hand-edited.

Advanced or internal targets such as `R`, `R-build`, and `docker-*` still exist in
the Makefile, but they are no longer part of the primary development surface.
