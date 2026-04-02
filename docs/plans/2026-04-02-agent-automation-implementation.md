# Agent Automation Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add the repo-local policy and template files needed for issue-driven agent automation across the Infomap backlog.

**Architecture:** Keep the first implementation scoped to repository contracts only. Add a short repo-local `AGENTS.md` plus supporting automation docs under `docs/automation/`, and do not implement the actual automation runner in this pass.

**Tech Stack:** Markdown, git, existing Makefile- and GitHub-based workflows

---

### Task 1: Add Repo-Local AGENTS Contract

**Files:**
- Create: `AGENTS.md`
- Reference: `/Users/anton/kod/AGENTS.md`
- Reference: `Makefile`
- Reference: `.github/workflows/build.yml`
- Reference: `.github/workflows/python-build.yml`
- Reference: `.github/workflows/npm-build.yml`

**Step 1: Confirm the file does not already exist**

```bash
test ! -f AGENTS.md
```

Expected: exit code 0

**Step 2: Write the repo-local operating contract**

Add `AGENTS.md` with:

- mission and scope for backlog reduction
- repo map for `src/`, `interfaces/python/`, `interfaces/js/`, `test/`, and `docs/`
- default work unit for issue-driven changes
- risk policy summary
- verification summary with references to `docs/automation/verification-matrix.md`
- GitHub write permissions
- environment rules for local vs Codex Cloud
- repo-specific `do not guess` rules and escalation triggers

**Step 3: Verify the key sections exist**

```bash
rg -n "Mission|Repo Map|Risk Policy|Verification|GitHub Contract|Environment" AGENTS.md
```

Expected: matching headings are found

**Step 4: Review for scope discipline**

Check that the file is short, specific to this repo, and does not duplicate large chunks of the global `/Users/anton/kod/AGENTS.md`.

**Step 5: Commit**

```bash
git add AGENTS.md
git commit -m "docs: add infomap agent contract"
```

### Task 2: Add Automation Risk Rubric

**Files:**
- Create: `docs/automation/risk-rubric.md`
- Reference: `AGENTS.md`
- Reference: `docs/plans/2026-04-02-agent-automation-design.md`

**Step 1: Confirm the file does not already exist**

```bash
test ! -f docs/automation/risk-rubric.md
```

Expected: exit code 0

**Step 2: Write the risk rubric**

Add classification rules for:

- `low`
- `medium`
- `high`
- `blocked`

For each class, include:

- typical issue types
- common file surfaces
- allowed automation behavior
- required escalation conditions

**Step 3: Verify each class is present**

```bash
rg -n "^## (Low|Medium|High|Blocked)" docs/automation/risk-rubric.md
```

Expected: four section headings are found

**Step 4: Cross-check PR policy alignment**

Ensure the rubric explicitly states:

- `low` may open `ready` PRs if verification passes
- `medium` may open only `draft` PRs
- `high` and `blocked` may not open implementation PRs automatically

**Step 5: Commit**

```bash
git add docs/automation/risk-rubric.md
git commit -m "docs: add automation risk rubric"
```

### Task 3: Add Verification Matrix

**Files:**
- Create: `docs/automation/verification-matrix.md`
- Reference: `Makefile`
- Reference: `.github/workflows/build.yml`
- Reference: `.github/workflows/python-build.yml`
- Reference: `.github/workflows/npm-build.yml`

**Step 1: Confirm the file does not already exist**

```bash
test ! -f docs/automation/verification-matrix.md
```

Expected: exit code 0

**Step 2: Write the matrix**

Map change surfaces to minimum required commands, including at least:

- `src/` -> `make`
- Python wrapper/package changes -> `make python`, `make py-test`
- JS worker/package changes -> `npm ci`, `npm run build` or `make js-worker`
- docs-only changes -> minimal or no code verification
- CI/release changes -> default `draft` handling and limited local verification

**Step 3: Verify the expected commands are documented**

