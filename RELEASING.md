# Releasing

Infomap releases are tag-driven and published by GitHub Actions.
The normal path does not involve publishing to PyPI or npm from a local shell.

Public release deliverables are:

- native CLI release assets
- the `infomap` Python package
- the `infomap` R package:
  - source tarball: `infomap_X.Y.Z.tar.gz`
  - macOS binaries: `infomap_X.Y.Z-rrelease-macos.tgz` and
    `infomap_X.Y.Z-roldrel-macos.tgz`
  - Windows binaries: `infomap_X.Y.Z-rrelease-windows.zip` and
    `infomap_X.Y.Z-roldrel-windows.zip`
- the `@mapequation/infomap` npm package
- multi-arch Docker images published to GHCR for `linux/amd64` and
  `linux/arm64`
- the Python documentation site, built from `interfaces/python/source/`
  and `README.rst` and deployed to GitHub Pages

Internal-supported packages and secondary Docker images are outside the default
public release flow unless a maintainer explicitly widens the release scope.

## One-time repository setup

Configure these integrations before the first release:

1. Create the GitHub environments `pypi-release` and `npm-release` (and, for
   the prerelease workflow, `pypi-prerelease` and `npm-prerelease`).
2. Add the required reviewers to these environments.
3. Configure PyPI trusted publishing for project `infomap` with owner
   `mapequation`, repository `infomap`, workflow `release.yml`, and
   environment `pypi-release`.
4. Configure npm trusted publishing for package `@mapequation/infomap` with
   owner `mapequation`, repository `infomap`, workflow `release.yml`, and
   environment `npm-release`.
5. Remove legacy registry secrets after trusted publishing is confirmed:
   - `PYPI_USERNAME`
   - `PYPI_PASSWORD`
   - `NPM_TOKEN`
6. Keep `release-please-config.json` aligned with the historic tag format:
   `include-v-in-tag: true` and `include-component-in-tag: false`.
   Infomap's existing releases use tags like `v2.9.2`, not
   `infomap-v2.9.2`. If Release Please is allowed to add the component
   prefix, the first release PR after the migration can re-include already
   released commits and generate an overlapping `CHANGELOG.md`.
7. Confirm the Release Please GitHub App is configured through these
   repository secrets:
   - `RELEASE_PLEASE_APP_ID`
   - `RELEASE_PLEASE_PRIVATE_KEY`

   The app needs repository permissions for contents, pull requests, and
   issues. Do not use the default `GITHUB_TOKEN` for Release Please: tags
   created with `GITHUB_TOKEN` do not trigger the follow-up `release.yml`
   workflow that publishes packages.
8. Confirm that GitHub Actions can publish this repository's package to GHCR.
   The release workflow uses `GITHUB_TOKEN` with `packages: write` permission
   and publishes `ghcr.io/mapequation/infomap`.
9. Add repository dispatch tokens for the downstream update workflows fired at
   the end of `release.yml` (all send `event_type=infomap_release_published`):
   - `HOMEBREW_INFOMAP_REPO_DISPATCH_TOKEN` → `mapequation/homebrew-infomap`
   - `PYTHON_DOCS_DISPATCH_TOKEN` → `mapequation/infomap-python-docs`
   - `MAPEQUATION_REPO_DISPATCH_TOKEN` → `mapequation/mapequation.github.io`

## Normal release flow

1. Merge ordinary pull requests to `master` using Conventional Commit messages.
2. Let `.github/workflows/release-please.yml` open or update the release PR.
3. Review the release PR. Verify the main release version fields moved to the
   same `X.Y.Z` and match the PR title:
   - `package.json` (`version`) — drives the npm package
   - `src/version.h` (`INFOMAP_VERSION`) — compiled into the native CLI
   - `interfaces/python/src/infomap/_version.py` (`__version__`) — Python package
   - `interfaces/R/infomap/DESCRIPTION` (`Version`) — R package version only
   - `CITATION.cff` (`version`) — citation metadata

   Also confirm `CHANGELOG.md` contains only entries new since the last
   `vX.Y.Z` tag. If a version field did not move, the issue is in
   `release-please-config.json` `extra-files`, not in the PR.

   The R package is not a separate Release Please component. Its committed
   `DESCRIPTION` version is bumped as a generic `extra-files` entry so release
   artifacts and repository metadata stay aligned. The staged `DESCRIPTION` is
   also resynced from `_version.py` by `scripts/stage_r_package.py` whenever
   `make build-r` or the r-universe `configure` script runs.
