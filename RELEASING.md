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
- the committed Python docs output in `docs/`

Internal-supported packages and secondary Docker images are outside the default
public release flow unless a maintainer explicitly widens the release scope.

## One-time repository setup

Configure these integrations before the first release:

1. Create the GitHub environments `pypi-release` and `npm-release`.
2. Add the required reviewers to both environments.
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
9. Add repository dispatch tokens for downstream update workflows:
   - `HOMEBREW_INFOMAP_REPO_DISPATCH_TOKEN`
   - `INFOMAP_ONLINE_REPO_DISPATCH_TOKEN`

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
   - builds the Python sdist and wheels
   - builds the R source tarball once, then builds macOS and Windows R
     binaries from that exact tarball for both R `release` and `oldrel`
   - builds and verifies the npm package
   - attaches assets to the GitHub Release
   - builds, smoke-tests, and publishes multi-arch Docker images to GHCR:
     - `ghcr.io/mapequation/infomap:X.Y.Z`
     - `ghcr.io/mapequation/infomap:latest`
     - `ghcr.io/mapequation/infomap:notebook-X.Y.Z`
     - `ghcr.io/mapequation/infomap:notebook`
   - dispatches the Homebrew tap update workflow
   - publishes to PyPI behind `pypi-release`
   - publishes to npm behind `npm-release`
   - dispatches the `infomap-online` package update workflow

   The R package is also continuously published by r-universe from
   `master`; the release workflow does not push to r-universe directly.

   `workflow_dispatch` for `.github/workflows/release.yml` rebuilds and
   republishes an existing tag. Use it only after confirming the target version
   has not already been published to a registry that rejects duplicate uploads.

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

The published Python docs are served from the committed `docs/` tree on
`master`. `.github/workflows/docs.yml` is verify-only: it runs `make test-docs`
to confirm that the committed generated output derived from
`interfaces/python/source/` and `README.rst` is fresh.

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