```bash
rg -n "make python|make py-test|make js-worker|npm run build|make$" docs/automation/verification-matrix.md
```

Expected: all required commands are present

**Step 4: Check for over-verification**

Ensure the document tells agents to run the smallest sufficient verification by default, not the broadest available suite.

**Step 5: Commit**

```bash
git add docs/automation/verification-matrix.md
git commit -m "docs: add automation verification matrix"
```

### Task 4: Add GitHub Contract And Templates

**Files:**
- Create: `docs/automation/github-contract.md`
- Create: `docs/automation/issue-triage-template.md`
- Create: `docs/automation/pr-template.md`
- Reference: `docs/automation/risk-rubric.md`
- Reference: `docs/automation/verification-matrix.md`

**Step 1: Confirm the files do not already exist**

```bash
test ! -f docs/automation/github-contract.md
test ! -f docs/automation/issue-triage-template.md
test ! -f docs/automation/pr-template.md
```

Expected: exit code 0

**Step 2: Write the GitHub contract**

Document:

- which labels automation may add or update
- what triage comments must contain
- when PRs may be `ready` versus `draft`
- what automation must never do on GitHub

**Step 3: Write the templates**

Add:

- a triage comment template with risk, scope, verification, and status
- a PR template with issue link, change summary, verification, and residual risk

**Step 4: Verify template fields exist**

```bash
rg -n "Risk|Verification|Residual Risk|Issue" docs/automation/github-contract.md docs/automation/issue-triage-template.md docs/automation/pr-template.md
```

Expected: all required fields are present

**Step 5: Commit**

```bash
git add docs/automation/github-contract.md docs/automation/issue-triage-template.md docs/automation/pr-template.md
git commit -m "docs: add GitHub automation contract"
```

### Task 5: Link The New Policy Surface

**Files:**
- Modify: `README.rst`
- Reference: `AGENTS.md`
- Reference: `docs/automation/github-contract.md`

**Step 1: Write the failing check**

```bash
rg -n "AGENTS.md|automation" README.rst
```

Expected: no matches

**Step 2: Add a short maintainer section**

Add a concise section pointing maintainers and agents to:

- `AGENTS.md`
- `docs/automation/`

Keep it brief and maintainership-focused.

**Step 3: Verify the new references exist**

```bash
rg -n "AGENTS.md|docs/automation" README.rst
```

Expected: matching lines are found

**Step 4: Review for user-facing clarity**

Check that the section is useful for maintainers but does not distract from installation and user documentation.

**Step 5: Commit**

```bash
git add README.rst
git commit -m "docs: link agent automation policies"
```

### Task 6: Final Validation Pass

**Files:**
- Reference: `AGENTS.md`
- Reference: `docs/automation/risk-rubric.md`
- Reference: `docs/automation/verification-matrix.md`
- Reference: `docs/automation/github-contract.md`
- Reference: `docs/automation/issue-triage-template.md`
- Reference: `docs/automation/pr-template.md`
- Reference: `README.rst`

**Step 1: List the policy files**

```bash
find AGENTS.md docs/automation -maxdepth 1 -type f | sort
```

Expected: all expected files are listed

**Step 2: Run a fast content sanity check**

```bash
rg -n "ready|draft|blocked|Codex Cloud|do not guess" AGENTS.md docs/automation README.rst
```

Expected: the policy terms are present where expected

**Step 3: Review diffs**

```bash
git diff -- AGENTS.md README.rst docs/automation docs/plans/2026-04-02-agent-automation-design.md docs/plans/2026-04-02-agent-automation-implementation.md
```

Expected: diff is limited to the new agent-policy surface

**Step 4: Run git status**

```bash
git status --short
```

Expected: only intended files are modified or added, aside from any pre-existing unrelated changes

**Step 5: Commit**

```bash
git add AGENTS.md README.rst docs/automation docs/plans/2026-04-02-agent-automation-design.md docs/plans/2026-04-02-agent-automation-implementation.md
git commit -m "docs: prepare repo for issue-driven agents"
```