4. Merge the release PR.
5. Release Please creates the `vX.Y.Z` tag and the GitHub Release.
6. `.github/workflows/release.yml` runs for that tag and:
   - builds the native release assets
   - builds the Python sdist and wheels through the full Python workflow tier
   - builds the R source tarball once, then builds macOS and Windows R
     binaries from that exact tarball for both R `release` and `oldrel`
   - builds and verifies the npm package
   - attaches assets to the GitHub Release
   - builds, smoke-tests, and publishes multi-arch Docker images to GHCR:
     - `ghcr.io/mapequation/infomap:X.Y.Z`
     - `ghcr.io/mapequation/infomap:latest`
     - `ghcr.io/mapequation/infomap:notebook-X.Y.Z`
     - `ghcr.io/mapequation/infomap:notebook`
   - publishes to PyPI behind `pypi-release` (with PEP 740 attestations)
   - publishes to npm behind `npm-release` (with `--provenance`)
   - dispatches the downstream update workflows: the Homebrew tap
     (`homebrew-infomap`), the Python documentation site
     (`infomap-python-docs`), and the mapequation.org website
     (`mapequation.github.io`)

   The R package is also continuously published by r-universe from
   `master`; the release workflow does not push to r-universe directly.

   `workflow_dispatch` for `.github/workflows/release.yml` rebuilds and
   republishes an existing tag. Use it only after confirming the target version
   has not already been published to a registry that rejects duplicate uploads.

## Feature prerelease wheels

Feature prerelease wheels use the same `infomap` package name and Python
prerelease versions. They are wheel-only artifacts because sdists are rebuilt
by installers and cannot reliably preserve compile-time feature flags.

`.github/workflows/prerelease.yml` has two modes:

- Tag pushes for `vX.Y.Z-rc.N` or `vX.Y.Z-beta.N` build the full Python and
  JavaScript prerelease and publish to PyPI and npm.
- Manual `workflow_dispatch` runs build feature-enabled Python wheels only.
  Use `publish=false` first to build artifacts for inspection.

When publishing feature wheels, use a prerelease version that has not already
been uploaded to PyPI:

```bash
python -m pip install --pre infomap
python -m pip install "infomap==X.Y.ZrcN"
```

Stable releases remain feature-free unless a feature graduates out of its
compile-time gate.

## R Package Publishing

GitHub Releases ship the R source tarball plus macOS and Windows binaries.
Linux users install the source tarball or use r-universe. The release workflow
does not attach Linux R binaries because `R CMD INSTALL --build` produces
Linux artifacts tied to the runner's specific R and glibc environment, unlike
the conventional macOS and Windows binary channels.

The R release filenames are intentionally unique across R versions:

| Asset | Filename |
| ----- | -------- |
| Source | `infomap_X.Y.Z.tar.gz` |
| macOS, R release | `infomap_X.Y.Z-rrelease-macos.tgz` |
| macOS, R oldrel | `infomap_X.Y.Z-roldrel-macos.tgz` |
| Windows, R release | `infomap_X.Y.Z-rrelease-windows.zip` |
| Windows, R oldrel | `infomap_X.Y.Z-roldrel-windows.zip` |

r-universe is continuous off `master` and usually publishes within minutes of
a merged commit. It is separate from the tag-driven GitHub Release flow:

- Registry source of truth:
  `https://github.com/mapequation/mapequation.r-universe.dev`
- User-facing package endpoint:
  `https://mapequation.r-universe.dev`
- Auto-managed build repository and run history:
  `https://github.com/r-universe/mapequation/actions/workflows/build.yml`
- Reusable deploy workflow maintained by r-universe:
  `https://github.com/r-universe-org/workflows`

The registry repository's `packages.json` declares `subdir` for this package.
If the R package directory is moved or renamed, update `subdir` there. Do not
edit `https://github.com/r-universe/mapequation` directly; it is generated by
r-universe from the registry.

### Conventional Commit to version bump

Release Please derives the bump from commit messages on `master` since the
last `vX.Y.Z` tag:

| Commit prefix                       | Bump  |
| ----------------------------------- | ----- |
| `fix:`                              | patch |
| `feat:`                             | minor |
| `feat!:` / `BREAKING CHANGE:` body  | major |
| `chore:`, `docs:`, `test:`, `ci:`   | none  |

If the proposed bump does not match the commit log
(`git log vLAST..master --oneline`), investigate before merging.

### Python API deprecation policy

A deprecated method, property, class, or keyword carries a
`.. deprecated:: <version>` note in its docstring, keeps working, and returns
the same values as its non-deprecated replacement. How loudly it warns at
runtime depends on how the caller reaches it — three tiers, all pinned by
`test/python/test_deprecations.py` (plus `test/python/test_wrapper_args.py`
for the parameters):

- **Silent-by-default `PendingDeprecationWarning`** — the legacy stateful
  surface: the `Result` accessors mirrored on `Infomap` (`get_modules`,
  `codelength`, …), the advanced-tier keywords on `Infomap()` / `Infomap.run`,
  and `from_options` / `run_with_options` / `from_scipy_sparse_matrix` /
  `from_edge_index`. Silent under the default warning filter, so existing code
  is not disrupted; surface it with `python -W` or a logging filter.
- **Docs-only, no runtime warning** — the `add_*` build-from-graph adapters
  (`add_networkx_graph`, `add_scipy_sparse_matrix`, `add_edge_index`,
  `add_igraph_graph`). The `.. deprecated::` note is the only signal.
