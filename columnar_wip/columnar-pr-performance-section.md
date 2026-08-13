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
  This one is **columnar-only and per trial**, and it is the whole measurable cost. See below.

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

The cause is **#948's cluster-data tree-shape validation**, which the columnar engine pays *per trial*:

| binary (web-NotreDame `-C -N1`, `trial_optimize_s`, min-of-5) | | |
|---|--:|--:|
| pre-sync core | 1.953s | — |
| post-sync core | 2.100s | +7.5% |
| post-sync core, #948 check disabled | 1.992s | **+2.0%** |

`initTree` builds two `std::map<Path, bool>` keyed by `std::vector<unsigned int>`, one entry per path
prefix per leaf, to reject cluster data that gives a module both a leaf and a sub-module as children.
That is the right check for user-supplied `--cluster-data`. But `columnarPartition` calls `initTree`
**every trial** to materialize `opt.toNodePaths(...)` — Infomap's own output, which cannot mix depths
by construction — so on 325 729 leaves it is ~1.6M map insertions per trial, buying nothing. It costs
`-C` 5.5pp of its 7.5pp and `-C -F` 7pp of its 11.2pp; science2001 (7 170 nodes) and air30k are inside
noise, so the cost scales with leaf count × depth as the shape predicts.

Two things this is *not*, both checked rather than assumed: it is **not** the #958/#961/#963 flow
post-conditions (gated off in a measurement build, they cost **0.003s — +0.1%** of the trial), and it
is **not** #954's iterator change (reverting it leaves the regression intact). It is also **not** paid
by the OO engine, which does not call `initTree` per trial — OO on web-NotreDame is 5% *faster* after
the sync. **A cheap fix exists** (skip the shape validation when `initTree` is handed
engine-generated paths rather than parsed cluster data); it is left for a follow-up rather than folded
into a master sync, and the residual +2.0% is inside code-layout noise.

### The refine knee stays at 5e-3, and 1e-3 is a dial (F23 → F26)

`ColumnarTwoLevel::m_minRelTuneImprovement` stops the interior-layer refinement once a whole up/down
sweep gains less than this fraction of the post-build codelength. It is **5e-3 on this branch** — the same
value as before it. A change to 1e-3 was measured, shipped and then **reverted**: the A/B behind it ran
at load average ~20–25 and inflated the 5e-3 arm more than the 1e-3 arm, reporting the cost as +8.7%
when idle re-measurement puts it at **+20.3%** (F26 in the findings log; Daniel caught it by
cross-session arithmetic on OO times).

