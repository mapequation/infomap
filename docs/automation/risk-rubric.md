# Automation Risk Rubric

Use this rubric before changing code for a GitHub issue.

## Decision Rules

Classify from the issue content, likely file surface, and verification burden. Existing labels are hints only.

Escalate the risk if any of these are true:

- the issue touches more than one major surface
- the verification path is unclear or expensive
- the change affects algorithm output, determinism, memory, or release logic
- the issue lacks a clear reproduction or expected result

## Low

Typical issues:

- docs fixes
- test fixes
- small packaging or metadata corrections
- narrow Python or JS wrapper bugs
- import or installation issues with a clear local fix

Common file surfaces:

- `README.rst`
- `interfaces/python/`
- `interfaces/js/`
- `test/`
- packaging metadata and simple workflow configuration

Automation behavior:

- may implement automatically
- may open a `ready` PR if required verification passes

Escalate if:

- the issue implies C++ core changes
- the fix requires cross-platform validation beyond the available environment
- the reproduction is unclear

## Medium

Typical issues:

- bounded parsing fixes
- wrapper API behavior changes
- small C++ fixes with a clear hypothesis
- build and CI changes with moderate regression surface
- issues requiring a few files across one subsystem

Common file surfaces:

- `src/io/`
- `src/utils/`
- `interfaces/python/`
- `interfaces/js/`
- `.github/workflows/`

Automation behavior:

- may implement automatically
- must open a `draft` PR even if verification passes

Escalate if:

- the issue affects output correctness broadly
- the diff spreads across core and wrapper surfaces
- the verification results are inconclusive

## High

Typical issues:

- algorithm correctness
- numerical stability
- determinism across compilers or platforms
- OpenMP and parallel behavior
- memory growth or leaks
- release or publish logic

Common file surfaces:

- `src/core/`
- algorithm-sensitive code in `src/`
- release and publish workflows

Automation behavior:

- may triage
- may prepare notes or reproduction guidance
- must not open an automatic implementation PR

Escalate immediately if:

- the issue description mentions differing results across environments
- the issue is about NaN, Inf, overflow, convergence, or nondeterministic outputs

## Blocked

Typical issues:

- no reproduction
- missing environment or credentials
- unclear expected behavior
- unresolved maintainer decision

Common file surfaces:

- unknown until clarified

Automation behavior:

- do not implement
- do not open a PR
- leave a short comment that explains what is missing

Escalate by marking blocked when:

- required tools are unavailable
- the issue does not identify a failing behavior precisely enough to test
- the likely fix would require guessing at intended semantics

## PR Outcome Mapping

- `low` + required verification passes -> `ready` PR allowed
- `medium` + required verification passes -> `draft` PR only
- `high` -> no automatic implementation PR
- `blocked` -> no implementation PR
