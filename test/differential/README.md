# Differential checks

Two checks run the same corpus, `cases.tsv`, with the same seed and options,
and compare the results. Both run in CI (`ci.yml`). Each compares builds on a
single platform, because the default random number generator maps its draws
through the standard library and gives other results on another one (#342).

- **Current vs. last release** (`differential-release`, ubuntu-24.04): builds
  the CLI at the release tag named in `release_differences.tsv` and at the
  current commit, and runs the corpus through both. Every difference must be
  listed in `release_differences.tsv`, with the issue or PR numbers of the
  `RESULT_CHANGES.csv` rows that explain it.
- **Front ends** (`differential-frontends`, macos-15): runs the corpus through
  the CLI, the Python package, the R package and the Node API of the npm
  package. All four build against libc++ there. Every front end must agree with
  the CLI, apart from the disagreements listed in `frontend_differences.tsv`,
  each with the issue that tracks it.

Both lists are exact: the check also fails on a listed difference that no
longer shows. A pull request that changes a listed result, or fixes one,
updates the line.

Differences between OO and columnar builds are tracked in #832 and #881.

## What is compared

For each case a run writes `<id>.tree` and `<id>.codelength`, or `<id>.error`
if the front end refused the case. A higher-order network is compared on its
state-level tree. Differences are named

- `error`: one run refused the case and the other did not,
- `codelength`: the codelengths differ by more than 1e-9 bits,
- `tree`: the modules differ, at some level,
- `order`: the same modules, with other module ids or another order,
- `flow`: the same tree, with other node flows.

The JavaScript Node API cannot pass other input files (#1327), so the
`meta_data` cases are skipped there.

## Running locally

From the repository root, after `make build-native`:

```bash
python test/differential/differential.py run-cli ./Infomap out/cli
```

The other front ends, after `make dev-python-install`, `make dev-r-install`
and `make build-js`:

```bash
python test/differential/differential.py run-python out/python
Rscript test/differential/run_r.R out/r
node test/differential/run_js.mjs out/js
python test/differential/differential.py compare out/cli out/python out/r out/js
```

Against the release, with its CLI built from the tag:

```bash
git worktree add --detach ../reference "$(python test/differential/differential.py release-reference)"
make -C ../reference build-native
python test/differential/differential.py run-cli ../reference/Infomap out/reference
python test/differential/differential.py compare-release out/reference out/cli
```

The lists hold the results on the CI platforms. On another platform the same
commands show which cases differ there.

## After a release

Set `# reference:` in `release_differences.tsv` to the new tag and empty the
list.
