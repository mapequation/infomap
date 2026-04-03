# AGENTS.md

## Mission

Reduce the GitHub issue backlog with the smallest safe change, the smallest useful verification, and the least possible ambiguity.

## Repo Map

- `src/`: C++ core logic, algorithms, parsing, IO, and CLI behavior
- `interfaces/python/`: Python package sources and SWIG-facing wrapper code
- `interfaces/js/`: JavaScript worker packaging and TypeScript sources
- `test/`: Python-facing regression tests and fixtures
- `examples/python/`: executable Python examples used by `make test-python`
- `docs/`: generated documentation output; do not hand-edit unless the task is explicitly about docs generation or publishing
- `.github/workflows/`: CI, release, packaging, and publishing workflows

## Default Work Unit

Treat each issue as one bounded work unit:

1. classify risk
2. identify the smallest affected surface
3. choose the minimum required verification
4. make the smallest useful change
5. open a PR according to the repo PR policy

Do not batch unrelated issues into a single change.

## Risk Policy

- `low`: docs, tests, small build or tooling fixes, isolated Python or JS wrapper bugs, import or packaging issues with narrow blast radius
- `medium`: bounded parsing changes, wrapper API behavior changes, constrained C++ fixes with a clear hypothesis, or build fixes with moderate regression surface
- `high`: algorithm correctness, determinism across platforms, numerical stability, OpenMP or parallel behavior, memory behavior, release or publishing logic
- `blocked`: missing reproduction, missing environment, unclear ownership, or unresolved product or research decision

PR policy:

- `low` + required verification passes -> `ready` PR allowed
- `medium` + required verification passes -> `draft` PR only
- `high` or `blocked` -> no automatic implementation PR

See `docs/automation/risk-rubric.md` for the detailed rubric.

## Verification

Run the smallest sufficient verification for the changed surface.

- `src/` changes: at least `make build-native`
- Python wrapper changes: `make build-python` and `make test-python`
- JS package or worker changes: `npm ci` and the smallest relevant build, usually `make build-js` or `make test-js`
- CI or release workflow changes: limit local verification and default to `draft`
- docs-only changes: no code build unless the issue depends on generated docs freshness, in which case run `make test-docs`

See `docs/automation/verification-matrix.md` for the command matrix.

## GitHub Contract

Automation may:

- classify issues
- add or update labels
- leave one short structured triage comment per issue and update it when needed
- open issue-scoped pull requests

Automation may not:

- merge pull requests
- close issues without a clear PR or merge relationship
- silently broaden issue scope
- treat unlabeled issues as low risk by default

See `docs/automation/github-contract.md` for the exact rules and templates.

## Environment

- Prefer Codex Cloud for recurring backlog automation.
- Do not assume local shell setup matches cloud setup.
- Verify tool availability before use: `gh`, `python`, `node`, `swig`, `em++`, and the compiler toolchain.
- On some local macOS setups, `gh` may exist at `/opt/homebrew/bin/gh` even when Homebrew paths are missing from `PATH`.
- Never develop directly on `master`; create or use an issue-specific branch.

## Do Not Guess

- Do not improvise around SWIG generation or Python packaging.
- Do not improvise around JS worker generation or Emscripten.
- Do not hand-edit generated docs in `docs/` unless the issue is explicitly about the generated output.
- Do not make release or publishing changes automatically.
- Do not treat algorithmic, numerical, or determinism bugs as low risk.

## Escalation

Stop and hand off when:

- the issue spans multiple major surfaces
- the verification path is unavailable in the current environment
- the fix appears to require architectural redesign instead of a bounded patch
- the behavior change cannot be validated with a small scoped check
- the issue points to algorithm correctness, cross-platform divergence, or memory behavior