What survived the revert is that the deeper refinement is real and cannot be bought with more trials —
at matched CPU on web-NotreDame, 5e-3 saturates at 5.5727 by `-N12` while 1e-3 reaches 5.5674 for the
same budget. So it is offered rather than defaulted: `--tune-iteration-relative-threshold 1e-3` on
web-NotreDame gives **5.56067487 (−0.14%) for +30% wall**, and `0` (full convergence) **5.55881205
(−0.17%) for +91%** (re-measured at this branch's tip). Only the two deep base networks react at all —
`refineSweeps` is 1 for a stack with at most one interior layer, so science2001/air30k/malaria cannot,
and `-F` never enters `refineHierarchy`.

> The per-feature attribution tables in the sections below (F25 partial seeding, F27 split operator, flat-first trials, coarse-tune, and the #875 tele/meta hoist) are **as measured at the PR that landed each feature on this branch**, on the pre-#949 trial sequence. They are kept as the record of what each feature contributed when it landed; the headline tables above are the current numbers. The before/after *deltas* in those tables remain the right reading — both arms of each were measured in one session against one seeding scheme.

Every measured run behind this section is logged row-per-run in [`columnar_wip/columnar-search-runs.tsv`](columnar_wip/columnar-search-runs.tsv) for plotting the codelength/time frontier. Read the `batch` column before comparing times: session noise floors ranged ±3% to 13%, so the time axis is only comparable within a batch (`load1m`, `reps`, `agg` are recorded for that reason), and `derived=1` marks rows reconstructed from reported percentages rather than measured absolutes.

### Partial seeding: release the boundary, lock the module cores (F25, PR #985)

A grandparent re-refine in `refineLayerWithinGrandparent` had two settings and both waste something. **From singletons** (the default) rediscovers the entire sub-partition, including the module cores that were never in doubt, and it is the most expensive pass in the search. **Fully seeded** (`seedAssignment`: singletons, then deterministic placement back into the old module, then greedy) reproduces the current partition, so the gate reverts and the pass is a full-price no-op.

Partial seeding is the middle, and it is a differently-*targeted* search rather than a cheaper one: rank a grandparent's units by how much of their flow leaves their current module, **lock** the confident cores and **release** the loosest fraction *q* as fresh singletons, so the greedy loop must re-place exactly the units whose membership was marginal.

**The ranking is the mechanism, not the release count** — the inverse control settles it (web-NotreDame, q=0.5, seed 123): boundary-release −0.015% < baseline < inverse-release +0.069% < random +0.153%. Releasing the same *number* of units by the inverse ranking is worse than baseline.

Two policy decisions came from measurement rather than design:
- **Re-refine only.** A layer is partial-seeded only on a re-refine (`refineHierarchy` sweep > 0), never on its first from-singletons derivation, because seeding's quality damage is localised to *first contact* with a grandparent rather than to stale seeds. This is what removes every regression the always-on variant had (science2001 +0.016% on 3/3 seeds, `-F` web-NotreDame +0.080% on 3/3). Consequence: `-F` refines once, so nothing there is ever a re-refine and **`-F` is bit-identical by construction**.
- **q = 0.40, metric = crossing link flow / total link flow.** Broad plateau; web-NotreDame collapses below q≈0.33 when always-on and everything degrades above 0.6. The alternative `exit/(flow + exit)` is **degenerate on undirected first-order networks** — `FlowCalculator.cpp:1475` sets `node.exitFlow = node.flow`, so the ratio is 0.5 for every leaf and the ranking collapses to node order; proof: on powergrid that metric and its exact inverse give bit-identical output.

| config (`-C`) | before | at landing | Δ CPU | vs OO |
|---|--:|--:|--:|--:|
| powergrid | 4.74907624 | **4.73960028** (−0.199%) | **+0.0%** (0.26s) | **beats by 0.36%** |
| web-NotreDame | 5.57279424 | **5.56799417** (−0.086%) | **+1.7%** (20.53 → 20.88s) | +0.035% (was +0.121%) |
| netscicoauthor2010 | 4.05186752 | **4.04960341** (−0.056%) | +0.0% (at floor) | +0.16% (was +0.21%) |
| the other 10 configs | — | bit-identical | — | — |

Seed-robust: at the 1e-3 knee the same policy held on 4 seeds per network (web-NotreDame −0.121/−0.114/−0.099/−0.140%, powergrid −0.151/−0.169/−0.253/−0.076%). Nothing regresses on any network or seed at either knee.

**Cost: none that resolves.** Interleaved min-of-3 on an idle machine: powergrid +0.0% (0.26s both), web-NotreDame +1.7% (20.53 → 20.88s), netsci and science2001 at the timer floor. At the 1e-3 knee the same feature cost powergrid +7.4%; at the shipped 5e-3 default that bill disappears **and powergrid's win is larger** (−0.199% vs −0.151%), because the shallower refinement leaves more for a well-targeted re-refine to find. Per-trial overhead is ≤1%.

air30k is **bit-identical on every variant and seed**: its `-C` winner is a flat-first trial whose leaf layer is already skipped by `m_bottomConverged` (F22), so there is no re-refine to partial-seed. Applying partial seeding to the converged flat bottom *instead* of skipping it was measured and rejected (−0.033%/−0.011% on two seeds for +16%/+34%/+41% CPU).

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

### Flat-first trials: the flat candidate for the hierarchical searches (F21/F22, PR #891 — #889 hierarchical half, closes #834)

Measured at the coarse-tune PR: the `-C` overshoot forms in the **enter-flow up-build**, not in the within-parent sub-optimizers — on networks whose optimum is (near-)flat with many modules (air30k: OO wants 301–332 top modules), no amount of interior refinement can reach the flat basin from the fine-blocks build. OO's own hierarchical search is flat-first (partition, then super/sub structure), so the columnar engine now gets **both search directions** and lets best-of-N pick per network:

1. **Flat-first alternate trials.** Even-numbered trials (2nd, 4th, …) of `-C` and `-F` run the flat search first; the first trial keeps the fine-blocks build, so `-N1` is **bit-identical** (verified on all 13 configs × {`-C`, `-C -F`}, and identical in cost). To avoid paying the flat pipeline where flat is hopeless, the trial first runs a **cheap probe** — the full aggregation *without* the leaf fine-tune (module-level cost), sharing pass-1 with the fine-blocks screen (no second leaf sweep) — and completes the expensive leaf-level flat pipeline (fine-tune + merge↔retune interleave) only when the probe lands within 0.5% of the fine-blocks up-build. The completed flat stack becomes a gated candidate against the refined hierarchy, and the up-builds grown from the flat bottom join the strategy screen. The probe separates the benchmark set perfectly: every network where flat truly wins probes at est/build ≤ 1.000 (air30k 0.84, regularized 0.80, malaria 0.88, pref-mods 0.63, politicalblogs 0.96, jazz 0.996, lazega 0.995), every true flat-loser at ≥ 1.008 (science2001 1.008, netsci 1.034, powergrid 1.10, web-NotreDame 1.18).
2. **The flat bottom skips the leaf-layer re-derivation** (`m_bottomConverged`). A build grown from the completed flat pipeline arrives with its leaf partition *already* at the two-level fixpoint, but `refineHierarchy` still began its interior sweep at layer 0 — and `refineLayerWithinGrandparent(0)` re-partitions every leaf from singletons within its grandparent. Phase timers say that pass was **the single most expensive thing in a flat trial and returned nothing**: 0.59 s of a 1.05 s air30k flat trial, with all five trials printing `build=5.393664 -> refined=5.393664`, while the flat completion the probe gate exists to ration cost only 0.14 s. It is skipped while the bottom is the flat fixpoint (`-F`'s `refineBottomWithinParents` likewise); interior layers above still refine, and an accepted refine there re-marks layer 0 dirty, so the re-derivation stays reachable once the structure it nests in actually moves.
3. **Winner deep repair for hierarchical runs.** The once-per-run deep repair of the winning trial (previously `-2` only) now also fires when a hierarchical run's winning tree is two-level-shaped — i.e. a flat trial won outright — and leaves deeper winners untouched. Deterministic in serial and parallel-trial modes (post-trial-loop, config-derived seed).

Per-feature attribution on the networks the PR moves (`-C`, best of 10):
| config (`-C`) | before | flat-first trials | + winner repair | OO |
|---|--:|--:|--:|--:|
| air30k | 5.46566862 | **5.39366442** | – (3-level winner) | 5.39395534 |
| air30k reg. | 5.66241021 | **5.57602419** | – (3-level winner) | 5.57565280 |
| malaria | 7.47888701 | 7.47430185 | **7.42225457** | 7.50337896 |
| lazega | 6.03455147 | 6.02121843 | **6.01786027** | 6.01786027 |
| jazz | 6.87505673 | **6.86122978** | – (base net) | 6.86122978 |
| pref-mods | 8.46079677 | **8.23835056** | – (leaf-only bias) | 7.92800030 |
| politicalblogs | 6.74069306 | **6.74058207** | – (3-level winner) | 6.73952481 |

Malaria is the one config the flat trials barely move on their own (−0.06%): its win is the winner deep repair. Malaria's flat-bottom builds are two-level, so they never enter the interior refinement at all — which is why its flat trials are *cheaper* than its hierarchical ones.

**Cost.** The flat trials cost time only where the probe says flat is competitive — i.e. where they win codelength — and item 2 removes the part of that bill that was buying nothing. Interleaved same-session A/B, `-N10`; the session ran at load average 24–65, so each config was repeated 4–5× per binary and the table reports the **minimum** observed time (the contention-free estimate), pooled over every repetition run this session. Wall and CPU agree to within ~1 pp on every row.

| config | codelength Δ | base CPU → new CPU | time Δ (`-C`) | time Δ (`-F`) | for reference: #891 before this fix |
|---|--:|--:|--:|--:|--:|
| air30k | −1.32% | 3.68s → 3.92s | **+6.5%** | +11.7% | +39% |
| air30k reg. | −1.53% | 3.52s → 4.13s | **+17.3%** | +18…21% | +61% |
| malaria | −0.76% | 3.37s → 3.34s | **−0.9%** | +4.1% | +37% |
| science2001 pref-mods | −2.63% | 3.26s → 3.42s | **+4.9%** | +5.7% | +22% |
| jazz / lazega / politicalblogs | −0.20% / −0.28% / −0.002% | ≤0.07s (floor) | 0% | 0% | — |
| science2001 | unchanged | 3.17s → 3.22s | +1.6% | ~0% | ±noise |
| web-NotreDame | unchanged | 21.19s → 21.07s | −0.6% | ~0% | ±noise |
| powergrid / netsci | unchanged | 0.25s → 0.26s / floor | +4% (0.01s) / 0% | ~0% | ±noise |

**No network now pays a non-trivial cost for a non-existent gain.** The five configs whose codelength the flat search cannot improve (netsci, powergrid, science2001, web-NotreDame, and politicalblogs at −0.002%) come in between −0.6% and +4%, and the +4% on powergrid is a single hundredth of a second at the measurement floor — the probe is module-level work on a network whose aggregation has already collapsed. Regularized air30k is the only config left with a bill worth naming, and it is now genuine flat-search work: the probe's aggregation passes (+0.15 s/trial — the tele/regularized corrections ride the module-level passes there) plus the flat completion (+0.15 s/trial), partly offset by the refinement that no longer runs. Individual repetitions of that row ranged +14% to +25% under load; +17% is the pooled minimum. `-C` stays 52–69% faster than OO on all four gaining networks.

**One codelength moved relative to the first cut of PR #891**, and it is worth stating plainly: regularized air30k at seed 123 goes 5.575137 → 5.576024 (+0.016%). That 0.24% was won by a single flat trial's leaf re-derivation, and it does not survive a seed change — seeds 234/345/456 are bit-identical with and without the pass, and on all three `-C` already sits 0.02–0.10% above OO. The earlier "regularized air30k beats OO" was a seed-123 artefact; ~3 s per `-N10` run for a 1-in-4-seed lottery ticket is not a trade worth keeping, and a winner-only variant does not recover it either (at seed 123 the winning trial is one where the pass gains nothing).

### Coarse-tune: trajectory repair + winner deep repair (#889 two-level half, PR #890)

The objective-aware aggregation (#834) can overshoot: consolidation makes each pass's units atomic, so a merge that shouldn't have happened cannot be undone by later passes, the leaf fine-tune or the gated merges (single-leaf moves can't cross the barrier). The coarse-tune PR added the subdivision half, split by cost:

1. **In-trajectory descending repair** (every trial, marginal cost): each aggregation pass's unit level and leaf composition are retained — only when module-move-capable corrections are attached, so base networks pay nothing — and after the aggregation converges, *before* fine-tune smears the boundaries, each retained granularity is re-sorted within the best partition with a seeded module-level move loop, coarse to fine. This alone puts air30k and regularized air30k `-2` **below OO, at less time than before the operator existed**.
2. **Deep repair of the winning trial** (once per run): the expensive discovery step — from-singletons sub-clustering within each module (community granularity, so extracting a whole overshot community is a single gated move), interleaved with the seeded leaf fine-tune and the merge. A per-trial version was measured and rejected (malaria 2.5 → 8.9s: 78% of the time re-derived sub-clusterings the retunes kept invalidating); the engine runs it **once on the best-of-N partition** after the trial loop — deterministic in serial and parallel-trial modes, never worse than the seed, cost amortizing with `-N`. This is what finds malaria's 7.4223 and lazega's exact OO tie — and, with the flat-first trials above, carries those same values into `-C` and `-F`.

### Tele-path and metadata move-loop hoist (#875)

Two follow-ups to the #868 move-loop speedups: the recorded-teleportation delta hoists its six old-module plogp terms once per unit (`hoistOldSideTele`), and the metadata correction caches its per-leaf move-loop terms with the same zero-path fast-track as the memory correction. Both are **bit-exact** — codelengths are unchanged on every network × {`-C`, `-F`, `-2 -C`}.

The measurable win is the tele path: on **air30k `-d --regularized`** the hoist alone accounts for −13% / −15% / −16% on `-C` / `-F` / `-2 -C` (measured at #875). The Meta fast paths are correctness/consistency (the set has no large metadata network). The biased **science2001 `-d --preferred-number-of-modules 25`** config uses no teleport or metadata; its columnar partition lands the finest level on exactly 25 modules (`-2 -C`, top = 25) — the `|K − K_pref|` bias wired in [#827](https://github.com/mapequation/infomap/issues/827).

### The split operator on the hierarchical path (F27, PR #987/#988)

The hierarchical search could split a *single* unit off into a new empty module (`moveLoop`'s empty-module option) but never a *group* out of an over-merged one: `mergeLeafModulesWithinParents` only coarsens, and `refineLayerWithinGrandparent` re-derives a whole grandparent all-or-nothing, so a re-derivation containing one good split plus several bad ones is rejected wholesale. `splitLevelModules` is the hierarchical analogue of the two-level operator from #890: partition a level-(k+1) module's level-k children into pieces, aggregate a piece-level network, run a seeded move loop over the pieces (a piece may land in any module, including an empty one — group-split *and* cross-parent relocation), gated on the true stack codelength.

It runs **once per run on the winning trial**, not per trial. The per-trial version was measured and dropped: on air30k it accumulates 4.05% of in-trial gain and delivers −0.033%, because ~85% of that lands on hierarchical trials sitting 1.1–1.5% behind the flat-first trials, which can never win the best-of-N. **The discriminator is trial competitiveness, which is inherently cross-trial**, so no within-trial rationing separates the networks — level gating, piece-source gating, gain ratchets, attempt caps and shape gating were each measured and each failed. The per-trial half cost air30k +24% and regularized +16%.

| config (`-C`) | before | at landing | seed 234 | seed 345 | mean | Δ CPU |
|---|--:|--:|--:|--:|--:|--:|
| malaria | 7.42225457 | **7.41714932** (−0.069%) | −0.613% | −0.431% | **−0.371%** | +5.1…11.1% |
| air30k | 5.39366442 | **5.39320406** (−0.009%) | −0.001% | −0.002% | −0.004% | +1.9…2.9% |
| air30k (reg.) | 5.57602419 | **5.57591489** (−0.002%) | −0.022% | −0.031% | −0.018% | +1.5…3.1% |
| the other 10 configs | — | bit-identical | — | — | 0 | at floor |

Malaria's is the win that justifies the operator, and note that **seed 123 is its weakest seed** — the headline single-seed figure understates the mean. It is the first hierarchical `-C` result to beat malaria's repaired flat one.

Three things make the once-per-run path work, each measured separately: **best-per-shape repair** (track the best *deep* trial and repair it when the overall winner is flat — without it the hook makes zero attempts on malaria, whose winner *is* flat); a **`winner` level policy** (leaf level only when a module-move correction is attached, keeping the from-singletons piece source — web-NotreDame operator 1.92s → 1.02s at identical codelength); and a **correction gate on the whole repair** (base networks otherwise pay +7.0% on web-NotreDame for −0.0005%, because the scaffolding dwarfs the splits).

Bundled, bit-exact: the gated lambdas in `coarsenModules` and `refineHierarchy` no longer snapshot
`m_hierLevels[0]` — ~50 MB of leaf CSR per gated step on web-NotreDame, twice per sweep.

> **Attribution correction (measured at the master sync).** Bundling that fix into this PR meant the Δ
> CPU column above is a *net* of two opposing effects, which is exactly what the project protocol says
> not to do. Isolated by reverting only the snapshot avoidance on the current tip (web-NotreDame `-C
> -N10`, min-of-3 interleaved, codelength bit-identical throughout): the snapshot fix is worth
> **−2.8% peak RSS and +0.3% CPU — i.e. no measurable CPU effect at all.** So the Δ CPU figures above
> are, within noise, the split operator's own cost, and the bundling did not flatter it. What the
> original wording overstated is the *size*: "~50 MB per gated step" is real per step, but those
> snapshots are transient and freed inside the step, so they barely touch the run's *peak*. For the
> same reason the fix does not meaningfully pre-claim the leaf-CSR single-owner work (#960), which is
> worth a further −30.5% RSS on top of it.

**`-N1` is completely untouched** (0 attempts, 0 CPU delta, identical codelength): `updateBestResult` only materializes `bestTree` when `numTrials > 1`, so the hook is inert at single-trial — the single-trial cost rule is satisfied with no caveat.

**This and partial seeding partition the benchmark set with no overlap**, verified in both directions: with partial seeding disabled the split's three networks are bit-identical, and the split repair records **0 attempts** on web-NotreDame and science2001 against 11 attempts / 5 accepted on malaria.
