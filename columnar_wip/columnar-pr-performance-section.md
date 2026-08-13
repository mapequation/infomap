## Performance

> Manual benchmark comparing the two engines on the same binary. This is **not** the CI `perf-pr.yml` check (which only sees the default OO path, since the new core is flag-gated) — it's included here so reviewers can see how the opt-in `--columnar` engine compares to the default.

Single-threaded (`MODE=release OPENMP=0`), `--seed 123 -N10` — **best of 10 trials**, the way Infomap is normally run. Codelength in bits; `time` = total wall for all 10 trials, minimum of 3 interleaved repetitions; `top`/`lvls` = top modules / levels of the best partition. The network set spans base (undirected + directed), metadata, multilayer and state/memory objectives; see [`columnar_wip/benchmark-networks.md`](columnar_wip/benchmark-networks.md) for paths and full details. Columnar column = the default columnar search (`-C`).

> **Fully re-measured after merging master in** (all 65 configs × 3 interleaved repetitions, one idle-machine session). **Every codelength in this section moved, on both engines**, and none of it is a search change: master's [#949](https://github.com/mapequation/infomap/pull/949) now seeds every trial the same way in every mode, so trials 1..9 of an `-N10` run draw a different sequence than before. Two controls pin that down — **`-N1` is bit-identical across the sync** on all 8 networks checked (the columnar search itself is untouched), and **master's OO output equals this branch's OO output** on all 7 OO configs checked (the branch is exactly neutral on the OO path, so every OO shift arrived with master). The per-seed spread swamps the per-cell shift in both directions: over seeds 123/234/345/456 the old→new `-C` delta averages +0.04% on netsci, −0.04% on malaria and +0.06% on powergrid. Master's `7da08cfb` also hoists per-node-constant plogp out of the **OO** move loop, so every columnar-vs-OO *speed ratio* below is against a faster baseline than the previous refresh. One real cost arrives too — master's #948 validates cluster-data tree shape inside `initTree`, which the columnar engine calls **per trial** — costing web-NotreDame ~5.5pp of its +7.5% and nothing measurable below ~10k nodes; the master-sync section below measures it and names the fix.

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="3">Meta</th>
<th colspan="4">OO (default)</th>
<th colspan="4">columnar (<code>-C</code>)</th>
</tr>
<tr>
<th>nodes</th><th>objective</th><th>flags</th>
<th>codelength</th><th>time</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>top</th><th>lvls</th>
</tr>
</thead>
<tbody>
<tr><td>ninetriangles</td><td align="right">27</td><td>base</td><td>—</td><td align="right">3.38583082</td><td align="right">0.02s</td><td align="right">3</td><td align="right">3</td><td align="right"><b>3.38583082</b> (=)</td><td align="right">0.01s</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">198</td><td>base</td><td>—</td><td align="right">6.86304747</td><td align="right">0.03s</td><td align="right">5</td><td align="right">2</td><td align="right"><b>6.86275593</b> (−0.004%)</td><td align="right">0.02s</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">552</td><td>base</td><td>—</td><td align="right">4.04354934</td><td align="right">0.13s</td><td align="right">2</td><td align="right">5</td><td align="right">4.05454025 (+0.27%)</td><td align="right">0.03s (−74%)</td><td align="right">2</td><td align="right">4</td></tr>
<tr><td>powergrid</td><td align="right">4 941</td><td>base</td><td>—</td><td align="right">4.75872920</td><td align="right">1.88s</td><td align="right">5</td><td align="right">7</td><td align="right"><b>4.74107206</b> (−0.37%)</td><td align="right">0.26s (−86%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td>politicalblogs</td><td align="right">1 046</td><td>base</td><td><code>-d</code></td><td align="right">6.73892798</td><td align="right">0.15s</td><td align="right">80</td><td align="right">3</td><td align="right">6.74094314 (+0.03%)</td><td align="right">0.07s (−51%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td>science2001</td><td align="right">7 170</td><td>base</td><td><code>-d</code></td><td align="right">7.83638921</td><td align="right">7.66s</td><td align="right">11</td><td align="right">4</td><td align="right"><b>7.83343660</b> (−0.04%)</td><td align="right">3.15s (−59%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>web-NotreDame</td><td align="right">325 729</td><td>base</td><td><code>-d</code></td><td align="right">5.56592477</td><td align="right">144.2s</td><td align="right">17</td><td align="right">13</td><td align="right">5.56852929 (+0.05%)</td><td align="right">20.6s (−86%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td>lazega</td><td align="right">69</td><td>metadata</td><td>—</td><td align="right">6.01786027</td><td align="right">0.03s</td><td align="right">7</td><td align="right">2</td><td align="right"><b>6.01786027</b> (=)</td><td align="right">0.01s</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">5</td><td>multilayer</td><td>—</td><td align="right">2.01140524</td><td align="right">0.01s</td><td align="right">2</td><td align="right">2</td><td align="right"><b>2.01140524</b> (=)</td><td align="right">0.01s</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">307·9L</td><td>multilayer</td><td>—</td><td align="right">7.50242050</td><td align="right">8.89s</td><td align="right">142</td><td align="right">3</td><td align="right"><b>7.39750171</b> (−1.40%)</td><td align="right">3.17s (−64%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>air30k</td><td align="right">13 213</td><td>state/memory</td><td>—</td><td align="right">5.39287115</td><td align="right">11.3s</td><td align="right">16</td><td align="right">4</td><td align="right"><b>5.39242541</b> (−0.008%)</td><td align="right">3.80s (−66%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">13 213</td><td>state/memory</td><td><code>-d --regularized</code></td><td align="right">5.57843563</td><td align="right">7.42s</td><td align="right">301</td><td align="right">3</td><td align="right"><b>5.57624241</b> (−0.04%)</td><td align="right">4.06s (−45%)</td><td align="right">11</td><td align="right">3</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">7 170</td><td>base</td><td><code>-d --preferred-number-of-modules 25</code></td><td align="right">7.94035360</td><td align="right">7.88s</td><td align="right">25</td><td align="right">4</td><td align="right">8.23558553 (+3.72%)</td><td align="right">3.35s (−58%)</td><td align="right">25</td><td align="right">2</td></tr>
</tbody>
</table>

`nodes`: state nodes for air30k (13 213 states over 183 physical); physical·layers for malaria. **Bold** = columnar beats or exactly ties OO. Parentheses on the columnar `-C` columns = change vs OO (`(=)` = bit-identical; negative time = faster).

**Reading the table**
- **Codelength** — columnar `-C` **ties or beats OO on 9 of 13 configs**: it beats OO on malaria (−1.40%), powergrid (−0.37%), science2001 (−0.04%), regularized air30k (−0.04%), air30k (−0.008%) and jazz (−0.004%), and ties lazega and the toys. Remaining gaps: pref-mods +3.72% (the `|K − K_pref|` bias is leaf-loop-only, #827), netsci +0.27%, web-NotreDame +0.05%, politicalblogs +0.03%. Regularized air30k crossed from +0.007% behind to ahead, and jazz from an exact tie to a win — both within the per-seed spread, so read them as ties that landed on the good side of this seed rather than as new wins.
- **The web-NotreDame gap is a user choice.** At the default knee it is +0.05%. Adding `--tune-iteration-relative-threshold 1e-3` takes it to 5.56067487 — **−0.094%, ahead of OO** — for +30% wall; `0` (full convergence) reaches 5.55881205, −0.128% ahead, for +91%. F26 explains why the deeper refinement is a dial rather than the default.
- **Speed** — columnar is faster on every non-trivial network: ~1.8× on regularized air30k, ~2.4× on science2001 and pref-mods, ~2.8–3.0× on malaria and air30k, ~7× on powergrid and web-NotreDame (20.6s vs 144.2s). The multipliers are lower than the previous refresh on the OO-heavy rows because master's `7da08cfb` made the **OO** baseline 5–13% faster, not because columnar slowed.
- **Shape** — columnar produces leaner, shallower maps (web-NotreDame: 5 top modules / 6 levels vs OO's 17 / 13)

### The fast dial `-F`

`-F` (`--fast-hierarchical-solution`) skips the interior-layer refinement and instead does a single bottom re-partition within grandparents plus the module-level coarsening loop. The flat-first trials apply to `-F` too (same probe, same gating), which is why it now matches `-C` on every flat-winning network.

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="4">columnar <code>-C</code></th>
<th colspan="4">columnar <code>-F</code></th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>top</th><th>lvls</th>
</tr>
</thead>
<tbody>
<tr><td>ninetriangles</td><td align="right">3.38583082</td><td align="right">0.01s</td><td align="right">3</td><td align="right">3</td><td align="right">3.38583082 (=)</td><td align="right">0.01s</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">6.86275593</td><td align="right">0.02s</td><td align="right">6</td><td align="right">2</td><td align="right">6.86275593 (=)</td><td align="right">0.02s</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.05454025</td><td align="right">0.03s</td><td align="right">2</td><td align="right">4</td><td align="right">4.06300588 (+0.21%)</td><td align="right">0.03s</td><td align="right">4</td><td align="right">4</td></tr>
<tr><td>powergrid</td><td align="right">4.74107206</td><td align="right">0.26s</td><td align="right">5</td><td align="right">5</td><td align="right">4.77402224 (+0.69%)</td><td align="right">0.17s (−35%)</td><td align="right">4</td><td align="right">5</td></tr>
<tr><td>politicalblogs (<code>-d</code>)</td><td align="right">6.74094314</td><td align="right">0.07s</td><td align="right">81</td><td align="right">2</td><td align="right">6.74094314 (=)</td><td align="right">0.07s</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td>science2001 (<code>-d</code>)</td><td align="right">7.83343660</td><td align="right">3.15s</td><td align="right">15</td><td align="right">3</td><td align="right">7.83343660 (=)</td><td align="right">3.01s (−4%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>web-NotreDame (<code>-d</code>)</td><td align="right">5.56852929</td><td align="right">20.6s</td><td align="right">5</td><td align="right">6</td><td align="right">5.62506198 (+1.02%)</td><td align="right">15.5s (−25%)</td><td align="right">2</td><td align="right">5</td></tr>
<tr><td>lazega (meta)</td><td align="right">6.01786027</td><td align="right">0.01s</td><td align="right">7</td><td align="right">2</td><td align="right">6.01786027 (=)</td><td align="right">0.01s</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.01s</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.01s</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.39750171</td><td align="right">3.17s</td><td align="right">2</td><td align="right">3</td><td align="right">7.39750171 (=)</td><td align="right">3.17s</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>air30k (states)</td><td align="right">5.39242541</td><td align="right">3.80s</td><td align="right">22</td><td align="right">3</td><td align="right">5.39242541 (=)</td><td align="right">3.57s (−6%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57624241</td><td align="right">4.06s</td><td align="right">11</td><td align="right">3</td><td align="right">5.57624241 (=)</td><td align="right">3.58s (−12%)</td><td align="right">11</td><td align="right">3</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">8.23558553</td><td align="right">3.35s</td><td align="right">25</td><td align="right">2</td><td align="right">8.23558553 (=)</td><td align="right">3.23s (−4%)</td><td align="right">25</td><td align="right">2</td></tr>
</tbody>
</table>

Parentheses on the columnar `-F` columns = change vs `-C` (`(=)` = bit-identical; negative time = faster); both columns are from the current same-session re-measurement. Sub-0.1s toy times are shown without a percentage (measurement floor).

**`-F` still ties `-C` on 10 of 13 configs** — including pref-mods, where it previously lost another +2.3% — because the flat-first trials carry the same winning partitions into both searches. The dial only bites on the base networks with real deep hierarchy, where skipping the interior refinement trades codelength for speed — and the F23 knee widens that trade, because it deepens exactly the refinement `-F` skips: web-NotreDame **+1.02% for −25% time**, powergrid +0.69% for −35%, netsci +0.21%. Partial seeding widens the dial further, because it improves `-C` on exactly the three networks `-F` cannot follow it on (the re-refine gate makes `-F` bit-identical by construction). On the flat-winning networks `-F` is 0–12% faster than `-C`, because `-C`'s expensive pass — the interior refinement — is exactly the one the flat bottom no longer needs (below). The tie set is unchanged by the sync; only the three dial rows move, and they move because both arms drew a new trial sequence (see the note at the top).

The columnar interior refinement stops at a diminishing-returns knee (default `--tune-iteration-relative-threshold` = 5e-3). On the two deep base networks, lowering it to 1e-3 buys 0.05–0.14% of codelength for 23–30% more time; shallow networks are structurally unaffected. See the knee section below.

### Merging master in: what moved, and why none of it is the engine (F28)

**Merged, not rebased, and that is a deliberate choice.** Thirteen sub-PRs have been merged into this
branch (#823, #833, #835, #868, #874, #879, #880, #890, #891, #983, #985, #987, #988), each by
rebase-merge — so each one's recorded merge commit *is* a commit on this branch. A rebase rewrites all
of them: checked, **13 of 13** would end up detached, leaving those PRs marked merged into a history
that no longer contains them. The merge commit also gives the numbers a boundary to hang on: every
per-feature table below it was measured against the pre-sync baseline, and the tables at the top
against the new one.

Master was merged in after 122 upstream commits. Three of them touch every number in
this section, and none of them is a columnar change:

- **[#949](https://github.com/mapequation/infomap/pull/949) — seed every trial the same way, in every mode.**
  The serial path used to reseed only in "sharding mode"; it now reseeds unconditionally from
  `baseSeed + trialOffset + trialIndex`, which is exactly the formula the columnar branch already used
  in the parallel path. Trials 1..9 of an `-N10` run therefore draw a different sequence than before,
  on **both** engines. This is the whole story behind the codelength movement.
- **`7da08cfb` — plogp hoisted out of the OO move-loop delta.** The OO baseline is 5–13% faster, so
  every columnar-vs-OO speed multiplier is quoted against a faster reference.
- **[#948](https://github.com/mapequation/infomap/pull/948) (`d55c2365`) — cluster-data tree-shape validation.**
  Does not touch the columnar core, but adds work to a shared function the columnar engine calls per
  trial. Accounts for about three quarters of the one network-scale slowdown. See below.

Two controls separate "the seeds moved" from "the search changed", and both come out clean:

| control | result |
|---|---|
| `-C -N1`, pre-sync core vs post-sync core | **bit-identical codelength on 8/8 networks** — trial 0 is unaffected by #949, so the search is provably untouched |
| OO `-N10`, plain master vs post-sync core | **identical on 7/7 networks** — the branch is exactly neutral on the OO path, so every OO shift arrived with master |

That leaves the per-cell `-N10` differences as re-draws from an unchanged distribution, which the seed
sweep confirms — old→new `-C` deltas over seeds 123/234/345/456:

| network | 123 | 234 | 345 | 456 | mean |
|---|--:|--:|--:|--:|--:|
| netscicoauthor2010 | +0.122% | +0.009% | −0.034% | +0.058% | **+0.039%** |
| malaria | −0.265% | +0.143% | +0.110% | −0.145% | **−0.039%** |
| powergrid | +0.031% | +0.056% | +0.015% | +0.135% | **+0.059%** |

Every mean is inside ±0.06% and the per-seed spread is larger than the shift, so no network changed
quality. The two cells that clear 0.1% at seed 123 — netsci `-C` +0.122% and malaria `-C` −0.265% —
are the tails of exactly that spread, in opposite directions.

**Speed, measured where the partition is bit-identical** (`-C -N1`, so the only variable is code;
min-of-5 to min-of-7, interleaved): malaria −2.0%, air30k −2.6%, regularized air30k ±0, powergrid one
timer tick, science2001 +2.0%, **web-NotreDame +4.7%** (`-N10`: +6.7%). Only web-NotreDame is worth
explaining, and the timing registry localises it exactly — `flow_calculation_s` 0.315 → 0.313 and
`init_network_s` 0.153 → 0.142 are flat, while `trial_optimize_s` goes **1.943 → 2.073 (+6.7%)**.

The cause is **#948's cluster-data tree-shape validation**. To be precise about what that means,
because "a cluster-data fix slowed the columnar engine" sounds wrong: **#948 does not touch the
columnar core at all** — it changed five files, none of them `Columnar*`. What it changed is the
*shared* `InfomapBase::initTree`, and the columnar engine is the one caller that invokes `initTree`
**per trial**:

| `initTree` caller | when |
|---|---|
| `columnarPartition` | **every trial** — materializes `opt.toNodePaths(...)` into the InfoNode tree |
| `initTrialPartition` | per trial, but only when the input carries embedded JSON initial-partition paths |
| `initPartition(cluster file)`, `restoreBestResult`, `maybeDeepRepairBest` | once per run |

That per-trial call is how the columnar result becomes an output tree; it is **pre-existing on this
branch, not something the sync introduced** (it is at `InfomapBase.cpp:2023` on the pre-sync tip). The
sync only made the function it calls more expensive. On ordinary input OO reaches none of the
per-trial rows, which is why OO gets *faster* across the sync while `-C` gets slower.

| binary (web-NotreDame `-C -N1`, `trial_optimize_s`, min-of-5) | | |
|---|--:|--:|
| pre-sync core | 1.953s | — |
| post-sync core | 2.100s | +7.5% |
| post-sync core, #948 check disabled | 1.992s | **+2.0%** |

`initTree` now builds two `std::map<Path, bool>` keyed by `std::vector<unsigned int>`, one entry per
path prefix per leaf, to reject cluster data that gives a module both a leaf and a sub-module as
children. That is the right check for user-supplied `--cluster-data`. But what `columnarPartition`
hands it is Infomap's *own* output, which cannot mix depths by construction — so on 325 729 leaves it
is ~1.6M map insertions per trial for a condition that can never hold.

#948 made three additions that a columnar trial can reach, and they do not fall the same way on every
search. Each gated off independently (web-NotreDame `-N1`, `trial_optimize_s`, min-of-5, codelength
identical on every arm; the parts do not sum exactly to the total at these magnitudes):

| addition | `-C` | `-C -F` | `-2 -C` |
|---|--:|--:|--:|
| `initTree` mixed-depth shape check | **+6.9%** | **+8.2%** | +0.6% |
| `removeSubModules` loop condition (`numLevels()` → full child scan) | −0.7% | +0.4% | +1.2% |
| `aggregatePerLevelCodelength` per-child classification | +0.3% | +2.1% | +1.8% |
| **all three** | **+8.0%** | **+8.8%** | **+2.5%** |

So the shape check owns the hierarchical searches and is **not** what `-2 -C` pays: the two-level path
leaves `initTree` early for `initPartition` and never reaches the check, and its +2.5% is the other
two additions. science2001 (7 170 nodes) and air30k are inside noise on all of them, so the cost
scales with leaf count × depth as the shape predicts.

Two things this is *not*, both checked rather than assumed: it is **not** the #958/#961/#963 flow
post-conditions (gated off in a measurement build, they cost **0.003s — +0.1%** of the trial), and it
is **not** #954's iterator change (reverting it leaves the regression intact). Nothing is disabled on
this branch — those gates existed only in throwaway measurement builds. **A cheap fix exists** (skip the shape validation when `initTree` is handed
engine-generated paths rather than parsed cluster data); it is left for a follow-up rather than folded
into a master sync, and the residual +2.0% is inside code-layout noise.

### The refine knee stays at 5e-3, and 1e-3 is a dial (F23 → F26)

`ColumnarTwoLevel::m_minRelTuneImprovement` stops the interior-layer refinement once a whole up/down
sweep gains less than this fraction of the post-build codelength. The shipped default is **5e-3**, and
the deeper setting is offered as a dial rather than defaulted — it is real quality that more trials
cannot buy (at matched CPU on web-NotreDame, 5e-3 saturates at 5.5727 by `-N12` while 1e-3 reaches
5.5674), but not worth a fifth more CPU by default. Re-measured at this tip:

| `--tune-iteration-relative-threshold` | web-NotreDame `-C` | vs default | wall |
|---|--:|--:|--:|
| 5e-3 (default) | 5.56852929 | — | — |
| 1e-3 | **5.56067487** | −0.14% | +30% |
| 0 (full convergence) | **5.55881205** | −0.17% | +91% |

Only the two deep base networks react at all: `refineSweeps` is 1 for a stack with at most one
interior layer, so science2001/air30k/malaria cannot, and `-F` never enters `refineHierarchy`. (The
default was briefly changed to 1e-3 and reverted on a measurement error — F23/F26 in the findings
log.)

> **This section is a snapshot, not a changelog.** It carries the current numbers plus the evidence for
> the change under review. Per-feature attribution for features that already landed (partial seeding
> #985, flat-first #891, coarse-tune #890, the #875 hoist, the split operator #987/#988) lives in each
> of those PRs, whose own copy of this file is the measurement that justified it; the running narrative
> is in [`columnar_wip/columnar-rethink-notes.md`](columnar_wip/columnar-rethink-notes.md).

Every measured run behind this section is logged row-per-run in [`columnar_wip/columnar-search-runs.tsv`](columnar_wip/columnar-search-runs.tsv) for plotting the codelength/time frontier. Read the `batch` column before comparing times: session noise floors ranged ±3% to 13%, so the time axis is only comparable within a batch (`load1m`, `reps`, `agg` are recorded for that reason), and `derived=1` marks rows reconstructed from reported percentages rather than measured absolutes.

### Two-level clustering (`-2`)

`--two-level` is wired to the columnar engine on the `columnar-two-level` branch (PR #823): the full two-level optimize materialized as a two-level stack, followed by the correction-aware module-merge coarsening interleaved with a seeded leaf fine-tune until the pair stops improving, plus the #889 coarse-tune (in-trajectory repair every trial + deep repair of the winning trial). **No change to the `-2` code path since PR #890** — the codelengths below moved only with the sync's new trial sequence (see the note at the top), and every row is re-measured in the same session as the tables above.

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="3">OO <code>-2</code></th>
<th colspan="3">columnar <code>-2 -C</code></th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>top</th>
<th>codelength</th><th>time</th><th>top</th>
</tr>
</thead>
<tbody>
<tr><td>ninetriangles</td><td align="right">3.51775481</td><td align="right">0.01s</td><td align="right">9</td><td align="right"><b>3.51775481</b> (=)</td><td align="right">0.01s</td><td align="right">9</td></tr>
<tr><td>jazz</td><td align="right">6.86304747</td><td align="right">0.02s</td><td align="right">5</td><td align="right"><b>6.86122977</b> (−0.03%)</td><td align="right">0.02s</td><td align="right">6</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.28501267</td><td align="right">0.04s</td><td align="right">56</td><td align="right"><b>4.28307258</b> (−0.05%)</td><td align="right">0.02s</td><td align="right">59</td></tr>
<tr><td>powergrid</td><td align="right">5.60044386</td><td align="right">0.57s</td><td align="right">419</td><td align="right">5.63729688 (+0.66%)</td><td align="right">0.11s (−81%)</td><td align="right">419</td></tr>
<tr><td>politicalblogs (<code>-d</code>)</td><td align="right">6.73972141</td><td align="right">0.09s</td><td align="right">80</td><td align="right"><b>6.73957529</b> (−0.002%)</td><td align="right">0.06s</td><td align="right">81</td></tr>
<tr><td>science2001 (<code>-d</code>)</td><td align="right">7.95003960</td><td align="right">3.94s</td><td align="right">496</td><td align="right"><b>7.94997883</b> (−0.001%)</td><td align="right">2.44s (−38%)</td><td align="right">506</td></tr>
<tr><td>web-NotreDame (<code>-d</code>)</td><td align="right">6.74298853</td><td align="right">42.8s</td><td align="right">11809</td><td align="right">6.75421666 (+0.17%)</td><td align="right">18.5s (−57%)</td><td align="right">11991</td></tr>
<tr><td>lazega (meta)</td><td align="right">6.01786027</td><td align="right">0.02s</td><td align="right">7</td><td align="right"><b>6.01786027</b> (=)</td><td align="right">0.01s</td><td align="right">7</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.01s</td><td align="right">2</td><td align="right"><b>2.01140524</b> (=)</td><td align="right">0.01s</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.50595639</td><td align="right">6.47s</td><td align="right">142</td><td align="right"><b>7.40044538</b> (−1.41%)</td><td align="right">2.78s (−57%)</td><td align="right">168</td></tr>
<tr><td>air30k (states)</td><td align="right">5.39331278</td><td align="right">4.37s</td><td align="right">332</td><td align="right"><b>5.39305505</b> (−0.005%)</td><td align="right">3.45s (−21%)</td><td align="right">334</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57921689</td><td align="right">5.53s</td><td align="right">301</td><td align="right"><b>5.57557704</b> (−0.07%)</td><td align="right">3.63s (−34%)</td><td align="right">304</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">8.13096953</td><td align="right">5.84s</td><td align="right">25</td><td align="right">8.23558553 (+1.29%)</td><td align="right">3.06s (−48%)</td><td align="right">25</td></tr>
</tbody>
</table>

Parentheses on the columnar columns = change vs OO `-2` (**bold** = columnar beats or exactly ties OO). Columnar `-2` ties or beats OO on 10 of 13 configs, including *every* correction network; the three exceptions are base-objective configs (powergrid +0.66%, web-NotreDame +0.17%, pref-mods +1.29%), and it is faster on every non-trivial network (−21% to −81%).