- **Louder `DeprecationWarning`** — an explicitly passed deprecated *parameter*
  (`include_self_links`, `pretty`): silently ignoring or rewriting an argument
  the caller actually typed hides a migration they need to make.

**Removal is a major-version event** (`feat!:` / `BREAKING CHANGE`), never part
of a minor or patch release.

Always include the version in the directive (`.. deprecated:: 2.15`) — Sphinx
renders an empty version otherwise. Use the release in which the deprecation
first shipped; the 2.x → 3.0 wave deprecating the pre-redesign API is `2.15`
(2.14.0 shipped the redesign but no versioned markers or runtime warnings).

### Red flags that block merge

- **Tag format regressed.** PR title shows `infomap-vX.Y.Z` instead of
  `vX.Y.Z`. Means `include-component-in-tag` flipped to `true`. Fix
  `release-please-config.json` first; do not merge the release PR.
- **CHANGELOG contains duplicate entries.** Entries near the top are
  already present further down. Release Please rebuilt from the wrong
  base tag. Close the stale release PR (or delete the
  `release-please--branches--...` branch) and rerun
  `.github/workflows/release-please.yml` so it rebuilds from the latest
  existing `vX.Y.Z` tag.
- **Version surfaces out of sync.** One of the four `extra-files` did
  not move. Fix `release-please-config.json` and rerun release-please;
  do not patch the PR by hand.
- **Bump does not match commits.** E.g. only `fix:` commits but a minor
  bump appears. Inspect the log; a stray `feat:` may be hiding, or the
  config has changelog-section overrides that need attention.

## Documentation publishing

`release.yml` does not deploy documentation itself. After a tagged release it
dispatches the `mapequation/infomap-python-docs` repository (via the
`notify-python-docs` job, `event_type=infomap_release_published`), and that
downstream repository builds and deploys the published Python docs site. The
`mapequation/mapequation.github.io` website is refreshed by the parallel
`notify-mapequation` dispatch.

On pull requests, the `docs` job in `.github/workflows/ci.yml` runs
`make build-docs` to verify the site still builds. Nothing is deployed
from CI. The `docs/` directory is build output and is not tracked in the
repo. (`.github/workflows/deploy-redirect.yml` is a separate,
manually-dispatched workflow that publishes only a GitHub Pages redirect stub;
it is not part of the release flow.)

## Recovery

Before any recovery action, verify what actually published. Registry
state is the source of truth, not the workflow log:

```bash
gh release view vX.Y.Z
pip index versions infomap
npm view @mapequation/infomap versions --json
docker buildx imagetools inspect ghcr.io/mapequation/infomap:X.Y.Z
docker buildx imagetools inspect ghcr.io/mapequation/infomap:notebook-X.Y.Z
```

If a recovery action would conflict with what is already published, stop
and resume from the failed job instead. Never delete or rewrite a tag
whose version exists on PyPI or npm.

After the first release carrying them, confirm the signed attestations were
emitted (generated by the `attestations: true` PyPI publish and the
`npm publish --provenance` step):

```bash
# PyPI (PEP 740, sigstore-backed): the release files carry a provenance URL.
# Check the "Provenance" panel on https://pypi.org/project/infomap/#files,
# or via the JSON API:
curl -s https://pypi.org/pypi/infomap/json | \
  python3 -c 'import sys,json; [print(u["filename"], bool(u.get("provenance"))) for u in json.load(sys.stdin)["urls"]]'

# npm provenance: shown on the npmjs.org version page, or:
npm view @mapequation/infomap --json | python3 -c 'import sys,json; print("provenance" in json.load(sys.stdin).get("dist",{}))'
```

If a release only partially succeeds:

- If GitHub Release assets fail, rerun `.github/workflows/release.yml` with
  `workflow_dispatch` for the same tag to rebuild and re-attach the GitHub
  Release assets before approving registry publishing.
- If PyPI fails before any successful publish, fix the configuration problem and
  rerun `.github/workflows/release.yml` with `workflow_dispatch` for the same
  tag, or rerun `publish-pypi` in the original workflow if its artifacts are
  still available.
- If npm fails before any successful publish, fix the configuration problem and
  rerun `.github/workflows/release.yml` with `workflow_dispatch` for the same
  tag, or rerun `publish-npm` in the original workflow if its artifacts are
  still available.
- If GHCR publishing fails before any successful publish, fix the configuration
  problem and rerun `.github/workflows/release.yml` with `workflow_dispatch`
  for the same tag. If any GHCR tags already published, inspect the registry
  state before rerunning because the workflow also updates the mutable `latest`
  and `notebook` tags.
- If a registry publish already succeeded, do not delete or rewrite the tag.
  Resume from the remaining failed job and keep the published version.

Manual publish from a local shell should be reserved for incident recovery only.
The fallback commands remain in the `Makefile`, but they now build directly from
the repository root rather than from a staged `build/py` package tree. Treat
any local publish as an exception and document it in the release notes or issue
tracker.
