## Columnar core
When working on the new columnar core (wip branch `columnar-hierarchical-core`), remember to:
- append findings to the log in `columnar_wip/columnar-rethink-notes.md`
- run benchmark networks in `columnar_wip/benchmark-networks.md` to keep all the numbers in `columnar_wip/columnar-pr-performance-section.md` up-to-date for each PR, and report the differences to previous numbers of the columnar core in the chat and in the PR body.
- include **old-vs-new columnar comparison tables** in the snapshot, styled like the `-F` table: old
  columnar on the left, this PR's columnar on the right with parenthesized (±%) deltas against the
  old, same columns (codelength/time/top/lvls) — one table for the standard search (`-C`) and one for
  two-level (`-C -2`). **"Old" is a freshly built binary at the tip of `columnar-hierarchical-core`**
  (name its commit + md5 in the snapshot) — never an intermediate commit of the PR branch. Both arms
  measured in the same session, interleaved, with the same instrument: **`--timing-json`'s
  `timing.total_s`** (the engine's own wall time), NOT the process wall — process wall carries ~30 ms
  of startup that swamps every sub-0.1 s row and once made the unchanged tip read 0.04s where the
  snapshot said 0.01s.
- **Warn about and explain every cell where new is worse than old**, in codelength or in time. If a
  regression breaches the performance rules above (>0.1% worse in bits, or >1% worse in seconds
  without an offsetting gain on the other axis), do not just report it — **inspect whether there is a
  better way to solve the issue**. A significant time increase without a codelength gain is rejected
  automatically; it is not a trade-off to present.
- **Codelength and time are one trade-off — never report one without the other.** A codelength change
  quoted without the time change on the same rows is meaningless, and vice versa. Every mention of a
  gain / change / drift / regression — with or without a percentage — must make unambiguous from its
  immediate context which axis it is on, e.g. by writing the unit (bits, s) next to it.
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

### Where a change belongs
- **Shared code** — anything outside `Columnar*` — goes to **master through a PR**, even when only the
  columnar engine feels the bug. #991 (`initTree` validation) and #998 (#831, the objective's
  leaf-network terms) were both found through `-C` and both fixed on master.
- **Columnar-only code** goes to a **sub-PR into `columnar-hierarchical-core`**.
- A master fix reaches this branch **only by syncing master in** (below). **Never cherry-pick it here.**
  That leaves the same change committed twice, and every later sync has to reconcile the copies.

### Syncing master in: merge, through a PR, never rebase
Sub-PRs are rebase-merged into this branch, so each one's merge commit *is* a commit here. A rebase
detaches all of them (measured once: 13 of 13), leaving those PRs marked merged into a history that no
longer contains them, and silently re-parenting every old measurement so it reads as current. A merge
commit keeps the provenance and gives the numbers an explicit baseline boundary.

**A sync is a change like any other: branch, PR, perf snapshot.** The first sync went straight onto the
branch and pushed it red — three master gates this branch had never satisfied surfaced only afterwards.
A PR runs CI on the merged result *before* it lands. Always redo the snapshot: a master change can move
both engines' codelengths (e.g. per-trial seeding) without touching the search.

```
git worktree add -b sync-master-into-columnar .claude/worktrees/sync origin/columnar-hierarchical-core
cd .claude/worktrees/sync && git merge --no-ff origin/master     # resolve, do NOT commit yet
make test-native MODE=release OPENMP=1                           # all three BEFORE committing
make test-native MODE=release OPENMP=0
make test-native MODE=release OPENMP=1 FEATURES="lossy-map-equation regularized-multilayer"
git commit                      # then refresh the perf snapshot, then open the PR
gh pr create --base columnar-hierarchical-core --head sync-master-into-columnar
git push origin origin/sync-master-into-columnar:columnar-hierarchical-core   # land: fast-forward
```

Land it with that **fast-forward push**, not `gh pr merge`: GitHub marks the PR merged once its head is
reachable from the base, and both merge modes this repo allows would flatten master out of the branch,
breaking the "master is an ancestor" property the next sync depends on.

### What the GitHub API allows here (checked, not assumed)
`allow_merge_commit: false`, `allow_squash_merge: true`, `allow_rebase_merge: true`,
`allow_auto_merge: false`, `delete_branch_on_merge: true`.

- **`gh pr merge --merge` always fails** ("Merge commits are not allowed on this repository") — a repo
  setting, not a branch rule. Ordinary sub-PR: `--squash` or `--rebase`. Sync PR: neither, per above.
- **Stacked PRs cannot be merged through the API at all.** When the base is a branch with its own open
  PR (any sub-PR, since #808 is open), `gh pr merge` and `PUT /pulls/{n}/merge` demand the async merge
  API, and `POST /pulls/{n}/merge-async` 404s via `gh api`. Land those locally (`--no-ff`) and push.
- `master` is **protected**; `columnar-hierarchical-core` is **not**, so direct pushes are how syncs and
  sub-PR merges land here. Changes to master still go through a PR.
- `delete_branch_on_merge` is on: a merged sub-PR's remote branch disappears by itself — clean up the
  local worktree and branch only.

### Orientation for a fresh session
- The wip branch is `columnar-hierarchical-core`, PR **#808** into master, additive behind
  `--columnar`/`-C`. See "Where a change belongs" above for what targets the branch and what targets
  master.
- Do columnar work in `.claude/worktrees/*`, not the main worktree.
- Build: `make build-native MODE=release OPENMP=0` for benchmarks (single-thread is the measurement
  convention), `OPENMP=1` for tests. `make test-native` needs python `jsonschema` + `referencing` in
  the interpreter **CMake picks** (check `build/cmake/CMakeCache.txt` for `_Python3_EXECUTABLE`; it is
  not necessarily the one on PATH).
- **`make build-native` does not track header dependencies** — no `.d` file lists `InfomapBase.h`
  (#999). After editing any header, `rm -rf build/native` before building: an incremental build
  recompiles only the `.cpp` files whose own mtime changed, so a header that changes object layout
  (adding a member) leaves every other translation unit on the old layout. The binary then *runs* and
  prints correct codelengths while writing garbage (`# codelength 2.62515e-313`) to the tree file.
  `make test-native` is CMake and tracks headers correctly; only the native build is affected. Always
  `md5` the two binaries in an A/B — an incremental build that silently matched a clean one is how
  this was caught.
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
