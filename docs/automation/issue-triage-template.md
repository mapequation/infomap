# Issue Triage Template

Use this template for the current automation triage comment on an issue.

```md
Agent triage:

- Risk: <low|medium|high|blocked>
- Likely Surface: <core|python|js|build|docs|test|mixed>
- Verification: <minimum commands or "not runnable">
- Status: <runnable|blocked>
- Reason: <one short sentence>
```

Notes:

- Omit `Reason` only when the issue is obviously runnable and the risk is already justified by the issue body.
- If the issue is blocked, `Reason` is required.
- Update the existing automation comment instead of posting a duplicate.
