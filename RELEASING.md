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

## Normal release flow

1. Merge ordinary pull requests to `master` using Conventional Commit messages.
2. Let `.github/workflows/release-please.yml` open or update the release PR.
3. Review the release PR like any other change. At minimum, check:
   - the version bumps
   - `CHANGELOG.md`
   - `src/version.h`
   - `CITATION.cff`
4. Merge the release PR.
5. Release Please creates the `vX.Y.Z` tag and the GitHub Release.
6. `.github/workflows/release.yml` runs for that tag and:
   - builds the native release assets
   - builds the Python sdist and wheels
   - builds and verifies the npm package
   - attaches assets to the GitHub Release
   - publishes to PyPI behind `pypi-release`
   - publishes to npm behind `npm-release`

## Documentation publishing

The published Python docs are served from the committed `docs/` tree on
`master`. `.github/workflows/docs.yml` is verify-only: it runs `make test-docs`
to confirm that the committed generated output derived from
`interfaces/python/source/` and `README.rst` is fresh.

## Recovery

If a release only partially succeeds:

- If GitHub Release assets fail, rerun `.github/workflows/release.yml` for the
  same tag before approving registry publishing.
- If PyPI fails before any successful publish, fix the configuration problem and
  rerun `publish-pypi`.
- If npm fails before any successful publish, fix the configuration problem and
  rerun `publish-npm`.
- If a registry publish already succeeded, do not delete or rewrite the tag.
  Resume from the remaining failed job and keep the published version.

Manual publish from a local shell should be reserved for incident recovery only.
The fallback commands remain in the `Makefile`, but they now build directly from
the repository root rather than from a staged `build/py` package tree. Treat
any local publish as an exception and document it in the release notes or issue
tracker.
