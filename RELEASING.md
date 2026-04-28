# Releasing

Infomap releases are tag-driven and published by GitHub Actions.
The normal path does not involve publishing to PyPI or npm from a local shell.

Public release deliverables are:

- native CLI release assets
- the `infomap` Python package
- the `@mapequation/infomap` npm package
- the committed Python docs output in `docs/`

Internal-supported packages and secondary Docker images are outside the default
public release flow unless a maintainer explicitly widens the release scope.

## One-time repository setup

Configure these integrations before the first release:

1. Create the GitHub environments `pypi-release` and `npm-release`.
2. Add the required reviewers to both environments.
3. Configure PyPI trusted publishing for this repository and
   `.github/workflows/release.yml`.
4. Configure npm trusted publishing for this repository and
   `.github/workflows/release.yml`.
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
7. Add repository dispatch tokens for downstream update workflows:
   - `HOMEBREW_INFOMAP_REPO_DISPATCH_TOKEN`
   - `INFOMAP_ONLINE_REPO_DISPATCH_TOKEN`

## Normal release flow

1. Merge ordinary pull requests to `master` using Conventional Commit messages.
2. Let `.github/workflows/release-please.yml` open or update the release PR.
3. Review the release PR. Verify all four version surfaces moved to the
   same `X.Y.Z` and match the PR title:
   - `package.json` (`version`) — drives the npm package
   - `src/version.h` (`INFOMAP_VERSION`) — compiled into the native CLI
   - `interfaces/python/src/infomap/_version.py` (`__version__`) — Python package
   - `CITATION.cff` (`version`) — citation metadata

   Also confirm `CHANGELOG.md` contains only entries new since the last
   `vX.Y.Z` tag. If a version surface did not move, the issue is in
   `release-please-config.json` `extra-files`, not in the PR.
4. Merge the release PR.
5. Release Please creates the `vX.Y.Z` tag and the GitHub Release.
6. `.github/workflows/release.yml` runs for that tag and:
   - builds the native release assets
   - builds the Python sdist and wheels
   - builds and verifies the npm package
   - attaches assets to the GitHub Release
   - dispatches the Homebrew tap update workflow
   - publishes to PyPI behind `pypi-release`
   - publishes to npm behind `npm-release`
   - dispatches the `infomap-online` package update workflow

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
```

If a recovery action would conflict with what is already published, stop
and resume from the failed job instead. Never delete or rewrite a tag
whose version exists on PyPI or npm.

If a release only partially succeeds:

- If GitHub Release assets fail, rerun `.github/workflows/release.yml` with
  `workflow_dispatch` for the same tag to rebuild and re-attach the GitHub
  Release assets before approving registry publishing.
- If PyPI fails before any successful publish, fix the configuration problem and
  rerun `publish-pypi` in the original tag-triggered release workflow.
- If npm fails before any successful publish, fix the configuration problem and
  rerun `publish-npm` in the original tag-triggered release workflow.
- If a registry publish already succeeded, do not delete or rewrite the tag.
  Resume from the remaining failed job and keep the published version.

Manual publish from a local shell should be reserved for incident recovery only.
The fallback commands remain in the `Makefile`, but they now build directly from
the repository root rather than from a staged `build/py` package tree. Treat
any local publish as an exception and document it in the release notes or issue
tracker.
