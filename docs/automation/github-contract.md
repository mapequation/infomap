# GitHub Contract

This file defines how automation may interact with GitHub issues and pull requests for this repository.

## Allowed Actions

Automation may:

- read open issues and pull requests
- classify issue risk and likely code surface
- add or update labels
- post or update one structured triage comment per issue
- create issue-scoped branches
- open pull requests

Automation may not:

- merge pull requests
- close issues without a clear PR or merge relationship
- spam multiple near-duplicate comments
- broaden issue scope beyond the smallest useful fix
- auto-implement `high` or `blocked` issues

## Preferred Labels

Use existing labels when they already match. If standardized labels are needed and maintainers allow it, prefer:

- risk: `risk:low`, `risk:medium`, `risk:high`, `risk:blocked`
- area: `area:core`, `area:python`, `area:js`, `area:build`, `area:docs`, `area:test`
- status: `status:triaged`, `status:runnable`, `status:blocked`, `status:in-progress`

If label creation is unavailable or undesirable, the triage comment must still carry the same information.

## Triage Comment Rules

- Keep one current triage comment per issue from the automation.
- Update the existing automation comment instead of stacking a new one when the classification changes.
- Keep the comment short and structured.
- Include at least:
  - Risk
  - Likely Surface
  - Verification
  - Status
  - Reason when blocked

Use `docs/automation/issue-triage-template.md`.

## PR Rules

- `low` risk + required verification passed -> `ready` PR allowed
- `medium` risk + required verification passed -> `draft` PR only
- `high` or `blocked` -> no automatic implementation PR

Every PR opened by automation must include:

- Issue reference
- Risk classification
- Short change summary
- Exact verification run
- Residual risk

Use `docs/automation/pr-template.md`.

## Branch Naming

Use one branch per issue, with a predictable prefix such as:

`codex/issue-<number>-<short-slug>`

## Re-entrancy

Automation should be safe to rerun:

- skip issues that already have a current triage result unless the issue changed
- avoid opening a second PR for the same issue unless the previous one is closed or superseded
- avoid claiming work on an issue already covered by an open automation PR
