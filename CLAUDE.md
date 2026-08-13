## Columnar core
When working on the new columnar core (wip branch `columnar-hierarchical-core`), remember to:
- append findings to the log in `columnar_wip/columnar-rethink-notes.md`
- run benchmark networks in `columnar_wip/benchmark-networks.md` to keep all the numbers in `columnar_wip/columnar-pr-performance-section.md` up-to-date for each PR, and report the differences to previous numbers of the columnar core in the chat and in the PR body.
- report all cases where it performs more than 0.1% worse in codelength or 1% worse in speed relative previous numbers and explain those. If a systematic bias due to changes in how busy the environment is, the codelength should be same and columnar vs OO speed ratio should be same within a margin.
- Marginal win in codelength should not cost non-marginal loss in speed unless behind a speed-to-quality trade-off flag, but always report and ask before accepting such trade-off.
- if a PR includes more than one optimization feature, report the individual contribution of each feature on at least a relevant subset.
- if adding cross-trial or post-trial passes that may slow down single trial runs but not noticeable on ten trials, report how single-trial run times changes.