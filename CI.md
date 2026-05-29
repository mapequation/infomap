# CI strategy

How the GitHub Actions workflows split work across the development lifecycle,
and why a green PR is not the same as a green release. For the release
procedure itself, see [RELEASING.md](RELEASING.md).

## What runs where

| Stage | Workflow | Trigger | Purpose |
| ----- | -------- | ------- | ------- |
| **PR** | `ci.yml` | `pull_request`, push to `master` | Fast feedback. Path-filtered; Python and R run in `quick` mode (Linux + latest version only). |
| **Scheduled** | `scheduled-ci.yml` | nightly (weekdays 03:17 UTC) + manual | Full coverage: sanitizers, clang-tidy, the full Python and R OS/version matrices, and r-universe simulation. |
| **Rehearsal** | `release-rehearsal.yml` | manual, against an existing tag | Dry run of the release build — all release artifacts, nothing published. |
| **Release** | `release.yml` | push of a `vX.Y.Z` tag | Build release artifacts and publish to PyPI, npm, GHCR, and the GitHub release. |

### Reusable workflows and `mode`

`_native-build.yml`, `_python-package.yml`, `_js-package.yml`, and
`_r-package.yml` hold the build/test logic and are called by the stages above.

- `_python-package.yml` and `_r-package.yml` take a `mode` input (`quick` |
  `full`, default `full`). PR CI passes `quick`; scheduled, release, and
  rehearsal use `full`. `quick` runs Linux + the latest version only and skips
  r-universe simulation.
- `build-release-artifacts` / `upload-artifacts` / `pack-artifact` switch the
  reusables into artifact-producing mode, which runs only at release and
  rehearsal.

Heavier native checks (sanitizers, clang-tidy) and the full matrices live in
`scheduled-ci.yml`, not PR CI. To run them on demand for a risky branch,
dispatch `scheduled-ci.yml` manually on that branch.

## Why a green PR is not a green release

PR CI never builds release artifacts. The artifact paths — cibuildwheel wheels,
the R source tarball + platform binaries (including the rename step), native
packaging, and the multi-arch Docker images — are exercised only when
`build-release-artifacts` / `upload-artifacts` / `pack-artifact` are set, i.e.
only at release time.

`release-rehearsal.yml` closes that gap. Run it against the tag **before**
pushing the release (`workflow_dispatch` → enter the tag); it reproduces every
artifact build and the version preflight without publishing anything.

## Version preflight

`scripts/check_release_versions.py --tag vX.Y.Z` asserts that every tracked
version-bearing file matches the tag and each other (`src/version.h`,
`interfaces/python/src/infomap/_version.py`, `interfaces/R/infomap/DESCRIPTION`,
`package.json`, `CITATION.cff` — the surfaces listed in
[RELEASING.md](RELEASING.md)). It reads fixed paths only, never a repo-wide
search, so a stale local build artifact cannot cause a false mismatch. It runs
as the `check-versions` gate in both `release.yml` and `release-rehearsal.yml`.

## Residual release-only risk

A green rehearsal still cannot exercise the publish path: PyPI/npm trusted
publishing and the `pypi-release`/`npm-release` environments, the downstream
repo-dispatch tokens, and the `action-gh-release` file globs in `release.yml`
(the rehearsal confirms artifacts exist, not that the globs match their names).
These are configured and verified per the one-time setup and recovery sections
of [RELEASING.md](RELEASING.md).

## Measuring the PR speedup

The `quick`/`full` split and moving sanitizers/clang-tidy/r-universe to
`scheduled-ci.yml` aim to shorten PR wall-clock. To confirm, compare a
representative PR's total runtime before and after on the Actions run page.
