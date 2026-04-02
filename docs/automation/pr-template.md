# PR Template

Use this structure for automation-created pull requests.

```md
## Issue

Closes #<issue-number>

## Risk

<low|medium>

## Summary

- <smallest useful change>

## Verification

- `<exact command>`
- `<exact command>`

## Residual Risk

- <one short sentence>
```

Rules:

- Open `ready` only for `low` risk issues after required verification passes.
- Open `draft` for every `medium` risk issue, even when verification passes.
- Do not use this template for `high` or `blocked` issues because they should not produce automatic implementation PRs.
