# Releasing

Infomap releases are tag-driven and orchestrated by GitHub Actions.
Maintainers do not publish npm or PyPI packages directly from local shells during
the normal flow.

Supported public release surfaces are:

- CLI release assets
- Python package
- `@mapequation/infomap`
- committed Python documentation output in `docs/`

Internal-supported JS sibling packages and secondary Docker images are outside
the automated public release path unless a maintainer explicitly expands scope.

## One-time repository setup

Configure these repository integrations before the first automated release:

1. GitHub Environments
   - `pypi-release`
   - `npm-release`
2. Add required reviewers to `pypi-release` and `npm-release`.
3. Configure PyPI trusted publishing for this repository and the
   `.github/workflows/release.yml` workflow.
4. Configure npm trusted publishing for this repository and the
   `.github/workflows/release.yml` workflow.
5. Remove legacy registry secrets after trusted publishing is confirmed:
   - `PYPI_USERNAME`
   - `PYPI_PASSWORD`
   - `NPM_TOKEN`

## Normal release flow

1. Merge ordinary pull requests with Conventional Commit messages to `master`.
2. Wait for `.github/workflows/release-please.yml` to open or update a release PR.
3. Review the release PR like any other change:
   - version bump
   - changelog
   - `src/version.h`
   - `CITATION.cff`
4. Merge the release PR.
5. Release Please creates the `vX.Y.Z` tag and GitHub Release.
6. `.github/workflows/release.yml` runs on that tag and:
   - builds native binaries
   - builds Python sdist and wheels
   - builds and tests the npm package
   - uploads release assets to GitHub
   - publishes to PyPI behind the `pypi-release` environment gate
   - publishes to npm behind the `npm-release` environment gate

## Documentation publishing

Python docs are published from the committed `docs/` tree on `master`, which is
the current GitHub Pages source for this repository. `.github/workflows/docs.yml`
is a verify-only workflow: on pull requests and on `master`, it runs
`make test-docs` to verify that the committed generated output derived from
`interfaces/python/source/` plus `README.rst` is fresh.

## Recovery

If a release partially succeeds:

- If GitHub Release assets failed, rerun `.github/workflows/release.yml` for the
  same tag before approving any registry publish steps.
- If PyPI publish failed before success, rerun the `publish_pypi` job after
  fixing the configuration issue.
- If npm publish failed before success, rerun the `publish_npm` job after
  fixing the configuration issue.
- If a registry publish already succeeded, do not delete or rewrite the tag.
  Resume from the remaining failed job and keep the existing version.

Manual publish from a local shell should be reserved for incident recovery only.
The fallback commands remain in the `Makefile`, but they now build directly from
the repository root rather than from a staged `build/py` package tree. Treat
any local publish as an exception and document it in the release notes or issue
tracker.
