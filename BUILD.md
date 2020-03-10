# Building

## Conventional commits

This repo uses [conventional commits](https://www.conventionalcommits.org) formatted messages.

Basically, it means that your commit messages should be [atomic](https://en.wikipedia.org/wiki/Atomic_commit#Atomic_commit_convention)
and prepended with the type of change it introduces.

This is used for creating [CHANGELOG.md](CHANGELOG.md) and the changelog
object used by the JavaScript worker version.

For example

- `fix: Off by one error in output`
- `feat: Support multilayer files`
- `docs: Update installation instructions for Windows`

Other commit types are e.g. `perf`, `refactor`, `test` and `build`.

### Scopes

Scopes in conventional commits is appended to the commit type.

We use the scopes `js` and `python`, for example

- `fix(js): Check that error handler is a function`
- `docs(python): Update Python documentation`


## Releasing new versions

- Commit everything with conventional commit messages
- Run `npm run release -- --dry-run` to run [standard-version](https://github.com/conventional-changelog/standard-version)
    - Double check the output and that the `bumpFiles` has been updated
    - Run `npm run release` (note: do not amend the release commit, the tag will point to the wrong commit!)
    - Run `git push --follow-tags`


### JavaScript

Building requires [Emscripten](https://emscripten.org/docs/getting_started/downloads.html).

In short:

```
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

We also need Infomap to extract the command line arguments for Infomap Online.

To build:

- Follow the [release workflow](#releasing-new-versions) before building
- Run `make js-worker`
    - This creates `build/js/infomap.worker.js`
    - Copies the worker to `interfaces/js/src/worker`
    - Runs `npm run build` which bundles the worker with the js source files and copies them to `dist`
    - Copies the js README.md to the root
- Run `npm publish`
    - Optionally run `make js-clean`

To test:

Instead of `npm publish`, run `npm pack` and extract the `tgz` file. Copy `packages/dist/index.js` to
`examples/js/` and replace the script source from the CDN to the local `./index.js`.


### Python

Building requires [Swig](http://swig.org) and [Sphinx](https://www.sphinx-doc.org)

On macOS, install with `brew install swig` and `brew install sphinx-doc`.

To build:

- Follow the [release workflow](#releasing-new-versions) before building
- Run `make python`
    - Runs `swig`
    - Creates `package-meta.py`
    - Copies python package files to `build/py`
- Generate documentation and test
    - Run `make py-local-install` and try `python -c "import infomap; print(infomap.__version__)"`
    - Run `make py-doc` which generates the documentation for Github pages. The front page is generated from `README.rst`
    - Commit the documentation with a `docs(python)` scoped commit.
- Test publish with `make pypitest_publish`
    - Install in a clean environment `pip3 --no-cache-dir install --index-url https://test.pypi.org/simple/ infomap`
- Publish with `make pypi_publish`
