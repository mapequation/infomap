## Columnar core
When working on the new columnar core (wip branch `columnar-hierarchical-core`), remember to:
- append findings to the log in `columnar_wip/columnar-rethink-notes.md`
- run benchmark networks in `columnar_wip/benchmark-networks.md` to keep all the numbers in `columnar_wip/columnar-pr-performance-section.md` up-to-date for each PR, and report the differences to previous numbers of the columnar core in the chat and in the PR body.
- report all cases where it performs more than 0.1% worse in codelength or 1% worse in speed relative previous numbers and explain those. If a systematic bias due to changes in how busy the environment is, the codelength should be same and columnar vs OO speed ratio should be same within a margin.
- Marginal win in codelength should not cost non-marginal loss in speed unless behind a speed-to-quality trade-off flag, but always report and ask before accepting such trade-off.
- if a PR includes more than one optimization feature, report the individual contribution of each feature on at least a relevant subset.
- if adding cross-trial or post-trial passes that may slow down single trial runs but not noticeable on ten trials, report how single-trial run times changes.

### The two `columnar_wip` documents have different jobs
- **`columnar-pr-performance-section.md` is a SNAPSHOT, not a changelog.** It is the body of whichever
  PR is currently under review, and it holds exactly two things: the current numbers, and the evidence
  for the change being reviewed. **Its opening note must name the PR — what the change is, not just
  that the numbers are fresh.** "Every number here is fresh" is unfalsifiable on its own; "measured on
  the binary for *<this change>*" is the claim a reviewer can check, and it is what makes the numbers
  evidence *for this PR* rather than numbers that merely happen to sit next to it. When a sub-PR merges, the next one **replaces** its section — it does
  not accumulate. Per-feature attribution for an already-merged feature stays reachable by opening that
  sub-PR on GitHub, where that PR's own copy of this file is the measurement that justified it. So:
  **delete sections belonging to already-merged sub-PRs rather than relabelling them.** Stale numbers
  left in the snapshot are the only thing that makes it confusing.
- **`columnar-rethink-notes.md` is the LOG.** Findings accumulate here, numbered F1, F2, … Nothing is
  removed. Corrections to an earlier finding go in as an addendum to that finding (e.g. "F27 addendum")
  rather than by editing the original — the wrong reasoning is part of the record and is often the
  useful part.
- `columnar-search-runs.tsv` is the row-per-run log; tag each batch and read the `batch` column before
  comparing times across sessions.

### Syncing with master: merge, do not rebase — and do it through a PR
Sub-PRs are rebase-merged into `columnar-hierarchical-core`, so each one's recorded merge commit **is**
a commit on this branch. A rebase rewrites all of them (measured once: 13 of 13 detached), leaving
those PRs marked merged into a history that no longer contains them, and it silently re-parents every
old measurement so it reads as current. `git merge master` keeps the provenance and gives the numbers
an explicit baseline boundary. Redo the perf snapshot after any sync — a master change can move both
engines' codelengths (e.g. a change to per-trial seeding) without touching the search.

**A sync is a change to this branch like any other: give it a branch, a PR and a perf snapshot.**
The first sync was merged straight onto the branch and pushed it **red** — three master gates this
branch had never satisfied surfaced only afterwards. A PR runs CI on the merged result *before* it
lands, and gives the sync a discoverable snapshot instead of burying the evidence in a commit message.
The recipe:

```
git worktree add -b sync-master-into-columnar .claude/worktrees/sync origin/columnar-hierarchical-core
cd .claude/worktrees/sync && git merge --no-ff origin/master        # resolve, do NOT commit yet
make test-native MODE=release OPENMP=1        # all three configs BEFORE committing the merge
make test-native MODE=release OPENMP=0
make test-native MODE=release OPENMP=1 FEATURES="lossy-map-equation regularized-multilayer"
git commit                                     # then refresh the perf snapshot, then open the PR
gh pr create --base columnar-hierarchical-core --head sync-master-into-columnar
```

Land it with a **fast-forward push**, not `gh pr merge` — see the API constraints below. GitHub marks
the PR merged once its head is reachable from the base:
`git push origin origin/sync-master-into-columnar:columnar-hierarchical-core`

