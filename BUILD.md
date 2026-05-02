# Building

This file covers the local build and verification paths that maintainers should
use most often. Release policy lives in [RELEASING.md](RELEASING.md). Ownership
and source-of-truth rules live in [ARCHITECTURE.md](ARCHITECTURE.md).

## General notes

- Use `make help` to see the maintained target surface.
- Use `make doctor` to inspect tool availability and active build flags.
- `MODE=release|debug` is the source of truth for Infomap optimization and
  debug-symbol flags across the native CLI, Python extension, and CMake-based
  native tests.
- `OPENMP=0|1` controls whether the shared build policy adds OpenMP flags.
- Builds use all detected logical cores by default; add `JOBS=1` when you want
  a serial build.
- The repository uses
  [Conventional Commits](https://www.conventionalcommits.org).

Examples:

- `fix: correct output formatting`
- `fix(python): restore iterator behavior`
- `docs: refresh build instructions`

## Native CLI

Build policy summary:

- `MODE=release` adds the maintained release optimization flags.
- `MODE=debug` adds the maintained debug flags (`-O0 -g` on Clang/GCC-style
  toolchains).
- `make doctor` prints the resolved compile and link flags for the current
  settings.

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

## C++ development

For editor and agent workflows, configure a clangd-friendly CMake build:

```bash
make configure-cpp-dev
```

This writes CMake output under `build/cmake-dev/` and links
`compile_commands.json` in the repository root. The default dev configuration
uses `INFOMAP_MODE=debug` and disables OpenMP for simpler local setup. Use
`CMAKE_DEV_OPENMP=1` when you need OpenMP in the dev build.

Run the fast C++ feedback path with:

```bash
make dev-cpp-check
```

Format C++ sources or verify formatting without rewriting files:

```bash
make format-native
make format-native-check
```

Run the opt-in clang-tidy profile with:

```bash
make tidy-native
```

On macOS with Homebrew LLVM, use the matching compiler if `clang-tidy` cannot
resolve the standard library from the default AppleClang compile database:

```bash
make tidy-native CMAKE_DEV_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
```

The same dev build is available through CMake Presets:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Use `dev-openmp` instead of `dev` for an OpenMP-enabled preset.

## Python package

`make build-python` uses the same shared `MODE`/`OPENMP` policy as
`make build-native`, so native and Python builds stay aligned unless you pass
extra flags explicitly with `CPPFLAGS`, `CXXFLAGS`, or `LDFLAGS`.

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

Run `make dev-python-install` again after changing C++ extension sources,
SWIG interfaces, or tracked SWIG outputs. Local tests import the installed
editable package; without reinstalling, they can load an older `_infomap`
extension that does not match the current Python wrapper.

More targeted checks are available when you only need one slice:

- `make test-python-unit`
- `make test-python-doctest`
- `make test-python-examples`

To build the Python extension in debug mode, pass the same mode override:

```bash
make build-python MODE=debug
```

Build source and wheel distributions locally when needed:

```bash
make release-python-dist
```

## R package

`make build-r` builds an R source tarball of the `infomap` R package
under `interfaces/R/infomap/`. The package wraps the same C++ core via
SWIG-generated R bindings tracked in `interfaces/R/generated/`.

Building the R package requires R, Python 3 (for the staging script),
and a C++ toolchain. SWIG 4.4.1 is only needed when refreshing the
tracked SWIG outputs.

On macOS with Homebrew:

```bash
brew install r libomp
# Optional, only if regenerating SWIG outputs:
brew install swig
```

Local maintainer flow:

```bash
make build-r-stage          # stage skeleton + C++ core into build/R/infomap
make dev-r-install          # R CMD INSTALL into the user library
make test-r                 # R CMD check --as-cran on the staged package
make test-r-examples        # run examples/R/*.R against the installed package
```

Build a tarball and platform binary:

```bash
make build-r                # writes dist/R/infomap_X.Y.Z.tar.gz
make build-r-binary         # writes dist/R/infomap_X.Y.Z.tgz (or .zip on Windows)
```

Refresh tracked SWIG outputs (only when the SWIG interface changes):

```bash
make build-r-swig
make test-r-swig-freshness
```

Regenerate the package's roxygen-managed Rd and NAMESPACE after
changing `R/Infomap.R`:

```bash
R CMD INSTALL --with-keep.source build/R/infomap
Rscript -e 'roxygen2::roxygenise("interfaces/R/infomap")'
```

The R skeleton declares C++17 (`SystemRequirements: C++17`,
`CXX_STD = CXX17`). This lets R's build system provide the standard flag
portably on r-universe without putting non-portable `-std=...` flags in
`PKG_CXXFLAGS`.

On macOS, `mk/r.mk` writes a temporary Makevars that pins
`CC=/usr/bin/clang` and `CXX=/usr/bin/clang++` so the compiled `.so` is
ABI-compatible with Homebrew R's libc++. This workaround is documented
in `mk/r.mk` and tagged for removal once Homebrew R links against LLVM
libc++.

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

## Native tests and benchmarks

`make test-native` and `make bench-native` pass `MODE` through to the shared
build policy used by CMake. `CMAKE_BUILD_TYPE` is still forwarded for generator
and output-directory behavior, but it does not override Infomap's own
optimization/debug policy.

Examples:

```bash
make test-native
make test-native MODE=debug
make bench-native MODE=debug CMAKE_BUILD_TYPE=Debug
```

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
