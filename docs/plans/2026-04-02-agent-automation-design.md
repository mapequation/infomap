# Agent Automation Design

**Date:** 2026-04-02

## Goal

Prepare the repository for agent-driven backlog reduction across the full GitHub issue queue, with enough policy and structure that automation can triage issues, implement low- and medium-risk fixes, and open pull requests safely.

## Context

Infomap is a multi-surface repository with:

- C++ core logic in `src/`
- Python packaging and SWIG-based bindings in `interfaces/python/`
- JavaScript worker and npm packaging in `interfaces/js/`
- Tests in `test/` and `examples/python/`
- Generated documentation in `docs/`

The issue backlog is old and unevenly labeled. Some issues are obvious packaging, wrapper, or documentation fixes. Others touch algorithm correctness, determinism, memory use, parsing, or release flows. Existing GitHub metadata cannot be trusted as the sole source of truth for routing and risk.

## Chosen Approach

Use a hybrid GitHub-plus-repo workflow.

- GitHub is the public work surface for issues, labels, triage comments, and pull requests.
- The repository is the policy surface for agent behavior: risk classification, verification scope, environment rules, and PR rules.
- Automation is split into two stages:
  - `triage`: classify issues, update labels/comments, and determine whether they are runnable
  - `execution`: pick triaged low- and medium-risk issues, implement the smallest useful fix, verify at the smallest useful scope, and open a PR

This avoids putting all judgment into a single automation step while still allowing the system to work through the full backlog autonomously.

## Non-Goals

- Full autonomy for high-risk algorithmic or release-sensitive issues
- Automatic merges
- Broad refactors or “cleanup” work not anchored to a specific issue
- Treating unlabeled issues as safe by default

## Repository Contract

The repository should define automation behavior through a small set of explicit files:

- `AGENTS.md`
- `docs/automation/risk-rubric.md`
- `docs/automation/verification-matrix.md`
- `docs/automation/github-contract.md`
- `docs/automation/issue-triage-template.md`
- `docs/automation/pr-template.md`

These files should be short, opinionated, and version-controlled so the automation can improve without relying on hidden prompts.

## AGENTS.md Responsibilities

The repo-local `AGENTS.md` should:

- map the important code surfaces and generated outputs
- define the default work unit for issue-driven changes
- state the risk policy and escalation triggers
- summarize verification expectations and point to the detailed verification matrix
- define GitHub write permissions for the automation
- distinguish local workstation behavior from Codex Cloud behavior
- list repo-specific “do not guess” rules for SWIG, JS worker builds, release flows, and generated docs

The file should be short enough to read at the start of every session and strict enough to keep agents from improvising process.

## Risk Policy

The risk model should be deterministic and simple:

- `low`
  - docs, tests, small build/tooling fixes
  - isolated Python or JS wrapper bugs with clear reproduction
  - import, packaging, or metadata issues with narrow blast radius
- `medium`
  - limited parsing fixes
  - bounded wrapper/API behavior changes
  - constrained C++ fixes with a clear hypothesis and scoped verification
  - build or CI fixes with moderate regression surface
- `high`
  - algorithm correctness
  - determinism across platforms
  - numerical stability
  - parallelism and OpenMP behavior
  - memory behavior
  - release and publishing logic
- `blocked`
  - no clear reproduction
  - missing environment/tooling
  - unresolved product or research decision

PR policy should be tied directly to this model:

- `low` + required verification passes -> open `ready` PR
- `medium` + required verification passes -> open `draft` PR
- `high` and `blocked` -> no automatic implementation PR

## Verification Model

Verification should always match the smallest affected surface.

- C++ changes in `src/` require at least `make`
- Python wrapper changes require `make python` and `make py-test`
- JavaScript worker or package changes require the relevant npm or worker build
- CI/release changes should default to `draft`, even if basic checks pass
- Generated docs in `docs/` should not be hand-edited unless the issue specifically requires it

The detailed command mapping belongs in `docs/automation/verification-matrix.md`, not duplicated throughout the repo.

## GitHub Contract

Automation may:

- classify and apply labels
- leave short structured triage comments
- open pull requests
- document verification and residual risk in PR descriptions

Automation may not:

- merge PRs automatically
- close issues without a clear PR or merge relationship
- remove labels casually
- silently broaden issue scope

## Environment Policy

The design should explicitly support both local runs and Codex Cloud.

- Prefer Codex Cloud for repeatable automation runs against the issue queue
- Do not assume local shell setup matches cloud setup
- Verify tool availability explicitly, especially `gh`, `python`, `node`, `swig`, and `em++`
- Local runs may require Homebrew paths that are not on the default shell `PATH`

## Rollout Strategy

Phase 1 should establish the repo contract only:

- add `AGENTS.md`
- add the automation policy documents
- add templates for triage and PR output

Phase 2 should implement the actual automations against GitHub and Codex Cloud using the new contract files as policy inputs.

## Decision Summary

The approved direction is:

- optimize for autonomous issue execution across the full backlog
- use a hybrid GitHub-plus-repo control model
- allow automation to classify issues itself
- allow automation to write GitHub labels and comments
- allow `ready` PRs only for low-risk issues
- route medium-risk issues to `draft`
- keep high-risk and blocked issues out of automatic implementation
