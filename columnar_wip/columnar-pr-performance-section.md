## Performance

> Manual benchmark comparing the two engines on the same binary. This is **not** the CI `perf-pr.yml` check (which only sees the default OO path, since the new core is flag-gated) — it's included here so reviewers can see how the opt-in `--columnar` engine compares to the default.

Single-threaded (`MODE=release OPENMP=0`), `--seed 123 -N10` — **best of 10 trials**, the way Infomap is normally run. Codelength in bits; `time` = total wall for all 10 trials, minimum of 3 interleaved repetitions; `top`/`lvls` = top modules / levels of the best partition. The network set spans base (undirected + directed), metadata, multilayer and state/memory objectives; see [`columnar_wip/benchmark-networks.md`](columnar_wip/benchmark-networks.md) for paths and full details. Columnar column = the default columnar search (`-C`).

> **This PR fixes [#994](https://github.com/mapequation/infomap/issues/994)**: a `--parallel-trials`
> worker never received the native columnar leaf input, so it rebuilt the columnar structure from the
> InfoNode leaf tree on **every trial** while the serial path borrows a leaf SoA built once. Workers now
> borrow the main instance's. Every number below is measured on the **fixed** binary in one session: all
> 65 configs (13 networks × {OO, `-C`, `-C -F`, OO `-2`, `-2 -C`}), interleaved, minimum of 3
> repetitions, with the pre-fix binary run arm-for-arm in the same session. **All 65 codelengths — and
> every top-module and level count — are bit-identical between the two binaries.**
>
> This table cannot show the fix working: no benchmarked configuration uses `--parallel-trials`, so
> `borrowColumnarLeafInputFrom` is never called in any row of it. The evidence is the parallel-trials
> section at the bottom; this table is here to show the fix costs nothing off that path.
>
> **Read the time column with care in this session.** It ran on a machine at load ~8–9 and is much
> noisier than the previous snapshot's. The calibration is in the table itself: the **OO arms, whose
> execution this change cannot touch at all**, moved −8.9% to +9.8% between the two binaries, while the
> columnar arms moved −2.8% to +5.2% — both with a median of exactly 0.0%. The columnar arms are the
> *tighter* of the two, so nothing in the spread is attributable to this change; below ~10% this
> session's time axis simply does not resolve. The columnar/OO time ratio is preserved to within 4.2% of
> the previous session on every network (web-NotreDame 0.132 in both).

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
<tr><td>ninetriangles</td><td align="right">27</td><td>base</td><td>—</td><td align="right">3.38583082</td><td align="right">0.01s</td><td align="right">3</td><td align="right">3</td><td align="right"><b>3.38583082</b> (=)</td><td align="right">0.01s</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">198</td><td>base</td><td>—</td><td align="right">6.86304747</td><td align="right">0.03s</td><td align="right">5</td><td align="right">2</td><td align="right"><b>6.86275593</b> (−0.00425%)</td><td align="right">0.02s</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">552</td><td>base</td><td>—</td><td align="right">4.04354934</td><td align="right">0.13s</td><td align="right">2</td><td align="right">5</td><td align="right">4.05454025 (+0.272%)</td><td align="right">0.03s (−77%)</td><td align="right">2</td><td align="right">4</td></tr>
<tr><td>powergrid</td><td align="right">4 941</td><td>base</td><td>—</td><td align="right">4.75872920</td><td align="right">2.04s</td><td align="right">5</td><td align="right">7</td><td align="right"><b>4.74107206</b> (−0.371%)</td><td align="right">0.27s (−87%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td>politicalblogs</td><td align="right">1 046</td><td>base</td><td><code>-d</code></td><td align="right">6.73892798</td><td align="right">0.16s</td><td align="right">80</td><td align="right">3</td><td align="right">6.74094314 (+0.0299%)</td><td align="right">0.07s (−56%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td>science2001</td><td align="right">7 170</td><td>base</td><td><code>-d</code></td><td align="right">7.83638921</td><td align="right">8.11s</td><td align="right">11</td><td align="right">4</td><td align="right"><b>7.83343660</b> (−0.0377%)</td><td align="right">3.15s (−61%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>web-NotreDame</td><td align="right">325 729</td><td>base</td><td><code>-d</code></td><td align="right">5.56592477</td><td align="right">148.7s</td><td align="right">17</td><td align="right">13</td><td align="right">5.56852929 (+0.0468%)</td><td align="right">19.6s (−87%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td>lazega</td><td align="right">69</td><td>metadata</td><td>—</td><td align="right">6.01786027</td><td align="right">0.02s</td><td align="right">7</td><td align="right">2</td><td align="right"><b>6.01786027</b> (=)</td><td align="right">0.01s</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">5</td><td>multilayer</td><td>—</td><td align="right">2.01140524</td><td align="right">0.01s</td><td align="right">2</td><td align="right">2</td><td align="right"><b>2.01140524</b> (=)</td><td align="right">0.01s</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">307·9L</td><td>multilayer</td><td>—</td><td align="right">7.50242050</td><td align="right">9.13s</td><td align="right">142</td><td align="right">3</td><td align="right"><b>7.39750171</b> (−1.4%)</td><td align="right">3.26s (−64%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>air30k</td><td align="right">13 213</td><td>state/memory</td><td>—</td><td align="right">5.39287115</td><td align="right">11.7s</td><td align="right">16</td><td align="right">4</td><td align="right"><b>5.39242541</b> (−0.00827%)</td><td align="right">3.74s (−68%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">13 213</td><td>state/memory</td><td><code>-d --regularized</code></td><td align="right">5.57843563</td><td align="right">7.47s</td><td align="right">301</td><td align="right">3</td><td align="right"><b>5.57624241</b> (−0.0393%)</td><td align="right">4.02s (−46%)</td><td align="right">11</td><td align="right">3</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">7 170</td><td>base</td><td><code>-d --preferred-number-of-modules 25</code></td><td align="right">7.94035360</td><td align="right">7.93s</td><td align="right">25</td><td align="right">4</td><td align="right">8.23558553 (+3.72%)</td><td align="right">3.28s (−59%)</td><td align="right">25</td><td align="right">2</td></tr>
</tbody>
</table>

`nodes`: state nodes for air30k (13 213 states over 183 physical); physical·layers for malaria. **Bold** = columnar beats or exactly ties OO. Parentheses on the columnar `-C` columns = change vs OO (`(=)` = bit-identical; negative time = faster).

**Reading the table**
- **Codelength** — columnar `-C` **ties or beats OO on 9 of 13 configs**: it beats OO on malaria (−1.40%), powergrid (−0.37%), science2001 (−0.04%), regularized air30k (−0.04%), air30k (−0.008%) and jazz (−0.004%), and ties lazega and the toys. Remaining gaps: pref-mods +3.72% (the `|K − K_pref|` bias is leaf-loop-only, #827), netsci +0.27%, web-NotreDame +0.05%, politicalblogs +0.03%. Regularized air30k, air30k and jazz are inside the per-seed spread either way, so read those three as ties that fell on the good side of this seed rather than as wins.
- **The web-NotreDame gap is a user choice.** At the default knee it is +0.05%. Adding `--tune-iteration-relative-threshold 1e-3` takes it to 5.56067487 — **−0.094%, ahead of OO** — for +18–30% wall; `0` (full convergence) reaches 5.55881205, −0.128% ahead, for +85–95%. F26 explains why the deeper refinement is a dial rather than the default.
- **Speed** — columnar is faster on every non-trivial network: ~1.8× on regularized air30k, ~2.4–2.6× on pref-mods and science2001, ~2.8–3.1× on malaria and air30k, ~7.6× on web-NotreDame and powergrid (19.6s vs 148.7s).
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
<tr><td>netscicoauthor2010</td><td align="right">4.05454025</td><td align="right">0.03s</td><td align="right">2</td><td align="right">4</td><td align="right">4.06300588 (+0.209%)</td><td align="right">0.02s</td><td align="right">4</td><td align="right">4</td></tr>
<tr><td>powergrid</td><td align="right">4.74107206</td><td align="right">0.27s</td><td align="right">5</td><td align="right">5</td><td align="right">4.77402224 (+0.695%)</td><td align="right">0.16s (−41%)</td><td align="right">4</td><td align="right">5</td></tr>
<tr><td>politicalblogs (<code>-d</code>)</td><td align="right">6.74094314</td><td align="right">0.07s</td><td align="right">81</td><td align="right">2</td><td align="right">6.74094314 (=)</td><td align="right">0.07s</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td>science2001 (<code>-d</code>)</td><td align="right">7.83343660</td><td align="right">3.15s</td><td align="right">15</td><td align="right">3</td><td align="right">7.83343660 (=)</td><td align="right">3.01s (−4%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>web-NotreDame (<code>-d</code>)</td><td align="right">5.56852929</td><td align="right">19.6s</td><td align="right">5</td><td align="right">6</td><td align="right">5.62506198 (+1.02%)</td><td align="right">14.9s (−24%)</td><td align="right">2</td><td align="right">5</td></tr>
<tr><td>lazega (meta)</td><td align="right">6.01786027</td><td align="right">0.01s</td><td align="right">7</td><td align="right">2</td><td align="right">6.01786027 (=)</td><td align="right">0.01s</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.01s</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.01s</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.39750171</td><td align="right">3.26s</td><td align="right">2</td><td align="right">3</td><td align="right">7.39750171 (=)</td><td align="right">3.20s (−2%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>air30k (states)</td><td align="right">5.39242541</td><td align="right">3.74s</td><td align="right">22</td><td align="right">3</td><td align="right">5.39242541 (=)</td><td align="right">3.54s (−5%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57624241</td><td align="right">4.02s</td><td align="right">11</td><td align="right">3</td><td align="right">5.57624241 (=)</td><td align="right">3.57s (−11%)</td><td align="right">11</td><td align="right">3</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">8.23558553</td><td align="right">3.28s</td><td align="right">25</td><td align="right">2</td><td align="right">8.23558553 (=)</td><td align="right">3.21s (−2%)</td><td align="right">25</td><td align="right">2</td></tr>
</tbody>
</table>

Parentheses on the columnar `-F` columns = change vs `-C` (`(=)` = bit-identical; negative time = faster); both columns are from the current same-session re-measurement. Sub-0.1s toy times are shown without a percentage (measurement floor).

**`-F` still ties `-C` on 10 of 13 configs** — including pref-mods, where it previously lost another +2.3% — because the flat-first trials carry the same winning partitions into both searches. The dial only bites on the base networks with real deep hierarchy, where skipping the interior refinement trades codelength for speed — and the F23 knee widens that trade, because it deepens exactly the refinement `-F` skips: web-NotreDame **+1.02% for −24% time**, powergrid +0.69% for −41%, netsci +0.21%. Partial seeding widens the dial further, because it improves `-C` on exactly the three networks `-F` cannot follow it on (the re-refine gate makes `-F` bit-identical by construction). On the flat-winning networks `-F` is 0–11% faster than `-C`, because `-C`'s expensive pass — the interior refinement — is exactly the one the flat bottom no longer needs (below).

The columnar interior refinement stops at a diminishing-returns knee (default `--tune-iteration-relative-threshold` = 5e-3). On the two deep base networks, lowering it to 1e-3 buys 0.05–0.14% of codelength for ~20–30% more time; shallow networks are structurally unaffected. See the knee section below.

### The refine knee stays at 5e-3, and 1e-3 is a dial (F23 → F26)

`ColumnarTwoLevel::m_minRelTuneImprovement` stops the interior-layer refinement once a whole up/down
sweep gains less than this fraction of the post-build codelength. The shipped default is **5e-3**, and
the deeper setting is offered as a dial rather than defaulted — it is real quality that more trials
cannot buy (at matched CPU on web-NotreDame, 5e-3 saturates at 5.5727 by `-N12` while 1e-3 reaches
5.5674), but not worth a fifth more CPU by default. Re-measured at this tip:

| `--tune-iteration-relative-threshold` | web-NotreDame `-C` | vs default | wall |
|---|--:|--:|--:|
| 5e-3 (default) | 5.56852929 | — | — |
| 1e-3 | **5.56067487** | −0.14% | +22% (+18–30% across sessions) |
| 0 (full convergence) | **5.55881205** | −0.17% | +95% (+85–95% across sessions) |

Only the two deep base networks react at all: `refineSweeps` is 1 for a stack with at most one
interior layer, so science2001/air30k/malaria cannot, and `-F` never enters `refineHierarchy`. (The
default was briefly changed to 1e-3 and reverted on a measurement error — F23/F26 in the findings
log.)

> **This section is a snapshot, not a changelog.** It carries the current numbers plus the evidence for
> the change under review. Per-feature attribution for features that already landed (partial seeding
> #985, flat-first #891, coarse-tune #890, the #875 hoist, the split operator #987/#988) lives in each
> of those PRs, whose own copy of this file is the measurement that justified it; the running narrative
> is in [`columnar_wip/columnar-rethink-notes.md`](columnar_wip/columnar-rethink-notes.md). The one
> section below — "What this PR fixes: #994" — is the evidence for **this** PR.

Every measured run behind this section is logged row-per-run in [`columnar_wip/columnar-search-runs.tsv`](columnar_wip/columnar-search-runs.tsv) for plotting the codelength/time frontier. Read the `batch` column before comparing times: session noise floors ranged ±3% to 13%, so the time axis is only comparable within a batch (`load1m`, `reps`, `agg` are recorded for that reason), and `derived=1` marks rows reconstructed from reported percentages rather than measured absolutes.

### Two-level clustering (`-2`)

`--two-level` is wired to the columnar engine on the `columnar-two-level` branch (PR #823): the full two-level optimize materialized as a two-level stack, followed by the correction-aware module-merge coarsening interleaved with a seeded leaf fine-tune until the pair stops improving, plus the #889 coarse-tune (in-trajectory repair every trial + deep repair of the winning trial). **No change to the `-2` code path since PR #890**, and none from this PR — every codelength below is bit-identical to the base branch, and every row is re-measured in the same session as the tables above.

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
<tr><td>jazz</td><td align="right">6.86304747</td><td align="right">0.02s</td><td align="right">5</td><td align="right"><b>6.86122977</b> (−0.0265%)</td><td align="right">0.02s</td><td align="right">6</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.28501267</td><td align="right">0.04s</td><td align="right">56</td><td align="right"><b>4.28307258</b> (−0.0453%)</td><td align="right">0.02s</td><td align="right">59</td></tr>
<tr><td>powergrid</td><td align="right">5.60044386</td><td align="right">0.62s</td><td align="right">419</td><td align="right">5.63729688 (+0.658%)</td><td align="right">0.11s (−82%)</td><td align="right">419</td></tr>
<tr><td>politicalblogs (<code>-d</code>)</td><td align="right">6.73972141</td><td align="right">0.10s</td><td align="right">80</td><td align="right"><b>6.73957529</b> (−0.00217%)</td><td align="right">0.06s (−40%)</td><td align="right">81</td></tr>
<tr><td>science2001 (<code>-d</code>)</td><td align="right">7.95003960</td><td align="right">4.36s</td><td align="right">496</td><td align="right"><b>7.94997883</b> (−0.000764%)</td><td align="right">2.59s (−41%)</td><td align="right">506</td></tr>
<tr><td>web-NotreDame (<code>-d</code>)</td><td align="right">6.74298853</td><td align="right">44.2s</td><td align="right">11809</td><td align="right">6.75421666 (+0.167%)</td><td align="right">18.6s (−58%)</td><td align="right">11991</td></tr>
<tr><td>lazega (meta)</td><td align="right">6.01786027</td><td align="right">0.02s</td><td align="right">7</td><td align="right"><b>6.01786027</b> (=)</td><td align="right">0.01s</td><td align="right">7</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.01s</td><td align="right">2</td><td align="right"><b>2.01140524</b> (=)</td><td align="right">0.01s</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.50595639</td><td align="right">6.60s</td><td align="right">142</td><td align="right"><b>7.40044538</b> (−1.41%)</td><td align="right">2.83s (−57%)</td><td align="right">168</td></tr>
<tr><td>air30k (states)</td><td align="right">5.39331278</td><td align="right">4.42s</td><td align="right">332</td><td align="right"><b>5.39305505</b> (−0.00478%)</td><td align="right">3.49s (−21%)</td><td align="right">334</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57921689</td><td align="right">5.53s</td><td align="right">301</td><td align="right"><b>5.57557704</b> (−0.0652%)</td><td align="right">3.66s (−34%)</td><td align="right">304</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">8.13096953</td><td align="right">5.85s</td><td align="right">25</td><td align="right">8.23558553 (+1.29%)</td><td align="right">3.26s (−44%)</td><td align="right">25</td></tr>
</tbody>
</table>

Parentheses on the columnar columns = change vs OO `-2` (**bold** = columnar beats or exactly ties OO). Columnar `-2` ties or beats OO on 10 of 13 configs, including *every* correction network; the three exceptions are base-objective configs (powergrid +0.66%, web-NotreDame +0.17%, pref-mods +1.29%), and it is faster on every non-trivial network (−21% to −82%).

### What this PR fixes: #994, measured here

`buildColumnarLeafInput` runs in `RunSession`, on the main instance only. `runTrialsInParallel` builds
each trial worker and calls `worker.initNetwork(m_network)` directly, bypassing that wrapper — so
`m_columnarNativeInput` stayed false in every worker and `setupColumnarOptimizer` took the
`buildFromLeaves(m_leafNodes, …)` branch, rebuilding the columnar structure from the InfoNode leaf tree
on **every trial**.

A worker cannot build its own: the CLI releases the links before the trials run, which is exactly why
the main instance builds it early. So it borrows the main instance's — sound for the same reason each
trial's optimizer already borrows it (immutable for the whole run, owner outlives every borrower) — and
this also puts workers on the **same input construction as the serial path** instead of a parallel one.

**The two constructions were numerically identical.** Checked before and after on science2001,
powergrid, air30k and malaria: all four trials bit-identical, same top modules and levels. This is a
pure cost removal plus the removal of a divergence class that had only ever been a measured fact, never
a structural one.

**Effect.** One worker (`OMP_NUM_THREADS=1`, `-N4`, min-of-3), every (build, engine, mode) cell measured
once per rep and cycled so drift hits all cells alike. Columnar parallel-arm CPU:

| network | before | after | change | gap vs serial |
|---|--:|--:|--:|--:|
| web-NotreDame `-d` (325 729 leaves) | 10.23s | 9.68s | **−5.4%** | +17.6% → +15.2% |
| science2001 `-d` (7 170) | 1.61s | 1.55s | **−3.7%** | +12.9% → +6.8% |
| air30k (13 213 states) | 1.78s | 1.79s | +0.6%, below the floor | +3.6% → +7.4% |

The noise floor here is set by the OO arms, which this change does not touch: byte-identical OO code
measured 0.1–2.1% apart between the two builds, and its *gap* percentages — differences of two noisy
numbers — moved by up to 2 points. So the two large networks are real and **air30k is not resolvable**,
which is the expected shape: the removed work is a per-trial rebuild that scales with leaf count, and
air30k has 13k leaves against web-NotreDame's 326k. Read air30k as below the floor, not as a regression.

**The residual gap is not the columnar engine's — the percentages say otherwise only because of
arithmetic.** At face value the residual looks damning: on web-NotreDame the columnar worker path is
+15.2% against serial while OO's is +6.4%. But the worker overhead is an **absolute** per-trial cost,
and in absolute terms **OO pays more**:

| network | OO extra, per trial | columnar extra, per trial |
|---|--:|--:|
| web-NotreDame `-d` | 0.58–0.91 s | 0.32–0.38 s |
| air30k | 0.054–0.076 s | 0.016–0.031 s |
| science2001 `-d` | 0.032–0.045 s | 0.024–0.046 s |

A columnar trial on web-NotreDame takes ~2.2s against OO's ~14s, so the same fixed overhead is ~6× the
fraction. That is the whole of the percentage difference. What remains is shared `runTrialsInParallel`
work — the worker loop calls full `initNetwork` per trial where the serial loop calls `removeModules()`,
and materializes `sortChildrenOnFlow`, the per-level statistics and the full `NodePaths` walk for every
trial when only one survives — and it is filed as
[#996](https://github.com/mapequation/infomap/issues/996), on master, since it is outside `Columnar*`
and both engines pay it. Deliberately **not** attributed to a single cause: a profile diff shows no
dominant cost, each candidate is under 1.5% of the run, and the two profiles have different sample
denominators, so the arithmetic does not close on any one of them.

**A guard became load-bearing that was not before.** `setupColumnarOptimizer` gates the native path on
`m_columnarNativeInput && !haveHardPartition()` — hard cluster-data needs the InfoNode leaf tree for
`restoreHardPartition`. That second condition never mattered on the worker path, because a worker's
`m_columnarNativeInput` was unconditionally false; borrowing makes it true for the first time. Removing
the guard now **segfaults** rather than returning a wrong number, and the new `Parallel trials with hard
cluster-data match serial trials` case was verified to catch it.

**Verified unchanged.** Parallel `-N4` == serial `-N4` element-wise on `--columnar` across default,
`-2`, `-F`, `--entropy-corrected`, `--markov-time 1.5`, `--variable-markov-time`, `-d`,
`-d --recorded-teleportation`, `-d --regularized`, `--preferred-number-of-modules`, states, multilayer,
bipartite, weighted directed, `--meta-data`, and `--cluster-data`.