### What the GitHub API allows here (checked, not assumed)
`gh api repos/mapequation/infomap` reports **`allow_merge_commit: false`**, `allow_squash_merge: true`,
`allow_rebase_merge: true`, `allow_auto_merge: false`, `delete_branch_on_merge: true`. Consequences:

- **`gh pr merge --merge` always fails** with "Merge commits are not allowed on this repository". That
  is the repo setting, not a branch rule. For an ordinary sub-PR use `--squash` or `--rebase`; for a
  **sync PR use neither** — both would flatten master out of the branch and break the "master is an
  ancestor" property the next sync depends on. Fast-forward push instead.
- **Stacked PRs cannot be merged through the API at all.** When a PR's base is a branch that itself has
  an open PR (any sub-PR of `columnar-hierarchical-core`, since #808 is open), both `gh pr merge` and
  `PUT /pulls/{n}/merge` refuse with "must be merged using the asynchronous merge REST API", and
  `POST /pulls/{n}/merge-async` 404s via `gh api`. Land those the same way: merge locally onto the base
  (`--no-ff` for a real merge commit) and push.
- `master` is **protected**; `columnar-hierarchical-core` is **not**, so direct pushes to the wip branch
  work and are how syncs and sub-PR merges actually land. Changes to master still go through a PR.
- `delete_branch_on_merge` is on, so a merged sub-PR's remote branch disappears by itself — clean up the
  local worktree and branch, but do not expect to have to delete the remote.

### Orientation for a fresh session
- The wip branch is `columnar-hierarchical-core`, PR **#808** into master, additive behind
  `--columnar`/`-C`. Sub-PRs target the wip branch, not master. A fix to *shared* code (anything
  outside `Columnar*`) belongs on **master** even when only the columnar engine feels it — e.g. #991,
  where `initTree` was master's function but the per-trial caller was columnar's.
- Do columnar work in `.claude/worktrees/*`, not the main worktree.
- Build: `make build-native MODE=release OPENMP=0` for benchmarks (single-thread is the measurement
  convention), `OPENMP=1` for tests. `make test-native` needs python `jsonschema` + `referencing` in
  the interpreter **CMake picks** (check `build/cmake/CMakeCache.txt` for `_Python3_EXECUTABLE`; it is
  not necessarily the one on PATH).
- Regenerated artifacts are CI-gated. After any option/SWIG/description change run
  `make build-r-swig build-python-swig build-binding-options build-js-metadata` and commit the results.
  `make build-r-man` needs roxygen2 **7.3.3** exactly and refuses to run with another version; install
  the pin into a temp lib and pass `R_LIBS_USER=<tmpdir>` rather than downgrading the user's library.
- A new result-affecting CLI option must also be added to the config fingerprint in
  `src/io/RunMetadata.cpp`, or the python coverage test fails.
- `make format-native` before pushing; the pre-commit CI gate runs clang-format and will fail on
  hand-written blocks.

### Verification discipline
- **Run the C++ tests with OpenMP ON as well as OFF.** `make test-native OPENMP=0` is the benchmark
  configuration, not the test configuration: every `#ifdef _OPENMP` test body — which is where the
  parallel-trials contracts live — compiles away without it, so a green run says nothing about them.
  CI runs `make test-native` with OpenMP on and will catch what a local `OPENMP=0` run cannot.
- Run the feature build too (`FEATURES="lossy-map-equation regularized-multilayer"`), which CI does as
  a second job.

### Measurement discipline
- **Establish that a difference is real before explaining it.** If a number fails to reproduce,
  rebuild the *original* pair (old base + old tip) and measure both arms in one session first. That
  distinguishes "the effect moved" from "the number was wrong". A plausible mechanism makes a number
  look explained and stops the investigation.
- **Profile before attributing.** A regression with bit-identical output is a localised cost; `sample`
  finds it in one run. Kill hypotheses with builds (gate the suspect off, rebuild, measure), not with
  arguments about which upstream commit looks expensive.
- **Check the arithmetic closes** before accepting a cause, and quote a spread when sessions disagree
  rather than one figure plus a rationalisation.
- Use `-N1` to compare code speed: it is the only point where two binaries compute the same partition,
  so a time difference is code and nothing else.
- Never leave a measurement gate (an env-var bypass of a check) in committed code — build those in a
  throwaway worktree.
