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
This document only covers local build and verification.

## Native build

Compile the C++ binary from the repository root:

```bash
make
```

On macOS with Homebrew `libomp`, use:

```bash
PATH="/opt/homebrew/bin:$PATH" \
CXXFLAGS="-I/opt/homebrew/opt/libomp/include" \
LDFLAGS="-L/opt/homebrew/opt/libomp/lib" \
make
```

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
make js-worker
make js-test
```

`make js-worker` builds the worker and bundles the published package. `make js-test`
packs the npm package and validates the local example against the packed artifact.

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
make python

make py-local-install
make py-test
```

Build source and wheel distributions locally when needed:

```bash
make pypi-dist
```

Generate the Python documentation locally:

```bash
make py-doc
```

This writes the static site into `docs/`.
